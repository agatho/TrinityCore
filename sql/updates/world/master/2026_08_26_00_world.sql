--
-- Familie 0x67 (Zauber & Kampflog), Phase A - Nacharbeit.
-- '.cheat status' listet bisher CHEAT_GOD, CHEAT_COOLDOWN, CHEAT_CASTTIME, CHEAT_POWER,
-- CHEAT_WATERWALK und den Taxi-Schalter. Der in dieser Einheit hinzugekommene sechste Schalter
-- CHEAT_IGNORE_DIMINISHING_RETURNS (Player.h, SMSG_CHEAT_IGNORE_DIMISHING_RETURNS 0x670002) fehlte
-- dort; ein GM konnte nicht ablesen, ob die DR-Umgehung fuer ihn an ist.
-- Die Zeichenkette traegt genau ein %s, wie die fuenf vorhandenen Schalterzeilen (Eintraege
-- 358-362, 364), und wird von cs_cheat.cpp HandleCheatStatusCommand ueber LANG_COMMAND_CHEAT_DR
-- ausgegeben.
--
DELETE FROM `trinity_string` WHERE `entry`=11022;
INSERT INTO `trinity_string` (`entry`,`content_default`) VALUES
(11022,'Ignore Diminishing Returns is %s');
