# OUR_idx295 UPDATE_OBJECT Entity Inventory

- Total CREATE blocks parsed: **15**
- Unparsed (slurped raw): **0**
- Non-CREATE blocks (values/oor/near): **0**

## Breakdown by HighGuid

| HighGuid | Name | Count |
|---|---|---|
| 3 | Item | 8 |
| 55 | Housing | 5 |
| 30 | BNetAccount | 1 |
| 2 | Player | 1 |

## Breakdown by (HighGuid, ObjectType)

| HighGuid | HGName | ObjType | OTName | Count |
|---|---|---|---|---|
| 3 | Item | 1 | Item | 8 |
| 55 | Housing | 18 | HousingEntity | 5 |
| 30 | BNetAccount | 18 | HousingEntity | 1 |
| 2 | Player | 7 | ActivePlayer | 1 |

## Top 20 fragment signatures

| Count | Fragments |
|---|---|
| 8 | `CGObject, Tag_Item` |
| 4 | `FHousingPlayerHouse_C` |
| 1 | `FHousingStorage_C` |
| 1 | `FNeighborhoodMirrorData_C` |
| 1 | `CGObject, PlayerHouseInfoComponent_C, PlayerInitiativeComponent_C, Tag_Unit, Tag_Player` |

## Entities by role

| Role | Count |
|---|---|
| Item (HighGuid=3) | 8 |
| Housing/3 identity (FHousingPlayerHouse_C) | 4 |
| BNetAccount (HighGuid=30) -- non-standard for map entry | 1 |
| Housing/4 neighborhood-mirror | 1 |
| Player (HighGuid=2) | 1 |
