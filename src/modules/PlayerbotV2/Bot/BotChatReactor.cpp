// BotChatReactor — see header for scope. Implementation here resolves
// the keyword pattern, picks one bot to speak, and pushes a
// PartyChatIntent through the per-bot intent queue.

#include "BotChatReactor.h"

#include "BotAI.h"
#include "BotIntent.h"
#include "BotPersonality.h"
#include "BotRegistry.h"
#include "BotTypes.h"
#include "../Services.h"
#include "../Threading/IntentQueue.h"

#include "DB2Stores.h"     // sAreaTableStore for LocationQuery whisper reply
#include "Cell.h"          // SC-P1a grid-bounded say/yell range query
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "GroupReference.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Player.h"
#include "World.h"        // sWorld locale + CONFIG_LISTEN_RANGE_* for say/yell
#include "WorldSession.h"
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot {

namespace {

// SC-P3b: the reaction-timestamp maps below are keyed by bot/area/guild ids
// and only ever grow — a key is inserted the first time that bot/area/guild
// reacts and is never removed. Over a long uptime with churn (bots cycling
// in and out, players roaming many areas, guilds forming/disbanding) these
// maps leak unboundedly. PruneStale sweeps entries whose timestamp is older
// than `keep_ms` (the relevant cooldown, doubled — anything that stale can
// never gate a future reaction, so dropping it is behavior-preserving). It
// is called under the map's own mutex at insert time, but ONLY when the map
// has grown past `threshold` so the common path stays O(1). `now_ms` is the
// monotonic GameTime tick; uint32 millis wrap every ~49.7 days but the
// `now - ts` subtraction is wrap-safe (modular arithmetic) and a wrap at
// worst spares one sweep, never corrupts state.
template <typename Map>
void PruneStale(Map& m, uint32_t now_ms, uint32_t keep_ms, size_t threshold)
{
    if (m.size() < threshold) return;
    for (auto it = m.begin(); it != m.end(); )
    {
        if (now_ms - it->second > keep_ms)
            it = m.erase(it);
        else
            ++it;
    }
}

// Sweep threshold for the reaction maps. A real server tops out at a few
// thousand distinct bots/areas/guilds reacting; 4096 keeps the maps small
// while making the sweep a rare event (it runs only once the map crosses
// the threshold, then again each insert until it drops back under).
constexpr size_t kReactMapPruneThreshold = 4096;

// Per-bot last-reaction timestamp (ms). Read+write from the chat handler
// thread; the world thread doesn't touch this map. Guarded by a small mutex.
// Chat fires at human typing rates so the contention here is negligible.
// SC-P3b: the single global mutex is intentionally NOT sharded. Reactions
// are driven by human chat cadence (a handful per second server-wide even at
// 2000 bots), so the critical section — a hash probe + a rare bounded sweep
// — is never hot. Sharding would add complexity (per-shard prune, key→shard
// hashing) for no measured win; revisit only if a profile shows g_react_mtx
// contention, which chat traffic cannot realistically produce.
std::mutex                                g_react_mtx;
std::unordered_map<BotId, uint32_t>       g_last_react_ms;

constexpr uint32_t kPerBotReactCooldownMs = 90u * 1000u;   // one reply per 90s per bot

// SC-P1b: addressed reactions (someone said the bot's name, or whispered it
// directly) get a SHORTER cooldown than ambient chatter — staying silent
// when called by name for 90s reads as broken — but they are NO LONGER
// exempt from the cooldown entirely. The old code bypassed the throttle for
// addressed replies AND never wrote the reaction timestamp, so a duo could
// both answer and a name spammer could trigger a reply on every line. With a
// 20s addressed window + an always-written timestamp, a bot answers a direct
// address promptly but not on back-to-back lines.
constexpr uint32_t kAddressedReactCooldownMs = 20u * 1000u;

// SC-P1a: per-area cooldown for /say and /yell reactions. A crowded hub
// (Stormwind trade district, Valdrakken) can see dozens of /say lines per
// second; without an area-scoped gate the bots there would chatter
// constantly. Keyed by (map_id<<32 | area_id). 8s lets a hub feel alive
// without turning into a wall of bot replies.
std::mutex                                  g_area_react_mtx;
std::unordered_map<uint64_t, uint32_t>      g_last_area_react_ms;
constexpr uint32_t kPerAreaReactCooldownMs  = 8u * 1000u;

// Lowercase + collapse whitespace to a single space. Strips leading/trailing
// whitespace and ASCII punctuation that would otherwise wreck keyword
// matching ("ty!" → "ty", "thx," → "thx"). Returns at most 256 chars; longer
// messages are truncated for matching purposes only (the bot's reply doesn't
// depend on input length).
std::string Normalize(std::string const& in)
{
    std::string out;
    out.reserve(std::min<size_t>(in.size(), 256));
    bool last_was_space = true;
    for (char c : in)
    {
        if (out.size() >= 256) break;
        unsigned char uc = static_cast<unsigned char>(c);
        // Treat punctuation-like ASCII as whitespace so "ty!" / "thx," normalize cleanly.
        if (uc == '!' || uc == '?' || uc == '.' || uc == ',' ||
            uc == ';' || uc == ':' || uc == '(' || uc == ')' ||
            uc == '\t' || uc == '\r' || uc == '\n' || uc == ' ')
        {
            if (!last_was_space) { out.push_back(' '); last_was_space = true; }
            continue;
        }
        out.push_back(char(std::tolower(uc)));
        last_was_space = false;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// Returns true if normalized `hay` contains `needle` as a whole word.
// "ty" matches "ty", "ty everyone", " ty all" — not "pretty" or "country".
bool ContainsWord(std::string const& hay, char const* needle)
{
    const std::string n = needle;
    size_t pos = 0;
    while ((pos = hay.find(n, pos)) != std::string::npos)
    {
        const bool left_ok  = (pos == 0) || hay[pos - 1] == ' ';
        const bool right_ok = (pos + n.size() == hay.size()) || hay[pos + n.size()] == ' ';
        if (left_ok && right_ok) return true;
        pos += n.size();
    }
    return false;
}

enum class ReactKind : uint8_t
{
    None,
    Thanks,        // ty / thx / thanks
    Grats,         // gz / grats / congratulations
    Laugh,         // lol / haha / lmao / xd
    Acknowledge,   // pull / go / wait / hold / inc / back / brb — behavioral cues
    Ready,         // ready / rdy — separate so reply can echo "rdy!" not just "k"
    Greeting,      // hi / hello / hey — small social opener
    DirectAddress, // bot's name appears as a whole word
    LocationQuery, // "where" / "wo bist du" — bot whispers back its current zone
};

ReactKind ClassifyMessage(std::string const& norm, std::string const& bot_first_name_lower)
{
    const bool addressed =
        !bot_first_name_lower.empty() && bot_first_name_lower.size() >= 3 &&
        ContainsWord(norm, bot_first_name_lower.c_str());

    // Includes a handful of non-English thanks tokens — the user's server is
    // German-leaning ("danke") and EU players mix DE/FR/ES freely in /p chat.
    const bool has_thanks =
        ContainsWord(norm, "ty")     || ContainsWord(norm, "thx") ||
        ContainsWord(norm, "thanks") || ContainsWord(norm, "thank") ||
        ContainsWord(norm, "tyvm")   || ContainsWord(norm, "danke") ||
        ContainsWord(norm, "merci")  || ContainsWord(norm, "gracias");
    const bool has_grats =
        ContainsWord(norm, "gz")     || ContainsWord(norm, "grats") ||
        ContainsWord(norm, "gratz")  || ContainsWord(norm, "congrats") ||
        ContainsWord(norm, "congratulations") ||
        // "Ding!" / "Ding 47" — bots and players announce a level-up
        // this way. Treat it as a Grats trigger so the rest of the group
        // congratulates on the ding. "Hark!" prefix from Roleplay-verbose
        // bot dings is also recognised. Without this, dings dropped into
        // dead air — humans congratulate every party-ding.
        ContainsWord(norm, "ding") || ContainsWord(norm, "hark");
    const bool has_laugh =
        ContainsWord(norm, "lol")  || ContainsWord(norm, "haha") ||
        ContainsWord(norm, "lmao") || ContainsWord(norm, "xd")   ||
        ContainsWord(norm, "rofl");
    // "gg" / "wp" / "good game" — universal after-kill or end-of-BG line.
    // Folded into Grats reply pool ("gz!"/"nice"/"well done") since the
    // sentiment is the same (acknowledging a finish).
    const bool has_gg =
        ContainsWord(norm, "gg")        || ContainsWord(norm, "wp") ||
        ContainsWord(norm, "wellplayed");
    // Behavioral cues: words real players type to coordinate the pull/pace
    // of the group. Bots don't execute these (the ;-prefix command path
    // does), but a one-line acknowledgement reads like a focused human
    // teammate instead of a silent NPC. "inc"/"incoming" is acknowledged
    // even though no defensive action follows — silence on "inc" feels
    // worse than a "k" while the bot keeps doing its job.
    const bool has_ack =
        ContainsWord(norm, "pull")    || ContainsWord(norm, "pulling") ||
        ContainsWord(norm, "go")      || ContainsWord(norm, "wait")    ||
        ContainsWord(norm, "hold")    || ContainsWord(norm, "stop")    ||
        ContainsWord(norm, "back")    || ContainsWord(norm, "regroup") ||
        ContainsWord(norm, "rg")      || ContainsWord(norm, "inc")     ||
        ContainsWord(norm, "incoming")|| ContainsWord(norm, "afk")     ||
        ContainsWord(norm, "brb");
    const bool has_ready =
        ContainsWord(norm, "ready")   || ContainsWord(norm, "rdy");
    // Greeting tokens — the kind of thing said when a player joins
    // /p or just opens the conversation. EU mix again (German/French/
    // Spanish hello variants). "yo" / "hey" / "hi" / "sup" are also
    // common in younger-skewed groups.
    const bool has_greeting =
        ContainsWord(norm, "hi")    || ContainsWord(norm, "hello") ||
        ContainsWord(norm, "hey")   || ContainsWord(norm, "yo")    ||
        ContainsWord(norm, "sup")   || ContainsWord(norm, "hallo") ||
        ContainsWord(norm, "bonjour") || ContainsWord(norm, "hola") ||
        ContainsWord(norm, "moin");
    // Location query — "where are you" / "wo bist du" / "where r u".
    // Triggers a zone-aware reply ("im in {zone}") instead of the generic
    // "?" direct-address response. SC-P1c: this is now answered on BOTH the
    // whisper AND the party path (when the bot is addressed by name) via the
    // shared BuildLocationReply() helper — previously the party-chat
    // PickReply had no LocationQuery case, so an addressed "where are you
    // Aron?" in /p returned silence. It is still only classified as a
    // LocationQuery when the bot is addressed (a bare party "where do we go
    // next?" must not trip a zone dump from every bot).
    const bool has_location_query =
        ContainsWord(norm, "where") || ContainsWord(norm, "wherer") ||
        ContainsWord(norm, "wo");

    // When a social keyword AND the bot's name are both present, the social
    // keyword wins ("gz Aron" → Aron replies "ty!", not "yes?"). When only
    // the name appears, the address reply ("?", "yes?") fires.
    if (addressed)
    {
        if (has_thanks)         return ReactKind::Thanks;
        if (has_grats || has_gg) return ReactKind::Grats;
        if (has_ready)          return ReactKind::Ready;
        if (has_laugh)          return ReactKind::Laugh;
        if (has_ack)            return ReactKind::Acknowledge;
        if (has_location_query) return ReactKind::LocationQuery;
        if (has_greeting)       return ReactKind::Greeting;
        return ReactKind::DirectAddress;
    }

    if (has_thanks)         return ReactKind::Thanks;
    if (has_grats || has_gg) return ReactKind::Grats;
    if (has_ready)          return ReactKind::Ready;
    if (has_laugh)          return ReactKind::Laugh;
    if (has_ack)            return ReactKind::Acknowledge;
    if (has_greeting)       return ReactKind::Greeting;

    return ReactKind::None;
}

// Sub-kind for the Acknowledge reply pool — lets the reply track the
// kind of cue (pause vs go vs incoming vs general) so a "wait" gets
// "ok holding" instead of a generic "k". Resolved at React-time from
// the normalized message and threaded into PickReply.
enum class AckFlavor : uint8_t { Generic, Pause, Go, Inc, Drink };

// SC-P2a: politeness-aware pool selector. Picks one of three pools by the
// bot's Politeness personality field. Rude bots are terse/curt, polite bots
// add please/thanks/smileys, neutral keeps the existing middle-ground tone.
// A pool may be empty (nullptr, 0) to fall through to neutral — used where a
// kind has no meaningful rude/polite variant.
inline char const* PickFromTier(Politeness pol, uint32 sel,
                                 char const* const* rude,  size_t rude_n,
                                 char const* const* neut,  size_t neut_n,
                                 char const* const* polite, size_t polite_n)
{
    char const* const* pool = neut;
    size_t n = neut_n;
    if (pol == Politeness::Rude   && rude   && rude_n)   { pool = rude;   n = rude_n; }
    if (pol == Politeness::Polite && polite && polite_n) { pool = polite; n = polite_n; }
    if (!pool || n == 0) { pool = neut; n = neut_n; }
    return pool[sel % n];
}
#define POOL(arr) (arr), (sizeof(arr) / sizeof((arr)[0]))

char const* PickReply(ReactKind k, bool addressed, uint32 sel,
                      AckFlavor ack = AckFlavor::Generic,
                      Politeness pol = Politeness::Neutral)
{
    switch (k)
    {
        case ReactKind::Thanks:
        {
            // Whether addressed by name or not, the bot was thanked → "you're welcome".
            static constexpr char const* kRude[]   = { "np", "k", "sure", "yw" };
            static constexpr char const* kNeut[]   = {
                "np", "yw!", "no problem", "anytime",
                "np :)", "happy to help", "no worries",
            };
            static constexpr char const* kPolite[] = {
                "you're welcome!", "happy to help :)", "anytime, friend",
                "my pleasure", "no problem at all :)", "glad to help!",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::Grats:
        {
            // Addressee role: someone congratulated THIS bot → reply with thanks.
            // Otherwise: bot is congratulating a third party → reply with "gz!".
            if (addressed)
            {
                static constexpr char const* kRude[]   = { "ty", "thanks", "cheers" };
                static constexpr char const* kNeut[]   = {
                    "ty!", "thanks!", "thank you :)", "ty :)",
                    "thanks everyone!", "ty all!",
                };
                static constexpr char const* kPolite[] = {
                    "thank you so much! :)", "aw thanks everyone!",
                    "thank you, really appreciate it", "ty all, you're the best :)",
                };
                return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
            }
            static constexpr char const* kRude[]   = { "gz", "gg", "nice" };
            static constexpr char const* kNeut[]   = {
                "gz!", "grats!", "congrats!", "nice :)",
                "gz :)", "well done", "gg!", "wp!",
            };
            static constexpr char const* kPolite[] = {
                "congratulations! :)", "well deserved, gz!", "grats, that's awesome!",
                "nicely done :)", "huge gz!",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::Laugh:
        {
            // Not politeness-sensitive — one neutral pool.
            static constexpr char const* kPool[] = {
                "lol", "haha", "hehe", ":)", "xD",
            };
            return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
        }
        case ReactKind::Acknowledge:
        {
            switch (ack)
            {
                case AckFlavor::Pause:
                {
                    constexpr char const* kPool[] = {
                        "ok holding", "k waiting", "holding", "ok",
                        "k", "wait, got it", "sure",
                    };
                    return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
                }
                case AckFlavor::Go:
                {
                    constexpr char const* kPool[] = {
                        "going", "let's go", "ok pulling", "k", "on it",
                        "incoming dmg", "engage",
                    };
                    return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
                }
                case AckFlavor::Inc:
                {
                    constexpr char const* kPool[] = {
                        "k watching", "got it", "on it", "ready",
                        "k", "watching",
                    };
                    return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
                }
                case AckFlavor::Drink:
                {
                    constexpr char const* kPool[] = {
                        "k drinking", "drinking too", "k",
                        "ok drink", "rest break", "k :)",
                    };
                    return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
                }
                case AckFlavor::Generic:
                default: break;
            }
            static constexpr char const* kRude[]   = { "k", "kk", "fine", "sure" };
            static constexpr char const* kNeut[]   = {
                "k", "ok", "on it", "got it", "k :)", "sure", "kk",
            };
            static constexpr char const* kPolite[] = {
                "sure thing :)", "on it!", "got it, thanks", "okay :)", "will do!",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::Ready:
        {
            // Not politeness-sensitive.
            static constexpr char const* kPool[] = {
                "rdy", "ready", "rdy!", "yes", "yep", "ready :)", "rdy here",
            };
            return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
        }
        case ReactKind::DirectAddress:
        {
            static constexpr char const* kRude[]   = { "?", "what", "hm?", "yeah?" };
            static constexpr char const* kNeut[]   = {
                "?", "yes?", "hm?", "what's up?", "yeah?",
            };
            static constexpr char const* kPolite[] = {
                "yes? :)", "hey, what's up?", "yes, how can i help?", "hi! you called?",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::Greeting:
        {
            static constexpr char const* kRude[]   = { "hi", "hey", "yo", "sup" };
            static constexpr char const* kNeut[]   = {
                "hi", "hi!", "hey", "hello", "yo",
                "hi :)", "heya", "hey there", "hi all",
            };
            static constexpr char const* kPolite[] = {
                "hi there! :)", "hello! :)", "hey, good to see you", "heya :)",
                "hi everyone!", "well met :)",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::None:
        default:
            return nullptr;
    }
}
#undef POOL

// SC-P1c: shared zone-aware reply builder. Resolves the bot's current area
// from AreaTable.db2 and renders one of a few natural "im in {zone}"
// templates. Factored out of ReactWhisper so the party-chat path can answer
// an addressed LocationQuery too (previously PickReply had no LocationQuery
// case, so "where are you Aron?" in /p returned silence). `sel` chooses the
// template + (for politeness) tone. Falls back to a numeric area id when the
// locale string is empty (rare).
std::string BuildLocationReply(Player* bot, uint32 sel, Politeness pol)
{
    uint32 const area_id = bot->GetAreaId();
    std::string area_name;
    if (auto const* ae = sAreaTableStore.LookupEntry(area_id))
        area_name = ae->AreaName[sWorld->GetDefaultDbcLocale()];
    if (area_name.empty())
        area_name = "area " + std::to_string(area_id);

    static constexpr char const* kRude[]   = { "{}", "im in {}", "{} atm" };
    static constexpr char const* kNeut[]   = {
        "im in {}", "{} atm", "over in {}", "currently in {}",
    };
    static constexpr char const* kPolite[] = {
        "i'm over in {} :)", "currently in {}, come find me!",
        "im in {} right now :)", "you'll find me in {}",
    };
    char const* const* pool = kNeut;
    size_t n = sizeof(kNeut) / sizeof(kNeut[0]);
    if (pol == Politeness::Rude)   { pool = kRude;   n = sizeof(kRude) / sizeof(kRude[0]); }
    if (pol == Politeness::Polite) { pool = kPolite; n = sizeof(kPolite) / sizeof(kPolite[0]); }

    std::string t = pool[sel % n];
    auto const pos = t.find("{}");
    if (pos != std::string::npos)
        t.replace(pos, 2, area_name);
    else
        t = area_name;
    return t;
}

// True when the message carries a pause cue ("wait"/"hold"/"stop"/"back"/
// "regroup") — real-player shorthand that means "everyone freeze, don't
// pull yet". back/regroup pause a bit longer because the group is
// typically reforming after a wipe or repositioning.
// Distinct from the broader Acknowledge classification because we only
// want to inject the BotAI pause for these specific tokens, not for
// "pull"/"go" (which mean the opposite) or "afk"/"brb" (which mean
// "the SPEAKER is gone", not "the group should hold").
bool IsPauseCue(std::string const& norm)
{
    return ContainsWord(norm, "wait")    ||
           ContainsWord(norm, "hold")    ||
           ContainsWord(norm, "stop")    ||
           ContainsWord(norm, "back")    ||
           ContainsWord(norm, "regroup") ||
           ContainsWord(norm, "rg");
}
// Drink cue — healer or caster announces a mana break. Triggers a
// LONGER pause (30s) than the standard wait cue because a real drink
// from full empty takes ~25s and you want the tank to wait through
// the whole regen, not just the first sip. Distinct cue lets the
// reply pool acknowledge specifically ("k drink") and not get mixed
// with the generic wait reply.
bool IsDrinkCue(std::string const& norm)
{
    return ContainsWord(norm, "drink") ||
           ContainsWord(norm, "drinking") ||
           ContainsWord(norm, "mb")  ||   // "mana break"
           ContainsWord(norm, "oom");     // "out of mana"
}
// "go" / "pull" / "pulling" — the inverse cue: clear any pause so a
// previous "wait" doesn't keep the group frozen indefinitely.
bool IsGoCue(std::string const& norm)
{
    return ContainsWord(norm, "go")      ||
           ContainsWord(norm, "pull")    ||
           ContainsWord(norm, "pulling") ||
           ContainsWord(norm, "engage");
}

// Pause window — 12s. Long enough to cover a typical "wait, healer is
// drinking" beat; short enough that a forgotten "wait" doesn't strand
// the group. Real players keep this kind of gate mental; the bot just
// uses a fixed timeout instead.
constexpr uint32_t kChatPauseMs = 12u * 1000u;

// First-token-only lowercased bot name. WoW character names are single-word
// in practice; this also lets matching work without case sensitivity.
std::string FirstNameLower(std::string const& name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name)
    {
        if (c == ' ' || c == '\t') break;
        out.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

// Verbosity gate. Returns false when the bot is too quiet to react.
bool VerbosityAllows(Verbosity v, ReactKind k)
{
    if (v == Verbosity::Silent) return false;
    if (v == Verbosity::Terse)
    {
        // Terse bots only answer direct address; they don't volunteer chatter.
        return k == ReactKind::DirectAddress;
    }
    return true;   // Normal / Chatty / Roleplay all react
}

// Stable selection: pick one bot from the candidates list by hash of
// (sender_guid_low ^ message_first_char ^ candidate_count). Same group
// + same speaker tends to get the same talker reply, giving the channel
// a recognizable "this bot speaks for the group" rhythm.
size_t PickReplier(uint32 sender_low, char first_char, size_t candidate_count)
{
    if (candidate_count == 0) return 0;
    const uint32 seed = sender_low ^ uint32(uint8(first_char)) * 2654435761u;
    return size_t(seed) % candidate_count;
}

// SC-P2a: human-pacing reply latency derived from the bot's personality
// instead of hardcoded read/type constants. response_delay_ms is the bot's
// baseline reaction time; response_jitter_ms is the random spread on top of
// it (a fast, decisive player has a small delay+jitter; a distracted one a
// larger one). A read component scales with the incoming message length
// (longer lines take longer to notice/read) and a type component scales with
// the reply length (~55ms/char) so a longer reply visibly takes longer to
// "type". Returns an absolute defer_until timestamp (now + total). `salt`
// (typically bot id) decorrelates the per-bot jitter so two bots don't fire
// in lockstep.
uint32 ComputeReplyDeferMs(uint32 now_ms, BotPersonality const& p,
                           size_t in_len, size_t reply_len, uint32 salt)
{
    const uint32 base   = p.response_delay_ms;
    const uint32 jitter = (p.response_jitter_ms == 0)
        ? 0u
        : (((salt * 2654435761u) ^ now_ms) % (uint32(p.response_jitter_ms) + 1u));
    const uint32 read_ms = uint32(in_len) * 30u;          // scan/read the line
    const uint32 type_ms = uint32(reply_len) * 55u + 120u; // type the reply
    return now_ms + base + jitter + read_ms + type_ms;
}

} // namespace

void BotChatReactor::React(Player* sender, Group* group, std::string const& msg)
{
    if (!sender || !group || msg.empty()) return;
    // Bot speakers never trigger reactions — prevents echo storms when
    // multiple V2 bots are in the same group.
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;
    // Don't react to our own command messages (`;`-prefix handled by
    // BotCommandParser; the parser is upstream of us already in OnPartyChat).
    if (msg.front() == ';' || msg.front() == '.' || msg.front() == '/') return;

    const std::string norm = Normalize(msg);
    if (norm.size() < 2) return;

    // Build candidate list: bots in the group that are registered with V2
    // and whose verbosity allows reaction. Classify per-bot because the
    // direct-address check depends on each bot's name.
    struct Candidate
    {
        Player*    bot;
        BotId      id;
        BotAI*     ai;          // for personality (politeness / pacing) — SC-P2a
        ReactKind  kind;
        bool       addressed;   // bot's own name appeared in the message
    };
    std::vector<Candidate> candidates;
    candidates.reserve(8);

    BotRegistry& reg = Services::Registry();

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == sender) continue;
        if (!member->GetSession() || !member->GetSession()->IsBot()) continue;
        // Dead bots stay silent — a ghost saying "ty!" reads as broken. The
        // ghost will rejoin chatter once rezzed and snapshot ticks Idle.
        if (!member->IsAlive()) continue;
        const BotId mid = member->GetGUID().GetCounter();
        if (!reg.has(mid)) continue;

        BotAI* ai = reg.ai(mid);
        if (!ai) continue;
        const Verbosity v = ai->personality().verbosity;
        if (v == Verbosity::Silent) continue;

        const std::string first = FirstNameLower(member->GetName());
        ReactKind kind = ClassifyMessage(norm, first);
        if (kind == ReactKind::None) continue;
        if (!VerbosityAllows(v, kind)) continue;

        const bool addr = !first.empty() && first.size() >= 3 &&
                          ContainsWord(norm, first.c_str());
        candidates.push_back({member, mid, ai, kind, addr});
    }

    // Behavioral cue: inject a per-bot pause when the SPEAKER (a real
    // player) said wait/hold/stop/back/regroup, OR a shorter pause for
    // inc/incoming, OR a LONGER pause for drink/oom (mana break = full
    // regen takes ~25s). "go"/"pull" clears any pause so a stuck "wait"
    // can be lifted by saying "go".
    {
        const bool pause = IsPauseCue(norm);
        const bool go    = IsGoCue(norm);
        const bool inc   = ContainsWord(norm, "inc") ||
                           ContainsWord(norm, "incoming");
        const bool drink = IsDrinkCue(norm);
        if (pause || go || inc || drink)
        {
            BotRegistry& reg2 = Services::Registry();
            const uint32 now2 = GameTime::GetGameTimeMS();
            // "inc" earns a shorter window than wait/hold — it's an
            // alert, not an indefinite hold. 8s is enough to span the
            // typical incoming-enemy arrival without committing to a
            // long freeze. "drink" stretches the pause to 30s so a
            // healer drinking from empty actually finishes regen.
            constexpr uint32 kIncAlertMs   = 8u * 1000u;
            constexpr uint32 kDrinkPauseMs = 30u * 1000u;
            for (GroupReference const& itr : group->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member || member == sender) continue;
                if (!member->GetSession() || !member->GetSession()->IsBot()) continue;
                const BotId mid = member->GetGUID().GetCounter();
                if (!reg2.has(mid)) continue;
                if (BotAI* mai = reg2.ai(mid))
                {
                    // Priority: drink > pause > inc > go (longer windows
                    // override shorter; explicit "go" still clears).
                    if (drink)       mai->set_chat_pause_until_ms(now2 + kDrinkPauseMs);
                    else if (pause)  mai->set_chat_pause_until_ms(now2 + kChatPauseMs);
                    else if (inc)    mai->set_chat_pause_until_ms(now2 + kIncAlertMs);
                    else /* go */    mai->set_chat_pause_until_ms(0);
                }
            }
        }
    }

    if (candidates.empty()) return;

    // When ANY candidate is addressed (their name appeared), only addressed
    // bots reply — picking a random non-addressed bot would create the
    // "wrong bot answered" effect. Otherwise pick one bot stably by hash.
    bool any_addressed = false;
    for (auto const& c : candidates) if (c.addressed) { any_addressed = true; break; }

    const uint32 now_ms      = GameTime::GetGameTimeMS();
    const uint32 sender_low  = sender->GetGUID().GetCounter();
    const char first_ch      = norm.empty() ? '?' : norm[0];

    // Resolve Acknowledge flavor once for the whole reply batch — every
    // reply for this message uses the same cue context.
    AckFlavor ack_flavor = AckFlavor::Generic;
    if (IsDrinkCue(norm))                       ack_flavor = AckFlavor::Drink;
    else if (IsPauseCue(norm))                  ack_flavor = AckFlavor::Pause;
    else if (IsGoCue(norm))                     ack_flavor = AckFlavor::Go;
    else if (ContainsWord(norm, "inc") ||
             ContainsWord(norm, "incoming"))    ack_flavor = AckFlavor::Inc;

    // `extra_delay_ms` staggers a 2nd addressed bot so a duo doesn't reply on
    // the same tick (SC-P1b multi-name cap fallback).
    auto emit_reply = [&](Candidate const& c, uint32 extra_delay_ms)
    {
        // SC-P1b: tiered cooldown. Addressed reactions use a SHORTER window
        // (20s) than ambient chatter (90s) — silence when called by name
        // reads as broken — but they are NO LONGER cooldown-exempt, and the
        // timestamp is ALWAYS written. The old code skipped the throttle AND
        // the stamp for addressed replies, so a name-spammer triggered a
        // reply on every line and both bots in a duo answered repeatedly.
        const uint32 cd = c.addressed ? kAddressedReactCooldownMs
                                      : kPerBotReactCooldownMs;
        {
            std::lock_guard<std::mutex> lk(g_react_mtx);
            auto it = g_last_react_ms.find(c.id);
            if (it != g_last_react_ms.end() && now_ms - it->second < cd)
                return;
            // SC-P3b: bound map growth — sweep keys older than 2x the longest
            // per-bot cooldown before inserting (only fires past threshold).
            PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                       kReactMapPruneThreshold);
            g_last_react_ms[c.id] = now_ms;   // always stamp on a real reply
        }

        const uint32 sel = (sender_low ^ uint32(c.id) ^ now_ms);
        const Politeness pol = c.ai ? c.ai->personality().politeness
                                    : Politeness::Neutral;
        // SC-P1c: an addressed LocationQuery in party chat now answers with
        // the bot's zone (shared BuildLocationReply), instead of returning
        // silent because PickReply has no LocationQuery case.
        std::string reply_str;
        if (c.kind == ReactKind::LocationQuery)
        {
            reply_str = BuildLocationReply(c.bot, sel, pol);
        }
        else
        {
            char const* reply = PickReply(c.kind, c.addressed, sel, ack_flavor, pol);
            if (!reply || !*reply) return;
            reply_str = reply;
        }

        const size_t reply_len = reply_str.size();
        Intent it{};
        it.bot_id = c.id;
        it.body   = ChatIntent{PartyChatIntent{std::move(reply_str)}};
        // SC-P2a: personality-paced latency (see ComputeReplyDeferMs), plus a
        // per-replier stagger for multi-name lines.
        const BotPersonality& p = c.ai ? c.ai->personality() : DefaultPersonality();
        it.defer_until_ms = ComputeReplyDeferMs(now_ms, p, norm.size(),
                                                reply_len, uint32(c.id))
                            + extra_delay_ms;
        Services::Intents(c.id).push(std::move(it));
    };

    if (any_addressed)
    {
        // SC-P1b: cap a multi-name line to ONE replying bot, staggering a
        // 2nd by +1.5s. A line like "gz Aron and Bren" should not make both
        // answer simultaneously (reads as scripted); at most two reply, the
        // second a beat later. Beyond two, the rest stay silent.
        uint32 replied = 0;
        for (auto const& c : candidates)
        {
            if (!c.addressed) continue;
            if (replied >= 2) break;
            emit_reply(c, replied == 1 ? 1500u : 0u);
            ++replied;
        }
        return;
    }

    // Pick exactly one bot to speak for the group. PickReplier is stable
    // across messages so the same bot tends to be "the talker" of the group.
    const size_t idx = PickReplier(sender_low, first_ch, candidates.size());
    emit_reply(candidates[idx], 0u);
}

// Whisper social reactor. Real players reply when strangers whisper "hi"
// or "ty". The reactor is reused here: classify the message against the
// same keyword table and emit a whisper-back via the bot's intent queue.
// Distinct from ReactBot (party-chat) in that:
//   * Single target (no group iteration / candidate picking)
//   * Reply goes via WhisperIntent, not PartyChatIntent
//   * Same 90s per-bot throttle, but tracked separately so a whisper
//     from a player who never sees /p chat doesn't get blocked by the
//     bot's last /p reply
// Sender must be a real player. Bot self-talk and ;-prefix commands
// are filtered upstream in PlayerbotV2.cpp::OnWhisperReceived.
void BotChatReactor::ReactWhisper(Player* sender, Player* bot, std::string const& msg)
{
    if (!sender || !bot || msg.empty()) return;
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;
    if (msg.front() == ';' || msg.front() == '.' || msg.front() == '/') return;
    const BotId mid = bot->GetGUID().GetCounter();
    BotRegistry& reg = Services::Registry();
    if (!reg.has(mid)) return;
    BotAI* ai = reg.ai(mid);
    if (!ai) return;
    const Verbosity v = ai->personality().verbosity;
    if (v == Verbosity::Silent) return;
    if (!bot->IsAlive()) return;
    const std::string norm = Normalize(msg);
    if (norm.size() < 2) return;
    const std::string first_name = FirstNameLower(bot->GetName());
    ReactKind kind = ClassifyMessage(norm, first_name);
    if (kind == ReactKind::None) return;
    if (!VerbosityAllows(v, kind)) return;

    // Per-bot 90s throttle (same key namespace as party-chat reactor so a
    // bot doesn't fire two reactions in 1s across both channels).
    const uint32 now_ms = GameTime::GetGameTimeMS();
    {
        std::lock_guard<std::mutex> lk(g_react_mtx);
        auto it = g_last_react_ms.find(mid);
        if (it != g_last_react_ms.end() &&
            now_ms - it->second < kPerBotReactCooldownMs)
            return;
        // SC-P3b: bound map growth (see PruneStale).
        PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                   kReactMapPruneThreshold);
        g_last_react_ms[mid] = now_ms;
    }

    // Whisper-flavored ack — same pool selectors but DirectAddress
    // applies because every whisper is implicitly addressed to the bot.
    const uint32 sender_low = sender->GetGUID().GetCounter();
    const uint32 sel = sender_low ^ uint32(mid) ^ now_ms;
    // SC-P2a: personality politeness drives the reply tone.
    const Politeness pol = ai->personality().politeness;
    std::string reply_str;
    if (kind == ReactKind::LocationQuery)
    {
        // SC-P1c: shared zone-aware reply builder (also used by the party
        // path now). Resolves the bot's current AreaTable.db2 zone.
        reply_str = BuildLocationReply(bot, sel, pol);
    }
    else
    {
        char const* reply = PickReply(kind, /*addressed=*/true, sel,
                                      AckFlavor::Generic, pol);
        if (!reply || !*reply) return;
        reply_str = reply;
    }
    Intent it{};
    it.bot_id = mid;
    it.body   = ChatIntent{WhisperIntent{sender->GetName(), reply_str}};
    // SC-P2a: human-pacing now derives from the bot's personality
    // (response_delay_ms + rng(response_jitter_ms)) plus length-scaled
    // read/type components, instead of the old hardcoded 500ms/60ms-per-char
    // constants. The Intent's defer_until_ms is honored by the DrainIntents
    // dispatcher (BotIntentExecutor.cpp) — the queued intent waits until now
    // passes the deadline before executing.
    it.defer_until_ms = ComputeReplyDeferMs(now_ms, ai->personality(),
                                            norm.size(), reply_str.size(), uint32(mid));
    Services::Intents(mid).push(std::move(it));
}

// Phase C.3 guild-chat reactor. Picks one online officer-or-better of
// the sender's guild and emits a contextual guild_chat reply through
// the bot's intent queue. Per-guild throttle prevents reaction storms
// when multiple players chat back-to-back.
void BotChatReactor::ReactGuild(Player* sender, uint64 guild_id, std::string const& msg)
{
    if (!sender || guild_id == 0 || msg.empty()) return;
    // Filter bot speakers — same echo-storm guard as React().
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;
    // Skip command prefixes / addon traffic that isn't social chat.
    if (msg.front() == ';' || msg.front() == '.' || msg.front() == '/') return;

    Guild* g = sGuildMgr->GetGuildById(guild_id);
    if (!g) return;

    // Per-guild throttle (separate from per-bot to coexist with party
    // chat). 60s window — slightly looser than party's 90s because
    // guild chat has fewer total speakers per minute on average and
    // we want guild conversation to feel responsive.
    static std::mutex                            s_guild_react_mtx;
    static std::unordered_map<uint64, uint32_t>  s_last_guild_react_ms;
    constexpr uint32_t kPerGuildReactCooldownMs = 60u * 1000u;
    const uint32 now_ms = GameTime::GetGameTimeMS();
    {
        std::lock_guard<std::mutex> lk(s_guild_react_mtx);
        auto it = s_last_guild_react_ms.find(guild_id);
        if (it != s_last_guild_react_ms.end() &&
            now_ms - it->second < kPerGuildReactCooldownMs)
            return;
        // Defer the stamp until after we successfully pick a speaker —
        // if no eligible officer exists, the next message gets a real
        // chance. (Re-grab the mutex below.)
    }

    const std::string norm = Normalize(msg);
    if (norm.size() < 2) return;

    // Walk guild members; pick the highest-rank online bot whose
    // classifier produces a non-None reply for the message. Officer+
    // preferred (the "talker" role); falls back to any rank when no
    // officer is online.
    BotRegistry& reg = Services::Registry();
    struct GCand
    {
        Player*    bot;
        BotId      id;
        uint8      rank;
        ReactKind  kind;
        bool       addressed;
    };
    std::vector<GCand> candidates;
    candidates.reserve(8);

    for (auto const& [mguid, member] : g->GetMembers())
    {
        if (!member.IsOnline()) continue;
        if (mguid == sender->GetGUID()) continue;
        Player* mp = ObjectAccessor::FindConnectedPlayer(mguid);
        if (!mp) continue;
        if (!mp->GetSession() || !mp->GetSession()->IsBot()) continue;
        if (!mp->IsAlive()) continue;
        const BotId mid = mguid.GetCounter();
        if (!reg.has(mid)) continue;
        BotAI* ai = reg.ai(mid);
        if (!ai) continue;
        const Verbosity v = ai->personality().verbosity;
        if (v == Verbosity::Silent) continue;

        const std::string first = FirstNameLower(mp->GetName());
        ReactKind kind = ClassifyMessage(norm, first);
        if (kind == ReactKind::None) continue;
        if (!VerbosityAllows(v, kind)) continue;

        const bool addr = !first.empty() && first.size() >= 3 &&
                          ContainsWord(norm, first.c_str());
        candidates.push_back({mp, mid, static_cast<uint8>(member.GetRankId()),
                              kind, addr});
    }
    if (candidates.empty()) return;

    // Sort: addressed bots first, then by rank ascending (lower rank id
    // = higher real rank; GM=0 outranks Officer=1 etc.).
    std::sort(candidates.begin(), candidates.end(),
        [](GCand const& a, GCand const& b) {
            if (a.addressed != b.addressed) return a.addressed;
            return a.rank < b.rank;
        });

    const uint32 sender_low = sender->GetGUID().GetCounter();
    auto const& chosen = candidates.front();
    const uint32 sel = sender_low ^ uint32(chosen.id) ^ now_ms;
    char const* reply = PickReply(chosen.kind, chosen.addressed, sel);
    if (!reply || !*reply) return;

    // Stamp the throttle only AFTER confirming a reply will emit. The
    // earlier-commented intent ("defer the stamp until after we
    // successfully pick a speaker — if no eligible officer exists, the
    // next message gets a real chance") was contradicted by stamping
    // before the PickReply nullptr guard. With the guard added in
    // anticipation of future ReactKinds whose reply pool may be
    // conditional, an empty-reply path would otherwise silence the
    // entire guild for the full 60s cooldown without anyone speaking.
    {
        std::lock_guard<std::mutex> lk(s_guild_react_mtx);
        // SC-P3b: bound per-guild map growth — guilds form and disband over
        // a long uptime; sweep keys older than 2x the per-guild cooldown.
        PruneStale(s_last_guild_react_ms, now_ms, 2u * kPerGuildReactCooldownMs,
                   kReactMapPruneThreshold);
        s_last_guild_react_ms[guild_id] = now_ms;
    }

    Intent it{};
    it.bot_id = chosen.id;
    it.body   = ChatIntent{GuildChatIntent{std::string(reply)}};
    // Guild chat is typically less time-pressured than party. Slightly
    // longer read/type window (1–4s end-to-end).
    const uint32 reply_len = uint32(std::strlen(reply));
    const uint32 read_ms3  = 600u + uint32(norm.size()) * 40u;
    const uint32 type_ms3  = reply_len * 65u + 250u;
    const uint32 jitter3   = (uint32(chosen.id) * 2654435761u) % 800u;
    it.defer_until_ms = now_ms + read_ms3 + type_ms3 + jitter3;
    Services::Intents(chosen.id).push(std::move(it));
}

namespace {

// SC-P1a: shared /say + /yell reaction core. Anchored on the speaking
// player's position, runs a grid-bounded PlayerListSearcher within `range`
// (CONFIG_LISTEN_RANGE_SAY / _YELL — never a magic number), gathers nearby
// V2 bots, classifies the line per-bot, then lets at most ONE answer:
//   * If a bot was addressed by name → that bot wins.
//   * Otherwise → one bot chosen stably by the guid-hash PickReplier, with a
//     probabilistic fire gate so a hub of bots doesn't all qualify and
//     answer every line. `one_in_n` is the inverse fire chance for the
//     ambient (non-addressed) case — say uses a low N (answers often), yell
//     a high N (answers rarely, matching how players treat yells).
// Cooldowns: per-bot (tiered, via the same g_last_react_ms map as party /
// whisper) AND per-area (g_last_area_react_ms) so crowded hubs stay calm at
// scale. Yell/say replies are biased toward greetings, direct-name mentions
// and location questions (handled by the classifier + the channel parameter
// below): an ambient "lol" in /say from a stranger you can't see is NOT
// answered — only socially-directed lines are.
void ReactSpoken(Player* sender, std::string const& msg, bool is_yell,
                 uint32 one_in_n)
{
    if (!sender || msg.empty()) return;
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;
    if (msg.front() == ';' || msg.front() == '.' || msg.front() == '/') return;

    const std::string norm = Normalize(msg);
    if (norm.size() < 2) return;

    // Range from server config — the same listen ranges the core uses to
    // decide who HEARS the say/yell, so a bot only answers what it could
    // actually have heard.
    const float range = sWorld->getFloatConfig(
        is_yell ? CONFIG_LISTEN_RANGE_YELL : CONFIG_LISTEN_RANGE_SAY);

    // Per-area cooldown gate, keyed by (map<<32 | area). Cheap pre-check
    // before the (more expensive) grid walk.
    const uint64 area_key =
        (uint64(sender->GetMapId()) << 32) | uint64(sender->GetAreaId());
    const uint32 now_ms = GameTime::GetGameTimeMS();
    {
        std::lock_guard<std::mutex> lk(g_area_react_mtx);
        auto it = g_last_area_react_ms.find(area_key);
        if (it != g_last_area_react_ms.end() &&
            now_ms - it->second < kPerAreaReactCooldownMs)
            return;
    }

    // Grid-bounded nearby-player query anchored on the speaker.
    std::list<Player*> nearby;
    Position const senderPos = sender->GetPosition();
    Trinity::AnyPlayerInPositionRangeCheck check(&senderPos, range, /*reqAlive=*/true);
    Trinity::PlayerListSearcher<Trinity::AnyPlayerInPositionRangeCheck>
        searcher(sender, nearby, check);
    Cell::VisitWorldObjects(sender, searcher, range);

    BotRegistry& reg = Services::Registry();
    struct SCand { Player* bot; BotId id; BotAI* ai; ReactKind kind; bool addressed; };
    std::vector<SCand> candidates;
    candidates.reserve(8);

    for (Player* member : nearby)
    {
        if (!member || member == sender) continue;
        if (!member->GetSession() || !member->GetSession()->IsBot()) continue;
        if (!member->IsAlive()) continue;
        const BotId mid = member->GetGUID().GetCounter();
        if (!reg.has(mid)) continue;
        BotAI* ai = reg.ai(mid);
        if (!ai) continue;
        const Verbosity v = ai->personality().verbosity;
        if (v == Verbosity::Silent) continue;

        const std::string first = FirstNameLower(member->GetName());
        ReactKind kind = ClassifyMessage(norm, first);
        if (kind == ReactKind::None) continue;
        if (!VerbosityAllows(v, kind)) continue;

        const bool addr = !first.empty() && first.size() >= 3 &&
                          ContainsWord(norm, first.c_str());
        // Ambient (non-addressed) bias: in open /say|/yell a real player only
        // answers socially-directed lines — a greeting, a "gz", or an "anyone
        // know where X is?" (LocationQuery). They don't reply to a random
        // "lol" or "k" yelled across the zone. Addressed lines always pass.
        if (!addr)
        {
            const bool worth_answering =
                kind == ReactKind::Greeting ||
                kind == ReactKind::Grats    ||
                kind == ReactKind::Thanks   ||
                kind == ReactKind::LocationQuery;
            if (!worth_answering) continue;
        }
        candidates.push_back({member, mid, ai, kind, addr});
    }
    if (candidates.empty()) return;

    const uint32 sender_low = sender->GetGUID().GetCounter();
    const char   first_ch   = norm[0];

    // Resolve Acknowledge flavor (rare in /say, but keeps replies consistent).
    AckFlavor ack_flavor = AckFlavor::Generic;
    if (IsDrinkCue(norm))                       ack_flavor = AckFlavor::Drink;
    else if (IsPauseCue(norm))                  ack_flavor = AckFlavor::Pause;
    else if (IsGoCue(norm))                     ack_flavor = AckFlavor::Go;
    else if (ContainsWord(norm, "inc") ||
             ContainsWord(norm, "incoming"))    ack_flavor = AckFlavor::Inc;

    // Prefer an addressed bot; else pick one stably by hash and apply the
    // probabilistic fire gate so not every ambient line is answered.
    SCand const* chosen = nullptr;
    for (auto const& c : candidates)
        if (c.addressed) { chosen = &c; break; }
    if (!chosen)
    {
        // Probabilistic ambient gate. Deterministic per (sender, line) so the
        // same line doesn't flip-flop across re-entries. one_in_n controls
        // how chatty the channel is (say low, yell high).
        const uint32 roll = (sender_low ^ uint32(uint8(first_ch)) * 2654435761u);
        if (one_in_n > 1 && (roll % one_in_n) != 0) return;
        const size_t idx = PickReplier(sender_low, first_ch, candidates.size());
        chosen = &candidates[idx];
    }

    // Per-bot tiered cooldown + always-stamp (SC-P1b parity).
    const uint32 cd = chosen->addressed ? kAddressedReactCooldownMs
                                        : kPerBotReactCooldownMs;
    {
        std::lock_guard<std::mutex> lk(g_react_mtx);
        auto it = g_last_react_ms.find(chosen->id);
        if (it != g_last_react_ms.end() && now_ms - it->second < cd)
            return;
        // SC-P3b: bound map growth (see PruneStale).
        PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                   kReactMapPruneThreshold);
        g_last_react_ms[chosen->id] = now_ms;
    }
    // Stamp the per-area cooldown now that a reply is committed.
    {
        std::lock_guard<std::mutex> lk(g_area_react_mtx);
        // SC-P3b: areas are visited transiently (players roam); without a
        // sweep this map accretes a key per (map,area) ever spoken in. Keep
        // 2x the per-area cooldown.
        PruneStale(g_last_area_react_ms, now_ms, 2u * kPerAreaReactCooldownMs,
                   kReactMapPruneThreshold);
        g_last_area_react_ms[area_key] = now_ms;
    }

    const uint32 sel = sender_low ^ uint32(chosen->id) ^ now_ms;
    const Politeness pol = chosen->ai ? chosen->ai->personality().politeness
                                      : Politeness::Neutral;
    std::string reply_str;
    if (chosen->kind == ReactKind::LocationQuery)
        reply_str = BuildLocationReply(chosen->bot, sel, pol);
    else
    {
        char const* reply = PickReply(chosen->kind, chosen->addressed, sel,
                                      ack_flavor, pol);
        if (!reply || !*reply) return;
        reply_str = reply;
    }

    // The bot answers in the SAME channel it heard (say↔say, yell↔yell), via
    // the racial-language-aware say()/yell() API path.
    const size_t reply_len = reply_str.size();
    Intent it{};
    it.bot_id = chosen->id;
    if (is_yell)
        it.body = ChatIntent{YellChatIntent{std::move(reply_str)}};
    else
        it.body = ChatIntent{SayChatIntent{std::move(reply_str)}};
    const BotPersonality& p = chosen->ai ? chosen->ai->personality()
                                         : DefaultPersonality();
    it.defer_until_ms = ComputeReplyDeferMs(now_ms, p, norm.size(),
                                            reply_len, uint32(chosen->id));
    Services::Intents(chosen->id).push(std::move(it));
}

} // namespace

void BotChatReactor::ReactSay(Player* sender, std::string const& msg)
{
    // Say is answered relatively often (1-in-2 ambient gate) since /say is
    // close-range and conversational.
    ReactSpoken(sender, msg, /*is_yell=*/false, /*one_in_n=*/2u);
}

void BotChatReactor::ReactYell(Player* sender, std::string const& msg)
{
    // Yell is answered rarely (1-in-6 ambient gate) — players mostly ignore
    // zone yells unless directly named (addressed bots still bypass the gate).
    ReactSpoken(sender, msg, /*is_yell=*/true, /*one_in_n=*/6u);
}

// SC-P2c: reciprocate a nearby player's text emote. A targeted emote (wave AT
// a specific bot) is answered by THAT bot; an untargeted one by a single
// nearby bot picked by hash. The reply is a reciprocal PerformEmoteIntent on
// a human-paced delay with the same per-bot cooldown namespace.
void BotChatReactor::ReactEmote(Player* sender, uint32 emote_id, ObjectGuid target)
{
    if (!sender) return;
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;

    BotRegistry& reg = Services::Registry();
    const uint32 now_ms = GameTime::GetGameTimeMS();

    // Reciprocal emote map — answer a wave with a wave, a salute with a
    // salute, a bow with a bow, a cheer with a cheer, applaud with applaud.
    // Falls back to a friendly wave for anything not explicitly mapped so a
    // bot never apes a /rude or /spit. Emote ids are SharedDefines.h Emote
    // enum values, matching the constants BotCommandParser uses for /wave.
    auto reciprocal = [](uint32 in) -> uint32 {
        switch (in)
        {
            case 3:   return 3;    // EMOTE_ONESHOT_WAVE    -> wave back
            case 66:  return 66;   // EMOTE_ONESHOT_SALUTE  -> salute back
            case 2:   return 2;    // EMOTE_ONESHOT_BOW     -> bow back
            case 4:   return 4;    // EMOTE_ONESHOT_CHEER   -> cheer back
            case 21:  return 21;   // EMOTE_ONESHOT_APPLAUD -> applaud back
            case 24:  return 3;    // EMOTE_ONESHOT_SHY     -> friendly wave
            default:  return 3;    // safe default: wave (never apes /rude /spit)
        }
    };

    // Helper: emit a reciprocal emote from one bot toward the sender.
    auto emit_emote = [&](Player* bot) -> bool {
        if (!bot || !bot->IsAlive()) return false;
        if (bot == sender) return false;
        if (!bot->GetSession() || !bot->GetSession()->IsBot()) return false;
        const BotId mid = bot->GetGUID().GetCounter();
        if (!reg.has(mid)) return false;
        BotAI* ai = reg.ai(mid);
        if (!ai) return false;
        if (ai->personality().verbosity == Verbosity::Silent) return false;
        // Per-bot cooldown (shared namespace with chat replies).
        {
            std::lock_guard<std::mutex> lk(g_react_mtx);
            auto it = g_last_react_ms.find(mid);
            if (it != g_last_react_ms.end() &&
                now_ms - it->second < kAddressedReactCooldownMs)
                return false;
            // SC-P3b: bound map growth (see PruneStale).
            PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                       kReactMapPruneThreshold);
            g_last_react_ms[mid] = now_ms;
        }
        Intent it{};
        it.bot_id = mid;
        it.body   = PerformEmoteIntent{reciprocal(emote_id), sender->GetGUID()};
        it.defer_until_ms = ComputeReplyDeferMs(now_ms, ai->personality(),
                                                /*in_len=*/8, /*reply_len=*/0,
                                                uint32(mid));
        Services::Intents(mid).push(std::move(it));
        return true;
    };

    // Targeted emote → only the targeted bot reciprocates.
    if (!target.IsEmpty() && target.IsPlayer())
    {
        if (Player* tgt = ObjectAccessor::FindConnectedPlayer(target))
            emit_emote(tgt);
        return;
    }

    // Untargeted emote → pick one nearby bot (range = text-emote listen
    // range) to wave/cheer back. Grid-bounded query, single replier by hash.
    const float range = sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE);
    std::list<Player*> nearby;
    Position const senderPos = sender->GetPosition();
    Trinity::AnyPlayerInPositionRangeCheck check(&senderPos, range, /*reqAlive=*/true);
    Trinity::PlayerListSearcher<Trinity::AnyPlayerInPositionRangeCheck>
        searcher(sender, nearby, check);
    Cell::VisitWorldObjects(sender, searcher, range);

    std::vector<Player*> bots;
    bots.reserve(8);
    for (Player* member : nearby)
    {
        if (!member || member == sender) continue;
        if (!member->GetSession() || !member->GetSession()->IsBot()) continue;
        if (!member->IsAlive()) continue;
        if (!reg.has(member->GetGUID().GetCounter())) continue;
        bots.push_back(member);
    }
    if (bots.empty()) return;
    const uint32 sender_low = sender->GetGUID().GetCounter();
    const size_t idx = PickReplier(sender_low, char(emote_id & 0xFF), bots.size());
    emit_emote(bots[idx]);
}

// SC-P2b: welcome a freshly-joined guild member. One online bot guildmate
// (deterministic by guid hash) drops a welcome line a few seconds later.
// Per-guild throttle prevents a recruiting spree from spamming chat.
void BotChatReactor::ReactGuildJoin(uint64 guild_id, ObjectGuid joiner_guid,
                                    std::string const& joiner_name)
{
    if (guild_id == 0) return;
    Guild* g = sGuildMgr->GetGuildById(guild_id);
    if (!g) return;

    // Don't welcome auto-spawned bot recruits — only real-player joins (a bot
    // welcoming a bot is pure noise). The joining player may be offline at add
    // time; if online and flagged as a bot, skip.
    if (Player* jp = ObjectAccessor::FindConnectedPlayer(joiner_guid))
        if (jp->GetSession() && jp->GetSession()->IsBot())
            return;

    static std::mutex                           s_join_mtx;
    static std::unordered_map<uint64, uint32_t> s_last_join_ms;
    constexpr uint32_t kPerGuildJoinCooldownMs = 30u * 1000u;
    const uint32 now_ms = GameTime::GetGameTimeMS();
    {
        std::lock_guard<std::mutex> lk(s_join_mtx);
        auto it = s_last_join_ms.find(guild_id);
        if (it != s_last_join_ms.end() &&
            now_ms - it->second < kPerGuildJoinCooldownMs)
            return;
    }

    // Collect online bot guildmates (excluding the joiner). Pick one stably.
    BotRegistry& reg = Services::Registry();
    struct WCand { BotId id; uint8 rank; };
    std::vector<WCand> cands;
    cands.reserve(8);
    for (auto const& [mguid, member] : g->GetMembers())
    {
        if (!member.IsOnline()) continue;
        if (mguid == joiner_guid) continue;
        Player* mp = ObjectAccessor::FindConnectedPlayer(mguid);
        if (!mp || !mp->GetSession() || !mp->GetSession()->IsBot()) continue;
        if (!mp->IsAlive()) continue;
        const BotId mid = mguid.GetCounter();
        if (!reg.has(mid)) continue;
        BotAI* ai = reg.ai(mid);
        if (!ai || ai->personality().verbosity == Verbosity::Silent) continue;
        cands.push_back({mid, static_cast<uint8>(member.GetRankId())});
    }
    if (cands.empty()) return;

    // Prefer the highest rank (lowest rank id) as the "greeter", tie-broken by
    // a stable hash so the same officer tends to welcome people.
    std::sort(cands.begin(), cands.end(),
        [](WCand const& a, WCand const& b) { return a.rank < b.rank; });
    const uint32 seed = uint32(guild_id) ^ uint32(joiner_guid.GetCounter());
    // Pick among the top-rank tier for variety.
    uint8 top_rank = cands.front().rank;
    size_t top_n = 0;
    while (top_n < cands.size() && cands[top_n].rank == top_rank) ++top_n;
    const BotId greeter = cands[seed % top_n].id;

    // Compose a welcome line. Address the joiner by first name where possible.
    std::string first = joiner_name;
    if (auto sp = first.find(' '); sp != std::string::npos) first.resize(sp);
    static constexpr char const* kTemplates[] = {
        "welcome {}!", "welcome to the guild {} :)", "hey {}, welcome!",
        "welcome aboard {}!", "{} welcome! :)", "glad to have you {}!",
    };
    constexpr size_t kN = sizeof(kTemplates) / sizeof(*kTemplates);
    std::string line = kTemplates[seed % kN];
    if (auto pos = line.find("{}"); pos != std::string::npos)
    {
        if (!first.empty()) line.replace(pos, 2, first);
        else                line.replace(pos, 2, "friend");
    }

    // Stamp throttle only now that we will emit.
    {
        std::lock_guard<std::mutex> lk(s_join_mtx);
        // SC-P3b: bound per-guild map growth (see PruneStale).
        PruneStale(s_last_join_ms, now_ms, 2u * kPerGuildJoinCooldownMs,
                   kReactMapPruneThreshold);
        s_last_join_ms[guild_id] = now_ms;
    }

    Intent it{};
    it.bot_id = greeter;
    it.body   = ChatIntent{GuildChatIntent{std::move(line)}};
    // A few-second delay so the welcome lands AFTER the system join line and
    // reads like a person noticing the join notification, not a trigger.
    BotPersonality const& gp = reg.ai(greeter) ? reg.ai(greeter)->personality()
                                               : DefaultPersonality();
    const uint32 base = 2500u + (seed % 2500u);   // 2.5–5s noticing delay
    it.defer_until_ms = now_ms + base + gp.response_delay_ms;
    Services::Intents(greeter).push(std::move(it));
}

} // namespace Playerbot
