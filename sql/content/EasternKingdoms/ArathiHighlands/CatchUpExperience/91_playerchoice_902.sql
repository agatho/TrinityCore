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
-- WIRE CORRECTION 2026-09-02 (TCHarvest, 6 Arathi captures, builds 69382-69587):
--   UiTextureKitId      0 -> 263    every capture's SMSG_DISPLAY_PLAYER_CHOICE sends 263
--   HideWarboardHeader  1 -> 0      every capture sends false
-- Both were authored from datamine; the captures agree with each other against
-- them. Corrected HERE rather than in a later migration on purpose: the updater
-- pools every included directory and sorts by FILENAME, so a 2026_*.sql patch
-- would run BEFORE this file (2 < 9) and its UPDATE would match zero rows --
-- silently, because MySQL reports success for an UPDATE that matches nothing.
--
-- STILL OPEN, deliberately not changed here: ResponseIds 9021/9022/9023 are
-- local inventions; the wire says 5404/5405/5414, and the response list is
-- composed PER PLAYER (entitlement + progression), so a capture's list is a
-- floor on the content, never a roster. Renumbering would have to move the
-- SourceEntry of every conditions row in 92_*.sql in the same commit. See
-- out/branch_fixes/arathi_rpe_playerchoice_902_corrections.sql in the harvest
-- rig for the full evidence.
-- CANDIDATE ONLY -- idempotent. Requires the RPE C++ finale hook to credit 90911 + route.
-- ============================================================================
DELETE FROM `playerchoice_response_reward` WHERE `ChoiceId`=902;
DELETE FROM `playerchoice_response` WHERE `ChoiceId`=902;
DELETE FROM `playerchoice` WHERE `ChoiceId`=902;

INSERT INTO `playerchoice` (`ChoiceId`, `UiTextureKitId`, `SoundKitId`, `CloseSoundKitId`, `Duration`, `Question`, `PendingChoiceText`, `InfiniteRange`, `HideWarboardHeader`, `KeepOpenAfterChoice`, `ShowChoicesAsList`, `ForceDontShowChoicesAsList`, `RequiresSelection`, `MaxResponses`, `ScriptName`) VALUES
 (902, 263, 0, 0, 0, 'Where Do You Want To Go?', '', 0, 0, 0, 0, 0, 1, 3, 'playerchoice_arathi_rpe_finale');

INSERT INTO `playerchoice_response` (`ChoiceId`, `ResponseId`, `Index`, `ChoiceArtFileId`, `Flags`, `WidgetSetID`, `UiTextureAtlasElementID`, `SoundKitID`, `GroupID`, `UiTextureKitID`, `Answer`, `Header`, `SubHeader`, `ButtonTooltip`, `Description`, `Confirmation`, `RewardQuestID`) VALUES
 (902, 9021, 0, 0, 0, 0, 0, 0, 0, 0, 'Dragonflight',            'Dragonflight',            '', '', 'Adventure within the Dragon Isles to see events leading to the Worldsoul Saga!', '', 65436),
 (902, 9022, 1, 0, 0, 0, 0, 0, 0, 0, 'The War Within Recap',   'The War Within Recap',    '', '', 'Experience an accelerated recap of the War Within story.',                      '', 93929),
 (902, 9023, 2, 0, 0, 0, 0, 0, 0, 0, 'The War Within',         'The War Within',          '', '', 'Continue into the current adventures of the War Within.',                       '', 92405);
