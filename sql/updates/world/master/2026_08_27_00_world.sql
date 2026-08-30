DELETE FROM `command` WHERE `name` IN ('debug send markremotetimeinvalid');
INSERT INTO `command` (`name`, `help`) VALUES
('debug send markremotetimeinvalid', 'Syntax: .debug send markremotetimeinvalid\r\n\r\nSends SMSG_MOVE_MARK_REMOTE_TIME_INVALID for the selected unit to everybody who observes it. The observing clients drop the time base they hold for that mover and rebuild it from the next movement update; the client controlling the mover is skipped, so select another unit rather than yourself. The situation in which retail sends this is not known - this command is the only trigger.');
