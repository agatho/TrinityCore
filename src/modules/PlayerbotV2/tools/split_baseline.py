#!/usr/bin/env python3
"""One-shot refactor helper: split monolithic Apl_Baseline.cpp into
per-class Apl_Baseline_<Class>.cpp files. After this script runs the
original file is reduced to its top-level RegisterApl_Baseline()
aggregator. Each per-class TU is independently editable so parallel
coverage work can proceed without merge conflicts.

Run once from repo root: python src/modules/PlayerbotV2/tools/split_baseline.py
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC  = ROOT / "modules/PlayerbotV2/Combat/Apl/Apl_Baseline.cpp"
DST_DIR = SRC.parent

CLASS_REGISTER_FN = {
    "warrior":  "RegisterApl_Baseline_Warrior",
    "paladin":  "RegisterApl_Baseline_Paladin",
    "hunter":   "RegisterApl_Baseline_Hunter",
    "rogue":    "RegisterApl_Baseline_Rogue",
    "priest":   "RegisterApl_Baseline_Priest",
    "dk":       "RegisterApl_Baseline_DK",
    "shaman":   "RegisterApl_Baseline_Shaman",
    "mage":     "RegisterApl_Baseline_Mage",
    "warlock":  "RegisterApl_Baseline_Warlock",
    "monk":     "RegisterApl_Baseline_Monk",
    "druid":    "RegisterApl_Baseline_Druid",
    "dh":       "RegisterApl_Baseline_DH",
    "evoker":   "RegisterApl_Baseline_Evoker",
}

# CLASS_* constant used by RegisterRotation per class.
CLASS_CONSTANT = {
    "warrior":  "CLASS_WARRIOR",
    "paladin":  "CLASS_PALADIN",
    "hunter":   "CLASS_HUNTER",
    "rogue":    "CLASS_ROGUE",
    "priest":   "CLASS_PRIEST",
    "dk":       "CLASS_DEATH_KNIGHT",
    "shaman":   "CLASS_SHAMAN",
    "mage":     "CLASS_MAGE",
    "warlock":  "CLASS_WARLOCK",
    "monk":     "CLASS_MONK",
    "druid":    "CLASS_DRUID",
    "dh":       "CLASS_DEMON_HUNTER",
    "evoker":   "CLASS_EVOKER",
}

# Capture each top-level `namespace baseline_<cls> { ... }` block. The
# closing brace is the FIRST line that's just "}" at column 0 after the
# opening — Apl_Baseline.cpp follows that convention. Optional leading
# comment block precedes the namespace and we include it.
PER_CLASS = re.compile(
    r"(?:^// ====+\n// [A-Z][A-Z0-9 \(\)]+\n// ====+\n(?:^//[^\n]*\n)*)?"
    r"^namespace baseline_(\w+) \{\n(.*?)^\}\n",
    re.DOTALL | re.MULTILINE,
)


def main() -> int:
    text = SRC.read_text(encoding="utf-8")
    extracted = 0
    for m in PER_CLASS.finditer(text):
        prelude = m.group(0)[:m.start(2) - m.start(0)]
        # Strip the closing "\n}\n" from prelude — that belongs to the body
        # capture. PER_CLASS keeps the leading-comment block + "namespace
        # baseline_xxx {\n" only inside `prelude` here.
        prelude = prelude.rstrip()
        cls_slug = m.group(1)
        body     = m.group(2)
        out_path = DST_DIR / f"Apl_Baseline_{cls_slug.capitalize()}.cpp"
        register_fn = CLASS_REGISTER_FN[cls_slug]
        cls_const   = CLASS_CONSTANT[cls_slug]
        # Detect the kRules array name inside the body. It's always
        # `ApRule const kRules[] = { ... };` in the existing code.
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(
                f"// {out_path.name} — baseline rotation for class "
                f"{cls_const} (spec=0). Extracted from the monolithic "
                f"Apl_Baseline.cpp on the split refactor; future edits go\n"
                f"// here exclusively. See Apl_Baseline_Common.h for the\n"
                f"// shared helpers + rule macros.\n"
                f"//\n"
                f"// To audit coverage:\n"
                f"//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py\n"
                f"\n"
                f"#include \"Apl_Baseline_Common.h\"\n"
                f"\n"
                f"namespace Playerbot::Combat {{\n"
                f"\n"
                f"namespace {{\n"
                f"\n"
                f"using ::Playerbot::Combat::baseline_common::HasLiveTarget;\n"
                f"using ::Playerbot::Combat::baseline_common::AlwaysInCombat;\n"
                f"using ::Playerbot::Combat::baseline_common::DoAutoAttack;\n"
                f"\n"
            )
            # Trim the original prelude's "namespace baseline_xxx {" line —
            # we already opened our own anonymous namespace above. Re-emit
            # the comment block (everything before the namespace opener).
            comment_end = prelude.rfind("namespace baseline_")
            if comment_end > 0:
                f.write(prelude[:comment_end])
            # The body has internal indentation (4 spaces for the original
            # namespace body). De-indent by one level.
            for line in body.splitlines(keepends=True):
                if line.startswith("    "):
                    f.write(line[4:])
                else:
                    f.write(line)
            f.write(
                f"\n}} // anonymous\n"
                f"\n"
                f"void {register_fn}()\n"
                f"{{\n"
                f"    RegisterRotation({cls_const}, 0, "
                f"ApRotation{{baseline_{cls_slug}_kRules}});\n"
                f"}}\n"
                f"\n"
                f"}} // namespace Playerbot::Combat\n"
            )
        # Body referred to `kRules` (anonymous namespace scope). After our
        # de-indent the kRules array is at file scope inside the anonymous
        # namespace, so its name is still `kRules`. But we want it to be
        # unique per class TU (avoid LNK collisions if two classes share an
        # internal anonymous-namespace symbol via the umbrella library).
        # Rename `kRules` → `baseline_<cls>_kRules` here.
        out_text = out_path.read_text(encoding="utf-8")
        out_text = out_text.replace("ApRule const kRules[]",
                                    f"ApRule const baseline_{cls_slug}_kRules[]")
        out_path.write_text(out_text, encoding="utf-8")
        print(f"[split] {out_path.name}  ({len(body):,} chars)")
        extracted += 1

    # Rewrite Apl_Baseline.cpp into a minimal aggregator. Keep the top
    # header comment for context; drop all per-class namespaces.
    new_body = (
        "// Apl_Baseline.cpp — registers per-class baseline rotations\n"
        "// (spec=0) for L1-9 pre-spec characters and bots with corrupt /\n"
        "// missing ChrSpecialization. Per-class rule sets live in the\n"
        "// sibling Apl_Baseline_<Class>.cpp files; this file is now just\n"
        "// the aggregator that wires the 13 per-class registration calls\n"
        "// into a single entry point that ApRegistry::RegisterAllRotations\n"
        "// invokes at module init.\n"
        "//\n"
        "// See Apl_Baseline_Common.h for shared helpers + rule macros, and\n"
        "// src/modules/PlayerbotV2/tools/baseline_coverage_audit.py for the\n"
        "// wago.tools-driven coverage audit.\n"
        "\n"
        "#include \"Apl_Baseline_Common.h\"\n"
        "\n"
        "namespace Playerbot::Combat {\n"
        "\n"
        "void RegisterApl_Baseline()\n"
        "{\n"
    )
    for slug in CLASS_REGISTER_FN:
        new_body += f"    RegisterApl_Baseline_{slug.capitalize()}();\n"
    # DK and DH use a different casing in the function name (RegisterApl_Baseline_DK).
    # Fix those two specifically: replace _Dk -> _DK, _Dh -> _DH.
    new_body = new_body.replace("RegisterApl_Baseline_Dk()",
                                "RegisterApl_Baseline_DK()")
    new_body = new_body.replace("RegisterApl_Baseline_Dh()",
                                "RegisterApl_Baseline_DH()")
    new_body += (
        "}\n"
        "\n"
        "} // namespace Playerbot::Combat\n"
    )
    SRC.write_text(new_body, encoding="utf-8")
    print(f"[split] rewrote {SRC.name} as aggregator")
    print(f"[split] extracted {extracted} per-class files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
