# 100% Blizzlike Housing System — Master Implementation Plan

**Binary**: WoW 12.0.1 Build 66198
**Sniffs**: Builds 65940, 66102 (Alliance + Horde, 3 captures)
**Reference**: `docs/HOUSING_SYSTEM_REFERENCE_66198.md`, `docs/neighborhood-analysis/10_gap_list.md`
**Date**: 2026-03-04

---

## Current State Summary

| Metric | Value |
|--------|-------|
| Handler functions | 112 (82 Housing + 30 Neighborhood) |
| Fully implemented handlers | ~86 (77%) |
| Stub/partial handlers | ~26 (23%) |
| CMSG opcodes registered | 86 |
| SMSG opcodes registered | 109 |
| Character DB tables | 12 |
| Hotfix DB2 tables | 41 (31 base + 10 locale) |
| Prepared statements | 62 (28 housing + 24 neighborhood + 10 initiative) |
| UpdateField structs | 20 housing-related |
| Entity fragments | 17 (13 updateable + 4 tag) |
| Identified gaps | 48 (5 CRITICAL, 12 MAJOR, 15 MODERATE, 16 MINOR) |

### What Works Today
- House purchase (Cornerstone + HouseFinder reserve paths)
- Full decor system (place, move, remove, lock, dye, storage, redeem)
- Full room system (add, remove, rotate, move, theme, wallpaper, door, ceiling)
- Full fixture system (core, create, delete, size, type)
- Edit modes (basic, expert, layout, customize, exterior)
- Interior entry/exit with map transfer
- Neighborhood charter (create, edit, sign, finalize)
- Neighborhood management (invite, cancel, decline, managers, roster)
- House finder (search, BNet friends, guild)
- House settings, teleport-to-plot, tutorial start
- Ownership transfer (offer, accept, reject)
- Entity spawning (cornerstones, houses, rooms, decor, area triggers)
- WorldState per-plot ownership display
- Cosmetic phase system (16 phases, enter/exit)
- Plot enter/exit spell packets

### What's Broken or Missing
- **12 stub handlers** that return SUCCESS without real logic
- **6 missing JAM wire format structs** (client will misparse data)
- **GAP-003**: JamNeighborhoodRosterEntry format mismatch (CRITICAL)
- **GAP-024**: Neighborhood name opcode group mismatch (0x46 vs 0x44)
- **Hardcoded values** throughout handlers (AccessFlags=32, permissions=0xE0/0x40, level/favor)
- **Initiative system**: Only 4 of 17 opcodes handled, no progression
- **Photo sharing**: Stub only
- **House snapshot/export**: Stub only
- **House name/description**: Not persisted
- **Cosmetic owner change**: Not persisted
- **Level/favor query**: All hardcoded values

---

## Phase 0 — Foundation Fixes (CRITICAL)

**Priority**: Immediate | **Effort**: 2-3 days | **Gaps**: GAP-003, GAP-024, GAP-032

These fixes correct broken wire formats that cause client misparses.

### T0.1 — Fix JamNeighborhoodRosterEntry Wire Format (GAP-003)

**Files**: `HousingPackets.h`, `HousingPackets.cpp`, `NeighborhoodHandler.cpp`

Current struct (WRONG):
```cpp
struct JamNeighborhoodRosterEntry {
    uint64 Timestamp; PackedGUID PlayerGuid; PackedGUID HouseGuid; uint64 ExtraData;
};
```

IDA-verified (stride=48):
```cpp
struct RosterMemberData {
    PackedGUID128 PlayerGUID;   // +0
    PackedGUID128 HouseGUID;    // +16
    uint8 ResidentType;         // +32 (0=Resident, 1=Manager, 2=Owner)
    uint8 StatusFlags;          // +33 (0x80=online, 0x00=offline)
};
```

**Impact**: Roster UI shows wrong names, roles, and online status. Every consumer of roster data must be updated.

### T0.2 — Fix Neighborhood Name Opcode (GAP-024, GAP-025)

**File**: `Opcodes.h`, `Opcodes.cpp`

- `SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE` should be `0x440012` (not `0x460012`)
- Add `SMSG_INVALIDATE_NEIGHBORHOOD_NAME` at `0x440013`

**Impact**: Client cannot cache neighborhood names — displays "Unknown" everywhere.

### T0.3 — Fix HouseSettingFlags Default Value

**File**: `Housing.cpp`

Constructor (line 43), `Create()` (line 534), and reset path (line 625) all set:
```cpp
_settingsFlags = HOUSE_SETTING_HOUSE_ACCESS_ANYONE  // 0x001 — WRONG
```
Should be (sniff-verified):
```cpp
_settingsFlags = HOUSE_SETTING_PLOT_ACCESS_ANYONE    // 0x020
```

### T0.4 — Replace All Hardcoded AccessFlags = 32

**Files**: `HousingHandler.cpp` (7 locations), `NeighborhoodHandler.cpp` (2 locations)

Every `response.AccessFlags = 32` must become `housing->GetSettingsFlags()`. See the existing plan at `playful-enchanting-knuth.md` for exact line numbers.

### T0.5 — Fix HandleHousingSvcsUpdateHouseSettings

**File**: `HousingHandler.cpp` line 1998

Add proper validation: mask incoming flags to `HOUSE_SETTING_VALID_MASK` (0x3FF), verify ownership, persist via `SaveSettings()`, read back stored value for response. Broadcast updated AccessFlags to nearby players.

### T0.6 — Fix HandleHousingGetPlayerPermissions

**File**: `HousingHandler.cpp` line 2717

Owner: `0xE0` (correct, keep). Visitor: currently hardcoded `0x40` — make dynamic based on `CanVisitorAccess()` against the visited plot owner's stored `settingsFlags`.

### T0.7 — Complete HousingResult Enum (GAP-032)

**File**: `HousingDefines.h`

Verify all 90 values (0-89) are present and match IDA. Key additions if missing:
`CannotAfford(5)`, `CharterComplete(6)`, `GuildMoreAccountsNeeded(23)`, `MissingExpansionAccess(52)`, `PlotReservationCooldown(72)`, `TimerunningNotAllowed(80)`, `TokenRequired(81)`.

**Verification Gate**: Roster UI shows correct member data; neighborhood names display; settings changes persist across relog.

---

## Phase 1 — Wire Format & Struct Corrections

**Priority**: High | **Effort**: 3-5 days | **Gaps**: GAP-002, GAP-004, GAP-018-022

### T1.1 — Implement RosterMemberUpdateInfo (GAP-018)

**File**: `HousingPackets.h`

```cpp
struct RosterMemberUpdateInfo {
    ObjectGuid PlayerGUID;  // +0
    uint8 UpdateType;       // +16 (0=Added, 1=RoleChanged, 2=Removed)
    uint8 NewRole;          // +17
};
```
Used by: `NeighborhoodRosterResidentUpdate` (0x5C0013), eviction responses.

### T1.2 — Implement JamHousingSearchResult (GAP-004)

**File**: `HousingPackets.h`

```cpp
struct JamHousingSearchResult {
    ObjectGuid PrimaryGUID;     // +0 (Neighborhood GUID)
    uint64 SortKey;             // +16
    uint64 SortData;            // +24
    uint8 StatusType;           // +32
    ObjectGuid SecondaryGUID;   // +40 (Owner GUID)
    std::string Name;           // +56 (variable length)
};
```
Stride=96 in client memory. Used by all HouseFinder responses.

### T1.3 — Implement JamCliNeighborhood (GAP-002)

**File**: `HousingPackets.h`

Variable-length struct with conditional fields:
```
uint32 NeighborhoodID
PackedGUID128 GUID1, GUID2
uint64 Value64
uint8 StatusByte
PackedGUID128 GUID3
uint8 FlagsByte1 (bits: HasGuild, IsPublic, HasRoster, HasCornerstone, HasPlot, HasGuildData)
uint8 FlagsByte2 (bits: HasTimeout, HasInviteData)
[conditional] Roster[] (RosterMemberData[])
CString Name
[conditional] uint64 GuildGUID
[conditional] uint32 PlotCount
```
Used by: `SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE`, full neighborhood state sync.

### T1.4 — Implement JamCliHousingInvitation (GAP-019)

**File**: `HousingPackets.h`

```
uint32 IDType (CompressedUInt32)
PackedGUID128 InviterGUID, HouseGUID
uint64 TimestampOrID
uint8 StatusByte
PackedGUID128 ThirdGUID
uint8 FlagByte (packed conditional bits)
[optional] ResidentArray
CString Name
[optional] uint64, uint32
```
Used by: `NeighborhoodPlayerGetInviteResponse`, `NeighborhoodGetInvitesResponse`.

### T1.5 — Implement JamHousingDBHouseLevelFavorUpdateData (GAP-022)

**File**: `HousingPackets.h`

Per-element (stride=64):
```
PackedGUID128 SourceGUID, HouseGUID, NeighborhoodGUID
uint32 FavorAmount, PreviousFavor
uint8 UpdateSource (HousingFavorUpdateSource enum)
uint8 UpdateType (HousingFavorUpdateType enum)
```
Used by: `SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR` (0x540011).

### T1.6 — Fix PermissionsFailure Wire Format

**File**: `HousingPackets.h`

`SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE` (0x540000): Currently sends `uint16 Result`. IDA shows `uint8 FailureType + uint8 ErrorCode`. Fix the struct and Write() method.

**Verification Gate**: All JAM structs serialize/deserialize correctly; round-trip test with sample data.

---

## Phase 2 — Complete Stub Handlers

**Priority**: High | **Effort**: 5-7 days | **Gaps**: GAP-006, GAP-007, GAP-011, GAP-012, GAP-016

Implement real logic for the 12+ stub handlers that currently return SUCCESS without action.

### T2.1 — HandleHousingDecorUpdateDyeSlot (0x300008)

**File**: `HousingHandler.cpp` line 3190

Currently returns SUCCESS without persisting. Implement: update single dye slot on decor, persist via `CHAR_UPD_CHARACTER_HOUSING_DECOR_DYES`, update Account entity FHousingStorage_C.

### T2.2 — HandleHousingDecorPlacementPreview (0x30000F)

**File**: `HousingHandler.cpp` line 3295

Currently returns SUCCESS with RestrictionFlags=0. Implement actual boundary validation using plot bounds (`-35,-30,-1.01` to `35,30,125.01`) with `housingDecor_PlotBoundsBuffer` (10.0). Return appropriate `HousingDecorPlacementRestriction` bitmask.

### T2.3 — HandleHousingFixtureCreateBasicHouse (0x310001)

**File**: `HousingHandler.cpp` line 3315

Currently returns SUCCESS if house exists. Implement: create house for player who doesn't have one yet (alternative to BuyHouse path), spawn entities.

### T2.4 — HandleHousingFixtureDeleteHouse (0x310002)

**File**: `HousingHandler.cpp` line 3341

Currently returns SUCCESS without deletion. Implement: full house deletion flow — despawn entities, clear DB, update neighborhood plot state.

### T2.5 — HandleHousingSvcsClearPlotReservation (0x330005)

**File**: `HousingHandler.cpp` line 3383

Currently returns SUCCESS without clearing. Implement: remove plot reservation, free plot for other buyers.

### T2.6 — HandleHousingSvcsChangeHouseCosmeticOwner (0x330010)

**File**: `HousingHandler.cpp` line 3465

Currently echoes GUIDs. Implement: validate permissions, transfer cosmetic ownership, persist change, notify affected players.

### T2.7 — HandleHousingSvcsQueryHouseLevelFavor (0x330012)

**File**: `HousingHandler.cpp` line 3481

Currently all hardcoded (-1/-1/1/-1/-1/0x8000). Implement: read actual level/favor from Housing object, compute NextLevelFavorCost from HouseLevelData DB2, use proper `JamHousingDBHouseLevelFavorUpdateData` struct.

### T2.8 — HandleHousingSvcsGuildAddHouse (0x330013)

**File**: `HousingHandler.cpp` line 3503

Currently echoes GUID. Implement: validate guild membership, add house to guild neighborhood, broadcast notification.

### T2.9 — HandleHousingSvcsGuildAppendNeighborhood (0x330014)

Currently echoes GUID. Implement: validate guild ownership, extend neighborhood plot count, broadcast notification.

### T2.10 — HandleHousingSvcsGuildRenameNeighborhood (0x330015)

Currently echoes GUID + name. Implement: validate ownership, profanity filter, update DB, broadcast `SMSG_INVALIDATE_NEIGHBORHOOD_NAME` + `SMSG_NEIGHBORHOOD_UPDATE_NAME_NOTIFICATION`.

### T2.11 — HandleHousingSvcsGuildGetHousingInfo (0x330016)

**File**: `HousingHandler.cpp` line 3547

Currently returns requesting player's housing. Implement: look up ALL houses belonging to the target guild's neighborhood.

### T2.12 — HandleHousingSystemHouseSnapshot (0x350002)

Currently returns SUCCESS with no action. Implement: capture house state for sharing/display. At minimum, send a valid response with house metadata.

### T2.13 — HandleHousingSystemExportHouse (0x350003)

Currently returns empty JSON "{}". Implement: serialize full house layout (rooms, decor positions, fixtures, themes) as exportable JSON.

### T2.14 — HandleHousingSystemUpdateHouseInfo (0x350004)

**File**: `HousingHandler.cpp` line 3670

Currently echoes back without persisting. Implement: persist HouseName/HouseDescription to DB. **Requires new DB column(s)** — add `houseName VARCHAR(64)`, `houseDescription VARCHAR(256)` to `character_housing`.

### T2.15 — HandleHousingDecorCatalogCreateSearcher (0x300007)

Currently echoes Owner + SUCCESS. Implement: create proper search interface for decor catalog browsing.

### T2.16 — HandleHousingSvcsRosterUpdateSubscribe (0x33000D)

Currently no response. Implement: register player for real-time roster push notifications, so `NeighborhoodRosterResidentUpdate` (0x5C0013) fires on member changes.

**Verification Gate**: Each stub handler now performs real operations; all changes persist across relog.

---

## Phase 3 — House Lifecycle Completion

**Priority**: High | **Effort**: 5-7 days | **Gaps**: GAP-006, GAP-007, GAP-008, GAP-011, GAP-015

### T3.1 — House Move System (GAP-006)

**Handler**: `HandleNeighborhoodMoveHouse` (exists, partially implemented)

Complete the full flow:
1. Player initiates move → validate target neighborhood has open plot
2. Deduct move cost (500g / `HOUSE_MOVE_COST_COPPER`)
3. Pack all placed decor into storage (return to catalog)
4. Transfer plot ownership from old to new neighborhood
5. Send `SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE` (0x5C0009) with full `JamCliNeighborhood` (from T1.3)
6. Respawn house entities on new neighborhood map
7. Track `HasMoveOutTime` and move cooldown

### T3.2 — House Relinquish (GAP-011)

**Handler**: `HandleHousingSvcsRelinquishHouse` (exists, real logic but verify completeness)

Verify response wire format matches IDA: `uint8(result) + ObjectGuid(HouseGUID) + ObjectGuid(NeighborhoodGUID) + uint64(timestamp)`. Add move-out timer tracking.

### T3.3 — Eviction System (GAP-007)

**Handler**: `HandleNeighborhoodEvictPlot` (exists)

Verify: eviction removes resident from plot, broadcasts `RosterMemberUpdateInfo[]` to all members, despawns evictee's house entities, sends `SMSG_NEIGHBORHOOD_EVICT_PLOT_NOTICE` (0x5C0016) to evictee.

### T3.4 — Teleport-to-Plot Improvements (GAP-015)

**Handler**: `HandleHousingSvcsTeleportToPlot` (exists, implemented)

Verify: access validation against stored `settingsFlags` (not hardcoded), support all `HousingTeleportReason` values, send proper result codes.

### T3.5 — House Level/Favor Progression

**Files**: `Housing.cpp`, `HousingHandler.cpp`

Connect quest completion to favor accumulation:
- `Housing::OnQuestCompleted()` → `AddFavor(amount, source)` → check level-up threshold from `HouseLevelData` DB2
- On level-up: send `SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR` with proper `JamHousingDBHouseLevelFavorUpdateData`
- Recalculate budgets (`RecalculateBudgets()`) when level changes
- Fire `HOUSING_HOUSE_XP_ADDED_FORMAT` client event

### T3.6 — Reservation System

Implement plot reservation with cooldown:
- `HandleHousingSvcsNeighborhoodReservePlot`: add timer (configurable CVar)
- `HandleHousingSvcsClearPlotReservation`: clear reservation
- `PlotReservationCooldown` result if attempting to reserve too frequently

**Verification Gate**: Complete house lifecycle: create → customize → move → relinquish. Level progression triggers budget increases.

---

## Phase 4 — Account Entity & State Replication

**Priority**: Medium-High | **Effort**: 3-5 days | **Gaps**: GAP-017, GAP-023, GAP-031

### T4.1 — Account Housing Notifications (GAP-017)

**File**: `HousingHandler.cpp`

The 4 `SMSG_ACCOUNT_HOUSING_*` notifications (0x40004F-0x400052) are already defined and sent in some handlers. Audit ALL room/fixture/theme/texture operations to ensure they fire:
- `SMSG_ACCOUNT_HOUSING_ROOM_ADDED` — on room add
- `SMSG_ACCOUNT_HOUSING_FIXTURE_ADDED` — on fixture create
- `SMSG_ACCOUNT_HOUSING_THEME_ADDED` — on theme apply
- `SMSG_ACCOUNT_HOUSING_ROOM_COMPONENT_TEXTURE_ADDED` — on material apply

### T4.2 — NeighborhoodMirrorData Fragment Completeness (GAP-031)

**File**: `UpdateFields.cpp`

Audit `NeighborhoodMirrorData` (Fragment 30) replication. Verify `Houses[]` (DynamicUpdateField<PlayerHouseInfo>), `Managers[]` (DynamicUpdateField<HousingOwner>), `OwnerGUID`, `Name` are all written correctly and updated on:
- Member join/leave
- Manager add/remove
- Ownership transfer
- Neighborhood rename

### T4.3 — PlayerMirrorHouse Data Completeness

**File**: `UpdateFields.h` line 2057

`PlayerMirrorHouse` has fields: Guid, NeighborhoodGUID, Level, Favor, InitiativeFavor, InitiativeCycleID, MapID, PlotID. Verify all are populated, especially `InitiativeFavor` and `InitiativeCycleID` which are currently likely zero.

### T4.4 — PlayerInitiativeComponentData Updates

**File**: `UpdateFields.h` line 2217

Verify `PlayerInitiativeComponentData` writes for:
- `CompletedTasks[]` — on initiative task completion
- `CompletedInitiatives[]` — on initiative completion
- `NeighborhoodGUID` — on neighborhood association
- `InitiativeInfo` — on initiative data update

**Verification Gate**: All entity fragment data replicates to nearby players; Account entity updates fire on all relevant operations.

---

## Phase 5 — Initiative System

**Priority**: Medium | **Effort**: 7-10 days | **Gaps**: GAP-001, GAP-044

### T5.1 — Initiative Data Infrastructure

**DB2 tables**: Already created (`initiative_cycle`, `initiative_milestone`, `initiative_reward`, `initiative_task`, `initiative_x_task`, `initiative_cycle_priority`, `initiative_reward_x_milestone`).
**Character DB**: `neighborhood_initiatives`, `neighborhood_initiative_contributions` exist.
**Prepared statements**: 10 initiative queries exist.

Populate seed data with meaningful initiatives:
- 4+ initiative types (Gathering, Crafting, Combat, Exploration)
- 10+ tasks with DB2 criteria
- Milestone thresholds per initiative
- Rewards (currency, decor, favor)

### T5.2 — Expand Initiative Service Status

**Handler**: `HandleNeighborhoodInitiativeServiceStatusCheck` (exists, sends `InitiativeServiceStatus`)

Enhance: return proper cycle timing, next initiative availability, current active initiative ID.

### T5.3 — GetAvailableInitiative Handler

**Handler**: `HandleGetAvailableInitiativeRequest` (exists)

Implement: query `neighborhood_initiatives` for active initiative, return initiative details + task list + player progress via `SMSG_GET_PLAYER_INITIATIVE_INFO_RESULT`.

### T5.4 — Initiative Activity Log

**Handler**: `HandleGetInitiativeActivityLogRequest` (exists)

Implement: query `neighborhood_initiative_contributions` for completed tasks, return via `SMSG_GET_INITIATIVE_ACTIVITY_LOG_RESULT` with `NICompletedTasksEntry[]`.

### T5.5 — Initiative Task Progression

Implement task completion tracking:
- Hook into quest/achievement/crafting/kill events
- Match events against active initiative tasks (by `InitiativeTaskType`)
- Update `neighborhood_initiative_contributions` via `CHAR_INS_INITIATIVE_CONTRIBUTION` (upsert)
- Check milestone thresholds
- Send `SMSG_INITIATIVE_TASK_COMPLETE` on task completion
- Send `SMSG_INITIATIVE_COMPLETE` on initiative completion

### T5.6 — Initiative Rewards

Implement reward distribution:
- On milestone completion: send `SMSG_INITIATIVE_REWARD_AVAILABLE`
- Grant rewards from `InitiativeReward` DB2 + `InitiativeRewardXMilestone`
- Support reward types: currency, decor, favor

### T5.7 — Initiative → Favor Integration (GAP-044)

Connect initiative completion to house favor:
- `HousingFavorUpdateSource::InitiativeTask` = 5
- `HousingFavorUpdateSource::InitiativeChest` = 6
- Add favor to house on task/chest completion
- Send level/favor update SMSG

### T5.8 — Remaining Initiative Opcodes

Register and implement remaining 0x38xxxx opcodes:
- 0x380001 through 0x380010 — stub with appropriate responses
- Priority: data query opcodes over action opcodes

**Verification Gate**: Initiative appears in UI; tasks trackable; rewards granted; favor flows to house.

---

## Phase 6 — Guild Housing & Social Polish

**Priority**: Medium | **Effort**: 3-5 days | **Gaps**: GAP-009, GAP-010, GAP-029, GAP-030

### T6.1 — Guild Neighborhood Validation (GAP-029)

**Handler**: `HandleHousingSvcsGuildCreateNeighborhood` (exists, implemented)

Verify guild-specific validation:
- `ValidateCreateGuildNeighborhoodSize()` — min members from `minNeighborhoodGroupMembers` CVar
- `CreateNeighborhoodErrorType` enum (Profanity, UndersizedGuild, OversizedGuild)
- Faction restriction auto-applied from guild faction

### T6.2 — Invitation System Completeness (GAP-010)

All invitation handlers exist. Verify:
- `NeighborhoodPlayerGetInviteResponse` uses correct `JamCliHousingInvitation` struct (from T1.4)
- `NeighborhoodGetInvitesResponse` properly serializes invitation array
- Auto-decline flag (`PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD`) persists
- `SMSG_NEIGHBORHOOD_INVITE_NOTIFICATION` (0x5C0010) fires on new invite

### T6.3 — Manager System Verification (GAP-009)

Verify add/remove secondary owner:
- Max `MAX_NEIGHBORHOOD_MANAGERS` (5) enforced
- Role change broadcasts `RosterMemberUpdateInfo[]` (from T1.1)
- `NeighborhoodMirrorData.Managers[]` updated on role change

### T6.4 — Social System Packets (GAP-030)

Verify/add handlers for:
- `CMSG_DECLINE_NEIGHBORHOOD_INVITES` (0x3A00AF) — exists as `HandleDeclineNeighborhoodInvites`
- `CMSG_QUERY_NEIGHBORHOOD_INFO` (0x3E00B7) — exists as `HandleQueryNeighborhoodInfo`
- `CMSG_INVITE_PLAYER_TO_NEIGHBORHOOD` (0x3E019B) — exists as `HandleInvitePlayerToNeighborhood`

### T6.5 — Roster Incremental Updates (GAP-028)

Implement `SMSG_NEIGHBORHOOD_ROSTER_RESIDENT_UPDATE` (0x5C0013) delivery:
- On member join/leave/role-change, push `RosterMemberUpdateInfo[]` to all subscribed players
- Requires subscription tracking from T2.16

**Verification Gate**: Full invitation workflow; guild creation validates; roster updates real-time.

---

## Phase 7 — New Build 66198 Features

**Priority**: Medium | **Effort**: 3-5 days

### T7.1 — Photo Sharing System

**Handlers**: `HandleHousingPhotoSharingCompleteAuthorization`, `HandleHousingPhotoSharingClearAuthorization` (exist as stubs)

Implement:
- Authorization state tracking per player
- `SMSG_HOUSING_PHOTO_SHARING_AUTHORIZATION_RESULT` with proper result codes
- Integration with housing screenshot system (CVar `housingDecorReportScreenshotDistanceThreshold`)

### T7.2 — Bulk Decor Refund

**Handlers**: `HandleGetDecorRefundList`, `HandleGetAllLicensedDecorQuantities` (exist, implemented)

Verify `BulkRefundDecors` flow:
- `JamClientRefundableDecor` struct (DecorID, RefundPrice, ExpiryTime, Flags)
- 2-hour refund window (`REFUND_WINDOW = 2 hours`, currently implemented)
- Proper gold refund on bulk operation

### T7.3 — Exterior Size Customization

**Handlers**: `HandleHousingFixtureSetHouseSize`, `HandleHousingFixtureSetHouseType` (exist, implemented)

Verify Small/Medium/Large tier system:
- `HousingFixtureSize` enum (1=Any, 2=Small, 3=Medium, 4=Large)
- Size validation against `HouseExteriorWmoData` DB2 constraints
- `HOUSING_EXTERIOR_CUSTOMIZATION_SIZE_SMALL/MEDIUM/LARGE` client events

### T7.4 — Transfer Abort for Max Players

Implement `TRANSFER_ABORT_HOUSING_MAX_PLAYERS_IN_HOUSE`:
- Track player count on housing maps
- Reject transfers when interior/exterior is full
- Return appropriate error to client

### T7.5 — Housing Warning System

Implement `ShouldShowHousingWarning()` gate:
- Check expansion access
- Check faction restrictions
- Check player eligibility
- Return warning reasons to client UI

**Verification Gate**: New features from build 66198 functional.

---

## Phase 8 — CVar Enforcement & Feature Flags

**Priority**: Low-Medium | **Effort**: 1-2 days | **Gaps**: GAP-045

### T8.1 — Server-Side CVar Enforcement

Add checks for each CVar in relevant handlers:

| CVar | Handler(s) | Check |
|------|-----------|-------|
| `housingEnableBuyHouse` | BuyHouse, ReservePlot | Block purchase if disabled |
| `housingEnableDeleteHouse` | RelinquishHouse, FixtureDeleteHouse | Block deletion if disabled |
| `housingEnableMoveHouse` | MoveHouse | Block move if disabled |
| `housingEnableCreateCharterNeighborhood` | CharterCreate, CharterFinalize | Block charter if disabled |
| `housingEnableCreateGuildNeighborhood` | GuildCreateNeighborhood | Block guild create if disabled |
| `housingTutorialsEnabled` | StartTutorial | Skip tutorial flow if disabled |
| `performHousingExpansionCheckClient` | Various entry points | Check expansion access |

### T8.2 — Add Missing Enums (GAP-040, GAP-041, GAP-042, GAP-043)

**File**: `HousingDefines.h`

Verify these are present (most already added):
- `HousingPlotOwnerType` (None=0, Stranger=1, Friend=2, Self=3)
- `InvalidPlotScreenshotReason`
- `ReservationFlags` (None, Relinquish, Canceled, Plotless)
- `HouseFinderSuggestionReason` (bitmask)
- `PurchaseHouseDisabledReason` (10 values)
- `CornerstonePurchaseMode` (Basic=0, Import=1, Move=2)
- All initiative enums (NeighborhoodInitiativeTaskType, etc.)

**Verification Gate**: All CVars enforced; all enums complete.

---

## Phase 9 — Polish & Remaining Opcodes

**Priority**: Low | **Effort**: 3-5 days | **Gaps**: GAP-014, GAP-026, GAP-027, GAP-033-048

### T9.1 — Tutorial System Enhancement (GAP-014)

**Handler**: `HandleHousingSvcsStartTutorial` (exists, implemented)

Currently: finds/creates public neighborhood, accepts quest 91863, casts teleport spell.

Enhance:
- Tutorial step tracking per player
- Step-by-step guidance (place first decor, enter house, etc.)
- Tutorial completion reward

### T9.2 — Neighborhood Name Validation (GAP-026)

Implement server-side validation:
- Length check (1-64 chars, `HOUSING_MAX_NAME_LENGTH`)
- Profanity filter integration
- `NEIGHBORHOOD_NAME_VALIDATED` event trigger
- Return `HousingResult::FilterRejected` on profanity

### T9.3 — Cornerstone UI Improvements (GAP-027)

**Handler**: `HandleNeighborhoodOpenCornerstoneUI` (exists, implemented)

Verify: optional data fields correctly serialized per IDA wire format. Support all `CornerstonePurchaseMode` values (Basic, Import, Move).

### T9.4 — Register Remaining Unnamed Opcodes (GAP-033-038)

Register all currently unregistered opcodes as `STATUS_LOGGEDIN` with handlers that:
1. Log the opcode + packet data for future analysis
2. Return `HousingResult::Success` where a response SMSG exists
3. Prevent client disconnect on unhandled opcode

Groups:
- 0x2E0000-0x2E0001 (HouseExterior enter/leave)
- 0x2F0000-0x2F0001 (HouseInterior enter/leave) — 0x2F0001 is `LeaveHouse`, already handled
- 0x300011 (unknown HousingDecor)
- 0x330019-0x33001B (unknown HousingServices)
- 0x330022 (unknown HousingServices)

### T9.5 — Kiosk Mode (GAP-046)

**Handler**: `HandleHousingResetKioskMode` (exists, implemented as "delete context housing")

Verify this is correct behavior for demo/preview mode.

### T9.6 — NeighborhoodPlotMapInfo (GAP-048)

Implement `NeighborhoodPlotMapInfo` struct for UI map display:
- Plot positions from `NeighborhoodPlot` DB2
- Owner info per plot
- Availability status

**Verification Gate**: All 231 opcodes handled (even if stub); no unhandled opcode disconnects; all client UI functional.

---

## Phase 10 — Testing & Verification

**Priority**: Ongoing | **Effort**: Continuous

### Test Scenarios

1. **New Player Flow**: Create character → tutorial → buy first home → place first decor → enter interior
2. **Charter Flow**: Create charter → collect 4 signatures → finalize → neighborhood created
3. **Guild Flow**: Guild leader creates guild neighborhood → members buy plots
4. **Settings**: Change access flags → verify other players see updated permissions → relog persistence
5. **House Finder**: Search neighborhoods → filter by faction → browse → select → teleport
6. **Move House**: Buy in neighborhood A → initiate move to B → decor packed → respawned at B
7. **Eviction**: Owner evicts resident → house despawns → plot freed → evictee notified
8. **Ownership Transfer**: Offer ownership → accept → roles updated → UI reflects
9. **Level Progression**: Complete quests → favor accumulates → level up → budgets increase
10. **Initiative**: Initiative starts → complete tasks → milestone reached → rewards granted
11. **Edit Modes**: Basic → Expert → Layout → Customize → Exterior → all persist correctly
12. **Interior**: Enter house → place/move/remove decor → set theme → exit → re-enter → state persisted
13. **Exterior**: Set house type → change size → add fixtures → position house → lock exterior
14. **Multi-Player**: Two players on same neighborhood → see each other's houses → permissions work

### Sniff Comparison

For each verified handler, compare server response bytes against sniff captures:
- `sniff_analysis_results.txt` — Alliance decorate session hex dumps
- Alliance/Horde housing start captures — full session flows

---

## File Change Summary

| File | Phase | Changes |
|------|-------|---------|
| `HousingDefines.h` | P0, P8 | Verify enums, add missing values |
| `HousingPackets.h` | P0, P1 | Fix RosterEntry, add 5 JAM structs, fix PermissionsFailure |
| `HousingPackets.cpp` | P0, P1 | Serialization for new/fixed structs |
| `Housing.cpp` | P0, P2, P3 | Fix default flags, implement level/favor, add name persistence |
| `Housing.h` | P2, P3 | Add houseName/houseDescription members |
| `HousingHandler.cpp` | P0-P9 | Replace hardcoded values, implement stubs, new handlers |
| `NeighborhoodHandler.cpp` | P0, P3, P6 | Fix AccessFlags, move house, eviction |
| `Opcodes.h` | P0, P9 | Fix name opcode, register missing opcodes |
| `Opcodes.cpp` | P0, P9 | Fix name opcode registration, new handlers |
| `UpdateFields.cpp` | P4 | Audit fragment completeness |
| `HousingMgr.cpp` | P3 | Level/favor progression helpers |
| `Neighborhood.cpp` | P3, P6 | Move house logic, eviction cleanup |
| `HousingMap.cpp` | P3 | Cross-neighborhood move respawn |
| `CharacterDatabase.h` | P2 | New prepared statements (houseName, houseDescription) |
| `CharacterDatabase.cpp` | P2 | SQL for new columns |
| `housing_schema.sql` | P2 | ALTER TABLE add columns |
| Various hotfix SQL | P5 | Seed initiative data |

---

## Dependency Graph

```
Phase 0  (Foundation Fixes)
  ↓
Phase 1  (Wire Format Structs) ← required by P2, P3, P5
  ↓
Phase 2  (Complete Stubs) + Phase 3 (Lifecycle) — parallel after P1
  ↓
Phase 4  (Entity State) — after P2
  ↓
Phase 5  (Initiative) — after P3, P4
  ↓
Phase 6  (Guild/Social) — after P2
  ↓
Phase 7  (66198 Features) — after P2
  ↓
Phase 8  (CVars) + Phase 9 (Polish) — after P6, P7
  ↓
Phase 10 (Testing) — continuous
```

---

## Estimated Total: ~35-55 working days

| Phase | Days | Status |
|-------|------|--------|
| P0: Foundation | 2-3 | Ready to start |
| P1: Wire Formats | 3-5 | Requires P0 |
| P2: Stub Handlers | 5-7 | Requires P1 |
| P3: Lifecycle | 5-7 | Requires P1 |
| P4: Entity State | 3-5 | Requires P2 |
| P5: Initiative | 7-10 | Requires P3, P4 |
| P6: Guild/Social | 3-5 | Requires P2 |
| P7: 66198 Features | 3-5 | Requires P2 |
| P8: CVars | 1-2 | Requires P6 |
| P9: Polish | 3-5 | Requires P8 |
| P10: Testing | Ongoing | Continuous |

---

*This plan covers all 48 identified gaps plus build 66198 delta features, organized by dependency chain and player-facing impact. Each phase has a verification gate before proceeding to the next.*
