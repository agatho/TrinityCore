# Gearing fixes verification — d6687708c2 — 2026-09-16

Deployed integration/12_1_with-bots @ d6687708c2, rebuilt clean, running rev
**d6687708c29c**. 224 bots online. No crashes (only pre-existing data warnings).

## 1. Pool size (filter fix) — CONFIRMED
`[BotGearGenerator] indexed` line: **80293 → 102906** items (of 175898), +22613
(+28%). Admitting BoP greens materially enlarged the pool, confirming the filter
was the constraint behind the slot-fill deficits. Rejected-no-appearance: 3575.

## 2. slot_outlier churn — LARGE FRACTION (flagging for review)
**119 of 224 online bots (~53%)** trip slot_outlier on the first passes — not a
handful. worst_slot distribution among them:

| worst_slot | count | slot |
|--:|--:|---|
| 13 | 96 | TRINKET1 |
| 11 | 61 | FINGER1 |
| 1  | 54 | NECK |
| 16 | 32 | OFFHAND |
| 15 | 23 | MAINHAND |
| 10 | 11 | BACK |
| 9/8/6 | 5 | hands/wrist/legs |

Dominated by jewelry/neck — consistent with genuine single-slot gaps (the
finger2/trinket2 dedup bug left those slots empty/mismatched), so this may be
legitimate rather than over-sensitive. But it IS a large fraction; your call on
whether the one-third-of-own-average threshold wants tightening. Churn is bounded
(25 re-gears/pass cap + 30-min park cooldown), so it won't spin.

Re-gear outcomes this boot: **73 succeeded, 87 parked** ("0 swaps this pass —
bags full / CanEquipItem refused: proficiency/level/unique — parked 30min").

## 3. Endgame picks — NO REGRESSION
Re-geared L90 bots stayed full epic (greens did NOT displace rares/epics):
- Duvall (hunter L90): epic 15/15, avg_eff 277
- Meihua (hunter L90): epic 15/15, avg_eff 277
- Donathar (warrior L90): epic 15/15, avg_eff 258
- Controls Folander/Balin (not re-geared): epic 13/13.

## Sellarino — detected correctly, but swap FAILS
`[GearBackfill] candidate Sellarino L17 ... slot_outlier=true worst_slot=15
worst_ilvl=1` — exactly as predicted (mainhand = the ilvl-1 white sword). BUT:
`Sellarino L17 under-geared but 0 swaps this pass (bags full / every slot's
CanEquipItem refused) - parked 30min`. The mainhand was NOT replaced — detection
works, the swap can't land because bags are full. Follow-up: the slot_outlier
re-gear needs a bag-free path (or to free a slot) for bag-full bots, else the
worst offenders (which tend to be the most-neglected, bag-cluttered bots) never
get fixed. Sellarino stays a 1H+shield Prot (spec 73), correct structure.

## 270575 cloak question — SETTLED
The generator's template-form gate (GetDefaultItemModifiedAppearance) rejects
270575 and the other flagged back-slot entries — matching the offline ItemID
scan. But the EQUIPPED owned-form check (IsItemRenderableInSlot via
GetVisibleEntry/GetVisibleAppearanceModId) renders it: the heal never removed it,
Folander still wears epic 270575 after this boot. So the two forms legitimately
differ; equipped copies are not a cold-inspect crash hazard.
