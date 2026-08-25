// Apl_Baseline.cpp — registers per-class baseline rotations
// (spec=0) for L1-9 pre-spec characters and bots with corrupt /
// missing ChrSpecialization. Per-class rule sets live in the
// sibling Apl_Baseline_<Class>.cpp files; this file is now just
// the aggregator that wires the 13 per-class registration calls
// into a single entry point that ApRegistry::RegisterAllRotations
// invokes at module init.
//
// See Apl_Baseline_Common.h for shared helpers + rule macros, and
// src/modules/PlayerbotV2/tools/baseline_coverage_audit.py for the
// wago.tools-driven coverage audit.

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

void RegisterApl_Baseline()
{
    RegisterApl_Baseline_Warrior();
    RegisterApl_Baseline_Paladin();
    RegisterApl_Baseline_Hunter();
    RegisterApl_Baseline_Rogue();
    RegisterApl_Baseline_Priest();
    RegisterApl_Baseline_DK();
    RegisterApl_Baseline_Shaman();
    RegisterApl_Baseline_Mage();
    RegisterApl_Baseline_Warlock();
    RegisterApl_Baseline_Monk();
    RegisterApl_Baseline_Druid();
    RegisterApl_Baseline_DH();
    RegisterApl_Baseline_Evoker();
}

} // namespace Playerbot::Combat
