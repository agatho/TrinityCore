#include "AddonControl.h"

#include "Bot/BotAI.h"
#include "Bot/BotCommandParser.h"
#include "Bot/BotPersonality.h"
#include "Bot/BotRegistry.h"
#include "Bot/BotRng.h"
#include "Bot/BotSnapshot.h"
#include "Bot/BotTypes.h"
#include "Threading/TickScheduler.h"
#include "Diagnostics/PerfCounters.h"
#include "Fleet/OwnerRegistry.h"
#include "Fleet/BotIdentityRegistry.h"
#include "Services.h"
#include "Session/BotSessionMgr.h"
#include "Threading/SnapshotPublisher.h"
#include "Util/ConfigReader.h"

#include "CharacterCache.h"
#include "Chat.h"
#include "ChatPackets.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Language.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::AddonControl {

namespace {

// ----------------------------------------------------------------------------
// Atomic sequence number for server-pushed frames. Distinct from client seq
// since the two streams interleave on the same envelope shape; collisions
// would confuse the LUA assembler.
// ----------------------------------------------------------------------------
std::atomic<uint32_t> g_server_seq{0x80000000u};

uint32_t NextServerSeq()
{
    uint32_t v = g_server_seq.fetch_add(1, std::memory_order_relaxed);
    if (v == 0) v = g_server_seq.fetch_add(1, std::memory_order_relaxed);
    return v;
}

// ----------------------------------------------------------------------------
// Per-account reassembly state. Keys by accountId so a player switching
// between alts on the same session inherits in-flight buffers naturally
// (though the addon resets on /reload, this almost never matters).
// ----------------------------------------------------------------------------
struct ReassemblyKey { uint32 account_id; uint32 seq; };
struct ReassemblyKeyHash {
    size_t operator()(ReassemblyKey const& k) const noexcept
    {
        return (size_t(k.account_id) * 2654435761u) ^ size_t(k.seq);
    }
};
inline bool operator==(ReassemblyKey const& a, ReassemblyKey const& b)
{
    return a.account_id == b.account_id && a.seq == b.seq;
}

struct PartialFrame
{
    uint32           total = 0;
    std::string      mtype;
    std::vector<std::string> parts;     // 1-based index storage [parts[0] unused]
    size_t           received = 0;
    uint32_t         deadline_unix = 0; // wall-clock unix sec when we drop
};

std::mutex                                                              g_rx_mtx;
std::unordered_map<ReassemblyKey, PartialFrame, ReassemblyKeyHash>      g_rx;

// Reap stale reassembly buffers. Called opportunistically on each inbound
// frame so we don't need a dedicated reaper thread for a low-volume buffer.
void ReapStaleReassemblies_locked()
{
    const uint32_t now = uint32_t(std::time(nullptr));
    for (auto it = g_rx.begin(); it != g_rx.end(); )
    {
        if (it->second.deadline_unix && now > it->second.deadline_unix)
            it = g_rx.erase(it);
        else
            ++it;
    }
}

// ----------------------------------------------------------------------------
// Escape / unescape — mirror Comms.lua exactly. `|` -> "\p", `\` -> "\\".
// ----------------------------------------------------------------------------
std::string Escape(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        if (c == '\\') out.append("\\\\");
        else if (c == '|') out.append("\\p");
        else out.push_back(c);
    }
    return out;
}

std::string Unescape(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size())
        {
            char nx = s[i + 1];
            if      (nx == 'p')  { out.push_back('|');  i++; }
            else if (nx == '\\') { out.push_back('\\'); i++; }
            else                 { out.push_back(nx);   i++; }
        }
        else
        {
            out.push_back(c);
        }
    }
    return out;
}

// Split on UNESCAPED pipes — escape sequences contain `\\p` which must not
// be treated as a separator. The LUA splitPipes does the same dance.
std::vector<std::string> SplitPipes(std::string_view s)
{
    std::vector<std::string> out;
    out.reserve(8);
    size_t start = 0;
    for (size_t i = 0; i < s.size(); )
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            i += 2;
        }
        else if (s[i] == '|')
        {
            out.push_back(Unescape(s.substr(start, i - start)));
            start = i + 1;
            ++i;
        }
        else
        {
            ++i;
        }
    }
    out.push_back(Unescape(s.substr(start)));
    return out;
}

// ----------------------------------------------------------------------------
// Send path. Build a CHAT_MSG_WHISPER addon packet from the player to
// themselves and ship it down their session — the LUA CHAT_MSG_ADDON event
// fires on receipt and the addon's OnAddonMsg dispatches by prefix.
// ----------------------------------------------------------------------------
void SendAddonRaw(WorldSession* sess, std::string const& body)
{
    if (!sess) return;
    Player* p = sess->GetPlayer();
    if (!p) return;
    WorldPackets::Chat::Chat packet;
    // sender == receiver — Blizz client routes self-addressed WHISPER addon
    // messages straight into the CHAT_MSG_ADDON event handler. No real
    // recipient required, so PBCFLEET (a fake name) just works.
    packet.Initialize(CHAT_MSG_WHISPER, LANG_ADDON, p, p, body, 0, "",
                      DEFAULT_LOCALE, std::string(kPrefix));
    sess->SendPacket(packet.Write());
}

// Chunk a logical payload into 240-byte frames and ship each as a separate
// addon-whisper. The wire envelope is "v|seqHex|idx|tot|MTYPE|payload";
// caller supplies the body, we attach the envelope + chunking.
void SendFrame(WorldSession* sess, uint32_t seq, std::string_view mtype,
               std::string const& body)
{
    if (!sess) return;
    const size_t n = body.size();
    const size_t total = (n + kMaxChunkBytes - 1) / std::max<size_t>(kMaxChunkBytes, 1);
    const size_t chunks = std::max<size_t>(total, 1);
    for (size_t i = 0; i < chunks; ++i)
    {
        const size_t off = i * kMaxChunkBytes;
        const size_t len = std::min(kMaxChunkBytes, n - off);
        std::string frame;
        frame.reserve(40 + len);
        char head[80];
        std::snprintf(head, sizeof(head), "%s|%08x|%zu|%zu|",
                      kProtoVersion, seq, i + 1, chunks);
        frame.append(head);
        frame.append(mtype);
        frame.push_back('|');
        frame.append(body, off, len);
        SendAddonRaw(sess, frame);
    }
}

// Convenience: build a pipe-joined body from already-escaped fields, then
// ship via SendFrame. Callers pass pre-escaped strings; SendFields does NOT
// re-escape (callers control which fields are user-controlled).
void SendFields(WorldSession* sess, uint32_t seq, std::string_view mtype,
                std::vector<std::string> const& escapedFields)
{
    std::string body;
    for (size_t i = 0; i < escapedFields.size(); ++i)
    {
        if (i) body.push_back('|');
        body.append(escapedFields[i]);
    }
    SendFrame(sess, seq, mtype, body);
}

// EVENT_PUSH helper for soft errors back to the client. Severity is one of
// "info"/"warn"/"error"; the LUA debug panel filters on this.
void PushEvent(WorldSession* sess, std::string_view severity,
               std::string_view bot_guid, std::string_view event,
               std::string_view detail)
{
    if (!sess) return;
    std::vector<std::string> fields = {
        Escape(severity), Escape(bot_guid), Escape(event), Escape(detail),
    };
    SendFields(sess, NextServerSeq(), "EVENT_PUSH", fields);
}

// ----------------------------------------------------------------------------
// Helpers — class / race / role token lookups so the LUA RAID_CLASS_COLORS
// table lights up properly.
// ----------------------------------------------------------------------------
char const* ClassToken(uint8 cls)
{
    switch (cls)
    {
        case  1: return "WARRIOR";
        case  2: return "PALADIN";
        case  3: return "HUNTER";
        case  4: return "ROGUE";
        case  5: return "PRIEST";
        case  6: return "DEATHKNIGHT";
        case  7: return "SHAMAN";
        case  8: return "MAGE";
        case  9: return "WARLOCK";
        case 10: return "MONK";
        case 11: return "DRUID";
        case 12: return "DEMONHUNTER";
        case 13: return "EVOKER";
        default: return "UNKNOWN";
    }
}

char const* RaceToken(uint8 race)
{
    switch (race)
    {
        case  1: return "HUMAN";
        case  2: return "ORC";
        case  3: return "DWARF";
        case  4: return "NIGHTELF";
        case  5: return "UNDEAD";
        case  6: return "TAUREN";
        case  7: return "GNOME";
        case  8: return "TROLL";
        case  9: return "GOBLIN";
        case 10: return "BLOODELF";
        case 11: return "DRAENEI";
        case 22: return "WORGEN";
        case 24: return "PANDAREN";
        case 25: return "PANDAREN_A";
        case 26: return "PANDAREN_H";
        case 27: return "NIGHTBORNE";
        case 28: return "HIGHMOUNTAINTAUREN";
        case 29: return "VOIDELF";
        case 30: return "LIGHTFORGEDDRAENEI";
        case 31: return "ZANDALARITROLL";
        case 32: return "KULTIRAN";
        case 34: return "DARKIRONDWARF";
        case 35: return "VULPERA";
        case 36: return "MAGHARORC";
        case 37: return "MECHAGNOME";
        case 52: return "DRACTHYR";
        case 70: return "DRACTHYR_H";
        default: return "UNKNOWN";
    }
}

// Role enum -> uppercase token. We use the snapshot's stored Role rather
// than re-deriving from cls/spec — Role::Tank/Healer/Dps were already
// resolved upstream and may include manual overrides.
char const* RoleToken(uint8_t role)
{
    switch (role)
    {
        case 1:  return "TANK";
        case 2:  return "HEALER";
        case 3:  return "DPS";
        default: return "UNKNOWN";
    }
}

// ----------------------------------------------------------------------------
// Stats. Walks the registry once + samples PerfCounters. Cheap enough to
// run on demand without a cache.
// ----------------------------------------------------------------------------
void HandleStatsReq(uint32 client_seq, WorldSession* sess)
{
    if (!Services::Initialized())
    {
        SendFields(sess, client_seq, "STATS_RESP",
                   {"0","0","0","0","0","0","0","0","0",""});
        return;
    }

    size_t total = 0, online = 0, tanks = 0, healers = 0, dps = 0, wedged = 0;
    Services::Registry().for_each(
        [&](BotId id, BotRegistryEntry const& e)
        {
            ++total;
            if (!e.ai) return;
            Player const* p = ObjectAccessor::FindConnectedPlayer(
                ObjectGuid::Create<HighGuid::Player>(uint64(id)));
            if (p) ++online;

            // Class+spec-derived role lives in group.my_role — populated
            // by BotSnapshotBuilder on every snapshot (infer_role via
            // IsTankSpec / IsHealerSpec). NOTE: do NOT use bg.bg_role
            // here; that's a BG-slot assignment (Attacker / Defender /
            // Roamer / Healer) which is zero/Free outside BGs and would
            // tag the entire fleet as DPS.
            if (auto snap = Services::Snapshots().latest(id))
            {
                switch (snap->group.my_role)
                {
                    case Role::Tank:   ++tanks;   break;
                    case Role::Healer: ++healers; break;
                    case Role::Dps:    ++dps;     break;
                    default:           ++dps;    break;  // Unknown → DPS bucket
                }
            }
            else
            {
                ++dps;
            }
        });

    auto perf = Services::Perf().snapshot();

    // Rolling intents/sec: compute the delta between this snapshot and the
    // previous one rather than total-over-uptime (which converges on the
    // long-run average and looks "stuck near 0" on small fleets). Cached
    // per-server, single-writer access from the world thread (this handler
    // runs from PlayerScript::OnChat → world tick), so a plain static is
    // race-free for our cadence.
    static uint64_t s_prev_intents = 0;
    static uint32_t s_prev_ms      = 0;
    const uint32_t now_ms = GameTime::GetGameTimeMS();
    uint64_t ips = 0;
    if (s_prev_ms != 0 && now_ms > s_prev_ms)
    {
        const uint32_t dt_ms = now_ms - s_prev_ms;
        if (dt_ms >= 250 && perf.intents_emitted_total >= s_prev_intents)
        {
            const uint64_t d_intents = perf.intents_emitted_total - s_prev_intents;
            ips = (d_intents * 1000ull) / dt_ms;
        }
    }
    s_prev_intents = perf.intents_emitted_total;
    s_prev_ms      = now_ms;

    // Tick budget: 333ms is the practical world-tick threshold — under
    // that, server time is "fine" (3+ Hz world update, no perceptible
    // lag for movement/spell packets). Above 333ms, players notice.
    // CONFIG_INTERVAL_MAPUPDATE (default 10ms) is the *map* tick interval
    // — way too small as a budget, made the dashboard show 1310% on a
    // healthy 131ms world tick.
    constexpr uint32 kWorldTickBudgetMs = 333u;
    const uint32 budget_ms = kWorldTickBudgetMs;
    uint32 used_ms = uint32(perf.world_p99.count());
    if (used_ms == 0)
        used_ms = uint32(perf.world_p50.count());

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "%zu|%zu|%zu|%zu|%zu|%llu|%u|%u|%zu|",
        total, online, tanks, healers, dps,
        (unsigned long long)ips, budget_ms, used_ms, wedged);
    SendFrame(sess, client_seq, "STATS_RESP", buf);
}

// ----------------------------------------------------------------------------
// Roster. Walks bots owned by the sender's account, emits one
// comma-delimited record per bot inside the pipe-delimited frame.
// ----------------------------------------------------------------------------
void HandleRosterReq(uint32 client_seq, WorldSession* sess, uint32 flags)
{
    if (!Services::Initialized())
    {
        SendFields(sess, client_seq, "ROSTER_RESP", {"0"});
        return;
    }
    const bool include_offline = (flags & 1u) != 0u;

    const uint32 acct = sess->GetAccountId();
    auto owned = Services::Owners().BotsOwnedBy(acct);

    Player* owner = sess->GetPlayer();
    const float ox = owner ? owner->GetPositionX() : 0.f;
    const float oy = owner ? owner->GetPositionY() : 0.f;
    const float oz = owner ? owner->GetPositionZ() : 0.f;
    const uint32 omap = owner ? owner->GetMapId() : 0u;

    std::vector<std::string> records;
    records.reserve(owned.size());

    for (BotId id : owned)
    {
        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(uint64(id));
        Player const* p = ObjectAccessor::FindConnectedPlayer(guid);
        const bool online = (p != nullptr);
        if (!online && !include_offline) continue;

        // Headless = driven by one of OUR BotSessions. False for the
        // owner's own self-AI character (real client session) — the
        // addon must NOT offer "Logout" for those rows.
        const bool headless = Services::SessionMgr().IsHeadless(guid);

        // Pull what we can from CharacterCache (works offline too).
        CharacterCacheEntry const* cc = sCharacterCache->GetCharacterCacheByGuid(guid);
        if (!cc) continue;

        // Snapshot for live fields (intent / last rule / hp / mana).
        std::string intent_name = "-";
        std::string last_rule   = "-";
        int hp_pct = 0, mana_pct = 0;
        int dist_m = -1;
        std::string zone_name = "-";
        uint8_t role = 0;

        if (auto snap = Services::Snapshots().latest(id))
        {
            hp_pct = snap->vitals.max_hp > 0
                ? int((100.0 * snap->vitals.hp) / snap->vitals.max_hp) : 0;
            const int32 mana_cur = snap->vitals.power[0];
            const int32 mana_max = snap->vitals.max_power[0];
            mana_pct = mana_max > 0 ? int((100.0 * mana_cur) / mana_max) : 0;
            if (snap->position.map_id == omap && online && p)
            {
                const float dx = snap->position.x - ox;
                const float dy = snap->position.y - oy;
                const float dz = snap->position.z - oz;
                dist_m = int(std::sqrt(dx*dx + dy*dy + dz*dz));
            }
            // Class+spec-derived role from group.my_role — populated on
            // every snapshot. bg.bg_role is a BG-slot assignment (and
            // zero outside BGs); using it here tagged the whole fleet
            // as DPS in the roster.
            switch (snap->group.my_role)
            {
                case Role::Tank:   role = 1; break;
                case Role::Healer: role = 2; break;
                case Role::Dps:    role = 3; break;
                default:           role = 3; break;
            }
        }

        // Look up the current intent + last rule directly from the AI.
        // BotAI's last_rule_fired() is char const* to a string literal —
        // safe to read from any thread for diagnostics.
        BotAI* ai = Services::Registry().ai(id);
        if (ai && ai->last_rule_fired())
            last_rule = ai->last_rule_fired();
        if (auto* iq = Services::Registry().intents(id))
        {
            (void)iq; // current_intent_name not exposed; left as "-".
        }

        // Build comma-delimited record. Bot names cannot contain commas.
        // Escape happens at the OUTER pipe-field boundary, not here.
        // Field 16 (`headless`) was appended 2026-06-12 for the addon's
        // Log In / Logout actions — positional append keeps old addon
        // builds decoding fields 1-15 unchanged.
        char rec[512];
        std::snprintf(rec, sizeof(rec),
            "%u,%s,%u,%s,%s,%s,%s,%s,%d,%d,%d,%s,%s,%u,%s,%s",
            unsigned(id),
            cc->Name.c_str(),
            unsigned(cc->Level),
            ClassToken(cc->Class),
            RaceToken(cc->Race),
            RoleToken(role),
            zone_name.c_str(),
            online ? "1" : "0",
            hp_pct, mana_pct, dist_m,
            intent_name.c_str(),
            last_rule.c_str(),
            /*groupId*/ 0u,
            /*spec*/ "-",
            headless ? "1" : "0");
        records.push_back(Escape(rec));
    }

    // Frame: <count>|<rec1>|<rec2>|...
    std::vector<std::string> fields;
    fields.reserve(records.size() + 1);
    fields.push_back(std::to_string(records.size()));
    for (auto& r : records) fields.push_back(std::move(r));
    SendFields(sess, client_seq, "ROSTER_RESP", fields);
}

// ----------------------------------------------------------------------------
// Bot detail — deeper dump for the single bot the user selected in the UI.
// ----------------------------------------------------------------------------
void HandleBotDetailReq(uint32 client_seq, WorldSession* sess, uint32 guidLow)
{
    if (!Services::Initialized() || guidLow == 0)
    {
        SendFields(sess, client_seq, "BOT_DETAIL_RESP", {"0"});
        return;
    }
    // Ownership check.
    const uint32 acct = sess->GetAccountId();
    OwnerBinding ob = Services::Owners().GetOwner(BotId(guidLow));
    if (ob.account_id != acct)
    {
        PushEvent(sess, "error", std::to_string(guidLow), "auth", "not your bot");
        return;
    }

    ObjectGuid bot_guid = ObjectGuid::Create<HighGuid::Player>(uint64(guidLow));
    CharacterCacheEntry const* cc = sCharacterCache->GetCharacterCacheByGuid(bot_guid);
    if (!cc)
    {
        SendFields(sess, client_seq, "BOT_DETAIL_RESP",
                   {std::to_string(guidLow)});
        return;
    }

    std::vector<std::string> fields;
    fields.push_back(std::to_string(guidLow));   // positional guidLow

    auto kv = [&](char const* k, std::string const& v)
    {
        std::string s = k; s += "="; s += v;
        fields.push_back(Escape(s));
    };
    auto kvi = [&](char const* k, long long v)
    {
        kv(k, std::to_string(v));
    };

    kv ("name",  cc->Name);
    kvi("level", cc->Level);
    kv ("class", ClassToken(cc->Class));
    kv ("race",  RaceToken(cc->Race));

    if (auto snap = Services::Snapshots().latest(BotId(guidLow)))
    {
        kvi("hp",       snap->vitals.hp);
        kvi("hp_max",   snap->vitals.max_hp);
        kvi("mana",     snap->vitals.power[0]);
        kvi("mana_max", snap->vitals.max_power[0]);
        kvi("map",      snap->position.map_id);
        kv ("pos_x",    std::to_string(snap->position.x));
        kv ("pos_y",    std::to_string(snap->position.y));
        kv ("pos_z",    std::to_string(snap->position.z));

        // Zone/area from AreaState (REFACTOR_2 sub-struct). The addon
        // resolves these IDs to names via GetMapNameByID / GetAreaName
        // on the client side; raw IDs over the wire keep the frame small.
        kvi("zone_id", snap->area.zone_id);
        kvi("area_id", snap->area.area_id);
        kvi("indoors", snap->area.is_indoors ? 1 : 0);

        // Role from class+spec-derived group.my_role (the same source
        // used by the roster + stats responses after the 2026-05-19 fix).
        kv ("role", RoleToken(
            snap->group.my_role == Role::Tank   ? uint8_t(1) :
            snap->group.my_role == Role::Healer ? uint8_t(2) :
            snap->group.my_role == Role::Dps    ? uint8_t(3) :
                                                  uint8_t(0)));

        // Spec ID (ChrSpecialization DB2). Addon can translate to name
        // via its own spec table; raw ID over wire is compact.
        kvi("spec_id", snap->identity.spec);

        // XP progress for the L1-9 leveling visibility.
        kvi("xp",           snap->identity.xp);
        kvi("xp_for_level", snap->identity.xp_for_level);
        kvi("rest_xp",      snap->identity.rest_bonus_xp);
    }
    if (BotAI* ai = Services::Registry().ai(BotId(guidLow)))
    {
        if (ai->last_rule_fired())
            kv("last_rule", ai->last_rule_fired());
    }
    // Per-bot intent queue depth (proxy for "how busy is the AI"). The
    // queue is lock-free MPMC; approximate_size is a relaxed-load read
    // that may drift by one but is the right call here — exact size
    // would require briefly stalling producers, which isn't worth it
    // for a diagnostic readout.
    if (Services::HasIntents(BotId(guidLow)))
    {
        kvi("intent_depth",
            static_cast<long long>(Services::Intents(BotId(guidLow)).approximate_size()));
    }
    SendFields(sess, client_seq, "BOT_DETAIL_RESP", fields);
}

// ----------------------------------------------------------------------------
// CMD — translate "<addr>|<verb>|<args...>" into a synthetic whisper sent
// from the owner to each addressed bot, then run BotCommandParser::Dispatch.
// That reuses the entire verb-handling pipeline (authority, squad addressing,
// IntentBody emission, reply whispers) instead of re-implementing it.
// ----------------------------------------------------------------------------
void HandleCmd(uint32 /*client_seq*/, WorldSession* sess,
               std::vector<std::string> const& fields)
{
    if (fields.size() < 2)
    {
        PushEvent(sess, "warn", "0", "bad_verb", "missing verb");
        return;
    }
    Player* owner = sess->GetPlayer();
    if (!owner) return;
    std::string const& addr_token = fields[0];
    std::string const& verb       = fields[1];

    // Reassemble the command text in the form BotCommandParser expects:
    // "<addr>: <verb> <arg1> <arg2>…" when addr != bot name, or just
    // "<verb> <args>" when addressing a single named bot.
    std::string args_joined;
    for (size_t i = 2; i < fields.size(); ++i)
    {
        if (i > 2) args_joined.push_back(' ');
        args_joined.append(fields[i]);
    }

    // Resolve target bot. For squad addresses we pick the first owned bot
    // as the "pivot" and let BotCommandParser fan out via the prefix path.
    auto owned = Services::Owners().BotsOwnedBy(sess->GetAccountId());
    if (owned.empty())
    {
        PushEvent(sess, "warn", "0", "bad_verb", "no owned bots");
        return;
    }

    auto is_squad_token = [](std::string const& t) {
        return t == "all" || t == "squad" || t == "tank" || t == "healer"
            || t == "dps" || t == "mage" || t == "warlock" || t == "warrior"
            || t == "paladin" || t == "rogue" || t == "priest" || t == "monk"
            || t == "druid" || t == "shaman" || t == "hunter"
            || t == "deathknight" || t == "demonhunter" || t == "evoker";
    };

    Player* pivot = nullptr;
    std::string cmd;
    if (is_squad_token(addr_token))
    {
        // Pick first online owned bot as pivot.
        for (BotId id : owned)
        {
            Player* p = ObjectAccessor::FindConnectedPlayer(
                ObjectGuid::Create<HighGuid::Player>(uint64(id)));
            if (p) { pivot = p; break; }
        }
        cmd = addr_token + ": " + verb;
        if (!args_joined.empty()) { cmd.push_back(' '); cmd.append(args_joined); }
    }
    else
    {
        // Treat as a bot character name.
        pivot = ObjectAccessor::FindConnectedPlayerByName(addr_token);
        if (!pivot)
        {
            PushEvent(sess, "warn", "0", "bad_verb",
                      "bot not online: " + addr_token);
            return;
        }
        cmd = verb;
        if (!args_joined.empty()) { cmd.push_back(' '); cmd.append(args_joined); }
    }
    if (!pivot)
    {
        PushEvent(sess, "warn", "0", "bad_verb", "no online bot to route to");
        return;
    }

    // BotCommandParser::Dispatch enforces authority via Owners(); we don't
    // re-check here because that would duplicate (and possibly diverge
    // from) the canonical check. If the sender isn't authorized, Dispatch
    // returns false silently.
    const bool ok = BotCommandParser::Dispatch(owner, pivot, cmd);
    if (!ok)
    {
        PushEvent(sess, "warn", std::to_string(pivot->GetGUID().GetCounter()),
                  "bad_verb", "verb refused: " + verb);
    }
}

// ----------------------------------------------------------------------------
// PING — round-trip latency. Echo back the original timestamp.
// ----------------------------------------------------------------------------
void HandlePing(uint32 client_seq, WorldSession* sess,
                std::vector<std::string> const& fields)
{
    std::string t0 = fields.empty() ? std::string("0") : fields[0];
    SendFields(sess, client_seq, "PONG", { Escape(t0) });
}

// ----------------------------------------------------------------------------
// ALTS_REQ — enumerate same-account characters so the addon can present a
// spawn picker. Marks each row with current state:
//   bot=1      → already marked as a bot in V2 lifecycle (spawned or pending)
//   on=1       → currently logged in (real client OR headless bot session)
//   headless=1 → the online session is one of OUR headless BotSessions —
//                the addon offers "Logout" for these and locks "In World"
//                rows where online=1 && headless=0 (real client).
//   self=1     → the caller's own character (collapsed into the .self toggle)
//
// We pull rows synchronously from `characters` — typical accounts have <20
// alts, well below any concerning sync-query budget. Returns:
//   ALTS_RESP|<count>|<rec1>|...   where each rec is
//     guidLow,name,class,race,level,online,isBot,isSelf,isHeadless
// ----------------------------------------------------------------------------
void HandleAltsReq(uint32 client_seq, WorldSession* sess)
{
    if (!sess)
        return;

    const uint32 acct = sess->GetAccountId();
    Player const* me  = sess->GetPlayer();
    const uint32 my_low = me ? me->GetGUID().GetCounter() : 0u;

    // Query characters table directly — CharacterCache has no by-account
    // iteration API, and we need class/race/level alongside the guid.
    QueryResult res = CharacterDatabase.PQuery(
        "SELECT guid, name, class, race, level, online "
        "FROM characters "
        "WHERE account = {} AND deleteDate IS NULL "
        "ORDER BY level DESC, name ASC",
        acct);

    std::vector<std::string> records;
    if (res)
    {
        records.reserve(res->GetRowCount());
        do
        {
            Field* f = res->Fetch();
            // characters.guid is INT UNSIGNED (uint32 low-guid). GetUInt64
            // here was a type-mismatch read — release-mode reinterprets
            // the 4-byte field buffer as 8 bytes (UB; high bits read past
            // the column) and the &0xFFFFFFFFu mask hid the corruption.
            // Debug asserts. Use GetUInt32 (matches CharacterCache pattern).
            const uint32 guid_low = f[0].GetUInt32();
            std::string  name     = f[1].GetString();
            const uint8  cls      = f[2].GetUInt8();
            const uint8  race     = f[3].GetUInt8();
            const uint8  level    = f[4].GetUInt8();
            const bool   db_online = f[5].GetUInt8() != 0;

            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(uint64(guid_low));

            // characters.online lags real state (written on save, can stay
            // stale after a crash) — OR it with the live connection check so
            // the picker never shows "Log In" for an in-world character.
            const bool connected = ObjectAccessor::FindConnectedPlayer(guid) != nullptr;
            const bool online    = db_online || connected;

            const bool is_bot = Services::Initialized()
                && Services::Lifecycle().is_bot(BotId(guid_low));
            const bool is_self = (guid_low == my_low);

            // Headless = one of OUR BotSessions drives this character.
            // online && !headless ⇒ a REAL client session (lock the row).
            const bool is_headless = Services::Initialized()
                && Services::SessionMgr().IsHeadless(guid);

            char rec[256];
            std::snprintf(rec, sizeof(rec), "%u,%s,%s,%s,%u,%d,%d,%d,%d",
                          unsigned(guid_low),
                          name.c_str(),
                          ClassToken(cls),
                          RaceToken(race),
                          unsigned(level),
                          online ? 1 : 0,
                          is_bot ? 1 : 0,
                          is_self ? 1 : 0,
                          is_headless ? 1 : 0);
            records.push_back(Escape(rec));
        } while (res->NextRow());
    }

    std::vector<std::string> fields;
    fields.reserve(records.size() + 1);
    fields.push_back(std::to_string(records.size()));
    for (auto& r : records) fields.push_back(std::move(r));
    SendFields(sess, client_seq, "ALTS_RESP", fields);
}

// ----------------------------------------------------------------------------
// SELF — addon counterpart to `.playerbot self on|off|status`. Attaches
// or detaches the V2 AI to the issuing player's OWN character. Mirrors
// HandleSelf in cs_playerbot_v2.cpp 1:1; keep these two paths in lock-step
// so a future RBAC tightening (or perf cap on self-attach) lands in both.
//
// Wire:  SELF|on        SELF|off       SELF|status (or empty)
// Reply: SELF_RESP|attached=<0|1>|marked=<0|1>
// ----------------------------------------------------------------------------
void HandleSelfToggle(uint32 client_seq, WorldSession* sess,
                      std::vector<std::string> const& fields)
{
    if (!sess) return;
    if (!Services::Initialized())
    {
        PushEvent(sess, "warn", "0", "self_failed", "v2 not initialized");
        return;
    }
    Player* me = sess->GetPlayer();
    if (!me)
    {
        PushEvent(sess, "warn", "0", "self_failed", "no player on session");
        return;
    }

    std::string mode = fields.empty() ? std::string("status") : fields[0];
    for (char& c : mode)
        if (c >= 'A' && c <= 'Z') c = char(c + 32);

    const BotId id          = BotId(me->GetGUID().GetCounter());
    const bool  was_bot     = Services::Lifecycle().is_bot(id);
    const bool  was_attached= Services::Registry().has(id);

    if (mode == "on")
    {
        if (!was_attached)
        {
            // CRITICAL: use TRANSIENT mark. .playerbot self toggles a
            // real human character into AI mode for the duration of
            // their session — it must NEVER persist to
            // playerbot_v2_character, otherwise BotPopulationManager
            // ::LoginExisting will grab the character on the next
            // server boot and the human can't log in (observed
            // 2026-05-20: 6 user alts locked out after a restart
            // because mark_as_bot wrote them all to the table).
            if (!was_bot)
                Services::Lifecycle().mark_as_bot_transient(id);
            Services::Owners().SetOwner(
                id, sess->GetAccountId(), /*player_guid=*/0u);
            if (!Services::Registry().has(id))
            {
                BotPersonality personality =
                    Services::Config().random_personality()
                        ? RandomPersonality(SeedForBot(id))
                        : DefaultPersonality();
                Services::Registry().register_bot(
                    id, personality, BotRng{SeedForBot(id)});
                Services::Scheduler().register_bot(id, ActivityTier::Idle);
                // Give the transient self-AI bot a coherent archetype too
                // (deterministic roll, NO DB write — this is a human's own
                // character marked transient; it must never touch
                // playerbot_v2_character per the comment above).
                if (Services::Config().archetype_enabled())
                    if (BotAI* sai = Services::Registry().ai(id))
                        sai->set_archetype(RollArchetype(SeedForBot(id)));
            }
            // Hand server-side movement control off the WoW client. Without
            // this the client keeps sending CMSG_MOVE_* every frame, which
            // overwrites every server-driven MotionMaster command, so the
            // AI's MoveChase / MovePoint visibly fight client input and
            // the character barely moves. Full-takeover mode: WASD is
            // locked while self-AI is on; re-enabled below on "off".
            // SAFE post-transient-mark fix: ControlUpdate is purely
            // runtime/network state, no DB persistence, so a crash mid
            // session can't lock the character — the next login defaults
            // to client control via Player::SendInitialPacketsAfterAddToMap
            // (Player.cpp:25125 SetMovedUnit(this)).
            me->SetClientControl(me, false);
        }
    }
    else if (mode == "off")
    {
        if (Services::Registry().has(id))
        {
            Services::Scheduler().unregister_bot(id);
            Services::Snapshots().remove(id);
            Services::Registry().unregister_bot(id);
        }
        // Use the transient unmark so we don't accidentally DELETE a
        // legitimate persistent marking for a headless fleet bot whose
        // GUID happened to match. (mark_as_bot_transient only added to
        // the in-memory set, so removing from it is the inverse.)
        if (Services::Lifecycle().is_bot(id))
            Services::Lifecycle().unmark_as_bot_transient(id);
        Services::Owners().ClearOwner(id);
        // Return movement control to the WoW client — the human takes
        // the wheel back. Pairs with the on-branch SetClientControl.
        me->SetClientControl(me, true);
    }
    // "status" (or anything else) falls through — just report current state.

    const bool now_bot      = Services::Lifecycle().is_bot(id);
    const bool now_attached = Services::Registry().has(id);
    std::vector<std::string> resp = {
        Escape(std::string("attached=") + (now_attached ? "1" : "0")),
        Escape(std::string("marked=")   + (now_bot      ? "1" : "0")),
    };
    SendFields(sess, client_seq, "SELF_RESP", resp);
}

// ----------------------------------------------------------------------------
// SUMMON — addon counterpart to `.playerbot summon <name>`. Enforces the
// SAME guards as the chat command (account match, cap, not-online refusal,
// no-self redirect) so neither path can bypass the other. On success the
// bot enters world on the next world tick (LoginBot DB callback).
//
// Wire:  SUMMON|<characterName>
// Reply: EVENT_PUSH info|<guidLow>|summoned|<name>   on success
//        EVENT_PUSH warn|0|summon_failed|<reason>     on refusal
// ----------------------------------------------------------------------------
void HandleSummon(uint32 /*client_seq*/, WorldSession* sess,
                  std::vector<std::string> const& fields)
{
    if (!sess) return;
    if (!Services::Initialized())
    {
        PushEvent(sess, "warn", "0", "summon_failed", "v2 not initialized");
        return;
    }
    if (fields.empty() || fields[0].empty())
    {
        PushEvent(sess, "warn", "0", "summon_failed", "missing alt name");
        return;
    }
    Player const* me = sess->GetPlayer();
    if (!me)
    {
        PushEvent(sess, "warn", "0", "summon_failed", "no player on session");
        return;
    }
    std::string const& target_name = fields[0];

    CharacterCacheEntry const* cc = sCharacterCache->GetCharacterCacheByName(target_name);
    if (!cc)
    {
        PushEvent(sess, "warn", "0", "summon_failed",
                  "no such character: " + target_name);
        return;
    }
    const uint32 guid_low = cc->Guid.GetCounter();
    if (guid_low == me->GetGUID().GetCounter())
    {
        PushEvent(sess, "warn", "0", "summon_failed",
                  "that's your current character — use `.playerbot self on`");
        return;
    }
    if (cc->AccountId != sess->GetAccountId())
    {
        PushEvent(sess, "warn", "0", "summon_failed",
                  "'" + target_name + "' is not on your account");
        return;
    }
    if (ObjectAccessor::FindConnectedPlayerByName(target_name))
    {
        PushEvent(sess, "warn", "0", "summon_failed",
                  "'" + target_name + "' is currently online");
        return;
    }

    // Per-account active-alt cap.
    const uint8 cap = Services::Config().max_alts_as_bots();
    if (cap == 0)
    {
        PushEvent(sess, "warn", "0", "summon_failed",
                  "alt-summon disabled on this realm");
        return;
    }
    auto owned = Services::Owners().BotsOwnedBy(sess->GetAccountId());
    size_t active_alts = 0;
    for (BotId b : owned)
        if (Services::Lifecycle().is_bot(b)) ++active_alts;
    if (active_alts >= cap)
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
            "cap reached: %zu/%u alts active — logout one first",
            active_alts, unsigned(cap));
        PushEvent(sess, "warn", "0", "summon_failed", buf);
        return;
    }

    // Mark + bind owner + headless login. Mirrors HandleSummon in
    // cs_playerbot_v2.cpp; keep these two paths in lock-step on any future
    // policy change (e.g. RBAC additions, summon-rate-limit).
    if (!Services::Lifecycle().is_bot(BotId(guid_low)))
        Services::Lifecycle().mark_as_bot(BotId(guid_low));
    Services::Owners().SetOwner(BotId(guid_low),
                                sess->GetAccountId(), /*player_guid=*/0u);

    auto res = Services::SessionMgr().LoginBot(cc->Guid);
    if (!res.ok)
    {
        PushEvent(sess, "warn", "0", "summon_failed", res.reason);
        return;
    }

    PushEvent(sess, "info", std::to_string(guid_low),
              "summoned", target_name);
}

// ----------------------------------------------------------------------------
// LOGIN_REQ / LOGOUT_REQ — headless session control from the addon.
//
// Authority model (STRICTER than CMD verbs — these mutate session state):
//   * same-account characters (the Alts-picker flow), OR
//   * bots owned by the requester's account via Services::Owners()
//     (the roster flow — fleet bots adopted onto this account).
//   Anything else is refused with an EVENT_PUSH warn. Note that group
//   leadership does NOT grant login/logout authority — unlike the legacy
//   CMD fallback tiers, an unowned bot's session may only be driven by
//   GM commands.
//
// LOGOUT only ever touches HEADLESS BotSessions: BotSessionMgr::LogoutBot
// walks its own session map (which never contains real-client sessions),
// and we pre-check IsHeadless for a precise refusal message. A real client
// session can NEVER be kicked through this path.
//
// Wire:  LOGIN_REQ|<characterName>      LOGOUT_REQ|<characterName>
// Reply: EVENT_PUSH info|<guidLow>|login_submitted|<name>   on login submit
//        EVENT_PUSH info|<guidLow>|logged_out|<name>        on logout
//        EVENT_PUSH warn|0|login_failed/logout_failed|<reason> on refusal
// ----------------------------------------------------------------------------

// Soft per-account rate gate for session-control requests. A login spins up
// a DB holder + async character load; a misbehaving client must not flood
// the character DB pool. Token bucket sized so a fleet-wide "log in all my
// bots" toolbar click (one frame per bot, same tick) goes through intact —
// burst 32 covers any owned-bot count the cap allows, refill 4/s caps the
// sustained rate well under the GM LoginAll budget. World-thread only
// (PlayerScript::OnChat → world tick), so plain statics are race-free.
bool SessionControlRateGate(WorldSession* sess)
{
    constexpr double kBurst        = 32.0;
    constexpr double kRefillPerSec = 4.0;
    // No NSDMI on the local struct — MSVC can't resolve a default member
    // initializer that names the enclosing function's constexpr locals.
    struct Bucket { double tokens; uint32 last_ms; };
    static std::unordered_map<uint32, Bucket> s_buckets;

    Bucket& b = s_buckets.try_emplace(
        sess->GetAccountId(), Bucket{ kBurst, 0u }).first->second;
    const uint32 now_ms = GameTime::GetGameTimeMS();
    if (b.last_ms != 0 && now_ms > b.last_ms)
        b.tokens = std::min(kBurst,
            b.tokens + (now_ms - b.last_ms) * kRefillPerSec / 1000.0);
    b.last_ms = now_ms;
    if (b.tokens < 1.0)
        return false;
    b.tokens -= 1.0;
    return true;
}

// Shared name→guid resolution + authority check for LOGIN_REQ/LOGOUT_REQ.
// On failure an EVENT_PUSH warn with `fail_event` was already sent and
// ok == false. Works fully offline via CharacterCache.
struct SessionTarget
{
    bool        ok = false;
    ObjectGuid  guid;
    uint32      guid_low = 0;
    std::string name;       // canonical-case name from the cache
};

SessionTarget ResolveSessionTarget(WorldSession* sess,
                                   std::string const& target_name,
                                   char const* fail_event)
{
    SessionTarget out;
    if (target_name.empty())
    {
        PushEvent(sess, "warn", "0", fail_event, "missing character name");
        return out;
    }
    // Normalize case — names typed into `/pbc login <name>` arrive verbatim
    // while CharacterCache stores the canonical capitalization.
    std::string lookup_name = target_name;
    if (!normalizePlayerName(lookup_name))
    {
        PushEvent(sess, "warn", "0", fail_event,
                  "invalid character name: " + target_name);
        return out;
    }
    CharacterCacheEntry const* cc =
        sCharacterCache->GetCharacterCacheByName(lookup_name);
    if (!cc)
    {
        PushEvent(sess, "warn", "0", fail_event,
                  "no such character: " + lookup_name);
        return out;
    }
    const uint32 acct = sess->GetAccountId();
    const bool same_account = (cc->AccountId == acct);
    const bool owned_by_me  =
        Services::Owners().GetOwner(BotId(cc->Guid.GetCounter())).account_id == acct;
    if (!same_account && !owned_by_me)
    {
        PushEvent(sess, "warn", "0", fail_event,
                  "'" + cc->Name + "' is not your character or bot");
        return out;
    }
    out.ok       = true;
    out.guid     = cc->Guid;
    out.guid_low = cc->Guid.GetCounter();
    out.name     = cc->Name;
    return out;
}

void HandleLoginReq(uint32 /*client_seq*/, WorldSession* sess,
                    std::vector<std::string> const& fields)
{
    if (!sess) return;
    if (!Services::Initialized())
    {
        PushEvent(sess, "warn", "0", "login_failed", "v2 not initialized");
        return;
    }
    if (!SessionControlRateGate(sess))
    {
        PushEvent(sess, "warn", "0", "rate_limit", "too many session requests");
        return;
    }
    Player const* me = sess->GetPlayer();
    if (!me)
    {
        PushEvent(sess, "warn", "0", "login_failed", "no player on session");
        return;
    }
    SessionTarget t = ResolveSessionTarget(
        sess, fields.empty() ? std::string{} : fields[0], "login_failed");
    if (!t.ok) return;

    if (t.guid_low == me->GetGUID().GetCounter())
    {
        PushEvent(sess, "warn", "0", "login_failed",
                  "that's your current character — use `/pbc self on`");
        return;
    }

    // Refuse while a REAL client session is in world. An already-running
    // headless session is also refused (idempotent no-op from the addon's
    // point of view; LoginBot would refuse anyway, but the message is
    // clearer when we distinguish the two cases up front).
    const bool headless = Services::SessionMgr().IsHeadless(t.guid);
    if (ObjectAccessor::FindConnectedPlayer(t.guid))
    {
        PushEvent(sess, "warn", "0", "login_failed", headless
            ? "'" + t.name + "' is already in world as a bot"
            : "'" + t.name + "' is online as a real player");
        return;
    }

    // First-time path: an unmarked same-account alt arriving via "Log In"
    // (e.g. picker raced a fresh character). Mirrors HandleSummon's mark +
    // cap policy — keep the two in lock-step on any future policy change.
    if (!Services::Lifecycle().is_bot(BotId(t.guid_low)))
    {
        const uint8 cap = Services::Config().max_alts_as_bots();
        if (cap == 0)
        {
            PushEvent(sess, "warn", "0", "login_failed",
                      "alt-summon disabled on this realm");
            return;
        }
        auto owned = Services::Owners().BotsOwnedBy(sess->GetAccountId());
        size_t active_alts = 0;
        for (BotId b : owned)
            if (Services::Lifecycle().is_bot(b)) ++active_alts;
        if (active_alts >= cap)
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "cap reached: %zu/%u alts active — logout one first",
                active_alts, unsigned(cap));
            PushEvent(sess, "warn", "0", "login_failed", buf);
            return;
        }
        Services::Lifecycle().mark_as_bot(BotId(t.guid_low));
    }

    // Bind ownership if the character is still unowned (same-account alts
    // marked through GM commands). Owned-by-someone-else was already
    // rejected by ResolveSessionTarget, so this never steals a binding.
    if (Services::Owners().GetOwner(BotId(t.guid_low)).account_id == 0)
        Services::Owners().SetOwner(BotId(t.guid_low),
                                    sess->GetAccountId(), /*player_guid=*/0u);

    auto res = Services::SessionMgr().LoginBot(t.guid);
    if (!res.ok)
    {
        PushEvent(sess, "warn", "0", "login_failed", res.reason);
        return;
    }
    PushEvent(sess, "info", std::to_string(t.guid_low),
              "login_submitted", t.name);
}

void HandleLogoutReq(uint32 /*client_seq*/, WorldSession* sess,
                     std::vector<std::string> const& fields)
{
    if (!sess) return;
    if (!Services::Initialized())
    {
        PushEvent(sess, "warn", "0", "logout_failed", "v2 not initialized");
        return;
    }
    if (!SessionControlRateGate(sess))
    {
        PushEvent(sess, "warn", "0", "rate_limit", "too many session requests");
        return;
    }
    SessionTarget t = ResolveSessionTarget(
        sess, fields.empty() ? std::string{} : fields[0], "logout_failed");
    if (!t.ok) return;

    // HARD RULE: only headless BotSessions may be kicked. LogoutBot already
    // walks only its own (headless) session map, but the explicit pre-check
    // turns "silently no-ops on a real client" into a precise refusal.
    if (!Services::SessionMgr().IsHeadless(t.guid))
    {
        PushEvent(sess, "warn", "0", "logout_failed",
                  ObjectAccessor::FindConnectedPlayer(t.guid)
                      ? "'" + t.name + "' is a real client session — refusing"
                      : "'" + t.name + "' has no active bot session");
        return;
    }
    if (!Services::SessionMgr().LogoutBot(t.guid))
    {
        PushEvent(sess, "warn", "0", "logout_failed",
                  "no active bot session for '" + t.name + "'");
        return;
    }
    PushEvent(sess, "info", std::to_string(t.guid_low),
              "logged_out", t.name);
}

// ----------------------------------------------------------------------------
// Main dispatcher. Called once a complete logical frame is assembled.
// ----------------------------------------------------------------------------
void DispatchAssembled(WorldSession* sess, uint32 seq,
                       std::string const& mtype, std::string const& body)
{
    auto fields = SplitPipes(body);
    if      (mtype == "ROSTER_REQ")
    {
        const uint32 flags = fields.empty() ? 15u
            : uint32(std::strtoul(fields[0].c_str(), nullptr, 10));
        HandleRosterReq(seq, sess, flags);
    }
    else if (mtype == "BOT_DETAIL_REQ")
    {
        const uint32 g = fields.empty() ? 0u
            : uint32(std::strtoul(fields[0].c_str(), nullptr, 10));
        HandleBotDetailReq(seq, sess, g);
    }
    else if (mtype == "STATS_REQ")
    {
        HandleStatsReq(seq, sess);
    }
    else if (mtype == "CMD")
    {
        HandleCmd(seq, sess, fields);
    }
    else if (mtype == "PING")
    {
        HandlePing(seq, sess, fields);
    }
    else if (mtype == "ALTS_REQ")
    {
        HandleAltsReq(seq, sess);
    }
    else if (mtype == "SUMMON")
    {
        HandleSummon(seq, sess, fields);
    }
    else if (mtype == "LOGIN_REQ")
    {
        HandleLoginReq(seq, sess, fields);
    }
    else if (mtype == "LOGOUT_REQ")
    {
        HandleLogoutReq(seq, sess, fields);
    }
    else if (mtype == "SELF")
    {
        HandleSelfToggle(seq, sess, fields);
    }
    else if (mtype == "ACK")
    {
        // Retry spool not implemented in v1 — silent drop.
    }
    else
    {
        PushEvent(sess, "warn", "0", "bad_verb", "unknown MTYPE: " + mtype);
    }
}

} // anonymous

// ============================================================================
// Public entry — called from the PlayerScript::OnChat hook.
// ============================================================================
bool OnAddonWhisper(Player* sender, Player* /*receiver*/,
                    std::string const& prefix, std::string const& body)
{
    if (!sender) return false;
    // The TC OnPlayerChat hook doesn't carry the addon prefix — only the
    // body text. We accept either an explicit "PBC" prefix (future-proof
    // for a hook that does carry it) OR an empty prefix when the body
    // shape matches the PBC frame layout. The "1|" header check + the
    // MTYPE whitelist inside DispatchAssembled are tight enough to reject
    // any non-PBC traffic without false positives.
    if (!prefix.empty() && prefix != kPrefix) return false;
    if (body.size() < 8) return false;
    if (body[0] != '1' || body[1] != '|') return false;

    WorldSession* sess = sender->GetSession();
    if (!sess) return false;

    // Parse envelope: v|seq|idx|tot|MTYPE|payload — exact field-order with
    // no escaping in the envelope itself (those 5 fields are server-
    // controlled; the addon never emits a `|` inside them).
    auto pos1 = body.find('|', 2);                if (pos1 == std::string::npos) return false;
    auto pos2 = body.find('|', pos1 + 1);         if (pos2 == std::string::npos) return false;
    auto pos3 = body.find('|', pos2 + 1);         if (pos3 == std::string::npos) return false;
    auto pos4 = body.find('|', pos3 + 1);         if (pos4 == std::string::npos) return false;

    const uint32 seq = uint32(std::strtoul(body.substr(2, pos1 - 2).c_str(), nullptr, 16));
    const uint32 idx = uint32(std::strtoul(body.substr(pos1 + 1, pos2 - pos1 - 1).c_str(), nullptr, 10));
    const uint32 tot = uint32(std::strtoul(body.substr(pos2 + 1, pos3 - pos2 - 1).c_str(), nullptr, 10));
    std::string  mtype = body.substr(pos3 + 1, pos4 - pos3 - 1);
    std::string  payload = body.substr(pos4 + 1);

    if (seq == 0 || idx == 0 || tot == 0 || idx > tot) return false;
    if (payload.size() > kMaxFrameBytes) return false;

    // Authorize: sender must own at least one bot — EXCEPT for a small
    // whitelist of MTYPEs that need to work for accounts that haven't
    // spawned anything yet (so the addon can show the alt picker and
    // accept the first SUMMON). PING is allowed too since it's a
    // diagnostics probe and any owner-less session can still want to
    // verify the wire is alive.
    if (Services::Initialized())
    {
        // LOGIN_REQ/LOGOUT_REQ are whitelisted alongside SUMMON: an account
        // with zero owned bots may still log a same-account marked alt in or
        // out (the handlers enforce same-account/ownership themselves).
        auto owned = Services::Owners().BotsOwnedBy(sess->GetAccountId());
        if (owned.empty()
            && mtype != "ALTS_REQ"
            && mtype != "SUMMON"
            && mtype != "LOGIN_REQ"
            && mtype != "LOGOUT_REQ"
            && mtype != "SELF"
            && mtype != "PING")
            return false;
    }

    if (tot == 1)
    {
        DispatchAssembled(sess, seq, mtype, payload);
        return true;
    }

    // Multi-chunk reassembly.
    std::lock_guard lk(g_rx_mtx);
    ReapStaleReassemblies_locked();
    ReassemblyKey key{ sess->GetAccountId(), seq };
    auto& pf = g_rx[key];
    if (pf.received == 0)
    {
        pf.total = tot;
        pf.mtype = mtype;
        pf.parts.assign(tot + 1, std::string{});
        pf.deadline_unix = uint32_t(std::time(nullptr) + kReassemblyTtlSec);
    }
    if (idx >= pf.parts.size()) { g_rx.erase(key); return false; }
    if (pf.parts[idx].empty())
    {
        pf.parts[idx] = std::move(payload);
        pf.received++;
    }
    if (pf.received == pf.total)
    {
        std::string glued;
        for (uint32 i = 1; i <= pf.total; ++i) glued.append(pf.parts[i]);
        std::string mt = std::move(pf.mtype);
        g_rx.erase(key);
        DispatchAssembled(sess, seq, mt, glued);
    }
    return true;
}

void OnSessionLogout(uint32 sessionAccountId)
{
    std::lock_guard lk(g_rx_mtx);
    for (auto it = g_rx.begin(); it != g_rx.end(); )
    {
        if (it->first.account_id == sessionAccountId)
            it = g_rx.erase(it);
        else
            ++it;
    }
}

} // namespace
