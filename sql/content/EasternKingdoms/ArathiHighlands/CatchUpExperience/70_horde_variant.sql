-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Horde finale branch
-- (quest 90898 "Back to Hammerfall")
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
--
-- STATUS (Horde-xval H3, task-3): CAPTURE-SOURCED. The original authoring of this file was
-- a documented gap stub -- the Horde run of the "Siege of Arathi Highlands" chain had not
-- yet been captured, and section 1's quest_template row below was an [INFERRED]-by-analogy
-- placeholder. A Horde cross-validation capture has since recorded the real 90898 data
-- (LogDescription flavor text, QuestDescription, RewardBonusMoney, RewardText, and the real
-- giver/ender creatures) -- section 1 and the new sections 1b/1c below now author that
-- captured data directly, replacing the placeholder text and the two inert
-- creature_queststarter/questender comment blocks from the original stub. Section 2
-- (quest_template_addon branch wiring + the 90911 PrevQuestID fix) was already correct and
-- is unchanged. See the trimmed "PHASE-K HORDE-CAPTURE GAPS" banner at the bottom for what
-- STILL remains uncaptured (objectives, phasing, POI, gossip beyond the 244715 hub picker).
--
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
--
-- DEPENDS ON (read-only): 30_quest_template.sql / 31_quest_template_addon.sql /
-- 33_creature_quest_links.sql (Task 3, Alliance chain, owns 90882-90897/90911's rows --
-- NOT re-inserted or altered here except the single idempotent partial-column UPDATE to
-- 90911's PrevQuestID documented in section 2, which is a bug-fix completion of Task 3's
-- own single-branch-only wiring, not a change to anything Task 3 actually authored for
-- 90897). Must run AFTER 31_quest_template_addon.sql (file-order 70_ > 31_ already
-- guarantees this) since section 2's UPDATE assumes the 90911 row already exists. Also
-- depends on 33_creature_quest_links.sql's Horde-xval H3 task-2 addition of 244715's
-- gossip-hub wiring (61_gossip.sql task-5) for the 90911 hand-off to actually be reachable.
-- ============================================================================


-- ============================================================================
-- 1) quest_template (90898 "Back to Hammerfall") -- CAPTURED Horde data
-- ============================================================================
-- Horde-xval H3 task-3: replaces the original [INFERRED]-by-analogy stub row with the
-- real captured LogDescription/QuestDescription text. LogTitle 'Back to Hammerfall' was
-- already retail-confirmed (Wowhead) and is unchanged.
--
-- LogDescription is now capture-sourced (was previously OMITTED entirely -- no analogous
-- short-form text existed to mirror, and inventing multi-sentence Thrall dialogue would
-- have crossed from "inferred by analogy" into "fabricated as real"; that concern is now
-- moot, this is the captured line).
--
-- QuestDescription is now the captured short form "Return to Hammerfall." (previously an
-- [INFERRED] "Meet Thrall within Hammerfall" placeholder mirrored off 90897's own
-- QuestDescription -- superseded).
--
-- RewardBonusMoney=5350 / RewardXPDifficulty=1 -- tier 1, matches this chain's other
-- terminus-adjacent quests (unchanged from the original analogy-based value; capture
-- confirms it).
--
-- AllowableRaces = RACEMASK_HORDE, computed from THIS worktree's
-- src/server/game/Miscellaneous/RaceMask.h (RACEMASK_HORDE_v = RACEMASK_ALL_PLAYABLE_v
-- & ~(RACEMASK_NEUTRAL_v | RACEMASK_ALLIANCE_v), RaceMask.h:290), same method Task 3
-- used for 90897's RACEMASK_ALLIANCE=2973060173 (30_quest_template.sql:78-87, sanity-
-- checked by recomputing it here too and getting the identical value). Bit set (raceId,
-- GetRaceBit() per RaceMask.h:97-147): {1 Orc, 4 Undead, 5 Tauren, 7 Troll, 8 Goblin,
-- 9 BloodElf, 12 Vulpera, 13 Mag'har Orc, 15 Dracthyr(Horde), 17 Earthen(Horde),
-- 19 Haranir(Horde), 25 Pandaren(Horde), 26 Nightborne, 27 Highmountain Tauren,
-- 30 Zandalari Troll}. Sum of 2^bit = 1309324210. Unchanged from original authoring
-- (this value was always an engine-level computation, not a capture-dependent field).
INSERT INTO `quest_template` (`ID`, `LogTitle`, `LogDescription`, `QuestDescription`, `RewardBonusMoney`, `RewardXPDifficulty`, `AllowableRaces`) VALUES
 (90898, 'Back to Hammerfall',
'We''re not always so lucky that a plan such as this is ended so swiftly and without a surprise portal or two.

That being said, more dangers lurk throughout Azeroth. I know you can be of help to some.

Let us return to Hammerfall to see how they''re recovering.',
'Return to Hammerfall.',
5350, 1,
1309324210) -- RACEMASK_HORDE, see banner above
ON DUPLICATE KEY UPDATE `LogTitle`=VALUES(`LogTitle`), `LogDescription`=VALUES(`LogDescription`), `QuestDescription`=VALUES(`QuestDescription`), `RewardBonusMoney`=VALUES(`RewardBonusMoney`), `RewardXPDifficulty`=VALUES(`RewardXPDifficulty`), `AllowableRaces`=VALUES(`AllowableRaces`);

-- ============================================================================
-- 1b) quest_offer_reward (90898) -- CAPTURED RewardText
-- ============================================================================
-- Real turn-in flavor text, same table/pattern as the Alliance chain's
-- 35_quest_offer_reward.sql Section 1 (that file intentionally does not cover 90898/90911
-- -- Horde-branch rows belong here instead).
INSERT INTO `quest_offer_reward` (`ID`, `RewardText`) VALUES
 (90898, 'Hammerfall will recover in time and become stronger than it was before.')
ON DUPLICATE KEY UPDATE `RewardText`=VALUES(`RewardText`);


-- ============================================================================
-- 2) quest_template_addon -- branch wiring for 90898, plus a required fix to
--    90911's PrevQuestID so the shared terminus accepts EITHER faction finale
-- ============================================================================
--
-- ---- 2a) 90898 itself: PrevQuestID=90896 (shared final Alliance/Horde ancestor,
-- "One Last Ogre"), NextQuestID=90911 ("Your Next Adventure"), ExclusiveGroup=0. ----
-- Mirrors 90897's own row exactly (31_quest_template_addon.sql:47-48:
-- `(90896, 90895, 90897, 0)` / `(90897, 90896, 90911, 0)`).
INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`) VALUES
 (90898, 90896, 90911, 0)
ON DUPLICATE KEY UPDATE `PrevQuestID`=VALUES(`PrevQuestID`), `NextQuestID`=VALUES(`NextQuestID`), `ExclusiveGroup`=VALUES(`ExclusiveGroup`);

-- ---- 2b) DECISION: AllowableRaces-only gating between 90897/90898, NOT a shared
-- ExclusiveGroup. Reasoning, verified by reading this worktree's own engine source
-- (not assumed): ----
--
-- The brief's Requirement 2 offered two options: "shared ExclusiveGroup (negative) OR
-- rely on AllowableRaces to gate them per faction". A negative shared ExclusiveGroup is
-- actively WRONG for this exact scenario and would have broken 90911 for BOTH factions
-- if used -- traced via:
--   * ObjectMgr.cpp:5292-5299 -- at load time, for EVERY quest with a nonzero
--     NextQuestID, the engine automatically appends that quest's OWN id onto
--     `<NextQuestID target>.DependentPreviousQuests`. Since both 90897.NextQuestID=90911
--     (Task 3) and 90898.NextQuestID=90911 (section 2a above) are set, 90911's
--     DependentPreviousQuests is auto-populated at runtime as [90897, 90898] -- NO SQL
--     row encodes this list; it is purely reverse-derived from NextQuestID. This is the
--     literal TC mechanism for "multiple predecessor quests funnel into one".
--   * Player.cpp:15472-15476 (SatisfyQuestDependentQuests) requires BOTH
--     SatisfyQuestPreviousQuest (the single scalar PrevQuestID field -- Player.cpp:
--     15478-15503) AND SatisfyQuestDependentPreviousQuests (the auto-derived list above
--     -- Player.cpp:15505-15546) to pass.
--   * SatisfyQuestDependentPreviousQuests (Player.cpp:15517-15522): for each entry in
--     the auto-derived list, "if IsQuestRewarded(prevId) return true" UNCONDITIONALLY
--     as long as `questInfo->GetExclusiveGroup() >= 0` for that specific prev quest --
--     i.e. plain OR-semantics (any ONE of 90897/90898 rewarded suffices) is the DEFAULT
--     behavior precisely because both already carry ExclusiveGroup=0 (non-negative).
--   * If either 90897 or 90898 carried a NEGATIVE ExclusiveGroup instead (the brief's
--     first option), Player.cpp:15524-15546 flips to "each-from-all" semantics: it would
--     then require EVERY quest in that exclusive group to ALSO be rewarded before 90911
--     unlocks -- meaning a Horde player would need 90897 (Alliance-only, structurally
--     impossible for them to ever complete) rewarded too. That would permanently lock
--     90911 for both factions. CONFIRMED WRONG for this case; not used.
--
-- Therefore: 90897 and 90898 are left with ExclusiveGroup=0 (already true for 90897,
-- Task 3's original value; set explicitly to 0 for 90898 in 2a above), and the
-- faction split is enforced ENTIRELY by AllowableRaces (section 1's Horde mask here;
-- 90897's Alliance mask in 30_quest_template.sql) -- a player only ever sees the one
-- quest matching their own race, so the auto-derived OR-list on 90911 never actually
-- offers a player the "wrong" branch; it only needs to accept whichever ONE the player
-- legitimately completed.
--
-- ---- 2c) REQUIRED FIX: 90911.PrevQuestID 90897 -> 0 (idempotent partial-column UPDATE,
-- does not touch NextQuestID/ExclusiveGroup or any other 90911 column). ----
-- Task 3's original 90911 row (31_quest_template_addon.sql:49: `(90911, 90897, 0, 0)`)
-- hardcoded PrevQuestID=90897 -- correct for an Alliance-only chain at the time it was
-- authored, but SatisfyQuestPreviousQuest (Player.cpp:15478-15503) checks PrevQuestID as
-- a SINGLE scalar id; it has no OR capability of its own. Left at 90897, a Horde player
-- who rewards 90898 would satisfy the auto-derived SatisfyQuestDependentPreviousQuests
-- check (section 2b) but FAIL SatisfyQuestPreviousQuest outright (90897 never rewarded,
-- can't be, wrong race) -- and SatisfyQuestDependentQuests requires BOTH to pass
-- (Player.cpp:15474, logical AND). Net effect: 90911 would stay permanently locked for
-- every Horde character. Setting PrevQuestID=0 makes SatisfyQuestPreviousQuest a no-op
-- (Player.cpp:15481-15482, `if (!qInfo->GetPrevQuestId()) return true;`), leaving
-- SatisfyQuestDependentPreviousQuests (90897 OR 90898 rewarded) as 90911's sole and
-- CORRECT gate for both factions. This is the "PrevQuestID handling for a faction-split
-- predecessor" the brief's Requirement 2 asks to be resolved and commented.
INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`) VALUES
 (90911, 0)
ON DUPLICATE KEY UPDATE `PrevQuestID`=VALUES(`PrevQuestID`);


-- ============================================================================
-- 3) creature_questender / creature_queststarter for 90898 -- CAPTURED, now live
-- ============================================================================
-- The Horde questgiver/ender entries are now capture-sourced (previously UNKNOWN --
-- left as inert SQL comments per Requirement 3's original authoring). Both creature
-- entries already exist as real creature_template rows in this feature's Task 1
-- (10_creature_template.sql): 244666 Thrall (siege climax, already wired as 90896's
-- giver/90893's ender in 33_creature_quest_links.sql) and 244715 Thrall (the Horde
-- finale/hub NPC, added by H2, npcflag=3 gossip+questgiver) -- so unlike the original
-- stub, entry 0 is no longer a concern; both target creatures are real and confirmed.
--
-- Giver: 244666 Thrall (siege climax) -- mirrors Alliance 244667 Jaina -> 90897 (same
-- narrative beat, siege climax handing off to the "return home" quest).
-- Ender: 244715 Thrall (Hammerfall hub) -- mirrors Alliance 244714 Jaina (Stromgarde Keep
-- hub) -- same NPC that also starts/ends 90911 (Task 2 of this Horde-xval fix,
-- 33_creature_quest_links.sql).
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
 (244666, 90898)  -- Thrall (siege climax) -- mirror of Alliance 244667 Jaina -> 90897
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);

INSERT INTO `creature_questender` (`id`, `quest`) VALUES
 (244715, 90898)  -- Thrall (Hammerfall hub) -- mirror of Alliance 244714 Jaina
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);
--
-- The 90911 hub's OWN giver/ender for Horde players (244715 Thrall) is now also wired --
-- see Task 2 of this Horde-xval fix (33_creature_quest_links.sql) -- so a Horde player who
-- rewards 90898 now has a creature (244715) that both accepts 90898's turn-in and
-- offers/accepts 90911 immediately after. That gap is CLOSED as of this task.


-- ============================================================================
-- 4) PHASE-K HORDE-CAPTURE GAPS -- explicitly NOT authored in this task
-- ============================================================================
-- Everything below requires a real Horde playthrough capture (addon dump / sniff /
-- WDB cache) of the "Siege of Arathi Highlands" Horde path before it can be authored as
-- candidate SQL. Updated by Horde-xval H3 task-3 -- several items below are now RESOLVED
-- (struck through) since the 90898 giver/ender entries and text are now capture-sourced
-- (section 1/1b/3 above); what remains is still genuinely uncaptured, not guessed:
--   * [RESOLVED by H3] Horde questgiver/ender creature_template entries -- 244666 (giver)
--     and 244715 (ender/hub) both exist and are now wired (section 3 above); no new
--     creature_template rows were needed, both were already authored (244666 by Task 1,
--     244715 by H2).
--   * Hammerfall-side creature spawns for the Horde mirror of this chain's encounters
--     (90882's gnoll fight already happens at Hammerfall in the SHARED opening -- only
--     the LATER, faction-diverging Stromgarde-vs-Hammerfall content is unconfirmed).
--   * Horde-side PhaseIds/phase_area rows analogous to Task 2's 15901-15905 (Alliance
--     narrative phasing) -- whether the Horde finale phases the zone at all, and how.
--   * [RESOLVED by H3] Dialogue text: 90898's LogDescription and QuestDescription are now
--     capture-sourced (section 1 above). Any Conversation/creature_text rows for the Horde
--     finale beyond the quest_template text itself remain uncaptured.
--   * quest_objectives row(s) for 90898 -- STILL out of scope (this task, H3, only fixes
--     Task 4's 90883/90911 per the brief) -- but now that the real ender 244715 exists
--     (section 3 above), a future pass CAN mirror 90897's own objective pattern
--     (32_quest_objectives.sql:166-172, Type=3 TALKTO) as target 244715 Thrall instead of
--     244714 Jaina. Do not reuse 244714 for a Horde objective.
--   * quest_poi / quest_poi_points for 90898 (Hammerfall-side map markers).
--   * Any Horde-specific gossip_menu / npc_text content at the Hammerfall hub itself
--     (distinct from the 244715 "next adventure" picker menu, which IS now wired --
--     see 61_gossip.sql Horde-xval H3 task-5 -- this item is about a possible SEPARATE
--     Hammerfall-hub-flavor gossip greeting, not the picker).
-- ============================================================================
