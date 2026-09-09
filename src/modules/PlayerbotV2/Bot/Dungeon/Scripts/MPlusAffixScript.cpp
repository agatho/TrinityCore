// MPlusAffixScript — global DungeonScript that returns advice for every
// common Mythic+ affix, regardless of which affixes are active in the
// current key. The advice fields reference creature entries / spell ids
// that only exist when their affix is active, so consuming idle rules
// react conditionally (no detection of affix-state needed).
//
// Coverage (Dragonflight / The War Within retail data):
//   Sanguine        — pool entry 150640, dangerous aura 226489
//   Spiteful        — shade entry 174773 (kited by DPS)
//   Volcanic        — debuff aura 209862 (move out)
//   Quaking         — debuff aura 240447 (spread)
//   Bursting        — stacking debuff 240443 (dispel priority)
//   Bolstering      — buff aura 209859 (pull separately)
//   Raging          — buff aura 228318 (CC / focus-fire)
//   Necrotic        — tank debuff 209858 (tank swap)
//   Storming        — moving AoE entry 196492 (move out / debuff 343520)
//
// New affixes get appended as Blizzard ships them; entries / spell ids
// come from Wago / WoWHead. The script is registered globally at module
// init via DungeonScriptMgr::RegisterGlobal.

#include "../DungeonScript.h"

namespace Playerbot {

class MPlusAffixScript : public DungeonScript
{
public:
    uint32_t map_id()          const override { return 0; }   // global
    char const* name()         const override { return "mplus_affixes"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Sanguine: step out of blood pools (dangerous_auras path).
        a.dangerous_auras.push_back(226489u);
        // Spiteful: kite the shade.
        a.kite_creature_entries.push_back(174773u);
        // Volcanic: move out when targeted.
        a.dangerous_auras.push_back(209862u);
        // Quaking: spread from allies when about to detonate.
        a.spread_on_self_auras.push_back(240447u);
        // Bursting: dispel-priority on the stacking debuff. Bursting is
        // a STACKING DEBUFF on the GROUP (each death applies a stack),
        // dispellable by Magic dispels — this is the correct consumer
        // (priority_dispel_candidate searches group members carrying it).
        a.dispel_priority_spells.push_back(240443u);
        // Bolstering (209859) — enemy buff stacking on ally death. Wired
        // via `pull_separately_auras` (added 2026-05-14): tank-pull rule
        // scans each candidate's NearbyUnit.affix_buffs (builder samples
        // a small whitelist incl. 209859) and refuses to pull a flagged
        // mob whose unique siblings live within 15y. Net effect: tank
        // single-pulls Bolstered packs, keeping the bolster stack at 1.
        a.pull_separately_auras.push_back(209859u);
        // Raging (228318) — enemy enrage buff. Wired via
        // `dispel_enemy_priority_spells` (added 2026-05-14): class APLs
        // for Hunter/Druid/Mage check the intersection of this list
        // against each nearby enemy's affix_buffs and dispatch the
        // appropriate dispel (Tranq Shot 19801, Soothe 2908, Spellsteal
        // 30449). Generic damage still works as fallback for classes
        // without an enrage dispel.
        a.dispel_enemy_priority_spells.push_back(228318u);
        // Necrotic: tank swap on stacks (tank_swap_on_spells path).
        a.tank_swap_on_spells.push_back(209858u);
        // Storming: AoE moving zone — treat as dangerous aura.
        a.dangerous_auras.push_back(343520u);
        return a;
    }
};

// Factory consumed by Services.cpp registry init. RegisterGlobal so the
// advice is merged into EVERY dungeon's per-tick advice — affix detection
// is implicit (the affix's creature entries / spell ids only exist when
// the affix is active, so unaffected pulls see no effect).
std::unique_ptr<DungeonScript> MakeMPlusAffixScript()
{
    return std::make_unique<MPlusAffixScript>();
}

} // namespace Playerbot
