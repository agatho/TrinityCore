#!/usr/bin/env python3
# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
# Licensed under GPL v2; see the C++ headers in the same workstream for the boilerplate.

"""
road_label.py — interactive labeling tool for road-aware mmap ground truth.

Reads a candidate CSV produced by road_corpus_dumper, shows one MCNK at a
time with its texture path + zone context + wow.tools hyperlink, and accepts
single-keypress labels: R (road), N (not road), A (ambiguous), U (undo),
S (save), Q (quit). Saves atomically after every label so progress is never
lost.

The script depends only on the Python 3 stdlib (tkinter for the GUI; csv +
tempfile for I/O). No third-party packages required — runs on any vanilla
Python 3 install with Tk support enabled.

Usage:
  road_label.py --corpus path/to/candidates.csv [--labeler <name>]
                [--skip-labeled] [--start-at <row>]

Workflow:
  1. road_corpus_dumper produces candidates.csv with empty `label` columns.
  2. Run road_label.py against that file. Existing labels are SKIPPED by
     default (resume-friendly). Pass --no-skip-labeled to relabel from scratch.
  3. When done, run road_validator --corpus candidates.csv to score against
     the classifier.

Schema is whatever road_validator/road_corpus_dumper agree on. See
CORPUS_BUILDING_GUIDE.md for the column contract.
"""

from __future__ import annotations

import argparse
import csv
import datetime
import os
import sys
import tempfile
import webbrowser
from dataclasses import dataclass, field
from typing import List, Optional

try:
    import tkinter as tk
    from tkinter import ttk, messagebox
except ImportError as e:
    sys.stderr.write(
        "ERROR: tkinter unavailable. Install python3-tk (Linux) or use "
        "a Python build with Tk support.\n"
    )
    sys.exit(1)


SCHEMA_COLUMNS = [
    "schema_version", "map_id", "adt_x", "adt_y", "mcnk_idx",
    "texture_blp", "effect_id", "layer_count", "labeler",
    "label_date", "label", "confidence", "notes",
]


@dataclass
class Row:
    """One labeled MCNK candidate row. Mirrors the CSV schema 1:1."""
    schema_version: str = "1"
    map_id:         str = "0"
    adt_x:          str = "0"
    adt_y:          str = "0"
    mcnk_idx:       str = "0"
    texture_blp:    str = ""
    effect_id:      str = "0"
    layer_count:    str = "0"
    labeler:        str = ""
    label_date:     str = ""
    label:          str = ""
    confidence:     str = ""
    notes:          str = ""

    @classmethod
    def from_dict(cls, d: dict) -> "Row":
        # Default-fill any missing columns so partial files (older schema)
        # still load.
        return cls(**{c: d.get(c, "") for c in SCHEMA_COLUMNS})

    def to_dict(self) -> dict:
        return {c: getattr(self, c) for c in SCHEMA_COLUMNS}


class Corpus:
    """Owns the corpus rows + atomic writeback."""

    def __init__(self, path: str) -> None:
        self.path = path
        self.rows: List[Row] = []
        self._load()

    def _load(self) -> None:
        if not os.path.isfile(self.path):
            raise SystemExit(f"Corpus file not found: {self.path}")
        with open(self.path, "r", encoding="utf-8", newline="") as f:
            reader = csv.DictReader(f)
            missing = set(SCHEMA_COLUMNS) - set(reader.fieldnames or [])
            if missing:
                sys.stderr.write(
                    f"WARN: CSV missing columns {sorted(missing)} — "
                    "default-filling.\n"
                )
            for d in reader:
                self.rows.append(Row.from_dict(d))

    def save(self) -> None:
        """Atomic write: temp file in the same directory, then rename."""
        directory = os.path.dirname(os.path.abspath(self.path)) or "."
        fd, tmp = tempfile.mkstemp(prefix=".road_label_", suffix=".csv",
                                    dir=directory)
        try:
            with os.fdopen(fd, "w", encoding="utf-8", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=SCHEMA_COLUMNS,
                                         quoting=csv.QUOTE_MINIMAL)
                writer.writeheader()
                for r in self.rows:
                    writer.writerow(r.to_dict())
            os.replace(tmp, self.path)
        except Exception:
            try: os.unlink(tmp)
            except OSError: pass
            raise


@dataclass
class Session:
    """Mutable state of an active labeling session."""
    corpus: Corpus
    labeler: str
    queue:   List[int] = field(default_factory=list)  # row indices to label
    pos:     int = 0                                   # cursor into queue
    history: List[int] = field(default_factory=list)  # for undo

    @property
    def total(self) -> int:
        return len(self.queue)

    @property
    def done(self) -> int:
        return self.pos

    def current_row(self) -> Optional[Row]:
        if self.pos >= len(self.queue):
            return None
        return self.corpus.rows[self.queue[self.pos]]

    def label_current(self, label: str, confidence: str = "HIGH") -> None:
        row = self.current_row()
        if row is None:
            return
        row.label      = label
        row.labeler    = self.labeler
        row.label_date = datetime.date.today().isoformat()
        row.confidence = confidence
        self.history.append(self.queue[self.pos])
        self.pos += 1
        self.corpus.save()      # checkpoint every label

    def undo_last(self) -> None:
        if not self.history:
            return
        last_idx = self.history.pop()
        row = self.corpus.rows[last_idx]
        row.label = ""
        row.labeler = ""
        row.label_date = ""
        row.confidence = ""
        self.pos -= 1
        self.corpus.save()


def make_wago_link(row: Row) -> str:
    """Build a wago.tools URL pointing roughly at the MCNK."""
    # wago.tools/maps shows the WoW map by map_id + ADT chunk. Coords roughly:
    #   chunk_center_world_y = (32 - adt_y) * 533.33 + (mcnk in adt) offset
    # We give a generic map link; the user can navigate manually.
    return (f"https://wago.tools/maps/{row.map_id}?adt={row.adt_x},{row.adt_y}"
            f"&mcnk={row.mcnk_idx}")


class LabelerGUI(tk.Tk):
    """Tk GUI. Modal labeling: arrow keys / r / n / a / u / s / q / o.

    Key handlers:
      R — ROAD
      N — NOT_ROAD
      A — AMBIGUOUS
      U — undo last label
      O — open wago.tools link for current row in browser
      S — save (also auto-saved per label, this is for paranoia)
      Q — quit
      Shift+R/N/A — same label but confidence=LOW
      Ctrl+R/N/A — same label but confidence=MEDIUM
    """

    BG_BY_KIND = {
        "easy_positive":  "#dfffdf",
        "hard_positive":  "#fff5d0",
        "hard_negative":  "#ffd0d0",
        "control":        "#d0d0ff",
    }

    def __init__(self, session: Session):
        super().__init__()
        self.session = session
        self.title("Road Label — TrinityCore P1.0b ground-truth tool")
        self.geometry("900x600")
        self.option_add("*Font", "TkDefaultFont 11")

        self._build_widgets()
        self._bind_keys()
        self._refresh()

    def _build_widgets(self) -> None:
        # Top: progress bar + counter.
        top = ttk.Frame(self, padding=8)
        top.pack(fill="x")
        self.progress_var = tk.StringVar(value="")
        ttk.Label(top, textvariable=self.progress_var).pack(side="left")
        self.progress_bar = ttk.Progressbar(top, length=400, mode="determinate")
        self.progress_bar.pack(side="right", padx=8)

        # Center: row context.
        center = ttk.Frame(self, padding=12)
        center.pack(fill="both", expand=True)

        self.kind_label = tk.Label(center, text="", font=("TkHeadingFont", 14, "bold"),
                                    padx=12, pady=8)
        self.kind_label.pack(fill="x")

        self.zone_label = tk.Label(center, text="", font=("TkDefaultFont", 12),
                                    wraplength=860, justify="left")
        self.zone_label.pack(fill="x", anchor="w", pady=(8, 4))

        grid = ttk.Frame(center)
        grid.pack(fill="x", anchor="w", pady=8)

        self._kv_labels: dict[str, tk.StringVar] = {}
        rows = [
            ("Map id / ADT (x,y)", "map_adt"),
            ("MCNK index",         "mcnk"),
            ("Texture (dominant)", "texture"),
            ("Effect ID",          "effect"),
            ("Layer count",        "layers"),
        ]
        for r, (label_text, key) in enumerate(rows):
            ttk.Label(grid, text=label_text + ":",
                      width=22, anchor="e").grid(row=r, column=0, sticky="e", padx=(0,8))
            v = tk.StringVar()
            ttk.Label(grid, textvariable=v, font=("TkFixedFont", 11),
                      anchor="w").grid(row=r, column=1, sticky="w")
            self._kv_labels[key] = v

        # Wago.tools link.
        self.link_label = tk.Label(center, text="", fg="blue",
                                    cursor="hand2", anchor="w")
        self.link_label.pack(fill="x", anchor="w", pady=(8, 4))
        self.link_label.bind("<Button-1>", lambda _e: self._open_link())

        # Bottom: help + status.
        bottom = ttk.Frame(self, padding=8)
        bottom.pack(fill="x", side="bottom")
        help_text = (
            "Keys:  R=ROAD  N=NOT_ROAD  A=AMBIGUOUS  "
            "U=undo  O=open wago link  S=save  Q=quit  "
            "Shift+key=confidence LOW  Ctrl+key=confidence MEDIUM"
        )
        ttk.Label(bottom, text=help_text, foreground="#666").pack(side="left")
        self.status_var = tk.StringVar(value="")
        ttk.Label(bottom, textvariable=self.status_var,
                   foreground="#080").pack(side="right")

    def _bind_keys(self) -> None:
        self.bind("r", lambda _e: self._label("ROAD",      "HIGH"))
        self.bind("n", lambda _e: self._label("NOT_ROAD",  "HIGH"))
        self.bind("a", lambda _e: self._label("AMBIGUOUS", "HIGH"))
        self.bind("R", lambda _e: self._label("ROAD",      "LOW"))
        self.bind("N", lambda _e: self._label("NOT_ROAD",  "LOW"))
        self.bind("A", lambda _e: self._label("AMBIGUOUS", "LOW"))
        self.bind("<Control-r>", lambda _e: self._label("ROAD",      "MEDIUM"))
        self.bind("<Control-n>", lambda _e: self._label("NOT_ROAD",  "MEDIUM"))
        self.bind("<Control-a>", lambda _e: self._label("AMBIGUOUS", "MEDIUM"))
        self.bind("u", lambda _e: self._undo())
        self.bind("o", lambda _e: self._open_link())
        self.bind("s", lambda _e: self._save())
        self.bind("q", lambda _e: self._quit())
        self.protocol("WM_DELETE_WINDOW", self._quit)

    def _refresh(self) -> None:
        row = self.session.current_row()
        if row is None:
            messagebox.showinfo("Done",
                f"All {self.session.total} candidates labeled. Run "
                "road_validator next.")
            self._quit()
            return

        # Progress.
        self.progress_var.set(
            f"Labeled {self.session.done} / {self.session.total}")
        self.progress_bar["maximum"] = max(1, self.session.total)
        self.progress_bar["value"]   = self.session.done

        # Determine case kind from notes ("[easy_positive]" etc.).
        kind = "unknown"
        if "[" in row.notes and "]" in row.notes:
            kind = row.notes.rsplit("[", 1)[-1].rstrip("]").strip()

        bg = self.BG_BY_KIND.get(kind, "#f0f0f0")
        self.kind_label.configure(
            text=f"Case kind: {kind}", background=bg)

        self.zone_label.configure(text=row.notes or "(no zone label)")

        self._kv_labels["map_adt"].set(
            f"{row.map_id}   ({row.adt_x}, {row.adt_y})")
        self._kv_labels["mcnk"].set(row.mcnk_idx)
        self._kv_labels["texture"].set(row.texture_blp)
        self._kv_labels["effect"].set(row.effect_id)
        self._kv_labels["layers"].set(row.layer_count)

        self.link_label.configure(
            text=f"Open in browser: {make_wago_link(row)}")

    def _label(self, label: str, confidence: str) -> None:
        self.session.label_current(label, confidence)
        self.status_var.set(f"Labeled {label} ({confidence}). Saved.")
        self._refresh()

    def _undo(self) -> None:
        if not self.session.history:
            self.status_var.set("Nothing to undo.")
            return
        self.session.undo_last()
        self.status_var.set("Undid last label.")
        self._refresh()

    def _open_link(self) -> None:
        row = self.session.current_row()
        if row is not None:
            webbrowser.open(make_wago_link(row))

    def _save(self) -> None:
        self.session.corpus.save()
        self.status_var.set("Manually saved.")

    def _quit(self) -> None:
        self.session.corpus.save()
        self.destroy()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Label road-corpus candidates interactively.")
    parser.add_argument("--corpus", required=True,
                        help="Path to candidate CSV (from road_corpus_dumper).")
    parser.add_argument("--labeler", default=os.environ.get("USER", "unknown"),
                        help="Labeler name recorded with each label.")
    parser.add_argument("--skip-labeled", action="store_true", default=True,
                        help="Skip rows that already have a label (default).")
    parser.add_argument("--no-skip-labeled", dest="skip_labeled",
                        action="store_false",
                        help="Include already-labeled rows.")
    parser.add_argument("--start-at", type=int, default=0,
                        help="Start at row index N (0-based).")
    parser.add_argument("--list-only", action="store_true",
                        help="Print summary stats and exit.")
    args = parser.parse_args()

    corpus = Corpus(args.corpus)

    if args.list_only:
        total = len(corpus.rows)
        labeled = sum(1 for r in corpus.rows if r.label)
        by_label: dict[str, int] = {}
        for r in corpus.rows:
            by_label[r.label or "(empty)"] = by_label.get(r.label or "(empty)", 0) + 1
        print(f"Corpus: {args.corpus}")
        print(f"  total rows: {total}")
        print(f"  labeled:    {labeled}")
        print(f"  remaining:  {total - labeled}")
        print("  by label:")
        for k, v in sorted(by_label.items()):
            print(f"    {k:>12}  {v}")
        return 0

    queue: List[int] = []
    for i, r in enumerate(corpus.rows):
        if i < args.start_at:
            continue
        if args.skip_labeled and r.label:
            continue
        queue.append(i)

    if not queue:
        print("Nothing to label — corpus is fully labeled or queue is empty.")
        return 0

    print(f"Starting labeler GUI: {len(queue)} candidates queued "
          f"(out of {len(corpus.rows)} total).")
    print("Labels save after each keypress; close the window or press Q to exit.")

    session = Session(corpus=corpus, labeler=args.labeler, queue=queue)
    gui = LabelerGUI(session)
    gui.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
