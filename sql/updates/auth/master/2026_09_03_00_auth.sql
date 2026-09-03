--
-- Social Contract acceptance (CMSG_ACCEPT_SOCIAL_CONTRACT / CMSG_SOCIAL_CONTRACT_REQUEST).
-- Per-battlenet-account flag: once the account accepts the Social Contract at character select the
-- client must stop being prompted. The world server reads it to answer GetShouldShowSocialContract
-- and sets it on accept.
--
ALTER TABLE `battlenet_accounts`
  ADD COLUMN IF NOT EXISTS `social_contract_accepted` tinyint unsigned NOT NULL DEFAULT '0' AFTER `LoginTicketExpiry`;
