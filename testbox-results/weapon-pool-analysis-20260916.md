# Warrior weapon pool analysis (Sellarino) — 2026-09-16

Answering: is item 25 (Worn Shortsword, white ilvl 1) on L16 Sellarino a pool
filter problem or a scoring problem?

## Method
Parsed base client db2 directly (DB has only hotfix overrides):
- `Item.db2` → ClassID / SubclassID / InventoryType (pallet-compressed, fixed
  records; 153741 copy records resolved).
- `ItemSparse.db2` → Bonding / RequiredLevel / OverallQualityID / ItemLevel.
  This is a SPARSE table with 5 inline null-terminated strings at the record
  start; a naive fixed-offset read gives ~34% garbage. A per-record string walk
  (numeric fields sit at S + (nominal_byte - 20), S = total string bytes) gives
  99% in-range decode. Validated: item 25 = class2/sub7(sword)/invtype21,
  B=BoE RL1 ilvl1 Q=common; Hearthstone and Thunderfury also decode correctly.

Filter modelled: pool DROPS an item when `Bonding == BIND_ON_ACQUIRE(1) AND
OverallQualityID < RARE(3)`.

## Sellarino
- primarySpecialization = **73 (Protection)**. Offhand 200993 = **rare shield**
  (class4/sub6/invtype14). So 1H mainhand + shield is the STRUCTURALLY CORRECT
  Prot loadout — not a 1H-vs-2H error. The defect is only the 1H item chosen.

## Result: the pool is NOT starved → SCORING problem

Warrior 1H swords (subclass 7), RequiredLevel<=16, that SURVIVE the filter:
- All ilvl:            273 (RL1..16) + 119 (RL0) — rare-dominated.
- REAL L16 pool (also ItemLevel<=40): **324** — poor 9, common 25, uncommon 28,
  **rare 199**, epic 38, artifact 15, heirloom 10.
- ALL warrior 1H-melee (axe/mace/sword/fist/dagger), RL<=16 & ilvl<=40, survive:
  **1129** — rare 680, epic 108, uncommon 124, whites 136.

So the scorer had ~237 uncommon-or-better appropriate 1H swords (of 324 total low
candidates) and still equipped a white ilvl-1 starter. That is a scoring bug, not
pool starvation.

## The filter DOES cost level-appropriate greens (worth relaxing anyway)
1H swords RL<=16 DROPPED by the filter: 286 (poor 4, common 27, **uncommon 255**).
The 255 BoP uncommons are exactly the quest greens. Relaxing the filter for low
brackets is still worthwhile — it just isn't why item 25 was picked.

## Filter survival by slot (RequiredLevel<=16, valid decode)
| set | total | survive | % | dropped (BoP<rare) |
|---|--:|--:|--:|--:|
| warrior weapons | 4837 | 2536 | 52% | 2301 |
| head (inv 1) | 2608 | 1306 | 50% | 1302 |
| shoulder (inv 3) | 2092 | 1096 | 52% | 996 |
| back (inv 16) | 1758 | 976 | 55% | 782 |
| finger (inv 11) | 982 | 537 | 54% | 445 |
| trinket (inv 12) | 691 | 360 | 52% | 331 |

The filter removes ~half of low candidates uniformly across slots — consistent
with the dev-box slot-fill deficits being partly filter-driven — but the specific
item-25 weapon miss is scoring.
