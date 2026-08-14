-- Demon Hunter Devourer + Apex Midnight invent implement (EVR-47 slice #6): 23 seeds + Apex.
--
-- !! COLLIDES WITH origin/feature/devourer-spec - MUST BE RECONCILED BEFORE BOTH ARE MERGED !!
-- That branch adds src/server/scripts/Spells/spell_dh_devourer.cpp and
-- sql/updates/world/master/2026_08_12_00_world_devourer_spec.sql, which cover the same three
-- spells from the other direction:
--   1234195 Void Nova            - BOTH register the ScriptName 'spell_dh_void_nova' (duplicate
--                                  ScriptName -> ScriptMgr rejects one at load; the two classes
--                                  also share a name across translation units, which is an ODR
--                                  violation on a static-scripts build). Their handler is
--                                  EFFECT_1 SCHOOL_DAMAGE softcap, the other branch's is
--                                  EFFECT_2 DUMMY.
--   1217605 Void Metamorphosis   - here 'spell_dh_void_metamorphosis_cast',
--                                  there 'spell_dh_void_metamorphosis'
--   1217607 Void Metamorphosis   - here 'spell_dh_void_metamorphosis_devourer',
--                                  there 'spell_dh_void_metamorphosis_drain'
-- The 1217605/1217607 pairs have distinct ScriptNames, so neither branch's DELETE clears the
-- other's row: both scripts would bind and both would run on the same cast.
-- Nothing is renamed here on purpose - the owning branches have to decide which implementation
-- wins rather than having one silently reshaped by the port.
-- Build evidence: 12.0.7.67808 EvryDb2Export temp/db2/12.0.7.67808/SpellEffect-pack-41/9/11/13.csv
-- + SpellAuraOptions-devourer.csv (1225789 Cumul=50, 1227702 Cumul=40, 1232310/1244235/1242504).
-- Confirm before re-script: Eradicate 1226033 softcap core via SpellMgr; upgrade via 1239524.
-- Midnight 1250088 mostly aura108 core + E2 soft crit amp; 1250094 scripted on Meta cast;
-- 1242486 core-only aura107 CritChance. Void Nova stun host Focused Ire pre-bound.
-- Soft: Entropy OOC drip on combat tick; Emptiness haste CalcAmount; Waste Not chance scale;
-- Voidrush drain half; Soulshaper uses fragment counter proxy; Midnight E2 crit product.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_dh_consume_soul_devourer',
  'spell_dh_void_metamorphosis_cast',
  'spell_dh_void_metamorphosis_devourer',
  'spell_dh_collapsing_star',
  'spell_dh_collapsing_star_damage',
  'spell_dh_soul_immolation',
  'spell_dh_spontaneous_immolation',
  'spell_dh_entropy',
  'spell_dh_reap_devourer_talents',
  'spell_dh_reap_damage_devourer',
  'spell_dh_eradicate_void_ray',
  'spell_dh_void_ray_damage_devourer',
  'spell_dh_devourer_voidblade_hunt_talents',
  'spell_dh_hungering_slash',
  'spell_dh_hungering_slash_damage',
  'spell_dh_emptiness_haste',
  'spell_dh_void_nova'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Keystones / access
(1223423, 'spell_dh_consume_soul_devourer'),
(1217605, 'spell_dh_void_metamorphosis_cast'),
(1217607, 'spell_dh_void_metamorphosis_devourer'),
(1221150, 'spell_dh_collapsing_star'),
(1221162, 'spell_dh_collapsing_star_damage'),
-- Soul Immolation / Entropy / Spontaneous
(1241937, 'spell_dh_soul_immolation'),
(1246556, 'spell_dh_spontaneous_immolation'),
(1261684, 'spell_dh_entropy'),
-- Reap / Eradicate / Void Ray
(1226019, 'spell_dh_reap_devourer_talents'),
(1225823, 'spell_dh_reap_damage_devourer'),
(473728, 'spell_dh_eradicate_void_ray'),
(1213649, 'spell_dh_void_ray_damage_devourer'),
-- Voidblade / Hunt / Hungering
(1245414, 'spell_dh_devourer_voidblade_hunt_talents'),
(1246169, 'spell_dh_devourer_voidblade_hunt_talents'),
(1239123, 'spell_dh_hungering_slash'),
(1239127, 'spell_dh_hungering_slash_damage'),
-- Emptiness haste stacks / Void Nova softcap
(1242504, 'spell_dh_emptiness_haste'),
(1234195, 'spell_dh_void_nova');
