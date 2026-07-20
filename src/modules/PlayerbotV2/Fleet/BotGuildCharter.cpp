// BotGuildCharter - see header. Mirrors WorldSession petition handlers
// without packet serialization. All three primitives are world-thread
// only; callers are the idle:guild_charter_drive rule + the BotGuildMgr
// founder bookkeeping.

#include "BotGuildCharter.h"

#include "CharacterCache.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PetitionMgr.h"
#include "Player.h"
#include "UnitDefines.h"
#include "World.h"
#include "WorldSession.h"

// Petition item entry — copied from PetitionsHandler.cpp's #define.
// Kept inline rather than as a header constant because the upstream
// value lives in a .cpp; if Blizzard rotates the item entry we'd
// rather catch the divergence at compile-time than carry a stale
// header constant.
namespace {
constexpr uint32 kGuildCharterItemId = 5863;
} // anonymous

namespace Playerbot::V2 {

CharterBuyResult BotBuyGuildCharter(
    Player* bot,
    Creature* petitioner_npc,
    std::string const& guild_name,
    uint64& out_charter_item_low)
{
    out_charter_item_low = 0;

    if (!bot)
    {
        TC_LOG_WARN("playerbot.v2", "[BotBuyGuildCharter] reason=null_bot");
        return CharterBuyResult::BadNpc;
    }

    // Verify NPC is a petitioner the bot can interact with. Mirrors
    // GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_PETITIONER, ...).
    if (!petitioner_npc ||
        !bot->GetNPCIfCanInteractWith(petitioner_npc->GetGUID(),
                                      UNIT_NPC_FLAG_PETITIONER,
                                      UNIT_NPC_FLAG_2_NONE))
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotBuyGuildCharter] reason=cant_interact bot={} npc_guid={} "
            "npc_alive={} npc_flags={:#x}",
            bot->GetName(),
            petitioner_npc ? petitioner_npc->GetGUID().GetCounter() : 0,
            petitioner_npc ? petitioner_npc->IsAlive() : false,
            petitioner_npc ? uint64(petitioner_npc->GetNpcFlags()) : 0);
        return CharterBuyResult::BadNpc;
    }

    if (bot->GetGuildId())
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotBuyGuildCharter] reason=already_in_guild bot={} guild_id={}",
            bot->GetName(), bot->GetGuildId());
        return CharterBuyResult::InGuild;
    }

    if (sGuildMgr->GetGuildByName(guild_name))
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotBuyGuildCharter] reason=name_taken bot={} name='{}'",
            bot->GetName(), guild_name);
        return CharterBuyResult::NameTaken;
    }

    if (sObjectMgr->IsReservedName(guild_name) ||
        !ObjectMgr::IsValidCharterName(guild_name))
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotBuyGuildCharter] reason=name_invalid bot={} name='{}'",
            bot->GetName(), guild_name);
        return CharterBuyResult::NameInvalid;
    }

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(kGuildCharterItemId);
    if (!proto)
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotBuyGuildCharter] reason=no_template item_id={}",
            kGuildCharterItemId);
        return CharterBuyResult::NoTemplate;
    }

    const uint32 cost = sWorld->getIntConfig(CONFIG_CHARTER_COST_GUILD);
    if (!bot->HasEnoughMoney(uint64(cost)))
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotBuyGuildCharter] reason=no_money bot={} have={} need={}",
            bot->GetName(), bot->GetMoney(), cost);
        return CharterBuyResult::NoMoney;
    }

    // Sweep any stale charter Item the bot is already carrying — typically
    // a prior session's aborted FSM that left the bag-occupying Item
    // behind. Without this the CanStoreNewItem below fails with BagFull
    // when the bot has a leftover charter even if their bag is otherwise
    // open. Walk the player-bag slots (16 backpack + 4 extra bags) and
    // destroy any charter-entry item AND its server-side Petition row.
    if (Petition const* prior = sPetitionMgr->GetPetitionByOwner(bot->GetGUID()))
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotBuyGuildCharter] sweeping stale petition bot={} prior='{}'",
            bot->GetName(), prior->PetitionName);
        ObjectGuid prior_guid = prior->PetitionGuid;
        sPetitionMgr->RemovePetition(prior_guid);
        if (Item* prior_item = bot->GetItemByGuid(prior_guid))
            bot->DestroyItem(prior_item->GetBagSlot(), prior_item->GetSlot(), true);
    }
    // Also destroy any orphan charter items (entry 5863) — the petition
    // row may have been cleaned but the Item left behind on a prior
    // crash. Cheap (16-slot loop) and only runs at phase 2 buy time.
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* it = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            if (it->GetEntry() == kGuildCharterItemId)
                bot->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
        }
    }

    ItemPosCountVec dest;
    InventoryResult msg = bot->CanStoreNewItem(
        NULL_BAG, NULL_SLOT, dest, kGuildCharterItemId, proto->GetBuyCount());
    if (msg != EQUIP_ERR_OK)
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotBuyGuildCharter] reason=bag_full bot={} inv_result={}",
            bot->GetName(), uint32(msg));
        return CharterBuyResult::BagFull;
    }

    bot->ModifyMoney(-int32(cost));
    Item* charter = bot->StoreNewItem(dest, kGuildCharterItemId, true);
    if (!charter) return CharterBuyResult::StoreFailed;

    charter->SetPetitionId(charter->GetGUID().GetCounter());
    charter->SetState(ITEM_CHANGED, bot);

    sPetitionMgr->AddPetition(charter->GetGUID(), bot->GetGUID(),
                              guild_name, /*isLoading*/ false);

    out_charter_item_low = charter->GetGUID().GetCounter();
    return CharterBuyResult::Ok;
}

CharterSignResult BotSignGuildCharter(
    Player* signer,
    uint64 petition_item_low)
{
    if (!signer) return CharterSignResult::NoPetition;

    // Resolve the petition by item-guid. TC's PetitionMgr indexes
    // petitions by the charter item's full ObjectGuid; reconstruct
    // from low + high (Item HighGuid).
    ObjectGuid petition_guid = ObjectGuid::Create<HighGuid::Item>(petition_item_low);
    Petition* petition = sPetitionMgr->GetPetition(petition_guid);
    if (!petition) return CharterSignResult::NoPetition;

    if (petition->OwnerGuid == signer->GetGUID())
        return CharterSignResult::OwnerCantSign;

    if (signer->GetGuildId())
        return CharterSignResult::SignerInGuild;

    if (!sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
        signer->GetTeam() != sCharacterCache->GetCharacterTeamByGuid(petition->OwnerGuid))
        return CharterSignResult::WrongFaction;

    const uint32 account_id = signer->GetSession()->GetAccountId();
    if (petition->IsPetitionSignedByAccount(account_id))
        return CharterSignResult::AlreadySigned;

    if (petition->Signatures.size() >= 10)
        return CharterSignResult::SignaturesFull;

    petition->AddSignature(account_id, signer->GetGUID(), /*isLoading*/ false);

    // Bump the charter item's displayed signature count on the
    // founder's side, if their charter item is reachable. The founder
    // may not be in-range of the signer; if their item isn't found
    // we still return Ok — the counter resyncs on next bag refresh.
    if (Player* owner = ObjectAccessor::FindConnectedPlayer(petition->OwnerGuid))
    {
        if (Item* item = owner->GetItemByGuid(petition_guid))
        {
            item->SetPetitionNumSignatures(static_cast<uint32>(petition->Signatures.size()));
            item->SetState(ITEM_CHANGED, owner);
        }
    }

    return CharterSignResult::Ok;
}

CharterTurnInResult BotTurnInGuildCharter(
    Player* bot,
    Creature* petitioner_npc,
    uint64 petition_item_low,
    uint64& out_guild_id)
{
    out_guild_id = 0;
    if (!bot) return CharterTurnInResult::BadNpc;

    if (!petitioner_npc ||
        !bot->GetNPCIfCanInteractWith(petitioner_npc->GetGUID(),
                                      UNIT_NPC_FLAG_PETITIONER,
                                      UNIT_NPC_FLAG_2_NONE))
        return CharterTurnInResult::BadNpc;

    ObjectGuid petition_guid = ObjectGuid::Create<HighGuid::Item>(petition_item_low);
    Item* item = bot->GetItemByGuid(petition_guid);
    if (!item) return CharterTurnInResult::NoCharterItem;

    Petition const* petition = sPetitionMgr->GetPetition(petition_guid);
    if (!petition) return CharterTurnInResult::NoPetition;

    if (bot->GetGuildId())
        return CharterTurnInResult::OwnerInGuild;

    // Copy name + signatures (TC's handler does too — Guild::AddMember
    // invalidates the petition).
    std::string const name = petition->PetitionName;
    SignaturesVector const signatures = petition->Signatures;

    const uint32 required = sWorld->getIntConfig(CONFIG_MIN_PETITION_SIGNS);
    if (signatures.size() < required)
        return CharterTurnInResult::NeedMoreSignatures;

    if (sGuildMgr->GetGuildByName(name))
        return CharterTurnInResult::NameTakenAtSubmit;

    bot->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    Guild* guild = new Guild;
    if (!guild->Create(bot, name))
    {
        delete guild;
        return CharterTurnInResult::GuildCreateFailed;
    }
    sGuildMgr->AddGuild(guild);

    // Add signers as members. TC's handler does this in a single DB
    // transaction; mirror that to keep the guild_member rows
    // consistent across a partial failure.
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    for (Signature const& sig : signatures)
        guild->AddMember(trans, sig.second);
    CharacterDatabase.CommitTransaction(trans);

    sPetitionMgr->RemovePetition(petition_guid);

    out_guild_id = guild->GetId();
    return CharterTurnInResult::Ok;
}

RecruitResult BotRecruitToGuild(
    Player* recruiter,
    Player* target,
    uint64& out_guild_id)
{
    out_guild_id = 0;
    if (!recruiter || !target) return RecruitResult::BadRecruiter;

    const uint64 gid = recruiter->GetGuildId();
    if (gid == 0) return RecruitResult::BadRecruiter;
    Guild* g = sGuildMgr->GetGuildById(gid);
    if (!g) return RecruitResult::BadRecruiter;

    // Officer+ check (rank 0 = GM, 1 = Officer; recruiters need
    // GR_RIGHT_INVITE which both naturally have on default ranks).
    // Member::GetRankId returns GuildRankId enum.
    Guild::Member const* rm = const_cast<Guild const*>(g)->GetMember(recruiter->GetGUID());
    if (!rm) return RecruitResult::BadRecruiter;
    const uint8 rid = static_cast<uint8>(rm->GetRankId());
    if (rid > 1) return RecruitResult::BadRecruiter;     // only GM + Officer

    if (target->GetGuildId() != 0) return RecruitResult::BadTarget;
    if (!sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD)
        && recruiter->GetTeam() != target->GetTeam())
        return RecruitResult::BadTarget;

    // Member cap enforcement. TC's `Guild::AddMember` itself doesn't
    // enforce a cap, so we gate here.
    if (g->GetMembersCount() >= /*per-bot-guild cap pulled from manager*/ 75u)
        return RecruitResult::GuildFull;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    const bool ok = g->AddMember(trans, target->GetGUID());
    CharacterDatabase.CommitTransaction(trans);
    if (!ok) return RecruitResult::AddFailed;

    out_guild_id = gid;
    return RecruitResult::Ok;
}

} // namespace Playerbot::V2
