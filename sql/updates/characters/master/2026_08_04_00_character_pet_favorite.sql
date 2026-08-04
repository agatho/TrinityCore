-- Stable pet favorites (CMSG_SET_PET_FAVORITE). Kept in a dedicated table because character_pet rows are
-- deleted+reinserted on every pet save, which would reset a favorite column; keyed by the pet's persistent
-- pet number so it survives summon/stable/save cycles.
CREATE TABLE IF NOT EXISTS `character_pet_favorite` (
  `owner`     BIGINT UNSIGNED NOT NULL,
  `petNumber` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`owner`, `petNumber`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Stable pets pinned as favorite (star) in the pet stable UI';
