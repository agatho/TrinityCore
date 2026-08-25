#!/bin/bash
WT="${MIGRATE_WT:-I:/TrinityCore/_migrate121}"
SC="${MIGRATE_TOOLS:-$(cd "$(dirname "$0")" && pwd)}"
B="git --git-dir=${MIGRATE_BARE:-I:/TrinityCore/.bare}"
export GIT_EDITOR=true
> "$SC/iso_clean.txt"; > "$SC/iso_conflict.txt"; > "$SC/iso_empty.txt"
git -C "$WT" cherry-pick --abort 2>/dev/null
while IFS=$'\t' read -r h s; do
  [ -z "$h" ] && continue
  git -C "$WT" reset --hard >/dev/null 2>&1; git -C "$WT" clean -fd src/ sql/ >/dev/null 2>&1
  git -C "$WT" checkout --detach upstream/master >/dev/null 2>&1
  if git -C "$WT" cherry-pick -x "$h" >/dev/null 2>&1; then echo "$h	$s" >> "$SC/iso_clean.txt"
  elif git -C "$WT" diff --name-only --diff-filter=U 2>/dev/null | grep -q .; then echo "$h	$s	[$(git -C "$WT" diff --name-only --diff-filter=U | tr '\n' ',')]" >> "$SC/iso_conflict.txt"
  else echo "$h	$s" >> "$SC/iso_empty.txt"; fi
  git -C "$WT" cherry-pick --abort 2>/dev/null
done < "$SC/orphan_keep2.tsv"
echo "CLEAN=$(wc -l < "$SC/iso_clean.txt")  CONFLICT=$(wc -l < "$SC/iso_conflict.txt")  EMPTY(in 12.1)=$(wc -l < "$SC/iso_empty.txt")"
