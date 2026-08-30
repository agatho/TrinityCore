-- Demon Hunter Havoc invent implement (EVR-47 slice #2): 21 talents + Apex Eternal Hunt.
-- Build evidence: 12.0.7.67808 EvryDb2Export SpellEffect-pack-43 / pack-10 follow-ons.
-- Core-only / deferred: Burning Hatred 320374 (258922 energize), Eternal Hunt 1270901 (aura108),
-- Blind Fury E0/E1 + Desperate Instincts E2 + Eternal Hunt r2 E0/E1/E3 core mods,
-- A Fire Inside 427775 aura 220 → slice E. Fel Rush movement not reopened (cast-host readers only).

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_dh_inertia',
  'spell_dh_dash_of_chaos_override',
  'spell_dh_growing_inferno',
  'spell_dh_ragefire',
  'spell_dh_screaming_brutality',
  'spell_dh_initiative',
  'spell_dh_havoc_hunt_retreat_talents',
  'spell_dh_havoc_fel_rush_felblade_talents',
  'spell_dh_dash_of_chaos_back',
  'spell_dh_unbound_chaos_damage',
  'spell_dh_deflecting_dance',
  'spell_dh_screaming_brutality_cast',
  'spell_dh_glaive_tempest_talent',
  'spell_dh_screaming_brutality_slash',
  'spell_dh_throw_glaive_havoc_talents',
  'spell_dh_burning_wound_bite',
  'spell_dh_relentless_onslaught',
  'spell_dh_chaotic_disposition',
  'spell_dh_desperate_instincts',
  'spell_dh_eye_beam_havoc_talents',
  'spell_dh_eye_beam_damage_havoc',
  'spell_dh_isolated_prey_chaos_nova',
  'spell_dh_immolation_aura_tick_havoc',
  'spell_dh_ragefire_ia',
  'spell_dh_the_hunt_damage'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Talent auras (state / proc holders)
(427640, 'spell_dh_inertia'),
(427793, 'spell_dh_dash_of_chaos_override'),
(390158, 'spell_dh_growing_inferno'),
(388107, 'spell_dh_ragefire'),
(1220506, 'spell_dh_screaming_brutality'),
(388108, 'spell_dh_initiative'),
(428492, 'spell_dh_chaotic_disposition'),
(205411, 'spell_dh_desperate_instincts'),
-- Hunt / VR → Exergy / Unbound / Inertia / Eternal Hunt r1 / Initiative refresh
(370965, 'spell_dh_havoc_hunt_retreat_talents'),
(198793, 'spell_dh_havoc_hunt_retreat_talents'),
-- Fel Rush / Felblade cast hosts (no movement reopen)
(195072, 'spell_dh_havoc_fel_rush_felblade_talents'),
(232893, 'spell_dh_havoc_fel_rush_felblade_talents'),
(427785, 'spell_dh_dash_of_chaos_back'),
-- Unbound Chaos damage consumers
(192611, 'spell_dh_unbound_chaos_damage'),
(213243, 'spell_dh_unbound_chaos_damage'),
-- Blade Dance hosts
(188499, 'spell_dh_deflecting_dance'),
(210152, 'spell_dh_deflecting_dance'),
(188499, 'spell_dh_screaming_brutality_cast'),
(210152, 'spell_dh_screaming_brutality_cast'),
(200685, 'spell_dh_glaive_tempest_talent'),
(210155, 'spell_dh_glaive_tempest_talent'),
(199552, 'spell_dh_screaming_brutality_slash'),
(200685, 'spell_dh_screaming_brutality_slash'),
(210153, 'spell_dh_screaming_brutality_slash'),
(210155, 'spell_dh_screaming_brutality_slash'),
-- Throw Glaive / bites
(185123, 'spell_dh_throw_glaive_havoc_talents'),
(204157, 'spell_dh_throw_glaive_havoc_talents'),
(162243, 'spell_dh_burning_wound_bite'),
(203796, 'spell_dh_burning_wound_bite'),
-- Chaos Strike / Eye Beam / Chaos Nova / IA
(162794, 'spell_dh_relentless_onslaught'),
(198013, 'spell_dh_eye_beam_havoc_talents'),
(198030, 'spell_dh_eye_beam_damage_havoc'),
(179057, 'spell_dh_isolated_prey_chaos_nova'),
(258922, 'spell_dh_immolation_aura_tick_havoc'),
(258920, 'spell_dh_ragefire_ia'),
(370966, 'spell_dh_the_hunt_damage');

-- Initiative: deal/take combat damage
-- Chaotic Disposition: done harmful spell/ability/periodic (chaos school filtered in script)
DELETE FROM `spell_proc` WHERE `SpellId` IN (388108, 428492);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(388108,0x00,0,0x00000000,0x00000000,0x00000000,0x00000000,0x0003333C,0x0,0x0,0x2,0x0,0x0,0x0,0,100,0,0), -- Initiative deal+take hits
(428492,0x7C,0,0x00000000,0x00000000,0x00000000,0x00000000,0x00051000,0x0,0x1,0x2,0x0,0x0,0x0,0,100,0,0); -- Chaotic Disposition done magic/ability/periodic
