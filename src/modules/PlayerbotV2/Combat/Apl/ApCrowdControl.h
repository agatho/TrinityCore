// ApCrowdControl.h — shared crowd-control target validity helper.
//
// A6 (2026-06-07 log forensics): CC target pickers (Mage Polymorph x3, Shaman
// Hex, etc.) selected an off-target by guid/hp/aura ONLY, never consulting the
// creature's type. The core then rejects the cast against a creature outside the
// spell's TargetCreatureType mask with SPELL_FAILED_BAD_TARGETS — Polymorph
// (118) alone burned 495,677 BAD_TARGETS in the 4-day run (~95% of all cast
// rejections), the same creature re-attempted ~181x because the reject happens
// before the cooldown is consumed. We can't read SpellInfo on the AI worker, so
// this encodes the canonical creature-type masks for the CC spells the rotations
// actually use. Header-only so the per-spec TUs pay no link cost.

#pragma once

#include <cstdint>

#include "../ApRotation.h"          // ApPredicateContext
#include "Bot/BotSnapshotView.h"    // BotSnapshotView, NearbyUnit, Role

namespace Playerbot::Combat {

// creatureType is Creature::GetCreatureType() as carried in NearbyUnit::
// creature_type. Canonical enum (SharedDefines.h):
//   BEAST=1 DRAGONKIN=2 DEMON=3 ELEMENTAL=4 GIANT=5 UNDEAD=6 HUMANOID=7
//   CRITTER=8 MECHANICAL=9 NOT_SPECIFIED=10 ...  0 = a PLAYER target (CC on a
//   player is governed by PvP diminishing returns, not a creature-type mask, so
//   never block it here).
[[nodiscard]] inline bool CanBeCCd(uint32_t spellId, uint8_t creatureType)
{
    if (creatureType == 0)
        return true;   // player — not creature-type gated
    switch (spellId)
    {
        case 118:     // Polymorph (Mage) — Beast / Humanoid / Critter
        // Polymorph visual variants (sheep/pig/turtle/etc.) share the same
        // target mask — keep them aligned with base 118 so the baseline
        // mage's variant casts get the same creature-type gating.
        case 61025: case 61780: case 126819:
        case 161353: case 161354: case 161355:
        case 51514:   // Hex (Shaman)     — Beast / Humanoid / Critter
            return creatureType == 1 || creatureType == 7 || creatureType == 8;
        case 2637:    // Hibernate (Druid) — Beast / Dragonkin
            return creatureType == 1 || creatureType == 2;
        case 710:     // Banish (Warlock) — Demon / Elemental
            return creatureType == 3 || creatureType == 4;
        case 9484:    // Shackle Undead (Priest) — Undead
            return creatureType == 6;
        case 20066:   // Repentance (Paladin)  — Humanoid
        case 19386:   // Wyvern Sting (Hunter) — Humanoid
        case 6770:    // Sap (Rogue)           — Humanoid (+ Beast/Dragonkin/Demon in modern, keep conservative)
            return creatureType == 7;
        default:
            return true;   // unknown CC spell — don't block (preserve behaviour)
    }
}

// ---- Off-target crowd-control selection (shared) ----
//
// Picks an enemy OTHER than the bot's victim to crowd-control with `spellId`,
// or ObjectGuid::Empty when the bot should NOT cast CC this tick. Every
// off-target CC rule funnels through here (Mage Polymorph across all specs,
// Shaman Hex across all specs) so the gating lives in exactly one place and
// the per-spec rules stay one-liners.
//
// 2026-06-13 ROOT-CAUSE FIX ("Balastan the Frost mage only casts CC while
// questing"): the old per-spec pickers gated on
// `nearby_enemies.size() >= 2` and skipped already-CC'd units via
// `has_aura(spellId, enemyGuid)`. BOTH were wrong:
//   * nearby_enemies is a 40-yard SCAN of every attackable hostile in sight,
//     not the bot's combat set. A questing caster in the open world almost
//     always has 2+ hostiles within 40y, so the gate was satisfied
//     essentially everywhere — the bot tried to CC even when fighting a
//     single mob, and could sheep a distant neutral it then pulled.
//   * has_aura(spellId, enemyGuid) routes through my_auras_on_others, which
//     the snapshot builder only populates for multi-DoT specs. For a Mage /
//     Shaman it ALWAYS returns false, so the picker never saw its own sheep
//     and re-cast Polymorph/Hex every GCD — the damage rules lower in the
//     table never ran. Symptom: the bot "only casts CC".
//
// Corrected gating:
//   * PvE: require 2+ real ATTACKERS (units actually fighting the bot) and
//     pick the CC target from that set — the bot only ever sheeps something
//     that is hitting it, never a scan-radius bystander. A solo pull (one
//     attacker) never CCs; it just kills the mob.
//   * Skip already-CC'd units via NearbyUnit::is_cc_locked (populated for
//     EVERY attacker and nearby enemy), so we neither re-apply our own CC nor
//     break a teammate's.
//   * PvP (BG/arena): additionally scan nearby_enemies for an enemy Healer
//     then any caster — those rarely attack the bot directly, so the attacker
//     gate alone would miss the single highest-value CC target in PvP.
//   * CanBeCCd creature-type masking and the victim-skip are preserved.
[[nodiscard]] inline ObjectGuid PickOffTargetCC(ApPredicateContext const& ctx,
                                                uint32_t spellId, bool pvp)
{
    BotSnapshotView const& bot = ctx.bot;
    ObjectGuid const v = bot.victim();
    if (v.IsEmpty())
        return ObjectGuid::Empty;

    auto valid = [&](NearbyUnit const& u) -> bool {
        if (u.guid == v) return false;          // never CC our own kill target
        if (u.hp <= 0) return false;            // corpse
        if (u.is_cc_locked) return false;       // already CC'd (ours or ally's)
        return CanBeCCd(spellId, u.creature_type);
    };

    // PvE: only CC a genuine multi-attacker pull, and only a unit on us.
    auto const& attackers = bot.raw().combat.attackers;
    if (attackers.size() >= 2)
    {
        // Prefer silencing a casting attacker; otherwise any valid attacker.
        for (auto const& u : attackers)
            if (u.is_casting && valid(u)) return u.guid;
        for (auto const& u : attackers)
            if (valid(u)) return u.guid;
    }

    // PvP: lock the enemy healer / caster even if it isn't attacking us.
    if (pvp)
    {
        auto const& enemies = bot.raw().combat.nearby_enemies;
        for (auto const& u : enemies)
            if (u.role == Role::Healer && valid(u)) return u.guid;
        for (auto const& u : enemies)
            if (u.is_casting && valid(u)) return u.guid;
    }
    return ObjectGuid::Empty;
}

// Convenience PvP-flag derivation shared by every CC predicate.
[[nodiscard]] inline bool ApInPvp(ApPredicateContext const& ctx)
{
    return ctx.pvp.in_battleground || ctx.pvp.in_arena;
}

} // namespace Playerbot::Combat
