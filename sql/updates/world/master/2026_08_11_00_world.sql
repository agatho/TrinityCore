-- ============================================================================
-- Housing tutorial quest givers / enders
-- Source: retail packet captures, client build 12.0.1.65940
--   Alliance C:\sniff\alliance_housing_start\dumps\dump_12.0.1.65940_2026-02-19_10-35-38.pkt
--   Horde    C:\sniff\horde_housing\dumps\dump_12.0.1.65940_2026-02-19_10-51-32.pkt
-- Full analysis: c:\dumps\HOUSING_TUTORIAL_CHAIN_FROM_SNIFFS.md
--
-- ONLY pairs with direct packet evidence are inserted below.
-- Creature entry derived from ObjectGuid: entry = (highQword >> 6) & 0x7FFFFF
-- (23-bit mask; a 24-bit mask leaks mapId bit 29 and is wrong).
--
-- Target DB: integ_world
-- ============================================================================

-- ============================================================================
-- SECTION 1 - CONFIDENCE A : direct offer / accept / turn-in packet naming the
--                            creature GUID or QuestGiverCreatureID.
-- ============================================================================

-- ---- creature_queststarter -------------------------------------------------
-- 94379 "This Old Hearth" <- 233708 Tocho Cloudhide (Steward), Horde, map 2736
--   rec #41441 SMSG_GOSSIP_MESSAGE 0x600018 @0x0124357D  GossipQuests[0]=94379
--   rec #41600 CMSG_QUEST_GIVER_QUERY_QUEST 0x3B0029 @0x0125ADAD
--   rec #41603 SMSG_QUEST_GIVER_QUEST_DETAILS 0x600012 @0x0125AEAF  QuestGiverCreatureID=233708
--   rec #41635 CMSG_QUEST_GIVER_ACCEPT_QUEST 0x3B002A @0x0125C1CF  (byte-decoded)
DELETE FROM `creature_queststarter` WHERE (`id`,`quest`) IN ((233708,94379));
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(233708, 94379, 65940);

-- ---- creature_questender ---------------------------------------------------
-- 94379 "This Old Hearth"        <- 233708 Tocho Cloudhide
--   rec #42770 SMSG_GOSSIP_MESSAGE 0x600018   GossipQuests[0]=94379
--   rec #42813 CMSG_QUEST_GIVER_COMPLETE_QUEST 0x3B002B @0x012D7CF8 -> entry 233708
--   rec #42814 SMSG_QUEST_GIVER_OFFER_REWARD_MESSAGE 0x600014  QuestGiverCreatureID=233708
--   rec #42831 CMSG_QUEST_GIVER_CHOOSE_REWARD 0x3B002C @0x012D8A6B
-- 94210 "Feathering the Nest"    <- 233708 Tocho Cloudhide
--   rec #41332 SMSG_GOSSIP_MESSAGE 0x600018 @0x01240292  GossipQuests[0]=94210
--   rec #41362 CMSG_QUEST_GIVER_COMPLETE_QUEST 0x3B002B @0x01240E3E -> entry 233708
--   rec #41366 SMSG_QUEST_GIVER_OFFER_REWARD_MESSAGE 0x600014  QuestGiverCreatureID=233708
--   rec #41385 CMSG_QUEST_GIVER_CHOOSE_REWARD 0x3B002C
-- 91863 "My First Home"          <- 249850 Lyssabel Dawnpetal (Steward), Alliance, map 2735
--   rec #33932 SMSG_QUEST_GIVER_OFFER_REWARD_MESSAGE 0x600014 @0x01037425  QuestGiverCreatureID=249850
--   rec #33942 CMSG_QUEST_GIVER_CHOOSE_REWARD 0x3B002C @0x01037A66
--              GUID 0x203CAD55E0F3FE8001F6ED0000173CC9 -> Creature/map2735/entry 249850
-- 94995 "To Dye For"             <- 255125 Haleth Turnwater (Dye Crafter)
--   rec #35180 SMSG_GOSSIP_MESSAGE 0x600018 @0x0102CDC0  GossipID 41385, GossipQuests[0]=94995
--   rec #35246 CMSG_QUEST_GIVER_COMPLETE_QUEST 0x3B002B @0x01030972 -> entry 255125
--   rec #35250 SMSG_QUEST_GIVER_OFFER_REWARD_MESSAGE 0x600014  QuestGiverCreatureID=255125
--   rec #35284 CMSG_QUEST_GIVER_CHOOSE_REWARD 0x3B002C
-- 93647 "Lumber For You"         <- 255520 Xiz'ro (Lumberjack)
--   rec #37688 SMSG_GOSSIP_MESSAGE 0x600018 @0x010D6CE7  GossipID 42628, GossipQuests[0]=93647
--   rec #37707 CMSG_QUEST_GIVER_COMPLETE_QUEST 0x3B002B @0x010D7B4B -> entry 255520
--   rec #37713 SMSG_QUEST_GIVER_OFFER_REWARD_MESSAGE 0x600014  QuestGiverCreatureID=255520
--   rec #37743 CMSG_QUEST_GIVER_CHOOSE_REWARD 0x3B002C
DELETE FROM `creature_questender` WHERE (`id`,`quest`) IN
 ((233708,94379),(233708,94210),(249850,91863),(255125,94995),(255520,93647));
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(233708, 94379, 65940),
(233708, 94210, 65940),
(249850, 91863, 65940),
(255125, 94995, 65940),
(255520, 93647, 65940);

-- ============================================================================
-- SECTION 2 - CONFIDENCE B : NPC<->quest association proven by the gossip
--                            quest menu (GossipQuests block of SMSG_GOSSIP_MESSAGE),
--                            but no offer/accept/turn-in packet was captured.
--                            Review before applying.
-- ============================================================================

-- 93104 "Decor Treasure Hunt" <- 253596 The Last Architect (Horde side, map 2736)
--   rec #37114 SMSG_GOSSIP_MESSAGE 0x600018 @0x010B96CF  GossipID 40076
--              GossipQuests[0] QuestID=93104 "Decor Treasure Hunt"
--   The quest was NOT in the player's quest log at that time, so this is the
--   *available-quest* menu -> 253596 is a starter.
--   NOTE: integ_world currently maps 253596 -> 93087 (a different "Decor Treasure
--   Hunt" variant). The sniff shows 93104. Existing 93087 rows are left untouched.
DELETE FROM `creature_queststarter` WHERE (`id`,`quest`) IN ((253596,93104));
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(253596, 93104, 65940);

-- 91863 "My First Home" <- 233063 Lyssabel Dawnpetal (Alliance neighborhood steward)
--   rec #20268 SMSG_GOSSIP_MESSAGE 0x600018 @0x0092C2DD  GossipID 40502
--              GossipQuests[0] QuestID=91863 "My First Home" (Important: True)
--   rec #19489 SMSG_QUEST_GIVER_STATUS 0x60001B -> 233063 Status 0x80000 (ImportantReward)
--   The actual turn-in happened at the duplicate spawn 249850 (same name/subname).
--   233063 is the Alliance mirror of Horde 233708; adding as ender only.
DELETE FROM `creature_questender` WHERE (`id`,`quest`) IN ((233063,91863));
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(233063, 91863, 65940);

-- ============================================================================
-- SECTION 3 - NO NPC EXISTS (do NOT add questgiver rows)
-- ============================================================================
--
-- These quests were offered and/or turned in with the PLAYER'S OWN ObjectGuid
-- (HighGuid type 2 = Player) as QuestGiverGUID. They are AUTO_ACCEPT /
-- AUTO_COMPLETE quests driven by the server, exactly matching their
-- quest_template.Flags. creature_queststarter/creature_questender rows for these
-- are wrong and will not make them work.
--
--   Quest   Title                  Flags        AUTO_ACCEPT  AUTO_COMPLETE
--   93057   A House For You        0x02390000   yes          yes
--   91863   My First Home          0x02780000   yes          no   (ender IS an NPC - see above)
--   94455   Home at Last           0x02790000   yes          yes
--   91968   Welcome Home           0x02390000   yes          yes
--   91969   Time to Decorate       0x02390000   yes          yes
--   94210   Feathering the Nest    0x02380000   yes          no   (ender IS an NPC - see above)
--
-- Evidence highlights:
--   93057 : rec #5781  SMSG_QUEST_GIVER_QUEST_DETAILS 0x600012 @0x00555162, giver = Player
--           rec #16937 SMSG_QUEST_FORCE_REMOVED 0x60001C @0x008065B1 - never turned in
--   94455 : no SMSG_QUEST_GIVER_QUEST_DETAILS anywhere in either capture;
--           credit rec #29251 0x60000C @0x00EB8DD9 ObjectID 257763;
--           turn-in rec #29604 0x3B002B / #29625 0x3B002C, both giver = Player
--   91968 : enters quest log at rec #29588 with no details packet; turn-in giver = Player
--   91969 : rec #30380 0x600012 @0x00EDB456 giver = Player; turn-in giver = Player
--
-- The DB currently contains these CONTRADICTED rows. Consider removing them
-- (left commented out - verify against your own sources first):
--
--   DELETE FROM `creature_queststarter` WHERE (`id`,`quest`) IN
--     ((233063,91968),(233063,91969),(233063,94210),(233708,91863),(233708,94210));
--   DELETE FROM `creature_questender`   WHERE (`id`,`quest`) IN
--     ((233063,91968),(233063,91969),(233063,93057),(233708,91863));
--
-- ============================================================================
-- SECTION 4 - COULD NOT BE ATTRIBUTED (no NPC entry derivable - NOT invented)
-- ============================================================================
--
--   94995 "To Dye For"          - STARTER unknown (already in quest log at capture start)
--   93647 "Lumber For You"      - STARTER unknown (already in quest log at capture start)
--   93104 "Decor Treasure Hunt" - ENDER unknown (never turned in during the capture)
--   93049 "Homework Support"    - quest ID absent from BOTH captures
--   93182 "Healing Homeward"    - quest ID absent from BOTH captures
--   93769 "Midnight: Housing"   - quest ID absent from BOTH captures
--
--   Alliance mirrors of the Horde service NPCs: creature_template suggests
--     255126 Helmi Cooper (Dye Crafter)      ~ 255125 Haleth Turnwater
--     255519 Lestia Goldenstrike (Lumberjack)~ 255520 Xiz'ro
--     248854 The Last Architect              ~ 253596 The Last Architect
--   248854 did broadcast SMSG_QUEST_GIVER_STATUS 0x60001B Status 0x400000
--   (QuestGiverStatus::Quest = "has an available quest"), but the quest ID was
--   never transmitted. NO rows proposed for any of these.
--
--   The mechanism that GRANTS quest 94455 "Home at Last" is not in creature_*
--   tables at all. On Alliance it appeared in the quest log 1.47 s after the
--   91863 turn-in (rec #34161 SMSG_UPDATE_OBJECT 0x580000 @0x01045277), even
--   though quest_template.RewardNextQuest for 91863 is 0 - so it is granted by a
--   script or spell that these captures do not expose.
-- ============================================================================
