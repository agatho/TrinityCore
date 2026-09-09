// BotGuildCharter - direct-call helpers wrapping TC's petition flow.
//
// The Phase A.2 charter FSM (idle:guild_charter_drive) needs to drive a
// bot through the same operations a real player performs:
//   1. Buy charter from petitioner NPC.
//   2. Get nearby allies to sign.
//   3. Turn the charter in (founds the guild).
//
// `WorldSession::HandlePetitionBuy` / `HandleSignPetition` /
// `HandleTurnInPetition` already implement these for player-driven
// clients, but they expect WorldPackets and a network round-trip.
// Bots don't have a network round-trip; they invoke the underlying
// TC primitives directly. This file factors out the **non-packet
// portions** of those handlers into reusable Player*-taking helpers.
//
// Returns enum success/failure rather than throwing; callers (the
// FSM) inspect the result and advance / retry / abort accordingly.
//
// World-thread only.

#pragma once

#include <cstdint>
#include <string>

class Player;
class Creature;

namespace Playerbot::V2 {

enum class CharterBuyResult : uint8_t
{
    Ok            = 0,
    BadNpc        = 1,    // not a petitioner / not in interact range
    InGuild       = 2,    // bot already in a guild
    NameTaken     = 3,    // sGuildMgr->GetGuildByName != null
    NameInvalid   = 4,    // ObjectMgr::IsValidCharterName failed
    NoMoney       = 5,    // bot can't afford CHARTER_COST_GUILD
    BagFull       = 6,    // CanStoreNewItem returned !OK
    NoTemplate    = 7,    // GUILD_CHARTER_ITEM_ID template missing
    StoreFailed   = 8,    // StoreNewItem returned null
};

enum class CharterSignResult : uint8_t
{
    Ok                  = 0,
    NoPetition          = 1,    // sPetitionMgr->GetPetition == null
    OwnerCantSign       = 2,    // signer == petition owner
    SignerInGuild       = 3,    // signer already in a guild
    AlreadySigned       = 4,    // PETITION_SIGN_ALREADY_SIGNED
    SignaturesFull      = 5,    // ≥10 signatures
    WrongFaction        = 6,    // owner+signer factions differ + cross-faction-disabled
};

enum class CharterTurnInResult : uint8_t
{
    Ok                  = 0,
    BadNpc              = 1,
    NoCharterItem       = 2,
    NoPetition          = 3,
    OwnerInGuild        = 4,
    NeedMoreSignatures  = 5,
    NameTakenAtSubmit   = 6,
    GuildCreateFailed   = 7,    // Guild::Create returned false
};

// ----- Buy charter -----
//
// Mirrors WorldSession::HandlePetitionBuy without packet serialization.
// On Ok, the charter item is stored in `bot`'s inventory and registered
// with sPetitionMgr; returns the charter item's GUID in `out_charter_item_low`.
CharterBuyResult BotBuyGuildCharter(
    Player* bot,
    Creature* petitioner_npc,
    std::string const& guild_name,
    uint64& out_charter_item_low);

// ----- Sign charter -----
//
// Mirrors WorldSession::HandleSignPetition. `petition_item_low` is the
// charter item's GUID low (founder's possession). Adds `signer`'s
// signature; on Ok the petition's signature count grows by one.
CharterSignResult BotSignGuildCharter(
    Player* signer,
    uint64 petition_item_low);

// ----- Turn in charter -----
//
// Mirrors WorldSession::HandleTurnInPetition. On Ok the guild is
// created with `bot` as GM and all signers as members; the charter
// item is destroyed; the petition is removed from sPetitionMgr.
// `out_guild_id` is the newly-created guild's id.
CharterTurnInResult BotTurnInGuildCharter(
    Player* bot,
    Creature* petitioner_npc,
    uint64 petition_item_low,
    uint64& out_guild_id);

// ----- Phase B: organic recruitment -----
//
// Direct-call helper: officer `recruiter` adds `target` to recruiter's
// guild. Bypasses the player-style invite-popup dance (both sides are
// bots; no popup confirm needed — the recruiter's decision *is* the
// recruitment event). Mirrors what Guild::AddMember + HandleAcceptMember
// would do without the WorldPacket round-trip.
//
// Returns:
//   Ok                 - target joined; `out_guild_id` set.
//   BadRecruiter       - recruiter null / not in a guild / not officer+.
//   BadTarget          - target null / already in a guild / wrong faction.
//   GuildFull          - target guild at member_cap.
//   AddFailed          - Guild::AddMember refused (DB error, etc).
enum class RecruitResult : uint8_t
{
    Ok           = 0,
    BadRecruiter = 1,
    BadTarget    = 2,
    GuildFull    = 3,
    AddFailed    = 4,
};

RecruitResult BotRecruitToGuild(
    Player* recruiter,
    Player* target,
    uint64& out_guild_id);

} // namespace Playerbot::V2
