-- Merithra's Blessing (1256577/1256682/1256689): Midnight's Preservation Evoker Apex Talent.
-- Rank 1 makes Essence spenders grant a buff (1256579) whose core-side action-bar override swaps the
-- next Reversion (366155) for Merithra's Blessing (1256581); rank 2 reverses a share of the damage
-- Reversion's target takes back into healing; rank 3 also grants the buff from Dream Breath.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_evo_dream_breath', 'spell_evo_merithras_blessing', 'spell_evo_merithras_blessing_talent', 'spell_evo_reversion');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(355936, 'spell_evo_dream_breath'),                 -- Dream Breath (rank 3 grant chance)
(382614, 'spell_evo_dream_breath'),                 -- Dream Breath (Font of Magic variant)
(1256581, 'spell_evo_merithras_blessing'),          -- Merithra's Blessing (bloom heal, consumes the buff)
(1256577, 'spell_evo_merithras_blessing_talent'),   -- Merithra's Blessing (rank 1 Essence-spender proc)
(366155, 'spell_evo_reversion');                    -- Reversion (rank 2 damage reversal absorb)
