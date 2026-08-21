-- ============================================================================
-- ARATHI CATCH-UP / RPE CONSOLIDATION -- VENDOR / GOSSIP / REWARDS (issue 6)
-- ============================================================================
-- Branch: feature/arathi-rpe   Path: sql/updates/world/master/   Server mapID: 2927
-- Consolidated verbatim from the authoritative content slices (guid block 8000000);
-- runs after 2026_08_21_00 cleanup. Each source slice keeps its own banner + idempotency.
-- npc_vendor -> gossip (leader ScriptName) -> spell_target_position -> playerchoice + its conditions.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 60_vendor.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase H Win'sa vendor
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/addon_npc_vendor.sql
--   (22 raw rows: 5 for Win'sa 245026 + 17 for entry 5188). Entry 5188 is the Chromie-hub
--   Timewalking-tabard vendor (see addon_npc_context.txt / addon_gossip_menu.sql
--   menuid=5188 "Let me browse your goods." / "I've lost my Tabard of the Explorer.")
--   -- NOT part of the Catch-Up Experience roster (not one of Task-1's authored
--   creature_template entries). EXCLUDED per task-6-brief Req.1.
--
-- Column mapping note: addon_npc_vendor.sql's raw capture columns
-- (entry, item, slot, price, maxcount, extendedcost, type[, cost]) are the TCHarvest
-- addon's own shorthand, NOT the literal `npc_vendor` schema -- there is no `price`
-- column in `npc_vendor` (PK is entry,item,ExtendedCost,type; the client-displayed price
-- is item_template.BuyPrice, set elsewhere / already on baseline items). `price` is
-- therefore DROPPED here (informational-only in the capture); `incrtime` was not
-- captured and defaults to 0, matching maxcount=0 (unlimited stock, no restock timer
-- needed).
--
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================

-- Win'sa (245026), Food Vendor -- npcflag=129 (gossip+vendor) already on the Task-1
-- creature_template row (10_creature_template.sql); this file only supplies the wares.
INSERT INTO `npc_vendor` (`entry`, `item`, `slot`, `maxcount`, `incrtime`, `ExtendedCost`, `type`, `VerifiedBuild`) VALUES
(245026, 197858, 1, 0, 0, 0, 1, 69382), -- slot 1, captured price 25000c
(245026, 197857, 2, 0, 0, 0, 1, 69382), -- slot 2, captured price 25000c
(245026, 197855, 3, 0, 0, 0, 1, 69382), -- slot 3, captured price 37500c
(245026, 197856, 4, 0, 0, 0, 1, 69382), -- slot 4, captured price 25000c
(245026, 194680, 5, 0, 0, 0, 1, 69382)  -- slot 5, captured price 37500c
ON DUPLICATE KEY UPDATE `slot`=VALUES(`slot`), `maxcount`=VALUES(`maxcount`), `incrtime`=VALUES(`incrtime`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 61_gossip.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase H gossip (vendor + outro)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/: creature_template_gossip.sql,
--   gossip_menu_option.sql (properly-formed, real-schema captures for 39386/39348),
--   addon_gossip_menu.sql, addon_gossip_menu_option.sql, addon_gossip_option_state.txt
--   (REVIEW-ONLY behavioral capture -- see the Chromie reference block at the end of this
--   file for why it is NOT authored as live data).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================
-- FIX ROUND 1 (task-6 review): this file originally also authored a competing root
-- gossip menu for Chromie (CreatureID 167032, MenuID 167032). Engine-verified regression:
-- `Player::GetGossipMenuForSource` iterates `creature_template_gossip` as a flat vector
-- and the LAST menu whose gossip_menu conditions pass wins; an unconditioned
-- (167032,167032) row sorts after the shipped chromie-time menu (167032,25426) and would
-- SILENTLY OVERRIDE Chromie's real menu server-wide for every Chromie interaction on the
-- realm -- a live regression of already-shipped content. REMOVED. See the reference block
-- at the end of this file for the captured data and the correct place for it (an option
-- under the EXISTING menu 25426, chromie-time feature's domain, not authored here).
-- ============================================================================
-- SCHEMA NOTE (applies to the 2 menus below): the bundle's addon-captured
-- gossip_menu_option rows use a SHORTHAND column set (menuid, optionindex, text, icon,
-- OptionID) that is NOT the literal `gossip_menu_option` table. Cross-referencing the
-- bundle's two PROPERLY-formed rows (39386/39348, sourced from a real capture that
-- already used the live schema) shows the addon's "OptionID" field is actually the real
-- table's `GossipOptionID` column (the DB2 "flavor" id), while the real table's
-- `OptionID` column (PK component, PRIMARY KEY (MenuID,OptionID)) is a per-menu ordinal
-- NOT captured by the addon.
-- ============================================================================

-- ============================================================================
-- SECTION 1 -- creature_template_gossip (in-scope NPCs owned by THIS feature only)
-- ============================================================================
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(245026, 39386, 69382), -- Win'sa, Food Vendor (task-1 npcflag=129)
(244714, 39348, 69382), -- Lady Jaina Proudmoore, Stromgarde Keep hub clone (task-1 npcflag=1)
(244715, 39349, 69382)  -- Thrall, Hammerfall hub clone (H2 npcflag=3) -- Horde-xval H3 task-5, PLACEHOLDER MenuID, see SECTION 3c banner
ON DUPLICATE KEY UPDATE `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 2 -- gossip_menu (TextID = matching npc_text.ID, see 62_npc_text.sql)
-- ============================================================================
-- NOTE (Horde-xval H3 task-5): MenuID 39349's TextID is set equal to itself (39349) by the
-- same convention as 39386/39348, but NO npc_text row for 39349 exists anywhere in this
-- feature's slices (62_npc_text.sql only has 39386/39348) -- 62_npc_text.sql is outside
-- this task's file scope (brief Task 5 touches 61_gossip.sql only). TODO Phase K: author
-- npc_text 39349 (or whatever the real MenuID turns out to be) in 62_npc_text.sql before
-- this is ever applied, or the gossip window will render with empty header text.
INSERT INTO `gossip_menu` (`MenuID`, `TextID`, `VerifiedBuild`) VALUES
(39386, 39386, 69382),
(39348, 39348, 69382),
(39349, 39349, 69382) -- PLACEHOLDER MenuID -- Horde-xval H3 task-5, see SECTION 3c banner; npc_text 39349 NOT yet authored (out of this task's file scope)
ON DUPLICATE KEY UPDATE `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 3a -- gossip_menu_option 39386 (Win'sa vendor browse)
-- Captured verbatim (already real-schema): GossipOptionID 133911, OptionNpc=1
-- (GOSSIP_OPTION_NPC_VENDOR), OptionID=0.
-- ============================================================================
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(39386, 133911, 0, 1, 'Let me browse your goods.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382)
ON DUPLICATE KEY UPDATE `GossipOptionID`=VALUES(`GossipOptionID`), `OptionNpc`=VALUES(`OptionNpc`), `OptionText`=VALUES(`OptionText`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 3b -- gossip_menu_option 39348 (Jaina "next adventure" picker)
-- Captured verbatim (already real-schema): GossipOptionID 133893, OptionID=16777216
-- (0x1000000 -- captured live-server PK value, kept as-is rather than renumbered).
-- Quest-90911 "Your Next Adventure" hub; the level-routed hand-off (10-69->DF,
-- 70-80->TWW Recap, 80+->TWW per Plan Phase B Step 5) is wired by C++ (Task 7/Phase J),
-- not this data row.
-- ============================================================================
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(39348, 133893, 16777216, 0, 'Show me where I could go next.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382)
ON DUPLICATE KEY UPDATE `GossipOptionID`=VALUES(`GossipOptionID`), `OptionNpc`=VALUES(`OptionNpc`), `OptionText`=VALUES(`OptionText`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 3c -- gossip_menu_option 39349 (Thrall "next adventure" picker, Horde mirror)
-- -- Horde-xval H3 task-5, NEW ----
-- ============================================================================
-- Mirrors Jaina's 39348/133893 row exactly, for Thrall 244715 at the Hammerfall hub.
-- GossipOptionID=133894 and OptionText "Show me where I could go next." are CAPTURED
-- (Horde-xval cross-validation) -- real, not inferred. OptionID=16777216 (0x1000000) is
-- reused from Jaina's OptionNpc value BY ANALOGY (same PK-ordinal convention observed on
-- the Alliance side, not independently captured for Thrall) -- flagged here rather than
-- silently presented as equally-confirmed as GossipOptionID/OptionText.
--
-- *** MenuID=39349 IS A PLACEHOLDER, NOT A CAPTURED VALUE ***. The real Horde MenuID was
-- NOT wire-decoded (the capture only exposed a synthetic addon key for this menu, not the
-- server-assigned MenuID). 39349 is authored ONLY because it is Jaina 39348's numeric
-- sibling (+1) and every OTHER captured MenuID/TextID pair in this file happens to equal
-- its own CreatureID-adjacent id by the same shared-numbering convention -- this is an
-- educated guess by analogy, explicitly flagged, NOT presented as real. Do not treat
-- 39349 as confirmed anywhere downstream.
-- TODO Phase K: capture/datamine the real Thrall "next adventure" picker MenuID (wire
-- decode or a fresh addon dump that captures the real gossip_menu id, not just the
-- synthetic key) and replace 39349 (here, in SECTION 1's creature_template_gossip row
-- above, and in SECTION 2's gossip_menu row above) with the confirmed value.
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(39349, 133894, 16777216, 0, 'Show me where I could go next.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382) -- PLACEHOLDER MenuID 39349, see banner above -- GossipOptionID/OptionText [C]
ON DUPLICATE KEY UPDATE `GossipOptionID`=VALUES(`GossipOptionID`), `OptionNpc`=VALUES(`OptionNpc`), `OptionText`=VALUES(`OptionText`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 4 -- "Leave Catch Up Experience" early-exit gossip option (Phase K #5)
-- ============================================================================
-- The capture proved there is NO client opcode/API/string for leaving the RPE (only
-- CMSG_ENCOUNTER_JOURNAL_START_ARATHI_RPE exists, for entering); retail drives the early exit
-- through the guide NPC's gossip. This adds a plain talk option "Leave Catch Up Experience" to
-- BOTH guide menus (Alliance Jaina 244714/39348, Horde Thrall 244715/39349), alongside -- not
-- replacing -- their native "Show me where I could go next." adventure-map option above.
--
-- OptionID = 1 is the per-menu ordinal (gossip_menu_option.OptionID -> struct OrderIndex ->
-- arrives at the script as gossipListId); the C++ handler npc_arathi_rpe_guide::OnGossipSelect
-- (feature/arathi-rpe, zone_arathi_highlands_rpe.cpp) matches gossipListId == 1 and teleports the
-- player to their faction capital via the shared SendPlayerHomeFromRpe path (same exit the finale
-- PlayerChoice 902 uses). OptionNpc=0 (None/plain talk) and GossipOptionID=0 render fine (the
-- packet serializes every option unconditionally, GossipDef.cpp) -- no DB2 flavor row needed for a
-- scripted talk option. The label text is authored server-side (retail's is too -- it is not in
-- the client string table).
-- NOTE: OptionID 1 does not collide with the native option above (its OptionID is 16777216).
-- NOTE: MenuID 39349 remains a PLACEHOLDER (see SECTION 3c banner); when the real Horde guide
--   MenuID is captured, update this row's MenuID together with the SECTION 1/2/3c rows.
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(39348, 0, 1, 0, 'Leave Catch Up Experience', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382), -- Alliance Jaina 244714
(39349, 0, 1, 0, 'Leave Catch Up Experience', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382)  -- Horde Thrall 244715 (PLACEHOLDER MenuID)
ON DUPLICATE KEY UPDATE `GossipOptionID`=VALUES(`GossipOptionID`), `OptionNpc`=VALUES(`OptionNpc`), `OptionText`=VALUES(`OptionText`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- Wire the four RPE faction leaders to npc_arathi_rpe_leader (RegisterCreatureAI on
-- feature/arathi-rpe). This ScriptedAI does two things: (1) the "Leave Catch Up Experience" gossip
-- handler on the hub leaders (244714/244715), and (2) the personal-phase faction gate on the shared
-- co-given quests (90882/90883 at the pad greeters 244643/244642, 90911 at the hubs) -- both leaders
-- stay visible, but only the player's OWN faction leader shows the '!' and offers the quest (the
-- other faction's leader returns GetDialogStatus=None and an empty OnGossipHello). It does not
-- disturb the correct leader's DB-driven questgiver gossip or the native adventure-map option.
UPDATE `creature_template` SET `ScriptName`='npc_arathi_rpe_leader' WHERE `entry` IN (244643, 244642, 244714, 244715);

-- ============================================================================
-- REFERENCE ONLY -- Chromie (167032) captured timeline-picker options (map 85)
-- NOT LIVE DATA. No creature_template_gossip / gossip_menu / gossip_menu_option row is
-- authored for these anywhere in this file (FIX ROUND 1 -- see header note above).
--
-- Why this NPC has no data here at all:
-- Chromie (167032) is a hub NPC on map 85 (Eastern Kingdoms overworld), NOT inside the
-- Arathi Catch-Up instance (map 2796) -- she was never in this feature's ownership.
-- This content branch ALREADY ships her real root gossip menu, MenuID 25426
-- (2026_08_09_20_world.sql / 2026_08_15_00_world_chromie_faq.sql -- the "Chromie Time"
-- feature's own capture: options 51901/51902/51903/109278 + FAQ submenu 31336).
-- `creature_template_gossip` PK is (CreatureID,MenuID); TrinityCore's
-- `Player::GetGossipMenuForSource` walks that table as a FLAT VECTOR and the LAST menu
-- whose gossip_menu conditions pass wins -- so adding a second, unconditioned
-- (167032,167032) row (as this file originally did) sorts after the shipped (167032,25426)
-- row and SILENTLY OVERRIDES Chromie's real menu server-wide, for every Chromie
-- interaction on the realm. That is a live regression, not a scoped addition -- removed.
--
-- What was actually captured this session (addon_gossip_menu_option.sql + the REVIEW-ONLY
-- addon_gossip_option_state.txt), preserved verbatim below for provenance / Phase-K reuse:
--   optionindex 0: OptionID 51901 '|cFF0000FF(Recommended)|r Select a timeline.'  -- launch action
--   optionindex 1: OptionID 51902 'Select a different timeline.'                 -- launch action
--                  ALT (quest-state-gated, Q51443-only context): 109315 'What are Timewalking Campaigns?'
--   optionindex 2: OptionID 51903 'I''d like to return to the present timeline, Chromie.' -- exit
--                  ALT (quest-state-gated, Q51443-only context): 109313 'Can my friends join me?'
--   optionindex 3: OptionID 109314 'What if I don''t want to stay in the timeline I chose?'
--   optionindex 4: OptionID 109276 'I want to talk about something else.'
--   optionindex 5: OptionID 109278 'I have a question about Timewalking Campaigns.'
--   optionindex 6: OptionID 109317 'I want to explore the afterlives.'
--   optionindex 7: OptionID 109316 'I have another question.'
-- (The 109315/109313 alternates are NOT simultaneous options -- addon_gossip_option_state.txt
-- shows they replace 51902/51903 under a different quest-state context; the session never
-- resolved the triggering condition. Not authorable as static always-visible rows without
-- fabricating that condition.)
--
-- Canonical retail entry point for the Arathi Catch-Up launch is the Adventure Guide
-- (AdventureJournal DB2 row, Task 7 / player_catchup_enter.cpp) -- NOT this Chromie
-- gossip path. If the captured Chromie-Time timeline-picker flow above is ever wanted as
-- an ADDITIONAL entry into the Catch-Up Experience, it must be added as an OPTION under
-- the EXISTING menu 25426 (ActionMenuID wiring on one of its options, or a new
-- GossipOptionID within that menu) -- that is the chromie-time feature's domain, not this
-- one's. Flagged as a cross-feature Phase-K coordination item; not actioned here.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 90_spell_target_position.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up / RPE :: spell_target_position for the launch teleport
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server map 2927 (Arathi Highlands RPE, InstanceType=0 normal phased map).
-- Spell 1260320 "Teleport to Arathi Highlands" = Effect 252 (SPELL_EFFECT_TELEPORT_UNITS),
--   EffectIndex 0, ImplicitTarget_1 = 17 (TARGET_DEST_DB) -> destination read from THIS table
--   (server-side; no client DB2). Facing 6.2584 from wago SpellEffect EffectPos_facing.
--   Destination coords = the RE'd/captured landing pad (matches feature/arathi-rpe's hardcoded
--   login-relocate pos -1101.67,-3554.37,48.9203). Source: wago SpellEffect SpellID=1260320.
-- This makes the client-cast / in-game launch spell teleport correctly (the feature/arathi-rpe
--   LOGIN path relocates directly; the in-game tile-launch spell path needs this row).
-- CANDIDATE ONLY -- review before applying. Idempotent upsert on PK (ID,EffectIndex,OrderIndex).
-- ============================================================================
INSERT INTO `spell_target_position` (`ID`, `EffectIndex`, `OrderIndex`, `MapID`, `PositionX`, `PositionY`, `PositionZ`, `Orientation`, `VerifiedBuild`) VALUES
 (1260320, 0, 0, 2927, -1101.67, -3554.37, 48.9203, 6.2584, 69404)
ON DUPLICATE KEY UPDATE `MapID`=VALUES(`MapID`), `PositionX`=VALUES(`PositionX`), `PositionY`=VALUES(`PositionY`), `PositionZ`=VALUES(`PositionZ`), `Orientation`=VALUES(`Orientation`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 91_playerchoice_902.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up / RPE :: PlayerChoice 902 "Where Do You Want To Go?"
-- The quest-90911 "Your Next Adventure" finale picker. Server-authoritative (this fork loads
--   playerchoice/playerchoice_response via ObjectMgr::LoadPlayerChoices). NOT in client DB2.
-- Responses (Warcraft Wiki + Wowhead datamine, level brackets): Dragonflight (10-69) -> "The
--   Dragon Isles Await" 65435(Horde)/65436(Alliance); The War Within Recap (70-80) -> 93929;
--   The War Within (80+) -> 92405 "Meet Arator". RewardQuestID is single-valued so the
--   faction-specific Dragonflight quest (65435 H / 65436 A) is routed by the RPE C++ hook
--   (PlayerChoiceScript::OnResponse in zone_arathi_highlands_rpe.cpp on feature/arathi-rpe -- this
--   fork dispatches player choices via PlayerChoiceScript keyed on playerchoice.ScriptName, NOT a
--   PlayerScript::OnPlayerChoiceResponse hook), which also COMPLETES quest 90911 on any response.
--   RewardQuestID below = the neutral/Alliance default; the hook remaps the DF pair to the
--   player's own faction (implemented Phase K 2026-08-21, zone_arathi_highlands_rpe.cpp).
-- Phase K DONE (2026-08-21): level-bracket gating (show only the age-appropriate response) is now
--   authored in 92_conditions_playerchoice_902.sql (CONDITION_SOURCE_TYPE_PLAYER_CHOICE_RESPONSE
--   level ranges: 9021 DF 10-69, 9022 Recap 70-80, 9023 TWW >=80). Choice text from screenshot.
-- CANDIDATE ONLY -- idempotent. Requires the RPE C++ finale hook to credit 90911 + route.
-- ============================================================================
DELETE FROM `playerchoice_response_reward` WHERE `ChoiceId`=902;
DELETE FROM `playerchoice_response` WHERE `ChoiceId`=902;
DELETE FROM `playerchoice` WHERE `ChoiceId`=902;

INSERT INTO `playerchoice` (`ChoiceId`, `UiTextureKitId`, `SoundKitId`, `CloseSoundKitId`, `Duration`, `Question`, `PendingChoiceText`, `InfiniteRange`, `HideWarboardHeader`, `KeepOpenAfterChoice`, `ShowChoicesAsList`, `ForceDontShowChoicesAsList`, `RequiresSelection`, `MaxResponses`, `ScriptName`) VALUES
 (902, 0, 0, 0, 0, 'Where Do You Want To Go?', '', 0, 1, 0, 0, 0, 1, 3, 'playerchoice_arathi_rpe_finale');

INSERT INTO `playerchoice_response` (`ChoiceId`, `ResponseId`, `Index`, `ChoiceArtFileId`, `Flags`, `WidgetSetID`, `UiTextureAtlasElementID`, `SoundKitID`, `GroupID`, `UiTextureKitID`, `Answer`, `Header`, `SubHeader`, `ButtonTooltip`, `Description`, `Confirmation`, `RewardQuestID`) VALUES
 (902, 9021, 0, 0, 0, 0, 0, 0, 0, 0, 'Dragonflight',            'Dragonflight',            '', '', 'Adventure within the Dragon Isles to see events leading to the Worldsoul Saga!', '', 65436),
 (902, 9022, 1, 0, 0, 0, 0, 0, 0, 0, 'The War Within Recap',   'The War Within Recap',    '', '', 'Experience an accelerated recap of the War Within story.',                      '', 93929),
 (902, 9023, 2, 0, 0, 0, 0, 0, 0, 0, 'The War Within',         'The War Within',          '', '', 'Continue into the current adventures of the War Within.',                       '', 92405);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 92_conditions_playerchoice_902.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up / RPE :: PlayerChoice 902 level-bracket gating (Phase K #3)
-- ============================================================================
-- Closes the "TODO Phase K: level-bracket gating" left in 91_playerchoice_902.sql. Retail shows
-- only the age-appropriate destination(s) in the finale "Where Do You Want To Go?" picker: our
-- own Horde screenshot at ~level 20 offered ONLY Dragonflight, confirming the low end; the 70/80
-- boundaries are the datamined ranges (Warcraft Wiki), encoded exactly here.
--
--   ResponseId 9021  Dragonflight          level 10-69
--   ResponseId 9022  The War Within Recap  level 70-80
--   ResponseId 9023  The War Within        level >= 80 (max level)
--
-- Mechanism (verified against this fork's core, src/server/game/Conditions/ConditionMgr.h):
--   SourceType 36 = CONDITION_SOURCE_TYPE_PLAYER_CHOICE_RESPONSE (line 192; NOT 26=PHASE).
--     Lookup key for type 36 is {SourceGroup = ChoiceId, SourceEntry = ResponseId, SourceId = 0}
--     (ConditionMgr.cpp) -> SourceGroup=902, SourceEntry=9021/9022/9023, SourceId=0.
--   CondType 27 = CONDITION_LEVEL (line 89; NOT 9=QUESTTAKEN). Eval: CompareValues(op=Value2,
--     unitLevel, Value1) -> Value1 = level threshold, Value2 = comparison op.
--   ComparisonType (common/Utilities/Util.h): EQ=0, HIGH(>)=1, LOW(<)=2, HIGH_EQ(>=)=3, LOW_EQ(<=)=4.
--   A closed range = two CONDITION_LEVEL rows on the same response in the SAME ElseGroup (rows in
--   one ElseGroup are AND-ed): one >=min (op 3) and one <=max (op 4).
-- Column set is this fork's 16-column `conditions` layout (incl. ConditionStringValue1), matching
--   22_conditions_phasing.sql.
-- CANDIDATE ONLY -- idempotent (DELETE-scoped by SourceType+SourceGroup, then INSERT ... ODKU).
-- Apply AFTER 91_playerchoice_902.sql (the responses must exist first).
-- ============================================================================
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=36 AND `SourceGroup`=902;

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 -- 9021 Dragonflight: level 10..69
 (36, 902, 9021, 0, 0, 27, 0, 10, 3, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9021 Dragonflight: level >= 10'),
 (36, 902, 9021, 0, 0, 27, 0, 69, 4, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9021 Dragonflight: level <= 69'),
 -- 9022 The War Within Recap: level 70..80
 (36, 902, 9022, 0, 0, 27, 0, 70, 3, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9022 TWW Recap: level >= 70'),
 (36, 902, 9022, 0, 0, 27, 0, 80, 4, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9022 TWW Recap: level <= 80'),
 -- 9023 The War Within: level >= 80 (max level)
 (36, 902, 9023, 0, 0, 27, 0, 80, 3, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9023 The War Within: level >= 80')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);
