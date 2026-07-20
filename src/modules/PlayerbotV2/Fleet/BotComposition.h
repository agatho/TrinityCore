// BotComposition - Random (race, class, gender, name) picker producing
// approximately Blizzlike fleet distributions.
//
// Weights live as static tables in BotComposition.cpp (faction split ~50/50,
// per-race-within-faction weights from public WoW census data, per-class
// weights). Race/class compatibility is enforced at sample time by re-rolling
// the class against the subset valid for the chosen race (cheap: averages
// <5 retries because most race/class combos are valid).
//
// Names are syllable-generated to avoid dependency on a curated name list.
// Quality is "passable for testing", not "shippable lore-faithful".

#pragma once

#include <cstdint>
#include <string>

namespace Playerbot::V2 {

class BotComposition
{
public:
    struct Pick
    {
        uint8       race    = 0;
        uint8       cls     = 0;
        uint8       gender  = 0;
        std::string name;     // pre-validated unique against sCharacterCache
    };

    // Roll a fully-resolved (race, class, gender, name) tuple. Retries
    // internally on (a) invalid race/class combos (no PlayerInfo) and
    // (b) name collisions in sCharacterCache. Returns a Pick with race=0
    // only on catastrophic failure (very unlikely; logs).
    //
    // Hints: any non-zero (or non-empty for name) is treated as fixed and
    // the rest are rolled around it. If `race` is hinted, the class is
    // rolled from the legal subset for that race. If `cls` is hinted, the
    // race is rolled from races that support that class.
    static Pick Roll(uint8 race_hint = 0, uint8 cls_hint = 0,
                     uint8 gender_hint = 0xFF, std::string name_hint = "");

    // Roll a name only — useful when the caller already has race/class.
    // Retries until sCharacterCache reports the name as unused.
    static std::string RollUniqueName();

    // Human-readable dump of the configured weights — used by `.playerbot
    // dist` to show operators what distribution to expect.
    static std::string DescribeWeights();
};

} // namespace Playerbot::V2
