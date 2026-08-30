-- ============================================================================
-- Arathi Catch-Up / RPE -- "Gnoll Way" (90882) turn-in send-off  (2026-08-22)
-- ============================================================================
-- On turning in the first RPE quest 90882 "Gnoll Way" at the arrival pad, retail plays a short
-- send-off beat: the two pad leaders (Thrall 244642 / Lady Jaina Proudmoore 244643) remark that the
-- gnoll raids point at the farms and then walk out toward Go'shek Farm, right as the follow-up flight
-- quest 90883 "To Go'shek Farm" is auto-offered (quest_template.RewardNextQuest = 90883). This was a
-- content gap -- the leaders had no turn-in choreography. The C++ side is npc_arathi_rpe_leader::
-- OnQuestReward (zone_arathi_highlands_rpe.cpp), which fires this Talk + MovePath for BOTH leaders.
--
-- Data is from OUR OWN captures (69382 Alliance / 69404 Horde):
--   * creature_text: the two lines the leaders speak at this beat (chat type SAY). The captured raw
--     chat-type was 12 (CHAT_MSG_MONSTER_SAY); TrinityCore creature_text.Type 0 == SAY. The tester's
--     character name in Thrall's greeting line is NOT this beat (that is the arrival greeting, a
--     separate group not authored here) -- these are the "let's move / raiding farms" send-off lines.
--   * waypoint_path 2218835 (Jaina) / 2218842 (Thrall): the exact paths the capture recorded them
--     starting (tagged "OOC one-shot WP_START"). MoveType 0 (walk) as captured; the small Z rise off
--     the pad is preserved from the capture nodes.
--
-- Idempotent. No dependency on the phasing/spawn files beyond the pad leaders existing (01/02).
-- ============================================================================

-- ---- Dialogue (creature_text group 0 = the send-off line on each pad leader) ----
DELETE FROM `creature_text` WHERE `CreatureID` IN (244642,244643) AND `GroupID`=0 AND `ID`=0;
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(244642, 0, 0, 'This is starting to look like a coordinated assault. Let''s move!', 0, 0, 100, 0, 0, 0, 0, 0, 0, 'Thrall (RPE pad) - Gnoll Way send-off'),
(244643, 0, 0, 'Raiding farms... the ogres and kobolds must be gathering supplies for a larger strike.', 0, 0, 100, 0, 0, 0, 0, 0, 0, 'Lady Jaina Proudmoore (RPE pad) - Gnoll Way send-off');

-- ---- Fly-away paths (walk out toward Go'shek Farm) ----
DELETE FROM `waypoint_path_node` WHERE `PathId` IN (2218835,2218842);
DELETE FROM `waypoint_path` WHERE `PathId` IN (2218835,2218842);
INSERT INTO `waypoint_path` (`PathId`,`MoveType`,`Flags`,`Velocity`,`Comment`) VALUES
(2218835, 0, 0, 0, 'RPE Jaina 244643 - Gnoll Way send-off walk'),
(2218842, 0, 0, 0, 'RPE Thrall 244642 - Gnoll Way send-off walk');

INSERT INTO `waypoint_path_node` (`PathId`,`NodeId`,`PositionX`,`PositionY`,`PositionZ`) VALUES
-- Jaina (244643) -- 5 nodes, NW off the pad
(2218835, 1, -1088.8317, -3549.2361, 54.55032),
(2218835, 2, -1097.5903, -3544.8923, 55.13541),
(2218835, 3, -1117.1163, -3529.1875, 62.05157),
(2218835, 4, -1144.3698, -3502.1199, 62.05157),
(2218835, 5, -1173.2570, -3453.5313, 62.05157),
-- Thrall (244642) -- 3 nodes, NW off the pad
(2218842, 1, -1089.6615, -3555.3508, 55.26461),
(2218842, 2, -1098.4983, -3555.2449, 58.75526),
(2218842, 3, -1194.8334, -3467.1355, 58.75526);
