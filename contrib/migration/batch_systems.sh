#!/bin/bash
WT="${MIGRATE_WT:-I:/TrinityCore/_migrate121}"
SC="${MIGRATE_TOOLS:-$(cd "$(dirname "$0")" && pwd)}"   # toolkit dir (also used for *.log/result output)
# "SYSDIR NS PUSHBRANCH"
CONF=(
 "Warfronts Warfront warfronts"
 "CraftingOrders CraftingOrders crafting-orders"
 "Prey Prey prey-voidforge"
 "Clubs Club clubs"
 "BattlePay BattlePay battlepay"
)
> "$SC/batch_result.txt"
for entry in "${CONF[@]}"; do
  read -r SYS NS BR <<< "$entry"
  echo "==== $SYS ($NS -> feature/$BR) ====" | tee -a "$SC/batch_result.txt"
  bash "$SC/reconstruct_system.sh" "$SYS" "$NS" >/dev/null 2>&1
  git -C "$WT" checkout --detach "migrated/$SYS" >/dev/null 2>&1
  cmake -S "$WT" -B "$WT/build" >/dev/null 2>&1
  cmake --build "$WT/build" --config RelWithDebInfo --target worldserver --parallel 6 > "$SC/batch_${SYS}.log" 2>&1
  if [ -f "$WT/build/bin/RelWithDebInfo/worldserver.exe" ] && ! grep -qE "error C[0-9]|LNK[0-9]|cannot open" "$SC/batch_${SYS}.log"; then
    git -C "$WT" add src/ >/dev/null 2>&1
    git -C "$WT" -c user.name="Johannes Zoeller" -c user.email="johannes.zoeller@cws.com" commit -q -m "Reconstruct $SYS onto TC 12.1.0 (build 69404)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
    git -C "$WT" branch -f "migrated/$SYS" HEAD >/dev/null 2>&1
    if git -C "$WT" push --force-with-lease origin "migrated/$SYS:refs/heads/feature/$BR" >/dev/null 2>&1; then
      echo "  $SYS BUILT+PUSHED" | tee -a "$SC/batch_result.txt"
    else
      git -C "$WT" push origin "migrated/$SYS:refs/heads/feature/$BR" >/dev/null 2>&1 && echo "  $SYS BUILT+PUSHED(new)" | tee -a "$SC/batch_result.txt" || echo "  $SYS BUILT, PUSH FAILED" | tee -a "$SC/batch_result.txt"
    fi
  else
    echo "  $SYS BUILD FAILED: $(grep -cE 'error C[0-9]|LNK[0-9]|cannot open' "$SC/batch_${SYS}.log") errs" | tee -a "$SC/batch_result.txt"
  fi
done
echo "==== BATCH DONE ====" | tee -a "$SC/batch_result.txt"
