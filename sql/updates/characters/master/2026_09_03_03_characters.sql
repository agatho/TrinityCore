--
-- CMSG_SET_PET_FAVORITE: persist the stable "favorite" star per pet.
-- Mirrored at runtime into ActivePlayerData.PetStable.Pets[].PetFlags (PET_STABLE_FAVORITE).
--
ALTER TABLE `character_pet`
  ADD COLUMN `favorite` TINYINT UNSIGNED NOT NULL DEFAULT '0' AFTER `specialization`;
