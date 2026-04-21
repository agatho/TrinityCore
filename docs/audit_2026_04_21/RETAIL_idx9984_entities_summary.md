# RETAIL_idx9984 UPDATE_OBJECT Entity Inventory

- Total CREATE blocks parsed: **494**
- Unparsed (slurped raw): **28**
- Non-CREATE blocks (values/oor/near): **0**

## Breakdown by HighGuid

| HighGuid | Name | Count |
|---|---|---|
| 3 | Item | 353 |
| 55 | Housing | 52 |
| 11 | GameObject | 51 |
| 8 | Creature | 24 |
| 57 | Entity | 8 |
| 56 | MeshObject | 3 |
| 13 | AreaTrigger | 2 |
| 2 | Player | 1 |

## Breakdown by (HighGuid, ObjectType)

| HighGuid | HGName | ObjType | OTName | Count |
|---|---|---|---|---|
| 3 | Item | 1 | Item | 328 |
| 55 | Housing | 18 | HousingEntity | 52 |
| 11 | GameObject | 8 | GameObject | 51 |
| 3 | Item | 2 | Container | 24 |
| 8 | Creature | 5 | Unit | 24 |
| 57 | Entity | 18 | HousingEntity | 8 |
| 56 | MeshObject | 14 | MeshObject | 3 |
| 13 | AreaTrigger | 11 | AreaTrigger | 2 |
| 3 | Item | 4 | AzeriteItem | 1 |
| 2 | Player | 7 | ActivePlayer | 1 |

## Top 20 fragment signatures

| Count | Fragments |
|---|---|
| 328 | `CGObject, Tag_Item` |
| 46 | `FHousingPlayerHouse_C` |
| 28 | `<UNPARSED>` |
| 27 | `CGObject, Tag_GameObject` |
| 24 | `CGObject, Tag_Item, Tag_Container` |
| 23 | `CGObject, FJamHousingCornerstone_C, Tag_GameObject` |
| 5 | `FHousingRoom_C, FMirroredPositionData_C, Tag_HousingRoom` |
| 4 | `FMirroredPositionData_C, Tag_HouseExteriorPiece, Tag_HouseExteriorRoot` |
| 4 | `FMirroredPositionData_C` |
| 1 | `CGObject, Tag_Item, Tag_AzeriteItem` |
| 1 | `CGObject, PlayerHouseInfoComponent_C, PlayerInitiativeComponent_C, Tag_Unit, Tag_Player` |
| 1 | `FNeighborhoodMirrorData_C` |
| 1 | `CGObject, FHousingPlotAreaTrigger_C, Tag_AreaTrigger` |
| 1 | `CGObject, FMirroredPositionData_C, Tag_GameObject` |

## Entities by role

| Role | Count |
|---|---|
| Item (HighGuid=3) | 353 |
| GameObject (HighGuid=11) | 51 |
| Housing/3 identity (FHousingPlayerHouse_C) | 46 |
| Creature (HighGuid=8) | 24 |
| Entity mirror (HighGuid=57) | 8 |
| Housing/2 room (FHousingRoom_C) | 5 |
| MeshObject (HighGuid=56) | 3 |
| Player (HighGuid=2) | 1 |
| Housing/4 neighborhood-mirror | 1 |
| AreaTrigger plot (HighGuid=13) | 1 |
| AreaTrigger other (HighGuid=13) | 1 |
