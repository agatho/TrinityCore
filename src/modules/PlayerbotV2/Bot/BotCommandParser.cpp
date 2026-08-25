#include "BotCommandParser.h"
#include "BotIntent.h"
#include "BotIntentEmitter.h"
#include "BotRegistry.h"
#include "ClassTables.h"
#include "RecipeDifficulty.h"
#include "../Threading/IntentQueue.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Services.h"
#include "../PlayerbotV2.h"  // /tickperf reads Module::tickperf_snapshot()
#include "../Travel/PortalIndex.h"  // /route diagnostic uses NextHopMap
#include "../Fleet/BotPopulationManager.h"  // /rebalance diagnostic
#include "PathGenerator.h"       // /pathcompare builds an actual PathGenerator
#include "PlayerbotMovement.h"   // SehSafeCalculatePath (tile-race guard)
#include "dtQueryFilterTC.h"     // /roadstats reads RoadStats counters
#include "../Fleet/OwnerRegistry.h"
#include "BotAddressResolver.h"
#include "../Diagnostics/BotInspector.h"
#include "../Diagnostics/PerfCounters.h"
#include "Util.h"  // Trinity::Tokenize for /diag multi-line output
#include "BotSnapshot.h"
#include "BotSnapshotView.h"
#include "BotAI.h"
#include "IdleRule.h"
#include "Player.h"
#include "Item.h"
#include "ItemTemplate.h"  // ItemTemplate::GetDefaultLocaleName for /itemid + /findname
#include "ObjectMgr.h"     // sObjectMgr->GetItemTemplate
#include "SpellInfo.h"     // SpellInfo for /spell <id>
#include "SpellMgr.h"      // sSpellMgr for /spell lookup
#include "Bag.h"  // MAX_BAG_SIZE for /mailitem bag walk
#include "Group.h"
#include "DB2Stores.h"
#include "World.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ThreatManager.h"  // GetThreatManager().GetThreat for /threat
#include <algorithm>        // std::partial_sort / std::min for /wq command
#include <cmath>            // std::sqrt for /wq distance display
#include <vector>           // ranked vector inside /wq
#include "CharacterCache.h"  // sCharacterCache for friend/ignore name→guid lookup
#include "PetDefines.h"      // PetStable + PET_SAVE_FIRST_STABLE_SLOT for /stable
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "GameObject.h"      // GAMEOBJECT_TYPE_GUILD_BANK for /gbank
#include "fmt/format.h"
#include <algorithm>
#include <cmath>     // std::sqrt for /inrange distance calc
#include <cctype>
#include <cstring>
#include <deque>
#include <limits>

namespace Playerbot {

namespace {

// Lowercase + trim whitespace. Returns a copy.
std::string Normalize(std::string const& in)
{
    std::string s;
    s.reserve(in.size());
    for (char c : in) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, b - a + 1);
}

// Split a normalized command line on whitespace into the verb + numeric
// argument list. Non-numeric arg tokens are ignored (callers that need
// string arguments use bespoke parsing). Returns the verb and a vector of
// uint64s — large enough to carry copper amounts without overflow.
struct ParsedCommand
{
    std::string         verb;
    std::vector<uint64> args;
    // Raw text after the verb (before number-parsing). Used by commands
    // that take a free-form string like /follow <name>.
    std::string         tail;
};

ParsedCommand SplitArgs(std::string const& line)
{
    ParsedCommand out;
    size_t i = 0;
    auto skip_ws = [&]() { while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i; };
    auto take    = [&]() {
        skip_ws();
        size_t start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
        return line.substr(start, i - start);
    };
    out.verb = take();
    // Capture the rest of the line (after verb + whitespace) as tail.
    {
        size_t saved = i;
        skip_ws();
        out.tail = (i < line.size()) ? line.substr(i) : std::string{};
        i = saved;
    }
    while (i < line.size())
    {
        std::string tok = take();
        if (tok.empty()) break;
        // Accept "Ng" / "Ns" / "Nc" suffixes — sender shorthand for gold/silver/copper.
        // Keeps "auction sell ITEM 100g" readable. Plain digits parse as copper.
        char suffix = '\0';
        if (!tok.empty() && (tok.back() == 'g' || tok.back() == 's' || tok.back() == 'c'))
        { suffix = tok.back(); tok.pop_back(); }
        bool numeric = !tok.empty() &&
                       std::all_of(tok.begin(), tok.end(),
                                   [](char c){ return c >= '0' && c <= '9'; });
        if (!numeric) continue;
        try {
            uint64 v = std::stoull(tok);
            switch (suffix)
            {
                case 'g': v *= 10000ULL; break;   // 1 gold = 10000 copper
                case 's': v *= 100ULL;   break;   // 1 silver = 100 copper
                default:  break;                  // copper or unitless
            }
            out.args.push_back(v);
        }
        catch (...) { /* overflow — silently drop the token */ }
    }
    return out;
}

// Authorization: explicit owner binding wins. Falls back to the
// pre-owner-system "same group" check for unowned bots so legacy
// fleets keep working without a migration step.
//
// Authority precedence (highest first):
//   1. Owner binding match (account_id [+ optional player_guid]) —
//      the persistent owner set via `.playerbot mark` /
//      `.playerbot adopt`. Survives logout, group disband, server
//      restart. Once a bot is adopted, ONLY the owner can command —
//      group leadership no longer grants authority. This is the
//      production model.
//   2. (Unowned bot) bot's group leader.
//   3. (Unowned bot) any member of bot's group.
//   4. (Unowned bot, no group) any sender — permissive solo testing.
bool IsAuthorized(Player const* sender, Player const* bot)
{
    if (!sender || !bot) return false;
    if (sender == bot) return false;

    // Tier 1: persistent owner binding.
    const BotId bot_id = bot->GetGUID().GetCounter();
    OwnerBinding const owner = Services::Owners().GetOwner(bot_id);
    if (owner.account_id != 0)
    {
        // Bot is owned. Authority is exclusively the owner's account
        // (and optionally a specific char on it).
        return Services::Owners().IsOwner(
            bot_id,
            sender->GetSession() ? sender->GetSession()->GetAccountId() : 0,
            sender->GetGUID().GetCounter());
    }

    // Tier 2-4: legacy group / solo fallback for unowned bots.
    Group const* g = bot->GetGroup();
    if (!g) return true;     // Solo — fall back to permissive
    if (g->IsLeader(sender->GetGUID())) return true;
    if (g->IsMember(sender->GetGUID())) return true;
    return false;
}

// Push an intent without going through the worker thread — these are
// owner-issued commands, fire immediately.
template <class T>
bool Push(Player const* bot, T body)
{
    const BotId id = bot->GetGUID().GetCounter();
    IntentQueue* q = Services::Registry().intents(id);
    IntentId* next = Services::Registry().next_intent_id(id);
    if (!q || !next) return false;
    // Whisper rate-gate: collapse identical replies to the same
    // target within 500 ms. Owner-issued commands often emit
    // multiple replies (success line + manual-mode ack); the gate
    // permits distinct strings but kills duplicate floods from
    // spam-clicked /follow / /come / /status. Other intent types
    // bypass the gate entirely.
    if constexpr (std::is_same_v<std::decay_t<T>, WhisperIntent>)
    {
        if (BotAI* ai = Services::Registry().ai(id))
        {
            if (!ai->whisper_rate_check(body.target, body.text,
                                        GameTime::GetGameTimeMS()))
                return false;
        }
    }
    BotIntentEmitter emit(q, id, /*source*/ 0, next);
    return emit.emit(WrapForIntentBody(std::move(body)));
}

// Broadcast helper: emits `body` to `bot` plus every registered V2 bot in the
// same group (excluding sender's bot to avoid double-emit since we already
// fired for it). Returns the count of bots that received the intent (≥ 1
// when bot itself was reachable). The intent body must be copyable since we
// hand a fresh copy to each member.
template <class T>
uint32 BroadcastToGroup(Player const* bot, T const& body)
{
    if (!Push(bot, body)) return 0;
    uint32 count = 1;
    Group const* grp = bot->GetGroup();
    if (!grp) return count;
    for (GroupReference const& itr : grp->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == bot) continue;
        const BotId mid = member->GetGUID().GetCounter();
        if (!Services::Registry().has(mid)) continue;
        if (Push(member, body)) ++count;
    }
    return count;
}

// Resolve a bot's preferred follow distance — returns the BotAI override if
// set (>0), else the canonical default of 5yd. Used by every /follow path to
// honour the owner's `/follow_distance N` pin without re-resolving each time.
inline float FollowDistanceFor(Player const* bot)
{
    if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        if (ai->follow_distance() > 0.f) return ai->follow_distance();
    return 5.0f;
}

// Auto-assign a free formation slot when the bot's current slot would
// collide with another bot the same owner has in active Follow mode.
// Slot is sticky (persisted) so owner-set values stay; this only runs
// the picker when there's an actual collision. New bots default to
// slot 0; the second adopted bot picks 1; etc — without owner having
// to type /slot N for each.
inline void AutoAssignFormationSlot(Player const* bot, BotAI& ai)
{
    OwnerBinding const owner =
        Services::Owners().GetOwner(bot->GetGUID().GetCounter());
    if (owner.account_id == 0) return;          // unowned — keep slot 0
    auto const owned = Services::Owners().BotsOwnedBy(owner.account_id);
    if (owned.size() <= 1) return;               // only-bot — slot 0 is fine
    std::array<bool, 16> used{};
    bool collision = false;
    for (BotId other : owned)
    {
        if (other == bot->GetGUID().GetCounter()) continue;
        BotAI const* o = Services::Registry().ai(other);
        if (!o) continue;
        if (o->formation_slot() < used.size())
        {
            used[o->formation_slot()] = true;
            if (o->formation_slot() == ai.formation_slot())
                collision = true;
        }
    }
    if (!collision) return;                      // current slot is unique
    for (uint8 s = 0; s < used.size(); ++s)
        if (!used[s]) { ai.set_formation_slot(s); return; }
    // 16 bots all using slots 0..15 — fall through; collision absorbs.
}

// Persist the bot's owner-tunable state (formation, follow distance,
// verbose) to the playerbot_v2_character row. Called after every owner
// command that mutates a tunable so a relog comes back with the same
// preferences. Cheap (single UPDATE); no debouncing — owner-command
// frequency is bounded by human typing speed.
inline void PersistSquadState(Player const* bot, BotAI const& ai)
{
    OwnerRegistry::SquadState s;
    s.formation_type   = static_cast<uint8>(ai.formation_type());
    s.formation_slot   = ai.formation_slot();
    s.follow_distance  = ai.follow_distance() > 0.f ? ai.follow_distance() : 5.0f;
    s.owner_verbose    = ai.verbose_logging();
    Services::Owners().SaveSquadState(bot->GetGUID().GetCounter(), s);
}

// Build a FollowIntent that respects the bot's formation slot + type.
// All /follow paths route through this so a bot in Wedge slot 2 takes
// the right-flank-back position automatically. Free formation falls
// back to legacy "behind leader at default distance" — no slot offset.
inline FollowIntent BuildFollowIntent(Player const* bot, ObjectGuid leader)
{
    const float dist = FollowDistanceFor(bot);
    if (BotAI const* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
    {
        // effective_formation_type auto-spreads in combat for Tight - keeps
        // a stacked squad alive against AoE. Owner's chosen type is restored
        // when combat ends (re-resolved on the next /follow recalc tick).
        FormationType const t = ai->effective_formation_type(bot->IsInCombat());
        FormationOffset const off = ComputeFormationOffset(
            t, ai->formation_slot(), dist);
        return FollowIntent{leader, off.distance, off.angle_radians};
    }
    return FollowIntent{leader, dist, 0.0f};
}

// Iterate every registered V2 bot in `bot`'s group (including `bot` itself)
// and call `fn(player)`. Returns the count of bots visited. Used by commands
// that need to perform per-bot work that isn't expressible as a single intent
// emit (e.g. mutating BotAI state, walking snapshot fields for diagnostics).
template <class F>
uint32 ForEachGroupBot(Player const* bot, F&& fn)
{
    uint32 visited = 0;
    if (Services::Registry().has(bot->GetGUID().GetCounter()))
    {
        fn(bot);
        ++visited;
    }
    Group const* grp = bot->GetGroup();
    if (!grp) return visited;
    for (GroupReference const& itr : grp->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == bot) continue;
        const BotId mid = member->GetGUID().GetCounter();
        if (!Services::Registry().has(mid)) continue;
        fn(member);
        ++visited;
    }
    return visited;
}

} // anonymous

// Diagnostic label for an address kind — used in the squad-summary
// replies so the owner sees what filter their command actually matched.
static std::string FormatAddressKind(ResolvedAddress const& a)
{
    using K = ResolvedAddress::Kind;
    switch (a.kind)
    {
        case K::Single:      return "self";
        case K::All:         return "all";
        case K::Squad:       return "squad";
        case K::Here:        return "here";
        case K::Role_Tank:   return "tank";
        case K::Role_Healer: return "healer";
        case K::Role_Dps:    return "dps";
        case K::Marked:      return "marked";
        case K::Class:       return "class:" + a.filter;
        case K::Spec:        return "spec:" + a.filter;
        case K::Name:        return "name:" + a.filter;
    }
    return "?";
}

bool BotCommandParser::Dispatch(Player* sender, Player* bot, std::string const& msg)
{
    if (!sender || !bot) return false;
    // Resolve squad-address prefix (all:/tank:/mage:/Areon:/...).
    // Single-recipient is the most common path — short-circuit so we
    // don't pay the snapshot lookups when the owner just `/w Areon come`.
    ResolvedAddress addr = BotAddressResolver::Resolve(sender, bot, msg);
    if (addr.kind == ResolvedAddress::Kind::Single)
    {
        // No prefix — direct dispatch on the whispered bot. Authority
        // is checked by DispatchSingle.
        return DispatchSingle(sender, bot, msg);
    }
    // Prefix matched — apply to the resolved set. Reply once via the
    // whispered bot with a summary; per-bot acks would flood the chat.
    if (addr.bots.empty())
    {
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Squad: 0 bots match '{}'.", FormatAddressKind(addr))});
        return true;
    }
    uint32 succeeded = 0;
    for (Player* target : addr.bots)
    {
        if (!target) continue;
        if (DispatchSingle(sender, target, addr.command))
            ++succeeded;
    }
    Push(bot, WhisperIntent{sender->GetName(),
        fmt::format("Squad ({}): {} / {} bot(s) processed.",
                    FormatAddressKind(addr), succeeded, addr.bots.size())});
    return succeeded > 0;
}

bool BotCommandParser::DispatchSingle(Player* sender, Player* bot, std::string const& msg)
{
    if (!IsAuthorized(sender, bot)) return false;
    const std::string normalized = Normalize(msg);
    if (normalized.empty()) return false;
    const ParsedCommand parsed = SplitArgs(normalized);
    const std::string& cmd = parsed.verb;
    auto const& args = parsed.args;
    if (cmd.empty()) return false;

    // Multi-arg commands first so the single-word matchers below don't
    // swallow a "fly 5" / "bg 2" / "auction 12345 100g" with their leading
    // verb. Each guards on the right arg count.
    if (cmd == "fly" && args.size() >= 1)
    {
        // Selected target must be the flight master we're departing from.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsAnyTypeCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Fly: select a flight master first."});
            return true;
        }
        const uint32 to_node = static_cast<uint32>(args[0]);
        Push(bot, FlyToNodeIntent{selection, to_node});
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("Taxi: queued flight to node {}.", to_node)});
        return true;
    }
    if (cmd == "bg" && args.size() >= 1)
    {
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsAnyTypeCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "BG: select a battlemaster first."});
            return true;
        }
        const uint16 bg_type = static_cast<uint16>(args[0]);
        Push(bot, BgQueueIntent{selection, bg_type});
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("BG: queued for type {}.", bg_type)});
        return true;
    }
    if (cmd == "unbg" || cmd == "bgleave")
    {
        Push(bot, BgLeaveIntent{});
        return true;
    }
    if ((cmd == "bg_all" || cmd == "bgall") && args.size() >= 1)
    {
        // Mass BG queue. Sender's selected target must be a battlemaster
        // creature. Per-bot: API gates on group state (group BG queue is
        // rejected — owners should /leave_all or use the random-BG queue
        // path) and on bot's level/faction eligibility for the BG type.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsAnyTypeCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Bg_all: select a battlemaster first."});
            return true;
        }
        const uint16 bg_type = static_cast<uint16>(args[0]);
        const uint32 n = BroadcastToGroup(bot, BgQueueIntent{selection, bg_type});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Bg_all: queued {} bot(s) for BG type {}.", n, bg_type)});
        return true;
    }
    if (cmd == "unbg_all" || cmd == "bgleave_all" || cmd == "unbgall")
    {
        const uint32 n = BroadcastToGroup(bot, BgLeaveIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Unbg_all: dropped queue on {} bot(s).", n)});
        return true;
    }
    if (cmd == "lfg" && args.size() >= 1)
    {
        // Queue for an LFG dungeon by id. Role is inferred from the bot's
        // assigned spec via the snapshot — owner doesn't need to specify.
        const uint32 dungeon_id = static_cast<uint32>(args[0]);
        Role role = Role::Dps;
        if (auto snap = Services::Snapshots().latest(bot->GetGUID().GetCounter()))
            role = snap->group.my_role;
        Push(bot, LfgQueueIntent{dungeon_id, role});
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
            ai->set_last_lfg_dungeon_id(dungeon_id);
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("LFG: queued for dungeon {}.", dungeon_id)});
        return true;
    }
    if (cmd == "unlfg" || cmd == "lfgleave")
    {
        Push(bot, LfgUnqueueIntent{});
        return true;
    }
    if (cmd == "unlfg_all" || cmd == "lfgleave_all" || cmd == "unlfgall")
    {
        const uint32 n = BroadcastToGroup(bot, LfgUnqueueIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Unlfg_all: dropped LFG queue on {} bot(s).", n)});
        return true;
    }
    if ((cmd == "lfg_all" || cmd == "lfgall") && args.size() >= 1)
    {
        // Mass LFG queue for the same dungeon. Per-bot role auto-derived
        // from spec via snapshot; the API-level LfgQueueIntent fires the
        // standard LFG packet. Useful for sending a fresh group into a
        // matched dungeon together.
        const uint32 dungeon_id = static_cast<uint32>(args[0]);
        uint32 queued = 0;
        ForEachGroupBot(bot, [&](Player const* p)
        {
            const BotId bid = p->GetGUID().GetCounter();
            Role role = Role::Dps;
            if (auto snap = Services::Snapshots().latest(bid))
                role = snap->group.my_role;
            if (Push(p, LfgQueueIntent{dungeon_id, role}))
            {
                if (BotAI* ai = Services::Registry().ai(bid))
                    ai->set_last_lfg_dungeon_id(dungeon_id);
                ++queued;
            }
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Lfg_all: queued {} bot(s) for dungeon {}.", queued, dungeon_id)});
        return true;
    }
    if ((cmd == "auction" || cmd == "ah") && args.size() >= 2)
    {
        // Sell the bot's first inventory stack of `item_entry` for `buyout`
        // copper. Resolves item_entry → guid via the snapshot. Uses the
        // selected target as the auctioneer; default to 24h run-time.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsAnyTypeCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "AH: select an auctioneer first."});
            return true;
        }
        const uint32 item_entry = static_cast<uint32>(args[0]);
        const uint64 buyout     = args[1];
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        // Resolve a single matching item by entry — the snapshot doesn't
        // carry item guids, so we have to look it up live on the bot.
        Item* item = nullptr;
        for (auto const& it : snap->inventory.bag_items)
            if (it.entry == item_entry)
            {
                item = bot->GetItemByPos(it.bag, it.slot);
                if (item) break;
            }
        if (!item)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    fmt::format("AH: no inventory stack of item {}.", item_entry)});
            return true;
        }
        // Default: min_bid = buyout (no underbidding); 24h run.
        Push(bot, AuctionSellItemIntent{selection, item->GetGUID(), buyout, buyout, 1440});
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("AH: queued sell of {} for {}c.", item_entry, buyout)});
        return true;
    }
    if (cmd == "cancelall" || cmd == "cancelallah")
    {
        // Bulk-cancel every owned auction in the selected auctioneer's house.
        // Sender's selected target must be an auctioneer NPC the bot is in
        // interact range of; otherwise the API gates on InvalidTarget.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Cancelall: select an auctioneer first."});
            return true;
        }
        Push(bot, AuctionCancelAllIntent{target->GetGUID()});
        return true;
    }
    if (cmd == "cancel" && args.size() >= 1)
    {
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsAnyTypeCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Cancel: select an auctioneer first."});
            return true;
        }
        const uint32 auction_id = static_cast<uint32>(args[0]);
        Push(bot, AuctionCancelIntent{selection, auction_id});
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("AH: queued cancel of auction {}.", auction_id)});
        return true;
    }

    // Single-word commands first ----------------------------------------
    if (cmd == "unfollow" || cmd == "nofollow")
    {
        // Stop following whoever the bot is currently following. The follow
        // motion runs continuously on the world thread; StopMovementIntent
        // tears down the motion-master state cleanly. /stay/halt do the same
        // but their semantics ("stay here while I'm in combat") makes the
        // verb confusing for owners who just want to release a follow chain.
        Push(bot, StopMovementIntent{});
        Push(bot, WhisperIntent{sender->GetName(), "Unfollow: stopped."});
        return true;
    }
    if (cmd == "formation")
    {
        // /formation <type> — set the bot's formation type. Type is
        // typically broadcast via the squad-address parser ("squad:
        // formation wedge"), but the per-bot variant is useful too
        // (e.g. an off-tank explicitly going Tight while DPS is Spread).
        // Slots stay assigned across type changes.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        { Push(bot, WhisperIntent{sender->GetName(), "Formation: bot not registered."});
          return true; }
        if (parsed.tail.empty())
        {
            // No arg — report current.
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Formation: {} (slot {}).",
                            FormationTypeName(ai->formation_type()),
                            ai->formation_slot())});
            return true;
        }
        FormationType const t = ParseFormationType(parsed.tail.c_str());
        ai->set_formation_type(t);
        PersistSquadState(bot, *ai);
        // Re-emit the follow with new offset so the bot snaps to the
        // new slot immediately rather than waiting for the next
        // /follow.
        if (ai->owner_command() == BotAI::OwnerCommand::Follow &&
            !ai->owner_target().IsEmpty())
        {
            Push(bot, BuildFollowIntent(bot, ai->owner_target()));
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Formation: set to {}.", FormationTypeName(t))});
        return true;
    }
    if (cmd == "spread")
    {
        // Shorthand for /formation spread.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (ai)
        {
            ai->set_formation_type(FormationType::Spread);
            if (ai->owner_command() == BotAI::OwnerCommand::Follow &&
                !ai->owner_target().IsEmpty())
                Push(bot, BuildFollowIntent(bot, ai->owner_target()));
        }
        Push(bot, WhisperIntent{sender->GetName(), "Spread: ring formation."});
        return true;
    }
    if (cmd == "tight" || cmd == "stack")
    {
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (ai)
        {
            ai->set_formation_type(FormationType::Tight);
            if (ai->owner_command() == BotAI::OwnerCommand::Follow &&
                !ai->owner_target().IsEmpty())
                Push(bot, BuildFollowIntent(bot, ai->owner_target()));
        }
        Push(bot, WhisperIntent{sender->GetName(), "Tight: stacking on leader."});
        return true;
    }
    if (cmd == "regroup")
    {
        // Re-emit follow with current formation — useful after wipe /
        // teleport / loading screen where the MotionMaster lost the
        // chase target. Doesn't change the formation type, just snaps
        // the bot back into its slot.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (ai && !ai->owner_target().IsEmpty())
        {
            Push(bot, BuildFollowIntent(bot, ai->owner_target()));
            Push(bot, WhisperIntent{sender->GetName(), "Regroup: re-snapping to slot."});
        }
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Regroup: no follow target — use /follow first."});
        }
        return true;
    }
    if (cmd == "slot")
    {
        // /slot <n>  — manually assign a formation slot (rare; usually
        // the address-resolver auto-assigns by squad order). Useful
        // when an owner wants slot 0 = main tank, slot 1 = co-tank,
        // etc., enforced explicitly.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai || args.empty())
        { Push(bot, WhisperIntent{sender->GetName(), "Slot: usage <n>"});
          return true; }
        const int requested = args[0];
        const uint8 s = (requested < 0 ? 0 :
                         requested > 15 ? 15 :
                         static_cast<uint8>(requested));
        ai->set_formation_slot(s);
        PersistSquadState(bot, *ai);
        if (ai->owner_command() == BotAI::OwnerCommand::Follow &&
            !ai->owner_target().IsEmpty())
            Push(bot, BuildFollowIntent(bot, ai->owner_target()));
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Slot: set to {}.", s)});
        return true;
    }
    if (cmd == "follow_distance" || cmd == "followdistance" || cmd == "fd")
    {
        // /follow_distance <n>  — pin per-bot follow distance (yards).
        // /follow_distance 0    — clear override; revert to default (5yd).
        // /follow_distance      — report current setting.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Follow_distance: bot not registered."});
            return true;
        }
        if (args.empty())
        {
            const float d = ai->follow_distance();
            Push(bot, WhisperIntent{sender->GetName(),
                d > 0.f ? fmt::format("Follow_distance: pinned at {:.1f}yd.", d)
                        : "Follow_distance: not pinned (default 5yd)."});
            return true;
        }
        // Cap to a sane range — 0 clears, 1..40 yd is the usable band. The
        // server's path-update loop chases the leader within ~40yd before the
        // follow chain resets; pinning past that is a no-op.
        float d = static_cast<float>(args[0]);
        if (d < 0.f) d = 0.f;
        if (d > 40.f) d = 40.f;
        ai->set_follow_distance(d);
        PersistSquadState(bot, *ai);
        Push(bot, WhisperIntent{sender->GetName(),
            d > 0.f ? fmt::format("Follow_distance: set to {:.1f}yd.", d)
                    : "Follow_distance: cleared (default 5yd)."});
        return true;
    }
    if (cmd == "follow_distance_all" || cmd == "fd_all" || cmd == "fdall")
    {
        // Mass /follow_distance. /follow_distance_all 0 clears across the group.
        if (args.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Follow_distance_all: usage <n>"});
            return true;
        }
        float d = static_cast<float>(args[0]);
        if (d < 0.f) d = 0.f;
        if (d > 40.f) d = 40.f;
        const uint32 changed = ForEachGroupBot(bot, [&](Player const* p)
        {
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            if (ai) ai->set_follow_distance(d);
        });
        Push(bot, WhisperIntent{sender->GetName(),
            d > 0.f ? fmt::format("Follow_distance_all: {:.1f}yd on {} bot(s).", d, changed)
                    : fmt::format("Follow_distance_all: cleared on {} bot(s).", changed)});
        return true;
    }
    if (cmd == "follow")
    {
        // Follow the sender at default distance, OR follow another player by
        // name if /follow <name> was used. Name lookup is case-insensitive.
        // Out-of-range / offline names whisper-back rather than silently
        // no-op'ing — owner often misspells.
        // /follow stop is an in-band alias for /unfollow.
        if (parsed.tail == "stop" || parsed.tail == "off")
        {
            Push(bot, StopMovementIntent{});
            Push(bot, WhisperIntent{sender->GetName(), "Follow: stopped."});
            // /follow stop also clears manual mode — owner has explicitly
            // asked the bot to stop tracking, autonomous behaviour can
            // pick up next tick.
            if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
                ai->exit_manual();
            return true;
        }
        if (args.size() >= 1)
        {
            // The arg parser folds quoted args, but unquoted single-word
            // names land as args[0] reinterpreted as numeric (0 if non-num).
            // So use the original raw text post-cmd via parsed.tail to grab
            // the literal name. Fallback: do a name lookup via ObjectAccessor.
            std::string name = parsed.tail;
            // Drop any leading whitespace.
            size_t a = name.find_first_not_of(" \t");
            if (a != std::string::npos) name = name.substr(a);
            if (!name.empty())
            {
                // Special keywords: "tank" / "healer" / "leader" walk the
                // group snapshot for the matching role / leadership flag.
                // Useful in groups where the owner doesn't want to type the
                // tank's name every dungeon, or wants the bot to chase the
                // raid leader's beacon during transitions.
                if (name == "tank" || name == "healer" || name == "leader")
                {
                    auto gsnap = Services::Snapshots().latest_group(bot->GetGUID().GetCounter());
                    if (!gsnap)
                    {
                        Push(bot, WhisperIntent{sender->GetName(),
                            "Follow: not in group."});
                        return true;
                    }
                    GroupMemberSummary const* tgt = nullptr;
                    if (name == "leader")
                    {
                        for (auto const& m : gsnap->members)
                            if (m.online && m.hp > 0 && m.guid == gsnap->leader
                                && m.guid != bot->GetGUID()) { tgt = &m; break; }
                    }
                    else
                    {
                        Role want = (name == "tank") ? Role::Tank : Role::Healer;
                        for (auto const& m : gsnap->members)
                            if (m.online && m.hp > 0 && m.role == want
                                && m.guid != bot->GetGUID()) { tgt = &m; break; }
                    }
                    if (!tgt)
                    {
                        Push(bot, WhisperIntent{sender->GetName(),
                            fmt::format("Follow: no {} in group.", name)});
                        return true;
                    }
                    Push(bot, BuildFollowIntent(bot, tgt->guid));
                    Push(bot, WhisperIntent{sender->GetName(),
                        fmt::format("Follow: now following {} ({}).", name, tgt->name)});
                    return true;
                }
                Player* tgt = ObjectAccessor::FindPlayerByName(name);
                if (!tgt)
                {
                    Push(bot, WhisperIntent{sender->GetName(),
                        fmt::format("Follow: player '{}' not found.", name)});
                    return true;
                }
                Push(bot, BuildFollowIntent(bot, tgt->GetGUID()));
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Follow: now following {}.", name)});
                return true;
            }
        }
        // Enter manual-mode Follow + auto-pick a free formation slot
        // if the current slot collides with another owned bot. The
        // FollowIntent emits AFTER the slot is set so the very first
        // chase uses the right offset.
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            AutoAssignFormationSlot(bot, *ai);
            ai->enter_manual(BotAI::OwnerCommand::Follow, sender->GetGUID(),
                             GameTime::GetGameTimeMS());
            ai->set_last_owner_name(sender->GetName());
            PersistSquadState(bot, *ai);
        }
        Push(bot, BuildFollowIntent(bot, sender->GetGUID()));
        return true;
    }
    if (cmd == "goto" || cmd == "moveto")
    {
        // /goto <x> <y>          - point destination on the CURRENT map.
        //                          Bot walks there, then holds (manual /stay).
        // /goto <map> <x> <y>    - CROSS-MAP travel goal. Synthesized as a
        //                          relocation objective; the full travel
        //                          pipeline (walk/taxi/portal/ship/areatrigger)
        //                          drives the bot there organically — never a
        //                          teleport. The bot resumes normal AI on
        //                          arrival (it's a journey, not a parking).
        float a1 = 0.f, a2 = 0.f, a3 = 0.f;
        const int n_args = std::sscanf(parsed.tail.c_str(), "%f %f %f", &a1, &a2, &a3);
        if (n_args == 3)
        {
            const uint32 dest_map = uint32(a1);
            if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
            {
                ai->set_manual_travel(dest_map, a2, a3, bot->GetPositionZ());
                ai->set_last_owner_name(sender->GetName());
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Goto: traveling to map {} ({:.0f}, {:.0f}) — "
                                "using flights/portals/ships as needed.",
                                dest_map, a2, a3)});
            }
            return true;
        }
        float x = a1, y = a2;
        if (n_args != 2)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Goto: <x> <y>  or  <map> <x> <y>"});
            return true;
        }
        // Bot's current Z; server snaps to terrain in MoveTo's path-find.
        float z = bot->GetPositionZ();
        Push(bot, MoveToIntent{x, y, z, /*run=*/true});
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            ai->enter_manual(BotAI::OwnerCommand::Stay, ObjectGuid::Empty,
                             GameTime::GetGameTimeMS());
            ai->set_last_owner_name(sender->GetName());
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Goto: moving to ({:.0f}, {:.0f}).", x, y)});
        return true;
    }
    if (cmd == "come" || cmd == "summon")
    {
        // Teleport the bot to the sender. The API guards against in-combat
        // and casting bots — caller can re-issue when the bot reports back.
        Push(bot, TeleportToIntent{sender->GetMapId(),
                                   sender->GetPositionX(),
                                   sender->GetPositionY(),
                                   sender->GetPositionZ(),
                                   sender->GetOrientation()});
        // After /come the owner usually wants the bot to stay nearby —
        // enter Follow manual mode so the bot doesn't immediately
        // resume questing across the map.
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            ai->enter_manual(BotAI::OwnerCommand::Follow, sender->GetGUID(),
                             GameTime::GetGameTimeMS());
            ai->set_last_owner_name(sender->GetName());
        }
        return true;
    }
    if (cmd == "come_all" || cmd == "summon_all" || cmd == "comeall" || cmd == "summonall")
    {
        // Teleport all registered group-mate bots to the sender. Each bot
        // may individually refuse (in-combat / casting) — same gates as the
        // single-bot variant. Bot dispatching the command is included.
        TeleportToIntent tp{sender->GetMapId(),
                            sender->GetPositionX(),
                            sender->GetPositionY(),
                            sender->GetPositionZ(),
                            sender->GetOrientation()};
        Push(bot, tp);
        uint32 broadcast_count = 1;
        if (Group const* grp = bot->GetGroup())
        {
            for (GroupReference const& itr : grp->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member || member == bot) continue;
                const BotId mid = member->GetGUID().GetCounter();
                if (!Services::Registry().has(mid)) continue;
                Push(member, tp);
                ++broadcast_count;
            }
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Come_all: ported {} bot(s).", broadcast_count)});
        return true;
    }
    if (cmd == "resume_all" || cmd == "resumeall" || cmd == "unpause_all")
    {
        // Clear paused_until_ms on all registered group-mate bots. Counterpart
        // to the auto-pause set by /pull (which delays engagement by N ms).
        // Useful when the puller wants to abort a /pull countdown early.
        uint32 cleared = 0;
        Services::Registry().for_each([&](BotId /*bid*/, BotRegistryEntry const& e)
        {
            const_cast<BotRegistryEntry&>(e).paused_until_ms.store(0u, std::memory_order_relaxed);
            ++cleared;
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Resume_all: cleared pause on {} bot(s).", cleared)});
        return true;
    }
    if (cmd == "unfollow_all" || cmd == "unfollowall" || cmd == "nofollow_all")
    {
        // Counterpart to /follow_all: clear follow movement on all group bots.
        // Each bot's StopMovement intent breaks the follow loop server-side.
        Push(bot, StopMovementIntent{});
        uint32 broadcast_count = 1;
        if (Group const* grp = bot->GetGroup())
        {
            for (GroupReference const& itr : grp->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member || member == bot) continue;
                const BotId mid = member->GetGUID().GetCounter();
                if (!Services::Registry().has(mid)) continue;
                Push(member, StopMovementIntent{});
                ++broadcast_count;
            }
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Unfollow_all: stopped {} bot(s).", broadcast_count)});
        return true;
    }
    if (cmd == "cdreset_all" || cmd == "cdresetall")
    {
        // Reset spell cooldowns across all group bots. Diagnostic — useful
        // when iterating on combat tuning to skip CD-gating.
        Push(bot, ResetCooldownsIntent{});
        uint32 broadcast_count = 1;
        if (Group const* grp = bot->GetGroup())
        {
            for (GroupReference const& itr : grp->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member || member == bot) continue;
                const BotId mid = member->GetGUID().GetCounter();
                if (!Services::Registry().has(mid)) continue;
                Push(member, ResetCooldownsIntent{});
                ++broadcast_count;
            }
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Cdreset_all: reset {} bot(s).", broadcast_count)});
        return true;
    }
    if (cmd == "stay_all" || cmd == "halt_all" || cmd == "stayall")
    {
        // Stop all registered group-mate bots. Counterpart to /come_all.
        Push(bot, StopMovementIntent{});
        uint32 broadcast_count = 1;
        if (Group const* grp = bot->GetGroup())
        {
            for (GroupReference const& itr : grp->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member || member == bot) continue;
                const BotId mid = member->GetGUID().GetCounter();
                if (!Services::Registry().has(mid)) continue;
                Push(member, StopMovementIntent{});
                ++broadcast_count;
            }
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Stay_all: stopped {} bot(s).", broadcast_count)});
        return true;
    }
    if (cmd == "follow_all" || cmd == "followall")
    {
        // /follow_all      → all group bots follow the sender
        // /follow_all <name> → all group bots follow the named target
        const ObjectGuid target_guid = [&]() -> ObjectGuid {
            std::string name = parsed.tail;
            size_t a = name.find_first_not_of(" \t");
            if (a != std::string::npos) name = name.substr(a);
            if (name.empty()) return sender->GetGUID();
            if (Player* p = ObjectAccessor::FindPlayerByName(name))
                return p->GetGUID();
            return ObjectGuid::Empty;
        }();
        if (target_guid.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Follow_all: target name not found."});
            return true;
        }
        // Each bot uses its own per-bot follow_distance preference (set via
        // /follow_distance N) — owner can mix tight melee and loose caster
        // distances within the same /follow_all command without rewiring.
        uint32 broadcast_count = 0;
        ForEachGroupBot(bot, [&](Player const* p)
        {
            if (Push(p, BuildFollowIntent(p, target_guid)))
                ++broadcast_count;
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Follow_all: {} bot(s) following.", broadcast_count)});
        return true;
    }
    if (cmd == "dismount_all" || cmd == "dismountall")
    {
        // Dismount every group bot at once. Useful at the start of a pull
        // when the raid was running mounted to position, or when a bot's
        // Mass Resurrection drops the group out of combat and people stay
        // grounded for the corpse-run.
        const uint32 n = BroadcastToGroup(bot, DismountIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Dismount_all: dismounted {} bot(s).", n)});
        return true;
    }
    if (cmd == "mount_all" || cmd == "mountall")
    {
        // Mount up across the group. Each bot picks its best context-aware
        // mount via the API (ground vs flying based on map / zone). Mount=0
        // tells the API to pick rather than force a specific entry.
        const uint32 n = BroadcastToGroup(bot, MountIntent{0});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Mount_all: mounted {} bot(s).", n)});
        return true;
    }
    if (cmd == "sit_all" || cmd == "sitall")
    {
        const uint32 n = BroadcastToGroup(bot, SetStandStateIntent{1});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Sit_all: {} bot(s) seated.", n)});
        return true;
    }
    if (cmd == "stand_all" || cmd == "standall")
    {
        const uint32 n = BroadcastToGroup(bot, SetStandStateIntent{0});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Stand_all: {} bot(s) standing.", n)});
        return true;
    }
    if (cmd == "release_all" || cmd == "releaseall")
    {
        // Mass corpse-release. Useful after a wipe — instead of whispering
        // each bot individually, owner fires one /release_all and the whole
        // group ghosts out together so corpse-runs sync up. Live bots are
        // no-ops server-side (release packet is gated on dead state).
        const uint32 n = BroadcastToGroup(bot, ReleaseCorpseIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Release_all: released {} bot(s).", n)});
        return true;
    }
    if (cmd == "revive_all" || cmd == "reviveall")
    {
        // Mass corpse-revive. Counterpart to /release_all — once the group
        // has run back to their corpses (or close enough), this fires the
        // accept-resurrection at-corpse path on every bot. Live bots are
        // a no-op server-side.
        const uint32 n = BroadcastToGroup(bot, ReviveAtCorpseIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Revive_all: revived {} bot(s).", n)});
        return true;
    }
    if (cmd == "dismiss_all" || cmd == "dismissall")
    {
        // Dismiss every pet across the group (hunters, warlocks, DKs, mage
        // water elemental). Useful before flight paths (pets don't board
        // taxis) or when the group needs minimal mob count for a stealth
        // section. The auto-MaintainPet rule re-summons OOC.
        const uint32 n = BroadcastToGroup(bot, DismissPetIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Dismiss_all: dismissed pets on {} bot(s).", n)});
        return true;
    }
    if (cmd == "hearth_all" || cmd == "hearthall")
    {
        // Mass hearth-out. Common end-of-session call — owner runs one
        // /hearth_all and every group bot starts the 10s hearth cast.
        // In-combat / in-instance gates are enforced API-side (Locked).
        const uint32 n = BroadcastToGroup(bot, HearthIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Hearth_all: {} bot(s) hearthing.", n)});
        return true;
    }
    if (cmd == "ready_all" || cmd == "readyall")
    {
        // Mass ready-check ack. Useful when the leader fires /readycheck and
        // the group is split between V2 bots and human players — the bots
        // would auto-ack (the InGroup layer handles it), but this lets the
        // owner force the ack early (e.g. clear a stuck ready-check window
        // after the bot's edge-trigger missed a refresh).
        const uint32 n = BroadcastToGroup(bot, GroupReadyResponseIntent{true});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Ready_all: {} bot(s) acked.", n)});
        return true;
    }
    if (cmd == "afk_all" || cmd == "afkall")
    {
        // Toggle AFK across the group. Useful for raid breaks — owner
        // fires /afk_all so the whole raid shows as AFK in the player frame.
        // Toggle, so a second /afk_all clears it.
        const uint32 n = BroadcastToGroup(bot, ToggleAfkIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Afk_all: toggled {} bot(s).", n)});
        return true;
    }
    if (cmd == "repair_all" || cmd == "repairall")
    {
        // Mass repair at sender's selected vendor. Each bot in the group
        // queues a RepairAllIntent against the same NPC; the API enforces
        // interact range per bot, so out-of-range bots fail gracefully.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Repair_all: select a repair NPC first."});
            return true;
        }
        const uint32 n = BroadcastToGroup(bot,
            RepairAllIntent{target->GetGUID(), /*from_guild_bank*/ false});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Repair_all: queued repair on {} bot(s).", n)});
        return true;
    }
    if (cmd == "sell_all" || cmd == "sellall")
    {
        // Mass sell-trash at sender's selected vendor. Each bot dumps its
        // grey-quality items via VendorSellTrashIntent. Same vendor; per-bot
        // interact range is API-gated so distance failures are silent.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Sell_all: select a vendor first."});
            return true;
        }
        const uint32 n = BroadcastToGroup(bot,
            VendorSellTrashIntent{target->GetGUID()});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Sell_all: queued trash-sell on {} bot(s).", n)});
        return true;
    }
    if (cmd == "buyfood_all" || cmd == "buypot_all" || cmd == "buybandage_all")
    {
        // Mass restock at sender's selected vendor. Same category mapping
        // as the single-bot variants. Per-bot gold check is enforced by
        // the API (skip when broke); per-bot interact range likewise.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Restock_all: select a vendor first."});
            return true;
        }
        uint8 subclass = 5;     // Food/Drink
        uint8 amount   = 20;
        if (cmd == "buypot_all")     { subclass = 1;  amount = 5;  }
        if (cmd == "buybandage_all") { subclass = 7;  amount = 10; }
        const uint32 n = BroadcastToGroup(bot,
            VendorBuyByCategoryIntent{target->GetGUID(), 0, subclass, amount});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Restock_all: queued {} bot(s).", n)});
        return true;
    }
    if (cmd == "buyall_all" || cmd == "buyallall")
    {
        // Mass top-off (food + potions + bandages) at sender's selected vendor.
        // Each bot fires three category buys in sequence; API queues them per
        // bot's intent queue.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Buyall_all: select a vendor first."});
            return true;
        }
        const ObjectGuid npc = target->GetGUID();
        const uint32 nf = BroadcastToGroup(bot, VendorBuyByCategoryIntent{npc, 0, /*food*/    5, 20});
        BroadcastToGroup(bot,                  VendorBuyByCategoryIntent{npc, 0, /*potion*/  1, 5});
        BroadcastToGroup(bot,                  VendorBuyByCategoryIntent{npc, 0, /*bandage*/ 7, 10});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Buyall_all: queued food/potion/bandage on {} bot(s).", nf)});
        return true;
    }
    if (cmd == "stay" || cmd == "halt")
    {
        Push(bot, StopMovementIntent{});
        // Manual-mode Stay — bot stops where it is and refuses to
        // autonomously engage / wander until /resume.
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            ai->enter_manual(BotAI::OwnerCommand::Stay, ObjectGuid::Empty,
                             GameTime::GetGameTimeMS());
            ai->set_last_owner_name(sender->GetName());
        }
        return true;
    }
    if (cmd == "hold")
    {
        // Like /stay but expressly allows in-combat self-defence —
        // /hold is "don't move" rather than "don't act". Useful for
        // healers who should keep healing the squad without chasing.
        Push(bot, StopMovementIntent{});
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            ai->enter_manual(BotAI::OwnerCommand::Hold, ObjectGuid::Empty,
                             GameTime::GetGameTimeMS());
            ai->set_last_owner_name(sender->GetName());
        }
        Push(bot, WhisperIntent{sender->GetName(), "Hold: holding position."});
        return true;
    }
    if (cmd == "resume")
    {
        // Explicit autonomous resume — clears manual mode immediately
        // (rather than waiting for the 30s window to expire).
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            ai->exit_manual();
            Push(bot, WhisperIntent{sender->GetName(), "Resume: autonomous."});
        }
        return true;
    }
    if (cmd == "squadrun" || cmd == "dungeonrun" || cmd == "run")
    {
        // Activate dungeon-run mode. Bot drives the dungeon execution
        // rules (idle:tank_pull_next, idle:dungeon_interrupt, etc).
        // /run pause / /run resume / /run stop sub-commands; bare
        // /squadrun = activate.
        // /run loop on|off|toggle controls the Phase J post-completion
        // re-queue (auto LFG-queue the same dungeon after the bot exits).
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        { Push(bot, WhisperIntent{sender->GetName(),
            "Squadrun: bot not registered."}); return true; }
        char const* tail = parsed.tail.c_str();
        if (parsed.tail == "pause")
        {
            ai->set_dungeon_run_mode(BotAI::DungeonRunMode::Paused);
            Push(bot, WhisperIntent{sender->GetName(), "Run: paused."});
        }
        else if (parsed.tail == "stop" || parsed.tail == "off")
        {
            ai->set_dungeon_run_mode(BotAI::DungeonRunMode::Off);
            Push(bot, WhisperIntent{sender->GetName(), "Run: stopped."});
        }
        else if (parsed.tail.rfind("loop", 0) == 0)
        {
            // "loop", "loop on", "loop off", "loop toggle".
            std::string const& t = parsed.tail;
            bool want = !ai->dungeon_loop_mode();   // toggle by default
            if (t == "loop on" || t == "loop 1" || t == "loop true")
                want = true;
            else if (t == "loop off" || t == "loop 0" || t == "loop false")
                want = false;
            ai->set_dungeon_loop_mode(want);
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Run: loop {} (last_dungeon={}).",
                            want ? "ON" : "off",
                            ai->last_lfg_dungeon_id())});
        }
        else
        {
            ai->set_dungeon_run_mode(BotAI::DungeonRunMode::Active);
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Run: active (tail='{}').", tail)});
        }
        ai->set_last_owner_name(sender->GetName());
        return true;
    }
    if (cmd == "squadstop")
    {
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            ai->set_dungeon_run_mode(BotAI::DungeonRunMode::Off);
            ai->set_bg_run_mode(BotAI::BgRunMode::Off);
            Push(bot, WhisperIntent{sender->GetName(),
                "Squadstop: dungeon-run + bg-run off."});
        }
        return true;
    }
    if (cmd == "bgrun")
    {
        // Activate BG-run mode. Bot drives the battleground play
        // (objective focus, role-by-slot, FC chase / escort etc.) per
        // the registered BattlegroundScript for the bg type id.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Bgrun: no AI."});
            return true;
        }
        if (parsed.tail == "pause")
        {
            ai->set_bg_run_mode(BotAI::BgRunMode::Paused);
            Push(bot, WhisperIntent{sender->GetName(), "Bgrun: paused."});
        }
        else if (parsed.tail == "stop" || parsed.tail == "off")
        {
            ai->set_bg_run_mode(BotAI::BgRunMode::Off);
            Push(bot, WhisperIntent{sender->GetName(), "Bgrun: stopped."});
        }
        else
        {
            ai->set_bg_run_mode(BotAI::BgRunMode::Active);
            Push(bot, WhisperIntent{sender->GetName(), "Bgrun: active."});
        }
        ai->set_last_owner_name(sender->GetName());
        return true;
    }
    if (cmd == "who" || cmd == "nearby")
    {
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Who: no snapshot."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Nearby: {} enemies, {} friends, {} attackers, {} objects.",
                        snap->combat.nearby_enemies.size(), snap->combat.nearby_friends.size(),
                        snap->combat.attackers.size(), snap->world_objects.nearby_objects.size())});
        return true;
    }
    if (cmd == "where_all" || cmd == "whereall")
    {
        // Diagnostic: walk all group bots and whisper their map+pos back to
        // the sender. Useful for finding scattered bots after a wipe or for
        // confirming everyone's in the right zone before a pull. One whisper
        // per bot (not per-bot summary roll-up) so the formatting fits the
        // standard whisper line length.
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Where[{}]: map={} zone={} pos=({:.1f},{:.1f},{:.1f})",
                            snap->identity.name, snap->position.map_id, snap->area.zone_id,
                            snap->position.x, snap->position.y, snap->position.z)});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Where_all: reported {} bot(s).", reported)});
        return true;
    }
    if (cmd == "state_all" || cmd == "stateall")
    {
        // Diagnostic: walk all group bots and whisper their state machine
        // (and last-fired rule) back to the sender. Counterpart to /state for
        // the whole group — quickly find which bot is stuck in Resurrecting
        // vs Idle, etc.
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            if (!ai) return;
            char const* rule = ai->last_rule_fired();
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("State[{}]: now={} prev={} last={}",
                            p->GetName(),
                            static_cast<int>(ai->state()),
                            static_cast<int>(ai->previous_state()),
                            rule ? rule : "(none)")});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("State_all: reported {} bot(s).", reported)});
        return true;
    }
    if (cmd == "queue_all" || cmd == "queueall")
    {
        // Diagnostic: walk all group bots and report their intent-queue depth.
        // Useful for spotting stalled bots — if the queue keeps growing, the
        // AI worker can't keep up with the executor or vice versa.
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            IntentQueue* q = Services::Registry().intents(p->GetGUID().GetCounter());
            if (!q) return;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Queue[{}]: {}/{}", p->GetName(),
                            q->approximate_size(), q->capacity())});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Queue_all: reported {} bot(s).", reported)});
        return true;
    }
    if (cmd == "wait_all" || cmd == "waitall")
    {
        // Mass pause across all group bots. Sets paused_until_ms on each
        // registered bot's registry entry. Useful for "freeze everyone for N
        // seconds" — coordinating a movement break, waiting for a flight path
        // to land, holding the group still during a sneak section.
        uint32 ms = args.size() >= 1 ? static_cast<uint32>(args[0]) : 1000u;
        if (ms > 60'000) ms = 60'000;
        const uint32_t until = GameTime::GetGameTimeMS() + ms;
        const uint32 paused = ForEachGroupBot(bot, [&](Player const* p)
        {
            const BotId target_id = p->GetGUID().GetCounter();
            Services::Registry().for_each([&](BotId bid, BotRegistryEntry const& e)
            {
                if (bid != target_id) return;
                const_cast<BotRegistryEntry&>(e).paused_until_ms
                    .store(until, std::memory_order_relaxed);
            });
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Wait_all: paused {} bot(s) for {}ms.", paused, ms)});
        return true;
    }
    if (cmd == "buff_all" && args.size() >= 1)
    {
        // /buff_all <spell_id>  → every group bot self-casts the buff. Distinct
        // from /cast_all (which targets sender's selection) — a buff usually
        // wants to land on the bot itself or the group's tank, not a hostile
        // target. The API gates on knowing the spell + having the resource.
        // Usage: pre-pull "Power Word: Fortitude on everyone".
        const uint32 spell_id = static_cast<uint32>(args[0]);
        const uint32 n = BroadcastToGroup(bot, CastSpellIntent{spell_id, ObjectGuid::Empty});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Buff_all: queued buff {} on {} bot(s).", spell_id, n)});
        return true;
    }
    if (cmd == "buffclass_all" || cmd == "buff_class_all")
    {
        // /buffclass_all → each group bot casts ITS OWN class self-buff via
        // `ClassSelfBuff(cls)` lookup. Smarter than /buff_all <id> when the
        // group has mixed classes — Priest fires Fortitude, Druid fires MotW,
        // Mage fires Arcane Intellect, etc. all from one command. Bots whose
        // class has no raid buff (Paladin, DK, Hunter) silently no-op.
        uint32 fired = 0;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            const uint32 buff = ClassSelfBuff(snap->identity.cls);
            if (!buff) return;
            // Cast on bot itself with empty target — class self-buff is a
            // raid-wide aoe in modern WoW (single emit ripples to group).
            if (Push(p, CastSpellIntent{buff, ObjectGuid::Empty}))
                ++fired;
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Buffclass_all: {}/{} bot(s) cast their class buff.", fired, reported)});
        return true;
    }
    if (cmd == "cast_all" && args.size() >= 1)
    {
        // Mass spell cast across the group. /cast_all <spell_id> [target_guid].
        // Owner-driven coordination — useful for synchronized burst (Time Warp
        // proxy: every shaman pops Bloodlust at once), mass-utility (every
        // mage drops Frost Nova on the same target), or testing.
        // Per-bot: API gates on knowing the spell, having the resource, etc.
        // Unknown spell on a given bot fails Result::NotKnown (silent drop).
        const uint32 spell_id = static_cast<uint32>(args[0]);
        // Optional second arg is the target — a unit GUID or 0/missing for
        // self-cast. Owner can also leave the target off and let the per-bot
        // current target resolve via cast(spell_id, ObjectGuid::Empty).
        ObjectGuid target = ObjectGuid::Empty;
        if (Unit const* sel = sender->GetSelectedUnit())
            target = sel->GetGUID();
        const uint32 n = BroadcastToGroup(bot, CastSpellIntent{spell_id, target});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Cast_all: queued spell {} on {} bot(s).", spell_id, n)});
        return true;
    }
    if (cmd == "stop_all" || cmd == "stopall" || cmd == "passive_all")
    {
        // Mass disengage. Counterpart to /pull: every group bot drops attack
        // and stops casting. Useful for aborting a wipe-in-progress.
        BroadcastToGroup(bot, StopAttackIntent{});
        const uint32 n = BroadcastToGroup(bot, CancelCastIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Stop_all: dropped attack/cast on {} bot(s).", n)});
        return true;
    }
    if (cmd == "pull")
    {
        // Coordinated raid pull. Owner says "pull" with the next pull's
        // target selected. Bot pauses N seconds (default 3) before engaging,
        // matching the WoW pull-timer macro convention. Owners can call /pull
        // 5 for a longer countdown when the puller needs to walk to position.
        // When the bot is a raid leader/assistant, also broadcasts a raid
        // warning ("Pulling <name> in Ns!") so the rest of the raid sees the
        // standard pull-timer cue.
        Unit const* target = sender->GetSelectedUnit();
        if (!target || target->GetGUID() == bot->GetGUID())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Pull: select an enemy first."});
            return true;
        }
        uint32 ms = args.size() >= 1 ? static_cast<uint32>(args[0] * 1000ULL) : 3000u;
        if (ms > 60'000) ms = 60'000;
        const uint64 id = bot->GetGUID().GetCounter();
        Services::Registry().for_each([&](BotId bid, BotRegistryEntry const& e)
        {
            if (bid != id) return;
            const uint32_t until = GameTime::GetGameTimeMS() + ms;
            const_cast<BotRegistryEntry&>(e).paused_until_ms.store(until, std::memory_order_relaxed);
        });
        // Broadcast a raid warning if the bot can. raid_warning() bails Locked
        // when not allowed, which is fine — solo bots and non-officers just
        // skip the broadcast and still fire the pull on their end.
        Push(bot, RaidWarningIntent{
            fmt::format("Pulling {} in {}s!", target->GetName(), ms / 1000)});
        Push(bot, StartAttackIntent{target->GetGUID()});

        // Broadcast to all OTHER registered bots in the same group: pause
        // them for the same window then engage the same target. Caller bots
        // already had their pause set above. Healers/casters benefit from
        // the synchronized pull window so they can pre-position; melee bots
        // sync their burst CDs.
        if (Group const* grp = bot->GetGroup())
        {
            const ObjectGuid target_guid = target->GetGUID();
            const uint32_t until = GameTime::GetGameTimeMS() + ms;
            uint32 broadcast_count = 0;
            for (GroupReference const& itr : grp->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member || member == bot) continue;
                const BotId mid = member->GetGUID().GetCounter();
                if (!Services::Registry().has(mid)) continue;
                Services::Registry().for_each([&](BotId bid, BotRegistryEntry const& e)
                {
                    if (bid != mid) return;
                    const_cast<BotRegistryEntry&>(e).paused_until_ms
                        .store(until, std::memory_order_relaxed);
                });
                Push(member, StartAttackIntent{target_guid});
                ++broadcast_count;
            }
            if (broadcast_count > 0)
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Pull: synced to {} group bot(s).", broadcast_count)});
        }

        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Pull: engaging {} in {}ms.", target->GetName(), ms)});
        return true;
    }
    if (cmd == "trinkets")
    {
        // Owner diagnostic for the on-use trinket auto-fire rule. Reports
        // both trinket slots: item entry, on-use spell id (0 = no on-use),
        // and cooldown remaining. Helps debug "why didn't my trinket fire"
        // (typically: no on-use effect, or shared cooldown still ticking).
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot."});
            return true;
        }
        constexpr uint8 TRINKET1 = 13, TRINKET2 = 14;
        const BotSnapshotView v{*snap};
        for (uint8 slot : {TRINKET1, TRINKET2})
        {
            auto const& eq = snap->inventory.equipped[slot];
            if (eq.entry == 0)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Trinket{}: (empty)", slot - 12)});
                continue;
            }
            if (eq.on_use_spell_id == 0)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Trinket{}: entry={} (no on-use)", slot - 12, eq.entry)});
                continue;
            }
            const auto cd = v.cd_remaining(eq.on_use_spell_id);
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Trinket{}: entry={} use_spell={} cd={}ms",
                            slot - 12, eq.entry, eq.on_use_spell_id, cd.count())});
        }
        return true;
    }
    if (cmd == "stock" || cmd == "consumables")
    {
        // Owner diagnostic for the auto-restock rules. Reports how many
        // food/drink, potions, and bandages the bot currently has — so a
        // sender can confirm "yes, the auto-restock at vendor fired" or
        // "the bot ran out of pots, top them up before the raid."
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Stock: food/drink={} potions={} bandages={} bag_free={}",
                        snap->consumables.food_drink_count, snap->consumables.potion_count,
                        snap->consumables.bandage_count, snap->bags.bag_free_slots)});
        return true;
    }
    if (cmd == "sheet" || cmd == "stats2")
    {
        // Combat-stat character sheet. Reports rating-derived percents from
        // the snapshot (post-DR, x100 fixed-point). PvP fields appear only
        // when nonzero so PvE bots get a clean line. /stats already exists
        // for system status — /sheet is the in-character version.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Sheet: crit={:.2f}% haste={:.2f}% mastery={:.2f}% versatility={:.2f}%",
                        snap->secondary_stats.crit_pct_x100        / 100.0,
                        snap->secondary_stats.haste_pct_x100       / 100.0,
                        snap->secondary_stats.mastery_pct_x100     / 100.0,
                        snap->secondary_stats.versatility_pct_x100 / 100.0)});
        if (snap->secondary_stats.resilience_pct_x100 != 0 || snap->secondary_stats.pvp_power_pct_x100 != 0)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Sheet (PvP): resilience={:.2f}% pvp_power={:.2f}%",
                            snap->secondary_stats.resilience_pct_x100 / 100.0,
                            snap->secondary_stats.pvp_power_pct_x100  / 100.0)});
        return true;
    }
    if (cmd == "queues")
    {
        // Owner diagnostic: report any active BG queues + ports awaiting
        // accept. snapshot.bg_queues is populated from the player's BG
        // queue slots; entries with invited_to_instance != 0 mean a port
        // is open ("press the dialog"), the auto-port rule fires on those.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot."});
            return true;
        }
        if (snap->bg.queues.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                snap->bg.in_battleground ? "Queues: (none — in battleground)"
                                         : "Queues: (none queued)"});
            return true;
        }
        const uint32 now_sec = GameTime::GetGameTime();
        for (auto const& q : snap->bg.queues)
        {
            const uint32 waited = now_sec - q.joined_at_sec;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Queue: bg_type={} waited={}s {}",
                            q.bg_type_id, waited,
                            q.invited_to_instance ? "[PORT READY]" : "")});
        }
        return true;
    }
    if (cmd == "wait" || cmd == "sleep")
    {
        // /wait <ms>  — pause intent execution for the bot for N ms.
        // Defaults to 1000 if no arg. Used for coordinated pull timing
        // ("hold for 3 seconds, then pull"). Caps at 60s as a safety net.
        uint32 ms = args.size() >= 1 ? args[0] : 1000u;
        if (ms > 60'000) ms = 60'000;
        // Reach into the registry entry's atomic via the typed-with helper
        // we already have for loot. The paused field is per-entry.
        const uint64 id = bot->GetGUID().GetCounter();
        // No `with_entry` accessor exists; do the safe map traversal here
        // by adding a one-off via for_each (read-only — but we mutate the
        // atomic which is fine under shared_lock since only the entry's
        // atomic value mutates, not the map structure).
        bool ok = false;
        Services::Registry().for_each([&](BotId bid, BotRegistryEntry const& e)
        {
            if (bid != id) return;
            const uint32_t until = GameTime::GetGameTimeMS() + ms;
            // const_cast is safe — the atomic itself is the only mutation,
            // and atomics tolerate concurrent writers by design.
            const_cast<BotRegistryEntry&>(e).paused_until_ms.store(until, std::memory_order_relaxed);
            ok = true;
        });
        Push(bot, WhisperIntent{sender->GetName(),
            ok ? fmt::format("Wait: paused for {}ms.", ms)
               : "Wait: bot not registered."});
        return true;
    }
    if (cmd == "cdreset")
    {
        // Clear all spell cooldowns. Diagnostic; mostly used to retest a
        // rotation back-to-back without waiting for the server CD walls.
        Push(bot, ResetCooldownsIntent{});
        Push(bot, WhisperIntent{sender->GetName(), "Cdreset: cleared cooldowns."});
        return true;
    }
    if (cmd == "setspec")
    {
        // /setspec <spec_id>  — applies ChrSpecialization.db2 id; rejected
        // when bot is in combat. Same args[0] parser shape as other id
        // commands.
        if (args.size() < 1)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Setspec: <spec_id>"});
            return true;
        }
        Push(bot, ActivateSpecIntent{static_cast<uint32>(args[0])});
        // Auto-apply Blizzard's starter build for the new spec — without this,
        // the bot has no talent picks for the just-activated spec and the APL
        // thinks it doesn't know talented spells. Visitor processes in order:
        // ActivateSpec runs first, then ApplyStarterTalents acts on the new
        // active config.
        Push(bot, ApplyStarterTalentsIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Setspec: queued switch to spec {} + starter talents.", args[0])});
        return true;
    }
    if (cmd == "setspec_all" || cmd == "setspecall")
    {
        // Mass /setspec — every group bot switches to the named spec id.
        // Per-bot: API rejects InvalidTarget if the spec id doesn't belong to
        // the bot's class (e.g. trying to set spec 102 / Balance on a Warrior).
        // Combat-gated per bot.
        if (args.size() < 1)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Setspec_all: <spec_id>"});
            return true;
        }
        const uint32 spec_id = static_cast<uint32>(args[0]);
        // Send Activate then Apply — same order as the single-bot variant.
        BroadcastToGroup(bot, ActivateSpecIntent{spec_id});
        const uint32 n = BroadcastToGroup(bot, ApplyStarterTalentsIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Setspec_all: queued spec {} + starter talents on {} bot(s).",
                        spec_id, n)});
        return true;
    }
    if (cmd == "upgrades")
    {
        // /upgrades         — report count of pending bag→slot upgrades
        // /upgrades apply   — equip every strict-ilvl upgrade in one batch
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Upgrades: no snapshot."});
            return true;
        }
        if (parsed.tail != "apply")
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Upgrades: {} pending. /upgrades apply to equip.",
                            snap->bags.upgrades_pending)});
            return true;
        }
        size_t emitted = 0;
        // Re-walk bag_items applying the same predicate as the snapshot's
        // count. Each EquipItem packs the source bag/slot and target equip
        // slot. Server enforces eligibility on apply (level/class), so any
        // mismatch surfaces as Result::Locked which we silently swallow —
        // the bot can re-issue once the gate clears.
        for (auto const& it : snap->inventory.bag_items)
        {
            if (it.equip_slot == 0xFF) continue;
            if (it.item_level == 0)    continue;
            if (it.quality == 0)       continue;
            EquippedItem const& cur = snap->inventory.equipped[it.equip_slot];
            if (cur.entry != 0 && it.item_level <= cur.item_level) continue;
            Push(bot, EquipItemIntent{it.bag, it.slot, it.equip_slot});
            ++emitted;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Upgrades: queued {} swaps.", emitted)});
        return true;
    }
    if (cmd == "upgrades_all" || cmd == "upgradesall")
    {
        // Mass /upgrades. Without arg → report per-bot count; with `apply` →
        // equip every pending strict-ilvl upgrade across the group. Bots that
        // dinged together after a dungeon clear can be batch-upgraded with
        // one command.
        const bool apply_mode = (parsed.tail == "apply");
        size_t total_pending = 0;
        size_t total_emitted = 0;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            const size_t pending = snap->bags.upgrades_pending;
            total_pending += pending;
            if (!apply_mode)
            {
                if (pending > 0)
                    Push(bot, WhisperIntent{sender->GetName(),
                        fmt::format("Upgrades[{}]: {} pending.", snap->identity.name, pending)});
                return;
            }
            for (auto const& it : snap->inventory.bag_items)
            {
                if (it.equip_slot == 0xFF) continue;
                if (it.item_level == 0)    continue;
                if (it.quality == 0)       continue;
                EquippedItem const& cur = snap->inventory.equipped[it.equip_slot];
                if (cur.entry != 0 && it.item_level <= cur.item_level) continue;
                Push(p, EquipItemIntent{it.bag, it.slot, it.equip_slot});
                ++total_emitted;
            }
        });
        Push(bot, WhisperIntent{sender->GetName(),
            apply_mode
                ? fmt::format("Upgrades_all: queued {} swaps across {} bot(s).",
                              total_emitted, reported)
                : fmt::format("Upgrades_all: {} pending across {} bot(s). /upgrades_all apply to equip.",
                              total_pending, reported)});
        return true;
    }
    if (cmd == "train_all" || cmd == "trainall")
    {
        // Mass-train at sender's selected trainer. Each group bot fires a
        // bulk-train; the API enforces interact range and trainer eligibility
        // per bot, so out-of-range bots fail silently.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Train_all: select a trainer first."});
            return true;
        }
        const uint32 n = BroadcastToGroup(bot, TrainerBuyAllIntent{target->GetGUID()});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Train_all: queued bulk-train on {} bot(s).", n)});
        return true;
    }
    if (cmd == "queue")
    {
        // Per-bot intent-queue diagnostic. Reports queue size + capacity
        // so the owner can see whether the AI worker is stalled (queue
        // grows) or healthy (queue empties between ticks).
        const uint64 id = bot->GetGUID().GetCounter();
        IntentQueue* q = Services::Registry().intents(id);
        if (!q)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Queue: bot not registered."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Queue: {}/{} slots used.",
                        q->approximate_size(), q->capacity())});
        return true;
    }
    if (cmd == "last")
    {
        // Diagnostic: report the last APL/idle rule that fired + current
        // BotState. Useful when iterating on combat tuning to see whether
        // the rotation is even reaching the rule the owner expects.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Last: bot not registered."});
            return true;
        }
        char const* rule = ai->last_rule_fired();
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Last: rule={} state={}", rule ? rule : "(none)",
                        static_cast<int>(ai->state()))});
        return true;
    }
    if (cmd == "history")
    {
        // /history  → dump the last ~16 distinct rules fired (oldest first).
        // Useful for debugging "why did the bot do X 5 ticks ago?" — set_last_rule_fired
        // ring-buffers each named rule. Dedup against immediate-prior rule means
        // a single rule firing every tick won't drown the history.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "History: bot not registered."});
            return true;
        }
        size_t count = 0;
        ai->for_each_rule_history([&](size_t /*idx*/, char const* name)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("  [{}] {}", count, name)});
            ++count;
        });
        if (count == 0)
            Push(bot, WhisperIntent{sender->GetName(), "History: empty."});
        else
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("History: {} entries (oldest first).", count)});
        return true;
    }
    if (cmd == "state")
    {
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "State: bot not registered."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("State: now={} prev={}",
                        static_cast<int>(ai->state()),
                        static_cast<int>(ai->previous_state()))});
        return true;
    }
    if (cmd == "perf")
    {
        // Process-wide PerfCounters snapshot. Not per-bot — covers all V2
        // activity in the worldserver process. Useful for monitoring at
        // 2000-bot scale: tick_p99 / world_p99 spike when the world
        // thread saturates; path_outcomes track the move_to nav-mesh
        // gate (NoPath / FarFromPolyEnd / etc.).
        auto s = Services::Perf().snapshot();
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Perf: ticks={} emit={} exec={} fail={} drop={} exc={}",
                        s.ticks_total, s.intents_emitted_total,
                        s.intents_executed_total, s.intents_failed_total,
                        s.intents_dropped_total, s.exceptions_total)});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format(" tick_p50={}ms tick_p99={}ms world_p50={}ms world_p99={}ms",
                        int64_t(s.tick_p50.count()), int64_t(s.tick_p99.count()),
                        int64_t(s.world_p50.count()), int64_t(s.world_p99.count()))});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format(" path: ok={} nopath={} fps_start={} fps_end={} incomplete={} short={}",
                        s.path_outcomes[0], s.path_outcomes[1], s.path_outcomes[2],
                        s.path_outcomes[3], s.path_outcomes[4], s.path_outcomes[5])});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format(" qfill: req={} jit={} done={} p50={}ms p99={}ms",
                        s.queue_fill_requests_total,
                        s.queue_fill_jit_spawned_total,
                        s.queue_fill_completions_total,
                        int64_t(s.queue_fill_p50.count()),
                        int64_t(s.queue_fill_p99.count()))});
        return true;
    }
    if (cmd == "tickperf" || cmd == "tp")
    {
        // Real-time TickPerf snapshot of the in-flight 60s window — same
        // data the [PlayerbotV2] TickPerf log line emits, but available
        // on demand instead of waiting for the next emit. Ms values are
        // window TOTALS; the avg division below mirrors the log emit
        // format so the operator reads ms-per-tick directly. n == 0
        // means no ticks have accumulated yet (process just started or
        // window just reset); guard the divide to avoid NaN.
        auto tp = V2::Module::instance().tickperf_snapshot();
        const uint32 n = tp.ticks_in_window;
        if (n == 0)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "TickPerf: no ticks accumulated in current window yet."});
            return true;
        }
        const uint32 snap_total = tp.snap_built_count + tp.snap_skipped_count;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("TickPerf (avg ms over {} ticks): "
                        "snap={:.2f} (setup={:.2f} build={:.2f} group={:.2f})",
                        n,
                        double(tp.snap_ms_total) / n,
                        double(tp.snap_setup_ms_total) / n,
                        double(tp.snap_build_ms_total) / n,
                        double(tp.group_build_ms_total) / n)});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format(" bgport={:.2f} drain={:.2f} session={:.2f} "
                        "pop={:.2f} sched={:.2f} | total={:.2f} max={}ms",
                        double(tp.bgport_ms_total) / n,
                        double(tp.drain_ms_total) / n,
                        double(tp.session_ms_total) / n,
                        double(tp.pop_ms_total) / n,
                        double(tp.sched_ms_total) / n,
                        double(tp.total_ms_total) / n,
                        tp.total_ms_max)});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format(" throughput: snaps_built={} snaps_skipped={} "
                        "(build_rate={:.1f}%) intents_drained={} ({:.1f}/tick)",
                        tp.snap_built_count, tp.snap_skipped_count,
                        snap_total > 0 ? 100.0 * double(tp.snap_built_count) / double(snap_total) : 0.0,
                        tp.intents_drained,
                        double(tp.intents_drained) / double(n))});
        return true;
    }
    if (cmd == "rep" || cmd == "reputation")
    {
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Rep: no snapshot."});
            return true;
        }
        // Sort by standing descending; report top 5 with their faction id
        // and rank label. Snapshot vector is small (≤60), in-place sort
        // copy is cheap.
        std::vector<BotSnapshot::ReputationEntry> sorted = snap->progression.reputations;
        std::sort(sorted.begin(), sorted.end(),
                  [](auto const& a, auto const& b) { return a.standing > b.standing; });
        static char const* kRankNames[] = { "Hated","Hostile","Unfriendly","Neutral",
                                             "Friendly","Honored","Revered","Exalted","Paragon" };
        std::string line = "Rep:";
        const size_t cap = std::min<size_t>(sorted.size(), 5);
        for (size_t i = 0; i < cap; ++i)
        {
            char const* rname = sorted[i].rank < 9 ? kRankNames[sorted[i].rank] : "?";
            line += fmt::format(" {}={}({})",
                                sorted[i].faction_id, sorted[i].standing, rname);
        }
        if (sorted.empty())
            line += " (none)";
        Push(bot, WhisperIntent{sender->GetName(), line});
        return true;
    }
    if (cmd == "currency" || cmd == "wallet")
    {
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Currency: no snapshot."});
            return true;
        }
        if (snap->progression.currencies.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Currency: wallet empty."});
            return true;
        }
        // Whisper line ~250 chars; ~15 entries typical at "1234:567 " each.
        // Group by 6 per line so the chunk doesn't truncate.
        std::string line = "Currency:";
        size_t printed = 0;
        for (auto const& c : snap->progression.currencies)
        {
            line += fmt::format(" {}={}", c.currency_id, c.quantity);
            if (++printed % 6 == 0)
            {
                Push(bot, WhisperIntent{sender->GetName(), line});
                line = "Currency:";
            }
        }
        if (line != "Currency:")
            Push(bot, WhisperIntent{sender->GetName(), line});
        return true;
    }
    if (cmd == "honor")
    {
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Honor: no snapshot."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Honor lvl {} ({}/{}) HK today {} yest {} life {}",
                        snap->identity.honor_level, snap->identity.honor_xp, snap->identity.honor_xp_for_next,
                        snap->identity.honor_kills_today, snap->identity.honor_kills_yesterday,
                        snap->identity.honor_kills_lifetime)});
        return true;
    }
    if (cmd == "skip")
    {
        // Drop the head of pending_loot — used when the bot is wedged on a
        // corpse it can't reach. Walks the loot queue under its mutex.
        const uint64 id = bot->GetGUID().GetCounter();
        bool dropped = false;
        Services::Registry().with_loot(id, [&](std::deque<ObjectGuid>& q)
        {
            if (!q.empty()) { q.pop_front(); dropped = true; }
        });
        Push(bot, WhisperIntent{sender->GetName(),
            dropped ? "Skip: dropped current corpse." : "Skip: loot queue empty."});
        return true;
    }
    if (cmd == "skip_all" || cmd == "skipall")
    {
        // Mass /skip — drop the head of each bot's loot queue. Useful when a
        // shared corpse-source despawned and the whole group is wedged on it.
        size_t dropped = 0;
        const uint32 visited = ForEachGroupBot(bot, [&](Player const* p)
        {
            const BotId id = p->GetGUID().GetCounter();
            Services::Registry().with_loot(id, [&](std::deque<ObjectGuid>& q)
            {
                if (!q.empty()) { q.pop_front(); ++dropped; }
            });
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Skip_all: dropped {} corpses across {} bot(s).", dropped, visited)});
        return true;
    }
    if (cmd == "clearloot")
    {
        // Drop ALL queued corpses. For wipe recovery when 30+ corpses pile up.
        const uint64 id = bot->GetGUID().GetCounter();
        size_t cleared = 0;
        Services::Registry().with_loot(id, [&](std::deque<ObjectGuid>& q)
        {
            cleared = q.size();
            q.clear();
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Clearloot: dropped {} corpses.", cleared)});
        return true;
    }
    if (cmd == "clearloot_all" || cmd == "clearlootall")
    {
        // Mass loot-queue purge across all group bots. Useful after a wipe
        // wherever per-bot tag queues built up to dozens of corpses while
        // the bots couldn't reach them. Clearing frees the bot to resume
        // questing/idle work without endless move-to-corpse churn.
        size_t total = 0;
        const uint32 visited = ForEachGroupBot(bot, [&](Player const* p)
        {
            const BotId id = p->GetGUID().GetCounter();
            Services::Registry().with_loot(id, [&](std::deque<ObjectGuid>& q)
            {
                total += q.size();
                q.clear();
            });
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Clearloot_all: dropped {} corpses across {} bot(s).",
                        total, visited)});
        return true;
    }
    if (cmd == "focus")
    {
        // /focus              — pin sender's GUID as focus
        // /focus <playername> — pin named player's GUID as focus
        // /focus clear        — drop the focus pin
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Focus: bot not registered."});
            return true;
        }
        if (parsed.tail == "clear" || parsed.tail == "off")
        {
            ai->set_focus_target(ObjectGuid::Empty);
            Push(bot, WhisperIntent{sender->GetName(), "Focus: cleared."});
            return true;
        }
        ObjectGuid g;
        if (parsed.tail.empty())
        {
            // /focus by itself reports current focus rather than implicitly
            // pinning sender — least surprising default. /focus me pins
            // sender explicitly.
            ObjectGuid current = ai->focus_target();
            Push(bot, WhisperIntent{sender->GetName(),
                current.IsEmpty()
                    ? "Focus: no pin (use /focus me or /focus <name>)"
                    : fmt::format("Focus: pinned on {}", current.ToString())});
            return true;
        }
        if (parsed.tail == "me" || parsed.tail == "self")
            g = sender->GetGUID();
        else
        {
            Player* lookup = ObjectAccessor::FindPlayerByName(parsed.tail);
            if (!lookup)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Focus: player '{}' not found.", parsed.tail)});
                return true;
            }
            g = lookup->GetGUID();
        }
        ai->set_focus_target(g);
        Push(bot, WhisperIntent{sender->GetName(), "Focus: pinned."});
        return true;
    }
    if (cmd == "target")
    {
        // /target <creature_entry>  — attack the nearest enemy in
        // nearby_enemies whose entry matches. Useful for elite-named macros
        // where the owner knows the entry but the unit is buried in clutter.
        if (args.size() < 1)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Target: <creature_entry>"});
            return true;
        }
        const uint32 want_entry = args[0];
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Target: no snapshot."});
            return true;
        }
        // Closest matching entry by squared distance from bot's position.
        const float bx = snap->position.x, by = snap->position.y, bz = snap->position.z;
        ObjectGuid pick;
        float best = std::numeric_limits<float>::max();
        for (auto const& u : snap->combat.nearby_enemies)
        {
            if (u.entry != want_entry) continue;
            if (u.hp <= 0) continue;
            const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
            const float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 < best) { best = d2; pick = u.guid; }
        }
        if (pick.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Target: entry {} not in range.", want_entry)});
            return true;
        }
        Push(bot, StartAttackIntent{pick});
        return true;
    }
    if (cmd == "assist")
    {
        // /assist                — target what the sender is targeting
        // /assist <playername>   — target what the named player is targeting
        // Mirrors the WoW "assist" macro chain. Picks up cross-group targets
        // since we don't gate on group membership; the bot's own faction
        // checks happen server-side when the attack starts.
        Player* anchor = sender;
        if (!parsed.tail.empty())
        {
            Player* lookup = ObjectAccessor::FindPlayerByName(parsed.tail);
            if (!lookup)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Assist: player '{}' not found.", parsed.tail)});
                return true;
            }
            anchor = lookup;
        }
        ObjectGuid tgt = anchor->GetTarget();
        if (tgt.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Assist: anchor has no target."});
            return true;
        }
        Push(bot, StartAttackIntent{tgt});
        return true;
    }
    if (cmd == "assist_all" || cmd == "assistall")
    {
        // Mass /assist. Each group bot picks up the anchor's target. Useful
        // for "everyone focus down what I'm hitting" pulls.
        Player* anchor = sender;
        if (!parsed.tail.empty())
        {
            Player* lookup = ObjectAccessor::FindPlayerByName(parsed.tail);
            if (!lookup)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Assist_all: player '{}' not found.", parsed.tail)});
                return true;
            }
            anchor = lookup;
        }
        ObjectGuid tgt = anchor->GetTarget();
        if (tgt.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Assist_all: anchor has no target."});
            return true;
        }
        const uint32 n = BroadcastToGroup(bot, StartAttackIntent{tgt});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Assist_all: {} bot(s) engaging.", n)});
        return true;
    }
    if (cmd == "attack" || cmd == "kill" || cmd == "engage")
    {
        // Attack the sender's selected target. Manual-mode Engage so
        // the bot prioritises this kill over autonomous goals; clears
        // automatically when the target dies (in_combat→Idle transition
        // detects empty victim and resets owner_command).
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Attack: select a target first."});
            return true;
        }
        Push(bot, StartAttackIntent{target->GetGUID()});
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            ai->enter_manual(BotAI::OwnerCommand::Engage, target->GetGUID(),
                             GameTime::GetGameTimeMS());
            ai->set_last_owner_name(sender->GetName());
        }
        return true;
    }
    if (cmd == "interrupt" || cmd == "kick")
    {
        // Cast the bot's class-resolved interrupt at the sender's
        // selected target. /interrupt with no selection picks the bot's
        // current target as a fallback. Owner can override with
        // /interrupt <name>. Only fires if the spell is in the bot's
        // spellbook (gates fresh / low-level chars).
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Interrupt: select a casting target first."});
            return true;
        }
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Interrupt: no snapshot."});
            return true;
        }
        const uint32 spell_id = ClassInterrupt(snap->identity.cls, snap->identity.spec);
        if (spell_id == 0)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Interrupt: this class/spec has no interrupt."});
            return true;
        }
        // knows_spell() check via snapshot, not Player. The spellbook
        // is published once per snapshot; a level-too-low bot with the
        // class spell ungranted yet bounces with Result::NotKnown if
        // we skip this gate.
        bool known = false;
        for (uint32 sid : snap->spellbook.known_spells) if (sid == spell_id) { known = true; break; }
        if (!known)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Interrupt: spell {} not yet learned.", spell_id)});
            return true;
        }
        Push(bot, CastSpellIntent{spell_id, target->GetGUID()});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Interrupt: kicking {}.", target->GetName())});
        return true;
    }
    if (cmd == "cc")
    {
        // Class-resolved single-target CC at the sender's selection.
        // /cc with no symbol arg: just cast on selection.
        // /cc <symbol>: also place the raid icon on the same target so
        // group sees the Sheep / Sap / Hex marker. Mirrors what a
        // human MC team would type in voice ("sheep skull please").
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Cc: select a target first."});
            return true;
        }
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        { Push(bot, WhisperIntent{sender->GetName(), "Cc: no snapshot."});
          return true; }
        const uint32 spell_id = ClassCC(snap->identity.cls, snap->identity.spec);
        if (spell_id == 0)
        { Push(bot, WhisperIntent{sender->GetName(),
            "Cc: this class has no canonical CC."}); return true; }
        bool known = false;
        for (uint32 sid : snap->spellbook.known_spells) if (sid == spell_id) { known = true; break; }
        if (!known)
        { Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Cc: spell {} not yet learned.", spell_id)});
          return true; }
        // Optional raid-icon symbol so /cc moon = poly the moon target.
        if (!parsed.tail.empty())
        {
            uint8 idx = 0xFF;
            std::string const& sym = parsed.tail;
            if      (sym == "star")     idx = 0;
            else if (sym == "circle")   idx = 1;
            else if (sym == "diamond")  idx = 2;
            else if (sym == "triangle") idx = 3;
            else if (sym == "moon")     idx = 4;
            else if (sym == "square")   idx = 5;
            else if (sym == "cross" || sym == "x") idx = 6;
            else if (sym == "skull")    idx = 7;
            if (idx != 0xFF)
                Push(bot, SetRaidTargetIconIntent{idx, target->GetGUID()});
        }
        Push(bot, CastSpellIntent{spell_id, target->GetGUID()});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Cc: casting on {}.", target->GetName())});
        return true;
    }
    if (cmd == "tank")
    {
        // Tank-spec only — pull the sender's selection and hold threat.
        // For non-tank specs, reply with a clear "not a tank" message
        // rather than silently failing; the address resolver's role:
        // filter normally prevents non-tanks from receiving this, but
        // a stray /w direct still hits.
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        { Push(bot, WhisperIntent{sender->GetName(), "Tank: no snapshot."}); return true; }
        if (snap->group.my_role != Role::Tank)
        { Push(bot, WhisperIntent{sender->GetName(),
            "Tank: not a tank-spec — try squad: tank pull."});
          return true; }
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        { Push(bot, WhisperIntent{sender->GetName(),
            "Tank: select a pull target first."});
          return true; }
        Push(bot, StartAttackIntent{target->GetGUID()});
        if (BotAI* ai = Services::Registry().ai(id))
        {
            ai->enter_manual(BotAI::OwnerCommand::Engage, target->GetGUID(),
                             GameTime::GetGameTimeMS());
            ai->set_last_owner_name(sender->GetName());
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Tank: pulling {}.", target->GetName())});
        return true;
    }
    if (cmd == "stop" || cmd == "passive" || cmd == "disengage")
    {
        Push(bot, StopAttackIntent{});
        // /disengage clears manual mode entirely (vs /stop which is a
        // single-shot break-combat). Cleanly returning to autonomous
        // is the right semantic when the owner says "back off".
        if (cmd == "disengage")
            if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
                ai->exit_manual();
        return true;
    }
    if (cmd == "unstuck")
    {
        // Owner emergency unstuck. Default distance 5yd; takes optional
        // arg via /unstuck <yd>. Server clamps to navigable terrain so
        // overshoot is benign.
        const float dist = (args.size() >= 1) ? static_cast<float>(args[0]) : 5.0f;
        Push(bot, UnstuckIntent{dist});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Unstuck: nudging {}yd forward.", dist)});
        return true;
    }
    if (cmd == "unstuck_all" || cmd == "unstuckall")
    {
        // Mass emergency unstuck. Each bot near-teleports forward `dist` yd
        // via Player::NearTeleportTo, server-clamped to navigable terrain.
        // Useful when the whole group is wedged on the same terrain glitch
        // (cliff edge, doorway, mob clump). Default 5yd per bot.
        const float dist = (args.size() >= 1) ? static_cast<float>(args[0]) : 5.0f;
        const uint32 n = BroadcastToGroup(bot, UnstuckIntent{dist});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Unstuck_all: nudged {} bot(s) {}yd.", n, dist)});
        return true;
    }
    if (cmd == "clearprefs" || cmd == "clear_prefs")
    {
        // Wipe per-bot owner overrides: focus, role override, follow distance,
        // verbose logging, AoE preference. Useful as a "reset to factory" for
        // a bot that's been micro-tuned and needs to return to spec defaults.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Clearprefs: bot not registered."});
            return true;
        }
        ai->set_focus_target(ObjectGuid::Empty);
        ai->set_role_override(Role::Unknown);
        ai->set_follow_distance(0.f);
        ai->set_verbose_logging(false);
        ai->set_aoe_preference(false);
        Push(bot, WhisperIntent{sender->GetName(),
            "Clearprefs: focus, role, follow_distance, verbose, aoe reset."});
        return true;
    }
    if (cmd == "clearprefs_all" || cmd == "clearprefsall" || cmd == "clear_prefs_all")
    {
        // Mass /clearprefs across the group.
        const uint32 changed = ForEachGroupBot(bot, [&](Player const* p)
        {
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            if (!ai) return;
            ai->set_focus_target(ObjectGuid::Empty);
            ai->set_role_override(Role::Unknown);
            ai->set_follow_distance(0.f);
            ai->set_verbose_logging(false);
            ai->set_aoe_preference(false);
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Clearprefs_all: reset on {} bot(s).", changed)});
        return true;
    }
    if (cmd == "attackmark" || cmd == "skull")
    {
        // Pull the group's currently-marked skull and start attacking it.
        // Useful when a group leader wants the bot to focus the called
        // target without typing the GUID. Default symbol is skull (7);
        // /attackmark cross attacks the X-marked target, etc.
        uint8 sym = 7;
        if      (parsed.tail == "star")     sym = 0;
        else if (parsed.tail == "circle")   sym = 1;
        else if (parsed.tail == "diamond")  sym = 2;
        else if (parsed.tail == "triangle") sym = 3;
        else if (parsed.tail == "moon")     sym = 4;
        else if (parsed.tail == "square")   sym = 5;
        else if (parsed.tail == "cross" || parsed.tail == "x") sym = 6;
        const uint64 id = bot->GetGUID().GetCounter();
        auto gsnap = Services::Snapshots().latest_group(id);
        if (!gsnap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Attackmark: not in group."});
            return true;
        }
        ObjectGuid mark = (sym < 8) ? gsnap->raid_marks[sym] : ObjectGuid::Empty;
        if (mark.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Attackmark: no unit marked with symbol {}.", sym)});
            return true;
        }
        Push(bot, StartAttackIntent{mark});
        return true;
    }
    if (cmd == "clearmark" || cmd == "clearmarks")
    {
        // Walk all 8 raid icons and empty them. Server gates leader/assistant.
        for (uint8 i = 0; i < 8; ++i)
            Push(bot, SetRaidTargetIconIntent{i, ObjectGuid::Empty});
        Push(bot, WhisperIntent{sender->GetName(), "Clearmark: cleared all 8 icons."});
        return true;
    }
    if (cmd == "mark")
    {
        // /mark <symbol>  — places the named raid icon on the bot's selected
        // target (or clears it if no selection). Symbols mirror Blizzard
        // names. Bot must be group leader (or assistant in raids); the API
        // returns Locked otherwise. Useful as a standalone marker bot when
        // the owner is on a leveling alt without raid-icon UI.
        std::string const& sym = parsed.tail;
        uint8 idx = 0xFF;
        if      (sym == "star")     idx = 0;
        else if (sym == "circle")   idx = 1;
        else if (sym == "diamond")  idx = 2;
        else if (sym == "triangle") idx = 3;
        else if (sym == "moon")     idx = 4;
        else if (sym == "square")   idx = 5;
        else if (sym == "cross" || sym == "x") idx = 6;
        else if (sym == "skull" || sym.empty()) idx = 7;
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Mark: star|circle|diamond|triangle|moon|square|cross|skull"});
            return true;
        }
        Push(bot, SetRaidTargetIconIntent{idx, sender->GetTarget()});
        return true;
    }
    if (cmd == "aoe")
    {
        // /aoe on   — bias rotation toward AoE
        // /aoe off  — back to single-target priority
        // /aoe      — toggle (default)
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Aoe: bot not registered."});
            return true;
        }
        bool desired;
        if (parsed.tail == "on" || parsed.tail == "1")  desired = true;
        else if (parsed.tail == "off" || parsed.tail == "0") desired = false;
        else                                                  desired = !ai->aoe_preference();
        ai->set_aoe_preference(desired);
        Push(bot, WhisperIntent{sender->GetName(),
            desired ? "Aoe: ON (rotation biases AoE)" : "Aoe: OFF (single-target)"});
        return true;
    }
    if (cmd == "setrole")
    {
        // /setrole tank|healer|dps|clear  — pin the bot to a role
        // independent of its spec. Picked up by all group/healer logic via
        // BotAI::effective_role(). `clear` (or no argument) drops the
        // override so the snapshot's spec-derived role applies again.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Setrole: bot not registered."});
            return true;
        }
        std::string const& want = parsed.tail;
        Role new_role = Role::Unknown;
        if (want == "tank") new_role = Role::Tank;
        else if (want == "healer" || want == "heal") new_role = Role::Healer;
        else if (want == "dps" || want == "damage")  new_role = Role::Dps;
        else if (want == "clear" || want.empty())    new_role = Role::Unknown;
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Setrole: tank|healer|dps|clear"});
            return true;
        }
        ai->set_role_override(new_role);
        char const* label =
            new_role == Role::Tank   ? "tank"   :
            new_role == Role::Healer ? "healer" :
            new_role == Role::Dps    ? "dps"    : "cleared";
        Push(bot, WhisperIntent{sender->GetName(), std::string("Setrole: ") + label});
        return true;
    }
    if (cmd == "aoe_all" || cmd == "aoeall")
    {
        // Broadcast AoE-preference toggle across all group bots. /aoe_all on /
        // off / (no arg → toggle each individually). Mutates BotAI state
        // directly via Services::Registry().ai(id), since AoE preference is a
        // per-bot AI flag rather than an emitted intent.
        const std::string& want = parsed.tail;
        const int mode = (want == "on" || want == "1") ? 1
                       : (want == "off" || want == "0") ? 0
                       : -1;   // -1 = toggle
        const uint32 changed = ForEachGroupBot(bot, [&](Player const* p)
        {
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            if (!ai) return;
            const bool desired = (mode == -1) ? !ai->aoe_preference() : (mode == 1);
            ai->set_aoe_preference(desired);
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Aoe_all: applied to {} bot(s).", changed)});
        return true;
    }
    if (cmd == "setrole_all" || cmd == "setroleall")
    {
        // Mass role-override across the group. Useful for "everyone DPS, no
        // healer for this trash pull" or "everyone tank, /follow_all to taunt
        // a path". Same arg shape as /setrole.
        std::string const& want = parsed.tail;
        Role new_role = Role::Unknown;
        if (want == "tank") new_role = Role::Tank;
        else if (want == "healer" || want == "heal") new_role = Role::Healer;
        else if (want == "dps" || want == "damage")  new_role = Role::Dps;
        else if (want == "clear" || want.empty())    new_role = Role::Unknown;
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Setrole_all: tank|healer|dps|clear"});
            return true;
        }
        const uint32 changed = ForEachGroupBot(bot, [&](Player const* p)
        {
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            if (ai) ai->set_role_override(new_role);
        });
        char const* label =
            new_role == Role::Tank   ? "tank"   :
            new_role == Role::Healer ? "healer" :
            new_role == Role::Dps    ? "dps"    : "cleared";
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Setrole_all: {} on {} bot(s).", label, changed)});
        return true;
    }
    if (cmd == "verbose")
    {
        // Per-bot toggle for the rule-fire trace logging. /verbose on / off /
        // (no arg → toggle). When ON, set_last_rule_fired emits TC_LOG_DEBUG
        // each fired rule under "playerbot.v2" — useful for live combat
        // tuning without restarting the server.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Verbose: bot not registered."});
            return true;
        }
        const std::string& want = parsed.tail;
        const bool desired = (want == "on" || want == "1") ? true
                           : (want == "off" || want == "0") ? false
                           : !ai->verbose_logging();
        ai->set_verbose_logging(desired);
        Push(bot, WhisperIntent{sender->GetName(),
            desired ? "Verbose: ON (rule fires log to playerbot.v2)"
                    : "Verbose: OFF"});
        return true;
    }
    if (cmd == "focus_all" || cmd == "focusall")
    {
        // Mass focus pin across the group. /focus_all me | <name> | clear
        // Uses sender's selection if no tail — handy for "everyone focus
        // this mob".
        ObjectGuid focus_guid = ObjectGuid::Empty;
        if (parsed.tail == "clear" || parsed.tail == "off")
        {
            focus_guid = ObjectGuid::Empty;
        }
        else if (parsed.tail == "me" || parsed.tail == "self")
        {
            focus_guid = sender->GetGUID();
        }
        else if (!parsed.tail.empty())
        {
            if (Player* p = ObjectAccessor::FindPlayerByName(parsed.tail))
                focus_guid = p->GetGUID();
            else
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Focus_all: player '{}' not found.", parsed.tail)});
                return true;
            }
        }
        else if (Unit const* sel = sender->GetSelectedUnit())
        {
            focus_guid = sel->GetGUID();
        }
        const uint32 changed = ForEachGroupBot(bot, [&](Player const* p)
        {
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            if (ai) ai->set_focus_target(focus_guid);
        });
        Push(bot, WhisperIntent{sender->GetName(),
            focus_guid.IsEmpty()
                ? fmt::format("Focus_all: cleared on {} bot(s).", changed)
                : fmt::format("Focus_all: pinned on {} bot(s).", changed)});
        return true;
    }
    if (cmd == "verbose_all" || cmd == "verboseall")
    {
        // Mass verbose toggle. Same arg shape as /verbose.
        const std::string& want = parsed.tail;
        const int mode = (want == "on" || want == "1") ? 1
                       : (want == "off" || want == "0") ? 0
                       : -1;   // -1 = toggle each
        const uint32 changed = ForEachGroupBot(bot, [&](Player const* p)
        {
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            if (!ai) return;
            const bool desired = (mode == -1) ? !ai->verbose_logging() : (mode == 1);
            ai->set_verbose_logging(desired);
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Verbose_all: applied to {} bot(s).", changed)});
        return true;
    }
    if (cmd == "reset")
    {
        // Owner emergency unstuck. Clears in-flight movement, attack, and
        // pending cast. Does NOT touch the snapshot or intent queue (the
        // worker drains the queue as a normal-turn result of the next
        // tick), so the bot picks up fresh AI decisions on the next loop.
        // Useful when the bot is wedged on a target it can't reach or
        // mid-rotation on a target the owner wants to disengage from.
        Push(bot, StopMovementIntent{});
        Push(bot, StopAttackIntent{});
        Push(bot, CancelCastIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
                                "Reset: cleared movement / attack / cast."});
        return true;
    }
    if (cmd == "release")
    {
        Push(bot, ReleaseCorpseIntent{});
        return true;
    }
    if (cmd == "revive")
    {
        Push(bot, ReviveAtCorpseIntent{});
        return true;
    }
    if (cmd == "dismount")
    {
        Push(bot, DismountIntent{});
        return true;
    }
    if (cmd == "sit")
    {
        Push(bot, SetStandStateIntent{1});
        return true;
    }
    if (cmd == "stand")
    {
        Push(bot, SetStandStateIntent{0});
        return true;
    }
    if (cmd == "dismiss")
    {
        // Hunter/warlock: dismiss the active pet. Useful before flight paths
        // (the pet doesn't follow), or for warlocks switching demons before
        // a hard pull. The auto-MaintainPet rule re-summons OOC, so dismissal
        // is reversible — hunters get Call Pet, warlocks re-summon their
        // class pet on the next OOC tick.
        Push(bot, DismissPetIntent{});
        return true;
    }
    if (cmd == "hearth")
    {
        Push(bot, HearthIntent{});
        return true;
    }
    if (cmd == "bind")
    {
        // Manual homebind. Sender's selected target should be an innkeeper
        // NPC; falls back to the bot's selected target. The auto-bind rule
        // also handles this in the proximity-driven flow but the manual
        // form skips the dedup so an owner can re-bind on demand.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Bind: select an innkeeper first."});
            return true;
        }
        Push(bot, BindHomebindIntent{target->GetGUID()});
        return true;
    }
    if (cmd == "bind_all" || cmd == "bindall")
    {
        // Mass homebind at sender's selected innkeeper. Per-bot interact
        // range gated by API; out-of-range bots fail silently. Each bind
        // costs ~10c gold per bot — owners with broke bots see those skip
        // (NotEnoughResource on the API path).
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Bind_all: select an innkeeper first."});
            return true;
        }
        const uint32 n = BroadcastToGroup(bot, BindHomebindIntent{target->GetGUID()});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Bind_all: queued homebind on {} bot(s).", n)});
        return true;
    }
    if (cmd == "abandonall")
    {
        // Bulk-abandon every quest in the bot's log. Useful when the owner
        // wants to reset the quest log before a fresh leveling pass or when
        // the log is full of irrelevant cross-zone quests. Iterates the
        // snapshot's quest list; each abandon goes through the same API
        // gate as the single form.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->quest_log.quests.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Quest: log empty."});
            return true;
        }
        size_t dropped = 0;
        for (auto const& q : snap->quest_log.quests)
        {
            Push(bot, QuestAbandonIntent{q.quest_id});
            ++dropped;
        }
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("Quest: abandoning {} quest(s).", dropped)});
        return true;
    }
    if (cmd == "abandon" && args.size() >= 1)
    {
        // Manual quest drop. The owner picks a quest_id from /stats or
        // anywhere they tracked it. The API gates on Quest log lookup;
        // unknown ids fail with NotKnown but we still emit the intent so
        // the bot reports something.
        const uint32 quest_id = static_cast<uint32>(args[0]);
        Push(bot, QuestAbandonIntent{quest_id});
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("Quest: abandoning {}.", quest_id)});
        return true;
    }
    if (cmd == "buyall")
    {
        // Convenience: top off all three consumable categories from one
        // vendor visit. Saves the owner from issuing /buyfood, /buypot,
        // /buybandage in sequence. The API gates each on the vendor's
        // stock and the bot's gold, so over-buy doesn't go negative.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Buyall: select a vendor first."});
            return true;
        }
        // ITEM_CLASS_CONSUMABLE = 0; subclasses match the auto-rules.
        Push(bot, VendorBuyByCategoryIntent{target->GetGUID(), 0, /*food*/    5, 20});
        Push(bot, VendorBuyByCategoryIntent{target->GetGUID(), 0, /*potion*/  1, 5});
        Push(bot, VendorBuyByCategoryIntent{target->GetGUID(), 0, /*bandage*/ 7, 10});
        Push(bot, WhisperIntent{sender->GetName(),
                                "Vendor: queued food/potion/bandage restock."});
        return true;
    }
    if (cmd == "buy" && args.size() >= 1)
    {
        // /buy <item_entry> [<count>] — purchase a specific item from sender's
        // selected vendor. Default count = 1. Useful for reagents, ammo, gear.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Buy: select a vendor first."});
            return true;
        }
        const uint32 entry = static_cast<uint32>(args[0]);
        const uint32 count = args.size() >= 2 ? static_cast<uint32>(args[1]) : 1u;
        Push(bot, VendorBuyByEntryIntent{target->GetGUID(), entry, count});
        return true;
    }
    if (cmd == "buyfood" || cmd == "buypot" || cmd == "buybandage")
    {
        // Manual restock triggers — auto-rules already handle these but the
        // owner sometimes wants to top off before queueing. Sender's selected
        // target must be a vendor; the API gates on InvalidTarget if not.
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Restock: select a vendor first."});
            return true;
        }
        // ITEM_CLASS_CONSUMABLE = 0; subclasses match the auto-rules.
        uint8 subclass = 5;     // Food/Drink
        uint8 amount   = 20;
        if (cmd == "buypot")     { subclass = 1;  amount = 5;  }
        if (cmd == "buybandage") { subclass = 7;  amount = 10; }
        Push(bot, VendorBuyByCategoryIntent{target->GetGUID(), 0, subclass, amount});
        return true;
    }
    if (cmd == "sell")
    {
        // The sender's selected target should be a vendor; the bot must be
        // close enough to interact. The API gates on InvalidTarget if not.
        // /sell                 — sells all grey-quality trash
        // /sell <bag> <slot>    — sells the specific stack at bag/slot
        // /sell <bag> <slot> <n>— sells `n` units from that stack
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Sell: select a vendor first."});
            return true;
        }
        if (args.empty())
        {
            Push(bot, VendorSellTrashIntent{target->GetGUID()});
            return true;
        }
        if (args.size() < 2)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Sell: usage /sell [<bag> <slot> [<count>]]."});
            return true;
        }
        const uint8 bag   = static_cast<uint8>(args[0]);
        const uint8 slot  = static_cast<uint8>(args[1]);
        const uint8 count = args.size() >= 3 ? static_cast<uint8>(args[2]) : 0;
        Push(bot, VendorSellIntent{target->GetGUID(), bag, slot, count});
        return true;
    }
    if (cmd == "repair")
    {
        Unit const* target = sender->GetSelectedUnit();
        if (!target)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Repair: select a repair NPC first."});
            return true;
        }
        Push(bot, RepairAllIntent{target->GetGUID(), /*from_guild_bank*/ false});
        return true;
    }
    if (cmd == "mount")
    {
        Push(bot, MountIntent{0});
        return true;
    }
    if (cmd == "leave" || cmd == "leavegroup")
    {
        // Bot leaves the current party/raid. Useful when the player is
        // disbanding casually or the bot ended up in the wrong group.
        Push(bot, GroupLeaveIntent{});
        return true;
    }
    if (cmd == "ready")
    {
        // Manual ready — same intent the auto-responder fires.
        Push(bot, GroupReadyResponseIntent{true});
        return true;
    }
    if (cmd == "promote")
    {
        // Hand off raid/party leadership. The new leader is whoever the
        // sender currently has targeted (must be a Player in the bot's
        // group). Fails silently with a whisper if selection is wrong;
        // the API additionally enforces leader/member checks.
        Unit const* target = sender->GetSelectedUnit();
        if (!target || !target->IsPlayer())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Promote: select a player in our group first."});
            return true;
        }
        Push(bot, GroupPromoteToLeaderIntent{target->GetGUID()});
        return true;
    }
    if (cmd == "kick" || cmd == "uninvite")
    {
        // Kick the sender's selected target from the bot's group. Bot must be
        // group leader; the API additionally validates membership and refuses
        // a self-kick. Owner can also use sender's selection of any unit —
        // non-Player selections short-circuit with a whisper hint.
        Unit const* target = sender->GetSelectedUnit();
        if (!target || !target->IsPlayer())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Kick: select a player in our group first."});
            return true;
        }
        Push(bot, GroupKickMemberIntent{target->GetGUID()});
        return true;
    }
    if (cmd == "convert" || cmd == "toraid")
    {
        // Convert the bot's party to a raid (40-man). Leader-only; idempotent
        // when already raid. No selection needed.
        Push(bot, GroupConvertToRaidIntent{});
        return true;
    }
    if (cmd == "readycheck" || cmd == "rc")
    {
        // Initiate a ready check. Leader in parties, leader/assistant in raids.
        Push(bot, GroupStartReadyCheckIntent{});
        return true;
    }
    if (cmd == "assist+" || cmd == "assistflag")
    {
        // Grant assistant flag to the sender's selected raid member.
        // Raid+leader-only; the API enforces both.
        Unit const* target = sender->GetSelectedUnit();
        if (!target || !target->IsPlayer())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Assist+: select a player in our raid first."});
            return true;
        }
        Push(bot, GroupSetAssistantIntent{target->GetGUID(), true});
        return true;
    }
    if (cmd == "assist-" || cmd == "unassist")
    {
        // Strip assistant flag from sender's selected raid member.
        Unit const* target = sender->GetSelectedUnit();
        if (!target || !target->IsPlayer())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Assist-: select a player in our raid first."});
            return true;
        }
        Push(bot, GroupSetAssistantIntent{target->GetGUID(), false});
        return true;
    }
    if (cmd == "resetinstances" || cmd == "ri")
    {
        // Reset the bot's instance binds. When grouped the bot must be leader
        // (Group::ResetInstances enforces this); ungrouped resets the bot's own.
        // Useful between farming runs.
        Push(bot, ResetInstancesIntent{});
        return true;
    }
    if (cmd == "gaccept" || cmd == "guildaccept")
    {
        // Accept a pending guild invite. Manual variant; bots auto-accept
        // owner-issued invites in State_Idle once the snapshot exposes them.
        Push(bot, GuildAcceptInviteIntent{});
        return true;
    }
    if (cmd == "gdecline" || cmd == "guilddecline")
    {
        // Decline a pending guild invite (clear GuildIdInvited). Idempotent.
        Push(bot, GuildDeclineInviteIntent{});
        return true;
    }
    if (cmd == "gquit" || cmd == "guildleave" || cmd == "gleave")
    {
        // Leave the bot's guild. GM cannot leave (server enforces).
        Push(bot, GuildLeaveIntent{});
        return true;
    }
    if (cmd == "afk")
    {
        // Toggle the AFK flag. Owner-driven: bots otherwise stay clear of AFK.
        Push(bot, ToggleAfkIntent{});
        return true;
    }
    if (cmd == "pvp")
    {
        // Toggle PvP flag. Useful when corralling bots into a contested zone
        // so they can defend each other.
        Push(bot, TogglePvpIntent{});
        return true;
    }
    if (cmd == "addfriend" || cmd == "afriend")
    {
        // /addfriend <name> [note] — add named player to bot's friend list.
        // Useful for the duel-initiator-is-friend auto-accept flow: if the
        // bot whitelists you, your /duel will be accepted instead of declined.
        if (parsed.tail.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Addfriend: usage /addfriend <name> [<note>]"});
            return true;
        }
        // Split tail into name + note (rest after first space).
        std::string name = parsed.tail;
        std::string note;
        size_t sp = name.find(' ');
        if (sp != std::string::npos)
        {
            note = name.substr(sp + 1);
            name = name.substr(0, sp);
        }
        Push(bot, AddFriendIntent{std::move(name), std::move(note)});
        return true;
    }
    if ((cmd == "removefriend" || cmd == "rfriend") && parsed.tail.empty() == false)
    {
        // /removefriend <name> — drop a player from friend list. Resolves the
        // name via CharacterCache to a guid; if the cache misses we whisper-back.
        std::string name = parsed.tail;
        if (CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByName(name))
        {
            Push(bot, RemoveFriendIntent{info->Guid});
        }
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Removefriend: '{}' not found in cache.", name)});
        }
        return true;
    }
    if (cmd == "mailsend" && args.size() >= 1)
    {
        // /mailsend <copper> <recipient_name> [<subject text>]
        // Sends money-only mail to a named character. Recipient name must
        // resolve via CharacterCache. The subject defaults to "Bot remittance".
        // Suffix shorthand on copper: 100g/50s/25c per the SplitArgs grammar.
        if (parsed.tail.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Mailsend: usage /mailsend <amount> <recipient> [<subject>]"});
            return true;
        }
        const uint64 copper = args[0];
        // Re-parse tail to extract recipient + subject after the amount token.
        // Skip the first whitespace-bounded token (the amount), keep the rest.
        std::string rest = parsed.tail;
        size_t a = rest.find_first_not_of(" \t");
        if (a != std::string::npos) rest = rest.substr(a);
        size_t sp = rest.find(' ');
        if (sp == std::string::npos)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Mailsend: missing recipient."});
            return true;
        }
        rest = rest.substr(sp + 1);
        // Now rest = "<recipient> [<subject>]"
        std::string recipient = rest;
        std::string subject = "Bot remittance";
        size_t sp2 = rest.find(' ');
        if (sp2 != std::string::npos)
        {
            recipient = rest.substr(0, sp2);
            subject = rest.substr(sp2 + 1);
        }
        Push(bot, MailSendMoneyIntent{std::move(recipient), copper,
                                      std::move(subject), std::string{}});
        return true;
    }
    if (cmd == "mailitem" && args.size() >= 1)
    {
        // /mailitem <item_entry> <recipient> [<count>]
        // Mails the first matching item by entry from the bot's bags. count=0
        // (default) mails the full stack; otherwise clones a partial. Postage
        // 30c flat. The recipient is resolved server-side via CharacterCache.
        if (parsed.tail.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Mailitem: usage /mailitem <item_entry> <recipient> [<count>]"});
            return true;
        }
        const uint32 entry = static_cast<uint32>(args[0]);
        // Skip the first token (entry) and pull the rest of the tail.
        std::string rest = parsed.tail;
        size_t a = rest.find_first_not_of(" \t");
        if (a != std::string::npos) rest = rest.substr(a);
        size_t sp = rest.find(' ');
        if (sp == std::string::npos)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Mailitem: missing recipient."});
            return true;
        }
        rest = rest.substr(sp + 1);  // now "<recipient> [<count>]"
        std::string recipient = rest;
        uint32 count = 0;
        size_t sp2 = rest.find(' ');
        if (sp2 != std::string::npos)
        {
            recipient = rest.substr(0, sp2);
            // Trailing token is optional count (plain integer).
            std::string tail = rest.substr(sp2 + 1);
            try { count = static_cast<uint32>(std::stoul(tail)); }
            catch (...) { /* leave count=0 = full stack */ }
        }
        // Locate the first item by entry in the bot's bags. Walks the canonical
        // inventory ranges (equipped, backpack, bank-hot, child bags). We
        // prefer non-equipped slots: skipping EQUIPMENT_SLOT_START..END so the
        // owner can't accidentally mail a worn item.
        ObjectGuid item_guid;
        for (uint8 bag : { (uint8)INVENTORY_SLOT_BAG_0,
                           (uint8)INVENTORY_SLOT_BAG_START,
                           (uint8)(INVENTORY_SLOT_BAG_START + 1),
                           (uint8)(INVENTORY_SLOT_BAG_START + 2),
                           (uint8)(INVENTORY_SLOT_BAG_START + 3) })
        {
            if (!item_guid.IsEmpty())
                break;
            uint8 const slot_count = (bag == INVENTORY_SLOT_BAG_0)
                ? static_cast<uint8>(INVENTORY_SLOT_ITEM_END)
                : static_cast<uint8>(MAX_BAG_SIZE);
            uint8 const slot_start = (bag == INVENTORY_SLOT_BAG_0)
                ? static_cast<uint8>(INVENTORY_SLOT_ITEM_START)
                : uint8{0};
            for (uint8 slot = slot_start; slot < slot_count; ++slot)
            {
                Item* it = bot->GetItemByPos(bag, slot);
                if (!it) continue;
                if (it->GetEntry() != entry) continue;
                item_guid = it->GetGUID();
                break;
            }
        }
        if (item_guid.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Mailitem: no matching item in bags."});
            return true;
        }
        Push(bot, MailSendItemIntent{std::move(recipient), item_guid, count,
                                     0u, 0u, std::string{"Bot delivery"}, std::string{}});
        return true;
    }
    if (cmd == "tell" || cmd == "w" || cmd == "msg")
    {
        // /tell <recipient> <text> — bot whispers a player by name. Useful
        // for owners scripting bot interactions ("ack" the raid leader, etc).
        // Splits on the first whitespace after the verb.
        std::string rest = parsed.tail;
        size_t a = rest.find_first_not_of(" \t");
        if (a == std::string::npos)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Tell: usage /tell <name> <text>"});
            return true;
        }
        rest = rest.substr(a);
        size_t sp = rest.find(' ');
        if (sp == std::string::npos)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Tell: missing text."});
            return true;
        }
        std::string recipient = rest.substr(0, sp);
        std::string text = rest.substr(sp + 1);
        Push(bot, WhisperIntent{std::move(recipient), std::move(text)});
        return true;
    }
    if (cmd == "casting" || cmd == "cast?")
    {
        // Quick "what is the bot doing right now" snapshot. Reports spell id +
        // remaining ms when casting; "idle" otherwise. Useful for diagnosing
        // why a manual /cast was rejected (already casting something else).
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Casting: no snapshot."});
            return true;
        }
        if (snap->cast.is_casting)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Casting: spell={} {}ms remaining (gcd {}ms)",
                            snap->cast.current_cast_spell_id,
                            snap->cast.current_cast_remaining.count(),
                            snap->cooldowns.gcd_remaining.count())});
        }
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Casting: idle (gcd {}ms)",
                            snap->cooldowns.gcd_remaining.count())});
        }
        return true;
    }
    if (cmd == "speed" || cmd == "movespeed")
    {
        // Report current run/swim/fly speed (yards per second). Useful for
        // diagnosing speed-buff stacking or seeing why bot can't keep up.
        const float run = bot->GetSpeed(MOVE_RUN);
        const float walk = bot->GetSpeed(MOVE_WALK);
        const float swim = bot->GetSpeed(MOVE_SWIM);
        const float fly = bot->GetSpeed(MOVE_FLIGHT);
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Speed: walk={:.2f} run={:.2f} swim={:.2f} fly={:.2f} (yd/s)",
                        walk, run, swim, fly)});
        return true;
    }
    if (cmd == "rsvpall" || cmd == "calaccept")
    {
        // /rsvpall — accept every pending calendar invite. Useful when raid
        // leaders mass-invite a roster of bots. Optional arg "decline" to
        // RSVP no instead.
        bool accept = !(parsed.tail == "decline" || parsed.tail == "no");
        Push(bot, CalendarRsvpAllIntent{accept});
        return true;
    }
    if (cmd == "personality" || cmd == "perso")
    {
        // Dump the bot's stored personality settings (skill tier / verbosity /
        // aggression / risk tolerance / politeness / loyalty / activity pref).
        // Read-only — set via the playerbot_v2_personality table directly.
        const BotId id = bot->GetGUID().GetCounter();
        BotAI* ai = Services::Registry().ai(id);
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Personality: not registered."});
            return true;
        }
        auto const& p = ai->personality();
        char const* skill[] = {"Novice","Competent","Expert","Elite"};
        char const* verb[]  = {"Silent","Terse","Normal","Chatty","Roleplay"};
        char const* aggr[]  = {"Passive","Defensive","Normal","Aggressive"};
        char const* risk[]  = {"Cautious","Careful","Normal","Reckless"};
        char const* polite[] = {"Rude","Neutral","Polite"};
        char const* loyal[]  = {"Flighty","Normal","Devoted"};
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Skill={} Verb={} Aggr={} Risk={}",
                        skill[std::min<int>(static_cast<int>(p.skill_tier), 3)],
                        verb [std::min<int>(static_cast<int>(p.verbosity),    4)],
                        aggr [std::min<int>(static_cast<int>(p.aggression),   3)],
                        risk [std::min<int>(static_cast<int>(p.risk_tolerance), 3)])});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Polite={} Loyal={} ActivityPref=0x{:02x} ResponseDelay={}±{}ms MistakeRate={}%",
                        polite[std::min<int>(static_cast<int>(p.politeness), 2)],
                        loyal [std::min<int>(static_cast<int>(p.loyalty),    2)],
                        p.activity_pref, p.response_delay_ms,
                        p.response_jitter_ms, p.mistake_rate)});
        return true;
    }
    if ((cmd == "find_all" || cmd == "findall") && args.size() >= 1)
    {
        // /find_all <item_entry>  → walk every group bot's bag_items for matches.
        // Reports which bots have the item + count. Useful for locating a craft
        // mat the owner needs to pull from one bot's inventory.
        const uint32 want_entry = static_cast<uint32>(args[0]);
        uint32 holders = 0;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            uint32 total = 0;
            for (auto const& it : snap->inventory.bag_items)
                if (it.entry == want_entry) total += it.count;
            if (total > 0)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Find[{}]: {} x{}", snap->identity.name, want_entry, total)});
                ++holders;
            }
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Find_all: {} of {} bot(s) have entry {}.",
                        holders, reported, want_entry)});
        return true;
    }
    if (cmd == "auracount_all" || cmd == "auracountall")
    {
        // Compact per-bot aura-count summary across the group. Useful at
        // raid pull to quickly verify "everyone has full buffs" without
        // dumping each individual aura.
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            uint32 buffs = 0, debuffs = 0;
            for (auto const& a : snap->auras.own_auras)
                (a.is_harmful ? debuffs : buffs)++;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Auras[{}]: {} buffs / {} debuffs",
                            snap->identity.name, buffs, debuffs)});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Auracount_all: {} bot(s) reported.", reported)});
        return true;
    }
    if (cmd == "lootqueue" || cmd == "lootq")
    {
        // Per-bot pending-loot count via Services::Registry().peek_loot_size.
        const BotId id = bot->GetGUID().GetCounter();
        const size_t n = Services::Registry().peek_loot_size(id);
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Lootqueue: {} corpse(s) pending.", n)});
        return true;
    }
    if (cmd == "lootqueue_all" || cmd == "lootqall")
    {
        // Per-bot pending-loot count rollup. Useful for finding stuck bots
        // accumulating corpses they can't reach.
        size_t total = 0;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            const size_t n = Services::Registry().peek_loot_size(p->GetGUID().GetCounter());
            if (n > 0)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Lootq[{}]: {} corpse(s)", p->GetName(), n)});
                total += n;
            }
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Lootqueue_all: {} corpse(s) total across {} bot(s).", total, reported)});
        return true;
    }
    if (cmd == "map_all" || cmd == "mapall")
    {
        // Compact map distribution. Useful for "are all my bots in the same
        // zone" sanity check before a /come_all teleport. Counts unique
        // (map_id, zone_id) pairs and reports a one-liner.
        std::vector<std::pair<uint32, uint32>> seen;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            const std::pair<uint32, uint32> key{snap->position.map_id, snap->area.zone_id};
            bool dup = false;
            for (auto const& e : seen) if (e == key) { dup = true; break; }
            if (!dup) seen.push_back(key);
        });
        std::string maps;
        for (auto const& [m, z] : seen)
        {
            if (!maps.empty()) maps += " ";
            maps += fmt::format("[{}:{}]", m, z);
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Map_all: {} bot(s) across {} (map:zone) {}",
                        reported, seen.size(), maps)});
        return true;
    }
    if (cmd == "sheet_all" || cmd == "sheetall")
    {
        // Per-bot rating-derived stat sheet across the group. Useful pre-raid:
        // "who's haste-capped, who needs to swap gear". One whisper per bot.
        // PvP percents folded in only when nonzero (PvE bots get a clean line).
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            std::string line = fmt::format(
                "Sheet[{}]: crit={:.1f}% haste={:.1f}% mast={:.1f}% vers={:.1f}%",
                snap->identity.name,
                snap->secondary_stats.crit_pct_x100        / 100.0,
                snap->secondary_stats.haste_pct_x100       / 100.0,
                snap->secondary_stats.mastery_pct_x100     / 100.0,
                snap->secondary_stats.versatility_pct_x100 / 100.0);
            if (snap->secondary_stats.resilience_pct_x100 != 0 || snap->secondary_stats.pvp_power_pct_x100 != 0)
                line += fmt::format(" resil={:.1f}% pvp_pow={:.1f}%",
                                    snap->secondary_stats.resilience_pct_x100 / 100.0,
                                    snap->secondary_stats.pvp_power_pct_x100  / 100.0);
            Push(bot, WhisperIntent{sender->GetName(), line});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Sheet_all: {} bot(s) reported.", reported)});
        return true;
    }
    if (cmd == "honor_all" || cmd == "honorall")
    {
        // Per-bot honor track summary. Useful for PvP groups to see who's
        // closest to the next honor level / weekly cap.
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            const uint32 hxp_pct = snap->identity.honor_xp_for_next > 0
                ? (snap->identity.honor_xp * 100u) / snap->identity.honor_xp_for_next : 0u;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Honor[{}]: lvl{} hxp={}% kills(t/y/L)={}/{}/{}",
                            snap->identity.name, snap->identity.honor_level, hxp_pct,
                            snap->identity.honor_kills_today, snap->identity.honor_kills_yesterday,
                            snap->identity.honor_kills_lifetime)});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Honor_all: {} bot(s) reported.", reported)});
        return true;
    }
    if (cmd == "skills_all" || cmd == "skillsall")
    {
        // Per-bot profession-skill rollup. Filters to "interesting" skills:
        // gathering (Herbalism / Mining / Skinning / Fishing) + crafting
        // (Alchemy / BS / Enchanting / Engineering / Inscription / Jewelcrafting
        // / LW / Tailoring / Cooking / FirstAid). Useful for deciding "who
        // gathers, who crafts" before a profession run.
        static const std::vector<std::pair<uint16, char const*>> kProfs = {
            {164,"BS"}, {165,"LW"}, {171,"Alch"}, {182,"Herb"}, {184,"Skin"},
            {185,"Cook"}, {186,"Mining"}, {197,"Tailor"}, {202,"Eng"},
            {333,"Ench"}, {356,"Fish"}, {393,"Skin"}, {755,"JC"}, {773,"Inscr"},
            {129,"FirstAid"}, {794,"Arch"}};
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            std::string line = fmt::format("Skills[{}]:", snap->identity.name);
            bool any = false;
            for (auto const& [pid, label] : kProfs)
            {
                for (auto const& s : snap->progression.skills)
                    if (s.skill_id == pid && s.value > 0)
                    {
                        line += fmt::format(" {}={}/{}", label, s.value, s.max);
                        any = true;
                        break;
                    }
            }
            if (!any) line += " (no profs)";
            Push(bot, WhisperIntent{sender->GetName(), line});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Skills_all: {} bot(s) reported.", reported)});
        return true;
    }
    if (cmd == "personality_all" || cmd == "perso_all" || cmd == "personalityall")
    {
        // Compact per-bot personality summary across the group. Useful for
        // owners who want to spot-check group composition: "is there a chatty
        // bot, are any of them aggressive". One line per bot.
        static char const* const kVerb[]   = {"Silent","Terse","Normal","Chatty","Roleplay"};
        static char const* const kAggr[]   = {"Passive","Defensive","Normal","Aggressive"};
        static char const* const kRisk[]   = {"Cautious","Careful","Normal","Reckless"};
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            if (!ai) return;
            auto const& pers = ai->personality();
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Pers[{}]: verb={} aggr={} risk={} delay={}±{}ms",
                            p->GetName(),
                            kVerb[std::min<int>(static_cast<int>(pers.verbosity), 4)],
                            kAggr[std::min<int>(static_cast<int>(pers.aggression), 3)],
                            kRisk[std::min<int>(static_cast<int>(pers.risk_tolerance), 3)],
                            pers.response_delay_ms, pers.response_jitter_ms)});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Personality_all: {} bot(s) reported.", reported)});
        return true;
    }
    if (cmd == "addignore" || cmd == "ignore")
    {
        if (parsed.tail.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Addignore: usage /addignore <name>"});
            return true;
        }
        Push(bot, AddIgnoreIntent{parsed.tail});
        return true;
    }
    if ((cmd == "removeignore" || cmd == "unignore") && !parsed.tail.empty())
    {
        std::string name = parsed.tail;
        if (CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByName(name))
        {
            Push(bot, RemoveIgnoreIntent{info->Guid});
        }
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Unignore: '{}' not found in cache.", name)});
        }
        return true;
    }
    if ((cmd == "perform" || cmd == "doemote") && args.size() >= 1)
    {
        // /perform <emote_id> — visual emote (dance=10, sit=68, wave=3,
        // salute=66, applaud=21, laugh=11, cry=2). Optional second arg = target
        // selection (defaults to sender's selected unit if it's a player).
        ObjectGuid tgt = ObjectGuid::Empty;
        if (Unit const* sel = sender->GetSelectedUnit())
            if (sel->IsPlayer())
                tgt = sel->GetGUID();
        Push(bot, PerformEmoteIntent{static_cast<uint32>(args[0]), tgt});
        return true;
    }
    if (cmd == "dance")
    {
        Push(bot, PerformEmoteIntent{10 /*EMOTE_STATE_DANCE*/, ObjectGuid::Empty});
        return true;
    }
    if (cmd == "wave")
    {
        ObjectGuid tgt = sender->GetGUID();
        Push(bot, PerformEmoteIntent{3 /*EMOTE_ONESHOT_WAVE*/, tgt});
        return true;
    }
    if (cmd == "salute")
    {
        ObjectGuid tgt = sender->GetGUID();
        Push(bot, PerformEmoteIntent{66 /*EMOTE_ONESHOT_SALUTE*/, tgt});
        return true;
    }
    if (cmd == "dance_all" || cmd == "danceall")
    {
        // Synchronized group dance. Useful for screenshot moments, raid
        // celebrations, post-boss group photos.
        const uint32 n = BroadcastToGroup(bot,
            PerformEmoteIntent{10 /*EMOTE_STATE_DANCE*/, ObjectGuid::Empty});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Dance_all: {} bot(s) dancing.", n)});
        return true;
    }
    if (cmd == "wave_all" || cmd == "waveall")
    {
        const ObjectGuid tgt = sender->GetGUID();
        const uint32 n = BroadcastToGroup(bot,
            PerformEmoteIntent{3 /*EMOTE_ONESHOT_WAVE*/, tgt});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Wave_all: {} bot(s) waving.", n)});
        return true;
    }
    if (cmd == "salute_all" || cmd == "saluteall")
    {
        const ObjectGuid tgt = sender->GetGUID();
        const uint32 n = BroadcastToGroup(bot,
            PerformEmoteIntent{66 /*EMOTE_ONESHOT_SALUTE*/, tgt});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Salute_all: {} bot(s) saluting.", n)});
        return true;
    }
    if ((cmd == "perform_all" || cmd == "doemote_all") && args.size() >= 1)
    {
        // Mass emote across the group. Same shape as /perform.
        ObjectGuid tgt = ObjectGuid::Empty;
        if (Unit const* sel = sender->GetSelectedUnit())
            if (sel->IsPlayer()) tgt = sel->GetGUID();
        const uint32 n = BroadcastToGroup(bot,
            PerformEmoteIntent{static_cast<uint32>(args[0]), tgt});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Perform_all: {} bot(s) emoting.", n)});
        return true;
    }
    if ((cmd == "say_all" || cmd == "sayall") && !parsed.tail.empty())
    {
        // Mass /say across the group. Each bot says the text into local /say
        // chat. Useful for "everyone says hi" or for distributing a long
        // monologue line by line via multiple bots.
        const uint32 n = BroadcastToGroup(bot, SayChatIntent{parsed.tail});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Say_all: {} bot(s) said.", n)});
        return true;
    }
    if ((cmd == "yell_all" || cmd == "yellall") && !parsed.tail.empty())
    {
        const uint32 n = BroadcastToGroup(bot, YellChatIntent{parsed.tail});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Yell_all: {} bot(s) yelled.", n)});
        return true;
    }
    if ((cmd == "p_all" || cmd == "pall") && !parsed.tail.empty())
    {
        // Mass party chat. Each bot fires a PartyChatIntent so the message
        // shows up in /p N times. Useful for testing chat distribution or
        // for synchronized warning ("INCOMING ADDS!") from multiple sources.
        const uint32 n = BroadcastToGroup(bot, PartyChatIntent{parsed.tail});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("P_all: {} bot(s) sent.", n)});
        return true;
    }
    if ((cmd == "em_all" || cmd == "emall" || cmd == "me_all") && !parsed.tail.empty())
    {
        const uint32 n = BroadcastToGroup(bot, EmoteChatIntent{parsed.tail});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Em_all: {} bot(s) emoted.", n)});
        return true;
    }
    if ((cmd == "whisper_all" || cmd == "whisperall" || cmd == "tell_all") && !parsed.tail.empty())
    {
        // /whisper_all <player> <text...>  → every bot whispers `text` to `player`.
        // Useful for a "the bots all say hi" gag, or for distributing the same
        // prompt to a target across multiple senders. Splits tail at the first
        // whitespace; the rest is the message.
        std::string tail = parsed.tail;
        const size_t sp = tail.find_first_of(" \t");
        if (sp == std::string::npos)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Whisper_all: usage <player> <text>"});
            return true;
        }
        std::string target = tail.substr(0, sp);
        std::string body   = tail.substr(tail.find_first_not_of(" \t", sp));
        if (body.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Whisper_all: missing text"});
            return true;
        }
        const uint32 n = BroadcastToGroup(bot,
            WhisperIntent{std::move(target), std::move(body)});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Whisper_all: {} bot(s) whispered.", n)});
        return true;
    }
    if (cmd == "lookat" || cmd == "face")
    {
        // Face the bot toward the sender's selection (or sender if nothing
        // selected). Useful for screenshots and group photos.
        Unit const* tgt = sender->GetSelectedUnit();
        ObjectGuid g = tgt ? tgt->GetGUID() : sender->GetGUID();
        Push(bot, FaceTargetIntent{g});
        return true;
    }
    if (cmd == "lookat_all" || cmd == "lookatall" || cmd == "face_all" || cmd == "faceall")
    {
        // Face every group bot toward the sender's selection (or sender).
        // Useful for synchronized screenshots / raid photos before pulls.
        Unit const* tgt = sender->GetSelectedUnit();
        const ObjectGuid g = tgt ? tgt->GetGUID() : sender->GetGUID();
        const uint32 n = BroadcastToGroup(bot, FaceTargetIntent{g});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Lookat_all: {} bot(s) facing.", n)});
        return true;
    }
    if (cmd == "jump_all" || cmd == "jumpall")
    {
        // Synchronized small jump — visual flair, also useful for clearing
        // tiny terrain bumps that are blocking pathing.
        const uint32 n = BroadcastToGroup(bot, JumpIntent{7.0f});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Jump_all: {} bot(s) jumping.", n)});
        return true;
    }
    if (cmd == "reset_all" || cmd == "resetall")
    {
        // Mass emergency unstuck. Each bot drops movement / attack / cast.
        // Useful when everyone is wedged on the same terrain glitch.
        BroadcastToGroup(bot, StopMovementIntent{});
        BroadcastToGroup(bot, StopAttackIntent{});
        const uint32 n = BroadcastToGroup(bot, CancelCastIntent{});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Reset_all: cleared movement/attack/cast on {} bot(s).", n)});
        return true;
    }
    if ((cmd == "cancelaura_all" || cmd == "cancelauraall") && args.size() >= 1)
    {
        // Mass /cancelaura <spell_id>. Each bot fires a CancelAuraIntent for
        // the spell. Used to drop a buff that was applied across the raid
        // (Hand of Freedom, Power Word: Shield) when no longer needed.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        const uint32 n = BroadcastToGroup(bot, CancelAuraIntent{spell_id});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Cancelaura_all: spell {} on {} bot(s).", spell_id, n)});
        return true;
    }
    if ((cmd == "aura_all" || cmd == "auraall") && args.size() >= 1)
    {
        // Diagnostic: which group bots have aura `spell_id` and how much
        // remaining? Walks each bot's snapshot.own_auras for the spell.
        // Useful for raid-wide buff coverage checks.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        uint32 with = 0;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            for (auto const& a : snap->auras.own_auras)
                if (a.spell_id == spell_id)
                {
                    Push(bot, WhisperIntent{sender->GetName(),
                        fmt::format("Aura[{}]: {} stacks={} rem={}ms",
                                    snap->identity.name, spell_id,
                                    a.stacks, a.remaining.count())});
                    ++with;
                    return;
                }
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Aura_all {}: {}/{} bot(s) carry it.",
                        spell_id, with, reported)});
        return true;
    }
    if (cmd == "casting_all" || cmd == "castingall")
    {
        // Diagnostic: whisper back which group bots are mid-cast right now,
        // and what spell. Counterpart to /casting for the whole group —
        // quickly find which bot is stuck on a long cast.
        uint32 active = 0;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap || !snap->cast.is_casting) return;
            ++active;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Casting[{}]: spell {} ({}ms left)",
                            snap->identity.name, snap->cast.current_cast_spell_id,
                            snap->cast.current_cast_remaining.count())});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Casting_all: {}/{} bot(s) casting.", active, reported)});
        return true;
    }
    if (cmd == "dashboard" || cmd == "board" || cmd == "summary")
    {
        // Compact group rollup: one whisper per bot with the load-bearing
        // identity / vital-stat fields. Useful for "who's in the group, what
        // are they doing, are they alive". Counterpart to /raidframe but
        // includes the bot's own state / class info.
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            BotAI* ai = Services::Registry().ai(p->GetGUID().GetCounter());
            const char* state_lbl = "?";
            if (ai)
            {
                static char const* const kStateNames[] = {
                    "LoggingIn","LoggingOut","Idle","Travelling","Questing",
                    "InCombat","Looting","Dead","Resurrecting"};
                const int si = static_cast<int>(ai->state());
                if (si >= 0 && si < int(sizeof(kStateNames)/sizeof(kStateNames[0])))
                    state_lbl = kStateNames[si];
            }
            const int32 hp_pct = snap->vitals.max_hp > 0
                ? static_cast<int32>((int64_t(snap->vitals.hp) * 100) / snap->vitals.max_hp) : 0;
            // Mana index 0 in power[]; for non-mana classes it's their primary
            // resource (rage / focus / energy / etc) which still slots at 0
            // for some classes — easier than special-casing each.
            const int32 mp_pct = snap->vitals.max_power[0] > 0
                ? (snap->vitals.power[0] * 100) / snap->vitals.max_power[0] : 0;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Bot[{}]: lvl{} cls{} {} hp={}% pwr={}% ilvl={}",
                            snap->identity.name, snap->identity.level, snap->identity.cls, state_lbl,
                            hp_pct, mp_pct, snap->inventory.average_item_level)});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Dashboard: {} bot(s) reported.", reported)});
        return true;
    }
    if (cmd == "gold_all" || cmd == "goldall" || cmd == "money_all")
    {
        // Sum gold across all group bots and report the total + per-bot lines.
        // Useful for "do we have enough gold for this raid repair" decisions.
        uint64 total_copper = 0;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            total_copper += static_cast<uint64>(snap->inventory.gold);
            const int32 c = snap->inventory.gold;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Money[{}]: {}g {}s {}c", snap->identity.name,
                            c / 10000, (c / 100) % 100, c % 100)});
        });
        const uint64 g = total_copper / 10000;
        const uint64 s = (total_copper / 100) % 100;
        const uint64 c = total_copper % 100;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Gold_all: {} bot(s), total {}g {}s {}c.", reported, g, s, c)});
        return true;
    }
    if (cmd == "xp_all" || cmd == "xpall" || cmd == "level_all" || cmd == "lvl_all")
    {
        // Per-bot level + XP% summary across the group. Useful when leveling
        // a fresh group to know who's lagging behind.
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            const uint32 xp_pct = snap->identity.xp_for_level > 0
                ? (snap->identity.xp * 100u) / snap->identity.xp_for_level : 0u;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Level[{}]: lvl{} xp={}% rest={} ilvl={}",
                            snap->identity.name, snap->identity.level, xp_pct,
                            snap->identity.rest_bonus_xp, snap->inventory.average_item_level)});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Level_all: {} bot(s) reported.", reported)});
        return true;
    }
    if (cmd == "dur_all" || cmd == "durall")
    {
        // Lowest-durability summary across group. Useful pre-raid: "is anyone
        // running gear that needs repair?" — the bot with the lowest min %
        // is the one who'd benefit most from a vendor stop.
        uint32 lowest_overall = 100;
        std::string lowest_who;
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            const uint32 lo = BotSnapshotView{*snap}.lowest_equipped_durability_pct();
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Dur[{}]: lowest={}%", snap->identity.name, lo)});
            if (lo < lowest_overall) { lowest_overall = lo; lowest_who = snap->identity.name; }
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Dur_all: {} bot(s); lowest = {}% on {}.",
                        reported, lowest_overall, lowest_who.empty() ? "(none)" : lowest_who)});
        return true;
    }
    if (cmd == "distance" || cmd == "dist")
    {
        // Report distance from bot to sender's selection (default sender).
        // Whisper-only diagnostic; no AI side effect.
        Unit const* tgt = sender->GetSelectedUnit();
        if (!tgt) tgt = sender;
        const float dist = bot->GetDistance(tgt);
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Distance to {}: {:.2f} yd", tgt->GetName(), dist)});
        return true;
    }
    if (cmd == "dnd")
    {
        // Toggle DND flag.
        Push(bot, ToggleDndIntent{});
        return true;
    }
    if ((cmd == "dungeon" || cmd == "ddiff") && args.size() >= 1)
    {
        // /dungeon <difficulty_id> — change the bot's dungeon difficulty.
        // Common ids: 1=Normal, 2=Heroic, 23=Mythic, 8=Mythic+. Server validates.
        Push(bot, SetDungeonDifficultyIntent{static_cast<uint32>(args[0])});
        return true;
    }
    if (cmd == "dungeon" || cmd == "ddiff")
    {
        Push(bot, WhisperIntent{sender->GetName(),
                                "Dungeon: usage /dungeon <difficulty_id>"});
        return true;
    }
    if ((cmd == "rdiff" || cmd == "raiddiff") && args.size() >= 1)
    {
        // /rdiff <difficulty_id> [legacy] — change the bot's raid difficulty.
        // Common ids: 14=Normal, 15=Heroic, 16=Mythic, 17=LFR; legacy=non-zero
        // selects the legacy slot for older 10/25-man variants. Note: /raid is
        // reserved for raid-chat broadcast; use /rdiff to avoid the collision.
        bool legacy = args.size() >= 2 && args[1] != 0;
        Push(bot, SetRaidDifficultyIntent{static_cast<uint32>(args[0]), legacy});
        return true;
    }
    if (cmd == "rdiff" || cmd == "raiddiff")
    {
        Push(bot, WhisperIntent{sender->GetName(),
                                "Rdiff: usage /rdiff <difficulty_id> [legacy_flag 0|1]"});
        return true;
    }
    if (cmd == "talk" || cmd == "interact")
    {
        // Open the sender's selected NPC's gossip dialog. Useful for
        // remote-driving the bot through a quest hub or trainer.
        Unit const* target = sender->GetSelectedUnit();
        if (!target || !target->IsCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Talk: select an NPC first."});
            return true;
        }
        Push(bot, InteractWithNpcIntent{target->GetGUID()});
        return true;
    }
    if (cmd == "jump")
    {
        // Hop forward — handy as an unstuck for sender-issued nudges.
        Push(bot, JumpIntent{7.0f});
        return true;
    }
    if (cmd == "decline")
    {
        // Decline an outstanding group invite. Counterpart to the auto-accept
        // path in State_Idle — useful when the player wants the bot to
        // refuse a stranger's invite.
        Push(bot, GroupDeclineIntent{});
        return true;
    }
    if (cmd == "use")
    {
        // Use the sender's selected target as a game object (chest, herb,
        // ore, quest object). For NPC interaction use the "talk" command
        // instead. The intent is rejected by the API if the selection is
        // not a GameObject.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsGameObject())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Use: select a game object first."});
            return true;
        }
        Push(bot, UseObjectIntent{selection});
        return true;
    }
    if (cmd == "talents_all" || cmd == "talentsall")
    {
        // Mass /talents apply — every group bot reapplies its starter build.
        // Useful after a /setspec_all or when bulk-respeccing a fresh raid.
        if (parsed.tail == "apply")
        {
            const uint32 n = BroadcastToGroup(bot, ApplyStarterTalentsIntent{});
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Talents_all: queued starter-build on {} bot(s).", n)});
            return true;
        }
        // Default: per-bot summary (count of talent picks per bot).
        const uint32 reported = ForEachGroupBot(bot, [&](Player const* p)
        {
            auto snap = Services::Snapshots().latest(p->GetGUID().GetCounter());
            if (!snap) return;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Talents[{}]: {} picks {} glyphs (starter={})",
                            snap->identity.name, snap->spellbook.active_talents.size(),
                            snap->spellbook.active_glyphs.size(),
                            snap->spellbook.is_starter_build ? "Y" : "N")});
        });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Talents_all: {} bot(s) reported. /talents_all apply to reset.", reported)});
        return true;
    }
    if (cmd == "talents")
    {
        // Two modes:
        //   /talents apply  — apply Blizzard starter build for the active
        //                     spec. Server gates on combat (Locked back).
        //   /talents        — report active talent picks (DB2 Talent ids).
        //                     Glyphs reported on the same line.
        if (parsed.tail == "apply")
        {
            Push(bot, ApplyStarterTalentsIntent{});
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Talents: queued starter-build apply."});
            return true;
        }
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Talents: no snapshot."});
            return true;
        }
        if (snap->spellbook.active_talents.empty() && snap->spellbook.active_glyphs.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Talents: none chosen. /talents apply for starter build."});
            return true;
        }
        // 16 picks * "12345 " = ~96 chars; whisper line is ~250-char limit.
        std::string line = "Talents:";
        const size_t cap = std::min<size_t>(snap->spellbook.active_talents.size(), 24);
        for (size_t i = 0; i < cap; ++i)
            line += ' ' + std::to_string(snap->spellbook.active_talents[i]);
        if (snap->spellbook.active_talents.size() > cap)
            line += " (+" + std::to_string(snap->spellbook.active_talents.size() - cap) + ")";
        Push(bot, WhisperIntent{sender->GetName(), line});
        if (!snap->spellbook.active_glyphs.empty())
        {
            std::string gline = "Glyphs:";
            for (uint32 g : snap->spellbook.active_glyphs)
                gline += ' ' + std::to_string(g);
            Push(bot, WhisperIntent{sender->GetName(), gline});
        }
        return true;
    }
    if (cmd == "train")
    {
        // Buy every spell on the selected trainer the bot is currently
        // eligible for. Trainer enforces level/skill/prereq + cost gates
        // per spell; ineligible ones are silently skipped server-side.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsAnyTypeCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Train: select a trainer NPC first."});
            return true;
        }
        Push(bot, TrainerBuyAllIntent{selection});
        Push(bot, WhisperIntent{sender->GetName(), "Trainer: queued bulk-train."});
        return true;
    }
    if (cmd == "bank")
    {
        // Deposit every bag item into the bot's bank. Sender's selected
        // target must be a banker NPC; the API rejects non-bankers via
        // InvalidTarget, no-op moves via Locked. Iterates the snapshot
        // (read-only fact base) and emits one BankDepositItemIntent per
        // bag slot — the executor budget keeps a runaway dump from
        // monopolising the world tick.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsAnyTypeCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Bank: select a banker NPC first."});
            return true;
        }
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        size_t deposited = 0;
        for (auto const& it : snap->inventory.bag_items)
        {
            // Skip the equipped-bags themselves (slots 19-22) — depositing a
            // bag with items in it isn't directly supported by the AH-style
            // single-slot deposit path. Bags remain bot-side.
            if (it.bag >= 19 && it.bag <= 22 && it.slot == 0xFF) continue;
            Push(bot, BankDepositItemIntent{selection, it.bag, it.slot});
            ++deposited;
        }
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("Bank: queued deposit for {} item(s).", deposited)});
        return true;
    }
    if (cmd == "flyto")
    {
        // Without a destination node id this is a no-op; useful future
        // extension would parse "flyto <node>" into the dest. For now the
        // command surfaces the API by validating the FM is selected.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() || !selection.IsAnyTypeCreature())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Taxi: select a flight master first."});
            return true;
        }
        // Discover-only mode (no destination) — registers the FM's local
        // node so future fly_to calls can use it as origin or destination.
        Push(bot, DiscoverTaxiNodeIntent{selection});
        Push(bot, WhisperIntent{sender->GetName(), "Taxi: discovered local node."});
        return true;
    }
    if (cmd == "mail")
    {
        // Drain the bot's mailbox: requires the sender's selected target to
        // be a mailbox GO (or NPC with the MAILBOX flag) and the bot to be
        // standing next to it. Iterates the published mail snapshot and
        // emits take-money, take-each-item, then delete intents per drainable
        // mail. The API re-validates interaction range at execution time, so
        // mails added between snapshot and execution are simply skipped.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() ||
            (!selection.IsGameObject() && !selection.IsAnyTypeCreature()))
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Mail: select a mailbox first."});
            return true;
        }
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        size_t emitted = 0;
        for (auto const& m : snap->mailbox.mail)
        {
            if (m.deliver_in_sec > 0) continue;
            if (m.money > 0) Push(bot, MailTakeMoneyIntent{selection, m.message_id});
            for (uint64 item_low : m.item_guid_lows)
                Push(bot, MailTakeItemIntent{selection, m.message_id, item_low});
            // Only delete when the mail won't carry COD after item pickup —
            // mail_delete refuses COD-bearing rows.
            if (m.cod == 0)
                Push(bot, MailDeleteIntent{selection, m.message_id});
            ++emitted;
        }
        Push(bot, WhisperIntent{sender->GetName(),
                                fmt::format("Mailbox: queued drain for {} mail(s).", emitted)});
        return true;
    }
    if (cmd == "clearmail")
    {
        // Bulk delete all mails that have already been drained (no money,
        // no items, no COD). Owner cleanup helper after a long absence.
        // Same selection requirement as /mail.
        ObjectGuid selection = sender->GetTarget();
        if (selection.IsEmpty() ||
            (!selection.IsGameObject() && !selection.IsAnyTypeCreature()))
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Clearmail: select a mailbox first."});
            return true;
        }
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        size_t deleted = 0;
        for (auto const& m : snap->mailbox.mail)
        {
            if (m.deliver_in_sec > 0) continue;
            if (m.cod != 0) continue;
            if (m.money > 0) continue;
            if (m.item_count > 0) continue;
            Push(bot, MailDeleteIntent{selection, m.message_id});
            ++deleted;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Clearmail: queued delete for {} empty mail(s).", deleted)});
        return true;
    }
    if (cmd == "diag")
    {
        // Multi-line per-bot diagnostic dump. Used to root-cause "why is
        // this bot not progressing?" without GM access — the owner can
        // whisper /diag and see the same data the .playerbot inspect
        // command would surface, plus the intent execution ring + recent
        // pipeline failures. Output is one whisper per line, capped via
        // the standard whisper rate-gate.
        const BotId id = bot->GetGUID().GetCounter();
        const std::string body = Playerbot::Diagnostics::DiagBot(id);
        for (auto const& ln : Trinity::Tokenize(body, '\n', true))
            Push(bot, WhisperIntent{sender->GetName(), std::string{ln}});
        return true;
    }
    if (cmd == "whyidle")
    {
        // REFACTOR_3: enumerate the IdleRuleRegistry's rule table and dump
        // it back to the requester — priority, enabled flag, current
        // gate-evaluation result, and the ms-since-last-fire counter.
        // Drives "why isn't <rule> firing" debugging without poking
        // private state. Rule names + priorities are stable across builds;
        // gate_true is evaluated against the bot's latest snapshot view.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "WhyIdle: no snapshot published yet."});
            return true;
        }
        BotAI* ai = Services::Registry().ai(id);
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "WhyIdle: no AI handle."});
            return true;
        }
        const uint32 now_ms = GameTime::GetGameTimeMS();
        BotSnapshotView view(*snap);
        // /whyidle is diagnostic-only — pass an empty GroupSnapshotView.
        // Rules that consult group state report gate_true=false in the dump,
        // which is honest: without a live group view we can't evaluate them.
        GroupSnapshotView empty_g;
        auto diag = Services::IdleRules().diagnose(view, *ai, empty_g, now_ms);
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("WhyIdle: {} registered rule(s). last_rule_fired={}",
                        diag.size(),
                        ai->last_rule_fired() ? ai->last_rule_fired() : "(none)")});
        for (auto const& r : diag)
        {
            const uint32 since_ms = (r.last_fired_ms > 0 && now_ms >= r.last_fired_ms)
                ? (now_ms - r.last_fired_ms) : 0;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("  [{}] {} prio={} gate={} enabled={} last={}ms",
                            r.priority,
                            std::string{r.name},
                            r.priority,
                            r.gate_true ? "Y" : "N",
                            r.enabled ? "Y" : "N",
                            since_ms)});
        }
        return true;
    }
    if (cmd == "info")
    {
        // One-line bot summary — denser than /stats. Tradeoff: skips
        // consumable counts to make room for ilvl + gold + zone.
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Info: no snapshot."});
            return true;
        }
        const int32 hp_pct = snap->vitals.max_hp > 0 ? (snap->vitals.hp * 100) / snap->vitals.max_hp : 0;
        const int32 mp_pct = snap->vitals.max_power[0] > 0 ? (snap->vitals.power[0] * 100) / snap->vitals.max_power[0] : 0;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("L{} cls={} spec={} ilvl={} hp={}% mp={}% gold={}c combat={} zone={}",
                        snap->identity.level, snap->identity.cls, snap->identity.spec, snap->inventory.average_item_level,
                        hp_pct, mp_pct, snap->inventory.gold,
                        snap->vitals.in_combat ? "Y" : "N", snap->area.zone_id)});
        return true;
    }
    if (cmd == "stats" || cmd == "status")
    {
        // Whisper a one-line status report back to the requester. The data
        // comes from the latest published snapshot — same fact base the AI
        // workers see, so this is naturally consistent.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        std::string body;
        if (!snap)
        {
            body = "no snapshot published yet";
        }
        else
        {
            const int32 hp_pct = snap->vitals.max_hp > 0 ? (snap->vitals.hp * 100) / snap->vitals.max_hp : 0;
            const int32 mp_pct = snap->vitals.max_power[0] > 0 ? (snap->vitals.power[0] * 100) / snap->vitals.max_power[0] : 0;
            body = fmt::format(
                "L{} cls={} spec={} hp={}% mana={}% combat={} group={} bags={}/free food={} pot={} bnd={}",
                snap->identity.level, snap->identity.cls, snap->identity.spec,
                hp_pct, mp_pct,
                snap->vitals.in_combat ? "yes" : "no",
                snap->group.group_guid.IsEmpty() ? "no" : "yes",
                snap->bags.bag_free_slots,
                snap->consumables.food_drink_count, snap->consumables.potion_count, snap->consumables.bandage_count);
        }
        Push(bot, WhisperIntent{sender->GetName(), std::move(body)});
        // Secondary stats — second line so /stats stays scannable.
        if (snap)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("crit={:.2f} haste={:.2f} mastery={:.2f} vers={:.2f}",
                            snap->secondary_stats.crit_pct_x100   / 100.0,
                            snap->secondary_stats.haste_pct_x100  / 100.0,
                            snap->secondary_stats.mastery_pct_x100 / 100.0,
                            snap->secondary_stats.versatility_pct_x100 / 100.0)});
        }
        // Owner-squad-control state — shows who's in charge right now,
        // what mode the bot is in, and where it stands relative to its
        // formation slot. Critical for owners debugging "why isn't my
        // bot following me right now?".
        if (BotAI const* ai = Services::Registry().ai(id))
        {
            const uint32 now_ms = GameTime::GetGameTimeMS();
            char const* mode = "auto";
            if (ai->is_manual_mode(now_ms))
            {
                switch (ai->owner_command())
                {
                    case BotAI::OwnerCommand::Follow:    mode = "follow"; break;
                    case BotAI::OwnerCommand::Hold:      mode = "hold";   break;
                    case BotAI::OwnerCommand::Stay:      mode = "stay";   break;
                    case BotAI::OwnerCommand::Engage:    mode = "engage"; break;
                    case BotAI::OwnerCommand::Disengage: mode = "diseng"; break;
                    case BotAI::OwnerCommand::Action:    mode = "action"; break;
                    case BotAI::OwnerCommand::None:      mode = "auto";   break;
                }
            }
            const uint32 ttl_ms = ai->is_manual_mode(now_ms)
                ? (ai->manual_mode_until_ms() - now_ms) : 0;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("mode={} owner='{}' formation={}/slot={} fd={:.1f}",
                    mode,
                    ai->last_owner_name().empty() ? "—" : ai->last_owner_name().c_str(),
                    FormationTypeName(ai->formation_type()),
                    ai->formation_slot(),
                    ai->follow_distance() > 0.f ? ai->follow_distance() : 5.0f)});
            if (ttl_ms > 0)
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("manual ttl={}s", ttl_ms / 1000)});
        }
        return true;
    }
    if (cmd == "aura" && args.size() >= 1)
    {
        // /aura <spell_id> [target]  → query a specific aura on target
        // (defaults to self). Walks the snapshot's own_auras / target_auras
        // depending on target. Reports stacks + remaining ms + caster guid.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Aura: no snapshot."});
            return true;
        }
        ObjectGuid tgt = sender->GetTarget();
        std::vector<AuraEntry> const* aura_list = nullptr;
        if (tgt.IsEmpty() || tgt == bot->GetGUID())
        {
            aura_list = &snap->auras.own_auras;
        }
        else if (tgt == snap->combat.current_target)
        {
            aura_list = &snap->auras.target_auras;
        }
        else if (tgt == snap->combat.victim)
        {
            aura_list = &snap->auras.victim_auras;
        }
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Aura: target not in snapshot (selection or current_target/victim only)."});
            return true;
        }
        for (auto const& a : *aura_list)
        {
            if (a.spell_id != spell_id) continue;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Aura {}: stacks={} remain={}ms harmful={} caster={}",
                            a.spell_id, a.stacks, a.remaining.count(),
                            a.is_harmful ? "yes" : "no",
                            a.caster.ToString())});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Aura {}: not present.", spell_id)});
        return true;
    }
    if (cmd == "auras")
    {
        // Quick aura dump — useful for spotting missing buffs / unexpected
        // debuffs ("why is bot moving slow?" → spot Hamstring). One whisper
        // per aura, capped at 8 so chat doesn't throttle on long buff lists.
        // Optional filter: /auras buffs (helpful only) or /auras debuffs
        // (harmful only); default = all auras.
        enum { F_ALL, F_BUFFS, F_DEBUFFS } filter = F_ALL;
        if (!parsed.tail.empty())
        {
            std::string sub = parsed.tail;
            size_t a = sub.find_first_not_of(" \t");
            if (a != std::string::npos) sub = sub.substr(a);
            if (sub.rfind("buff", 0) == 0)   filter = F_BUFFS;
            if (sub.rfind("debuff", 0) == 0) filter = F_DEBUFFS;
        }
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->auras.own_auras.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "no auras"});
            return true;
        }
        size_t shown = 0;
        for (auto const& a : snap->auras.own_auras)
        {
            if (filter == F_BUFFS   && a.is_harmful) continue;
            if (filter == F_DEBUFFS && !a.is_harmful) continue;
            if (shown >= 8) break;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("aura: spell={} stacks={} remain={}ms harmful={}",
                            a.spell_id, a.stacks, a.remaining.count(),
                            a.is_harmful ? "yes" : "no")});
            ++shown;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "no matching auras"});
        return true;
    }
    if (cmd == "auctions")
    {
        // Bulk-list owned auctions across all 4 houses (snapshot already
        // walks them per tick). One whisper per entry; capped at 8 lines
        // because chat will throttle a flood. Owners can `/cancelall` after
        // reviewing.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->auction.auctions_owned.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "no owned auctions"});
            return true;
        }
        size_t shown = 0;
        auto house_name = [](uint32 id) -> char const*
        {
            switch (id) { case 1: return "neutral"; case 2: return "alliance";
                          case 6: return "horde";   case 7: return "goblin";
                          default: return "?"; }
        };
        for (auto const& a : snap->auction.auctions_owned)
        {
            if (shown >= 8) break;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("AH#{}: item={} stack={} buyout={}c bidder={} expires={}s house={}",
                            a.auction_id, a.item_entry, a.stack_count, a.buyout,
                            a.has_bidder ? "yes" : "no", a.expires_in_sec, house_name(a.house_id))});
            ++shown;
        }
        if (snap->auction.auctions_owned.size() > shown)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("... +{} more", snap->auction.auctions_owned.size() - shown)});
        return true;
    }
    if (cmd == "threat")
    {
        // /threat — report this bot's threat on each live attacker.
        // Walks attackers from the snapshot, resolves each via ObjectAccessor,
        // then queries `Unit::GetThreatManager().GetThreat(bot)`. Threat is
        // the absolute value (raw "1pt = 1 damage"); higher means closer to
        // pulling aggro. Useful for DPS pacing diagnostics.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->combat.attackers.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Threat: no attackers."});
            return true;
        }
        size_t shown = 0;
        for (auto const& a : snap->combat.attackers)
        {
            if (shown >= 6) break;
            Unit* u = ObjectAccessor::GetUnit(*bot, a.guid);
            if (!u) continue;
            float t = u->GetThreatManager().GetThreat(bot);
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Threat on entry={} L{} hp={}%: {:.0f}",
                            a.entry, a.level, a.hp_pct(), t)});
            ++shown;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "Threat: attackers unreachable."});
        return true;
    }
    if (cmd == "aggro")
    {
        // Quick attacker report — useful for off-tank coordination ("which
        // mob has the bot in combat?"). Echoes guid + level + hp% per
        // attacker; capped at 4 lines so we don't spam chat with raid
        // pulls. Owner can /inspect for the full list.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->combat.attackers.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "no attackers"});
            return true;
        }
        size_t shown = 0;
        // Sort by HP descending so the heaviest threats come first.
        std::vector<NearbyUnit> sorted = snap->combat.attackers;
        std::sort(sorted.begin(), sorted.end(),
                  [](auto const& a, auto const& b) { return a.hp > b.hp; });
        for (auto const& a : sorted)
        {
            if (shown >= 4) break;
            // Entry is the creature template id (or 0 for players); the
            // creature template has the displayable name but we don't
            // surface it here — keeps the report short and DB-free.
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("aggro: entry={} L{} hp={}% guid={}",
                            a.entry, a.level, a.hp_pct(), a.guid.ToString())});
            ++shown;
        }
        if (snap->combat.attackers.size() > shown)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("... +{} more", snap->combat.attackers.size() - shown)});
        return true;
    }
    if (cmd == "group" || cmd == "roster")
    {
        // Show group roster from the snapshot. Each member: name, role,
        // hp%, online flag. Capped at 8 lines for readability (raid groups
        // can be 40 — owner can /inspect for full state). No-op when bot
        // is solo.
        const BotId id = bot->GetGUID().GetCounter();
        auto group = Services::Snapshots().latest_group(id);
        if (!group || group->members.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Group: solo."});
            return true;
        }
        size_t shown = 0;
        for (auto const& m : group->members)
        {
            if (shown >= 8) break;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{}: role={} hp={}% online={}",
                            m.name, static_cast<int>(m.role),
                            m.hp_pct(), m.online)});
            ++shown;
        }
        if (group->members.size() > shown)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("... +{} more", group->members.size() - shown)});
        return true;
    }
    if (cmd == "version" || cmd == "ver")
    {
        // Build identity for owner sanity check (which V2 build is loaded?)
        // and a snapshot version number for "is the bot getting fresh data?".
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("PlayerbotV2 build {} {}, snap_v={}",
                        __DATE__, __TIME__,
                        snap ? snap->version : 0)});
        return true;
    }
    if (cmd == "checkmail")
    {
        // Owner mailbox-state poll without driving to a mailbox. Reports
        // total/unread + first 3 sender names. Snapshot's mail vector is
        // populated on each tick when bot is near a mailbox; otherwise
        // it's stale (last-known) — owner should /come to bot or vice
        // versa to refresh.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Mail: {} total ({} unread)",
                        snap->mailbox.mail.size(), snap->mailbox.unread_mail_count)});
        size_t shown = 0;
        for (auto const& m : snap->mailbox.mail)
        {
            if (shown >= 3) break;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("mail#{}: money={}c items={} type={}",
                            m.message_id, m.money, m.item_count, m.message_type)});
            ++shown;
        }
        return true;
    }
    if (cmd == "scout")
    {
        // Compact 3-line owner status. Tighter than /stats; designed for
        // quick at-a-glance check of multiple bots simultaneously. Combat
        // info on line 1, position+health on line 2, gold+inventory+dur
        // on line 3.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        const int32 hp_pct = snap->vitals.max_hp > 0
            ? static_cast<int32>((int64_t(snap->vitals.hp) * 100) / snap->vitals.max_hp) : 0;
        const int32 mp_pct = snap->vitals.max_power[0] > 0
            ? static_cast<int32>((int64_t(snap->vitals.power[0]) * 100) / snap->vitals.max_power[0]) : 0;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Scout1: combat={} victim={} attackers={} cast={}",
                        snap->vitals.in_combat, snap->combat.victim.ToString(),
                        snap->combat.attackers.size(), snap->cast.is_casting)});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Scout2: hp={}% mp={}% map={} ({:.0f},{:.0f}) zone={}",
                        hp_pct, mp_pct, snap->position.map_id, snap->position.x, snap->position.y, snap->area.zone_id)});
        const int32 c = snap->inventory.gold;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Scout3: gold={}g bag_free={} dur={}% mail={}",
                        c / 10000, snap->bags.bag_free_slots,
                        BotSnapshotView{*snap}.lowest_equipped_durability_pct(),
                        snap->mailbox.mail.size())});
        return true;
    }
    if (cmd == "level" || cmd == "lvl")
    {
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        const uint32 xp_pct = snap->identity.xp_for_level > 0
            ? (snap->identity.xp * 100u) / snap->identity.xp_for_level : 0u;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Level: {} (ilvl {}) xp={}/{} ({}%) rest={}",
                        snap->identity.level, snap->inventory.average_item_level,
                        snap->identity.xp, snap->identity.xp_for_level, xp_pct, snap->identity.rest_bonus_xp)});
        return true;
    }
    if (cmd == "xp")
    {
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        if (snap->identity.xp_for_level == 0)
        {
            Push(bot, WhisperIntent{sender->GetName(), "XP: max level."});
            return true;
        }
        const uint32 xp_pct = (snap->identity.xp * 100u) / snap->identity.xp_for_level;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("XP: {}/{} ({}%) +rest {}",
                        snap->identity.xp, snap->identity.xp_for_level, xp_pct, snap->identity.rest_bonus_xp)});
        return true;
    }
    if (cmd == "money" || cmd == "gold")
    {
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        const int32 c = snap->inventory.gold;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Money: {}g {}s {}c",
                        c / 10000, (c / 100) % 100, c % 100)});
        return true;
    }
    if (cmd == "dur")
    {
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        // /dur — one-line lowest dur summary
        // /dur all — per-slot dump (capped at 6 lowest slots so chat doesn't
        //            wrap too many lines for high-end gear)
        if (parsed.tail == "all" || parsed.tail == "full")
        {
            // Equipment slot names (mirrors EquipmentSlots in inventory.h).
            static char const* const kSlotNames[19] = {
                "Head","Neck","Shoulders","Body","Chest","Waist","Legs",
                "Feet","Wrists","Hands","Finger1","Finger2","Trinket1","Trinket2",
                "Back","MainHand","OffHand","Ranged","Tabard"};
            // Sort ascending by dur% so the most worn shows first.
            std::vector<std::pair<uint8, uint8>> slots;
            slots.reserve(19);
            for (uint8 i = 0; i < 19; ++i)
                if (snap->inventory.equipped[i].entry)
                    slots.emplace_back(snap->inventory.equipped[i].durability_pct, i);
            std::sort(slots.begin(), slots.end());
            const size_t cap = std::min<size_t>(slots.size(), 6);
            for (size_t k = 0; k < cap; ++k)
            {
                const uint8 idx = slots[k].second;
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("{}: {}% (entry {})",
                                kSlotNames[idx], slots[k].first,
                                snap->inventory.equipped[idx].entry)});
            }
            if (slots.size() > cap)
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("... +{} more above {}%",
                                slots.size() - cap, slots[cap-1].first)});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Lowest gear durability: {}%",
                        BotSnapshotView{*snap}.lowest_equipped_durability_pct())});
        return true;
    }
    if (cmd == "groupbuffs" || cmd == "gbuffs")
    {
        // /groupbuffs  → for each group member on the same map, list
        // currently tracked raid buffs by spell id. Useful to see at a glance
        // whether everyone has Fortitude / MotW / Battle Shout etc. Members
        // off-map are skipped.
        const BotId id = bot->GetGUID().GetCounter();
        auto bsnap = Services::Snapshots().latest(id);
        auto gsnap = Services::Snapshots().latest_group(id);
        if (!bsnap || !gsnap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Groupbuffs: not in group / no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& m : gsnap->members)
        {
            if (!m.online) continue;
            if (m.map_id != bsnap->position.map_id) continue;
            std::string buff_ids;
            for (auto const& b : m.buffs)
            {
                if (!buff_ids.empty()) buff_ids += ',';
                buff_ids += std::to_string(b.spell_id);
            }
            if (buff_ids.empty()) buff_ids = "(none)";
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{} L{}: {}", m.name, m.level, buff_ids)});
            if (++shown >= 8) break;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "Groupbuffs: nobody on map."});
        return true;
    }
    if (cmd == "dispelable" || cmd == "dispellable")
    {
        // /dispelable  → list group members carrying dispellable debuffs
        // (magic/curse/disease/poison) on the same map. Walks each member's
        // debuff vector. Useful to verify the dispel rule is targeting the
        // right thing.
        const BotId id = bot->GetGUID().GetCounter();
        auto bsnap = Services::Snapshots().latest(id);
        auto gsnap = Services::Snapshots().latest_group(id);
        if (!bsnap || !gsnap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Dispelable: not in group / no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& m : gsnap->members)
        {
            if (!m.online || m.hp <= 0) continue;
            if (m.map_id != bsnap->position.map_id) continue;
            for (auto const& d : m.debuffs)
            {
                if (!d.is_harmful) continue;
                char const* dt = "?";
                switch (d.dispel_type)
                {
                    case DispelType::Magic:   dt = "Magic"; break;
                    case DispelType::Curse:   dt = "Curse"; break;
                    case DispelType::Disease: dt = "Disease"; break;
                    case DispelType::Poison:  dt = "Poison"; break;
                    default: continue;  // not dispellable
                }
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("{}: {} ({}, stacks={})",
                                m.name, d.spell_id, dt, d.stacks)});
                if (++shown >= 10) goto done_dispel;
            }
        }
done_dispel:
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "Dispelable: nothing on group."});
        return true;
    }
    if (cmd == "buffwhois" && args.size() >= 1)
    {
        // /buffwhois <spell_id>  → for each same-map group member, report
        // whether they carry the named buff. Useful for healers verifying
        // buff coverage (Fortitude/MotW/etc).
        const uint32 spell_id = static_cast<uint32>(args[0]);
        const BotId id = bot->GetGUID().GetCounter();
        auto bsnap = Services::Snapshots().latest(id);
        auto gsnap = Services::Snapshots().latest_group(id);
        if (!bsnap || !gsnap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Buffwhois: not in group / no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& m : gsnap->members)
        {
            if (!m.online) continue;
            if (m.map_id != bsnap->position.map_id) continue;
            bool has = false;
            for (auto const& b : m.buffs)
                if (b.spell_id == spell_id) { has = true; break; }
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{}: {}", m.name, has ? "YES" : "no")});
            if (++shown >= 12) break;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "Buffwhois: nobody on map."});
        return true;
    }
    if (cmd == "manapct" || cmd == "mp")
    {
        // /manapct  → mana% per same-map caster (max_mana > 0). Useful for
        // Innervate / Mana Tide Totem target planning.
        const BotId id = bot->GetGUID().GetCounter();
        auto bsnap = Services::Snapshots().latest(id);
        auto gsnap = Services::Snapshots().latest_group(id);
        if (!bsnap || !gsnap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Manapct: not in group / no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& m : gsnap->members)
        {
            if (!m.online) continue;
            if (m.map_id != bsnap->position.map_id) continue;
            if (m.max_mana <= 0) continue;
            const int mp = static_cast<int>((static_cast<int64_t>(m.mana) * 100) / m.max_mana);
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{}: {}% ({}/{})", m.name, mp, m.mana, m.max_mana)});
            if (++shown >= 10) break;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "Manapct: no casters on map."});
        return true;
    }
    if (cmd == "inrange" && args.size() >= 1)
    {
        // /inrange <yards>  → list group members within Y yards of bot. Useful
        // to plan AoE buff casts (Mark of the Wild, Fortitude, etc) where the
        // bot needs nearby targets to satisfy spell range.
        const float yards = static_cast<float>(args[0]);
        if (yards <= 0.f)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Inrange: positive yards required."});
            return true;
        }
        const float yards2 = yards * yards;
        const BotId id = bot->GetGUID().GetCounter();
        auto bsnap = Services::Snapshots().latest(id);
        auto gsnap = Services::Snapshots().latest_group(id);
        if (!bsnap || !gsnap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Inrange: not in group / no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& m : gsnap->members)
        {
            if (!m.online) continue;
            if (m.map_id != bsnap->position.map_id) continue;
            const float dx = m.x - bsnap->position.x, dy = m.y - bsnap->position.y, dz = m.z - bsnap->position.z;
            const float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 > yards2) continue;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{}: {:.1f}yd", m.name, std::sqrt(d2))});
            if (++shown >= 12) break;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Inrange: no members within {:.0f}yd.", yards)});
        return true;
    }
    if (cmd == "raidframe" || cmd == "rf")
    {
        // /raidframe  → compact HP/mana dump per member on the same map.
        // Format per line: "Name HP%/MP% L<level> R<role>". Picks up to 12
        // members.
        const BotId id = bot->GetGUID().GetCounter();
        auto bsnap = Services::Snapshots().latest(id);
        auto gsnap = Services::Snapshots().latest_group(id);
        if (!bsnap || !gsnap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Raidframe: not in group / no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& m : gsnap->members)
        {
            if (!m.online) continue;
            if (m.map_id != bsnap->position.map_id) continue;
            char const* role = "?";
            switch (m.role)
            {
                case Role::Tank:   role = "T"; break;
                case Role::Healer: role = "H"; break;
                case Role::Dps:    role = "D"; break;
                default: role = "?"; break;
            }
            const int hp_pct = m.hp_pct();
            const int mp_pct = m.max_mana > 0
                ? static_cast<int>((static_cast<int64_t>(m.mana) * 100) / m.max_mana)
                : 0;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{:>12} HP{}% MP{}% L{} R{}",
                            m.name, hp_pct, mp_pct, m.level, role)});
            if (++shown >= 12) break;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "Raidframe: nobody on map."});
        return true;
    }
    if (cmd == "healassign")
    {
        // /healassign  → for healers in a multi-healer group: who is *this*
        // bot's heal-assignment target (the Nth-lowest wounded member, where
        // N is the bot's index among living healers in the group). Returns
        // (none) if no wounded member. Solo-healer or single wounded target
        // collapses to "lowest wounded".
        const BotId id = bot->GetGUID().GetCounter();
        auto bsnap = Services::Snapshots().latest(id);
        auto gsnap = Services::Snapshots().latest_group(id);
        if (!bsnap || !gsnap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Healassign: no group / no snapshot."});
            return true;
        }
        BotSnapshotView bv{*bsnap};
        GroupSnapshotView gv{*gsnap};
        if (!gv.exists())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Healassign: not in group."});
            return true;
        }
        auto const* tgt = gv.heal_assignment(bv.raw().guid, bv.map_id());
        if (!tgt)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Healassign: no wounded target."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Healassign: {} ({}/{}, {}%)",
                        tgt->name, tgt->hp, tgt->max_hp, tgt->hp_pct())});
        return true;
    }
    if (cmd == "spec")
    {
        // Map (cls, spec) → human-readable name. Quick reference for owners
        // managing many bots with similar names. Spec IDs match
        // ChrSpecialization.db2; we hardcode the 39 entries here rather
        // than DB2-loading on the command thread.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        char const* spec_name = "Unknown";
        switch (snap->identity.spec)
        {
            case 250: spec_name = "Blood DK"; break;
            case 251: spec_name = "Frost DK"; break;
            case 252: spec_name = "Unholy DK"; break;
            case 253: spec_name = "Beast Mastery Hunter"; break;
            case 254: spec_name = "Marksmanship Hunter"; break;
            case 255: spec_name = "Survival Hunter"; break;
            case 256: spec_name = "Discipline Priest"; break;
            case 257: spec_name = "Holy Priest"; break;
            case 258: spec_name = "Shadow Priest"; break;
            case 259: spec_name = "Assassination Rogue"; break;
            case 260: spec_name = "Outlaw Rogue"; break;
            case 261: spec_name = "Subtlety Rogue"; break;
            case 262: spec_name = "Elemental Shaman"; break;
            case 263: spec_name = "Enhancement Shaman"; break;
            case 264: spec_name = "Restoration Shaman"; break;
            case 265: spec_name = "Affliction Warlock"; break;
            case 266: spec_name = "Demonology Warlock"; break;
            case 267: spec_name = "Destruction Warlock"; break;
            case 268: spec_name = "Brewmaster Monk"; break;
            case 269: spec_name = "Windwalker Monk"; break;
            case 270: spec_name = "Mistweaver Monk"; break;
            case 62:  spec_name = "Arcane Mage"; break;
            case 63:  spec_name = "Fire Mage"; break;
            case 64:  spec_name = "Frost Mage"; break;
            case 65:  spec_name = "Holy Paladin"; break;
            case 66:  spec_name = "Protection Paladin"; break;
            case 70:  spec_name = "Retribution Paladin"; break;
            case 71:  spec_name = "Arms Warrior"; break;
            case 72:  spec_name = "Fury Warrior"; break;
            case 73:  spec_name = "Protection Warrior"; break;
            case 102: spec_name = "Balance Druid"; break;
            case 103: spec_name = "Feral Druid"; break;
            case 104: spec_name = "Guardian Druid"; break;
            case 105: spec_name = "Restoration Druid"; break;
            case 577: spec_name = "Havoc DH"; break;
            case 581: spec_name = "Vengeance DH"; break;
            case 1467: spec_name = "Devastation Evoker"; break;
            case 1468: spec_name = "Preservation Evoker"; break;
            case 1473: spec_name = "Augmentation Evoker"; break;
            default: break;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Spec: {} (id {})", spec_name, snap->identity.spec)});
        return true;
    }
    if (cmd == "whois")
    {
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Whois: {} | L{} race={} class={} spec={} faction={}",
                        snap->identity.name, snap->identity.level, snap->identity.race, snap->identity.cls,
                        snap->identity.spec, snap->identity.faction)});
        return true;
    }
    if (cmd == "teleport" || cmd == "tele")
    {
        // Owner-driven hard teleport. Bypasses /come's combat / cast gates
        // because owners use this when the bot is wedged. Bot must be
        // logged-in (sender's session/lookup is fine; we use map+pos from
        // the sender directly).
        Player* anchor = sender;
        if (!parsed.tail.empty())
        {
            Player* lookup = ObjectAccessor::FindPlayerByName(parsed.tail);
            if (!lookup)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Teleport: player '{}' not found.", parsed.tail)});
                return true;
            }
            anchor = lookup;
        }
        Push(bot, TeleportToIntent{anchor->GetMapId(),
                                   anchor->GetPositionX(),
                                   anchor->GetPositionY(),
                                   anchor->GetPositionZ(),
                                   anchor->GetOrientation()});
        Push(bot, WhisperIntent{sender->GetName(), "Teleport: queued."});
        return true;
    }
    if (cmd == "pet")
    {
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Pet: no snapshot."});
            return true;
        }
        if (snap->pet.pet_guid.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Pet: none."});
            return true;
        }
        const int32 hp_pct = snap->pet.pet_max_hp > 0
            ? (snap->pet.pet_hp * 100) / snap->pet.pet_max_hp : 0;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Pet: {} L{} fam={} hp={}% combat={}",
                        snap->pet.pet_name.empty() ? "?" : snap->pet.pet_name.c_str(),
                        snap->pet.pet_level, snap->pet.pet_family,
                        hp_pct, snap->pet.pet_in_combat ? "yes" : "no")});
        return true;
    }
    if (cmd == "petattack" || cmd == "petatk")
    {
        // Send pet to attack sender's target (or selection passed by name).
        ObjectGuid tgt = sender->GetTarget();
        if (tgt.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Petattack: no target."});
            return true;
        }
        Push(bot, PetAttackIntent{tgt});
        return true;
    }
    if ((cmd == "petcast" || cmd == "petspell") && args.size() >= 1)
    {
        // /petcast <spell_id> [target] — manual pet ability fire. The pet must
        // know the spell (server validates). Defaults target to sender's
        // selection; the API permits self-target by passing an empty guid.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        ObjectGuid tgt = sender->GetTarget();
        if (tgt.IsEmpty()) tgt = bot->GetGUID();
        Push(bot, PetCastSpellIntent{spell_id, tgt});
        return true;
    }
    if (cmd == "petdismiss" || cmd == "dismisspet")
    {
        // Manual dismiss; auto-resummon will fire from State_Idle if the bot
        // is a hunter / warlock per the pet-maintenance rule.
        Push(bot, DismissPetIntent{});
        return true;
    }
    if (cmd == "stable" || cmd == "stables" || cmd == "petstable")
    {
        // /stable                    — list active + stabled pets
        // /stable swap <num> <slot>  — move pet to slot (0..MAX_ACTIVE_PETS-1
        //                              active, 5..(5+MAX_PET_STABLES-1) stable)
        // /stable summon <num>       — summon a pet from active slot
        // /stable abandon            — untame current pet (Hunter only)
        // /stable delete <num>       — permanently delete a stabled pet
        // /stable feed <food_entry>  — Feed Pet (6991) the named food
        if (bot->GetClass() != CLASS_HUNTER)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Stable: hunter-only."});
            return true;
        }
        // Sub-verb is the first whitespace-delimited token after "stable".
        std::string sub;
        {
            size_t p = parsed.tail.find_first_not_of(" \t");
            if (p != std::string::npos)
            {
                size_t e = parsed.tail.find_first_of(" \t", p);
                sub = parsed.tail.substr(p, (e == std::string::npos ? parsed.tail.size() : e) - p);
            }
        }
        // No sub-verb → list dump. The PetStable lives on the world thread
        // and we're already there (whisper dispatch); safe to read directly.
        if (sub.empty())
        {
            PetStable* stable = bot->GetPetStable();
            if (!stable)
            {
                Push(bot, WhisperIntent{sender->GetName(), "Stable: no pet bank."});
                return true;
            }
            uint32 lines = 0;
            // ActivePets first, then StabledPets. Cap output at 8 lines so a
            // full bank doesn't flood whisper chat.
            for (size_t i = 0; i < stable->ActivePets.size() && lines < 8; ++i)
            {
                if (!stable->ActivePets[i]) continue;
                auto const& p = *stable->ActivePets[i];
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("A[{}] #{} {} L{} ent={}",
                                int(i), p.PetNumber, p.Name, p.Level, p.CreatureId)});
                ++lines;
            }
            for (size_t i = 0; i < stable->StabledPets.size() && lines < 8; ++i)
            {
                if (!stable->StabledPets[i]) continue;
                auto const& p = *stable->StabledPets[i];
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("S[{}] #{} {} L{} ent={}",
                                int(PET_SAVE_FIRST_STABLE_SLOT + i),
                                p.PetNumber, p.Name, p.Level, p.CreatureId)});
                ++lines;
            }
            if (lines == 0)
                Push(bot, WhisperIntent{sender->GetName(), "Stable: empty."});
            return true;
        }
        if (sub == "swap" && args.size() >= 2)
        {
            const uint32 pet_num = static_cast<uint32>(args[0]);
            const uint8  slot    = static_cast<uint8>(args[1]);
            Push(bot, SwapPetToSlotIntent{pet_num, slot});
            return true;
        }
        if (sub == "summon" && args.size() >= 1)
        {
            Push(bot, SummonPetByNumberIntent{static_cast<uint32>(args[0])});
            return true;
        }
        if (sub == "abandon")
        {
            Push(bot, AbandonPetIntent{});
            return true;
        }
        if (sub == "delete" && args.size() >= 1)
        {
            Push(bot, DeleteStabledPetIntent{static_cast<uint32>(args[0])});
            return true;
        }
        if (sub == "feed" && args.size() >= 1)
        {
            Push(bot, FeedPetIntent{static_cast<uint32>(args[0])});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            "Stable: usage — list | swap <num> <slot> | summon <num> | abandon | delete <num> | feed <entry>"});
        return true;
    }
    if (cmd == "feed" && args.size() >= 1)
    {
        // Shortcut for /stable feed — same Feed Pet (6991) cast path.
        Push(bot, FeedPetIntent{static_cast<uint32>(args[0])});
        return true;
    }
    if ((cmd == "qshare" || cmd == "sharequest") && args.size() >= 1)
    {
        // /qshare <quest_id> — push the bot's active quest to the party.
        // Pop the share dialog on every eligible receiver. Mirrors the
        // client's "Share Quest" right-click on the quest log entry.
        Push(bot, ShareQuestIntent{static_cast<uint32>(args[0])});
        return true;
    }
    if ((cmd == "petauto" || cmd == "petautocast") && args.size() >= 1)
    {
        // /petauto <spell_id> [on|off]   default on
        const uint32 spell_id = static_cast<uint32>(args[0]);
        bool enabled = true;
        // Look for "off" / "0" in tail to disable. Args is numeric-only so
        // "off" wouldn't be in args[]; check tail string.
        size_t p = parsed.tail.find_first_not_of(" \t");
        if (p != std::string::npos)
        {
            // Skip the spell id token then look for the next word.
            size_t e = parsed.tail.find_first_of(" \t", p);
            if (e != std::string::npos)
            {
                size_t q = parsed.tail.find_first_not_of(" \t", e);
                if (q != std::string::npos)
                {
                    std::string sub = parsed.tail.substr(q);
                    if (sub == "off" || sub == "0" || sub == "false") enabled = false;
                }
            }
        }
        Push(bot, PetToggleAutocastIntent{spell_id, enabled});
        return true;
    }
    if (cmd == "petname" || cmd == "renamepet")
    {
        // /petname <new_name> — rename hunter pet (one-time, until next tame).
        // The full text after the verb is the name (preserves spaces).
        std::string name;
        size_t p = parsed.tail.find_first_not_of(" \t");
        if (p != std::string::npos)
            name = parsed.tail.substr(p);
        if (name.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "PetName: usage — petname <name>"});
            return true;
        }
        Push(bot, RenamePetIntent{std::move(name)});
        return true;
    }
    if (cmd == "petstance" || cmd == "petstate")
    {
        // /petstance passive|defensive|aggressive|helper
        // /petstance stay|follow            — command stance (no attack;
        //                                     use /petattack for that)
        // Pet stance/command in one whisper. Reads sub-verb from `tail`.
        std::string sub;
        {
            size_t p = parsed.tail.find_first_not_of(" \t");
            if (p != std::string::npos)
            {
                size_t e = parsed.tail.find_first_of(" \t", p);
                sub = parsed.tail.substr(p, (e == std::string::npos ? parsed.tail.size() : e) - p);
            }
        }
        if      (sub == "passive")    { Push(bot, PetSetReactStateIntent{0}); return true; }
        else if (sub == "defensive")  { Push(bot, PetSetReactStateIntent{1}); return true; }
        else if (sub == "aggressive") { Push(bot, PetSetReactStateIntent{2}); return true; }
        else if (sub == "assist")     { Push(bot, PetSetReactStateIntent{3}); return true; }
        else if (sub == "stay")       { Push(bot, PetSetCommandStateIntent{0}); return true; }
        else if (sub == "follow")     { Push(bot, PetSetCommandStateIntent{1}); return true; }
        Push(bot, WhisperIntent{sender->GetName(),
            "PetStance: passive|defensive|aggressive|helper | stay|follow"});
        return true;
    }
    if (cmd == "gbank" || cmd == "guildbank")
    {
        // /gbank deposit <amount>            — deposit money into guild bank
        // /gbank withdraw <amount>           — withdraw money from guild bank
        // /gbank put <tab> <slot> <count>    — deposit current selected item
        // /gbank take <tab> <slot> <count>   — withdraw to first free slot
        //
        // Banker GameObject is auto-resolved: we walk nearby Guild Vault
        // GOs in 8yd and pick the closest. Saves the owner from naming
        // the GO every time.
        if (!bot->GetGuild())
        {
            Push(bot, WhisperIntent{sender->GetName(), "GBank: bot is not in a guild."});
            return true;
        }
        // Pick the nearest Guild Vault to the bot.
        std::list<GameObject*> nearby;
        Trinity::GameObjectInRangeCheck check(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), 8.0f);
        Trinity::GameObjectListSearcher<Trinity::GameObjectInRangeCheck> searcher(bot, nearby, check);
        Cell::VisitGridObjects(bot, searcher, 8.0f);
        ObjectGuid vault;
        for (GameObject* go : nearby)
        {
            if (go && go->GetGoType() == GAMEOBJECT_TYPE_GUILD_BANK) { vault = go->GetGUID(); break; }
        }
        if (vault.IsEmpty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "GBank: no Guild Vault within 8yd."});
            return true;
        }

        // Parse sub-verb.
        std::string sub;
        {
            size_t p = parsed.tail.find_first_not_of(" \t");
            if (p != std::string::npos)
            {
                size_t e = parsed.tail.find_first_of(" \t", p);
                sub = parsed.tail.substr(p, (e == std::string::npos ? parsed.tail.size() : e) - p);
            }
        }
        if (sub == "deposit" && args.size() >= 1)
        {
            Push(bot, GuildBankDepositMoneyIntent{vault, args[0]});
            return true;
        }
        if (sub == "withdraw" && args.size() >= 1)
        {
            Push(bot, GuildBankWithdrawMoneyIntent{vault, args[0]});
            return true;
        }
        if (sub == "put" && args.size() >= 3)
        {
            // /gbank put <tab> <bank_slot> <count> — put sender's selected
            // bot inventory item via INVENTORY_SLOT_BAG_0 + first match.
            // For full control use the API directly with bag/slot.
            const uint8  tab    = static_cast<uint8>(args[0]);
            const uint8  bslot  = static_cast<uint8>(args[1]);
            const uint32 count  = static_cast<uint32>(args[2]);
            // Pick the first non-empty inventory slot. Caller can use
            // /gbank putslot <tab> <bslot> <bag> <pslot> <count> for explicit.
            uint8 src_bag = INVENTORY_SLOT_BAG_0;
            uint8 src_slot = NULL_SLOT;
            for (uint8 s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s)
            {
                if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, s)) { src_slot = s; break; }
            }
            if (src_slot == NULL_SLOT)
            {
                Push(bot, WhisperIntent{sender->GetName(), "GBank put: no inventory item to deposit."});
                return true;
            }
            Push(bot, GuildBankDepositItemIntent{vault, tab, bslot, src_bag, src_slot, count});
            return true;
        }
        if (sub == "take" && args.size() >= 3)
        {
            const uint8  tab   = static_cast<uint8>(args[0]);
            const uint8  bslot = static_cast<uint8>(args[1]);
            const uint32 count = static_cast<uint32>(args[2]);
            // Withdraw into NULL_SLOT — Guild::SwapItemsWithInventory
            // auto-stores into first free inventory slot.
            Push(bot, GuildBankWithdrawItemIntent{vault, tab, bslot, INVENTORY_SLOT_BAG_0, NULL_SLOT, count});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            "GBank: deposit/withdraw <amount> | put <tab> <slot> <count> | take <tab> <slot> <count>"});
        return true;
    }
    if (cmd == "age")
    {
        // Snapshot freshness — diagnostic for AI staleness investigation.
        const uint64 id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Age: no snapshot."});
            return true;
        }
        const uint32 age = GameTime::GetGameTimeMS() - snap->published_at_ms;
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Age: snapshot is {}ms old (tick {})", age, snap->world_tick)});
        return true;
    }
    if (cmd == "loc")
    {
        // Compact location for owner-side macros / GM commands.
        // Format: "map x y z" — copy/paste friendly into .tele or .gps.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Loc: no snapshot."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("{} {:.2f} {:.2f} {:.2f} {:.2f}",
                        snap->position.map_id, snap->position.x, snap->position.y, snap->position.z, snap->position.o)});
        return true;
    }
    if (cmd == "where")
    {
        // Quick location reply: zone + map + coords. Useful when the owner
        // wants to navigate to the bot for buff / trade. Pulls from the
        // already-published snapshot — no live world access needed.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot published yet"});
            return true;
        }
        // Resolve human-readable map / zone / area names from DB2 stores.
        // Names use the active locale; falls back to LOCALE_enUS if missing.
        char const* map_name  = "?";
        char const* zone_name = "?";
        char const* area_name = "?";
        if (MapEntry const* me = sMapStore.LookupEntry(snap->position.map_id))
            map_name = me->MapName[sWorld->GetDefaultDbcLocale()];
        if (AreaTableEntry const* ae = sAreaTableStore.LookupEntry(snap->area.zone_id))
            zone_name = ae->AreaName[sWorld->GetDefaultDbcLocale()];
        if (AreaTableEntry const* ae = sAreaTableStore.LookupEntry(snap->area.area_id))
            area_name = ae->AreaName[sWorld->GetDefaultDbcLocale()];
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Where: {} ({}) / {} / {} @ ({:.1f}, {:.1f}, {:.1f})",
                        map_name, snap->position.map_id, zone_name, area_name,
                        snap->position.x, snap->position.y, snap->position.z)});
        // Second line: instance / pvp flags. Skipped when bot is in a plain
        // open-world sanctuary-free zone (no flags worth reporting).
        if (snap->instance_ctx.is_in_instance || snap->vitals.is_sanctuary || snap->vitals.is_ffa_pvp)
        {
            std::string flags;
            if (snap->instance_ctx.is_in_raid)        flags += "raid ";
            else if (snap->instance_ctx.is_in_dungeon) flags += "dungeon ";
            else if (snap->instance_ctx.is_in_instance) flags += "instance ";
            if (snap->instance_ctx.map_difficulty)    flags += fmt::format("diff={} ", snap->instance_ctx.map_difficulty);
            if (snap->vitals.is_sanctuary)      flags += "sanctuary ";
            if (snap->vitals.is_ffa_pvp)        flags += "ffa-pvp ";
            // Trim trailing space.
            if (!flags.empty() && flags.back() == ' ') flags.pop_back();
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Where: flags=[{}]", flags)});
        }
        return true;
    }
    if (cmd == "popstats")
    {
        // Diagnostic: live per-bracket spec coverage. Reports
        // tank/heal/dps counts per (faction, level/10) bracket so
        // operators can verify rebalance is producing the 1T:1H:3D
        // mix and spot brackets that need attention. Each line also
        // flags `[STARVED]` when tanks or healers are below
        // ceil(total/5) — the same threshold the rebalance cron
        // uses, so the report shows what the cron will target on
        // the next cycle.
        if (!Services::Initialized())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Popstats: V2 services not initialized."});
            return true;
        }
        auto report = Services::Population().BracketCoverage();
        if (report.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Popstats: no L10+ bots online."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Popstats: {} bracket(s) online", report.size())});
        for (auto const& br : report)
        {
            const bool alliance = (br.bracket_key & 0x10000u) != 0;
            const uint32 bracket = br.bracket_key & 0xFu;
            const uint16 total = br.tanks + br.healers + br.dps;
            const uint16 lo = uint16(bracket * 10);
            const uint16 hi = uint16(lo + 9);
            const uint16 target = uint16((total + 4) / 5);  // ceil(total/5)
            std::string tag;
            if (total >= 5)
            {
                if (br.tanks < target)   tag += " [TANK-STARVED]";
                if (br.healers < target) tag += " [HEAL-STARVED]";
            }
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format(" {} L{:02}-{:02}: {}T / {}H / {}D ({} total){}",
                            alliance ? "A" : "H", lo, hi,
                            br.tanks, br.healers, br.dps, total, tag)});
        }
        return true;
    }
    if (cmd == "rebalance")
    {
        // Diagnostic: bypass the 5-min throttle and run a rebalance
        // pass right now. Useful for verifying the proactive spec
        // rotation is producing sane switches without waiting for the
        // next natural cycle. The pass logs `[Rebalance] cycle
        // complete: N switches across M brackets` to worldserver.log;
        // the whisper just reports that the pass ran.
        if (!Services::Initialized())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Rebalance: V2 services not initialized."});
            return true;
        }
        Services::Population().ForceRebalance();
        Push(bot, WhisperIntent{sender->GetName(),
            "Rebalance: pass forced — see worldserver.log [Rebalance] line for switches applied."});
        return true;
    }
    if (cmd == "route" && args.size() >= 1)
    {
        // Diagnostic: query the TravelPlanner for the next hop from the
        // bot's current map to the supplied destination map. Reports the
        // first-hop intermediate (or `dest` for direct routes / 0 if
        // unreachable). Verifies that `PortalIndex::NextHopMap` produces
        // sensible chains for cross-continent goals.
        const uint32 dest_map = static_cast<uint32>(args[0]);
        const uint32 from_map = bot->GetMapId();
        if (!Services::Initialized() || !Services::Portals().IsInitialized())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Route: PortalIndex not initialized."});
            return true;
        }
        const uint32 hop = Services::Portals().NextHopMap(from_map, dest_map);
        if (hop == 0)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Route: no path from map {} to map {} (or already there).",
                            from_map, dest_map)});
        }
        else if (hop == dest_map)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Route: direct anchor {} → {}.",
                            from_map, dest_map)});
        }
        else
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Route: multi-hop {} → {} → ... → {} (next hop: {}).",
                            from_map, hop, dest_map, hop)});
        }
        return true;
    }
    if (cmd == "roadstats")
    {
        // Diagnostic: read road-aware pathfinding telemetry.
        // dtQueryFilterTC maintains atomic counters incremented per
        // CalculatePath via dtQueryFilterTC::TallyPath (called from
        // PathGenerator::CalculatePath after the path stabilises).
        //
        // Modes:
        //   /roadstats              — global summary
        //   /roadstats <mapId>      — single-map breakdown
        //   /roadstats list         — table of every map with tallied paths
        //
        // Reports:
        //   paths run, paths with ≥1 road poly (% of total),
        //   road poly fraction (total road / total polys),
        //   paths in instance (road bonus auto-disabled),
        //   paths with road bonus manually disabled (combat/short-range),
        //   paths with slope penalty > 10%.
        //
        // After 24h of normal gameplay, expect paths_with_road in the
        // 10-40% range depending on quest density vs. cross-zone travel.
        // Use /roadreset to zero the counters.
        auto formatLine = [&](char const* label, dtQueryFilterTC::RoadStats const& st) {
            return fmt::format("{} paths_run={} with_road={} ({:.1f}%) disabled={} ({:.1f}%) "
                               "in_instance={} slope_penalty={} ({:.1f}%) "
                               "road_polys={}/{} ({:.2f}%)",
                               label,
                               st.pathsRun,
                               st.pathsWithRoadPoly,
                               st.pathsRun ? 100.0 * st.pathsWithRoadPoly / st.pathsRun : 0.0,
                               st.pathsRoadBonusDisabled,
                               st.pathsRun ? 100.0 * st.pathsRoadBonusDisabled / st.pathsRun : 0.0,
                               st.pathsInInstance,
                               st.pathsWithSlopePenalty,
                               st.pathsRun ? 100.0 * st.pathsWithSlopePenalty / st.pathsRun : 0.0,
                               st.roadPolysVisited, st.totalPolysVisited,
                               st.totalPolysVisited ? 100.0 * st.roadPolysVisited / st.totalPolysVisited : 0.0);
        };

        // Sub-mode parsing. parsed.tail carries the raw arg suffix.
        std::string tail = parsed.tail;
        // trim whitespace
        while (!tail.empty() && (tail.front() == ' ' || tail.front() == '\t'))
            tail.erase(tail.begin());
        while (!tail.empty() && (tail.back() == ' ' || tail.back() == '\t'))
            tail.pop_back();

        if (tail == "list")
        {
            // Per-map table. Sorted by paths_run desc so the busiest
            // map lands at the top.
            auto maps = dtQueryFilterTC::ListMapsWithStats();
            if (maps.empty())
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    "Roadstats list: no paths tallied yet."});
                return true;
            }
            // Pull each entry, sort.
            std::vector<std::pair<uint32, dtQueryFilterTC::RoadStats>> entries;
            entries.reserve(maps.size());
            for (uint32 m : maps)
                entries.emplace_back(m, dtQueryFilterTC::SampleStatsForMap(m));
            std::sort(entries.begin(), entries.end(),
                [](auto const& a, auto const& b) { return a.second.pathsRun > b.second.pathsRun; });

            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Roadstats list: {} maps with tallied paths", entries.size())});
            for (auto const& [m, st] : entries)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    formatLine(fmt::format("  map={}:", m).c_str(), st)});
            }
            return true;
        }

        if (!tail.empty())
        {
            // Try to parse as map id. A parsed-flag (not a 0 sentinel)
            // because map 0 is Eastern Kingdoms — a perfectly valid arg.
            uint32 mapId = 0;
            bool parsed = false;
            try { mapId = static_cast<uint32>(std::stoul(tail)); parsed = true; }
            catch (...) { parsed = false; }
            if (!parsed)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Roadstats: unknown arg '{}'. Use no arg (global), <mapId>, or 'list'.", tail)});
                return true;
            }
            dtQueryFilterTC::RoadStats st = dtQueryFilterTC::SampleStatsForMap(mapId);
            if (st.pathsRun == 0)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Roadstats map={}: no paths tallied for this map.", mapId)});
                return true;
            }
            Push(bot, WhisperIntent{sender->GetName(),
                formatLine(fmt::format("Roadstats map={}:", mapId).c_str(), st)});
            return true;
        }

        // Default — global summary.
        dtQueryFilterTC::RoadStats st = dtQueryFilterTC::SampleStats();
        Push(bot, WhisperIntent{sender->GetName(), formatLine("Roadstats:", st)});
        return true;
    }
    if (cmd == "roadreset")
    {
        // Companion to /roadstats: explicit reset of the counters. Kept
        // separate from /roadstats to avoid accidentally clearing the
        // history when querying.
        dtQueryFilterTC::ResetStats();
        Push(bot, WhisperIntent{sender->GetName(),
            "Roadstats: counters reset."});
        return true;
    }
    if (cmd == "pathcompare")
    {
        // A/B compare: run the same pathfind twice — once with road
        // bonus, once without — and report the difference in cost,
        // length, and road poly count. Lets the operator confirm that
        // the road-bias system is producing visibly different routes
        // for a specific destination.
        //
        // Usage:
        //   .playerbot pathcompare <x> <y> <z>
        //   .playerbot pathcompare preset <name>
        //
        // Built-in preset routes cover well-known road corridors so the
        // operator doesn't need to memorise coords. Each preset specifies
        // (mapId, x, y, z). If the bot is on a different map, we still
        // run the pathfind — Detour will fail with NOPATH, which is a
        // legitimate signal that the operator picked the wrong preset.
        struct Preset {
            char const* name;
            uint32 mapId;
            float x, y, z;
            char const* note;
        };
        constexpr Preset presets[] = {
            // (mapId, x, y, z) — destinations chosen for known road
            // corridors that should be measurably road-biased.
            {"stormwind-goldshire",   0, -9460.0f,    55.0f,  56.0f,
                "Eastern Kingdoms south road from Stormwind gate to Goldshire"},
            {"goldshire-westfall",    0, -10650.0f,  1052.0f,  37.0f,
                "Sentinel Hill — long cobblestone road south"},
            {"stormwind-darkshire",   0, -10590.0f,  -1136.0f, 48.0f,
                "Duskwood Darkshire — winding forest road"},
            {"ironforge-kharanos",    0, -5500.0f,    -710.0f, 350.0f,
                "Dun Morogh road from Ironforge to Kharanos"},
            {"orgrimmar-razorhill",   1, 326.0f,    -4690.0f,  10.0f,
                "Durotar south road from Orgrimmar gate to Razor Hill"},
            {"orgrimmar-crossroads",  1, -440.0f,   -2570.0f,  96.0f,
                "Barrens north road from Orgrimmar to The Crossroads"},
            {"thunderbluff-bloodhoof",1, -2360.0f,  -370.0f,   -10.0f,
                "Mulgore south road from Thunder Bluff to Bloodhoof Village"},
            {"darnassus-rutth",       1, 9947.0f,    2570.0f, 1316.0f,
                "Teldrassil south road from Darnassus to Rut'theran"},
        };

        std::string tail = parsed.tail;
        // trim
        while (!tail.empty() && (tail.front() == ' ' || tail.front() == '\t'))
            tail.erase(tail.begin());
        while (!tail.empty() && (tail.back() == ' ' || tail.back() == '\t'))
            tail.pop_back();

        float x = 0.0f, y = 0.0f, z = 0.0f;

        if (tail.rfind("preset", 0) == 0)
        {
            // tail is "preset <name>" or "preset" alone (list mode).
            std::string presetName;
            auto sp = tail.find(' ');
            if (sp != std::string::npos)
                presetName = tail.substr(sp + 1);

            if (presetName.empty())
            {
                // List available presets.
                Push(bot, WhisperIntent{sender->GetName(),
                    "Pathcompare presets:"});
                for (auto const& p : presets)
                    Push(bot, WhisperIntent{sender->GetName(),
                        fmt::format("  {} (map {}) — {}", p.name, p.mapId, p.note)});
                return true;
            }

            Preset const* chosen = nullptr;
            for (auto const& p : presets)
                if (presetName == p.name) { chosen = &p; break; }
            if (!chosen)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Pathcompare: unknown preset '{}'. Use .playerbot pathcompare preset (no name) to list.", presetName)});
                return true;
            }
            if (bot->GetMapId() != chosen->mapId)
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Pathcompare: warning — bot is on map {}, preset is for map {}. Pathfind may fail.",
                                bot->GetMapId(), chosen->mapId)});
            x = chosen->x; y = chosen->y; z = chosen->z;
        }
        else if (std::sscanf(tail.c_str(), "%f %f %f", &x, &y, &z) != 3)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Pathcompare: usage: <x> <y> <z> | preset <name> | preset (list)"});
            return true;
        }

        auto runOne = [&](bool disableBonus) -> std::tuple<bool, float, uint32, uint32> {
            PathGenerator pg(bot);
            pg.SetDisableRoadBonus(disableBonus);
            // SEH-guarded: raw Detour queries from playerbot code crash on
            // the tile race (2026-06-12) — every CalculatePath goes through
            // the guard, diagnostics included.
            bool ok = BotMovement::SehSafeCalculatePath(pg, x, y, z);
            // Cost proxy: total path length (sum of straight-line
            // distances between adjacent path points). Detour doesn't
            // expose the final A* g-score, but path length is a useful
            // observable. Real cost differences show up as length deltas
            // when road bias picks a longer-but-cheaper route.
            float len = pg.GetPathLength();
            uint32 nPoints = static_cast<uint32>(pg.GetPath().size());
            // Sample current TallyPath snapshot deltas via SampleStats
            // before/after — done outside this lambda.
            return { ok, len, nPoints, 0 };
        };

        dtQueryFilterTC::RoadStats stBefore = dtQueryFilterTC::SampleStats();
        auto [okWith, lenWith, ptsWith, _w] = runOne(/*disableBonus*/ false);
        dtQueryFilterTC::RoadStats stMid = dtQueryFilterTC::SampleStats();
        auto [okWithout, lenWithout, ptsWithout, _u] = runOne(/*disableBonus*/ true);
        dtQueryFilterTC::RoadStats stAfter = dtQueryFilterTC::SampleStats();

        const uint64 roadWith    = stMid.roadPolysVisited   - stBefore.roadPolysVisited;
        const uint64 roadWithout = stAfter.roadPolysVisited - stMid.roadPolysVisited;
        const uint64 totalWith   = stMid.totalPolysVisited   - stBefore.totalPolysVisited;
        const uint64 totalWithout = stAfter.totalPolysVisited - stMid.totalPolysVisited;

        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Pathcompare to ({:.1f},{:.1f},{:.1f}):", x, y, z)});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("  with road:    ok={} len={:.1f}y points={} road_polys={}/{}",
                        okWith, lenWith, ptsWith, roadWith, totalWith)});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("  without road: ok={} len={:.1f}y points={} road_polys={}/{}",
                        okWithout, lenWithout, ptsWithout, roadWithout, totalWithout)});

        // Diff line — if road bias picked a longer route by detouring
        // onto roads, len_with > len_without by some margin AND
        // road_polys_with > road_polys_without. If they're identical,
        // no roads near the route (or bonus had no effect).
        const float lenDelta = lenWith - lenWithout;
        const int64 roadDelta = static_cast<int64>(roadWith) - static_cast<int64>(roadWithout);
        char const* verdict;
        if (roadDelta > 0 && std::abs(lenDelta) < 5.0f)
            verdict = "ROAD BIAS ACTIVE — same length, road-only detour";
        else if (roadDelta > 0)
            verdict = "ROAD BIAS ACTIVE — picked road route";
        else if (roadDelta == 0 && std::abs(lenDelta) < 0.5f)
            verdict = "no road influence on this route";
        else
            verdict = "OFF-ROAD PREFERRED — slope or no roads nearby";
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("  Δlength={:+.1f}y Δroad_polys={:+}  → {}",
                        lenDelta, roadDelta, verdict)});
        return true;
    }
    if (cmd == "inspect")
    {
        // Whisper a full BotInspector dump back. Multi-line — sent as
        // separate WhisperIntents because the chat protocol caps message
        // size and the report is several hundred bytes.
        const BotId id = bot->GetGUID().GetCounter();
        std::string report = Diagnostics::Inspect(id);
        // Split on '\n' and ship one whisper per line; an empty trailing
        // line is dropped so we don't whisper "" at the end.
        size_t start = 0;
        while (start < report.size())
        {
            size_t nl = report.find('\n', start);
            std::string line = report.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            if (!line.empty())
                Push(bot, WhisperIntent{sender->GetName(), std::move(line)});
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        return true;
    }
    if (cmd == "quest")
    {
        // Whisper-back the bot's current_objective (the one State_Idle's
        // quest-execution rules are pursuing right now) plus the matching
        // POI waypoint if available. Lets the owner see "what is this bot
        // actually doing for its current quest" without parsing /quests.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->quest_log.current_quest_id == 0)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Quest: no actionable objective."});
            return true;
        }
        auto const& obj = snap->quest_log.current_objective;
        char const* tname = "?";
        switch (obj.type)
        {
            case 0:  tname = "MONSTER";    break;
            case 1:  tname = "ITEM";       break;
            case 2:  tname = "GAMEOBJECT"; break;
            case 3:  tname = "TALKTO";     break;
            case 10: tname = "AREATRIGGER"; break;
            case 19: tname = "AREA_ENTER"; break;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Quest Q{} obj#{}: {} target={} {}/{}",
                snap->quest_log.current_quest_id, obj.storage_index, tname,
                obj.object_id, obj.progress, obj.amount)});
        if (snap->quest_log.current_objective_poi.valid)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("POI: map={} ({:.1f}, {:.1f}, {:.1f})",
                    snap->quest_log.current_objective_poi.map_id,
                    snap->quest_log.current_objective_poi.x,
                    snap->quest_log.current_objective_poi.y,
                    snap->quest_log.current_objective_poi.z)});
        }
        return true;
    }
    if (cmd == "wq" || cmd == "wquests")
    {
        // Diagnostic: report the count of available world quests in the
        // bot's current zone + the closest 3 by distance. Drives the
        // "is this bot seeing world quests?" sanity check on overworld
        // bots — empty list means the build's quest_template DB has no
        // IsWorldQuest() rows visible from the bot's current position
        // (correct behavior pre-Legion / on stripped DB dumps).
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "WQ: no snapshot."});
            return true;
        }
        auto const& wqs = snap->quest_discovery.available_world_quests;
        if (wqs.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "WQ: no world quests visible (zone empty / pre-Legion DB)."});
            if (snap->quest_log.scenario_step.scenario_id != 0)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Scenario sc={} step={} order={}",
                        snap->quest_log.scenario_step.scenario_id,
                        snap->quest_log.scenario_step.step_id,
                        snap->quest_log.scenario_step.current_step_progress)});
            }
            return true;
        }
        const float bx = snap->position.x, by = snap->position.y, bz = snap->position.z;
        std::vector<std::pair<float, size_t>> ranked;
        ranked.reserve(wqs.size());
        for (size_t i = 0; i < wqs.size(); ++i)
        {
            const float dx = wqs[i].x - bx, dy = wqs[i].y - by, dz = wqs[i].z - bz;
            ranked.emplace_back(dx*dx + dy*dy + dz*dz, i);
        }
        const size_t want = std::min<size_t>(static_cast<size_t>(3u), ranked.size());
        std::partial_sort(ranked.begin(), ranked.begin() + want, ranked.end(),
            [](auto const& a, auto const& b) { return a.first < b.first; });
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("WQ: {} visible in zone (area {})",
                wqs.size(), snap->area.area_id)});
        for (size_t i = 0; i < want; ++i)
        {
            auto const& wq = wqs[ranked[i].second];
            const float dist = std::sqrt(ranked[i].first);
            char const* tname = wq.type == 1 ? "TURNIN" : "OFFER";
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("  q={} ({}) {:.0f}y area={} cur={} gold={}",
                    wq.quest_id, tname, dist, wq.area_id,
                    wq.reward_currency_id, wq.reward_money)});
        }
        if (snap->quest_log.scenario_step.scenario_id != 0)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Scenario sc={} step={} order={}",
                    snap->quest_log.scenario_step.scenario_id,
                    snap->quest_log.scenario_step.step_id,
                    snap->quest_log.scenario_step.current_step_progress)});
        }
        return true;
    }
    if ((cmd == "craftdifficulty" || cmd == "cdiff") && args.size() >= 1)
    {
        // /craftdifficulty <spell_id> — show RecipeDifficulty resolution +
        // current skill + thresholds. Lets the owner verify why the auto-craft
        // rule is or isn't picking a recipe.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        Playerbot::RecipeMeta const* meta = Playerbot::FindRecipeMeta(spell_id);
        if (!meta)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Recipe {} not found in SkillLineAbility.", spell_id)});
            return true;
        }
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "No snapshot."});
            return true;
        }
        BotSnapshotView v{*snap};
        const uint16 cur = v.skill_value(meta->skill_line_id);
        const Playerbot::RecipeColor c = Playerbot::ResolveRecipeColor(spell_id, cur);
        char const* cname = "Unknown";
        switch (c)
        {
            case Playerbot::RecipeColor::Orange:  cname = "Orange"; break;
            case Playerbot::RecipeColor::Yellow:  cname = "Yellow"; break;
            case Playerbot::RecipeColor::Green:   cname = "Green";  break;
            case Playerbot::RecipeColor::Gray:    cname = "Gray";   break;
            default: break;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Recipe {}: {} (sk{} = {}, min={}, low={}, high={}, reagents={})",
                spell_id, cname, meta->skill_line_id, cur,
                meta->min_skill_line_rank, meta->trivial_low, meta->trivial_high,
                v.has_reagents(spell_id) ? "ok" : "MISSING")});
        return true;
    }
    if (cmd == "craft")
    {
        // /craft           — list eligible recipes (Orange/Yellow/Green with reagents)
        // /craft <spell_id> — fire that recipe immediately (respects reagents + GCD)
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Craft: no snapshot."});
            return true;
        }
        if (args.empty())
        {
            // List mode: walk known_recipes, filter by color != Gray, show
            // top 12 with color tag. Skips capped skills entirely.
            uint32 listed = 0, total = 0;
            for (uint32 spell_id : snap->spellbook.known_recipes)
            {
                Playerbot::RecipeMeta const* meta = Playerbot::FindRecipeMeta(spell_id);
                if (!meta) continue;
                BotSnapshotView v{*snap};
                if (v.is_skill_capped(meta->skill_line_id)) continue;
                const Playerbot::RecipeColor c = Playerbot::ResolveRecipeColor(spell_id, v.skill_value(meta->skill_line_id));
                if (c == Playerbot::RecipeColor::Gray || c == Playerbot::RecipeColor::Unknown) continue;
                if (!v.has_reagents(spell_id)) continue;
                ++total;
                if (listed >= 12) continue;
                char const* ctag = c == Playerbot::RecipeColor::Orange ? "Or" :
                                   c == Playerbot::RecipeColor::Yellow ? "Yl" : "Gn";
                SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
                std::string sname = si ? std::string{(*si->SpellName)[sWorld->GetDefaultDbcLocale()]} : "?";
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Craft {} [{}] sk{}={}: {}", spell_id, ctag,
                        meta->skill_line_id, v.skill_value(meta->skill_line_id), sname)});
                ++listed;
            }
            if (total == 0)
                Push(bot, WhisperIntent{sender->GetName(), "Craft: no eligible recipes (need orange/yellow/green + reagents)."});
            else if (total > listed)
                Push(bot, WhisperIntent{sender->GetName(), fmt::format("...+{} more", total - listed)});
            return true;
        }
        // Cast mode: queue the recipe.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        if (!spell_id)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Craft: usage /craft [<spell_id>]"});
            return true;
        }
        Push(bot, CastSpellIntent{spell_id, ObjectGuid::Empty});
        Push(bot, WhisperIntent{sender->GetName(), fmt::format("Craft: queued {}", spell_id)});
        return true;
    }
    if (cmd == "quests" || cmd == "ql")
    {
        // Whisper-back a compact list of the bot's quest log. Format per line:
        //   "Q123: <progress 1/3, 0/1, ...> [done]" — only first 4 objectives
        // shown to avoid wide lines. Quest names aren't in the snapshot (we'd
        // need a sObjectMgr lookup), so the owner sees IDs and can cross-ref
        // wowhead. Capped to first 12 quests (a maxed log = 25; the second
        // batch can be added if anyone needs it, but 12 fits one mobile chat
        // screen).
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->quest_log.quests.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Quests: log empty."});
            return true;
        }
        size_t shown = 0;
        for (auto const& q : snap->quest_log.quests)
        {
            if (shown >= 12) break;
            std::string line = fmt::format("Q{} L{}", q.quest_id, q.level);
            if (q.state == 1) line += " [done]";
            else if (!q.objectives.empty())
            {
                line += " obj=";
                bool first = true;
                for (auto const& o : q.objectives)
                {
                    if (o.amount == 0) continue;
                    if (!first) line += ",";
                    // Type suffix lets the owner see the obj kind at a glance:
                    // K=kill, I=item, G=gameobject, T=talkto, A=areatrigger, ?=other.
                    char tag = '?';
                    switch (o.type)
                    {
                        case /*MONSTER*/    0: tag = 'K'; break;
                        case /*ITEM*/       1: tag = 'I'; break;
                        case /*GAMEOBJECT*/ 2: tag = 'G'; break;
                        case /*TALKTO*/     3: tag = 'T'; break;
                        case /*AREATRIGGER*/10: tag = 'A'; break;
                        case /*AREATRIGGER_ENTER*/19: tag = 'A'; break;
                    }
                    line += fmt::format("{}{}/{}", tag, o.progress, o.amount);
                    first = false;
                }
            }
            Push(bot, WhisperIntent{sender->GetName(), std::move(line)});
            ++shown;
        }
        if (snap->quest_log.quests.size() > shown)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("... +{} more", snap->quest_log.quests.size() - shown)});
        return true;
    }
    if (cmd == "skills")
    {
        // Whisper-back trained skills (professions, weapons, languages,
        // armour). Format: "skill 182: 450/600". Useful for owner to see
        // which professions the bot has and at what rank.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->progression.skills.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Skills: none."});
            return true;
        }
        // One whisper per skill with name lookup from SkillLine.db2 — keeps
        // line length sane and lets owners search the chat by skill name.
        size_t shown = 0;
        for (auto const& sk : snap->progression.skills)
        {
            SkillLineEntry const* sle = sSkillLineStore.LookupEntry(sk.skill_id);
            std::string name = sle ? sle->DisplayName[sWorld->GetDefaultDbcLocale()]
                                   : std::string{"?"};
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{} ({}): {}/{}", name, sk.skill_id, sk.value, sk.max)});
            if (++shown >= 12) break;  // chat budget cap
        }
        if (snap->progression.skills.size() > shown)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("... +{} more", snap->progression.skills.size() - shown)});
        return true;
    }
    if (cmd == "recipes")
    {
        // Whisper-back known recipes with names from SpellInfo. Capped at 15
        // to keep chat light. SpellInfo lookups are O(1) hash; safe on the
        // command thread.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap || snap->spellbook.known_recipes.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Recipes: none."});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Recipes: {} known.", snap->spellbook.known_recipes.size())});
        size_t shown = 0;
        for (uint32 sid : snap->spellbook.known_recipes)
        {
            if (shown >= 15) break;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(sid, DIFFICULTY_NONE);
            std::string name = info && info->SpellName
                ? info->SpellName->Str[sWorld->GetDefaultDbcLocale()]
                : std::string{"?"};
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("  {} ({})", name, sid)});
            ++shown;
        }
        if (snap->spellbook.known_recipes.size() > shown)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("... +{} more", snap->spellbook.known_recipes.size() - shown)});
        return true;
    }
    if (cmd == "bagsummary" || cmd == "bagvalue")
    {
        // /bagsummary  → walk bag_items, sum vendor sell prices. Reports
        // total copper + per-quality bucket counts. Useful for owners
        // deciding "should I send my bot to vendor".
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Bagsummary: no snapshot."});
            return true;
        }
        uint64 total = 0;
        std::array<uint32, 8> by_quality{};  // 0..7 (poor..artifact + heirloom)
        for (auto const& it : snap->inventory.bag_items)
        {
            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(it.entry);
            if (!tmpl) continue;
            const uint64 sell_each = tmpl->GetSellPrice();
            total += sell_each * it.count;
            if (it.quality < by_quality.size())
                ++by_quality[it.quality];
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Bagsummary: total {}g {}s {}c (sell value).",
                        total / 10000, (total % 10000) / 100, total % 100)});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("By quality: poor={} common={} uncommon={} rare={} epic={} leg={}",
                        by_quality[0], by_quality[1], by_quality[2],
                        by_quality[3], by_quality[4], by_quality[5])});
        return true;
    }
    if (cmd == "log")
    {
        // /log [on|off]  → toggle per-bot verbose logging. When on, every
        // rule fire emits a TC_LOG_DEBUG to "playerbot.v2". Useful for
        // debugging without restarting the server with global logging on.
        BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter());
        if (!ai)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Log: bot not registered."});
            return true;
        }
        std::string sub = parsed.tail;
        size_t a = sub.find_first_not_of(" \t");
        if (a != std::string::npos) sub = sub.substr(a);
        if (sub == "on")        ai->set_verbose_logging(true);
        else if (sub == "off")  ai->set_verbose_logging(false);
        else if (!sub.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Log: usage /log [on|off]"});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Log: verbose={}",
                        ai->verbose_logging() ? "on" : "off")});
        return true;
    }
    if (cmd == "achieved" && args.size() >= 1)
    {
        // /achieved <achievement_id>  → check if bot has the achievement.
        // Uses Player::HasAchieved directly (world-thread access).
        const uint32 ach_id = static_cast<uint32>(args[0]);
        const bool has = bot->HasAchieved(ach_id);
        AchievementEntry const* ae = sAchievementStore.LookupEntry(ach_id);
        std::string title = ae ? ae->Title[sWorld->GetDefaultDbcLocale()]
                              : std::string{"?"};
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Achieved {} ({}): {}", ach_id, title, has ? "YES" : "no")});
        return true;
    }
    if (cmd == "achsearch" && !parsed.tail.empty())
    {
        // /achsearch <substring>  → list up to 10 achievements whose title
        // matches the substring (case-insensitive). Useful for finding the
        // achievement id to pass to /achieved.
        std::string needle = parsed.tail;
        size_t a = needle.find_first_not_of(" \t");
        if (a != std::string::npos) needle = needle.substr(a);
        std::string lower_needle = needle;
        std::transform(lower_needle.begin(), lower_needle.end(), lower_needle.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        size_t shown = 0;
        const LocaleConstant loc = sWorld->GetDefaultDbcLocale();
        for (auto const* ae : sAchievementStore)
        {
            if (!ae) continue;
            std::string title = ae->Title[loc];
            std::string lower_title = title;
            std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(),
                           [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            if (lower_title.find(lower_needle) == std::string::npos) continue;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("  {} ({}pt): {}", ae->ID, ae->Points, title)});
            if (++shown >= 10)
            {
                Push(bot, WhisperIntent{sender->GetName(), "(more matches truncated)"});
                break;
            }
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Achsearch: no titles match '{}'.", needle)});
        return true;
    }
    if (cmd == "cdwait" && args.size() >= 1)
    {
        // /cdwait <spell_id>  → ms remaining on a specific spell cooldown.
        // Returns "ready" if not on CD, "<seconds>s" otherwise. Uses the
        // snapshot's pre-computed cooldown vector (server-side SpellHistory
        // queried at snapshot pub time).
        const uint32 spell_id = static_cast<uint32>(args[0]);
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Cdwait: no snapshot."});
            return true;
        }
        for (auto const& cd : snap->cooldowns.spell_cooldowns)
        {
            if (cd.spell_id != spell_id) continue;
            if (cd.remaining.count() <= 0)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Cdwait: spell {} ready.", spell_id)});
                return true;
            }
            const int64_t ms = cd.remaining.count();
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Cdwait: spell {} {}s ({}ms)", spell_id, ms / 1000, ms)});
            return true;
        }
        // Not in cooldown vector means not on CD (or not known).
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Cdwait: spell {} not on cd.", spell_id)});
        return true;
    }
    if (cmd == "cooldowns" || cmd == "cd")
    {
        // List active spell cooldowns (those with remaining > 0). Compact:
        // "spell <id>: <secs>s" per line, capped to 8. Useful for owner
        // planning around big cooldowns (Bloodlust, Heroism, Rebirth, etc.).
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& cd : snap->cooldowns.spell_cooldowns)
        {
            if (cd.remaining.count() <= 0) continue;
            if (shown >= 8) break;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("spell {}: {}s", cd.spell_id, cd.remaining.count() / 1000)});
            ++shown;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "Cooldowns: all ready."});
        return true;
    }
    if (cmd == "inv")
    {
        // Owner-side bag inspection. One whisper per item, capped at 10.
        // /inv         — show everything
        // /inv rare    — show quality >= 3 (Rare)
        // /inv epic    — show quality >= 4 (Epic)
        // /inv leg     — show quality >= 5 (Legendary)
        // Quality cutoff lets owners scan a fresh bot's inventory for the
        // few items worth keeping without manually filtering greys.
        uint8 min_quality = 0;
        if      (parsed.tail == "rare")    min_quality = 3;
        else if (parsed.tail == "epic")    min_quality = 4;
        else if (parsed.tail == "leg")     min_quality = 5;
        else if (parsed.tail == "uncommon") min_quality = 2;
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        if (snap->inventory.bag_items.empty())
        {
            Push(bot, WhisperIntent{sender->GetName(), "Inv: bags empty."});
            return true;
        }
        size_t shown = 0, skipped = 0;
        for (auto const& it : snap->inventory.bag_items)
        {
            if (it.quality < min_quality) { ++skipped; continue; }
            if (shown >= 10) break;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{}/{}: {} x{} q={} ilvl={}",
                            it.bag, it.slot, it.entry, it.count,
                            it.quality, it.item_level)});
            ++shown;
        }
        const size_t remaining = snap->inventory.bag_items.size() > shown + skipped
            ? snap->inventory.bag_items.size() - shown - skipped : 0;
        if (remaining > 0 || skipped > 0)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("... +{} more, {} below filter (free {})",
                            remaining, skipped, snap->bags.bag_free_slots)});
        return true;
    }
    if (cmd == "equipped" || cmd == "gear")
    {
        // Owner-side equipped-gear inspection. One whisper per equipped slot.
        // Empty slots skipped. Format: "slot N: <name> entry=X ilvl=M dur=P%".
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        size_t shown = 0;
        for (uint8 slot = 0; slot < snap->inventory.equipped.size(); ++slot)
        {
            auto const& eq = snap->inventory.equipped[slot];
            if (eq.entry == 0) continue;
            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(eq.entry);
            std::string item_name = tmpl ? tmpl->GetDefaultLocaleName() : std::string{"?"};
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("slot {}: {} entry={} ilvl={} dur={}%",
                            slot, item_name, eq.entry, eq.item_level, eq.durability_pct)});
            ++shown;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(), "Equipped: nothing equipped."});
        return true;
    }
    if (cmd == "find" && args.size() >= 1)
    {
        // Owner debug: where in the bot's bags is item entry X? Walks
        // snapshot.bag_items and reports bag/slot/count for each match.
        const uint32 entry = static_cast<uint32>(args[0]);
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& it : snap->inventory.bag_items)
        {
            if (it.entry != entry) continue;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("bag {} slot {} x{}", it.bag, it.slot, it.count)});
            ++shown;
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Find: item {} not in bags.", entry)});
        return true;
    }
    if (cmd == "spell" && args.size() >= 1)
    {
        // /spell <id>  → name + cast time + cooldown + range. Useful when an
        // owner wants to check a spell id from the snapshot's known-spell list
        // before issuing /cast or /petauto.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
        if (!info)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Spell: id {} not found.", spell_id)});
            return true;
        }
        std::string name = info->SpellName ? info->SpellName->Str[sWorld->GetDefaultDbcLocale()] : std::string{"?"};
        const int32 cast_ms = info->CalcCastTime();
        const int32 cd_ms   = static_cast<int32>(info->RecoveryTime);
        const int32 cd_cat  = static_cast<int32>(info->CategoryRecoveryTime);
        const float range   = info->GetMaxRange(/*positive=*/true);
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Spell {}: {} cast={}ms cd={}/{}ms range={:.1f}",
                        spell_id, name, cast_ms, cd_ms, cd_cat, range)});
        return true;
    }
    if (cmd == "itemid")
    {
        // /itemid <chat_link>  → reply with the entry id parsed from
        // |Hitem:NNN:...|h[Name]|h|r. Useful when the owner pastes a link
        // from chat and wants the numeric id for /find or /buy.
        const std::string tail = parsed.tail;
        const size_t hpos = tail.find("Hitem:");
        if (hpos == std::string::npos)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Itemid: paste an item link."});
            return true;
        }
        const size_t start = hpos + 6;
        const size_t end = tail.find(':', start);
        if (end == std::string::npos)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Itemid: malformed link."});
            return true;
        }
        std::string num = tail.substr(start, end - start);
        uint32 entry = 0;
        try { entry = static_cast<uint32>(std::stoul(num)); }
        catch (...) {}
        if (!entry)
        {
            Push(bot, WhisperIntent{sender->GetName(), "Itemid: failed to parse entry."});
            return true;
        }
        ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(entry);
        std::string name = tmpl ? tmpl->GetDefaultLocaleName() : std::string{"(unknown)"};
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Itemid: {} ({})", entry, name)});
        return true;
    }
    if (cmd == "findname")
    {
        // /findname <substring>  → list bag items whose name contains the
        // (case-insensitive) substring. Walks snapshot.bag_items and resolves
        // each entry's name via sObjectMgr.
        std::string needle = parsed.tail;
        size_t a = needle.find_first_not_of(" \t");
        if (a == std::string::npos)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                "Findname: usage /findname <substring>"});
            return true;
        }
        needle = needle.substr(a);
        std::string lower_needle = needle;
        std::transform(lower_needle.begin(), lower_needle.end(), lower_needle.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot."});
            return true;
        }
        size_t shown = 0;
        for (auto const& it : snap->inventory.bag_items)
        {
            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(it.entry);
            if (!tmpl) continue;
            std::string item_name = tmpl->GetDefaultLocaleName();
            std::string lower_name = item_name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                           [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            if (lower_name.find(lower_needle) == std::string::npos) continue;
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("{} bag {} slot {} x{} ({})",
                            it.entry, it.bag, it.slot, it.count, item_name)});
            ++shown;
            if (shown >= 10)  // cap output for chat readability
            {
                Push(bot, WhisperIntent{sender->GetName(), "(more matches truncated; refine query)"});
                break;
            }
        }
        if (shown == 0)
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Findname: no bag items match '{}'.", needle)});
        return true;
    }
    if (cmd == "useitem" && args.size() >= 1)
    {
        // Owner-driven item use by entry. Targets sender's selection or
        // self if none. Useful to manually fire a healthstone, potion,
        // or quest item that auto-rules don't pick up.
        const uint32 entry = static_cast<uint32>(args[0]);
        ObjectGuid target = sender->GetTarget();
        if (target.IsEmpty()) target = bot->GetGUID();
        Push(bot, UseItemByEntryIntent{entry, target});
        return true;
    }
    if (cmd == "useslot" && args.size() >= 2)
    {
        // /useslot <bag> <slot> — fire item at specific bag/slot. Useful when
        // there are duplicate stacks (e.g. multiple healthstones) and the
        // owner wants to drain a specific one.
        const uint8 bag  = static_cast<uint8>(args[0]);
        const uint8 slot = static_cast<uint8>(args[1]);
        ObjectGuid target = sender->GetTarget();
        if (target.IsEmpty()) target = bot->GetGUID();
        Push(bot, UseItemIntent{bag, slot, target});
        return true;
    }
    if (cmd == "equip" && args.size() >= 1)
    {
        // Owner-driven equip by entry. Looks up the item in the snapshot's
        // bag_items, uses its pre-resolved equip_slot. Skips items whose
        // FindEquipSlot returned NULL_SLOT (wrong class / armour subclass).
        const uint32 entry = static_cast<uint32>(args[0]);
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap)
        {
            Push(bot, WhisperIntent{sender->GetName(), "no snapshot."});
            return true;
        }
        for (auto const& it : snap->inventory.bag_items)
        {
            if (it.entry != entry) continue;
            if (it.equip_slot == 0xFF)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Equip: item {} not equippable for this bot.", entry)});
                return true;
            }
            Push(bot, EquipItemIntent{it.bag, it.slot, it.equip_slot});
            Push(bot, WhisperIntent{sender->GetName(),
                fmt::format("Equip: queued {} → slot {}.", entry, it.equip_slot)});
            return true;
        }
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Equip: item {} not in bags.", entry)});
        return true;
    }
    if (cmd == "portal" || cmd == "teleport")
    {
        // Mage city portal / teleport. Walks the bot's known_spells
        // looking for a spell whose name starts with "Portal: <city>"
        // (cmd=portal, opens a portal GO the owner can walk through)
        // or "Teleport: <city>" (cmd=teleport, self-casts to that city).
        // City name match is case-insensitive substring against the
        // localised spell name, so /portal stormwind, /portal SW, and
        // /portal Stormwind all match the same spell. Mage-only.
        if (parsed.tail.empty())
        { Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("{}: usage <city>", cmd == "portal" ? "Portal" : "Teleport")});
          return true; }
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        if (snap->identity.cls != CLASS_MAGE)
        { Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("{}: mage only.", cmd == "portal" ? "Portal" : "Teleport")});
          return true; }
        // Lower-case the requested city for case-insensitive match.
        std::string want;
        want.reserve(parsed.tail.size());
        for (char c : parsed.tail)
            want.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        char const* prefix = cmd == "portal" ? "portal:" : "teleport:";
        const size_t prefix_len = std::strlen(prefix);
        // Walk known_spells; each spell whose lower-cased name starts
        // with the prefix is a candidate. If the candidate's tail
        // contains `want`, that's our match. (Multi-language clients
        // would localise — we use the default DBC locale, matching the
        // existing spell-name lookup paths in this file.)
        uint32 matched = 0;
        std::string matched_name;
        for (uint32 sid : snap->spellbook.known_spells)
        {
            SpellInfo const* si = sSpellMgr->GetSpellInfo(sid, DIFFICULTY_NONE);
            if (!si || !si->SpellName) continue;
            std::string nm = si->SpellName->Str[sWorld->GetDefaultDbcLocale()];
            std::string lo;
            lo.reserve(nm.size());
            for (char c : nm) lo.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))));
            if (lo.size() < prefix_len ||
                lo.compare(0, prefix_len, prefix) != 0) continue;
            if (lo.find(want, prefix_len) == std::string::npos) continue;
            matched = sid;
            matched_name = std::move(nm);
            break;
        }
        if (matched == 0)
        { Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("{}: no '{}' spell known. (Known city portals/teleports require Mage city teleport / portal training.)",
                cmd == "portal" ? "Portal" : "Teleport", parsed.tail)});
          return true; }
        // Cast on self for both portal (creates summon GO at caster
        // position) and teleport (self-target spell). Owner walks
        // through the resulting portal.
        Push(bot, CastSpellIntent{matched, bot->GetGUID()});
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("{}: casting '{}'.",
                cmd == "portal" ? "Portal" : "Teleport", matched_name)});
        return true;
    }
    if (cmd == "rebirth")
    {
        // Druid combat rez — Rebirth (20484). Targets sender's selection
        // (or sender). Druid-only; we whisper-back if not a druid.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        if (snap->identity.cls != CLASS_DRUID)
        { Push(bot, WhisperIntent{sender->GetName(), "Rebirth: druid only."});
          return true; }
        constexpr uint32 REBIRTH = 20484;
        bool known = false;
        for (uint32 sid : snap->spellbook.known_spells) if (sid == REBIRTH) { known = true; break; }
        if (!known)
        { Push(bot, WhisperIntent{sender->GetName(), "Rebirth: not yet learned."});
          return true; }
        ObjectGuid target = sender->GetTarget();
        if (target.IsEmpty()) target = sender->GetGUID();
        Push(bot, CastSpellIntent{REBIRTH, target});
        return true;
    }
    if (cmd == "soulstone")
    {
        // Warlock pre-rez — Soulstone (20707). Casts on selection (or
        // sender) to apply the stone; target self-rezzes on death.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        if (snap->identity.cls != CLASS_WARLOCK)
        { Push(bot, WhisperIntent{sender->GetName(), "Soulstone: warlock only."});
          return true; }
        constexpr uint32 SOULSTONE = 20707;
        bool known = false;
        for (uint32 sid : snap->spellbook.known_spells) if (sid == SOULSTONE) { known = true; break; }
        if (!known)
        { Push(bot, WhisperIntent{sender->GetName(), "Soulstone: not yet learned."});
          return true; }
        ObjectGuid target = sender->GetTarget();
        if (target.IsEmpty()) target = sender->GetGUID();
        Push(bot, CastSpellIntent{SOULSTONE, target});
        return true;
    }
    if (cmd == "ankh" || cmd == "reincarnate")
    {
        // Shaman self-rez — Reincarnation (20608). Self-only; consumes
        // an Ankh item (modern WoW removed the item requirement, but
        // the spell still self-targets).
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        if (snap->identity.cls != CLASS_SHAMAN)
        { Push(bot, WhisperIntent{sender->GetName(), "Ankh: shaman only."});
          return true; }
        constexpr uint32 REINCARNATION = 20608;
        bool known = false;
        for (uint32 sid : snap->spellbook.known_spells) if (sid == REINCARNATION) { known = true; break; }
        if (!known)
        { Push(bot, WhisperIntent{sender->GetName(), "Ankh: not yet learned."});
          return true; }
        Push(bot, CastSpellIntent{REINCARNATION, bot->GetGUID()});
        return true;
    }
    if (cmd == "heal")
    {
        // Owner-driven heal cast on sender's selection (or sender if none).
        // Only meaningful for healers (table returns 0 for non-healer specs);
        // we whisper-back when class can't heal so owner doesn't wonder why
        // nothing happens.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        const uint32 spell = ClassOocHeal(snap->identity.cls, snap->identity.spec);
        if (!spell)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Heal: this bot has no healing spec."});
            return true;
        }
        ObjectGuid target = sender->GetTarget();
        if (target.IsEmpty()) target = sender->GetGUID();
        Push(bot, CastSpellIntent{spell, target});
        return true;
    }
    if (cmd == "dispel")
    {
        // Manual friendly dispel on sender's selection (or sender). Picks
        // the spec's friendly dispel spell from the shared class table.
        // The bot won't filter on actual debuff types — server-side cast
        // gating handles "no dispellable debuff" by failing silently.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        const ClassDispelSpell d = FriendlyDispel(snap->identity.cls, snap->identity.spec);
        if (!d.spell_id)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Dispel: this bot has no friendly dispel."});
            return true;
        }
        ObjectGuid target = sender->GetTarget();
        if (target.IsEmpty()) target = sender->GetGUID();
        Push(bot, CastSpellIntent{d.spell_id, target});
        return true;
    }
    if (cmd == "buff")
    {
        // Owner-driven self-buff cast — fires the class self-buff once at
        // the sender (typical use: "buff me" before a pull). Self-target
        // for classes with no group raid buff (Paladin/DK/Hunter/etc.); the
        // table returns 0 there and we whisper-back.
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        const uint32 buff = ClassSelfBuff(snap->identity.cls);
        if (!buff)
        {
            Push(bot, WhisperIntent{sender->GetName(),
                                    "Buff: this bot has no class raid buff."});
            return true;
        }
        ObjectGuid target = sender->GetTarget();
        if (target.IsEmpty()) target = sender->GetGUID();
        Push(bot, CastSpellIntent{buff, target});
        return true;
    }
    if ((cmd == "say" || cmd == "yell" || cmd == "em" || cmd == "me" ||
         cmd == "g"   || cmd == "o"    || cmd == "officer" ||
         cmd == "raid" || cmd == "rw" || cmd == "raidwarn" ||
         cmd == "p") && !parsed.tail.empty())
    {
        // Owner-driven chat broadcast. /say + /yell go through normal world
        // chat range; /em is a custom emote message; /g is guild chat;
        // /o or /officer is officer-only guild chat (server filters by rank);
        // /raid is raid chat; /rw or /raidwarn is raid-warning (leader/asst);
        // /p is party chat.
        if (cmd == "say")  Push(bot, SayChatIntent{parsed.tail});
        if (cmd == "yell") Push(bot, YellChatIntent{parsed.tail});
        if (cmd == "em" || cmd == "me") Push(bot, EmoteChatIntent{parsed.tail});
        if (cmd == "g")    Push(bot, GuildChatIntent{parsed.tail});
        if (cmd == "o" || cmd == "officer") Push(bot, OfficerChatIntent{parsed.tail});
        if (cmd == "raid") Push(bot, RaidChatIntent{parsed.tail});
        if (cmd == "rw" || cmd == "raidwarn") Push(bot, RaidWarningIntent{parsed.tail});
        if (cmd == "p")    Push(bot, PartyChatIntent{parsed.tail});
        return true;
    }
    if (cmd == "cancelaura" && args.size() >= 1)
    {
        // Cancel a self aura by spell id. Owner uses /auras to find the
        // spell id, then /cancelaura <id> to drop it. Refused if the bot
        // doesn't have the aura (NotKnown — surfaces as no-op).
        Push(bot, CancelAuraIntent{static_cast<uint32>(args[0])});
        return true;
    }
    if (cmd == "teach" && args.size() >= 1)
    {
        // Owner spell-availability probe. Reports knows / cooldown / GCD
        // for a specific spell id without actually casting. Useful for
        // "why didn't bot cast Bloodlust?" debugging.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        const BotId id = bot->GetGUID().GetCounter();
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return true;
        const BotSnapshotView v{*snap};
        const bool known_spell = v.knows_spell(spell_id);
        const bool ready_spell = v.is_ready(spell_id);
        const auto cd_ms = v.cd_remaining(spell_id);
        Push(bot, WhisperIntent{sender->GetName(),
            fmt::format("Spell {}: known={} ready={} cd_remain={}ms",
                        spell_id, known_spell, ready_spell, cd_ms.count())});
        return true;
    }
    if (cmd == "cast" && args.size() >= 1)
    {
        // Generic owner-driven cast.
        //   /cast <spell_id>            — target sender's selection (or sender)
        //   /cast <spell_id> <name>     — resolve target by player name
        //   /cast <spell_id> me         — target sender explicitly
        //   /cast <spell_id> bot        — target the bot itself
        // Server gates on the bot knowing the spell, range, los, gcd, etc.
        const uint32 spell_id = static_cast<uint32>(args[0]);
        ObjectGuid target;
        // Pull the second whitespace-separated token from parsed.tail for
        // the name-target form. The args parser already extracted the spell_id;
        // parsed.tail still holds the full "spell_id name" string after the cmd.
        std::string tail2;
        {
            size_t sp = parsed.tail.find_first_of(" \t");
            if (sp != std::string::npos)
                tail2 = parsed.tail.substr(parsed.tail.find_first_not_of(" \t", sp));
        }
        if (tail2 == "me" || tail2 == "self")
            target = sender->GetGUID();
        else if (tail2 == "bot" || tail2 == "you")
            target = bot->GetGUID();
        else if (!tail2.empty() && !std::isdigit(static_cast<unsigned char>(tail2[0])))
        {
            Player* lookup = ObjectAccessor::FindPlayerByName(tail2);
            if (!lookup)
            {
                Push(bot, WhisperIntent{sender->GetName(),
                    fmt::format("Cast: player '{}' not found.", tail2)});
                return true;
            }
            target = lookup->GetGUID();
        }
        else
        {
            target = sender->GetTarget();
            if (target.IsEmpty()) target = sender->GetGUID();
        }
        Push(bot, CastSpellIntent{spell_id, target});
        return true;
    }
    if (cmd == "help" || cmd == "commands" || cmd == "?")
    {
        // One-shot command index. Whispered back as a few lines so the owner
        // can discover what's available without `git grep`. Keep grouped by
        // intent class so it reads scannable. Update when commands land.
        // Squad-control help (Phase H of OWNER_SQUAD_CONTROL_PLAN.md).
        // Address prefixes go ahead of any command:  all: <cmd>,
        // squad: <cmd>, here: <cmd>, tank: <cmd>, mage: <cmd>,
        // <botname>: <cmd>. Without a prefix, the whisper applies
        // to the recipient bot only.
        Push(bot, WhisperIntent{sender->GetName(),
            "address: <bot>: | all: | squad: | here: | tank:|healer:|dps: | <class>: | <spec>: <command>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "manual: follow, come, stay, hold, resume, attack, disengage, focus, assist, kill, formation <type>, spread, tight, regroup, slot <n>, status"});
        Push(bot, WhisperIntent{sender->GetName(),
            "movement: follow, come/summon, stay/halt, mount, dismount, hearth, jump, dismiss"});
        Push(bot, WhisperIntent{sender->GetName(),
            "combat: attack, stop/passive, cast <spell_id>, heal, dispel, buff, reset"});
        Push(bot, WhisperIntent{sender->GetName(),
            "loot/inv: sell [<bag> <slot> [<n>]], repair, use, bank, buy <entry> [<n>], buyfood, buypot, buybandage, buyall"});
        Push(bot, WhisperIntent{sender->GetName(),
            "social: leave, ready, promote, kick/uninvite, convert/toraid, readycheck/rc, assist+/-, resetinstances/ri, decline, talk/interact"});
        Push(bot, WhisperIntent{sender->GetName(),
            "guild: gaccept, gdecline, gquit/gleave (chat already covered: g <text>)"});
        Push(bot, WhisperIntent{sender->GetName(),
            "misc2: afk, dnd, pvp, dungeon/ddiff <id>, rdiff/raiddiff <id> [legacy], o/officer <text>, rw/raidwarn <text>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "social2: addfriend/afriend <name> [<note>], removefriend/rfriend <name>, addignore/ignore <name>, removeignore/unignore <name>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "mail2: mailsend <amount> <recipient> [<subject>] (postage 30c flat); mailitem <entry> <recipient> [<count>]"});
        Push(bot, WhisperIntent{sender->GetName(),
            "diag2: speed/movespeed, casting/cast?, personality/perso, tell/w/msg <name> <text>, itemid <link>, findname <substring>, spell <id>, healassign, groupbuffs/gbuffs, dispelable, raidframe/rf, threat, follow tank|healer, aura <id> [target], buffwhois <id>, manapct/mp, inrange <yards>, history, cdwait <id>, achieved <id>, achsearch <substring>, log [on|off], bagsummary/bagvalue"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast: come_all/summon_all (port all group bots), stay_all/halt_all (stop all), follow_all [name] (all follow), unfollow_all (stop all follows), resume_all (clear pull pause), cdreset_all (reset all CDs), pull (already broadcasts)"});
        Push(bot, WhisperIntent{sender->GetName(),
            "calendar: rsvpall/calaccept [decline]"});
        Push(bot, WhisperIntent{sender->GetName(),
            "emotes: dance, wave, salute, perform/doemote <emote_id>, lookat/face, distance/dist"});
        Push(bot, WhisperIntent{sender->GetName(),
            "pet ext: pet (status), petattack/petatk, petcast/petspell <id> [tgt], petdismiss/dismisspet"});
        Push(bot, WhisperIntent{sender->GetName(),
            "stable: stable [list|swap <num> <slot>|summon <num>|abandon|delete <num>|feed <entry>], feed <entry>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "gbank: gbank deposit/withdraw <amount>, put/take <tab> <slot> <count> (auto-finds Vault in 8yd)"});
        Push(bot, WhisperIntent{sender->GetName(),
            "qshare: qshare/sharequest <quest_id> — push bot's active quest to the party"});
        Push(bot, WhisperIntent{sender->GetName(),
            "pet ctrl: petstance/petstate <passive|defensive|aggressive|assist|stay|follow>, petname/renamepet <name>, petauto <spell> [on|off]"});
        Push(bot, WhisperIntent{sender->GetName(),
            "questing: bind (innkeeper), train (trainer), talents, abandon <quest_id>, abandonall"});
        Push(bot, WhisperIntent{sender->GetName(),
            "queues: bg <type>, unbg, lfg <id>, unlfg, fly <node>, flyto, queues"});
        Push(bot, WhisperIntent{sender->GetName(),
            "ah: auction <entry> <buyout>, cancel <id>, cancelall, auctions"});
        Push(bot, WhisperIntent{sender->GetName(),
            "death: release, revive ; misc: mail, stats, inspect, where, aggro, auras"});
        Push(bot, WhisperIntent{sender->GetName(),
            "introspection: quests/ql, skills, recipes, cooldowns/cd, find <entry>, level/lvl, money/gold, dur, whois"});
        Push(bot, WhisperIntent{sender->GetName(),
            "manual: useitem <entry>, equip <entry>, cancelaura <id>, teach <id>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "intro2: inv, equipped/gear, group/roster, spec, level/lvl, xp, scout, checkmail"});
        Push(bot, WhisperIntent{sender->GetName(),
            "chat: say/yell/em/g/raid/p <text>, sit, stand, unstuck, reset, dismiss"});
        Push(bot, WhisperIntent{sender->GetName(),
            "v2 ext: assist, target <entry>, focus <name|clear>, mark <icon>, clearmark, setrole <role>, setspec <id>, upgrades [apply], unfollow, aoe [on|off], pet, petattack"});
        Push(bot, WhisperIntent{sender->GetName(),
            "v2 status: honor, currency/wallet, rep/reputation, talents [apply], who/nearby, loc, teleport <name>, sheet, stock, trinkets, pull [<sec>]"});
        Push(bot, WhisperIntent{sender->GetName(),
            "v2 diag: last, state, perf, queue, skip, clearloot, wait <ms>, cdreset"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast2: dismount_all, mount_all, sit_all, stand_all, release_all, revive_all, dismiss_all, hearth_all, ready_all, afk_all"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast3: repair_all, sell_all, buyfood_all, buypot_all, buybandage_all, buyall_all, train_all"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast4: cast_all <id>, stop_all, dance_all, wave_all, salute_all, perform_all <id>, lookat_all, jump_all, reset_all"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast5: say_all <text>, yell_all <text>, p_all <text>, em_all <text>, cancelaura_all <id>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast6: aoe_all [on|off], setrole_all <role>, verbose [on|off], verbose_all, follow_distance/fd <n>, fd_all <n>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "diag_all: where_all, state_all, queue_all, wait_all <ms>, dashboard/board, gold_all, dur_all, upgrades_all [apply]"});
        Push(bot, WhisperIntent{sender->GetName(),
            "diag_all2: aura_all <id>, casting_all, clearloot_all, unstuck_all [yd], unbg_all, unlfg_all"});
        Push(bot, WhisperIntent{sender->GetName(),
            "diag_all3: xp_all/level_all, skills_all, personality_all, talents_all [apply], honor_all, auracount_all"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast7: bind_all (innkeeper), bg_all <type>, lfg_all <dungeon>, buff_all <spell>, whisper_all <player> <text>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "prefs: clearprefs (reset focus/role/follow_dist/verbose/aoe), clearprefs_all, setspec_all <id>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast8: assist_all [<name>], focus_all <me|name|clear>, skip_all (drop loot heads), find_all <entry>"});
        Push(bot, WhisperIntent{sender->GetName(),
            "broadcast9: buffclass_all (each bot's class buff), sheet_all (per-bot crit/haste/mast/vers)"});
        return true;
    }

    return false;
}

} // namespace Playerbot
