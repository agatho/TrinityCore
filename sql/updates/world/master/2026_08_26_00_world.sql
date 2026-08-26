DELETE FROM `command` WHERE `name` IN ('debug send timeadjustment');
INSERT INTO `command` (`name`, `help`) VALUES
('debug send timeadjustment', 'Syntax: .debug send timeadjustment $timeScale\r\n\r\nSends SMSG_TIME_ADJUSTMENT with the given clock scale factor to your own client. The client applies the factor to its clock, logs "Time elapse scaled by %g to %g" and answers with CMSG_TIME_ADJUSTMENT_RESPONSE. The situation in which retail sends this is not known - this command is the only trigger.');
