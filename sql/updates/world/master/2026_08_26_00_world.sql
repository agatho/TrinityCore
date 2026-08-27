--
-- chat_2C_4A: the spam pattern table behind SMSG_EXPECTED_SPAM_RECORDS (0x4A0005) and the server
-- side cautionary chat check (SMSG_CAUTIONARY_CHAT_MESSAGE, 0x4A0008).
--
-- Text length: the wire carries the pattern length in 9 bits and the client reads it into a
-- 512 byte JamClientSpamRecord element to which it appends its own null terminator, so 511 bytes is
-- the hard ceiling (element reader 0x72F170, build 12.1.0.69382).
--
-- Seed suggestion: SpamMessages.db2 has 135 goldseller patterns in 12.1.0.69382. This tree does not
-- load that DB2, and the useful set is realm specific anyway, which is why the data lives here.
--
-- CREATE TABLE IF NOT EXISTS, deliberately NOT `DROP TABLE` + `CREATE TABLE`. UpdateFetcher
-- reapplies an update file of its own accord once its hash changes
-- (src/server/database/Updater/UpdateFetcher.cpp:276-279, "Reapplying update ... (it changed)"),
-- and this file has already been edited in place once. A DROP here would silently wipe the realm's
-- own pattern list on the next startup - the only data source for both SMSG_EXPECTED_SPAM_RECORDS
-- and the server side cautionary check. The table carries realm data, not TDB structure, so it is
-- created once and never redefined from here; a later column change needs its own ALTER TABLE
-- update file.
CREATE TABLE IF NOT EXISTS `chat_spam_record` (
  `ID` int unsigned NOT NULL,
  `Text` varchar(511) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- .reload chat_spam_record
-- The `command` table has only `name` and `help`; the RBAC permission is bound in code
-- (RBAC_PERM_COMMAND_RELOAD_CHAT_SPAM_RECORD = 1001, inserted by the matching auth update).
--
DELETE FROM `command` WHERE `name` = 'reload chat_spam_record';
INSERT INTO `command` (`name`, `help`) VALUES
('reload chat_spam_record', 'Syntax: .reload chat_spam_record\r\nReload the chat_spam_record table. Clients that are already logged in keep the pattern list they were sent at login, because SMSG_EXPECTED_SPAM_RECORDS is a login-once packet.');
