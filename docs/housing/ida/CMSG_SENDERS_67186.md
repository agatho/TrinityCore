# IDA-Extracted CMSG Senders for WoW Retail Build 67186

Generated 2026-05-12 from dynamic IDA analysis of the WoW retail client (build 67186)
via `sub_7FF75EE9FF10` (`WritePacketHeader`) call-site immediate extraction.

Methodology: for each function in the housing-related code clusters (10 `Cli*System`
dispatcher anchors), find calls to `WritePacketHeader(packet, opcode)` and extract
the opcode immediate from the preceding `mov edx, imm32` instruction.

**This is the ground truth for build 67186 CMSG opcodes.** Any CMSG marked as
TC-CUSTOM speculative in Opcodes.h that does NOT appear here is by definition
fake — the client never sends it.

## Dispatcher anchors

| Address | Class | Group |
|---|---|---|
| 0x7ff75c1769c0 | CliNeighborhoodCharterSystem | 0x37 |
| 0x7ff75c176cf0 | CliNeighborhoodInitiativeSystem | 0x38 |
| 0x7ff75c177360 | CliNeighborhoodSystem | 0x39 |
| 0x7ff75c19d900 | CliHouseExteriorSystem | 0x2E |
| 0x7ff75c19db20 | CliHouseInteriorSystem | 0x2F |
| 0x7ff75c19dbb0 | CliHousingDecorSystem | 0x30 |
| 0x7ff75c19e3d0 | CliHousingFixtureSystem | 0x31 |
| 0x7ff75c1abda0 | CliHousingRoomSystem | 0x32 |
| 0x7ff75c1ac380 | CliHousingServicesSystem | 0x33 |
| 0x7ff75c1acd80 | CliHousingSystem | 0x35 |

## All discovered CMSG senders (80 total)

| Opcode | Sender address | Sender name |
|---|---|---|
| `0x002E0000` | 0x7ff75c19d990 | sub_7FF75C19D990 |
| `0x002E0001` | 0x7ff75c19daa0 | sub_7FF75C19DAA0 |
| `0x002F0001` | 0x7ff75c19db30 | sub_7FF75C19DB30 |
| `0x00300000` | 0x7ff75c19dbc0 | DECOR_SET_EDIT_MODE |
| `0x00300001` | 0x7ff75c19dce0 | DECOR_PLACE |
| `0x00300002` | 0x7ff75c19dec0 | DECOR_MOVE |
| `0x00300003` | 0x7ff75c19dfd0 | DECOR_REMOVE |
| `0x00300004` | 0x7ff75c19e0b0 | DECOR_LOCK (named) |
| `0x00300006` | 0x7ff75c19e1c0 | DECOR_SET_DYE_SLOTS |
| `0x00300009` | 0x7ff75c19e2b0 | DECOR_DELETE_FROM_STORAGE |
| `0x0030000E` | 0x7ff75c19e340 | DECOR_REQUEST_STORAGE |
| `0x00300010` | 0x7ff75c19e380 | DECOR_REDEEM_DEFERRED_DECOR |
| `0x00310000` | 0x7ff75c19e3e0 | FIXTURE_SET_EDIT_MODE |
| `0x00310003` | 0x7ff75c19e440 | FIXTURE_SET_HOUSE_SIZE |
| `0x00310004` | 0x7ff75c19e4d0 | FIXTURE_SET_HOUSE_TYPE |
| `0x00310005` | 0x7ff75c19e520 | FIXTURE_SET_CORE_FIXTURE |
| `0x00310006` | 0x7ff75c19e5e0 | FIXTURE_CREATE_FIXTURE |
| `0x00310007` | 0x7ff75c19e6b0 | FIXTURE_DELETE_FIXTURE |
| `0x00320000` | 0x7ff75c1abdb0 | ROOM_SET_LAYOUT_EDIT_MODE |
| `0x00320001` | 0x7ff75c1abe90 | ROOM_ADD |
| `0x00320002` | 0x7ff75c1abf20 | ROOM_REMOVE |
| `0x00320003` | 0x7ff75c1abf60 | ROOM_ROTATE |
| `0x00320004` | 0x7ff75c1ac030 | ROOM_MOVE |
| `0x00320005` | 0x7ff75c1ac120 | ROOM_SET_COMPONENT_THEME |
| `0x00320006` | 0x7ff75c1ac240 | ROOM_APPLY_COMPONENT_MATERIALS |
| `0x00320007` | 0x7ff75c1ac2e0 | ROOM_SET_DOOR_TYPE |
| `0x00320008` | 0x7ff75c1ac330 | ROOM_SET_CEILING_TYPE |
| `0x00330001` | 0x7ff75c1ac390 | SVCS_GUILD_CREATE_NEIGHBORHOOD |
| `0x00330007` | 0x7ff75c1ac420 | SVCS_NEIGHBORHOOD_RESERVE_PLOT |
| `0x0033000A` | 0x7ff75c1ac490 | SVCS_RELINQUISH_HOUSE |
| `0x0033000B` | 0x7ff75c1ac5a0 | SVCS_UPDATE_HOUSE_SETTINGS |
| `0x00330010` | 0x7ff75c1ac670 | SVCS_PLAYER_VIEW_HOUSES_BY_PLAYER |
| `0x00330011` | 0x7ff75c1ac6b0 | SVCS_PLAYER_VIEW_HOUSES_BY_BNET_ACCOUNT |
| `0x00330013` | 0x7ff75c1ac6f0 | SVCS_GET_PLAYER_HOUSES_INFO |
| `0x00330019` | 0x7ff75c1ac710 | SVCS_TELEPORT_TO_PLOT |
| `0x0033001A` | 0x7ff75c1ac780 | SVCS_START_TUTORIAL |
| `0x0033001E` | 0x7ff75c1ac7a0 | SVCS_ACCEPT_NEIGHBORHOOD_OWNERSHIP |
| `0x0033001F` | 0x7ff75c1ac7e0 | SVCS_REJECT_NEIGHBORHOOD_OWNERSHIP |
| `0x00330020` | 0x7ff75c1ac820 | SVCS_GET_POTENTIAL_HOUSE_OWNERS |
| `0x00330021` | 0x7ff75c1ac860 | SVCS_GET_HOUSE_FINDER_INFO |
| `0x00330022` | 0x7ff75c1ac880 | SVCS_GET_HOUSE_FINDER_NEIGHBORHOOD |
| `0x00330023` | 0x7ff75c1ac8c0 | SVCS_GET_BNET_FRIEND_NEIGHBORHOODS |
| `0x00330025` | 0x7ff75c1ac900 | SVCS_DELETE_ALL_NEIGHBORHOOD_INVITES |
| `0x00350005` | 0x7ff75c1acd90 | HOUSE_STATUS |
| `0x00350006` | 0x7ff75c1acdb0 | GET_CURRENT_HOUSE_INFO |
| `0x00350007` | 0x7ff75c1ace40 | GET_PLAYER_PERMISSIONS |
| `0x00350008` | 0x7ff75c1acec0 | RESET_KIOSK_MODE (no body) |
| `0x00370000` | 0x7ff75c1769d0 | CHARTER_OPEN_CONFIRMATION_UI |
| `0x00370001` | 0x7ff75c176a70 | CHARTER_CREATE |
| `0x00370003` | 0x7ff75c176b00 | CHARTER_EDIT |
| `0x00370004` | 0x7ff75c176b90 | CHARTER_FINALIZE |
| `0x00370006` | 0x7ff75c176bb0 | CHARTER_ADD_SIGNATURE |
| `0x00370007` | 0x7ff75c176bf0 | CHARTER_SEND_SIGNATURE_REQUEST |
| `0x00380000` | 0x7ff75c176d00 | INITIATIVE_SERVICE_STATUS_CHECK |
| `0x00380001..0x0038000F` | 0x7ff75c176d20..1772c0 | (16 INITIATIVE Op-XX CMSGs — handlers in NeighborhoodHandler.cpp) |
| `0x00390000..0x0039000F+` | 0x7ff75c1773d0..1776xx | NEIGHBORHOOD_* (all neighborhood mgmt CMSGs) |

## Build-67186 fake CMSG opcodes (no sender → confirmed dead)

These are TC-CUSTOM in Opcodes.h and **must be retired** — the client never sends them in build 67186:

### Group 0x30 (Decor)
- `0x300008` UPDATE_DYE_SLOT — handler exists, may be inactive
- `0x30000A` DELETE_FROM_STORAGE_BY_ID
- `0x30000C` CLEANUP_MODE_TOGGLE
- `0x300011` CONFIRM_PREVIEW_PLACEMENT (STUB-LOG only)

### Group 0x31 (Fixture)
- `0x310001` FIXTURE_CREATE_BASIC_HOUSE
- `0x310002` FIXTURE_DELETE_HOUSE

### Group 0x35 (HOUSING_SYSTEM)
- `0x350000` HOUSE_STATUS_QUERY (duplicate of real `0x350005`)
- `0x350001` GET_HOUSE_INFO_ALT (duplicate of real `0x350006`)
- `0x350003` EXPORT_HOUSE
- `0x350004` UPDATE_HOUSE_INFO **← important: today's SMSG remap to 0x550004 emits to a handler that never fires!**

### Group 0x37 (Charter)
- `0x370002` CHARTER_SIGN_RESPONSE (STUB-OK)
- `0x370005` CHARTER_REMOVE_SIGNATURE (STUB-OK)

## Build-67186 mapped + confirmed correct

All 16 SVCS senders, 5 Decor real senders, 6 Fixture real senders, 9 Room senders,
4 HOUSING_SYSTEM senders, 6 Charter senders, 16 Initiative senders, 10+ Neighborhood
senders — all map correctly to existing Opcodes.h values.

## Critical finding: today's 0x550004 remap doesn't fully work

The SMSG `SMSG_HOUSING_UPDATE_HOUSE_INFO = 0x550004` remap (commit `6e7b84dbc1`) is
correct on the SMSG side, but the CMSG that triggers its emission
(`CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO = 0x350004`) is **fake** — no sender in
binary. The handler never fires in production.

Real house-rename flow likely goes through `CMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS
(0x33000B, REAL)` which already calls `Housing::SetHouseNameDescription` per audit.

## Build drift retrospective

Build versions across our sniff captures shifted opcode values:
- `RESERVE_PLOT`: was `0x330006` in build 65940, became `0x330007` in build 67186
- `TELEPORT_TO_PLOT`: was `0x330017` in builds 66102-66838, became `0x330019` in build 67186

Our Opcodes.h targets build 67186 correctly. Older-build sniff observations of
opcodes like `0x330006` and `0x330017` were build-drift artifacts, not unmapped
features.

## Extraction script

`I:/TrinityCore/housing/sniff_verify/verify_cmsgs.py` (sniff cross-check) plus
inline IDA `py_eval` script — should be packaged into a standalone tool for future
build verification work.
