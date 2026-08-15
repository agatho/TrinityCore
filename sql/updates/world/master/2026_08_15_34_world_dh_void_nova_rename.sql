-- Demon Hunter Void Nova: apply the spell_dh_void_nova -> spell_dh_void_nova_softcap rename.
--
-- 2026_08_15_30_world_dh_devourer.sql already carries the renamed binding, but that file has
-- shipped, so this repeats it as a standalone update in case the updater does not re-run the
-- amended file. Both are plain DELETE + INSERT, so applying either or both is idempotent.
--
-- Rationale (full detail in the 2026_08_15_30 header): origin/feature/devourer-spec registers a
-- class also named spell_dh_void_nova in src/server/scripts/Spells/spell_dh_devourer.cpp. Two
-- classes with the same name in two translation units is an ODR violation on a static-scripts
-- build and a duplicate ScriptName registration at runtime, so ours is renamed to make the two
-- mergeable in either order. That branch's superseded ScriptNames are deleted here as well; when
-- it is merged its cpp, its spell_script_loader.cpp hook and its
-- 2026_08_12_00_world_devourer_spec.sql must be dropped, otherwise that SQL re-inserts them.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_dh_void_nova',
  'spell_dh_void_nova_softcap',
  'spell_dh_void_metamorphosis',
  'spell_dh_void_metamorphosis_drain'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1234195, 'spell_dh_void_nova_softcap');
