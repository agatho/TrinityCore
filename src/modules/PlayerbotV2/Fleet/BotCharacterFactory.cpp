#include "BotCharacterFactory.h"
#include "BotAccountMgr.h"
#include "BotIdentityRegistry.h"
#include "BotNamePool.h"
#include "../Services.h"
#include "../Session/BotSession.h"

#include "AccountMgr.h"
#include "CharacterCache.h"
#include "CharacterPackets.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include <random>
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RealmList.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"

#include <fmt/format.h>
#include <memory>

namespace Playerbot::V2 {

namespace {

bool IsRacePlayable(uint8 race)
{
    ChrRacesEntry const* entry = sChrRacesStore.LookupEntry(race);
    if (!entry)
        return false;
    return !entry->GetFlags().HasFlag(ChrRacesFlag::NPCOnly);
}

bool IsClassPlayable(uint8 cls)
{
    return sChrClassesStore.LookupEntry(cls) != nullptr;
}

} // anonymous

BotCharacterFactory::Result BotCharacterFactory::Create(
    WorldSession*      ownerSession,
    std::string const& name,
    uint8              race,
    uint8              charClass,
    uint8              gender)
{
    if (!IsRacePlayable(race))
        return {false, fmt::format("race {} is not playable", race), {}};

    if (!IsClassPlayable(charClass))
        return {false, fmt::format("class {} is not playable", charClass), {}};

    if (gender != GENDER_MALE && gender != GENDER_FEMALE)
        return {false, fmt::format("gender {} must be 0 (male) or 1 (female)", gender), {}};

    // race/class pair must have a PlayerInfo (start position, items, spells)
    if (!sObjectMgr->GetPlayerInfo(race, charClass))
        return {false, fmt::format("race/class pair {}/{} has no PlayerInfo (invalid combination)", race, charClass), {}};

    // Build name: normalize, validate, and ensure uniqueness via the cache.
    // ownerSession is optional — null comes from headless callers (auto-spawn
    // on boot) where there's no GM session to read locale from. Default to
    // LOCALE_enUS in that case so the latin name validator runs the same
    // ruleset it would for an English-locale GM.
    std::string normalized = name;
    if (!normalizePlayerName(normalized))
        return {false, "invalid name (failed normalization)", {}};

    const LocaleConstant nameLocale = ownerSession
        ? ownerSession->GetSessionDbcLocale()
        : LOCALE_enUS;
    if (auto code = ObjectMgr::CheckPlayerName(normalized, nameLocale, true);
        code != CHAR_NAME_SUCCESS)
        return {false, fmt::format(
            "ObjectMgr::CheckPlayerName rejected '{}' with code={}", normalized, uint32(code)), {}};

    if (sObjectMgr->IsReservedName(normalized))
        return {false, "name is reserved", {}};

    if (sCharacterCache->GetCharacterCacheByName(normalized))
        return {false, "name already in use", {}};

    // Acquire a slot from the dedicated bot account pool. The new char
    // is owned by that pool account, NOT the GM's. AcquireSlot returns 0
    // only if pool starvation (account creation refused) — see log.
    const uint32 botAccountId = Services::Accounts().AcquireSlot();
    if (!botAccountId)
        return {false, "BotAccountMgr::AcquireSlot failed (pool starvation; see playerbot.v2 log)", {}};

    // Resolve the bot account name for the temporary BotSession's audit
    // string (used by WorldSession diagnostics). Falls back to a synthetic
    // tag if the account name lookup fails (which only happens if the
    // account was deleted between AcquireSlot and now — race-impossible
    // on the world thread).
    std::string botAccountName;
    if (!sAccountMgr->GetName(botAccountId, botAccountName))
        botAccountName = fmt::format("acct{}", botAccountId);

    // Build CharacterCreateInfo with DB2-driven random customizations.
    //
    // Previously Customizations was left empty. Player::Create accepts that
    // (ValidateAppearance treats an empty iterator as valid after confirming
    // the race/gender has DB2 options at all), but the resulting bot has
    // the bare default face/hair/skin — every bot of a given race+sex
    // looks identical, which reads as "obvious bot" in a city full of them.
    //
    // The client-side character creator picks one Choice per Option for the
    // chosen (race, sex). We mirror that by walking
    // sDB2Manager.GetCustomiztionOptions(race, sex) and rolling one valid
    // Choice per option. Skips choices flagged with NPC_ONLY (Flags bit 0)
    // to avoid awkward NPC-template appearances slipping into player bots.
    auto createInfo = std::make_shared<WorldPackets::Character::CharacterCreateInfo>();
    createInfo->Race  = race;
    createInfo->Class = charClass;
    createInfo->Sex   = gender;
    createInfo->Name  = normalized;

    if (auto const* options = sDB2Manager.GetCustomiztionOptions(race, gender))
    {
        // Use the bot's GUID counter as a seed-ish for visual diversity
        // across bots while staying deterministic within one Create call.
        static thread_local std::mt19937 cust_rng{std::random_device{}()};
        for (ChrCustomizationOptionEntry const* opt : *options)
        {
            if (!opt) continue;
            auto const* choices = sDB2Manager.GetCustomiztionChoices(opt->ID);
            if (!choices || choices->empty()) continue;

            // Collect choices that don't carry a CustomizationReq the
            // race/sex/class wouldn't satisfy. The simplest safe-list:
            // require ChrCustomizationReqID == 0 (no special unlock).
            // Catches "Allied Race only" / "Class quest only" choices.
            std::vector<ChrCustomizationChoiceEntry const*> candidates;
            candidates.reserve(choices->size());
            for (ChrCustomizationChoiceEntry const* c : *choices)
            {
                if (!c) continue;
                if (c->ChrCustomizationReqID != 0) continue;
                candidates.push_back(c);
            }
            if (candidates.empty()) continue;

            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            ChrCustomizationChoiceEntry const* chosen = candidates[pick(cust_rng)];

            UF::ChrCustomizationChoice entry;
            entry.ChrCustomizationOptionID = opt->ID;
            entry.ChrCustomizationChoiceID = chosen->ID;
            createInfo->Customizations.push_back(entry);
        }
    }

    // Temporary BotSession owned by the bot account. Player::Create reads
    // GetSession()->GetAccountId() when writing the characters row, and
    // ValidateAppearance / IsARecruiter via GetSession() — all of which
    // are safe on a socketless BotSession. The session is discarded as
    // soon as this function returns; the actual login uses a fresh
    // BotSession created by BotSessionMgr::LoginBot.
    auto creationSession = std::make_shared<BotSession>(botAccountId, std::string(botAccountName));

    // Mirror the core's shared_ptr<Player> with cleanup deleter so the
    // Player is properly torn down (map, motion master, lock state) when it
    // goes out of scope.
    std::shared_ptr<Player> newChar(new Player(creationSession.get()), [](Player* p)
    {
        p->CleanupsBeforeDelete();
        delete p;
    });

    newChar->GetMotionMaster()->Initialize();

    ObjectGuid::LowType newGuidLow = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();
    if (!newChar->Create(newGuidLow, createInfo.get()))
    {
        // Player::Create failed AFTER we may have acquired the name from
        // BotNamePool — release it so the slot returns to the pool.
        Fleet::BotNamePool::Release(normalized);
        return {false, "Player::Create failed (race/class/appearance validation)", {}};
    }

    newChar->SetAtLoginFlag(AT_LOGIN_FIRST);   // grants starting items + cinematic on first login

    // SaveToDB(true) commits asynchronously - the call returns before the
    // INSERT INTO characters has actually landed on the DB worker. Under
    // load (mass spawn after wipefleet), the caller's subsequent LoginBot
    // racing the async write produced "Player::LoadFromDB ... not found
    // in table characters". A previous fix attempted DirectCommitTransaction
    // for sync semantics but failed at runtime - several Player::SaveToDB
    // statements are flagged CONNECTION_ASYNC only and aren't loaded on the
    // sync connection (assertion failure: m_mStmt at MySQLConnection.cpp:224).
    //
    // The async path is the only one TC really supports for character save.
    // To avoid the LoginBot race, callers (BotPopulationManager::SpawnNew)
    // defer the LoginBot call by one world tick - by then the async worker
    // has committed and LoadFromDB sees the row. See BotPopulationManager
    // for the deferred-login queue.
    newChar->SaveToDB(true);

    ObjectGuid newGuid = ObjectGuid::Create<HighGuid::Player>(newGuidLow);

    // Mirror the core's post-save bookkeeping (script hook + cache entry).
    sScriptMgr->OnPlayerCreate(newChar.get());
    sCharacterCache->AddCharacterCacheEntry(
        newGuid,
        botAccountId,                    // owner is the bot account, NOT the GM
        normalized,
        Gender(gender),
        race,
        charClass,
        newChar->GetLevel(),
        false /*isDeleted*/);

    // Mark as a V2 bot. The shared_ptr deleter will run CleanupsBeforeDelete
    // on the Player when this function returns; the row in `characters` and
    // the entry in `playerbot_v2_character` persist.
    Services::Lifecycle().mark_as_bot(newGuidLow);

    // Bind the name pool entry to this character so future delete paths
    // (hygiene cron, .playerbot delete) can release the name on cleanup.
    // No-op if the name came from the syllable-gen fallback.
    Fleet::BotNamePool::BindToCharacter(normalized, newGuidLow);

    // Update the pool's character-count cache so the next AcquireSlot
    // accounts for this new char (preventing 11+ chars on one account).
    Services::Accounts().note_character_added(botAccountId);

    // Update realmcharacters so this bot account's character count stays
    // accurate. Mirrors the LOGIN_REP_REALM_CHARACTERS query that core's
    // HandleCharCreateOpcode runs at the end of finalizeCharacterCreation.
    // Counts ALL chars on the account via a fresh SELECT (cheaper than
    // tracking deltas across our path which doesn't see core deletes).
    if (auto countResult = CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM characters WHERE account = {}", botAccountId))
    {
        const uint32 newCharCount = uint32(countResult->Fetch()[0].GetUInt64());
        LoginDatabase.PExecute(
            "REPLACE INTO realmcharacters (numchars, acctid, realmid) VALUES ({}, {}, {})",
            newCharCount, botAccountId, sRealmList->GetCurrentRealmId().Realm);
    }

    TC_LOG_INFO("playerbot.v2",
        "[BotCharacterFactory] Created bot character '{}' (guid {}, race {} class {} gender {}) on bot account '{}' (id {}) — requested by GM account {}",
        normalized, newGuid.ToString(), race, charClass, gender,
        botAccountName, botAccountId,
        ownerSession ? ownerSession->GetAccountId() : 0);

    return {true, {}, newGuid};
}

} // namespace Playerbot::V2
