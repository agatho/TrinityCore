#include "BattlegroundScript.h"
#include "../BotSnapshotView.h"
#include "../BotSnapshot.h"
#include "Log.h"

#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace Playerbot {

namespace {

// Cross-bot callout-claim state. Process-global (one BG fleet per
// worldserver). Hash key combines callout-kind in high 32 bits with
// the per-callout key (node entry / FC guid bucket / etc) in low.
std::shared_mutex                        g_callout_mtx;
std::unordered_map<uint64_t, uint32_t>   g_callout_until_ms;

inline uint64_t MakeCalloutKey(uint32_t kind, uint64_t key)
{
    return (uint64_t(kind) << 32) | (key & 0x00000000FFFFFFFFull);
}

} // anonymous

bool BgCalloutCoordinator::TryClaim(uint32_t kind, uint64_t key,
                                    uint32_t now_ms, uint32_t lockout_ms)
{
    const uint64_t k = MakeCalloutKey(kind, key);
    {
        std::shared_lock<std::shared_mutex> lk(g_callout_mtx);
        auto it = g_callout_until_ms.find(k);
        if (it != g_callout_until_ms.end() && now_ms < it->second)
            return false; // somebody else already shouted within window
    }
    std::unique_lock<std::shared_mutex> lk(g_callout_mtx);
    // Re-check under write lock (TOCTOU guard — another worker may have
    // claimed in the gap between our shared-read and write-acquire).
    auto it = g_callout_until_ms.find(k);
    if (it != g_callout_until_ms.end() && now_ms < it->second)
        return false;
    g_callout_until_ms[k] = now_ms + lockout_ms;
    // Opportunistic GC: when the table grows past ~256 entries (way
    // more than realistic callout-types × nodes), purge expired keys.
    if (g_callout_until_ms.size() > 256)
    {
        for (auto eit = g_callout_until_ms.begin(); eit != g_callout_until_ms.end(); )
        {
            if (eit->second <= now_ms) eit = g_callout_until_ms.erase(eit);
            else                       ++eit;
        }
    }
    return true;
}

void BattlegroundScriptMgr::Register(std::unique_ptr<BattlegroundScript> script)
{
    if (!script) return;
    scripts_.emplace(script->bg_type_id(), std::move(script));
}

// Modern WoW exposes the same BG map under many BattlemasterList IDs:
// Domination / Comp Stomp / Brawl / CTF variants all reuse the original
// map but bind a different DBC entry. Without aliasing, GetScriptFor
// returns nullptr for the variant id and bots get no role/node advice
// — they stand in the start area doing nothing. Map each variant back
// to the base BG we already wrote a script for. IDs sourced from
// `BATTLEGROUND_*` constants in SharedDefines.h.
//
// Only listing variants that map cleanly to a base BG present in our
// 14 scripts (AV=1, WS=2, AB=3, EY=7, SA=9, IC=30, TP=108, BFG=120,
// TK=699, SM=708, DG=754, SS=894, EB_A=1020, plus catch-all 1018-style
// modern variants). Unmapped variants (Arenas / Epic BG modes / brand-
// new BGs) keep returning nullptr and the bots fall back to generic
// arena formation / no-advice behavior as before.
static uint16_t AliasToBaseBg(uint16_t variant_id)
{
    switch (variant_id)
    {
        // Warsong Gulch family
        case 1014:                       // BATTLEGROUND_WG_CTF
        case 861:                        // BATTLEGROUND_BRAWL_WS
        case 886:                        // BATTLEGROUND_BRAWL_WG
            return 2;                    // BATTLEGROUND_WS
        // Arathi Basin family
        case 1018:                       // BATTLEGROUND_DOM_AB
        case 1019:                       // BATTLEGROUND_AB_CS
        case 847:                        // BATTLEGROUND_BRAWL_ABW
        case 880:                        // BATTLEGROUND_BRAWL_AB
        case 1022:                       // BATTLEGROUND_BRAWL_AB2
            return 3;                    // BATTLEGROUND_AB
        // Eye of the Storm family
        case 859:                        // BATTLEGROUND_BRAWL_GL  (Gravity Lapse — low-grav EotS variant)
        case 862:                        // BATTLEGROUND_BRAWL_EH  (Eye of the Horn — Outland EotS reskin)
        case 882:                        // BATTLEGROUND_BRAWL_ES
            return 7;                    // BATTLEGROUND_EY
        // Battle for Gilneas family
        case 846:                        // BATTLEGROUND_BRAWL_TBG (old)
        case 885:                        // BATTLEGROUND_BRAWL_TBG2
            return 120;                  // BATTLEGROUND_BFG
        // Temple of Kotmogu family
        case 858:                        // BATTLEGROUND_BRAWL_TH
        case 884:                        // BATTLEGROUND_BRAWL_TK
            return 699;                  // BATTLEGROUND_TK
        // Silvershard Mines family
        case 883:                        // BATTLEGROUND_BRAWL_SM
            return 708;                  // BATTLEGROUND_SM
        // Deepwind Gorge (754) + Ashran (1020/1021) aliases REMOVED (BG audit
        // dead-code cleanup): their base scripts are no longer registered (no
        // battleground_template row, never queued, never dispatched). An
        // unmapped id simply falls through to GetScriptFor → no-op, same as
        // before. Re-add with the registration if they become real BGs.
        // Seething Shore family
        case 890:                        // BATTLEGROUND_DOM_SS (Seething Strand variant)
            return 894;                  // BATTLEGROUND_SS
        // Alterac Valley family — Korrak's Revenge is AV-classic with extra
        // questables (Black Lotus, Korrak world boss). Map + node layout
        // is the live AV map; AV advice serves it correctly.
        case 1033:                       // BATTLEGROUND_KR (Korrak's Revenge)
            return 1;                    // BATTLEGROUND_AV
        // Warfront Arathi (PvP epic mode) — same map as AB plus extra
        // workshops/mercenaries. Treat as AB until a dedicated script
        // models the vehicle/mercenary mechanics.
        case 1036:                       // BATTLEGROUND_EPIC_BG_WF
            return 3;                    // BATTLEGROUND_AB
        default:
            return 0;                    // no alias
    }
}

BattlegroundScript const* BattlegroundScriptMgr::TryGetScriptFor(uint16_t bg_type_id) const
{
    if (auto it = scripts_.find(bg_type_id); it != scripts_.end())
        return it->second.get();
    if (uint16_t base = AliasToBaseBg(bg_type_id); base != 0)
        if (auto it = scripts_.find(base); it != scripts_.end())
            return it->second.get();
    return nullptr;
}

BattlegroundScript const* BattlegroundScriptMgr::GetScriptFor(uint16_t bg_type_id) const
{
    if (BattlegroundScript const* script = TryGetScriptFor(bg_type_id))
        return script;
    // Surface unmapped variants once per process — if a future client patch
    // introduces a new BG ID we don't handle, bots will fall back to no-advice
    // (idle in start area). The warning makes that visible at the WARN level
    // so a /reload-config or grep over Server.log pinpoints which alias entry
    // to add.
    static std::shared_mutex             warned_mtx;
    static std::unordered_set<uint16_t>  warned;
    {
        std::shared_lock<std::shared_mutex> lk(warned_mtx);
        if (warned.contains(bg_type_id)) return nullptr;
    }
    {
        std::unique_lock<std::shared_mutex> lk(warned_mtx);
        if (warned.insert(bg_type_id).second)
            TC_LOG_WARN("playerbot.v2",
                "[BgScriptMgr] no script registered for bg_type_id={}; bots will "
                "have no role/node advice. Add to AliasToBaseBg() if this is a "
                "variant of an existing BG.", uint32(bg_type_id));
    }
    return nullptr;
}

BattlegroundAdvice BattlegroundScriptMgr::GetAdvice(BotSnapshotView const& s) const
{
    BattlegroundScript const* script = GetScriptFor(s.raw().bg.current_type_id);
    if (!script) return {};
    return script->get_advice(s);
}

} // namespace Playerbot
