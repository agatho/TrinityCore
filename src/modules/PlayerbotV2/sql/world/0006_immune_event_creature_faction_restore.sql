-- ===========================================================================
-- World-DB correction: immune event-creature faction restore (14 -> 16)
-- Target DB: world (this server: wc_world).  Canonical ref: tc_world (retail TDB)
-- Date:      2026-07-22
-- ===========================================================================
--
-- ROOT CAUSE (systemic dungeon false-combat wedge):
--   Live wc_world corrupted creature_template.faction on IMMUNE_TO_PC "event"
--   creatures (dormant adds/gauntlet mobs that a boss/instance script activates).
--   For these entries the immune bit is CORRECT and identical to canonical
--   tc_world (unit_flags & 64 = UNIT_FLAG_IMMUNE_TO_PC, immune-until-scripted),
--   but wc_world set faction = 14 where tc_world (= retail) has faction = 16.
--
--   FactionTemplate 14 is the universal "Monster" template: EnemyGroup = 15
--   (Player|Alliance|Horde|Monster) -> IsHostileToPlayers() = true. So the
--   immune, UN-damageable creature AGGROES bots while they can never damage it
--   -> permanent InCombat with nothing killable -> the bot never leaves combat:
--       State: InCombat, Combat duration climbs unbounded,
--       Dungeon: encounter=idle boss=0,
--       RuleHist loops dungeon:false_combat_escape <-> idle:dungeon_combat_wedge_advance_hold.
--   FactionTemplate 16 (retail) has EnemyGroup = 0 -> IsHostileToPlayers() = false:
--   the dormant creature does NOT pull players/bots until its script activates it
--   (the script sets the aggressive faction + clears immune at activation).
--   Observed live: Ragefire Chasm (map 389) wedged at 2/4, tank stuck 7+ min at
--   (-113.8, 65.2, -19.4) on 61412 Dark Shaman Koranthal (faction 14, immune).
--
-- FIX (surgical, canonical-anchored — NOT a blanket wc<-tc copy):
--   Restore ONLY the faction field, ONLY on rows carrying the exact corruption
--   signature verified against tc_world:
--       (wc.unit_flags & 64) AND (tc.unit_flags & 64)   -- immune in BOTH
--       AND wc.faction = 14 AND tc.faction = 16          -- faction diverged 14->16
--   335 creature_template entries match (fleet-wide: 29 maps incl. RFC's 10).
--   Setting faction = 16 restores each entry to its exact retail value, so the
--   existing TC C++ encounter scripts (written against faction-16 canonical data)
--   continue to work; no encounter is neutralized or altered. The 4 entries where
--   tc is NOT immune (1850 Putridius, 52582 Volcano, 52866 Crater, 56762 Yu'lon)
--   are DELIBERATELY EXCLUDED — different signature, out of scope.
--
--   The IN-list is materialized (no tc_world join) so this applies on any host.
--   The WHERE guard makes it idempotent and self-protecting: re-runs are no-ops,
--   and it will not touch an entry that has lost the immune flag or already = 16.
--
-- Reversible: UPDATE creature_template SET faction = 14 WHERE entry IN (<list>).
-- After apply: server console  .reload creature_template   (or restart).

UPDATE `creature_template`
   SET `faction` = 16
 WHERE `faction` = 14
   AND (`unit_flags` & 64) <> 0
   AND `entry` IN (
  1837, 1838, 1839, 1848, 2452, 2598, 2600, 3398, 5435, 5822,
  5865, 5928, 5932, 6648, 7104, 8660, 10200, 10357, 10641, 10741,
  10821, 17429, 17622, 20910, 21618, 30025, 32235, 32357, 32551, 32554,
  39612, 45260, 52076, 52311, 52340, 52380, 52381, 56439, 56732, 59544,
  59545, 59546, 59552, 59553, 60564, 61216, 61239, 61240, 61242, 61243,
  61303, 61337, 61338, 61339, 61340, 61387, 61389, 61392, 61408, 61412,
  61528, 61634, 61644, 61657, 61658, 61666, 61672, 61678, 61705, 61818,
  61946, 61947, 62795, 64061, 64063, 64068, 64203, 64947, 65362, 65402,
  65872, 65873, 69142, 94694, 97097, 97182, 97200, 97365, 98919, 99033,
  99188, 102375, 114712, 114714, 115115, 118175, 118176, 119000, 121504, 124927,
  126095, 126181, 128652, 129208, 130338, 131410, 131577, 132746, 132892, 132919,
  132921, 134331, 134992, 135043, 135044, 135646, 135649, 135830, 135834, 135837,
  135838, 135839, 135845, 135892, 135893, 135930, 135931, 135932, 135996, 136003,
  136011, 136012, 136043, 136049, 136051, 136797, 136798, 136810, 136812, 136819,
  136835, 136838, 136846, 136848, 136849, 136859, 136861, 136864, 136869, 136870,
  136878, 136991, 137058, 137059, 137060, 137061, 137062, 137177, 138428, 138429,
  138433, 138434, 138436, 138438, 138441, 138444, 138445, 138447, 138483, 138486,
  138495, 138497, 138498, 138502, 138503, 138504, 138505, 138506, 138509, 138510,
  138511, 138515, 138569, 138625, 138627, 138628, 138630, 138631, 138632, 138635,
  138647, 138650, 138653, 138822, 138827, 138829, 138837, 138839, 138840, 138842,
  138844, 138845, 138846, 138847, 138848, 138849, 138890, 138970, 138971, 138999,
  139002, 139003, 139091, 139219, 139337, 139344, 139345, 139352, 139354, 139386,
  139387, 139390, 139404, 139406, 139414, 139417, 139420, 139421, 139429, 139432,
  139438, 139439, 139442, 139443, 139444, 139462, 139463, 139466, 139469, 139470,
  139473, 139474, 139480, 139486, 139529, 139537, 139539, 139586, 139587, 139592,
  139651, 139657, 139661, 139666, 139671, 139675, 139676, 139691, 139692, 139698,
  139700, 139751, 139758, 139806, 139809, 139814, 139817, 139818, 139869, 140064,
  140070, 140089, 140093, 140102, 140158, 140266, 140269, 140298, 140301, 140338,
  140341, 140344, 140345, 140377, 140378, 140450, 140541, 140657, 140663, 140670,
  140829, 140844, 140854, 140863, 147222, 156918, 158930, 159112, 159113, 159151,
  159152, 159503, 160393, 160436, 162047, 164737, 166993, 167189, 167526, 170750,
  172715, 176123, 186962, 196044, 196045, 196102, 196115, 196117, 196198, 196202,
  196203, 196576, 196577, 196671, 196694, 196798, 197219, 197398, 197406, 197697,
  197698, 197904, 197905, 199614, 200113
);
