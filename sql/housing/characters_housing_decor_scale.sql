-- Add scale column to character_housing_decor for Advanced Mode decor scaling
-- Default 1.0 = original size. Sniff shows values like 0.45 to 1.62.
ALTER TABLE `character_housing_decor`
    ADD COLUMN `scale` FLOAT NOT NULL DEFAULT 1.0 AFTER `rotW`;
