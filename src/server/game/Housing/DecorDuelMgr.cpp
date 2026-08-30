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

#include "DecorDuelMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Player.h"

DecorDuelMgr& DecorDuelMgr::Instance()
{
    static DecorDuelMgr instance;
    return instance;
}

void DecorDuelMgr::Initialize()
{
    // Tolerant load. Decor Duels stays DISABLED unless a decor_duel_template row
    // both exists AND supplies the (CAPTURE-BLOCKED) minigame map id. Absent the
    // table (fresh DB) or the map id, we load quietly disabled — realm-safe.
    _enabled = false;
    _minigameMapId = 0;

    // The template table is optional; a missing table must never fault world load.
    // Expected (shipped, NOT applied) schema: decor_duel_template(id, mapId,
    // enabled, ...). Until a Decor Duel capture confirms the map id + round wire
    // the shipped seed leaves enabled=0 / mapId=0, so this query normally yields
    // a disabled manager.
    if (QueryResult result = WorldDatabase.Query("SELECT mapId, enabled FROM decor_duel_template WHERE id = 1"))
    {
        Field* fields = result->Fetch();
        _minigameMapId = fields[0].GetUInt32();
        bool const enabledFlag = fields[1].GetBool();

        // Only enable when the map id is actually known (non-zero) — never invent it.
        _enabled = enabledFlag && _minigameMapId != 0;
    }

    if (_enabled)
        TC_LOG_INFO("server.loading", "DecorDuelMgr: enabled (minigame map {}).", _minigameMapId);
    else
        TC_LOG_INFO("server.loading",
            "DecorDuelMgr: disabled (scaffolding only — round wire / roles' spells / map id are CAPTURE-BLOCKED). "
            "Achievement-crediting seam for category {} is available.", uint32(DECOR_DUEL_ACHIEVEMENT_CATEGORY));
}

bool DecorDuelMgr::IsDecorDuelAchievement(uint32 achievementId) const
{
    switch (achievementId)
    {
        case ACHIEVEMENT_DD_TAG_SPELLBREAKER:
        case ACHIEVEMENT_DD_TAG_NULLIFIER:
            return true;
        default:
            // 61878-61887 contiguous range (tag Arcane Ranger, round outcomes,
            // Kit-Upgrade + meta). 61884/61885 are reserved-but-unconfirmed and
            // accepted here so the seam covers the whole DB2 range.
            return achievementId >= ACHIEVEMENT_DD_TAG_ARCANE_RANGER
                && achievementId <= ACHIEVEMENT_DD_DISGUISED_TO_THE_NINES;
    }
}

void DecorDuelMgr::CreditAchievement(Player* player, uint32 achievementId) const
{
    if (!player)
        return;

    if (!IsDecorDuelAchievement(achievementId))
    {
        TC_LOG_WARN("misc", "DecorDuelMgr::CreditAchievement: {} is not a Decor Duel (cat {}) achievement; refusing.",
            achievementId, uint32(DECOR_DUEL_ACHIEVEMENT_CATEGORY));
        return;
    }

    // The crediting seam works even while the round wire is CAPTURE-BLOCKED so a
    // future round manager (or the debug driver) can grant the confirmed
    // achievements. We do not gate this on IsEnabled(): enabling is about the
    // round wire, not the reward grant.
    AchievementEntry const* achievement = sAchievementStore.LookupEntry(achievementId);
    if (!achievement)
    {
        // Client DB2 not seeded with this row on this build — tolerate quietly.
        TC_LOG_DEBUG("misc", "DecorDuelMgr::CreditAchievement: achievement {} absent from Achievement.db2; skipping.",
            achievementId);
        return;
    }

    player->CompletedAchievement(achievement);
    TC_LOG_DEBUG("misc", "DecorDuelMgr::CreditAchievement: credited achievement {} to {}.",
        achievementId, player->GetName());
}

void DecorDuelMgr::CreditRoleTag(Player* player, DecorDuelRole seekerRole) const
{
    switch (seekerRole)
    {
        case DecorDuelRole::Spellbreaker: CreditAchievement(player, ACHIEVEMENT_DD_TAG_SPELLBREAKER);  break;
        case DecorDuelRole::Nullifier:    CreditAchievement(player, ACHIEVEMENT_DD_TAG_NULLIFIER);     break;
        case DecorDuelRole::ArcaneRanger: CreditAchievement(player, ACHIEVEMENT_DD_TAG_ARCANE_RANGER); break;
        default:
            TC_LOG_DEBUG("misc", "DecorDuelMgr::CreditRoleTag: role {} has no tag achievement.", uint32(seekerRole));
            break;
    }
}

// ---------------------------------------------------------------------------
// Round / scenario seam — CAPTURE-BLOCKED no-ops.
// These intentionally do nothing until a Decor Duel capture yields the round
// wire, the roles' spell ids and the neighborhood minigame map id. Do NOT invent
// any of those here — that is the whole point of the seam.
// ---------------------------------------------------------------------------

bool DecorDuelMgr::StartRound(uint32 neighborhoodMapId)
{
    // TODO(CAPTURE-BLOCKED): create a lobby/scenario-like round on the housing
    // neighborhood minigame map, assign roles, start the 2-minute timer. Needs a
    // Decor Duel queue+round capture (opcodes) and the confirmed map id.
    if (!IsEnabled())
    {
        TC_LOG_DEBUG("misc", "DecorDuelMgr::StartRound: disabled (CAPTURE-BLOCKED round wire); map arg {} ignored.",
            neighborhoodMapId);
        return false;
    }
    return false;
}

void DecorDuelMgr::AssignRoles()
{
    // TODO(CAPTURE-BLOCKED): distribute Spellbreaker/Nullifier/Arcane Ranger vs
    // Hider roles and apply the roles' (unknown) disguise/seeker spells.
}

void DecorDuelMgr::RegisterTag(Player* /*seeker*/, Player* /*hider*/, DecorDuelRole /*seekerRole*/)
{
    // TODO(CAPTURE-BLOCKED): record a tag, update tag state, and drive the
    // per-role tag-achievement criteria. When the round wire exists, a Seeker tag
    // should route through CreditRoleTag() at the appropriate criteria point.
}

void DecorDuelMgr::CheckWinCondition()
{
    // TODO(CAPTURE-BLOCKED): evaluate all-hiders-tagged / last-hider-standing /
    // 2-minute timeout and credit the round-outcome achievements (61879-61887).
}
