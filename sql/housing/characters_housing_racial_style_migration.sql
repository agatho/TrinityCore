-- Migrate existing houses to racial WMO styles.
-- Previously all Alliance houses got WMO=9 (Human) and Horde got WMO=87 (Orc).
-- Now: Night Elf → 55 (Woodland), Blood Elf → 56 (Engraved).

-- Night Elf (race=4): Alliance default 9 → Woodland 55
UPDATE character_housing ch
INNER JOIN characters c ON ch.guid = c.guid
SET ch.houseType = 55
WHERE c.race = 4 AND ch.houseType = 9;

-- Blood Elf (race=10): Horde default 87 → Engraved 56
UPDATE character_housing ch
INNER JOIN characters c ON ch.guid = c.guid
SET ch.houseType = 56
WHERE c.race = 10 AND ch.houseType = 87;
