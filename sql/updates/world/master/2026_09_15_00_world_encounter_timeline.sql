--
-- Encounter timeline data for the Midnight dungeons, mined from retail captures.
--
-- Source: eleven 12.1.0.69587 captures (Windrunner Spire, Murder Row, Magister Terrace x2, Blinding Vale,
-- Maisara Caverns, Altar of Fangs x3, Voidscar Arena, Voidstorm), decoded byte-exact by
-- tools/encounter_wire_check.py and turned into rows by tools/encounter_timeline_mine.py in the sniff rig.
-- Before this file the table was empty on every line: the only rows ever written (encounter 3107, in
-- feature/encounter-start-end's 2026_08_14_00 file) target a different, dead schema that 2026_08_28_00
-- replaces.
--
-- Nothing below is predicted. Each row is one cast retail itself put on the timeline during a successful
-- pull: Delay = (tick of the SEQUENCE/APPEND that announced it - tick of SMSG_ENCOUNTER_START) + the
-- remaining delay it carried; EncounterEventID, Duration and IsApproximation are copied from that element.
-- Retail announces a future cast several times (a countdown resend, and a fresh instance id on every
-- re-sequence), so announcements of the same EncounterEventID less than 2000 ms apart are one cast and the
-- latest one - nearest the cast - is used. Casts scheduled after the boss died are dropped. Every
-- EncounterEventID is validated against EncounterEvent.db2 (build 69587): all belong to their encounter.
-- Two announcements of EncounterEventID 31 in encounter 3103 are omitted - its DB2 row carries
-- DungeonEncounterID 0 and the loader would reject it on every boot.
--
-- What this table cannot express. Retail re-sequences the whole timeline on a phase change; the table is
-- one flat list per encounter, so a pull is flattened into the casts it actually had. A boss that phases
-- on health will drift from these delays when it is killed faster or slower than in the capture.
--
-- Confidence, per encounter:
--   * 21 of 25 are REVIEW: backed by a single kill, a schedule observed once and not a proven one.
--   * Four were killed more than once and cross-checked:
--       3072 (Magister Terrace)   second kill within 17 ms on every cast           - stable
--       3456 (Altar of Fangs)     three kills, within 1120 ms                       - stable
--       3071 (Magister Terrace)   median -188 ms, worst -2106 ms                    - mostly stable
--       3073 (Magister Terrace)   median +5990 ms, worst +42214 ms                  - phase-driven, REVIEW
--   The per-encounter figures, and the cast-time check against SMSG_INSTANCE_ENCOUNTER_EVENT_CAST_UPDATE,
--   are in the sniff rig's out/encounter_timeline/REPORT.md.
--
-- DungeonEncounterID 3056: 14 casts from the first kill in windrunnerspire_12.1.0.69587 (73185 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3056;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3056,0,241,6000,5000,1,0), -- spell 466556, severity 2, cast seen at 6407 ms
(3056,1,239,10000,5000,1,0), -- spell 466064, severity 2, cast seen at 10053 ms
(3056,2,239,15733,5000,1,0), -- spell 466064, severity 2, cast not seen
(3056,3,241,15733,5000,1,0), -- spell 466556, severity 2, cast not seen
(3056,4,242,15733,5000,1,0), -- spell 467040, severity 2, cast not seen
(3056,5,241,21907,5000,1,0), -- spell 466556, severity 2, cast not seen
(3056,6,239,23053,5000,1,0), -- spell 466064, severity 2, cast not seen
(3056,7,241,42400,5000,1,0), -- spell 466556, severity 2, cast seen at 42484 ms
(3056,8,239,46400,5000,1,0), -- spell 466064, severity 2, cast seen at 47354 ms
(3056,9,241,57994,5000,1,0), -- spell 466556, severity 2, cast seen at 58287 ms
(3056,10,239,60354,5000,1,0), -- spell 466064, severity 2, cast seen at 60711 ms
(3056,11,239,66773,5000,1,0), -- spell 466064, severity 2, cast not seen
(3056,12,241,66773,5000,1,0), -- spell 466556, severity 2, cast not seen
(3056,13,242,66773,5000,1,0); -- spell 467040, severity 2, cast not seen

-- DungeonEncounterID 3057: 4 casts from the first kill in windrunnerspire_12.1.0.69587 (96081 ms fight); 1 kill(s) / 3 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3057;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3057,0,28,8000,5000,0,0), -- spell 472745, severity 2, cast seen at 8024 ms
(3057,1,25,17333,5000,0,0), -- spell 472888, severity 2, cast seen at 17350 ms
(3057,2,28,35357,5000,0,0), -- spell 472745, severity 2, cast seen at 35336 ms
(3057,3,27,48000,5000,0,0); -- spell 472736, severity 2, cast not seen

-- DungeonEncounterID 3058: 22 casts from the first kill in windrunnerspire_12.1.0.69587 (156980 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3058;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3058,0,210,3000,5000,1,0), -- spell 467620, severity 2, cast seen at 3626 ms
(3058,1,212,10000,5000,1,0), -- spell 472081, severity 2, cast seen at 10902 ms
(3058,2,210,21220,5000,1,0), -- spell 467620, severity 2, cast seen at 21875 ms
(3058,3,212,29187,0,1,0), -- spell 472081, severity 2, cast not seen
(3058,4,212,33193,0,1,0), -- spell 472081, severity 2, cast not seen
(3058,5,216,33193,5000,1,0), -- spell 470963, severity 2, cast seen at 34055 ms
(3058,6,216,42055,5000,1,0), -- spell 470963, severity 2, cast seen at 43067 ms
(3058,7,216,51067,5000,1,0), -- spell 470963, severity 2, cast seen at 52071 ms
(3058,8,216,60071,5000,1,0), -- spell 470963, severity 2, cast seen at 61089 ms
(3058,9,216,69089,5000,1,0), -- spell 470963, severity 2, cast seen at 70089 ms
(3058,10,216,78089,5000,1,0), -- spell 470963, severity 2, cast seen at 79113 ms
(3058,11,216,84024,5000,1,0), -- spell 470963, severity 2, cast not seen
(3058,12,210,85198,5000,1,0), -- spell 467620, severity 2, cast seen at 86287 ms
(3058,13,216,87111,5000,1,0), -- spell 470963, severity 2, cast not seen
(3058,14,212,92198,5000,1,0), -- spell 472081, severity 2, cast seen at 93598 ms
(3058,15,216,104901,5000,1,0), -- spell 470963, severity 2, cast seen at 105762 ms
(3058,16,216,113762,5000,1,0), -- spell 470963, severity 2, cast seen at 114769 ms
(3058,17,216,122769,5000,1,0), -- spell 470963, severity 2, cast seen at 123778 ms
(3058,18,216,131778,5000,1,0), -- spell 470963, severity 2, cast seen at 132798 ms
(3058,19,216,140901,0,1,0), -- spell 470963, severity 2, cast not seen
(3058,20,210,143900,5000,1,0), -- spell 467620, severity 2, cast seen at 144654 ms
(3058,21,212,150900,5000,1,0); -- spell 472081, severity 2, cast seen at 151952 ms

-- DungeonEncounterID 3059: 7 casts from the first kill in windrunnerspire_12.1.0.69587 (97990 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3059;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3059,0,23,9000,5000,0,0), -- spell 472556, severity 2, cast seen at 9523 ms
(3059,1,21,24000,5000,0,0), -- spell 468429, severity 2, cast seen at 25399 ms
(3059,2,23,47006,5000,0,0), -- spell 472556, severity 2, cast seen at 47522 ms
(3059,3,24,57006,5000,0,0), -- spell 472662, severity 2, cast seen at 57027 ms
(3059,4,538,59540,5000,0,0), -- spell 1253986, severity 2, cast seen at 59949 ms
(3059,5,22,75006,5000,0,0), -- spell 474528, severity 2, cast seen at 75009 ms
(3059,6,21,89006,5000,0,0); -- spell 468429, severity 2, cast seen at 90408 ms

-- DungeonEncounterID 3071: 13 casts from the first kill in magisterterrace3boss12.1.0.69587 (66080 ms fight); 2 kill(s) / 2 pull(s) captured
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3071;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3071,0,286,5000,5000,1,0), -- spell 474496, severity 2, cast seen at 6108 ms
(3071,1,288,15000,5000,1,0), -- spell 1214081, severity 2, cast seen at 15805 ms
(3071,2,287,22000,5000,1,0), -- spell 1214032, severity 2, cast seen at 22692 ms
(3071,3,286,28608,5000,1,0), -- spell 474496, severity 2, cast seen at 28746 ms
(3071,4,288,38805,5000,1,0), -- spell 1214081, severity 2, cast seen at 40034 ms
(3071,5,281,45697,0,1,0), -- spell 474345, severity 2, cast not seen
(3071,6,286,45697,5000,1,0), -- spell 474496, severity 2, cast not seen
(3071,7,281,48705,0,1,0), -- spell 474345, severity 2, cast not seen
(3071,8,286,51246,5000,1,0), -- spell 474496, severity 2, cast not seen
(3071,9,286,54264,5000,1,0), -- spell 474496, severity 2, cast not seen
(3071,10,288,57382,5000,1,0), -- spell 1214081, severity 2, cast not seen
(3071,11,288,63034,5000,1,0), -- spell 1214081, severity 2, cast not seen
(3071,12,288,66049,5000,1,0); -- spell 1214081, severity 2, cast not seen

-- DungeonEncounterID 3072: 6 casts from the first kill in magisterterrace3boss12.1.0.69587 (65848 ms fight); 2 kill(s) / 2 pull(s) captured
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3072;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3072,0,95,7000,5000,1,0), -- spell 1225787, severity 2, cast seen at 7269 ms
(3072,1,93,17000,5000,1,0), -- spell 1224903, severity 2, cast seen at 18204 ms
(3072,2,94,26000,5000,1,0), -- spell 1248689, severity 2, cast seen at 26701 ms
(3072,3,95,36269,5000,1,0), -- spell 1225787, severity 2, cast seen at 36409 ms
(3072,4,96,51000,5000,1,0), -- spell 1225193, severity 2, cast seen at 52219 ms
(3072,5,95,64226,5000,1,0); -- spell 1225787, severity 2, cast seen at 64335 ms

-- DungeonEncounterID 3073: 6 casts from the first kill in magisterterrace3boss12.1.0.69587 (64394 ms fight); 2 kill(s) / 2 pull(s) captured
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3073;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3073,0,760,5000,5000,1,0), -- spell 1296205, severity 2, cast seen at 5794 ms
(3073,1,100,16637,5000,1,0), -- spell 1223961, severity 2, cast seen at 16739 ms
(3073,2,98,27649,5000,1,0), -- spell 1224129, severity 2, cast not seen
(3073,3,98,37637,5000,1,0), -- spell 1224129, severity 2, cast not seen
(3073,4,98,41274,5000,1,0), -- spell 1224129, severity 2, cast seen at 42198 ms
(3073,5,100,62212,5000,1,0); -- spell 1223961, severity 2, cast seen at 62845 ms

-- DungeonEncounterID 3074: 22 casts from the first kill in magisterterracemidnight_12.1.0.69587 (157879 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3074;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3074,0,420,7000,5000,1,0), -- spell 1280106, severity 2, cast seen at 8148 ms
(3074,1,290,13000,5000,1,0), -- spell 1215893, severity 2, cast seen at 13002 ms
(3074,2,292,16000,5000,1,0), -- spell 1215067, severity 2, cast seen at 16673 ms
(3074,3,420,23148,5000,1,0), -- spell 1280106, severity 2, cast seen at 23153 ms
(3074,4,290,33002,5000,1,0), -- spell 1215893, severity 2, cast seen at 34098 ms
(3074,5,420,38153,5000,1,0), -- spell 1280106, severity 2, cast seen at 38555 ms
(3074,6,292,47673,5000,1,0), -- spell 1215067, severity 2, cast seen at 48280 ms
(3074,7,420,53555,5000,1,0), -- spell 1280106, severity 2, cast seen at 53557 ms
(3074,8,290,54098,5000,1,0), -- spell 1215893, severity 2, cast not seen
(3074,9,420,68557,5000,1,0), -- spell 1280106, severity 2, cast seen at 68952 ms
(3074,10,290,77207,5000,1,0), -- spell 1215893, severity 2, cast seen at 77442 ms
(3074,11,292,79280,5000,1,0), -- spell 1215067, severity 2, cast seen at 79875 ms
(3074,12,420,83952,5000,1,0), -- spell 1280106, severity 2, cast seen at 85124 ms
(3074,13,290,97442,5000,1,0), -- spell 1215893, severity 2, cast seen at 98483 ms
(3074,14,420,100124,5000,1,0), -- spell 1280106, severity 2, cast seen at 100893 ms
(3074,15,292,110875,5000,1,0), -- spell 1215067, severity 2, cast seen at 111407 ms
(3074,16,420,115893,5000,1,0), -- spell 1280106, severity 2, cast seen at 116645 ms
(3074,17,290,118483,5000,1,0), -- spell 1215893, severity 2, cast seen at 120284 ms
(3074,18,420,131645,5000,1,0), -- spell 1280106, severity 2, cast seen at 132412 ms
(3074,19,290,140284,5000,1,0), -- spell 1215893, severity 2, cast seen at 140901 ms
(3074,20,292,142407,5000,1,0), -- spell 1215067, severity 2, cast seen at 142915 ms
(3074,21,420,147412,5000,1,0); -- spell 1280106, severity 2, cast seen at 148157 ms

-- DungeonEncounterID 3101: 10 casts from the first kill in murderrow_12.1.0.69587_ (84346 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3101;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3101,0,122,8000,5000,1,0), -- spell 1253811, severity 2, cast seen at 8451 ms
(3101,1,120,15009,5000,1,0), -- spell 1264095, severity 2, cast seen at 15765 ms
(3101,2,202,25009,5000,1,0), -- spell 474240, severity 2, cast seen at 26704 ms
(3101,3,122,35951,5000,1,0), -- spell 1253811, severity 2, cast seen at 36433 ms
(3101,4,120,45769,5000,1,0), -- spell 1264095, severity 2, cast not seen
(3101,5,202,51705,5000,1,0), -- spell 474240, severity 2, cast not seen
(3101,6,122,56652,5000,1,0), -- spell 1253811, severity 2, cast not seen
(3101,7,122,63933,5000,1,0), -- spell 1253811, severity 2, cast not seen
(3101,8,122,76739,5000,1,0), -- spell 1253811, severity 2, cast seen at 77743 ms
(3101,9,120,83739,5000,1,0); -- spell 1264095, severity 2, cast seen at 83823 ms

-- DungeonEncounterID 3102: 17 casts from the first kill in murderrow_12.1.0.69587_ (115821 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3102;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3102,0,127,8000,5000,0,0), -- spell 474478, severity 2, cast seen at 7998 ms
(3102,1,124,12000,5000,0,0), -- spell 474765, severity 2, cast seen at 12007 ms
(3102,2,123,18000,5000,0,0), -- spell 1214357, severity 2, cast seen at 18020 ms
(3102,3,193,26000,5000,0,0), -- spell 1222795, severity 2, cast seen at 25997 ms
(3102,4,124,28007,5000,0,0), -- spell 474765, severity 2, cast seen at 29008 ms
(3102,5,125,36000,5000,0,0), -- spell 1218347, severity 2, cast seen at 36465 ms
(3102,6,127,50023,5000,0,0), -- spell 474478, severity 2, cast seen at 50047 ms
(3102,7,124,54023,5000,0,0), -- spell 474765, severity 2, cast seen at 54038 ms
(3102,8,123,60023,5000,0,0), -- spell 1214357, severity 2, cast seen at 60035 ms
(3102,9,193,68023,5000,0,0), -- spell 1222795, severity 2, cast seen at 68031 ms
(3102,10,124,70038,5000,0,0), -- spell 474765, severity 2, cast seen at 71044 ms
(3102,11,125,78023,5000,0,0), -- spell 1218347, severity 2, cast seen at 78816 ms
(3102,12,127,92052,5000,0,0), -- spell 474478, severity 2, cast seen at 92074 ms
(3102,13,124,96052,5000,0,0), -- spell 474765, severity 2, cast seen at 96064 ms
(3102,14,123,102052,5000,0,0), -- spell 1214357, severity 2, cast seen at 102059 ms
(3102,15,193,110052,5000,0,0), -- spell 1222795, severity 2, cast seen at 110066 ms
(3102,16,124,112064,5000,0,0); -- spell 474765, severity 2, cast seen at 113097 ms

-- DungeonEncounterID 3103: 14 casts from the first kill in murderrow_12.1.0.69587_ (118014 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3103;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3103,0,30,6000,5000,1,0), -- spell 473898, severity 2, cast seen at 7059 ms
(3103,1,752,30000,5000,1,0), -- spell 1295452, severity 2, cast seen at 30161 ms
(3103,2,30,34059,5000,1,0), -- spell 473898, severity 2, cast not seen
(3103,3,32,35000,5000,1,0), -- spell 474197, severity 2, cast seen at 35015 ms
(3103,4,30,39023,0,1,0), -- spell 473898, severity 2, cast seen at 39860 ms
(3103,5,30,60017,5000,1,0), -- spell 473898, severity 2, cast seen at 60543 ms
(3103,6,30,66860,5000,1,0), -- spell 473898, severity 2, cast not seen
(3103,7,30,81017,5000,1,0), -- spell 473898, severity 2, cast not seen
(3103,8,752,84017,5000,1,0), -- spell 1295452, severity 2, cast not seen
(3103,9,30,87543,5000,1,0), -- spell 473898, severity 2, cast not seen
(3103,10,32,89017,5000,1,0), -- spell 474197, severity 2, cast not seen
(3103,11,30,97330,5000,1,0), -- spell 473898, severity 2, cast not seen
(3103,12,30,111826,5000,1,0), -- spell 473898, severity 2, cast not seen
(3103,13,30,117328,5000,1,0); -- spell 473898, severity 2, cast seen at 117626 ms

-- DungeonEncounterID 3105: 9 casts from the first kill in murderrow_12.1.0.69587_ (141362 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3105;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3105,0,38,10000,5000,1,0), -- spell 474408, severity 2, cast seen at 11773 ms
(3105,1,37,15000,5000,1,0), -- spell 1218203, severity 2, cast seen at 16637 ms
(3105,2,207,24000,5000,0,0), -- spell 1224478, severity 2, cast seen at 25664 ms
(3105,3,38,68773,5000,1,0), -- spell 474408, severity 2, cast seen at 69257 ms
(3105,4,37,71637,5000,1,0), -- spell 1218203, severity 2, cast seen at 71692 ms
(3105,5,207,84664,5000,0,0), -- spell 1224478, severity 2, cast not seen
(3105,6,38,126257,5000,1,0), -- spell 474408, severity 2, cast not seen
(3105,7,37,126692,5000,1,0), -- spell 1218203, severity 2, cast seen at 128417 ms
(3105,8,207,139613,5000,0,0); -- spell 1224478, severity 2, cast not seen

-- DungeonEncounterID 3199: 15 casts from the first kill in blindingvaledung_12.1.0.69587_ (92809 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3199;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3199,0,175,4000,5000,0,0), -- spell 1235640, severity 2, cast seen at 4010 ms
(3199,1,173,5000,5000,0,0), -- spell 1234753, severity 2, cast seen at 4972 ms
(3199,2,175,14010,5000,0,0), -- spell 1235640, severity 2, cast seen at 13973 ms
(3199,3,174,20000,5000,0,0), -- spell 1234850, severity 2, cast seen at 19987 ms
(3199,4,175,23973,5000,0,0), -- spell 1235640, severity 2, cast seen at 24025 ms
(3199,5,175,33993,5000,0,0), -- spell 1235640, severity 2, cast seen at 34004 ms
(3199,6,177,35000,5000,0,0), -- spell 1235564, severity 2, cast seen at 35723 ms
(3199,7,175,44004,5000,0,0), -- spell 1235640, severity 2, cast seen at 43985 ms
(3199,8,173,49972,5000,0,0), -- spell 1234753, severity 2, cast seen at 49993 ms
(3199,9,175,53984,5000,0,0), -- spell 1235640, severity 2, cast seen at 53977 ms
(3199,10,175,63977,5000,0,0), -- spell 1235640, severity 2, cast seen at 63975 ms
(3199,11,174,64987,5000,0,0), -- spell 1234850, severity 2, cast not seen
(3199,12,175,73975,5000,0,0), -- spell 1235640, severity 2, cast seen at 73990 ms
(3199,13,177,79970,5000,0,0), -- spell 1235564, severity 2, cast seen at 81507 ms
(3199,14,175,83990,5000,0,0); -- spell 1235640, severity 2, cast seen at 84001 ms

-- DungeonEncounterID 3200: 6 casts from the first kill in blindingvaledung_12.1.0.69587_ (78533 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3200;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3200,0,178,6000,5000,1,0), -- spell 1236746, severity 2, cast seen at 6462 ms
(3200,1,179,20000,5000,1,0), -- spell 1236709, severity 2, cast seen at 20643 ms
(3200,2,180,40000,5000,1,0), -- spell 1237090, severity 2, cast seen at 40537 ms
(3200,3,178,51533,5000,1,0), -- spell 1236746, severity 2, cast seen at 52285 ms
(3200,4,179,65533,5000,1,0), -- spell 1236709, severity 2, cast seen at 65667 ms
(3200,5,178,69462,5000,1,0); -- spell 1236746, severity 2, cast not seen

-- DungeonEncounterID 3201: 12 casts from the first kill in blindingvaledung_12.1.0.69587_ (90822 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3201;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3201,0,186,500,5000,0,0), -- spell 1239882, severity 2, cast seen at 495 ms
(3201,1,181,5000,5000,0,0), -- spell 1239824, severity 2, cast seen at 5518 ms
(3201,2,182,18000,5000,0,0), -- spell 1240098, severity 2, cast seen at 18597 ms
(3201,3,181,26004,5000,0,0), -- spell 1239824, severity 2, cast seen at 26655 ms
(3201,4,182,38998,5000,0,0), -- spell 1240098, severity 2, cast seen at 39718 ms
(3201,5,181,46993,5000,0,0), -- spell 1239824, severity 2, cast seen at 47738 ms
(3201,6,184,58706,5000,0,0), -- spell 1241058, severity 2, cast seen at 58710 ms
(3201,7,182,59992,5000,0,0), -- spell 1240098, severity 2, cast not seen
(3201,8,183,64706,5000,0,0), -- spell 1240210, severity 2, cast seen at 64703 ms
(3201,9,181,68007,5000,0,0), -- spell 1239824, severity 2, cast not seen
(3201,10,184,79710,5000,0,0), -- spell 1241058, severity 2, cast seen at 79729 ms
(3201,11,183,85703,5000,0,0); -- spell 1240210, severity 2, cast seen at 85695 ms

-- DungeonEncounterID 3202: 6 casts from the first kill in blindingvaledung_12.1.0.69587_ (79716 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3202;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3202,0,189,4000,5000,0,0), -- spell 1246372, severity 2, cast seen at 4006 ms
(3202,1,190,18000,5000,0,0), -- spell 1247685, severity 2, cast seen at 18006 ms
(3202,2,191,32000,5000,0,0), -- spell 1246607, severity 2, cast not seen
(3202,3,189,49008,5000,0,0), -- spell 1246372, severity 2, cast seen at 49008 ms
(3202,4,190,63004,5000,0,0), -- spell 1247685, severity 2, cast seen at 63013 ms
(3202,5,191,77012,5000,0,0); -- spell 1246607, severity 2, cast not seen

-- DungeonEncounterID 3212: 10 casts from the first kill in 12.1.0.69587mAISARAcAVERNS (77993 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3212;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3212,0,150,5000,5000,0,0), -- spell 1266480, severity 2, cast seen at 5846 ms
(3212,1,154,12000,5000,0,0), -- spell 1246666, severity 2, cast seen at 12002 ms
(3212,2,152,20000,5000,0,0), -- spell 1260731, severity 2, cast seen at 20005 ms
(3212,3,151,28000,5000,0,0), -- spell 1243900, severity 2, cast seen at 28346 ms
(3212,4,153,35000,5000,0,0), -- spell 1260643, severity 2, cast seen at 35011 ms
(3212,5,155,41000,5000,0,0), -- spell 1249479, severity 2, cast seen at 41019 ms
(3212,6,150,50000,5000,0,0), -- spell 1266480, severity 2, cast seen at 50737 ms
(3212,7,154,57002,5000,0,0), -- spell 1246666, severity 2, cast seen at 57000 ms
(3212,8,152,65005,5000,0,0), -- spell 1260731, severity 2, cast seen at 64991 ms
(3212,9,151,73021,5000,0,0); -- spell 1243900, severity 2, cast seen at 73305 ms

-- DungeonEncounterID 3213: 7 casts from the first kill in 12.1.0.69587mAISARAcAVERNS (84888 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3213;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3213,0,16,3000,5000,0,0), -- spell 1251554, severity 2, cast seen at 3004 ms
(3213,1,19,14166,5000,0,0), -- spell 1251204, severity 2, cast seen at 14196 ms
(3213,2,17,25333,5000,0,0), -- spell 1252054, severity 2, cast seen at 25337 ms
(3213,3,16,36504,5000,0,0), -- spell 1251554, severity 2, cast seen at 36531 ms
(3213,4,19,47696,5000,0,0), -- spell 1251204, severity 2, cast seen at 47676 ms
(3213,5,17,58837,5000,0,0), -- spell 1252054, severity 2, cast seen at 58843 ms
(3213,6,20,70000,5000,0,0); -- spell 1250708, severity 2, cast not seen

-- DungeonEncounterID 3214: 7 casts from the first kill in 12.1.0.69587mAISARAcAVERNS (130130 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3214;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3214,0,156,4000,5000,0,0), -- spell 1251023, severity 2, cast seen at 4012 ms
(3214,1,157,17200,5000,0,0), -- spell 1252676, severity 2, cast seen at 17214 ms
(3214,2,156,30412,5000,0,0), -- spell 1251023, severity 2, cast seen at 30416 ms
(3214,3,157,43604,5000,0,0), -- spell 1252676, severity 2, cast seen at 43609 ms
(3214,4,156,56816,5000,0,0), -- spell 1251023, severity 2, cast seen at 56804 ms
(3214,5,158,70000,5000,0,0), -- spell 1253788, severity 2, cast not seen
(3214,6,156,124045,5000,0,0); -- spell 1251023, severity 2, cast seen at 124046 ms

-- DungeonEncounterID 3285: 7 casts from the first kill in Voidscararena12.1.0.69587 (65350 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3285;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3285,0,558,6000,5000,0,0), -- spell 1222098, severity 2, cast not seen
(3285,1,782,15991,5000,0,0), -- spell 1296963, severity 2, cast seen at 16010 ms
(3285,2,39,24991,5000,0,0), -- spell 1297017, severity 2, cast seen at 25009 ms
(3285,3,41,30991,5000,0,0), -- spell 1300259, severity 2, cast seen at 31001 ms
(3285,4,558,40994,5000,0,0), -- spell 1222098, severity 2, cast not seen
(3285,5,782,50999,5000,0,0), -- spell 1296963, severity 2, cast seen at 51006 ms
(3285,6,39,59999,5000,0,0); -- spell 1297017, severity 2, cast seen at 60026 ms

-- DungeonEncounterID 3286: 12 casts from the first kill in Voidscararena12.1.0.69587 (81809 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3286;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3286,0,55,5000,5000,0,0), -- spell 1226120, severity 2, cast seen at 5027 ms
(3286,1,47,10000,5000,0,0), -- spell 1222642, severity 2, cast seen at 10027 ms
(3286,2,54,15000,5000,0,0), -- spell 1222721, severity 2, cast seen at 14992 ms
(3286,3,55,25027,5000,0,0), -- spell 1226120, severity 2, cast seen at 24992 ms
(3286,4,47,30027,5000,0,0), -- spell 1222642, severity 2, cast seen at 30013 ms
(3286,5,297,35000,5000,0,0), -- spell 1262497, severity 2, cast seen at 35012 ms
(3286,6,54,44992,5000,0,0), -- spell 1222721, severity 2, cast seen at 45004 ms
(3286,7,55,55003,5000,0,0), -- spell 1226120, severity 2, cast seen at 55001 ms
(3286,8,47,60003,5000,0,0), -- spell 1222642, severity 2, cast seen at 60023 ms
(3286,9,54,65003,5000,0,0), -- spell 1222721, severity 2, cast seen at 65020 ms
(3286,10,55,75001,5000,0,0), -- spell 1226120, severity 2, cast seen at 75030 ms
(3286,11,47,80023,5000,0,0); -- spell 1222642, severity 2, cast seen at 80027 ms

-- DungeonEncounterID 3287: 8 casts from the first kill in Voidscararena12.1.0.69587 (77279 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3287;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3287,0,56,5000,5000,0,0), -- spell 1282770, severity 2, cast seen at 5007 ms
(3287,1,57,17000,5000,0,0), -- spell 1227264, severity 2, cast seen at 17021 ms
(3287,2,58,28000,5000,0,0), -- spell 1263982, severity 2, cast seen at 28026 ms
(3287,3,961,34000,5000,0,0), -- spell 1311923, severity 2, cast seen at 34011 ms
(3287,4,56,48020,5000,0,0), -- spell 1282770, severity 2, cast seen at 48035 ms
(3287,5,57,60020,5000,0,0), -- spell 1227264, severity 2, cast seen at 60042 ms
(3287,6,58,71020,5000,0,0), -- spell 1263982, severity 2, cast seen at 71041 ms
(3287,7,961,77020,5000,0,0); -- spell 1311923, severity 2, cast seen at 77035 ms

-- DungeonEncounterID 3456: 6 casts from the first kill in 12.1.0.69587_altaroffangs1boss (79602 ms fight); 3 kill(s) / 3 pull(s) captured
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3456;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3456,0,797,8000,5000,0,0), -- spell 1296220, severity 2, cast seen at 8007 ms
(3456,1,795,25000,5000,0,0), -- spell 1309522, severity 2, cast seen at 25003 ms
(3456,2,797,47242,5000,0,0), -- spell 1296220, severity 2, cast seen at 47262 ms
(3456,3,798,52242,5000,0,0), -- spell 1296050, severity 2, cast seen at 52256 ms
(3456,4,899,62242,5000,0,0), -- spell 1307894, severity 2, cast seen at 62237 ms
(3456,5,797,71262,5000,0,0); -- spell 1296220, severity 2, cast seen at 71263 ms

-- DungeonEncounterID 3457: 7 casts from the first kill in altaroffangsfull_12.1.0.69587 (93690 ms fight); 1 kill(s) / 5 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3457;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3457,0,813,1000,5000,0,0), -- spell 1299154, severity 2, cast seen at 1002 ms
(3457,1,814,7000,5000,0,0), -- spell 1298949, severity 2, cast seen at 7015 ms
(3457,2,938,14000,5000,0,0), -- spell 1310357, severity 2, cast seen at 14002 ms
(3457,3,815,30000,5000,0,0), -- spell 1299940, severity 2, cast seen at 30017 ms
(3457,4,816,44000,5000,0,0), -- spell 1299053, severity 2, cast seen at 44016 ms
(3457,5,939,62817,5000,0,0), -- spell 1310547, severity 2, cast seen at 62821 ms
(3457,6,818,77817,5000,0,0); -- spell 1300686, severity 2, cast seen at 77830 ms

-- DungeonEncounterID 3458: 7 casts from the first kill in altaroffangsfull_12.1.0.69587 (68463 ms fight); 1 kill(s) / 1 pull(s) captured  -- REVIEW: single kill
DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3458;
INSERT INTO `instance_encounter_timeline` (`DungeonEncounterID`,`Index`,`EncounterEventID`,`Delay`,`Duration`,`IsApproximation`,`Flags`) VALUES
(3458,0,822,3000,5000,0,0), -- spell 1300876, severity 2, cast seen at 4082 ms
(3458,1,823,18000,5000,0,0), -- spell 1301111, severity 2, cast seen at 18008 ms
(3458,2,824,30000,5000,0,0), -- spell 1301350, severity 2, cast seen at 30011 ms
(3458,3,821,36000,5000,0,0), -- spell 1301413, severity 2, cast seen at 36034 ms
(3458,4,821,52034,5000,0,0), -- spell 1301413, severity 2, cast seen at 52027 ms
(3458,5,824,60011,5000,0,0), -- spell 1301350, severity 2, cast seen at 60002 ms
(3458,6,822,68028,5000,0,0); -- spell 1300876, severity 2, cast not seen
