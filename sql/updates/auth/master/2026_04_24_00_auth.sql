-- Housing branch: allow client build 12.0.5.67186
--
-- Upstream TrinityCore added allowed build 12.0.5.67114 in commit d551e5b3
-- (2026_04_23_00_auth.sql). The Midnight retail build progressed to 67186
-- before stabilising; this patch lets our housing branch accept the same
-- client by adding 67186 to build_info and seeding a build_auth_key row
-- per platform/arch/type combination.
--
-- NOTE ON AUTH KEYS: the 16-byte HMAC keys in build_auth_key are embedded
-- in each released WoW client (one per Platform/Arch/Type variant). Without
-- the real 67186 keys the HMAC check in WorldSocket::HandleAuthSessionCallback
-- will reject the client with ERROR_DENIED. As a dev stopgap this patch seeds
-- the 67186 rows with the 67114 keys Shauren published so the build-info /
-- auth-key *lookup* path passes; the HMAC step will still fail unless you
-- replace these with actual 67186 keys extracted from your Wow[C].exe.
-- To extract: open the 67186 client binary in IDA, find the 16-byte AuthKey
-- literal referenced by the client's HMAC join function, and overwrite the
-- matching row's `key` column.

DELETE FROM `build_info` WHERE `build` IN (67186);
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(67186,12,0,5,NULL);

DELETE FROM `build_auth_key` WHERE `build`=67186 AND `platform`='Mac' AND `arch`='A64' AND `type`='WoW';
DELETE FROM `build_auth_key` WHERE `build`=67186 AND `platform`='Mac' AND `arch`='A64' AND `type`='WoWC';
DELETE FROM `build_auth_key` WHERE `build`=67186 AND `platform`='Mac' AND `arch`='x64' AND `type`='WoW';
DELETE FROM `build_auth_key` WHERE `build`=67186 AND `platform`='Mac' AND `arch`='x64' AND `type`='WoWC';
DELETE FROM `build_auth_key` WHERE `build`=67186 AND `platform`='Win' AND `arch`='A64' AND `type`='WoW';
DELETE FROM `build_auth_key` WHERE `build`=67186 AND `platform`='Win' AND `arch`='x64' AND `type`='WoW';
DELETE FROM `build_auth_key` WHERE `build`=67186 AND `platform`='Win' AND `arch`='x64' AND `type`='WoWC';
-- TODO: replace these placeholder keys with the real 67186 values once extracted.
-- Keys below are 67114 values (from Shauren's 2026_04_23_00_auth.sql) duplicated
-- so the server has *some* entry to match against — HMAC verification will fail
-- until they are updated.
INSERT INTO `build_auth_key` (`build`,`platform`,`arch`,`type`,`key`) VALUES
(67186,'Mac','A64','WoW',0x1A1662F9D15F926D6A85DF91BCE79DF1),
(67186,'Mac','A64','WoWC',0xD28D8D2C35FA6FE4C3C0D76894431F46),
(67186,'Mac','x64','WoW',0x59407231B082F7587562AC40C6992613),
(67186,'Mac','x64','WoWC',0x02E25F13D33F7486F2267D23D527D0FE),
(67186,'Win','A64','WoW',0x275017EC612F2E13305A0FA0C16A5FA8),
(67186,'Win','x64','WoW',0x43F598C4E67C9D2F644A560369C0DF41),
(67186,'Win','x64','WoWC',0x1F88AE965BD17EE37189BD736C5F7D7B);

UPDATE `realmlist` SET `gamebuild`=67186 WHERE `gamebuild`=67114;

ALTER TABLE `realmlist` CHANGE `gamebuild` `gamebuild` int unsigned NOT NULL DEFAULT '67186';
