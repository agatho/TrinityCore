# Parked-swap diagnosis — 5f66ef4ea4 — 2026-09-16

Running rev **5f66ef4ea459**, ~fleet online. This boot: **87 re-geared, 25 parked**
(new format) — much healthier than the earlier 73/87 split (the pool-filter fix
admitted enough candidates to clear most parks).

## Reason tally across the 25 parked bots
| count | reason (for the worst/outlier slot) |
|--:|---|
| 13 | quality guard: worn item is same or better quality |
| 8  | generator re-picked the worn item |
| 4  | staged to bags but CanEquipItem refused: err=N |

- **13 "quality guard"** = not a bug: the worst slot's worn item is already as
  good as the generator's pick, so there is nothing to upgrade.
- **8 "generator re-picked the worn item"** = your hypothesis, confirmed for 8
  bots: the scorer ranked the SAME worn entry top, so the entry-equality guard
  produces a silent zero-swap. Worth chasing if those 8 are genuinely
  under-geared on that slot (vs already-optimal).
- **4 "CanEquipItem refused"** = the pick is generated and staged but the equip
  hard-fails. Includes Sellarino.

So the dominant parked reason is "already fine" (13), then the scoring dupe (8),
then unequippable picks (4). Only ~12 of 25 parks are genuinely problematic.

## Sellarino — verbatim, and it REFUTES the re-pick hypothesis
```
[GearBackfill] Sellarino L17 under-geared but 0 swaps this pass - parked 30min | worst slot 15 (worn ilvl 1): pick 218333 -> staged to bags but CanEquipItem refused: err=10
```
- Generator pick for the mainhand = **218333**, NOT item 25. It is a **rare 1H
  sword, RequiredLevel 10** — a genuinely appropriate upgrade for L17. So the
  SCORING is fine here; it did pick a proper rare one-hander.
- err=10 = **EQUIP_ERR_CANT_EQUIP_EVER** ("you can never use that item").
- I decoded 218333's AllowableClass = -1 (all classes) and AllowableRace = all —
  so it is **NOT** class- or race-restricted. CANT_EQUIP_EVER is coming from some
  OTHER gate CanEquipNewItem enforces (required skill / required spec / gender /
  an item flag) that the generator's pre-filter does not replicate.

### Takeaway for the fix
The generator's candidate pre-filter is less strict than CanEquipItem: it stages
items that pass class/quality/ilvl/proficiency but hard-fail CANT_EQUIP_EVER for a
deeper reason. For Sellarino specifically the scorer is NOT at fault (218333 is a
correct rare 1H upgrade) — the pool admits an item the bot can never wear. Suggest
running CanEquipNewItem (or the relevant subset) inside the generator's candidate
filter so CANT_EQUIP_EVER items are excluded before scoring, rather than discovered
at swap time. That would also fix the other 3 CanEquipItem-refused parks.
