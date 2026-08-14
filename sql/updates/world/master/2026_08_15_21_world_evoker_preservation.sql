-- Evoker Preservation implement (EVR-37 slice #3 / EVR-42): 24 dummy talent nodes.
-- Build evidence: 12.0.7.67808 EvryDb2Export SpellEffect-pack-24 + Spell-pack-24-pres + follow-ons.
-- Hosts extended: Living Flame, Reversion, Emerald Blossom, Verdant Embrace, Dream Breath,
-- Fire Breath empower path, Dream Flight. Echo 364343 / Stasis 370537+370564 / Temporal Anomaly
-- / Temporal Barrier / Time Dilation / Lifebind / Time of Need trigger are new bindings.
-- Passiveives Empath / Flow State / Twin Echoes / Ouroboros / Field of Dreams / Fluttering Seedlings /
-- Temporal Compression / Spark of Insight / Exhilarating Burst / Titan's Gift / Nozdormu's Teachings /
-- Delay Harm / Inner Flame / Lifespark / Golden Hour / Dream Simulacrum are host-hooked (HasAura).

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_evo_echo',
  'spell_evo_echo_effectiveness',
  'spell_evo_preservation_essence_burst_living_flame',
  'spell_evo_preservation_essence_burst_reversion',
  'spell_evo_emerald_blossom_cast',
  'spell_evo_emerald_blossom_preservation',
  'spell_evo_reversion_cast',
  'spell_evo_stasis',
  'spell_evo_stasis_release',
  'spell_evo_temporal_anomaly',
  'spell_evo_temporal_barrier',
  'spell_evo_time_dilation_cast',
  'spell_evo_lifebind',
  'spell_evo_time_of_need_trigger',
  'spell_evo_dream_simulacrum_heal',
  'spell_evo_dream_flight_inner_flame',
  'spell_evo_titans_gift_damage'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(364343, 'spell_evo_echo'),
(361509, 'spell_evo_echo_effectiveness'),
(361195, 'spell_evo_echo_effectiveness'),
(355916, 'spell_evo_echo_effectiveness'),
(361469, 'spell_evo_preservation_essence_burst_living_flame'),
(366155, 'spell_evo_preservation_essence_burst_reversion'),
(355913, 'spell_evo_emerald_blossom_cast'),
(355916, 'spell_evo_emerald_blossom_preservation'),
(366155, 'spell_evo_reversion_cast'),
(370537, 'spell_evo_stasis'),
(370564, 'spell_evo_stasis_release'),
(373861, 'spell_evo_temporal_anomaly'),
(1291636, 'spell_evo_temporal_barrier'),
(357170, 'spell_evo_time_dilation_cast'),
(373267, 'spell_evo_lifebind'),
(368435, 'spell_evo_time_of_need_trigger'),
(361195, 'spell_evo_dream_simulacrum_heal'),
(359816, 'spell_evo_dream_flight_inner_flame'),
(361500, 'spell_evo_titans_gift_damage');

-- Lifebind: proc from caster healing while the bond aura is active (DB2 ProcTypeMask on 373267).
DELETE FROM `spell_proc` WHERE `SpellId` IN (373267);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(373267,0x00,0,0x00000000,0x00000000,0x00000000,0x00000000,0x4000,0x0,0x2,0x2,0x0,0x0,0x0,0,100,0,0); -- DONE_SPELL_MAGIC_DMG_CLASS_POS-ish heal done; SpellTypeMask heal
