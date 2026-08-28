--
-- Table structure for table `account_challenge_mode_history`
--
-- Backs SMSG_SET_INSTANCE_LEAVER / SMSG_UNSET_INSTANCE_LEAVER and C_InstanceLeaver.IsPlayerLeaver().
-- Modelled after the client side JAM type JamAccountChallengeModeHistory; only the columns this server
-- actually writes are present. This is NOT the classic LFG deserter aura (spell 71041) - the client keeps
-- both states apart and only this one gates the mythic+ entries in the group finder.
--
DROP TABLE IF EXISTS `account_challenge_mode_history`;
CREATE TABLE `account_challenge_mode_history` (
  `accountId` int unsigned NOT NULL,
  `totalLeaves` int unsigned NOT NULL DEFAULT '0',
  `leaverPenaltyExpiration` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'unix time the penalty expires, 0 = never flagged',
  PRIMARY KEY (`accountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
