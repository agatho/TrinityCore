# Cross-Referencing Lua FrameXML with IDA — Methodology + Findings

Demonstrates how Blizzard's public UI source (`github.com/Gethe/wow-ui-source`)
fills gaps that pure-IDA decompilation can't. Static call-graph tracing in IDA
hits dead ends at vtable indirection; the Lua side reveals intent and event flow
that resolves the ambiguity.

**Build alignment**: Pull from the `12.0.5` git tag of wow-ui-source — that
matches our target client `12.0.5.67186`. The `live` branch tracks whatever's
currently in retail (which may be ahead). Verified diff of
`Blizzard_HousingHouseSettings.lua` between `12.0.5` and `live` is empty as of
this writing, so for housing both refs work — but always pin to the tagged
build to stay safe.

```bash
# Pull a specific addon's Lua at 12.0.5
curl -fsSL "https://raw.githubusercontent.com/Gethe/wow-ui-source/12.0.5/Interface/AddOns/Blizzard_HousingHouseSettings/Blizzard_HousingHouseSettings.lua"
```

## The chain

```
[UI button click]
    ↓ FrameXML Lua
[C_Housing.SomeFunction(args)]
    ↓ C++ Lua binding  (named in IDA strings: "C_Housing.SomeFunction")
[CMSG packet sender (sub_7FF75Cxxxxxx)]
    ↓ network
[Server CMSG handler]
    ↓
[Server SMSG response]
    ↓ network
[Client SMSG dispatcher (sub_7FF75C1Dxxxx case 0xNNNNNN)]
    ↓ fires Lua event
[FrameXML event handler in Lua]
    ↓ updates UI state
```

If you have ANY two of these, you can fill in the rest.

## Sources

| Source | Where | What it gives you |
|---|---|---|
| Lua API names + signatures | IDA strings (`Usage: C_Housing.X(args)`) | Function existence + arg types |
| Lua API → C++ binding | IDA xrefs to the API name string | RVA of the C++ function |
| FrameXML Lua source | `github.com/Gethe/wow-ui-source` (public) | Which API is called for each UI flow + event names fired |
| CMSG sender | IDA decompile of the C++ binding | Wire format the client sends |
| SMSG handler | IDA dispatcher (e.g. `sub_7FF75C1D1020` for 0x55 group) | Wire format the client expects to receive |

## Demonstration: the "Permissions Window" investigation

User reported: "permissions window is not shown when clicked".

### Step 1 — Lua: find the UI panel and entry point

Pulled `Blizzard_HousingHouseSettings/Blizzard_HousingHouseSettings.lua` from
the public mirror.

```lua
function HousingHouseSettingsFrameMixin:OnShow()
    FrameUtil.RegisterFrameForEvents(self, HouseSettingsFrameShownEvents);
    C_Housing.RequestCurrentHouseInfo();              -- <- the trigger
    PlaySound(SOUNDKIT.HOUSING_SETTINGS_OPEN_MENU);
end

function HousingHouseSettingsFrameMixin:OnEvent(event, ...)
    if event == "PLAYER_CHARACTER_LIST_UPDATED" then ...
    elseif event == "CURRENT_HOUSE_INFO_RECIEVED" then ...   -- waits for this
    elseif event == "CURRENT_HOUSE_INFO_UPDATED" then ...
    end
end

function HousingHouseSettingsFrameMixin:OnSaveClicked()
    ...
    C_Housing.SaveHouseSettings(newOwnerGUID, accessSettings);  -- <- save path
end
```

**Key finding**: the Permissions UI does NOT call `RequestPlayerPermissions`.
It calls `RequestCurrentHouseInfo`, listens for `CURRENT_HOUSE_INFO_RECIEVED`
(Blizzard's typo, intentional in code), and on save calls
`SaveHouseSettings(playerGUID, accessFlags)`.

So our existing 0x350007 GET_PLAYER_PERMISSIONS handler is **not** what populates
the window — the fix path runs through 0x350006 GET_CURRENT_HOUSE_INFO instead.

### Step 2 — IDA: trace the SMSG response wire format

Decompiled `sub_7FF75C1D1020` (the SMSG dispatcher for group 0x55, found via
xref to "Failed to retrieve player's permissions" string). Case 0x550001 is
`SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE`:

```c
case 5570561:  // 0x550001
    ...
    sub_7FF75C1AC920(&v52, a5);          // parse HouseInfo struct
    ClientOpcode_helper_318EF90(a5, &n0x80);
    LOBYTE(v58) = n0x80;                 // Result byte
    ...
    sub_7FF75DF4E900(v14, &v52, v13);    // dispatch to Lua handler
                                         // (this fires CURRENT_HOUSE_INFO_RECIEVED)
```

Decompiled `sub_7FF75C1AC920`:
```c
ClientOpcode_helper_31E0120(a2, a1);          // PackedGUID → +0
ClientOpcode_helper_31E0120(a2, a1 + 16);     // PackedGUID → +16
ClientOpcode_helper_31E0120(a2, a1 + 32);     // PackedGUID → +32
ClientOpcode_helper_318EF90(a2, &v8);         // uint8 → +48
*(_BYTE *)(a1 + 48) = v8;
ai_Read_CompressedUInt32FromPacket(a2, &v8);  // CompressedUInt32 → +72
*(_DWORD *)(a1 + 72) = v8;
ClientOpcode_helper_318EF90(a2, &v8);         // uint8 (flag byte)
result = (uint8)v8 >> 7;                      // bit 7 only
if (bit 7 set):
    ai_Process_HousingDataPacket(a2, &v7);    // parse JamCliHouse blob
```

**Wire format**:
1. PackedGUID HouseGuid
2. PackedGUID OwnerGuid
3. PackedGUID NeighborhoodGuid
4. uint8 PlotIndex
5. **CompressedUInt32** AccessFlags (variable 1-5 bytes — NOT raw uint32)
6. uint8 flags (only bit 7 = HasJamCliHouse used)
7. (optional) JamCliHouse blob

### Step 3 — Compare with our serializer

`HousingPackets.cpp:1359` `operator<<(ByteBuffer&, HouseInfo const&)` writes:
```cpp
data << houseInfo.HouseGuid;
data << houseInfo.OwnerGuid;
data << houseInfo.NeighborhoodGuid;
data << houseInfo.PlotId;
data << houseInfo.AccessFlags;        // <-- uint32 (4 raw bytes), NOT compressed
uint8 flags = (HasMoveOutTime ? 0x80 : 0)  // <-- speculative semantics
            | (HouseName ? 0x40 : 0)
            | (NeighborhoodName ? 0x20 : 0)
            | (PlotReserved ? 0x10 : 0);
data << flags;
if (HasMoveOutTime)        data << uint64(MoveOutTime);
if (HouseName)             data << ...
if (NeighborhoodName)      data << ...
```

**Discrepancies**:
1. `AccessFlags` is written as raw `uint32` (4 bytes always); client expects
   CompressedUInt32 (1-5 bytes, varint-style).
2. `flags` bit 7 is documented as "HasMoveOutTime" with optional uint64 payload;
   IDA shows bit 7 = HasJamCliHouseData with optional JamCliHouse blob.
3. `flags` bits 6, 5, 4 are not used by the IDA parser; our speculative payloads
   would be parsed as garbage.

When AccessFlags=0 and no flags are set, the wire happens to align by accident
(extra trailing zeros get consumed as Result byte etc.) — which is why the
existing code "works" for the simple case. But for non-zero AccessFlags, the
client mis-aligns and corrupts subsequent reads.

### Step 4 — Caveat: same struct serves multiple opcodes

`HouseInfo` is also serialized by `NeighborhoodBuyHouseResponse`,
`NeighborhoodMoveHouseResponse`, and (per code comment) was originally verified
against 0x5C0008/0x5C0009 (INVITE_RESIDENT_RESPONSE / CANCEL_INVITATION_RESPONSE).

Those opcodes have their OWN parsers — they may or may not match
`sub_7FF75C1AC920`. Before changing the global serializer, decompile each of
those SMSG dispatchers in IDA to confirm wire-format alignment. If they
diverge, split into per-opcode write functions.

## Repeatable recipe

Given an unknown opcode or stub handler:

1. **What does the user-visible UI do?** Find the Blizzard AddOn that owns the
   feature. Search `github.com/Gethe/wow-ui-source` for the frame name
   (e.g. `HousingDashboardFrame`, `HousingPermissionsFrame`).
2. **Which API does it call?** Grep the Lua for `C_Housing.X` /
   `C_Neighborhood.X` calls inside the relevant handler functions.
3. **Match API → C++ binding**. Search IDA for the literal "Usage: C_X.Y(...)"
   string; xrefs lead to the binding function.
4. **Decompile the binding**. The binding will have a CMSG opcode literal
   passed to a `WriteUint32(buf, opcode)` call. That's the wire opcode —
   matches our enum value.
5. **Decompile the SMSG handler**. The SMSG dispatcher for the response group
   has a switch over opcode values — each case is the parser. Match field-by-field.
6. **Compare with our `Write()` and `Read()` functions**. Any divergence is a bug.

## What this enables

- **Bind 0x38 group**: the 12 unbound NeighborhoodInitiative opcodes can be
  matched by tracing the Lua API path:
  `C_NeighborhoodInitiative.AddTrackedInitiativeTask(taskID)` → IDA binding →
  CMSG opcode. The vtable indirection in `sub_7FF75DFB2D00` is bypassed
  because the binding function itself contains a static `WriteUint32(buf, OPCODE)`
  call before the vtable dispatch.
- **Resolve speculative TC-CUSTOM names**: for each `0x35*` / `0x33*` / `0x37*`
  / `0x39*` opcode in our Opcodes.h, find the corresponding C_Housing or
  C_Neighborhood Lua API in FrameXML, then verify wire format.
- **Audit response wire formats**: for each of our SMSG `Write()` functions,
  decompile the matching client SMSG dispatcher case and confirm field order,
  types, and optional-field semantics.

## Public source list (housing-related)

```
Blizzard_HousingBulletinBoard       — bulletin board UI
Blizzard_HousingCharter             — neighborhood charter creation/signing
Blizzard_HousingControls            — toolbar (decor/fixture/room edit modes)
Blizzard_HousingCornerstone         — cornerstone UI (move house, plot status)
Blizzard_HousingCreateNeighborhood  — guild neighborhood creation
Blizzard_HousingDashboard           — house management dashboard
Blizzard_HousingEventHandler        — global event-to-UI router
Blizzard_HousingHouseFinder         — house finder / public neighborhood browse
Blizzard_HousingHouseSettings       — owner settings + permissions (this doc's example)
Blizzard_HousingInspectModeUI       — inspect-house viewer
Blizzard_HousingMarketCart          — Trader's Tender market
Blizzard_HousingModelPreview        — 3D model preview shared widget
Blizzard_HousingTemplates           — shared XML templates
Blizzard_HousingTutorials           — tutorial/onboarding flow
```

Each is a few hundred to a few thousand lines of Lua. Combined with the IDA
SMSG dispatchers, every housing CMSG/SMSG can be wire-format verified.

## Concrete progress made via this method

### 0x38 NeighborhoodInitiative — narrowed from 12 unbound to 6 user-callable

Source: `Blizzard_APIDocumentationGenerated/NeighborhoodInitiativeDocumentation.lua`
on the `12.0.5` tag enumerates every C_NeighborhoodInitiative function and event.

**APIs that send CMSGs to the server** (have side effects, need network round-trip):
| Lua API | Argument | IDA wire match | Candidate opcodes |
|---|---|---|---|
| `AddTrackedInitiativeTask(taskID)` | uint32 | matches `uint32` senders | 0x380007, 0x38000A, 0x38000B |
| `RemoveTrackedInitiativeTask(taskID)` | uint32 | matches `uint32` senders | 0x380007, 0x38000A, 0x38000B |
| `SetActiveNeighborhood(neighborhoodGUID)` | PackedGUID | matches PackedGUID senders | 0x380001, 0x38000C |
| `SetViewingNeighborhood(neighborhoodGUID)` | PackedGUID | matches PackedGUID senders | 0x380001, 0x38000C |
| `RequestInitiativeActivityLog()` | (implicit) | sender writes PackedGUID at +32 | **0x380004** ✓ |
| `RequestNeighborhoodInitiativeInfo()` | (implicit) | sender writes PackedGUID at +32 | **0x380003** ✓ |

**APIs that read cached state** (no CMSG): 14 read-only functions like
`GetActiveNeighborhood`, `GetTrackedInitiativeTasks`, `IsInitiativeEnabled`,
`PlayerHasInitiativeAccess`. These query TLS state populated from prior SMSGs.

**Events fired when SMSGs arrive**:
- `InitiativeActivityLogUpdated` — server→client response after activity log query
- `NeighborhoodInitiativeUpdated` — response to RequestNeighborhoodInitiativeInfo
- `InitiativeCompleted` — server-pushed state change
- `InitiativeTaskCompleted` — server-pushed
- `InitiativeTasksTrackedListChanged` — server-pushed
- `InitiativeTasksTrackedUpdated` — server-pushed

**Remaining unbound**: 0x380005 (uint32 + PackedGUID), 0x380006 (empty),
0x380008 (empty), 0x380009 (float), 0x38000D-F (bulk arrays). These don't
match any documented API — likely internal client→server progress reports
from criteria/objective tracker plumbing, not exposed to user-callable Lua.

**Why 1:1 binding still requires sniff**: AddTrackedInitiativeTask's binding
chain is `Lua call → sub_7FF75CEBA231 → sub_7FF75DFB2D00 → vtable callback at
+9568 keyed by hash 0xBA8F5C5BC59E8E8E`. The vtable resolution happens at
runtime — IDA can't see which of the 3 candidate `uint32` senders the hash
resolves to. To finalize: capture a runtime sniff of clicking "Track Task" on
an initiative and read the opcode off the wire.

### Permissions / HouseSettings UI

`Blizzard_HousingHouseSettings.lua` shows the window opens via
`C_Housing.RequestCurrentHouseInfo()` (CMSG 0x350006) and waits for
`CURRENT_HOUSE_INFO_RECIEVED` event. The Save button calls
`C_Housing.SaveHouseSettings(playerGUID, accessFlags)` (maps to
CMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS = 0x33000B).

IDA case 0x550001 in `sub_7FF75C1D1020`:

```c
case 5570561:                                  // SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE
    sub_7FF75C1AC920(&v52, a5);                // parse HouseInfo:
                                               //   3 PackedGUIDs
                                               //   uint8 PlotIndex (+48)
                                               //   CompressedUInt32 AccessFlags (+72)  ← !!!
                                               //   uint8 flag byte (bit 7 only = HasJamCliHouse)
                                               //   if bit 7: JamCliHouse blob
    ClientOpcode_helper_318EF90(a5, &n0x80);   // uint8 Result
    sub_7FF75DF4E900(v14, &v52, v13);          // dispatches CURRENT_HOUSE_INFO_RECIEVED
```

Our `operator<<(ByteBuffer&, HouseInfo const&)` writes:
- 3 PackedGUIDs ✓
- uint8 PlotId ✓
- **uint32 AccessFlags** — wrong! Client expects CompressedUInt32 (1-5 byte varint)
- uint8 flags with bit 7 = HasMoveOutTime — wrong semantics (IDA: bit 7 = HasJamCliHouse)
- Optional uint64 MoveOutTime / strings — wrong payload (IDA: optional JamCliHouse)

**Impact**: when AccessFlags=0 and flags=0, the wire happens to align by accident
(extra 3 zero bytes get consumed as the SMSG Result byte and trailing slack).
For non-zero AccessFlags, the client misaligns and would corrupt subsequent
reads. The fix requires either:
1. A separate `Write()` for `HousingGetCurrentHouseInfoResponse` that writes
   the IDA-correct format (CompressedUInt32 + JamCliHouse).
2. Adding a CompressedUInt32 helper to TrinityCore's ByteBuffer.

`HouseInfo` is also serialized for `NeighborhoodBuyHouseResponse`,
`NeighborhoodMoveHouseResponse`, and per code comment was originally verified
against `0x5C0008/0x5C0009` (INVITE_RESIDENT / CANCEL_INVITATION). Those
opcodes need their own SMSG dispatchers decompiled before the global
serializer can be safely rewritten — the wire formats may diverge per opcode.

### What I'd do next (when sniff data or more time is available)

1. Sniff the 12.0.5 client clicking "Track Task" on an initiative — captures
   one of {0x380007, 0x38000A, 0x38000B} on the wire. Bind that opcode to
   `AddTrackedInitiativeTask`. Repeat for each of the 6 user-callable APIs.
2. Decompile SMSG dispatcher cases for `NeighborhoodBuyHouseResponse` and
   `NeighborhoodMoveHouseResponse` — confirm whether they use the same HouseInfo
   parser as 0x550001 or a different one. If different, split into per-opcode
   `Write()` functions.
3. Add `ByteBuffer::WriteCompressedUInt32` helper. Apply to
   `HousingGetCurrentHouseInfoResponse` first.
4. Verify the rest of `sub_7FF75C1D1020` cases (0x550002, 0x550003, 0x550004,
   0x550005) — those map to RESET_KIOSK_MODE, EXPORT_HOUSE, plus two unknowns.
   Decompile each parser and cross-check our matching `Write()` functions.
