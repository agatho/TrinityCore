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
DROP TABLE IF EXISTS `chat_spam_record`;
CREATE TABLE `chat_spam_record` (
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
