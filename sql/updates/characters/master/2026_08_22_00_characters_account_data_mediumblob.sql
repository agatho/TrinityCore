--
-- Account data blobs: BLOB (max 65535 bytes) -> MEDIUMBLOB (max 16 MB).
--
-- HandleUpdateAccountData rejected any account-data blob over 0xFFFF ("UAD: Account data packet
-- too big"), matching the old BLOB column limit. Modern clients send larger per-account / per-character
-- config + addon SavedVariables (a tester hit 93155 bytes), so the client's data silently failed to
-- persist. The handler cap is raised to 0xFFFFFF in the same change; widen the storage columns to match.
--
ALTER TABLE `account_data`           MODIFY `data` mediumblob NOT NULL;
ALTER TABLE `character_account_data` MODIFY `data` mediumblob NOT NULL;
