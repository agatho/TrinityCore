--
-- Chromie Time terrain: re-gate Silithus: The Wound (1817) on CHROMIE TIME ALONE (drop the level gate).
--
-- Supersedes 2026_08_20_00_world.sql, which restored the stock CONDITION_LEVEL>=110 gate on the
-- Silithus Wound terrain swap (Kalimdor base map 1 -> TerrainSwapMap 1817, the BfA sword-scar).
-- That was WRONG for this build: MaxPlayerLevel = 90, so `level >= 110` can NEVER be satisfied, which
-- made the Wound render for nobody (including normal current-timeline players who SHOULD see it).
--
-- The Wound is the permanent CURRENT-timeline state of Silithus, so the correct gate is purely the
-- timeline: show it UNLESS the player is in a pre-BfA Chromie Time. The old `level>=110` row was a
-- pre-squish proxy for "has progressed past Legion"; Chromie Time now expresses that intent correctly
-- and precisely, so the level condition is both stale (unreachable) and redundant, and is removed.
--
-- Result: 1817 is active (BfA scar shown) for everyone EXCEPT players in a pre-BfA Chromie timeline
-- (Cataclysm/TBC/WotLK/MoP/WoD/Legion). BfA/SL/DF timelines and non-Chromie players of ANY level see
-- the Wound. UiMapPhase 9491 (terrain_worldmap) reverts the map UI automatically when suppressed.
--
-- ConditionValue1 = UIChromieTimeExpansionInfo record id (5=Cata,6=TBC,7=WotLK,8=MoP,9=WoD,10=Legion);
-- ConditionTypeOrReference 60 = CONDITION_CHROMIE_TIME (record-id equality); NegativeCondition=1.
-- Idempotent DELETE+INSERT; applies after and supersedes both 2026_08_09_20 and 2026_08_20_00 for 1817.
--

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1817;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(25,0,1817,0,0,60,0,5,0,0,1,0,0,'','Silithus Wound terrain: not active in Cata Chromie Time'),
(25,0,1817,0,0,60,0,6,0,0,1,0,0,'','Silithus Wound terrain: not active in TBC Chromie Time'),
(25,0,1817,0,0,60,0,7,0,0,1,0,0,'','Silithus Wound terrain: not active in WotLK Chromie Time'),
(25,0,1817,0,0,60,0,8,0,0,1,0,0,'','Silithus Wound terrain: not active in MoP Chromie Time'),
(25,0,1817,0,0,60,0,9,0,0,1,0,0,'','Silithus Wound terrain: not active in WoD Chromie Time'),
(25,0,1817,0,0,60,0,10,0,0,1,0,0,'','Silithus Wound terrain: not active in Legion Chromie Time');
