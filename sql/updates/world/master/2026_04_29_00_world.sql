--
-- 2026-04-29: housing front-door GO templates — set goober.spell (Data10) to 1271876
--
-- Sniff-decoded from C:\sniff\housing_stuff\{alliance,horde}_housing\
-- dump_12.0.5.67186_2026-04-28_*.pkt: when a housing door is clicked the 12.0.5 retail
-- client emits CMSG_CAST_SPELL with SpellID 1,271,876 targeting the door GO. That ID
-- is the new "Housing Door Open" spell (added in this commit's hotfix update). The
-- corresponding goober field on the door GO templates was either zero (575017,
-- 602702) or pointing at the older 12.0.1 spell 1234192 (586576, 602705) — neither
-- matches what the retail client casts, so server-side spell validation rejects the
-- cast (Spell::CheckSpellId or Spell::SendCastResult). Aligning Data10 with the
-- 12.0.5 spell makes the cast accept and run our SpellScript, which calls
-- GameObject::Use() on the target — invoking the existing go_housing_door::
-- OnGossipHello handler that performs the interior teleport.
--
-- Entries covered (all GAMEOBJECT_TYPE_GOOBER housing front doors):
--   575017  Interior Front Door             (Data10 was 0)
--   586576  Front Door (Founder's Point)    (Data10 was 1234192)
--   587318  Razorwind Shores Front Door     (Data10 was 1234193 — Razorwind variant)
--   602702  Front Door                       (Data10 was 0)
--   602705  Front Door                       (Data10 was 1234192)

UPDATE `gameobject_template` SET `Data10` = 1271876 WHERE `entry` IN (
    575017,
    586576,
    587318,
    602702,
    602705
);


--
-- Bind SpellID 1271876 to the spell_housing_door_open SpellScript so the dummy
-- effect actually invokes our handler. SpellScripts registered via
-- RegisterSpellScript() in C++ are matched to spells through this table.
--
DELETE FROM `spell_script_names` WHERE `spell_id` = 1271876 AND `ScriptName` = 'spell_housing_door_open';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (1271876, 'spell_housing_door_open');
