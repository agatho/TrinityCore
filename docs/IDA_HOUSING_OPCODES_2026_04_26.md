# IDA-Decoded Housing Opcode Catalog — 12.0.5.67186

Source: Live IDA database. Each entry shows the client-side serializer (`sub_*`) decompiled and the wire-format fields it writes.

The serializer pattern is:
- `sub_7FF75EE9FF10(buf, value)` = `WriteUint32(buf, value)`
- `sub_7FF75EE9FFB0(buf, float)` = `WriteFloat(buf, value)`
- `sub_7FF75EE9FDD0(buf, byte)` = `WriteUint8(buf, value)`
- `ai_Serialize_PlayerSpellData(buf, guid_ptr)` = `WritePackedGUID(buf, guid)` (despite the misleading name)

## 0x30 — HousingDecorSystem (CMSG)

| Opcode | Sender RVA | Wire fields |
|---|---|---|
| 0x300001 PLACE | sub_7FF75C19DCE0 | PackedGUID DecorGuid + 7×float (Pos+Rot+Scale) + PackedGUID AttachParentGuid + PackedGUID RoomGuid + **PackedGUID AnchorMeshObjectGuid** + uint32 AttachPoint |
| 0x300002 MOVE | sub_7FF75C19DEC0 | PackedGUID DecorGuid + 7×float + PackedGUID AttachParentGuid + PackedGUID RoomGuid + PackedGUID Field_70 + uint32 Field_80 + uint8 Field_85 + uint8 Field_86 + Bits<1> IsBasicMove |

## 0x31 — HousingFixtureSystem (CMSG)

| Opcode | Sender RVA | Wire fields |
|---|---|---|
| 0x310004 SET_HOUSE_TYPE | sub_7FF75C19E4D0 | PackedGUID HouseGuid + uint32 HouseExteriorWmoDataID + uint8 Flags |
| 0x310005 SET_CORE_FIXTURE | sub_7FF75C19E520 | PackedGUID FixtureGuid + uint32 ExteriorComponentID + uint8 Flags |

## 0x32 — HousingRoomSystem (CMSG)

| Opcode | Sender RVA | Wire fields |
|---|---|---|
| 0x320006 APPLY_COMPONENT_MATERIALS | sub_7FF75C1AC240 | PackedGUID RoomGuid + uint32 OptionCount + uint32 ColorOverride + uint32 RoomComponentTextureID + **uint8 ComponentSlot** + uint32[OptionCount] OptionIDs |

## 0x38 — NeighborhoodInitiativeSystem (CMSG, 16 opcodes)

| Opcode | Sender RVA | Wire fields | Mapped to |
|---|---|---|---|
| 0x380000 (3670016) | sub_7FF75C176D00 | empty | CMSG_NEIGHBORHOOD_INITIATIVE_SERVICE_STATUS_CHECK |
| 0x380001 (3670017) | sub_7FF75C176D20 | PackedGUID at +32 | candidate `SetActiveNeighborhood` (Lua API exists) |
| 0x380002 (3670018) | sub_7FF75C176D60 | PackedGUID at +32 | CMSG_GET_AVAILABLE_INITIATIVE_REQUEST |
| 0x380003 (3670019) | sub_7FF75C176DA0 | PackedGUID at +32 | CMSG_GET_NEIGHBORHOOD_INITIATIVE_INFO_REQUEST |
| 0x380004 (3670020) | sub_7FF75C176DE0 | PackedGUID at +32 | CMSG_GET_INITIATIVE_ACTIVITY_LOG_REQUEST |
| 0x380005 (3670021) | sub_7FF75C176E20 | uint32 at +32 + PackedGUID at +40 | candidate task-related (taskID + neighborhoodGUID) |
| 0x380006 (3670022) | sub_7FF75C176E70 | empty | refresh / ack |
| 0x380007 (3670023) | sub_7FF75C176E90 | uint32 deref'd from +32 | candidate `AddTrackedInitiativeTask`(taskID) |
| 0x380008 (3670024) | sub_7FF75C176ED0 | empty | refresh / ack |
| 0x380009 (3670025) | sub_7FF75C176F10 | float deref'd from +32 | UNUSUAL — debug or progress contribution? |
| 0x38000A (3670026) | sub_7FF75C176F50 | uint32 deref'd from +32 | candidate `RemoveTrackedInitiativeTask`(taskID) |
| 0x38000B (3670027) | sub_7FF75C176F90 | uint32 deref'd from +32 | candidate task ID query |
| 0x38000C (3670028) | sub_7FF75C176FD0 | PackedGUID at +32 | candidate `SetViewingNeighborhood`(GUID) |
| 0x38000D (3670029) | sub_7FF75C1770E0 | uint32 X + uint32 N + (uint32,uint32)[N] + Bits<1> | progress submission (taskID, value pairs) |
| 0x38000E (3670030) | sub_7FF75C1771B0 | uint32 N + uint32[N] | bulk task track update |
| 0x38000F (3670031) | sub_7FF75C1772C0 | uint32 N + (uint32×4)[N] | bulk milestone / claim batch |

**Lua APIs known to call into 0x38 group**:
- `RequestNeighborhoodInitiativeInfo(neighborhoodGUID)` → 0x380003 ✓
- `AddTrackedInitiativeTask(taskID)` — likely 0x380007
- `RemoveTrackedInitiativeTask(taskID)` — likely 0x38000A
- `SetActiveNeighborhood(neighborhoodGUID)` — likely 0x380001
- `SetViewingNeighborhood(neighborhoodGUID)` — likely 0x38000C

The remaining (0x380005, 6, 8, 9, B, D, E, F) need additional Lua-handler decompilation to bind exactly. The 8 TC-CUSTOM speculative opcodes in our `Opcodes.h` (0xF0000006..0xF000000D) should be retired in favor of these real values once the bindings are confirmed.

## 0x39 — NeighborhoodSystem (CMSG)

| Opcode | Sender RVA | Wire fields | Mapping |
|---|---|---|---|
| 0x39000A MOVE_HOUSE | sub_7FF75C177680 | PackedGUID **CornerstoneGuid** (HighGuid::GameObject — validated by client at TryMoveHouse 0x7FF75CC59CA1) + PackedGUID HouseGuid | First field is the destination plot's cornerstone GO, NOT a neighborhood guid as previously assumed. |
| 0x39000B OPEN_CORNERSTONE_UI | sub_7FF75C1776D0 | uint32 PlotIndex + PackedGUID NeighborhoodGuid | unchanged |

## 0x3C — InitiativeSystem (CMSG)

| Opcode | Sender RVA | Wire fields | Mapping |
|---|---|---|---|
| 0x3C0000 | sub_7FF75C1A6810 | PackedGUID at +32 | CMSG_INITIATIVE_UPDATE_ACTIVE_NEIGHBORHOOD |
