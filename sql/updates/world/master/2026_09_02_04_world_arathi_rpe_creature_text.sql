-- CANDIDATE creature_text: lines the captures HEARD and the realm does not have.
-- Corroboration is by capture and is the confidence signal. Lines heard by 2+ independent
-- runs are emitted live; single-witness lines are emitted COMMENTED OUT so a reviewer opts
-- into them deliberately -- that bucket holds real content only one run happened to see.
-- REVIEW BEFORE APPLYING; golden-source rule: this belongs on feature/arathi-rpe.
-- 4 corroborated, 14 single-witness.

-- ---- CORROBORATED by 2+ captures ----
--   entry 249347 Fightbot Version 11.2.7 | 2 capture(s): alliance2,midnight
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (249347, 0, 0, 'PROGRAM INTERRUPTED! ANALYZING SCHEMA...', 12, 0, 100.0, 0, 0, 0);
--   entry 249347 Fightbot Version 11.2.7 | 2 capture(s): alliance2,midnight
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (249347, 0, 5, 'PROTOCOLS COMPLETE. RETURNING TO BASE STATE.', 12, 0, 100.0, 0, 0, 0);
--   entry 249347 Fightbot Version 11.2.7 | 2 capture(s): alliance2,midnight
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (249347, 0, 1, 'RESUMING DEBUG PROTOCOLS.', 12, 0, 100.0, 0, 0, 0);
--   entry 249456 Cindy Springstock | 2 capture(s): alliance2,midnight
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (249456, 0, 0, 'Are you here to help fix up this troublesome bot?', 12, 0, 100.0, 0, 0, 0);

-- ---- SINGLE WITNESS -- review and uncomment ----
--   entry 98066 Steven Nelson | 1 capture(s): alliance2
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (98066, 0, 0, 'That crazy guy... calls himself Mrgl-Mrgl. Says he was a king in Northrend. I don''''t believe him for a second.', 12, 0, 100.0, 0, 0, 0);
--   entry 244670 Gnoll Bowblaster | 1 capture(s): alliance2
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244670, 0, 0, 'Burn... all...', 12, 0, 100.0, 0, 0, 0);
--   entry 244670 Gnoll Bowblaster | 1 capture(s): midnight
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244670, 0, 0, 'Kill... all...', 12, 0, 100.0, 0, 0, 0);
--   entry 244671 Gnoll Ripper | 1 capture(s): alliance2
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244671, 0, 0, 'Burn... all...', 12, 0, 100.0, 0, 0, 0);
--   entry 244671 Gnoll Ripper | 1 capture(s): alliance2
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244671, 0, 1, 'This place ours now!', 12, 0, 100.0, 0, 0, 0);
--   entry 244672 Gnoll Bruiser | 1 capture(s): alliance2
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244672, 0, 2, 'Arathi... ours...', 12, 0, 100.0, 0, 0, 0);
--   entry 244672 Gnoll Bruiser | 1 capture(s): alliance2
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244672, 0, 0, 'More meat!', 12, 0, 100.0, 0, 0, 0);
--   entry 244674 Ogre Destroyer | 1 capture(s): midnight
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244674, 0, 2, 'No... stop... siege...', 12, 0, 100.0, 0, 0, 0);
--   entry 244674 Ogre Destroyer | 1 capture(s): midnight
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244674, 0, 1, 'Too late... to stop...', 12, 0, 100.0, 0, 0, 0);
--   entry 244674 Ogre Destroyer | 1 capture(s): midnight
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244674, 0, 0, 'We take food for siege!', 12, 0, 100.0, 0, 0, 0);
--   entry 244683 Gnoll Prowler | 1 capture(s): midnight
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244683, 0, 0, 'Arathi... ours...', 12, 0, 100.0, 0, 0, 0);
--   entry 244685 Ogre Basher | 1 capture(s): midnight
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (244685, 0, 0, 'No... fair...', 12, 0, 100.0, 0, 0, 0);
--   entry 249347 Fightbot Version 11.2.7 | 1 capture(s): alliance2
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (249347, 0, 0, 'INCORRECT USE CASE!', 12, 0, 100.0, 0, 0, 0);
--   entry 257072 Gnoll Biter | 1 capture(s): midnight
--   ^ SLOT ALREADY OCCUPIED with different text -- this is a CHANGE, not
--     a new line. Review the existing row before touching it.
-- INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`BroadcastTextId`) VALUES (257072, 0, 0, 'More meat!', 12, 0, 100.0, 0, 0, 0);

