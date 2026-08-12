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

#include "WorldSession.h"
#include "Common.h"
#include "DB2Stores.h"
#include "GossipDef.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "SpellPackets.h"
#include "TalentPackets.h"

void WorldSession::HandleLearnTalentsOpcode(WorldPackets::Talent::LearnTalents& packet)
{
    WorldPackets::Talent::LearnTalentFailed learnTalentFailed;
    bool anythingLearned = false;
    for (uint32 talentId : packet.Talents)
    {
        if (TalentLearnResult result = _player->LearnTalent(talentId, &learnTalentFailed.SpellID))
        {
            if (!learnTalentFailed.Reason)
                learnTalentFailed.Reason = result;

            learnTalentFailed.Talents.push_back(talentId);
        }
        else
            anythingLearned = true;
    }

    if (learnTalentFailed.Reason)
        SendPacket(learnTalentFailed.Write());

    if (anythingLearned)
        _player->SendTalentsInfoData();
}

void WorldSession::HandleUnlearnSpecialization(WorldPackets::Talent::UnlearnSpecialization& /*unlearnSpecialization*/)
{
    // Unlearn the active specialization at a trainer: reset spec talents + spec-granted spells back to the
    // class default and resend talent info. ResetTalentSpecialization operates on the active spec group.
    _player->ResetTalentSpecialization();
}

void WorldSession::HandleLearnPvpTalentsOpcode(WorldPackets::Talent::LearnPvpTalents& packet)
{
    WorldPackets::Talent::LearnPvpTalentFailed learnPvpTalentFailed;
    bool anythingLearned = false;
    for (WorldPackets::Talent::PvPTalent pvpTalent : packet.Talents)
    {
        if (TalentLearnResult result = _player->LearnPvpTalent(pvpTalent.PvPTalentID, pvpTalent.Slot, &learnPvpTalentFailed.SpellID))
        {
            if (!learnPvpTalentFailed.Reason)
                learnPvpTalentFailed.Reason = result;

            learnPvpTalentFailed.Talents.push_back(pvpTalent);
        }
        else
            anythingLearned = true;
    }

    if (learnPvpTalentFailed.Reason)
        SendPacket(learnPvpTalentFailed.Write());

    if (anythingLearned)
        _player->SendTalentsInfoData();
}

void WorldSession::HandleConfirmRespecWipeOpcode(WorldPackets::Talent::ConfirmRespecWipe& confirmRespecWipe)
{
    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(confirmRespecWipe.RespecMaster, UNIT_NPC_FLAG_TRAINER, UNIT_NPC_FLAG_2_NONE);
    if (!unit)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleConfirmRespecWipeOpcode - {} not found or you can't interact with him.", confirmRespecWipe.RespecMaster);
        return;
    }

    if (confirmRespecWipe.RespecType != SPEC_RESET_TALENTS)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleConfirmRespecWipeOpcode - reset type {} is not implemented.", confirmRespecWipe.RespecType);
        return;
    }

    if (!unit->CanResetTalents(_player))
        return;

    int64 cost = _player->GetNextResetTalentsCost();
    if (!_player->HasEnoughMoney(cost))
        return; // // silently return, client should display the error by itself

    // remove fake death
    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    if (!_player->ResetTalents())
    {
        _player->SendRespecWipeConfirm(ObjectGuid::Empty, 0, static_cast<SpecResetType>(confirmRespecWipe.RespecType));
        return;
    }

    _player->ModifyMoney(-cost);
    _player->IncreaseResetTalentsCostAndCounters(cost);
    _player->SendTalentsInfoData();

    unit->CastSpell(_player, 14867 /*SPELL_UNTALENT_VISUAL_EFFECT*/, true);
}

void WorldSession::HandleUnlearnSkillOpcode(WorldPackets::Spells::UnlearnSkill& packet)
{
    SkillRaceClassInfoEntry const* rcEntry = sDB2Manager.GetSkillRaceClassInfo(packet.SkillLine, GetPlayer()->GetRace(), GetPlayer()->GetClass());
    if (!rcEntry || !(rcEntry->Flags & SKILL_FLAG_UNLEARNABLE))
        return;

    GetPlayer()->SetSkill(packet.SkillLine, 0, 0, 0);
}

void WorldSession::HandleTradeSkillSetFavorite(WorldPackets::Spells::TradeSkillSetFavorite const& tradeSkillSetFavorite)
{
    if (!_player->HasSpell(tradeSkillSetFavorite.RecipeID))
        return;

    _player->SetSpellFavorite(tradeSkillSetFavorite.RecipeID, tradeSkillSetFavorite.IsFavorite);
}

// CMSG_OPEN_TRADESKILL_NPC (0x3A01E9): the client reports that a trade-skill window opened.
//
// This fires constantly - 124 times across the captures on this machine - and was landing in
// Handle_NULL. In 123 of those the guid is EMPTY (the player opened their own profession window);
// in 1 it is a real creature guid (crafting at an NPC, preceded by CMSG_SET_SELECTION on the same guid).
//
// The guid is a PackedGuid, so the empty case is a 2-byte body. Reading it as a fixed 16 bytes would
// throw ByteBufferPositionException on almost every one of these packets.
//
// The server-side effect is the interaction binding that every later profession opcode is validated
// against: a real guid starts a GarrTradeskill interaction after the usual can-interact checks, and an
// empty guid clears a stale crafter binding so a previous NPC session cannot keep authorising crafts.
void WorldSession::HandleOpenTradeSkillNpc(WorldPackets::Spells::OpenTradeSkillNpc const& packet)
{
    if (packet.NpcGUID.IsEmpty())
    {
        // Own profession window: drop any NPC crafter binding that is still standing.
        // InteractionData exposes Type directly; IsInteractingWith needs a guid we do not have here.
        if (_player->PlayerTalkClass->GetInteractionData().Type == PlayerInteractionType::GarrTradeskill)
            _player->PlayerTalkClass->GetInteractionData().Reset();

        return;
    }

    // Crafting at an NPC: only bind if the player really can interact with it right now.
    Creature* npc = _player->GetNPCIfCanInteractWith(packet.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_NONE);
    if (!npc)
        return;

    _player->PlayerTalkClass->GetInteractionData().StartInteraction(packet.NpcGUID, PlayerInteractionType::GarrTradeskill);
}
