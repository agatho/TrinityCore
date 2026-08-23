/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_DECOR_DUEL_MGR_H
#define TRINITYCORE_DECOR_DUEL_MGR_H

// =============================================================================
// Decor Duels (#12) — housing prop-hunt / tag minigame — SCAFFOLDING + SEAM
// -----------------------------------------------------------------------------
// Despite the name this is NOT a decorating contest. DB2 achievement category
// 15574 reveals a housing-instanced prop-hunt / tag minigame: Seekers (roles
// Spellbreaker / Nullifier / Arcane Ranger) hunt Hiders who disguise among the
// decor; players "tag opposing players", stay "the last untagged hider", win "a
// round as a Seeker with all hiders tagged within two minutes", buy "Kit
// Upgrades".
//
// EVIDENCE DISCIPLINE — what is CONFIRMED vs CAPTURE-BLOCKED:
//   CONFIRMED @68887 (wago.tools DB2 build 12.0.7.68887):
//     * Achievement category 15574
//     * Achievements 61792 / 61793 / 61878-61887 (see the enum below), icon 1392559
//   CAPTURE-BLOCKED (needs a Decor Duel queue+round capture — NONE held):
//     * the lobby/queue and round wire (opcodes)
//     * the role/kit spell ids
//     * the housing minigame MAP id
//   Nothing below invents any of those — they are 0 / no-op and flagged
//   TODO(CAPTURE-BLOCKED). This file is deliberately the SEAM only: it can
//   credit the confirmed achievements once a round completes, and it exposes the
//   round-manager entry points as documented no-ops so the real wire can be
//   dropped in later without touching the call sites.
//
// REALM-SAFE: everything is gated behind IsEnabled() and tolerant of absent data
// (no DB row, no map, no config → the manager loads disabled and every seam is a
// harmless no-op). It never touches the central integration realm.
// =============================================================================

#include "Define.h"
#include <array>

class Player;

// DecorCategory-15574 achievement ids (CONFIRMED @68887).
enum DecorDuelAchievement : uint32
{
    DECOR_DUEL_ACHIEVEMENT_CATEGORY          = 15574,

    // Role-tag achievements (Seeker roles)
    ACHIEVEMENT_DD_TAG_SPELLBREAKER          = 61792, // "T-A-G that spells Gotcha!"
    ACHIEVEMENT_DD_TAG_NULLIFIER             = 61793, // "Deployed to the Void"
    ACHIEVEMENT_DD_TAG_ARCANE_RANGER         = 61878, // "Tagged and Bagged"

    // Round-outcome achievements
    ACHIEVEMENT_DD_SEEKER_WIN_2MIN           = 61879, // "You're It" (all hiders tagged <=2 min)
    ACHIEVEMENT_DD_HIDE_90SEC                = 61880, // "It's Cold Here in This Shadow" (hide 1.5 min)
    ACHIEVEMENT_DD_HIDE_AND_PEEKLESS         = 61881, // "Hide and Peekless"
    ACHIEVEMENT_DD_NULL_AND_AVOIDED          = 61882, // "Null and Avoided"
    ACHIEVEMENT_DD_NOW_YOU_DONT_SEE_ME       = 61883, // "Now You Don't See Me..."
    // 61884 / 61885 exist in the 61878-61887 range but are not individually
    // confirmed in the blueprint; they are accepted by the range membership
    // check below and remain TODO(CAPTURE-BLOCKED) for their exact criteria.
    ACHIEVEMENT_DD_KIT_AND_CABOODLE          = 61886, // "The Whole Kit and Caboodle" (all Kit Upgrades)
    ACHIEVEMENT_DD_DISGUISED_TO_THE_NINES    = 61887, // "Disguised to the Nines" (meta)
};

// Seeker/Hider roles. The role→spell mapping is CAPTURE-BLOCKED (spell ids
// unknown); this enum only names the roles the achievements reference.
enum class DecorDuelRole : uint8
{
    None        = 0,
    Spellbreaker = 1,   // Seeker
    Nullifier    = 2,   // Seeker
    ArcaneRanger = 3,   // Seeker
    Hider        = 4,
};

class TC_GAME_API DecorDuelMgr
{
public:
    static DecorDuelMgr& Instance();

    DecorDuelMgr(DecorDuelMgr const&) = delete;
    DecorDuelMgr(DecorDuelMgr&&) = delete;
    DecorDuelMgr& operator=(DecorDuelMgr const&) = delete;
    DecorDuelMgr& operator=(DecorDuelMgr&&) = delete;

    // Load config/template (tolerant); leaves the manager disabled if the data
    // (or the CAPTURE-BLOCKED map id) is absent. Safe to call at world load.
    void Initialize();

    // Gate for every seam. Decor Duels is disabled until the round wire + map id
    // are captured and a decor_duel_template row enables it.
    bool IsEnabled() const { return _enabled; }

    // --- Achievement seam (LIVE) -------------------------------------------
    // These are the only functional pieces: once a real round completes, the
    // round manager calls these to credit the confirmed cat-15574 achievements.

    // True if achievementId belongs to the Decor Duel set (61792, 61793, or the
    // 61878-61887 range) — the membership gate that keeps the seam from crediting
    // anything it shouldn't.
    bool IsDecorDuelAchievement(uint32 achievementId) const;

    // Credit a single Decor Duel achievement to a player. No-op (with a warning)
    // if the id is not a Decor Duel achievement, the manager is disabled, the
    // player is null, or the achievement is absent from the client DB2.
    void CreditAchievement(Player* player, uint32 achievementId) const;

    // Convenience: credit the correct role-tag achievement for a Seeker role.
    void CreditRoleTag(Player* player, DecorDuelRole seekerRole) const;

    // --- Round / scenario seam (CAPTURE-BLOCKED no-ops) --------------------
    // The round wire, roles' spell ids and the neighborhood minigame map id are
    // all CAPTURE-BLOCKED. These entry points exist so the eventual capture can
    // be wired without changing call sites. They intentionally do nothing yet.

    // Would start a round on the housing/neighborhood minigame map. Returns false
    // until the map id + round wire are captured. TODO(CAPTURE-BLOCKED).
    bool StartRound(uint32 neighborhoodMapId);

    // Would assign Seeker/Hider roles for a starting round. TODO(CAPTURE-BLOCKED).
    void AssignRoles();

    // Would register a tag event (seeker tags hider) and drive tag state /
    // achievement criteria. TODO(CAPTURE-BLOCKED).
    void RegisterTag(Player* seeker, Player* hider, DecorDuelRole seekerRole);

    // Would evaluate the win condition (all hiders tagged / last hider standing /
    // 2-minute timer) and credit round-outcome achievements. TODO(CAPTURE-BLOCKED).
    void CheckWinCondition();

    // CAPTURE-BLOCKED map id (0 = unknown). Exposed for diagnostics/tests.
    uint32 GetMinigameMapId() const { return _minigameMapId; }

private:
    DecorDuelMgr() = default;

    bool _enabled = false;
    // CAPTURE-BLOCKED: the housing neighborhood minigame map id. 0 until a Decor
    // Duel capture yields it; keeping the manager disabled while 0.
    uint32 _minigameMapId = 0;
};

#define sDecorDuelMgr DecorDuelMgr::Instance()

#endif // TRINITYCORE_DECOR_DUEL_MGR_H
