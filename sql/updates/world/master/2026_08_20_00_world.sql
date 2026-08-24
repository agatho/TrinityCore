-- Chromie Time terrain: re-gate Silithus: The Wound (1817) = level>=110 AND not-pre-BfA-CT
-- ============================================================================
-- Root cause (RE-verified):
--   Silithus terrain swap 1817 (Kalimdor base map 1 -> TerrainSwapMap 1817, the BfA
--   sword-scar / "The Wound") is an ADDITIVE overlay on the pre-Wound (Cataclysm)
--   Silithus tiles. TC's stock gate on this swap is a single CONDITION_LEVEL>=110 row
--   (see sql/old/8.x .../2019_12_07_00_world.sql). A level-110+ character questing in a
--   PRE-BfA Chromie Time timeline (Cata/TBC/WotLK/MoP/WoD/Legion) therefore wrongly sees
--   the BfA-scarred terrain. Fix: keep the level gate AND additionally suppress 1817
--   whenever the player is in a pre-BfA Chromie Time timeline. No new terrain map id is
--   needed (1817 is an overlay; the base tiles are already the correct pre-Wound terrain).
--
-- Condition model on this branch:
--   CONDITION_CHROMIE_TIME = 60. Its comparand is ActivePlayerData::UiChromieTimeExpansionID,
--   which holds a UiChromieTimeExpansionInfo.ID (DB2 record id), NOT the Expansions enum.
--   Eval: ConditionValue1==0 -> "any Chromie Time"; else -> exact record-id match.
--   Record ids @12.0.7 (wago): Cata=5, TBC=6, WotLK=7, MoP=8, WoD=9, Legion=10,
--                              SL=14, BfA=15, DF=16.
--   Pre-BfA set = {5,6,7,8,9,10}. Because the condition matches ONE id per row, the
--   "player is in any pre-BfA timeline" suppression is expressed as six NegativeCondition=1
--   rows in the SAME ElseGroup (rows in one ElseGroup are ANDed): 1817 stays active only
--   when the player is in NONE of the pre-BfA timelines. A single-row mask form is not
--   possible with this condition's record-id-equality semantics.
--
-- Why this file exists:
--   The prior chromie world data (2026_08_09_20_world.sql) DELETEs the whole SourceEntry=1817
--   condition group and re-inserts only the six chromie negatives, which silently DROPS the
--   stock CONDITION_LEVEL>=110 gate. This file restores the level gate and re-states the six
--   chromie negatives together in ElseGroup 0 so BOTH constraints are ANDed. It supersedes
--   the 1817 rows from 2026_08_09_20_world.sql (applies later; idempotent DELETE+INSERT).
--
-- SourceTypeOrReferenceId=25 = CONDITION_SOURCE_TYPE_TERRAIN_SWAP; SourceEntry = terrain swap
-- map id. ConditionTypeOrReference 27 = CONDITION_LEVEL (ConditionValue1=level,
-- ConditionValue2=ComparisionType; 3 = COMP_TYPE_HIGH_EQ = ">=").
--
-- SCOPE: Only Silithus 1817 is confirmed here. Blasted Lands (1190) and the other terrain
-- swaps re-gated by 2026_08_09_20_world.sql (1066/1074/736/719) are LEFT UNCHANGED by this
-- file. Their base-vs-swap direction (and whether a level gate applies) still needs a
-- chromie zone-travel sniff to verify; treat them as a follow-up. In particular the Blasted
-- Lands (1190) WoD Iron-Horde terrain direction is UNVERIFIED and is intentionally not
-- touched here.
-- ============================================================================

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1817;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
-- preserved stock gate: The Wound only shows at level 110+
(25,0,1817,0,0,27,0,110,3,0,0,0,0,'','Silithus Wound terrain: only at level >= 110'),
-- added chromie suppression: not active in any pre-BfA Chromie Time timeline
(25,0,1817,0,0,60,0,5,0,0,1,0,0,'','Silithus Wound terrain: not active in Cata CT'),
(25,0,1817,0,0,60,0,6,0,0,1,0,0,'','Silithus Wound terrain: not active in TBC CT'),
(25,0,1817,0,0,60,0,7,0,0,1,0,0,'','Silithus Wound terrain: not active in WotLK CT'),
(25,0,1817,0,0,60,0,8,0,0,1,0,0,'','Silithus Wound terrain: not active in MoP CT'),
(25,0,1817,0,0,60,0,9,0,0,1,0,0,'','Silithus Wound terrain: not active in WoD CT'),
(25,0,1817,0,0,60,0,10,0,0,1,0,0,'','Silithus Wound terrain: not active in Legion CT');
