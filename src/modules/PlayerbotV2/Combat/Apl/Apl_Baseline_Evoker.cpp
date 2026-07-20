// Apl_Baseline_Evoker.cpp — baseline rotation for class CLASS_EVOKER (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
// here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

constexpr uint32 LIVING_FLAME    = 361469;
constexpr uint32 AZURE_STRIKE    = 362969;
constexpr uint32 DISINTEGRATE    = 356995;
constexpr uint32 FIRE_BREATH     = 382266;
constexpr uint32 ETERNITY_SURGE  = 359073;
constexpr uint32 PYRE            = 357211;
constexpr uint32 QUELL           = 351338;
constexpr uint32 RENEWING_BLAZE  = 374348;
// Obsidian Scales — Evoker 30% damage-reduction defensive. The legacy
// constant 235450 actually resolves to Mage Prismatic Barrier in
// SpellName.csv 12.0 (verified 2026-05-27 spec audit). Correct
// Evoker Obsidian Scales is 363916.
constexpr uint32 OBSIDIAN_SCALES = 363916;

BASELINE_SPELL_RULE(LivingFlame,   LIVING_FLAME)
BASELINE_SPELL_RULE(AzureStrike,   AZURE_STRIKE)
BASELINE_SPELL_RULE(Disintegrate,  DISINTEGRATE)
BASELINE_SPELL_RULE(FireBreath,    FIRE_BREATH)
BASELINE_SPELL_RULE(EternitySurge, ETERNITY_SURGE)
BASELINE_SPELL_RULE(Pyre,          PYRE)
BASELINE_INTERRUPT_RULE(Quell,     QUELL)

BASELINE_DEFENSIVE_RULE(RenewingBlaze,  RENEWING_BLAZE,  50)
BASELINE_DEFENSIVE_RULE(ObsidianScales, OBSIDIAN_SCALES, 40)

ApRule const baseline_evoker_kRules[] = {
    { ShouldObsidianScales,DoObsidianScales,"Obsidian Scales (<40%)"  },
    { ShouldRenewingBlaze, DoRenewingBlaze, "Renewing Blaze (<50%)"   },
    { ShouldQuell,         DoQuell,         "Quell (interrupt)"       },
    { ShouldFireBreath,    DoFireBreath,    "Fire Breath (empower)"   },
    { ShouldEternitySurge, DoEternitySurge, "Eternity Surge"          },
    { ShouldDisintegrate,  DoDisintegrate,  "Disintegrate (channel)"  },
    { ShouldPyre,          DoPyre,          "Pyre (AoE)"              },
    { ShouldAzureStrike,   DoAzureStrike,   "Azure Strike"            },
    { ShouldLivingFlame,   DoLivingFlame,   "Living Flame (filler)"   },
    { AlwaysInCombat,      DoAutoAttack,    "Auto attack"             },
};

} // anonymous

void RegisterApl_Baseline_Evoker()
{
    RegisterRotation(CLASS_EVOKER, 0, ApRotation{baseline_evoker_kRules});
}

} // namespace Playerbot::Combat
