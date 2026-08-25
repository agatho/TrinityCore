#!/bin/bash
# usage: rescue_system.sh <SystemName> <commits_oldest_file>
SYS="$1"; LIST="$2"
WT="I:/TrinityCore/_migrate121"; R="C:/Users/daimon/AppData/Local/Temp/claude/c--dumps/d61f229a-840a-4add-ab28-452509f4a22b/scratchpad/resolve.py"
B="git --git-dir=I:/TrinityCore/.bare"; export GIT_EDITOR=true
git -C "$WT" cherry-pick --abort 2>/dev/null; git -C "$WT" reset --hard >/dev/null 2>&1; git -C "$WT" clean -fd src/ sql/ >/dev/null 2>&1
git -C "$WT" checkout --detach upstream/master >/dev/null 2>&1
git -C "$WT" config user.name "Johannes Zoeller"; git -C "$WT" config user.email "johannes.zoeller@cws.com"
ap=0; skipped=""
while read -r h; do
  [ -z "$h" ] && continue
  if git -C "$WT" cherry-pick -x "$h" >/dev/null 2>&1; then ap=$((ap+1)); continue; fi
  U=$(git -C "$WT" diff --name-only --diff-filter=U 2>/dev/null)
  if [ -z "$U" ]; then git -C "$WT" cherry-pick --skip >/dev/null 2>&1; continue; fi  # empty
  bad=0
  for f in $U; do
    case "$f" in
      *Protocol/Opcodes*|*DataStores/DB2Metadata*) python "$R" "$WT/$f" ours ;;                                                                  # 12.1 already has these -> take 12.1
      *DataStores/DB2*|*Implementation/HotfixDatabase*|*Implementation/CharacterDatabase*|sql/*) python "$R" "$WT/$f" both ;;                     # additive union (system's new stores/stmts)
      src/server/game/$SYS/*|*${SYS}Packets*|*${SYS}Handler*|*${SYS}Mgr*) python "$R" "$WT/$f" theirs ;;                                          # system-owned -> incoming
      *) python "$R" "$WT/$f" both ;;                                                                                                             # default union
    esac
    git -C "$WT" add "$f" 2>/dev/null
  done
  if git -C "$WT" -c core.editor=true cherry-pick --continue >/dev/null 2>&1; then ap=$((ap+1)); else git -C "$WT" cherry-pick --skip >/dev/null 2>&1; skipped="$skipped $($B log -1 --format='%h' $h)"; fi
done < "$LIST"
echo "$SYS: applied=$ap skipped=[$skipped]"
git -C "$WT" branch -f migrated/$SYS HEAD >/dev/null 2>&1
