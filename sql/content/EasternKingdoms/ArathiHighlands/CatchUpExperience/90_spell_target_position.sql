-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up / RPE :: spell_target_position for the launch teleport
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server map 2927 (Arathi Highlands RPE, InstanceType=0 normal phased map).
-- Spell 1260320 "Teleport to Arathi Highlands" = Effect 252 (SPELL_EFFECT_TELEPORT_UNITS),
--   EffectIndex 0, ImplicitTarget_1 = 17 (TARGET_DEST_DB) -> destination read from THIS table
--   (server-side; no client DB2). Facing 6.2584 from wago SpellEffect EffectPos_facing.
--   Destination coords = the RE'd/captured landing pad (matches feature/arathi-rpe's hardcoded
--   login-relocate pos -1101.67,-3554.37,48.9203). Source: wago SpellEffect SpellID=1260320.
-- This makes the client-cast / in-game launch spell teleport correctly (the feature/arathi-rpe
--   LOGIN path relocates directly; the in-game tile-launch spell path needs this row).
-- CANDIDATE ONLY -- review before applying. Idempotent upsert on PK (ID,EffectIndex,OrderIndex).
-- ============================================================================
INSERT INTO `spell_target_position` (`ID`, `EffectIndex`, `OrderIndex`, `MapID`, `PositionX`, `PositionY`, `PositionZ`, `Orientation`, `VerifiedBuild`) VALUES
 (1260320, 0, 0, 2927, -1101.67, -3554.37, 48.9203, 6.2584, 69404)
ON DUPLICATE KEY UPDATE `MapID`=VALUES(`MapID`), `PositionX`=VALUES(`PositionX`), `PositionY`=VALUES(`PositionY`), `PositionZ`=VALUES(`PositionZ`), `Orientation`=VALUES(`Orientation`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
