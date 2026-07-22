#include "BotSnapshotBuilder.h"
#include "BotRegistry.h"
#include "BotAI.h"
#include "BagSizeTable.h"
#include "ClassTables.h"  // IsTankSpec / IsHealerSpec — canonical role inference
#include "QuestReverseIndex.h"
#include "QuestDoable.h"
#include "RecipeDifficulty.h"   // #4B wanted-reagent buyable-listing scan
#include "StatPriority.h"
#include "../Services.h"
#include "../Util/ConfigReader.h"     // ConfigReader full type — Services::Config() use below
#include "../Fleet/BotGuildMgr.h"
#include "../Fleet/CraftOrderBoard.h"
#include "Battleground/BgTeamCoordinator.h"
#include "Dungeon/PveGroupCoordinator.h"
#include "Dungeon/DungeonScript.h"      // DungeonScriptMgr::GetScriptFor + event_summoned_bosses()
#include "Player.h"
#include "Formulas.h"                 // Trinity::XP::GetGrayLevel (Fix 2 trivial-quest gate)
#include "Corpse.h"
#include "Pet.h"
#include "Unit.h"
#include "Group.h"
#include "Item.h"
#include "Bag.h"
#include "ItemTemplate.h"
#include "QuestDef.h"
#include "Mail.h"
#include "GameTime.h"
#include "Log.h"
#include "Loot.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellHistory.h"
#include "Map.h"
#include "InstanceScript.h"
#include "PhaseShift.h"
#include "InstanceScenario.h"
#include "DB2Structure.h"   // ScenarioEntry / ScenarioStepEntry
#include "Battleground.h"
#include "CharacterCache.h"   // sCharacterCache for owner_name lookup
#include "Fleet/OwnerRegistry.h"   // owner→character resolution for mat-share mail
#include "Guild.h"           // Guild::Member, GetMember / GetRankId for Phase B
#include "GuildMgr.h"        // sGuildMgr lookup of bot's guild for snapshot fields
#include "GameObject.h"
#include "GameObjectData.h"
#include "BattlegroundPackets.h"
#include "Vehicle.h"
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "DBCEnums.h"
#include "DB2Stores.h"
#include "GossipDef.h"
#include "ReputationMgr.h"
#include "LFGMgr.h"
#include "AuctionHouseMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "TaxiPathGraph.h"
#include "PlayerbotMovement.h"   // BotMovement::NearestNavPoint (water-escape dry footing)
#include "Transport.h"
#include "TransportMgr.h"
#include "Travel/ElevatorIndex.h"
#include "Travel/PortalIndex.h"
#include "Travel/RegionMapper.h"
#include "Travel/QuestHubDatabase.h"
#include "Travel/UnifiedTravelGraph.h"
#include "DatabaseEnv.h"   // WorldDatabase — questitem reverse-index (cave POI Z fix)
#include "RestMgr.h"
#include "SocialMgr.h"
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include <algorithm>
#include <limits>
#include <shared_mutex>
#include <unordered_map>

namespace Playerbot {

namespace {

// Combat-duration tracking (combat_start_ms / combat_exit_ms) was a pair of
// process-wide BotId-keyed maps here. It is now per-bot state on BotAI (see
// BotAI::combat_start_ms / combat_exit_ms) so the parallel-by-Map* Build does
// not concurrently mutate one shared map. The semantics are unchanged: when
// IsInCombat flips true with no prior start we record `now`; when it flips
// false we stamp the exit time and clear the start.

// Tier 1.1 (behavior-preserving): thread_local scratch buckets shared between
// the merged enemy+friend grid visit (which partitions) and the two downstream
// processing blocks (which consume). Function-scope thread_local so the
// allocation amortizes across builds exactly like the pre-merge per-block
// scratch vectors did, while remaining reachable from both consumer blocks.
// Safe single-threaded today (builder runs on the world thread); thread_local
// also keeps it correct if Tier 4 ever parallelizes Build per Map*.
inline std::vector<Unit*>& scratch_enemies_tl()
{
    thread_local std::vector<Unit*> v;
    return v;
}
inline std::vector<Unit*>& scratch_friends_tl()
{
    thread_local std::vector<Unit*> v;
    return v;
}

// True when at least one of the quest's objectives is structurally
// impossible for a bot to complete. We check two independent signals:
//
//   1. Explicit pet-battle objective types — types 11, 12, 13 require
//      the player to win a pet battle, which the pet-battle system
//      drives entirely from the client UI. Bots have no client and no
//      battle-pet roster, so progress on these is permanently 0.
//
//   2. MONSTER (0) / TALKTO (3) objectives whose ObjectID points at a
//      CreatureTemplate with type==NOT_SPECIFIED. That CreatureType is
//      TC's marker for internal trigger / kill-credit creatures — units
//      that are never spawned as combat targets. The kill is awarded by
//      a server script (`KillCreditMe`, `RewardPlayerAndGroupAtKill`)
//      attached to some other game event: SpellOnAccept on the quest's
//      OnAccept spell, an area-trigger script, an instance encounter
//      script, etc. None of those side-channels are reachable by the
//      bot's normal "walk to mob, attack" loop, so the credit never
//      triggers. The CreatureType signal is what distinguishes these
//      from legitimate quest mobs spawned dynamically by SmartScripts
//      (those have real types: Beast / Humanoid / etc).
//
// We deliberately do NOT use spawn-count as a signal: phased and
// SmartScript-driven spawns have zero rows in `creature` but are still
// killable when the right trigger fires.
// Process-wide memo for QuestHasObjectiveBotCannotComplete. The answer is
// immutable per quest_id — quest templates, creature templates, and the
// KillCredit reverse-index are all loaded once at server start and never
// mutate at runtime — yet the function re-walked every objective + a
// GetCreatureTemplate + KillCreditAliasesFor lookup per snapshot for every
// quest in every bot's log. Mirror the g_tool_effects_cache pattern: a
// shared_lock fast path, exclusive insert on miss. Single-threaded build
// today, but the same guarding style keeps it consistent and parallel-safe.
std::unordered_map<uint32, bool> g_quest_unachievable_cache;
std::shared_mutex                g_quest_unachievable_mtx;

// ---- Junk-quest predicate (R1/R2, 2026-06-03) ----
// A "junk" quest is one a bot can never finish OR turn in, so holding it
// strands the bot: the objective picker finds nothing and parks
// (current_quest_id=0 → [picker_none] forever). Live root cause: quest 55660
// "Time Trials" (QuestSortID=-22, ZERO objectives, NO creature/GO questender)
// was held by 13,641 bots = 56% of all picker_none.
//
// Detection is STRUCTURAL (self-maintaining — auto-covers the whole class and
// any future tainted quest) plus a small EXPLICIT override set for quests that
// are tainted for reasons the heuristic can't see (has an ender but is
// seasonal / phase-locked / scripted in a way bots can't drive). A quest is
// junk when: it is in the explicit blacklist, OR it has zero objectives AND no
// turn-in NPC and no turn-in GameObject at all. We deliberately do NOT flag a
// quest that merely lacks a DB ender row but HAS objectives — many legit
// quests have script-spawned enders, so abandoning those would drop real
// progress. Only the zero-objective + no-ender shape is unambiguously junk.
//
// Operators: add IDs to kBotQuestBlacklist for quests that slip the structural
// check. (The matching SQL purge keys on the same structural condition.)
static constexpr uint32 kBotQuestBlacklist[] = {
    55660,   // "Time Trials" — QuestSortID -22, zero-objective, no ender
};
inline bool IsExplicitlyBlacklistedQuest(uint32 quest_id)
{
    for (uint32 b : kBotQuestBlacklist)
        if (b == quest_id) return true;
    return false;
}
inline bool QuestHasAnyEnder(uint32 quest_id)
{
    auto ce = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(quest_id);
    if (ce.begin() != ce.end()) return true;
    auto ge = sObjectMgr->GetGOQuestInvolvedRelationReverseBounds(quest_id);
    return ge.begin() != ge.end();
}

std::unordered_map<uint32, bool> g_quest_junk_cache;
std::shared_mutex                g_quest_junk_mtx;

bool IsBotJunkQuest(uint32 quest_id)
{
    {
        std::shared_lock<std::shared_mutex> rlock(g_quest_junk_mtx);
        auto it = g_quest_junk_cache.find(quest_id);
        if (it != g_quest_junk_cache.end()) return it->second;
    }
    const bool result = [quest_id]() -> bool
    {
        if (IsExplicitlyBlacklistedQuest(quest_id)) return true;
        Quest const* q = sObjectMgr->GetQuestTemplate(quest_id);
        if (!q) return false;
        // A5: QuestSortIDs whose quests a leveling bot can never progress
        // autonomously and which PARK the objective picker forever (held with
        // current_quest_id!=0 so R7 relocation never fires). -655 Housing:
        // objective/ender resolve into an unenterable housing instance (q92572
        // alone = 10,417 picker_none in the 4-day run). GetZoneOrSort() stores
        // the negated QuestSort.db2 id. Operator-extensible.
        static constexpr int32 kBotJunkQuestSorts[] = { -655 /* Housing */ };
        const int32 sort = q->GetZoneOrSort();
        for (int32 s : kBotJunkQuestSorts) if (sort == s) return true;
        return q->Objectives.empty() && !QuestHasAnyEnder(quest_id);
    }();
    {
        std::unique_lock<std::shared_mutex> wlock(g_quest_junk_mtx);
        g_quest_junk_cache[quest_id] = result;
    }
    return result;
}

::CreatureData const* FirstSpawnByEntry(uint32 entry);   // defined below; used by the P3 spawn check

// Internal implementation (anon = internal linkage). The public
// Playerbot::QuestHasObjectiveBotCannotComplete (declared in QuestReverseIndex.h,
// defined as a thin forwarder at the bottom of this TU) calls this so other TUs
// — notably JunkQuestResolver — can route the impossible-for-bot class to
// force-complete instead of abandon. In-file callers use the public name too
// (resolves to the forwarder; no ambiguity since this impl is uniquely named).
bool QuestHasObjectiveBotCannotCompleteImpl(uint32 quest_id)
{
    // Junk quests (blacklist / zero-objective-no-ender) are also unachievable;
    // fold them in so the offer/accept scans that already call this gate them.
    if (IsBotJunkQuest(quest_id)) return true;
    {
        std::shared_lock<std::shared_mutex> rlock(g_quest_unachievable_mtx);
        auto it = g_quest_unachievable_cache.find(quest_id);
        if (it != g_quest_unachievable_cache.end()) return it->second;
    }
    const bool result = [quest_id]() -> bool
    {
    Quest const* q = sObjectMgr->GetQuestTemplate(quest_id);
    if (!q) return false;
    for (auto const& obj : q->Objectives)
    {
        switch (obj.Type)
        {
            case QUEST_OBJECTIVE_WINPETBATTLEAGAINSTNPC:
            case QUEST_OBJECTIVE_DEFEATBATTLEPET:
            case QUEST_OBJECTIVE_WINPVPPETBATTLES:
                return true;

            case QUEST_OBJECTIVE_MONSTER:
            case QUEST_OBJECTIVE_TALKTO:
                if (obj.ObjectID > 0)
                {
                    CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(uint32(obj.ObjectID));
                    // A kill/talk-credit target is unachievable when NOTHING in
                    // the world lets the bot earn the credit — checked via the
                    // KillCredit reverse-index first: a target is achievable if
                    // some OTHER creature aliases through KillCredit[i] to it
                    // (world bosses with a "shell" credit creature; the alias is
                    // the reachable one to fight).
                    if (ct && KillCreditAliasesFor(uint32(obj.ObjectID)).empty())
                    {
                        // With no alias, the objective is impossible if the target
                        // is EITHER an abstract NOT_SPECIFIED credit marker OR has
                        // ZERO world spawns. P3 (2026-06-16): the zero-spawn case
                        // catches scripted/vehicle credit-PROXY creatures
                        // (faction-35, uninteractible, UNSPAWNED — e.g. DK Ebon
                        // Hold q12779 'An End To All Things' obj 29150, q12680
                        // 'Grand Theft Palomino' obj 28767) that this filter
                        // previously missed because their type is 7/9/10, not
                        // NOT_SPECIFIED — so the picker kept selecting them and the
                        // bot CombatLoop'd at 0 XP forever. A SPAWNED friendly
                        // target is deliberately NOT flagged: it is findable, so
                        // the talk_credit/gossip/proximity handlers may still
                        // complete it (don't blind-abandon a reachable objective).
                        if (ct->type == CREATURE_TYPE_NOT_SPECIFIED ||
                            FirstSpawnByEntry(uint32(obj.ObjectID)) == nullptr)
                            return true;
                    }
                }
                break;

            default:
                break;
        }
    }
    return false;
    }();
    {
        std::unique_lock<std::shared_mutex> wlock(g_quest_unachievable_mtx);
        g_quest_unachievable_cache[quest_id] = result;
    }
    return result;
}

// Item-entry → distilled "quest tool" effect list. Built lazily on first
// snapshot lookup per item_entry and cached forever — item templates and
// their ON_USE spell effects are immutable at runtime, so a populated
// entry never invalidates. Saves ~100-200 SpellMgr probes per snapshot for
// bots with dense quest logs.
struct QuestToolEffect
{
    uint32 spell_id;        // ON_USE spell id (sent to SpellHistory cooldown checks)
    int32  effect_type;     // SPELL_EFFECT_* (KILL_CREDIT family or QUEST_COMPLETE)
    int32  misc_value;      // creature entry / quest_id / label depending on effect
};

// Map value vector is empty for items with no relevant effects; lookup
// still hits cache and returns the empty vector. nullptr = uncached.
std::unordered_map<uint32, std::vector<QuestToolEffect>> g_tool_effects_cache;
std::shared_mutex                                        g_tool_effects_mtx;

// Returns the (cached or newly built) distilled effect list for the
// given item entry. The first call for a new entry populates the cache
// under an exclusive lock; subsequent calls take a shared lock.
std::vector<QuestToolEffect> const& GetCachedToolEffects(ItemTemplate const* tmpl)
{
    static const std::vector<QuestToolEffect> kEmpty;
    if (!tmpl) return kEmpty;
    const uint32 entry = tmpl->GetId();
    {
        std::shared_lock<std::shared_mutex> rlock(g_tool_effects_mtx);
        auto it = g_tool_effects_cache.find(entry);
        if (it != g_tool_effects_cache.end()) return it->second;
    }
    // Build off-lock; insert under exclusive. Two concurrent first-callers
    // may both build, but the insert is idempotent and the cost is cheap.
    std::vector<QuestToolEffect> built;
    for (ItemEffectEntry const* eff : tmpl->Effects)
    {
        if (!eff) continue;
        if (eff->TriggerType != /*ITEM_SPELLTRIGGER_ON_USE*/ 0) continue;
        if (eff->SpellID <= 0) continue;
        SpellInfo const* si = sSpellMgr->GetSpellInfo(uint32(eff->SpellID), DIFFICULTY_NONE);
        if (!si) continue;
        for (auto const& spe : si->GetEffects())
        {
            const uint32 e = uint32(spe.Effect);
            // SPELL_EFFECT_CREATE_ITEM / _CREATE_ITEM_2 / _CREATE_LOOT cover
            // the "use the empty Crystal Phial at the pool → receive the
            // Filled Crystal Phial" pattern. Quest 28729 Teldrassil: Crown
            // of Azeroth has objective ITEM 5184; bot starts with empty
            // phial 5185 whose ON_USE spell creates 5184. Without this
            // match the tool discovery skipped the item and the bot stood
            // at the pool indefinitely.
            //
            // Misc-value mapping: for CREATE_ITEM the spawned item entry
            // lives in spe.ItemType (NOT spe.MiscValue, which is unused for
            // this effect family). Surface it as the misc_value in our
            // distilled record so the downstream matcher can compare
            // against obj.object_id uniformly.
            if (e == SPELL_EFFECT_CREATE_ITEM      ||
                e == SPELL_EFFECT_CREATE_LOOT      ||
                e == SPELL_EFFECT_CREATE_RANDOM_ITEM)
            {
                built.push_back({ uint32(eff->SpellID), int32(spe.Effect), int32(spe.ItemType) });
                continue;
            }
            const bool relevant =
                e == SPELL_EFFECT_QUEST_COMPLETE      ||
                e == SPELL_EFFECT_KILL_CREDIT         ||
                e == SPELL_EFFECT_KILL_CREDIT2        ||
                e == SPELL_EFFECT_KILL_CREDIT_LABEL_1 ||
                e == SPELL_EFFECT_KILL_CREDIT_LABEL_2;
            if (!relevant) continue;
            built.push_back({ uint32(eff->SpellID), int32(spe.Effect), int32(spe.MiscValue) });
        }
    }
    std::unique_lock<std::shared_mutex> wlock(g_tool_effects_mtx);
    auto [it, inserted] = g_tool_effects_cache.try_emplace(entry, std::move(built));
    return it->second;
}

// Lazy entry→first-spawn index. Walks GetAllCreatureData once and maps each
// creature template entry to A spawned CreatureData. Used by quest-ender
// resolution; CreatureData is loaded once at startup and immutable at runtime.
::CreatureData const* FirstSpawnByEntry(uint32 entry)
{
    static std::once_flag s_once;
    static std::unordered_map<uint32, ::CreatureData const*> s_index;
    std::call_once(s_once, []() {
        auto const& all = sObjectMgr->GetAllCreatureData();
        s_index.reserve(all.size());
        for (auto const& [spawn_id, cd] : all)
            s_index.emplace(cd.id, &cd);   // SpawnData::id == template entry; first wins
    });
    auto it = s_index.find(entry);
    return it == s_index.end() ? nullptr : it->second;
}

// ---- Tier 1.2 (behavior-preserving): immutable per-quest cache ----
// The quest-log build re-derived, for every quest in every bot's log, every
// tick: the flags bitfield, source-item id, the full per-objective static
// shape (id/type/storage/object_id/amount/flags + the resolved KillCredit
// alias list / KILL_WITH_LABEL target list / talk_credit faction probe), and
// the quest-ender spawn coordinates. ALL of those are functions of quest_id
// alone — quest templates, creature templates, faction templates, the
// KillCredit/label reverse-indexes, and creature spawn data are loaded once at
// startup and never mutate at runtime. Only the quest STATE (complete flag),
// the per-objective PROGRESS, and the bot-level-scaled level/min_level/xp are
// volatile per build.
//
// We cache the immutable shape keyed by quest_id (shared across all bots —
// the data is identical for everyone) and, per build, copy it and overlay the
// volatile fields. The produced QuestEntry is byte-for-byte identical to what
// the old inline build produced, so the published snapshot is unchanged.
//
// Process-wide map guarded shared/exclusive like g_quest_unachievable_cache /
// g_tool_effects_cache. Single-threaded build today; this keeps it correct if
// Build is ever parallelized.
//
// KEY = (quest_id, bot faction-template id). The only viewer-dependent field
// is QuestObjectiveEntry::talk_credit, which probes
// tft->IsFriendlyTo(p->GetFactionTemplateEntry()): for a faction-neutral quest
// shared by both teams the SAME quest_id can yield different talk_credit for an
// Alliance vs a Horde bot. Folding the bot's faction-template id into the key
// keeps every cached entry byte-identical to what the old inline per-build
// probe produced for that specific bot, while still collapsing the heavy
// recompute across the (many) bots that share a (quest_id, faction) pair.
// Everything else in the struct is genuinely quest_id-only immutable.
struct QuestImmutable
{
    uint8  flags = 0;
    uint32 source_item_id = 0;
    bool   unachievable = false;
    bool   unturnable = false;
    bool   ender_resolved = false;
    uint32 ender_map_id = 0;
    float  ender_x = 0.f, ender_y = 0.f, ender_z = 0.f;
    // ALL ender spawns for the quest (every quest-involved NPC that has a spawn).
    // Position-independent (so it stays in the (quest_id,faction) immutable cache);
    // the per-bot builder picks the NEAREST same-map one. A multi-ender quest
    // otherwise routed to the DB-FIRST ender regardless of distance — live: Durnan
    // sent to Brock (1681, 2048y, across a barriered route) instead of Gremlock
    // (1699, 983y, a clean advancing route — the quest is literally "Return to
    // Gremlock").
    struct EnderSpawn { uint32 map_id = 0; float x = 0.f, y = 0.f, z = 0.f; };
    std::vector<EnderSpawn> ender_spawns;
    bool   has_template = false;   // GetQuestTemplate(qid) != nullptr
    // Immutable objective scaffolding; per-build copy fills `.progress`.
    std::vector<QuestObjectiveEntry> objectives;
};

// Key: (uint64(quest_id) << 32) | faction_template_id. See struct note.
std::unordered_map<uint64, QuestImmutable> g_quest_immutable_cache;
std::shared_mutex                          g_quest_immutable_mtx;

// Builds the immutable part for one quest. Pure function of quest_id +
// (for talk_credit only) the bot's faction template, which is faction- not
// bot-identity-dependent — see note above.
QuestImmutable BuildQuestImmutable(uint32 qid, Player const* p)
{
    QuestImmutable im{};
    Quest const* qt = sObjectMgr->GetQuestTemplate(qid);
    im.has_template = (qt != nullptr);
    if (qt)
    {
        uint8 fl = 0;
        if (qt->IsRepeatable())  fl |= 0x01;
        if (qt->IsDaily())       fl |= 0x02;
        if (qt->IsWeekly())      fl |= 0x04;
        const uint32 qtype = qt->GetQuestType();
        if (qtype == 81)         fl |= 0x08;   // dungeon
        if (qtype == 88)         fl |= 0x10;   // raid
        if (qtype == 41)         fl |= 0x20;   // group
        im.flags = fl;
        im.source_item_id = qt->GetSrcItemId();

        im.objectives.reserve(qt->Objectives.size());
        for (auto const& obj : qt->Objectives)
        {
            if (obj.Flags & QUEST_OBJECTIVE_FLAG_HIDDEN) continue;
            if (obj.Flags & QUEST_OBJECTIVE_FLAG_PART_OF_PROGRESS_BAR) continue;
            QuestObjectiveEntry oe{};
            oe.id            = obj.ID;
            oe.quest_id      = qid;
            oe.type          = obj.Type;
            oe.storage_index = obj.StorageIndex;
            oe.object_id     = obj.ObjectID;
            oe.amount        = obj.Amount;
            oe.flags         = obj.Flags;
            oe.progress      = 0;   // volatile — filled per build
            if (oe.type == QUEST_OBJECTIVE_MONSTER && oe.object_id > 0)
            {
                auto const& aliases = KillCreditAliasesFor(uint32(oe.object_id));
                oe.credit_alias_entries.assign(aliases.begin(), aliases.end());
                if (CreatureTemplate const* tct =
                        sObjectMgr->GetCreatureTemplate(uint32(oe.object_id)))
                {
                    if (FactionTemplateEntry const* tft =
                            sFactionTemplateStore.LookupEntry(tct->faction))
                    {
                        if (FactionTemplateEntry const* pft = p->GetFactionTemplateEntry())
                            oe.talk_credit = tft->IsFriendlyTo(pft) &&
                                             !tft->IsHostileTo(pft);
                    }
                }
            }
            if (oe.type == QUEST_OBJECTIVE_KILL_WITH_LABEL && oe.object_id != 0)
            {
                auto const& labeled = CreaturesWithLabel(oe.object_id);
                oe.labeled_target_entries.assign(labeled.begin(), labeled.end());
            }
            im.objectives.push_back(std::move(oe));
        }
    }
    im.unachievable = QuestHasObjectiveBotCannotComplete(qid);
    im.unturnable   = IsBotJunkQuest(qid);

    auto involved = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(qid);
    for (auto it = involved.begin(); it != involved.end(); ++it)
    {
        ::CreatureData const* cd = FirstSpawnByEntry(it->second);
        if (!cd) continue;
        // Collect EVERY ender spawn so the builder can pick the nearest; keep the
        // first as the legacy single-ender fallback (ender_resolved / ender_x).
        im.ender_spawns.push_back({ cd->mapId, cd->spawnPoint.GetPositionX(),
                                    cd->spawnPoint.GetPositionY(), cd->spawnPoint.GetPositionZ() });
        if (!im.ender_resolved)
        {
            im.ender_resolved = true;
            im.ender_map_id   = cd->mapId;
            im.ender_x        = cd->spawnPoint.GetPositionX();
            im.ender_y        = cd->spawnPoint.GetPositionY();
            im.ender_z        = cd->spawnPoint.GetPositionZ();
        }
    }
    return im;
}

// Returns the cached immutable part for a quest, building+inserting on miss.
// Keyed by (quest_id, bot faction-template id) so the viewer-dependent
// talk_credit field stays correct for cross-faction shared quests.
QuestImmutable const& GetQuestImmutable(uint32 qid, Player const* p)
{
    FactionTemplateEntry const* pft = p->GetFactionTemplateEntry();
    const uint32 fid = pft ? pft->ID : 0u;
    const uint64 key = (uint64(qid) << 32) | uint64(fid);
    {
        std::shared_lock<std::shared_mutex> rlock(g_quest_immutable_mtx);
        auto it = g_quest_immutable_cache.find(key);
        if (it != g_quest_immutable_cache.end()) return it->second;
    }
    QuestImmutable built = BuildQuestImmutable(qid, p);
    std::unique_lock<std::shared_mutex> wlock(g_quest_immutable_mtx);
    auto [it, inserted] = g_quest_immutable_cache.try_emplace(key, std::move(built));
    return it->second;
}

// #4B-1(b): per-unit FAIR-VALUE CEILING for a buyable reagent. The buy-side
// rule (idle:ah_buy_reagents) refuses any listing/commodity whose per-unit
// price exceeds this, so a human can't price-pump a reagent and drain bots.
//
//   ceiling = max(vendor SellPrice * vendor_multiple, quality-based flat ceiling)
//
// Vendor SellPrice (ItemTemplate::GetSellPrice) is non-zero for most trade
// goods and anchors the ceiling to real intrinsic value. When SellPrice == 0
// (some quest/special reagents, or items the data omits) we fall back to a
// quality-based flat ceiling — the same quality bands AuctionRules.cpp uses
// for TradeGoodUnitFloor, scaled UP (this is a CEILING, not a floor, so it
// must sit well above the floor a legit listing would price at). Both arms
// take a max() so a vendor multiple that comes out below the flat band still
// allows a sanely-priced cheap reagent through.
uint64 ReagentFairValueCeiling(ItemTemplate const* tmpl, uint32 vendor_multiple)
{
    const uint8 quality = tmpl ? static_cast<uint8>(tmpl->GetQuality()) : 1u;

    // Quality-based flat ceiling — mirrors TradeGoodUnitFloor's bands
    // (AuctionRules.cpp) scaled up ~10x so the flat fallback is a generous
    // CEILING rather than a floor.
    uint64 flat;
    switch (quality)
    {
        case 0:  flat = 1000;   break;   //  10s   (floor    1s) grey trash mats
        case 1:  flat = 2500;   break;   //  25s   (floor  2s50) common mats
        case 2:  flat = 10000;  break;   //   1g   (floor   10s) uncommon reagents
        case 3:  flat = 50000;  break;   //   5g   (floor   50s) rare reagents/gems
        case 4:  flat = 250000; break;   //  25g   (floor 2g50) epic-tier mats
        default: flat = 10000;  break;   //   1g
    }

    uint64 ceiling = flat;
    if (tmpl)
    {
        const uint64 sell = static_cast<uint64>(tmpl->GetSellPrice());
        if (sell != 0)
        {
            const uint64 vendor_based = sell * uint64(vendor_multiple);
            if (vendor_based > ceiling) ceiling = vendor_based;
        }
    }
    return ceiling;
}

// #4B-2(a) part 2: reverse index product_item_entry -> producing recipe spell.
// The post rule needs to ask "is this reagent I'm short on itself the PRODUCT of
// a recipe (so another bot could craft it for me)?". Built once, lazily, from
// every profession recipe in SkillLineAbility (the same authoritative recipe set
// RecipeDifficulty indexes), reading each recipe spell's CREATE_ITEM effect to
// learn the item it produces. We only index SAFE create-item recipes:
// SPELL_EFFECT_CREATE_ITEM with a concrete ItemType, exactly the shape
// PlayerbotAPI::craft_fulfill_order can fulfil. CREATE_ITEM_2 (Conjure-style),
// CREATE_LOOT and CREATE_RANDOM_ITEM produce non-deterministic / loot-table
// output and are deliberately EXCLUDED so a posted order is always a concrete,
// fulfillable single-item craft. When several recipes make the same item we keep
// the one with the LOWEST spell id (deterministic; the canonical recipe is
// usually the lower id). Returns 0 when no safe recipe produces the entry.
uint32 ProducingRecipeSpellFor(uint32 item_entry)
{
    if (item_entry == 0) return 0;
    static std::unordered_map<uint32, uint32> s_product_to_spell;   // entry -> spell
    static std::once_flag s_once;
    std::call_once(s_once, []()
    {
        for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
        {
            SkillLineAbilityEntry const* row = sSkillLineAbilityStore.LookupEntry(i);
            if (!row || row->Spell <= 0) continue;
            const uint32 spell_id = uint32(row->Spell);
            SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
            if (!si) continue;
            for (SpellEffectInfo const& eff : si->GetEffects())
            {
                if (eff.Effect != SPELL_EFFECT_CREATE_ITEM) continue;
                const uint32 produced = uint32(eff.ItemType);
                if (produced == 0) continue;
                auto it = s_product_to_spell.find(produced);
                if (it == s_product_to_spell.end() || spell_id < it->second)
                    s_product_to_spell[produced] = spell_id;
            }
        }
    });
    auto it = s_product_to_spell.find(item_entry);
    return it == s_product_to_spell.end() ? 0 : it->second;
}

// Populate an ItemStatBlock from a live Item* for the given Player. Walks
// the item's stat-mod columns and the random-suffix bonuses, mapped onto
// our compact StatIndex via StatIndexForItemMod. Weapon DPS is derived
// from min/max/delay (delay in ms; we scale x10 for compact storage).
// Bonding + soulbound flag drive vendor-sell decisions in Phase 4.
void PopulateItemStatBlock(Player const* p, Item const* item, ItemStatBlock& out)
{
    if (!p || !item) return;
    ItemTemplate const* tmpl = item->GetTemplate();
    if (!tmpl) return;
    // Item::GetItemStatType(i) returns the *resolved* ItemModType (handles
    // random suffix overrides via BonusData). GetItemStatValue(i, owner)
    // resolves base + random-suffix + bonus-list values. Walk all 10
    // slots; non-stat ones return -1 type or 0 value.
    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 const type  = item->GetItemStatType(i);
        if (type < 0) continue;
        // Item::GetItemStatValue handles random suffix + bonus list. Pass
        // the owner so heirloom scaling and other player-aware bonuses
        // resolve correctly (no-op for plain items).
        float const fval  = item->GetItemStatValue(i, p);
        if (fval == 0.0f) continue;
        StatIndex const si = StatIndexForItemMod(uint32(type));
        if (si == StatIndex::Count) continue;       // unweighted (resistance / profession / etc)
        const int16 v = static_cast<int16>(std::clamp<float>(fval, -32768.f, 32767.f));
        out.stats[static_cast<size_t>(si)] = static_cast<int16>(out.stats[static_cast<size_t>(si)] + v);
    }
    // Weapons: ItemTemplate::GetDPS(ilvl) computes damage-per-second
    // already baked from ilvl scaling. Use the live item's effective
    // ilvl so heirlooms / scaling weapons report correctly. Stored x10
    // in compact uint16 (caps DPS at 6553.5 — well above any TWW weapon).
    if (tmpl->GetClass() == ITEM_CLASS_WEAPON)
    {
        const uint32 effective_ilvl = item->GetItemLevel(p);
        const float dps = tmpl->GetDPS(effective_ilvl);
        if (dps > 0.f)
            out.weapon_dps_x10 = static_cast<uint16>(std::clamp<float>(dps * 10.f, 0.f, 65535.f));
    }
    // Use the live item's resolved bonding (handles BonusData overrides
    // for items where bonus lists shift binding ON_ACQUIRE → ON_EQUIP etc).
    out.bonding      = static_cast<uint8>(item->GetBonding());
    out.is_soulbound = item->IsSoulBound();
}

// Map TrinityCore's DISPEL_* (uint32, SharedDefines.h) onto our compact
// DispelType (BotTypes.h). Anything we don't model collapses to None.
// Note: bleeds are NOT a dispel category in DBC — they're a mechanic flag
// (MECHANIC_BLEED = 15). The aura-copy path overrides None → Bleed when it
// detects a bleed mechanic, so Stoneform / Trinkets / Mass Dispel callers
// can find them via DispelType::Bleed.
DispelType TranslateDispelType(uint32 core)
{
    switch (core)
    {
        case 1:  return DispelType::Magic;    // DISPEL_MAGIC
        case 2:  return DispelType::Curse;    // DISPEL_CURSE
        case 3:  return DispelType::Disease;  // DISPEL_DISEASE
        case 4:  return DispelType::Poison;   // DISPEL_POISON
        case 9:  return DispelType::Enrage;   // DISPEL_ENRAGE
        default: return DispelType::None;
    }
}

// Append the auras a unit currently has into `out`. We capture spell id,
// stacks, remaining duration, caster, dispel type, harmful flag, and
// whether the aura is stealable. Permanent auras (Beacon of Light, Devotion
// Aura, talented passives) report remaining = INT32_MAX so refresh predicates
// (`a->remaining.count() <= N`) don't trigger phantom re-casts every tick;
// presence-only checks (`a != nullptr`) still work the same.
void CopyAuras(Unit const* u, std::vector<AuraEntry>& out)
{
    if (!u) return;
    auto const& applied = u->GetAppliedAuras();
    out.reserve(out.size() + applied.size());
    for (auto const& [id, app] : applied)
    {
        if (!app) continue;
        Aura const* base = app->GetBase();
        if (!base) continue;
        SpellInfo const* si = base->GetSpellInfo();
        if (!si) continue;

        AuraEntry e{};
        e.spell_id     = si->Id;
        e.stacks       = base->GetStackAmount();
        const int32 dur = base->GetDuration();
        e.remaining    = base->IsPermanent() ? Ms{std::numeric_limits<int32>::max()}
                       : (dur > 0 ? Ms{dur} : Ms{0});
        e.caster       = base->GetCasterGUID();
        e.dispel_type  = TranslateDispelType(si->Dispel);
        e.mechanic     = si->Mechanic;
        e.is_harmful   = !app->IsPositive();
        // Bleeds aren't tagged via Dispel in DBC — promote None → Bleed when
        // the spell carries the Bleed mechanic (MECHANIC_BLEED = 15). This
        // lets dispel-by-mechanic callers (Stoneform, Bestial Wrath bleed
        // immunity check, etc.) find them via DispelType::Bleed.
        if (e.dispel_type == DispelType::None && e.is_harmful
            && si->Mechanic == MECHANIC_BLEED)
            e.dispel_type = DispelType::Bleed;
        // Spellsteal only works on positive magic auras.
        e.is_stealable = app->IsPositive() && e.dispel_type == DispelType::Magic;
        out.push_back(e);
    }
}

// (spell_id, difficulty) → ChargeCategoryId. Filled lazily on first
// encounter and reused across snapshots — ChargeCategoryId is immutable
// per (spell, difficulty) since it lives in the SpellInfo DBC row. Lets
// CopyCooldowns skip GetSpellInfo for the vast majority of spellbook
// entries that aren't on cooldown AND aren't charge-based. At 2000 bots
// × ~300 spellbook entries per snapshot the saved SpellMgr probes add up.
// uint16 cat = 0xFFFF sentinel = "uncached"; 0 = no category; otherwise
// the actual ChargeCategoryId.
using ChargeKey = uint64;            // (uint64(spell_id) << 8) | difficulty
constexpr uint16 kChargeCatUncached = 0xFFFF;
// Sentinel: GetSpellInfo returned NULL for this (spell, difficulty) — a
// "ghost" spell the character knows (stale character_spell / spellbook row)
// whose data no longer exists after a hotfix-DB change. Cached so the probe
// happens once, and CRITICALLY so CopyCooldowns can skip the entry instead
// of feeding it to SpellHistory::HasCooldown(uint32), whose modern
// implementation ASSERTS SpellInfo — crashed the world thread on 2026-06-11
// after the wc_hotfixes swap removed spells bots still "know".
constexpr uint16 kChargeCatNoSpell  = 0xFFFE;
std::unordered_map<ChargeKey, uint16> g_charge_category_cache;
std::shared_mutex                     g_charge_category_mtx;

uint16 LookupOrCacheChargeCategory(uint32 spell_id, Difficulty diff)
{
    const ChargeKey key = (ChargeKey(spell_id) << 8) | uint64(diff);
    {
        std::shared_lock<std::shared_mutex> rlock(g_charge_category_mtx);
        auto it = g_charge_category_cache.find(key);
        if (it != g_charge_category_cache.end()) return it->second;
    }
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, diff);
    const uint16 cat = si ? static_cast<uint16>(si->ChargeCategoryId & 0xFFFFu)
                          : kChargeCatNoSpell;
    {
        std::unique_lock<std::shared_mutex> wlock(g_charge_category_mtx);
        g_charge_category_cache.emplace(key, cat);
    }
    return cat;
}

// Per-spell cooldown rows for the few hundred spells the player knows.
// We only emit entries with non-zero remaining time — `is_ready` treats
// "no row" as ready, which is what we want.
void CopyCooldowns(Player const* p, std::vector<CooldownEntry>& out)
{
    if (!p) return;
    SpellHistory const* hist = p->GetSpellHistory();
    if (!hist) return;
    Map* map = p->GetMap();
    if (!map) return;     // mid-teleport: m_currMap null but IsInWorld may still be true
    const Difficulty diff = map->GetDifficultyID();
    for (auto const& [id, _] : p->GetSpellMap())
    {
        // Existence + charge-category from the per-(spell, difficulty)
        // cache (one SpellMgr probe ever). GHOST spells — known by the
        // character but without SpellInfo after a hotfix-DB change — are
        // skipped HERE: the old code's first call was HasCooldown(uint32),
        // which (despite its historical no-probe reputation) now resolves
        // via SpellMgr::AssertSpellInfo and ABORTED the server (crash
        // 2026-06-11 08:13, wc_hotfixes swap).
        const uint16 cat = LookupOrCacheChargeCategory(id, diff);
        if (cat == kChargeCatNoSpell) continue;
        SpellInfo const* si = sSpellMgr->GetSpellInfo(id, diff);
        if (!si) continue;   // hotfix reload race — treat as ghost this pass
        const bool on_cd = hist->HasCooldown(si);
        if (!on_cd && cat == 0) continue;
        CooldownEntry e{};
        e.spell_id  = id;
        e.remaining = on_cd ? hist->GetRemainingCooldown(si) : Ms{0};
        if (cat != 0)
        {
            // Charge-based spell — without these, is_ready() falls through to
            // the remaining-time check and silently blocks Aimed Shot / Fel
            // Rush / Phoenix Flames whenever any charge is on CD, even with
            // other charges available.
            e.max_charges = static_cast<uint8>(hist->GetMaxCharges(cat));
            e.charges     = hist->HasCharge(cat) ? uint8{1} : uint8{0};
            // DEPLETED charge spell: HasCooldown() never consults
            // _categoryCharges, so on_cd=false left remaining at 0 — and
            // is_ready() then reported a 0-charge spell as castable. The APL
            // emitted it every 1.5s lockout window and the server rejected
            // NOT_READY each time, ~8 rejects per recharge cycle, fleet-wide
            // (audit B01: Fire Blast 319836 alone = 1,559 rejects = 53% of
            // all NOT_READY spam). Surface the real recharge timer so
            // is_ready() blocks until a charge is actually back.
            if (e.charges == 0)
                e.remaining = std::max(e.remaining,
                    Ms{ hist->GetChargeRecoveryTime(cat) });
        }
        out.push_back(e);
    }
}

// Q-P1a: first-spawn-by-entry caches for quest objective navigation fallback.
// TC indexes spawn data by spawnId, not by template entry, so there is no
// built-in entry->position lookup. We build a lazy one-shot index of the
// first observed spawn for each creature/GO template entry. ~200K templates →
// a few MB, built once under once_flag so concurrent world-thread builders
// don't race. Used when a quest objective has no usable QuestPOI: we walk the
// bot toward a known spawn of the target creature/GO so it can actually pursue
// the objective instead of scanning forever for a target that is in another
// zone (root cause of L5 "Astianon" frozen in Orgrimmar hunting Lazy Peons
// that spawn ~2700y away in Durotar).
::CreatureData const* FirstCreatureSpawnByEntry(uint32 entry)
{
    static std::once_flag s_once;
    static std::unordered_map<uint32, ::CreatureData const*> s_index;
    std::call_once(s_once, []() {
        auto const& all = sObjectMgr->GetAllCreatureData();
        s_index.reserve(all.size());
        for (auto const& [spawn_id, cd] : all)
            s_index.emplace(cd.id, &cd);     // first spawn per entry wins
    });
    auto it = s_index.find(entry);
    return it == s_index.end() ? nullptr : it->second;
}

::GameObjectData const* FirstGameObjectSpawnByEntry(uint32 entry)
{
    static std::once_flag s_once;
    static std::unordered_map<uint32, ::GameObjectData const*> s_index;
    std::call_once(s_once, []() {
        auto const& all = sObjectMgr->GetAllGameObjectData();
        s_index.reserve(all.size());
        for (auto const& [spawn_id, gd] : all)
            s_index.emplace(gd.id, &gd);
    });
    auto it = s_index.find(entry);
    return it == s_index.end() ? nullptr : it->second;
}

// Nearest-spawn variants (CombatLoop FIX C). The First* helpers above return a
// DB-arbitrary "first spawn per entry", which for an entry with many spawns
// across the world (e.g. a common quest mob with packs in two zones) often
// hands back a spawn FAR from the bot — so a scan-MISS POI fallback sends the
// bot to the WRONG cluster. These return the CLOSEST spawn on the bot's CURRENT
// map (planar distance), falling back to the first-any spawn when the entry has
// no spawn on this map. Built once into an entry -> [spawns] multi-index; the
// First* indices are kept for callers that don't have a bot position.
::CreatureData const* NearestCreatureSpawnByEntry(uint32 entry, float x, float y, uint32 map_id)
{
    static std::once_flag s_once;
    static std::unordered_map<uint32, std::vector<::CreatureData const*>> s_index;
    std::call_once(s_once, []() {
        auto const& all = sObjectMgr->GetAllCreatureData();
        for (auto const& [spawn_id, cd] : all)
            s_index[cd.id].push_back(&cd);
    });
    auto it = s_index.find(entry);
    if (it == s_index.end() || it->second.empty())
        return nullptr;
    ::CreatureData const* best = nullptr;
    float bestSq = std::numeric_limits<float>::max();
    for (::CreatureData const* cd : it->second)
    {
        if (cd->mapId != map_id) continue;
        const float dx = cd->spawnPoint.GetPositionX() - x;
        const float dy = cd->spawnPoint.GetPositionY() - y;
        const float dsq = dx * dx + dy * dy;
        if (dsq < bestSq) { bestSq = dsq; best = cd; }
    }
    // No same-map spawn — fall back to the first-any spawn so the bot still
    // gets a (cross-map) waypoint that the travel pipeline can route to.
    return best ? best : it->second.front();
}

// Nearest creature spawn to an arbitrary POINT (any entry, same map) within maxDist
// (planar). Used to anchor a use-item objective's POI on the actual TARGET FLOOR:
// the 2D QuestPOI centroid lands on the ground floor, but the target can stand
// UPSTAIRS (Q26118: Ambassador Slaghammer at z511 vs ground z502). The nearest
// spawn to the centroid sits on the target's floor (verified: a critter at z511,
// ~5y from Slaghammer), so anchoring the POI there routes the bot UP the (navmesh-
// verified) stairs. Lazily builds a cell-keyed spatial index ONCE (100y cells) —
// the same one-time-cost pattern as the entry indexes above.
::CreatureData const* NearestCreatureSpawnToPoint(float x, float y, uint32 map_id, float maxDist)
{
    constexpr float kCell = 100.0f;
    auto cellKey = [](uint32 m, int cx, int cy) -> int64
    { return (int64(m) << 44) ^ (int64(cx & 0x3FFFFF) << 22) ^ int64(cy & 0x3FFFFF); };
    static std::once_flag s_once;
    static std::unordered_map<int64, std::vector<::CreatureData const*>> s_cells;
    std::call_once(s_once, [&]() {
        auto const& all = sObjectMgr->GetAllCreatureData();
        for (auto const& [spawn_id, cd] : all)
        {
            const int cx = int(std::floor(cd.spawnPoint.GetPositionX() / kCell));
            const int cy = int(std::floor(cd.spawnPoint.GetPositionY() / kCell));
            s_cells[cellKey(cd.mapId, cx, cy)].push_back(&cd);
        }
    });
    const int pcx = int(std::floor(x / kCell));
    const int pcy = int(std::floor(y / kCell));
    ::CreatureData const* best = nullptr;
    float bestSq = maxDist * maxDist;
    for (int dcx = -1; dcx <= 1; ++dcx)
        for (int dcy = -1; dcy <= 1; ++dcy)
        {
            auto it = s_cells.find(cellKey(map_id, pcx + dcx, pcy + dcy));
            if (it == s_cells.end()) continue;
            for (::CreatureData const* cd : it->second)
            {
                if (cd->mapId != map_id) continue;
                const float ex = cd->spawnPoint.GetPositionX() - x;
                const float ey = cd->spawnPoint.GetPositionY() - y;
                const float dsq = ex * ex + ey * ey;
                if (dsq < bestSq) { bestSq = dsq; best = cd; }
            }
        }
    return best;
}

::GameObjectData const* NearestGameObjectSpawnByEntry(uint32 entry, float x, float y, uint32 map_id)
{
    static std::once_flag s_once;
    static std::unordered_map<uint32, std::vector<::GameObjectData const*>> s_index;
    std::call_once(s_once, []() {
        auto const& all = sObjectMgr->GetAllGameObjectData();
        for (auto const& [spawn_id, gd] : all)
            s_index[gd.id].push_back(&gd);
    });
    auto it = s_index.find(entry);
    if (it == s_index.end() || it->second.empty())
        return nullptr;
    ::GameObjectData const* best = nullptr;
    float bestSq = std::numeric_limits<float>::max();
    for (::GameObjectData const* gd : it->second)
    {
        if (gd->mapId != map_id) continue;
        const float dx = gd->spawnPoint.GetPositionX() - x;
        const float dy = gd->spawnPoint.GetPositionY() - y;
        const float dsq = dx * dx + dy * dy;
        if (dsq < bestSq) { bestSq = dsq; best = gd; }
    }
    return best ? best : it->second.front();
}

// Reverse questitem index: a quest ITEM objective stores the item entry, but
// the bot needs to reach the GAMEOBJECT (chest/herb) or CREATURE that grants
// it. quest_poi for such objectives carries the SURFACE Z over a cave (the
// Ban'ethil relic chests sit at den-floor z~1255 but the POI projects to the
// hilltop z~1462, so the bot climbs the hill instead of entering the den).
// gameobject_questitem / creature_questitem give the exact granting entity →
// its real spawn position (correct Z). Built once via DB (small tables).
::GameObjectData const* QuestItemGoSpawn(uint32 item_entry)
{
    static std::once_flag s_once;
    static std::unordered_map<uint32, uint32> s_item2go;   // item -> GO entry
    std::call_once(s_once, []() {
        if (QueryResult r = WorldDatabase.Query("SELECT ItemId, GameObjectEntry FROM gameobject_questitem"))
            do { Field* f = r->Fetch(); s_item2go.emplace(f[0].GetUInt32(), f[1].GetUInt32()); } while (r->NextRow());
    });
    auto it = s_item2go.find(item_entry);
    return it == s_item2go.end() ? nullptr : FirstGameObjectSpawnByEntry(it->second);
}
::CreatureData const* QuestItemCreatureSpawn(uint32 item_entry)
{
    static std::once_flag s_once;
    static std::unordered_map<uint32, uint32> s_item2cre;  // item -> creature entry
    std::call_once(s_once, []() {
        if (QueryResult r = WorldDatabase.Query("SELECT ItemId, CreatureEntry FROM creature_questitem"))
            do { Field* f = r->Fetch(); s_item2cre.emplace(f[0].GetUInt32(), f[1].GetUInt32()); } while (r->NextRow());
    });
    auto it = s_item2cre.find(item_entry);
    return it == s_item2cre.end() ? nullptr : FirstCreatureSpawnByEntry(it->second);
}

// ALL creature entries that grant a quest item (creature_questitem). Unlike
// QuestItemCreatureSpawn above (first-wins, used for POI/Z resolution), this
// returns EVERY granting creature so an item-collect objective can feed the
// kill-scan target set — the bot must be willing to kill ANY of the droppers
// (item 2858 -> Darkhounds 1547/1548/1549 for Q24990 "Darkhound Pounding").
// Built once from the (small) creature_questitem table; deduped per item.
std::vector<uint32> const& QuestItemCreatureEntries(uint32 item_entry)
{
    static std::once_flag s_once;
    static std::unordered_map<uint32, std::vector<uint32>> s_item2creatures;
    static const std::vector<uint32> s_empty;
    std::call_once(s_once, []() {
        if (QueryResult r = WorldDatabase.Query("SELECT ItemId, CreatureEntry FROM creature_questitem"))
            do
            {
                Field* f = r->Fetch();
                const uint32 item = f[0].GetUInt32();
                const uint32 cre  = f[1].GetUInt32();
                if (!cre) continue;
                auto& vec = s_item2creatures[item];
                if (std::find(vec.begin(), vec.end(), cre) == vec.end())
                    vec.push_back(cre);
            } while (r->NextRow());
    });
    auto it = s_item2creatures.find(item_entry);
    return it == s_item2creatures.end() ? s_empty : it->second;
}

// Nearest-spawn quest-item resolvers (CombatLoop FIX C). Same item->entry
// mapping as above, but resolve the granting entity's CLOSEST same-map spawn so
// a scan-MISS POI fallback heads to the nearest chest/mob, not a DB-arbitrary
// one. The item->entry maps are private to the First* functions, so re-resolve
// via the public First* (one map probe) then take the nearest of that entry.
::GameObjectData const* QuestItemGoSpawnNearest(uint32 item_entry, float x, float y, uint32 map_id)
{
    ::GameObjectData const* first = QuestItemGoSpawn(item_entry);
    if (!first) return nullptr;
    if (::GameObjectData const* n = NearestGameObjectSpawnByEntry(first->id, x, y, map_id))
        return n;
    return first;
}
::CreatureData const* QuestItemCreatureSpawnNearest(uint32 item_entry, float x, float y, uint32 map_id)
{
    ::CreatureData const* first = QuestItemCreatureSpawn(item_entry);
    if (!first) return nullptr;
    if (::CreatureData const* n = NearestCreatureSpawnByEntry(first->id, x, y, map_id))
        return n;
    return first;
}

// Picker LOCAL-WORK distance gate (cross-zone same-map fix). Estimate an
// objective's planar world position + area radius so the picker can distinguish
// LOCAL work (same zone, within ~one zone) from a same-MAP-but-cross-ZONE trek.
// Continent maps span the whole continent (map 0 = all of Eastern Kingdoms,
// map 1 = Kalimdor), so "objective POI map == bot map" is NOT, by itself,
// "reachable/local": a kill target 3,400y away in another zone — possibly with
// no navmesh route — reads as same-map and hijacks the current objective,
// direct-pathing the bot into a cross-zone wedge while a reachable breadcrumb
// turn-in sits demoted (Tindle: L4 in Stormwind harbor, Q27635 kill in Dun
// Morogh route_ok=0 chosen over completed Q270 turn-in route_ok=1).
//
// Mirrors the same source order the WINNER's current_objective_poi resolution
// uses (matched POI blob centroid, then nearest-spawn fallback), but resolves
// only the planar centroid + max-vertex radius needed for the gate. Returns
// false when no position can be resolved — the caller then treats the objective
// as local (preserving prior behavior; the gate only ADDS rejection where data
// exists). Out radius lets a polygon-zone POI count as local when the bot is at
// its near EDGE, not forced to the distant centroid.
bool EstimateObjectivePlanarPos(QuestPOIData const* quest_poi,
    QuestObjectiveEntry const& obj, float bot_x, float bot_y, uint32 bot_map,
    float& out_x, float& out_y, float& out_radius)
{
    // 1) Matched POI blob centroid — same match order as the winner (Q-P2).
    if (quest_poi)
    {
        QuestPOIBlobData const* matched = nullptr;
        if (obj.id != 0)
            for (auto const& b : quest_poi->Blobs)
                if (b.QuestObjectiveID == int32(obj.id)) { matched = &b; break; }
        if (!matched)
            for (auto const& b : quest_poi->Blobs)
                if (b.QuestObjectID == obj.object_id) { matched = &b; break; }
        if (!matched)
            for (auto const& b : quest_poi->Blobs)
                if (b.ObjectiveIndex == int32(obj.storage_index)) { matched = &b; break; }
        if (matched && !matched->Points.empty())
        {
            double sx = 0.0, sy = 0.0;
            for (auto const& pt : matched->Points) { sx += double(pt.X); sy += double(pt.Y); }
            const double n = double(matched->Points.size());
            out_x = float(sx / n);
            out_y = float(sy / n);
            float rsq = 0.f;
            for (auto const& pt : matched->Points)
            {
                const float dx = float(pt.X) - out_x;
                const float dy = float(pt.Y) - out_y;
                const float r2 = dx * dx + dy * dy;
                if (r2 > rsq) rsq = r2;
            }
            out_radius = std::sqrt(rsq);
            return true;
        }
    }
    // 2) Nearest-spawn fallback for the dominant objective types — the same
    //    resolvers the winner's Q-P1a fallback uses (radius 0: a point target).
    ::CreatureData const*   cs = nullptr;
    ::GameObjectData const* gs = nullptr;
    switch (obj.type)
    {
        case QUEST_OBJECTIVE_MONSTER:
        case QUEST_OBJECTIVE_TALKTO:
            if (obj.object_id > 0)
                cs = NearestCreatureSpawnByEntry(uint32(obj.object_id), bot_x, bot_y, bot_map);
            break;
        case QUEST_OBJECTIVE_KILL_WITH_LABEL:
            if (!obj.labeled_target_entries.empty())
                cs = NearestCreatureSpawnByEntry(obj.labeled_target_entries.front(), bot_x, bot_y, bot_map);
            break;
        case QUEST_OBJECTIVE_GAMEOBJECT:
            if (obj.object_id > 0)
                gs = NearestGameObjectSpawnByEntry(uint32(obj.object_id), bot_x, bot_y, bot_map);
            break;
        case QUEST_OBJECTIVE_ITEM:
            if (obj.object_id > 0)
            {
                gs = QuestItemGoSpawnNearest(uint32(obj.object_id), bot_x, bot_y, bot_map);
                if (!gs)
                    cs = QuestItemCreatureSpawnNearest(uint32(obj.object_id), bot_x, bot_y, bot_map);
            }
            break;
        default:
            break;
    }
    if (cs) { out_x = cs->spawnPoint.GetPositionX(); out_y = cs->spawnPoint.GetPositionY(); out_radius = 0.f; return true; }
    if (gs) { out_x = gs->spawnPoint.GetPositionX(); out_y = gs->spawnPoint.GetPositionY(); out_radius = 0.f; return true; }
    return false;
}

// Does this objective's target physically spawn on the bot's CURRENT map? Mirrors
// EstimateObjectivePlanarPos's spawn resolvers but returns only yes/no. Needed
// because a QuestPOI blob can sit on a DIFFERENT map than the real target spawn —
// e.g. Q56185's TALKTO target (152365) spawns ONLY on the BfA assault map 1929,
// yet its POI blob is on map 0, so a POI-map check passes while the bot can never
// reach the target. Such an objective must not become best_any (the map-agnostic
// fallback) and strand the bot. Spawn-less/dynamic types return true (don't
// over-reject). Build-thread safe: spatial-index lookups only, no pathfinding.
static bool ObjectiveHasSameMapSpawn(QuestObjectiveEntry const& obj,
    float bot_x, float bot_y, uint32 bot_map)
{
    switch (obj.type)
    {
        case QUEST_OBJECTIVE_MONSTER:
        case QUEST_OBJECTIVE_TALKTO:
            return obj.object_id > 0 &&
                NearestCreatureSpawnByEntry(uint32(obj.object_id), bot_x, bot_y, bot_map) != nullptr;
        case QUEST_OBJECTIVE_KILL_WITH_LABEL:
            return !obj.labeled_target_entries.empty() &&
                NearestCreatureSpawnByEntry(obj.labeled_target_entries.front(), bot_x, bot_y, bot_map) != nullptr;
        case QUEST_OBJECTIVE_GAMEOBJECT:
            return obj.object_id > 0 &&
                NearestGameObjectSpawnByEntry(uint32(obj.object_id), bot_x, bot_y, bot_map) != nullptr;
        case QUEST_OBJECTIVE_ITEM:
            return obj.object_id > 0 &&
                (QuestItemGoSpawnNearest(uint32(obj.object_id), bot_x, bot_y, bot_map) != nullptr ||
                 QuestItemCreatureSpawnNearest(uint32(obj.object_id), bot_x, bot_y, bot_map) != nullptr);
        default:
            return true;   // AREATRIGGER / dynamic / unknown — can't cheaply verify, allow
    }
}

// L-P0b: does an item of this InventoryType belong in the given equipment
// slot? Used by the loot-roll NEED logic so a bot with an empty slot doesn't
// NEED-roll an item that could never go there (e.g. NEED a 2H sword "because
// my trinket slot is empty"). Mirrors the slot semantics of
// Player::FindEquipSlot without needing a live Item.
bool InvTypeMatchesSlot(uint32 inv_type, uint8 slot)
{
    switch (inv_type)
    {
        case INVTYPE_HEAD:            return slot == EQUIPMENT_SLOT_HEAD;
        case INVTYPE_NECK:            return slot == EQUIPMENT_SLOT_NECK;
        case INVTYPE_SHOULDERS:       return slot == EQUIPMENT_SLOT_SHOULDERS;
        case INVTYPE_BODY:            return slot == EQUIPMENT_SLOT_BODY;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:            return slot == EQUIPMENT_SLOT_CHEST;
        case INVTYPE_WAIST:           return slot == EQUIPMENT_SLOT_WAIST;
        case INVTYPE_LEGS:            return slot == EQUIPMENT_SLOT_LEGS;
        case INVTYPE_FEET:            return slot == EQUIPMENT_SLOT_FEET;
        case INVTYPE_WRISTS:          return slot == EQUIPMENT_SLOT_WRISTS;
        case INVTYPE_HANDS:           return slot == EQUIPMENT_SLOT_HANDS;
        case INVTYPE_FINGER:          return slot == EQUIPMENT_SLOT_FINGER1 || slot == EQUIPMENT_SLOT_FINGER2;
        case INVTYPE_TRINKET:         return slot == EQUIPMENT_SLOT_TRINKET1 || slot == EQUIPMENT_SLOT_TRINKET2;
        case INVTYPE_CLOAK:           return slot == EQUIPMENT_SLOT_BACK;
        case INVTYPE_TABARD:          return slot == EQUIPMENT_SLOT_TABARD;
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND:  return slot == EQUIPMENT_SLOT_MAINHAND;
        case INVTYPE_WEAPON:          return slot == EQUIPMENT_SLOT_MAINHAND || slot == EQUIPMENT_SLOT_OFFHAND;
        case INVTYPE_SHIELD:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:        return slot == EQUIPMENT_SLOT_OFFHAND;
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
        case INVTYPE_RELIC:           return slot == EQUIPMENT_SLOT_RANGED;
        default:                      return false;
    }
}

// R7 island-escape: does reaching this SAME-MAP leveling hub require a non-walk
// "bridge" leg (areatrigger teleport / ship / portal / taxi)? Only then does the
// bot become a relocation (a directly-walkable same-map hub is left to
// idle:travel_to_hub). We probe the UnifiedTravelGraph with a throwaway
// FindRoute and cache only the boolean verdict in BotAI keyed by the hub goal —
// a POD the world thread owns exclusively, so (unlike the travel_plan_ vector,
// which the AI worker owns) there is no cross-thread race. The AI worker's
// travel-plan executor rebuilds the actual leg list itself when it runs. The
// probe runs once per goal change, never per tick.
// Does the travel graph reach (toMap,tx,ty,tz) from the bot's CURRENT position
// via a route containing at least one NON-WALK leg (taxi / portal / ship /
// teleport / elevator)? Cached on the BotAI per goal key so the A* FindRoute
// runs once per distinct goal, not every tick. This is the shared core behind
// both the R7 leveling-hub relocation and the stuck same-map objective bridge.
// Stable per-goal cache key (mapId + quantised X/Y). Shared by the bridge
// decision cache so a decision LATCHES for a goal and survives path_blocked_count
// resetting once the bot starts following the route.
inline uint64 BridgeGoalKey(uint32 toMap, float tx, float ty)
{
    // Quantize to a 16-yard grid: quest POIs jitter by a yard or two between
    // builds (observed: 1693<->1694 oscillation), and per-yard keys made the
    // per-goal probe cache useless — the full A* + world-thread PathGenerator
    // validation re-ran every flip-flop. Bridge routing doesn't care about
    // sub-16y goal precision (the route lands at an attach node anyway).
    return (uint64(toMap) << 42) ^
           (uint64((uint32(int32(tx)) >> 4) & 0x1FFFFF) << 21) ^
           (uint64((uint32(int32(ty)) >> 4) & 0x1FFFFF));
}

bool GraphHasBridgeRoute(Player* p, BotAI* ai, uint32 toMap,
                         float tx, float ty, float tz)
{
    using namespace ::Playerbot::V2::Travel;
    const uint64 goalKey = BridgeGoalKey(toMap, tx, ty);

    // Negative-verdict expiry: a "no route" probe result can be a FALSE
    // negative — right after login/teleport the map tiles are still
    // streaming, so the navmesh validation in FindRoute drops every source
    // attach (observed live: from_attach flipping 1 -> 0 for the same spot
    // across boots). A permanent latch would wedge the goal forever on one
    // bad sample; re-probe negatives every few minutes. POSITIVE verdicts
    // stay latched (route legs are stable until the goal changes).
    constexpr uint32 kNegativeRetryMs = 3u * 60u * 1000u;
    const uint32 probe_now = GameTime::GetGameTimeMS();
    const bool negative_expired =
        ai->reloc_bridge_goal_key() == goalKey && !ai->reloc_bridge_has() &&
        (probe_now - ai->reloc_bridge_at_ms()) >= kNegativeRetryMs;

    if (ai->reloc_bridge_goal_key() != goalKey || negative_expired)
    {
        bool has_bridge = false;
        std::vector<BotAI::PlanLeg> legs;
        if (Services::Initialized() && Services::TravelGraph().IsInitialized())
        {
            RouteRequest req{};
            req.bot      = p;
            req.from_map = p->GetMapId();
            req.from_x   = p->GetPositionX();
            req.from_y   = p->GetPositionY();
            req.from_z   = p->GetPositionZ();
            req.to_map   = toMap;
            req.to_x = tx; req.to_y = ty; req.to_z = tz;
            req.allow_hearth = false;   // hearth would lose travel progress
            // This probe runs on the snapshot-build worker thread for a bot that
            // has already repeatedly path-failed toward the goal — validate source
            // attaches against the live navmesh so a bot wedged in a disconnected
            // pocket (Undercity interior, portal rooms) is not handed a walk-only
            // "route" through a wall that masks the real bridge (elevator /
            // teleport) out of the pocket. Worker-thread navmesh access is safe —
            // see the FindRoute sourceWalkable threading note in UnifiedTravelGraph.
            req.validate_source_walk = true;
            Route route = Services::TravelGraph().FindRoute(req);
            if (route.ok)
            {
                auto& g = Services::TravelGraph();
                for (RouteLeg const& leg : route.legs)
                {
                    if (leg.kind != EdgeKind::Walk) has_bridge = true;
                    BotAI::PlanLeg pl;
                    pl.kind   = uint8(leg.kind);
                    pl.to_map = leg.to_map;
                    pl.to_x = leg.to_x; pl.to_y = leg.to_y; pl.to_z = leg.to_z;
                    pl.payload = leg.payload_id;
                    if (GraphNode const* fn = g.GetNode(leg.from_node))
                    { pl.from_x = fn->x; pl.from_y = fn->y; pl.from_z = fn->z; }
                    if (leg.kind == EdgeKind::Taxi)
                        if (GraphNode const* tn = g.GetNode(leg.to_node))
                            pl.to_taxi_node = tn->payload_id;
                    legs.push_back(pl);
                }
            }
            TC_LOG_INFO("playerbot.v2",
                "[bridge_probe] {} goal=({}:{:.0f},{:.0f}) route_ok={} legs={} has_bridge={} "
                "from_attach={} to_attach={}",
                p->GetName(), toMap, tx, ty, route.ok ? 1 : 0, route.legs.size(),
                has_bridge ? 1 : 0, route.from_attach_count, route.to_attach_count);
        }
        ai->set_reloc_bridge_decision(goalKey, has_bridge, probe_now);
        if (has_bridge)
            ai->set_reloc_bridge_legs(std::move(legs));
    }
    return ai->reloc_bridge_has();
}

bool SameMapRelocationNeedsBridge(
    Player* p, BotAI* ai, ::Playerbot::V2::Travel::QuestHub const& hub)
{
    return GraphHasBridgeRoute(p, ai, hub.mapId,
        hub.location.GetPositionX(), hub.location.GetPositionY(),
        hub.location.GetPositionZ());
}

// Per-unit LoS with a short TTL cache. IsWithinLOSInMap is a real raycast
// through terrain + VMAP BIH trees + dynamic objects — and since the
// 2026-06-12 nav regen EVERY map has populated vmaps (BG maps previously
// had none, so these raycasts were near-free there). The builder fires up
// to ~32 of them per bot per Build (nearby_enemies + nearby_friends); with
// 150+ bots clustered in one battleground that compounded into multi-
// second snapshot phases (TickPerf build max 3.5s, world tick spikes to
// 10s). 900ms of staleness on a rule-gating hint ("is this unit behind a
// wall?") is harmless — consumers act over seconds.
// Phase 4 parallel-by-Map*: the cache is thread_local, so each persistent
// worker thread owns its own partition of the LoS memo (N partial caches
// across the fixed pool). LoS here is a TTL hint, so a per-worker cache is
// behavior-equivalent — the only observable difference is a cold-miss
// raycast the first time a (player,unit) pair is seen on a given worker,
// which produces the SAME bool answer IsWithinLOSInMap would. No lock, no
// shared map, no cross-thread clear() race. Each map's bots build on one
// worker, so a given bot's pairs stay on a single cache (good locality).
bool CachedLosCheck(Player const* p, Unit const* t, uint32 now_ms)
{
    struct LosEntry { uint32 at_ms; bool los; };
    thread_local std::unordered_map<uint64, LosEntry> s_losCache;
    constexpr uint32 kTtlMs = 900;
    if (s_losCache.size() > 65536)
        s_losCache.clear();
    const uint64 key = (uint64(p->GetGUID().GetCounter()) << 32)
                     ^ (t->GetGUID().GetCounter() * 0x9E3779B97F4A7C15ull);
    auto it = s_losCache.find(key);
    if (it != s_losCache.end() && now_ms - it->second.at_ms < kTtlMs)
        return it->second.los;
    const bool los = p->IsWithinLOSInMap(t);
    s_losCache[key] = { now_ms, los };
    return los;
}

// ---- Tier 3.1: per-bot snapshot RECYCLE POOL ----
//
// Every Build() used to make_shared<BotSnapshot>(), allocating ~52 inner
// containers per call. At ~1178 bots × every-tick rebuild that is the bulk of
// the allocation churn the backlog targets. The pool reuses the bot's prior
// snapshot buffer (reset_for_reuse() clears contents but KEEPS heap capacity),
// so a recycled build does near-zero container allocations.
//
// SAFETY GATE — the AI worker holds a shared_ptr copy of the published
// snapshot for the duration of its tick (SnapshotPublisher stores it in an
// atomic<shared_ptr>; latest() hands out a counted copy). A buffer is only
// safe to overwrite when NO ONE else references it. We detect that with
// use_count()==1: the pool is then the SOLE owner.
//
// We keep TWO buffers per bot (ping-pong). The reason: the publisher's atomic
// always retains the MOST-RECENTLY-published snapshot, so the just-published
// buffer's use_count is >=2 (pool slot + atomic) and can never be reused next
// tick. The OTHER slot (published two ticks ago, since dropped by the atomic
// when the newer one was stored) is the recyclable candidate — its use_count
// falls to 1 once any worker that read it has finished its tick. If BOTH slots
// are still busy (a worker straggling on the older buffer too — rare), we fall
// back to a fresh make_shared for this tick and simply don't retain it; the
// ring self-heals next tick.
//
// Phase 4 parallel-by-Map*: the pool is thread_local (one instance per
// persistent worker thread), so two workers building different maps never
// touch the same `slots` map — no rehash / iterator-invalidation race on the
// shared structure, no lock. Worker threads only ever touch the snapshot
// through the published shared_ptr (read-only). The reset/refill happens
// strictly before publish, while the buffer is provably unshared
// (use_count==1).
//
// PING-PONG STABILITY: the use_count()==1 reuse invariant requires a bot to
// be recycled by the SAME pool every tick. Because Build partitions by Map*
// and each Map* is assigned to a STABLE worker for the life of that map (see
// the partition assignment in PlayerbotV2.cpp), a bot's snapshots route to one
// worker's thread_local pool across ticks. When a bot changes maps (zone / BG
// / instance) it lands in a different worker's pool and pays a single fresh
// make_shared that tick — correct, just one missed recycle. When parallel
// Build is DISABLED the whole loop runs on the world thread, so there is
// exactly one thread_local pool (the world thread's) — identical to the prior
// single-static behavior.
struct SnapRecyclePool
{
    // Two retained buffers per bot.
    std::unordered_map<BotId, std::array<std::shared_ptr<BotSnapshot>, 2>> slots;

    // Returns a cleared, capacity-retaining buffer to build into. Prefers a
    // free (use_count==1) retained slot; otherwise allocates fresh.
    std::shared_ptr<BotSnapshot> acquire(BotId id)
    {
        // Opportunistic bound: if the map has grown large (bot churn /
        // never-pruned despawns), drop entries whose BOTH slots are free so
        // it can't grow without limit. Cheap amortised sweep, world thread.
        if (slots.size() > 4096)
        {
            for (auto it = slots.begin(); it != slots.end(); )
            {
                if ((!it->second[0] || it->second[0].use_count() == 1) &&
                    (!it->second[1] || it->second[1].use_count() == 1))
                    it = slots.erase(it);
                else
                    ++it;
            }
        }

        auto& pair = slots[id];
        for (auto& buf : pair)
        {
            if (buf && buf.use_count() == 1)
            {
                buf->reset_for_reuse();   // clear contents, keep capacity
                return buf;               // pool keeps its slot ref; caller shares
            }
        }
        // No free retained slot — allocate a transient. Park it in an empty
        // slot if one exists so it becomes recyclable later; otherwise it's
        // simply not retained (GC'd when refs drop).
        auto fresh = std::make_shared<BotSnapshot>();
        for (auto& buf : pair)
        {
            if (!buf) { buf = fresh; break; }
        }
        return fresh;
    }
};

SnapRecyclePool& RecyclePool()
{
    // thread_local — one pool per worker thread (and the world thread when
    // parallel Build is disabled). See SnapRecyclePool's header comment.
    thread_local SnapRecyclePool pool;
    return pool;
}

} // anonymous

std::shared_ptr<BotSnapshot const> BotSnapshotBuilder::Build(Player* p, BotAI* bot_ai, SnapshotVer next_version, TickId tick)
{
    if (!p || !p->IsInWorld()) return {};
    // `bot_ai` is the build-target bot's BotAI*, pre-resolved on the world
    // thread (see header). Every lookup below that previously called
    // Services::Registry().ai(snap->bot_id) / ai(p->GetGUID().GetCounter())
    // — all of which resolve THIS bot — now uses bot_ai directly so the
    // parallel build workers never touch the unlocked registry map.

    // Tier 3.1: recycle the bot's prior snapshot buffer when it is provably
    // unshared (use_count()==1); otherwise this falls back to a fresh alloc —
    // identical to the original behavior. The returned buffer is guaranteed
    // empty (reset_for_reuse cleared it) or fresh, so every field below is
    // written into a clean slate just as before.
    auto snap = RecyclePool().acquire(p->GetGUID().GetCounter());

    // ---- Versioning & identity ----
    snap->version    = next_version;
    snap->bot_id     = p->GetGUID().GetCounter();
    snap->world_tick = tick;
    snap->published_at_ms = GameTime::GetGameTimeMS();

    snap->guid    = p->GetGUID();
    snap->identity.name    = p->GetName();
    snap->identity.level   = p->GetLevel();
    snap->identity.xp            = p->GetXP();
    snap->identity.xp_for_level  = p->GetXPForNextLevel();
    snap->identity.rest_bonus_xp = static_cast<uint32>(p->GetRestMgr().GetRestBonus(REST_TYPE_XP));
    // PvP — honor track lives on m_activePlayerData (UpdateField), no
    // dedicated Player accessor in 12.0. The HK counters are uint32; honor
    // xp / level are uint32 as well.
    snap->identity.honor_xp             = p->m_activePlayerData->Honor;
    snap->identity.honor_xp_for_next    = p->m_activePlayerData->HonorNextLevel;
    // HonorLevel lives on m_playerData (publicly visible "honor level"
    // displayed on inspect / unit frames), not m_activePlayerData (which
    // holds the private xp totals).
    snap->identity.honor_level          = static_cast<uint32>(int32{p->m_playerData->HonorLevel});
    snap->identity.honor_kills_today     = p->m_activePlayerData->TodayHonorableKills;
    snap->identity.honor_kills_yesterday = p->m_activePlayerData->YesterdayHonorableKills;
    snap->identity.honor_kills_lifetime  = p->m_activePlayerData->LifetimeHonorableKills;
    snap->identity.race    = p->GetRace();
    // Map ::Team enum to our 0/1/2 short form. ALLIANCE = 469,
    // HORDE = 67. GetEffectiveTeam() respects mercenary mode in BGs.
    {
        Team t = p->GetEffectiveTeam();
        snap->identity.team = (t == ALLIANCE) ? 1u : (t == HORDE) ? 2u : 0u;
    }
    snap->identity.cls     = p->GetClass();
    snap->identity.gender  = p->GetGender();
    snap->identity.faction = p->GetFaction();
    // Active spec ID maps to ChrSpecialization DB2 entries (e.g. 253 = BM Hunter).
    // The wider value is truncated into uint8 because spec IDs <= 256 today;
    // when Blizzard expands beyond that we'll widen the field.
    snap->identity.spec    = static_cast<uint32>(p->GetPrimarySpecialization());

    // Infer role from (class, spec). Single source of truth lives in
    // Bot/ClassTables.cpp (IsTankSpec/IsHealerSpec) — also used by
    // BotQueueFiller, idle:dual_spec_switch, RunRebalance,
    // BracketCoverage, and Group/GroupSnapshotBuilder.
    auto infer_role = [](uint8 cls, uint32 spec) -> Role
    {
        if (IsTankSpec(cls, uint16(spec)))   return Role::Tank;
        if (IsHealerSpec(cls, uint16(spec))) return Role::Healer;
        return cls != 0 ? Role::Dps : Role::Unknown;
    };
    snap->group.my_role = infer_role(snap->identity.cls, snap->identity.spec);

    // ---- Vitals ----
    snap->vitals.hp     = static_cast<int32>(p->GetHealth());
    snap->vitals.max_hp = static_cast<int32>(p->GetMaxHealth());

    // Power per type — bots may swap forms / shapeshift, so we mirror every
    // power slot the array can hold (POWER_MANA=0 through POWER_RUNE_UNHOLY=22).
    // GetPower returns 0 for power types this class doesn't have, so unused
    // slots are cheap.
    for (uint8 i = 0; i < snap->vitals.power.size(); ++i)
    {
        const Powers pw = static_cast<Powers>(i);
        // Normalize to DISPLAY units via PowerType.db2 DisplayModifier —
        // the same scaling the client UI and every human-authored number
        // uses. TC stores Rage/Runic Power/Fury/Pain x10 and Insanity x100
        // internally; the APLs compare against display-unit thresholds
        // ("Rampage at 80 rage"), so raw values made every such gate pass
        // ~10x too early and the server rejected the cast with NO_POWER —
        // a third of the melee fleet stuck spamming unaffordable spenders
        // (audit B09). One divide here fixes all 60+ APL files at once.
        int32 raw     = p->GetPower(pw);
        int32 raw_max = p->GetMaxPower(pw);
        if (PowerTypeEntry const* pt = sDB2Manager.GetPowerTypeEntry(pw))
            if (pt->DisplayModifier > 1)
            {
                raw     /= pt->DisplayModifier;
                raw_max /= pt->DisplayModifier;
            }
        snap->vitals.power[i]     = raw;
        snap->vitals.max_power[i] = raw_max;
    }

    // ---- Position ----
    snap->position.map_id  = p->GetMapId();
    snap->position.instance_id = p->GetMap() ? p->GetMap()->GetInstanceId() : 0;
    snap->position.map_is_bg_or_arena =
        p->GetMap() && p->GetMap()->IsBattlegroundOrArena();
    snap->position.x       = p->GetPositionX();
    snap->position.y       = p->GetPositionY();
    snap->position.z       = p->GetPositionZ();
    snap->position.o       = p->GetOrientation();
    snap->area.area_id = p->GetAreaId();
    snap->area.zone_id = p->GetZoneId();
    snap->movement.is_swimming = p->IsInWater();
    snap->movement.is_flying   = p->IsFlying();
    snap->movement.is_mounted  = p->IsMounted();
    {
        // Unit::isMoving() reads MOVEMENTFLAG_MASK_MOVING — which, for a
        // REAL-client character driven as a selfbot, is set/cleared by the
        // CLIENT's movement packets. A missed stop packet (relog or lag
        // mid-keypress) wedges the flag ON while the character stands
        // still — and every is_moving-gated idle rule (quest walk, wander,
        // manual travel, ambient) goes silent FOREVER while the few
        // maintenance rules keep firing (2026-06-13: user's selfbot parked
        // 10+ min at full idle, only 3-min equip retries in the intent
        // log). Corroborate the flag against actual displacement: no
        // position change for >= 2s ⇒ effectively NOT moving. The
        // displacement anchor is per-bot state on the bot's own BotAI
        // (Phase 4 parallel-by-Map*): formerly a process-wide BotId-keyed
        // static map, now single-writer per worker — no shared-map race,
        // same self-cleaning behavior (clear on stop, re-anchor on move).
        bool moving = p->isMoving();
        BotAI* mc_ai = bot_ai;
        // Fall back to a local anchor when the AI can't be resolved; without
        // persistent state the wedge-corroboration simply trusts the flag,
        // which is the safe default (matches a fresh map entry).
        BotAI::MoveCorroborate local_anchor{};
        if (moving)
        {
            const uint32 mc_now = GameTime::GetGameTimeMS();
            BotAI::MoveCorroborate const& e =
                mc_ai ? mc_ai->move_corroborate() : local_anchor;
            if (!e.valid)
            {
                if (mc_ai) mc_ai->set_move_corroborate(
                    p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), mc_now);
            }
            else
            {
                const float ddx = p->GetPositionX() - e.x;
                const float ddy = p->GetPositionY() - e.y;
                const float ddz = p->GetPositionZ() - e.z;
                if (ddx * ddx + ddy * ddy + ddz * ddz > 0.01f)
                {
                    if (mc_ai) mc_ai->set_move_corroborate(
                        p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), mc_now);
                }
                else if (mc_now - e.since_ms >= 2000)
                    moving = false;   // flag wedged: no displacement for 2s
            }
        }
        else if (mc_ai)
            mc_ai->clear_move_corroborate();
        snap->movement.is_moving = moving;
    }
    // UNIT_STATE_IN_FLIGHT — the core is flying the bot on a taxi path. The AI
    // must stand down entirely (DispatchIdle bails) so no idle/unstick/watchdog
    // rule fires a move_to or teleport that would corrupt the flight spline.
    snap->movement.is_in_taxi  = p->IsInFlight();
    // Transport passenger state. p->GetTransport() returns the TransportBase
    // the player is currently attached to (ship, zeppelin, vehicle elevator,
    // etc.). Vehicles use the same interface but their movement is owner-
    // driven, not ship-route-driven, so we narrow to actual Transport by
    // dynamic_cast and read its IsStopped() flag (GO_DYNFLAG_LO_STOPPED).
    // When the cast fails the bot is in a non-ship transport (e.g. boss
    // vehicle); we mark on_transport=true but transport_stopped=true so
    // the idle freeze doesn't accidentally trap the bot in a vehicle
    // scripted-event seat.
    snap->environment.on_transport      = false;
    snap->environment.transport_stopped = false;
    snap->environment.transport_is_ship = false;
    if (TransportBase* tb = p->GetTransport())
    {
        snap->environment.on_transport = true;
        if (auto* ship = dynamic_cast<Transport*>(tb))
        {
            snap->environment.transport_stopped = ship->IsStopped();
            snap->environment.transport_is_ship = true;   // type-15 ship / zeppelin
        }
        else
            snap->environment.transport_stopped = true;   // non-ship transport (type-11 elevator) — let normal rules run
    }
    snap->area.is_indoors  = !p->IsOutdoors();
    // Water survival: is_underwater = head submerged (drowning state),
    // not just feet wet. GetLiquidStatus has been updated by the engine
    // every position-change tick, so this read is cheap. Surface Z comes
    // from Map::GetWaterLevel which queries the liquid header at (x,y);
    // 0 if no water (the rule gates on is_underwater, so 0 is never used
    // when is_underwater=true).
    {
        // Re-query the liquid status with full LiquidData so we can
        // distinguish water from magma/slime by type. The cheaper
        // p->GetLiquidStatus() returns the cached ZLiquidStatus mask but
        // doesn't expose the type flags. This Map::GetLiquidStatus call
        // touches grid_map terrain — cheap (one indexed read) and runs
        // once per snapshot per bot.
        LiquidData ld{};
        Map* lqmap = p->GetMap();
        const ZLiquidStatus liquid = lqmap
            ? lqmap->GetLiquidStatus(
                p->GetPhaseShift(),
                p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(),
                {}, &ld)
            : ZLiquidStatus(0);
        const bool in_liquid = (liquid & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER)) != 0;
        const bool is_magma = (ld.type_flags & map_liquidHeaderTypeFlags::Magma) != map_liquidHeaderTypeFlags::NoWater;
        const bool is_slime = (ld.type_flags & map_liquidHeaderTypeFlags::Slime) != map_liquidHeaderTypeFlags::NoWater;
        // Underwater (water-specific) only when the head is submerged AND
        // the liquid is real water — not magma. A naked bot in lava also
        // has feet wet, but it's burning, not drowning.
        snap->environment.is_underwater = (liquid & LIQUID_MAP_UNDER_WATER) != 0
                            && !is_magma && !is_slime;
        snap->environment.is_in_damaging_liquid = in_liquid && (is_magma || is_slime);
        if (snap->environment.is_underwater && lqmap)
            snap->environment.water_surface_z = lqmap->GetWaterLevel(
                p->GetPhaseShift(), p->GetPositionX(), p->GetPositionY());
        else
            snap->environment.water_surface_z = 0.0f;

        // Open-world water-escape FOCUS target (FIX #12). When the bot is in real
        // swim-water (not magma/slime), resolve the nearest DRY navmesh footing
        // so idle:water_escape can drive straight to it and PREEMPT all other
        // movement until the bot is on land — breaking the Stormwind-harbor
        // oscillation where the water-exit recovery pulls the bot to shore while
        // travel/wander immediately walk it back in. NearestNavPoint excludes
        // water/magma (dry footing only) and is SEH-guarded for the world thread;
        // gated on in-water so only swimming bots pay the navmesh query.
        snap->environment.water_escape_valid = false;
        if (in_liquid && !is_magma && !is_slime)
        {
            Position exit{};
            if (Playerbot::BotMovement::NearestNavPoint(
                    const_cast<Player*>(p), p->GetPositionX(), p->GetPositionY(),
                    p->GetPositionZ(), /*hxy*/ 60.0f, /*hz*/ 25.0f, exit))
            {
                // Require a real exit, not the bot's own (submerged) spot.
                const float ex = exit.GetPositionX() - p->GetPositionX();
                const float ey = exit.GetPositionY() - p->GetPositionY();
                const float ez = exit.GetPositionZ() - p->GetPositionZ();
                if (ex * ex + ey * ey + ez * ez > 2.0f * 2.0f)
                {
                    snap->environment.water_escape_valid = true;
                    snap->environment.water_escape_x = exit.GetPositionX();
                    snap->environment.water_escape_y = exit.GetPositionY();
                    snap->environment.water_escape_z = exit.GetPositionZ();
                }
            }
        }
    }
    // Riding skill: 0 if no riding learned. Drives the auto-mount rule's
    // "do we have any usable mount" gate. Use SKILL_RIDING (762).
    snap->travel.riding_skill = uint16(p->GetSkillValue(/*SKILL_RIDING*/ 762));
    // Best mount spell: walk the bot's spellbook for any spell with a
    // SPELL_AURA_MOUNTED effect. We pick the LAST one found (later spell
    // ids tend to be higher-tier mounts in TC's data, though this is a
    // heuristic — the server ultimately matches mount capability to the
    // bot's riding skill at cast time, and rejects unsuitable mounts).
    // Skip flying-only mounts entirely if riding_skill < 225 (expert);
    // the cost of rejection isn't worth picking them. Empty (0) when the
    // bot has no mount in its spellbook — auto-mount rule skips.
    // SN-P0b / DMM-P1a: choose the FASTEST usable mount DETERMINISTICALLY.
    // The old code kept whatever mount happened to come last out of the
    // unordered SpellMap, so the choice was non-deterministic (flickered
    // between snapshots) and frequently the slowest mount — a clear bot tell.
    // We now score every mount by its resolved speed aura and keep the best,
    // preferring a flying mount when the bot can actually fly (Expert riding
    // 225+), since on flyable maps a real player uses their fast flyer. Ties
    // break on lowest spell id so the pick is stable across ticks.
    //   aura  32 = SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED        (ground)
    //   aura 207 = SPELL_AURA_MOD_INCREASE_VEHICLE_FLIGHT_SPEED (flight)
    //   aura 208 = SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED (flight)
    // The full-spellbook scan below (SpellInfo + effect-array walk per spell
    // to score mounts and pick the fastest usable one) used to run every
    // Build. The pick only changes on mount-learn or riding-skill change, so
    // it lives behind a 30s cache (perf 0.2), gated like the recipe cache.
    // The cached value is additionally keyed on riding_skill: crossing the
    // Expert (225) threshold unlocks flying mounts and flips which mount is
    // "best", so a skill change invalidates the cache even inside the window.
    snap->travel.best_mount_spell = 0;
    {
        BotAI* mt_ai = bot_ai;
        const uint32 mt_now = GameTime::GetGameTimeMS();
        constexpr uint32 kMountCacheIntervalMs = 30000u;
        const bool mt_throttled = mt_ai &&
            mt_ai->last_mount_scan_ms() != 0 &&
            mt_ai->cached_mount_riding_skill() == snap->travel.riding_skill &&
            (mt_now - mt_ai->last_mount_scan_ms()) < kMountCacheIntervalMs;
        if (mt_throttled)
        {
            snap->travel.best_mount_spell = mt_ai->cached_best_mount_spell();
        }
        else
        {
            uint32 best_sid = 0;
            if (snap->travel.riding_skill > 0)
            {
                const bool can_fly = snap->travel.riding_skill >= 225;
                int32  best_score = -1;
                bool   best_is_flying = false;
                for (auto const& [sid, _] : p->GetSpellMap())
                {
                    SpellInfo const* si = sSpellMgr->GetSpellInfo(sid, DIFFICULTY_NONE);
                    if (!si) continue;
                    bool is_mount = false;
                    int32 ground_speed = 0;
                    int32 flight_speed = 0;
                    for (auto const& eff : si->GetEffects())
                    {
                        switch (eff.ApplyAuraName)
                        {
                            case 78:  is_mount = true; break;                          // SPELL_AURA_MOUNTED
                            case 32:  ground_speed = std::max(ground_speed, int32(eff.BasePoints)); break;
                            case 207:
                            case 208: flight_speed = std::max(flight_speed, int32(eff.BasePoints)); break;
                            default: break;
                        }
                    }
                    if (!is_mount) continue;
                    const bool is_flying = flight_speed > 0;
                    // A flying-only mount is useless where we can't fly — skip it so we
                    // don't pick it over a usable ground mount (server would reject the
                    // cast anyway, leaving the bot stuck on foot).
                    if (is_flying && !can_fly && ground_speed == 0) continue;
                    // Score: flight speed when we can fly, else ground speed. Prefer a
                    // flying mount over a ground one of equal score when flight is
                    // allowed (real players fly when they can).
                    const int32 score = (is_flying && can_fly) ? flight_speed : ground_speed;
                    const bool better =
                        score > best_score ||
                        (score == best_score && is_flying && can_fly && !best_is_flying) ||
                        (score == best_score && (is_flying && can_fly) == best_is_flying && (best_sid == 0 || sid < best_sid));
                    if (better)
                    {
                        best_score = score;
                        best_sid = sid;
                        best_is_flying = is_flying && can_fly;
                    }
                }
            }
            snap->travel.best_mount_spell = best_sid;
            if (mt_ai)
            {
                mt_ai->set_cached_best_mount_spell(best_sid);
                mt_ai->set_cached_mount_riding_skill(snap->travel.riding_skill);
                mt_ai->set_last_mount_scan_ms(mt_now);
            }
        }
    }
    if (Map* m = p->GetMap())
    {
        snap->instance_ctx.is_in_instance = m->Instanceable();
        snap->instance_ctx.is_in_dungeon  = m->IsDungeon();
        snap->instance_ctx.is_in_raid     = m->IsRaid();
        snap->instance_ctx.map_difficulty = m->GetDifficultyID();
    }

    // ---- Combat state ----
    snap->vitals.in_combat = p->IsInCombat();
    {
        const int64_t now_ms = static_cast<int64_t>(GameTime::GetGameTimeMS());
        // Combat-duration tracking lives on the bot's OWN BotAI (Phase 4
        // parallel-by-Map*): it is inherently per-bot state, so relocating it
        // off the former process-wide g_combat_start_ms / g_combat_exit_ms maps
        // removes a shared-map data-race surface with zero behavior change.
        // combat_*_ms()==0 means "not timing" / "never left", exactly as a
        // missing map entry did. Fall back to local scalars if the registry
        // can't resolve the AI (mirrors every other ai() use-site here).
        BotAI* ct_ai = bot_ai;
        int64_t local_start = 0, local_exit = 0;
        if (snap->vitals.in_combat)
        {
            int64_t start = ct_ai ? ct_ai->combat_start_ms() : local_start;
            if (start == 0)
            {
                start = now_ms;
                if (ct_ai) ct_ai->set_combat_start_ms(start);
                else       local_start = start;
            }
            // GameTimeMS is uint32; wraps every ~49 days. The subtraction
            // wraps consistently in unsigned then promotes back to int64
            // — duration never goes negative for the lifetime of a fight.
            const int64_t delta = static_cast<int64_t>(
                static_cast<uint32_t>(now_ms - start));
            snap->vitals.combat_duration = Ms{delta};
            // While in combat, "since combat exit" resets to 0 — a value
            // of 0 reads as "still fighting or never fought".
            snap->vitals.ms_since_combat_exit = Ms{0};
        }
        else
        {
            // Record the moment combat ended (only on the first OOC tick
            // after combat). combat_start_ms != 0 means we WERE in combat
            // last tick; we stamp the exit time and clear the start slot.
            const int64_t prev_start = ct_ai ? ct_ai->combat_start_ms() : local_start;
            if (prev_start != 0)
            {
                if (ct_ai) { ct_ai->set_combat_exit_ms(now_ms); ct_ai->set_combat_start_ms(0); }
                else       { local_exit = now_ms; local_start = 0; }
            }
            snap->vitals.combat_duration = Ms{0};
            const int64_t exit = ct_ai ? ct_ai->combat_exit_ms() : local_exit;
            if (exit != 0)
            {
                const int64_t exit_delta = static_cast<int64_t>(
                    static_cast<uint32_t>(now_ms - exit));
                snap->vitals.ms_since_combat_exit = Ms{exit_delta};
            }
            else
            {
                snap->vitals.ms_since_combat_exit = Ms{0};
            }
        }
    }
    snap->vitals.is_alive    = p->IsAlive();
    // CC: HasUnitState catches stun/root from spell + control auras alike.
    // Silence is aura-driven; check the SILENCED unit-flag (Unit.h).
    snap->vitals.is_stunned  = p->HasUnitState(UNIT_STATE_STUNNED);
    snap->vitals.is_rooted   = p->HasUnitState(UNIT_STATE_ROOT);
    snap->vitals.is_silenced = p->HasAuraType(SPELL_AURA_MOD_SILENCE);
    snap->vitals.is_pvp       = p->IsPvP();
    snap->vitals.is_ffa_pvp   = p->IsFFAPvP();
    snap->vitals.is_sanctuary = p->IsInSanctuary();
    snap->death.has_resurrect_request = p->IsResurrectRequested();
    snap->social_events.has_summon_pending    = p->HasSummonPending();

    // Death recovery: ghost flag, corpse position, reclaim window, and the
    // corpse-to-graveyard distance State_Dead reads to choose between
    // corpse-run (no sickness, slow) and spirit-healer (sickness, instant).
    // Computed inline because the snapshot is a value type — the AI worker
    // can't safely walk Map / ObjectMgr lookups on its thread.
    snap->death.is_ghost = p->HasPlayerFlag(PLAYER_FLAGS_GHOST);
    snap->death.has_corpse = p->HasCorpse();
    if (snap->death.has_corpse)
    {
        WorldLocation const corpseLoc = p->GetCorpseLocation();
        snap->death.corpse_map_id = corpseLoc.GetMapId();
        snap->death.corpse_x = corpseLoc.GetPositionX();
        snap->death.corpse_y = corpseLoc.GetPositionY();
        snap->death.corpse_z = corpseLoc.GetPositionZ();
        if (Corpse* corpse = p->GetCorpse())
        {
            const bool pvp = corpse->GetType() == CORPSE_RESURRECTABLE_PVP;
            snap->death.corpse_reclaim_at_unix = static_cast<int64>(corpse->GetGhostTime())
                                         + static_cast<int64>(p->GetCorpseReclaimDelay(pvp));
            // Capture the live Corpse's Map* instance id so State_Dead
            // can distinguish "same continent, different instance" from
            // a true same-Map reclaim window.
            if (Map* cmap = corpse->FindMap())
                snap->death.corpse_instance_id = cmap->GetInstanceId();
        }
        // Corpse → graveyard distance. Cache key is the corpse position;
        // it doesn't change while the corpse is in the world, so we reuse
        // the cached distance until either (a) the bot moves the corpse
        // (release / reclaim) or (b) a fresh death produces a new corpse
        // at a different location. Without the cache, GetClosestGraveyard
        // walks every nearby graveyard's condition list per snapshot.
        BotAI* dead_ai = bot_ai;
        bool cache_hit = false;
        if (dead_ai)
        {
            BotAI::CachedGraveDist const& c = dead_ai->cached_grave_dist();
            if (c.valid && c.map_id == snap->death.corpse_map_id &&
                c.corpse_x == snap->death.corpse_x &&
                c.corpse_y == snap->death.corpse_y &&
                c.corpse_z == snap->death.corpse_z)
            {
                snap->death.corpse_to_graveyard_dist = c.distance;
                cache_hit = true;
            }
        }
        if (!cache_hit)
        {
            if (WorldSafeLocsEntry const* grave = sObjectMgr->GetClosestGraveyard(corpseLoc, p->GetTeam(), p))
            {
                const float dx = grave->Loc.GetPositionX() - corpseLoc.GetPositionX();
                const float dy = grave->Loc.GetPositionY() - corpseLoc.GetPositionY();
                const float dz = grave->Loc.GetPositionZ() - corpseLoc.GetPositionZ();
                snap->death.corpse_to_graveyard_dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (dead_ai)
                    dead_ai->set_cached_grave_dist(snap->death.corpse_map_id,
                                                    snap->death.corpse_x, snap->death.corpse_y, snap->death.corpse_z,
                                                    snap->death.corpse_to_graveyard_dist);
            }
        }
        // Cross-map ghost: when the corpse is inside an instance and
        // the bot's ghost is OUTSIDE on the parent map, populate
        // instance_entrance_* with the portal's parent-map XY so the
        // dead-state walk-to-portal rule has a target. MapEntry's
        // GetEntrancePos returns (CorpseMapID, Corpse.X, Corpse.Y) —
        // CorpseMapID is the PARENT map where the portal sits, and
        // Corpse.X/Y is the portal's outdoor coord. Walking to those
        // coords trips the area-trigger that teleports the ghost back
        // into the instance and auto-rezzes them (no sickness).
        // Only overwrite when the bot is on the entrance's parent map
        // — protects the existing in-instance entrance population from
        // being clobbered when bot+corpse are both inside.
        if (snap->death.corpse_map_id != 0 &&
            snap->death.corpse_map_id != snap->position.map_id)
        {
            if (MapEntry const* corpse_me = sMapStore.LookupEntry(snap->death.corpse_map_id))
            {
                int32 ent_map = 0;
                float ent_x = 0.f, ent_y = 0.f;
                if (corpse_me->GetEntrancePos(ent_map, ent_x, ent_y) &&
                    uint32(ent_map) == snap->position.map_id)
                {
                    snap->dungeon_exec.instance_entrance_map = uint32(ent_map);
                    snap->dungeon_exec.instance_entrance_x   = ent_x;
                    snap->dungeon_exec.instance_entrance_y   = ent_y;
                    // Z not in DB2; consumer's move_to UpdateAllowedPositionZ
                    // snaps to terrain when called.
                    snap->dungeon_exec.instance_entrance_z   = 0.f;
                }
            }
        }
    }
    else
    {
        // No corpse → drop any cached distance so the next death recomputes.
        if (BotAI* alive_ai = bot_ai)
            alive_ai->clear_cached_grave_dist();
    }
    // Pending guild invite: GetGuildIdInvited returns the inviter's guild id.
    // Auto-clears once the player accepts/declines or joins another guild.
    if (p->GetGuildIdInvited() && !p->GetGuildId())
    {
        snap->guild.has_invite = true;
        snap->guild.invite_id  = p->GetGuildIdInvited();
    }
    snap->guild.id = p->GetGuildId();
    // Phase B: rank + member count for idle:guild_recruit_nearby gating.
    // Avoids a per-tick Guild::GetMember + GetMembersCount roundtrip in
    // the rule's hot path (the snapshot already runs on the world thread).
    if (snap->guild.id != 0)
    {
        if (Guild const* g = sGuildMgr->GetGuildById(snap->guild.id))
        {
            snap->guild.member_count = static_cast<uint16>(g->GetMembersCount());
            if (Guild::Member const* m = g->GetMember(p->GetGUID()))
                snap->guild.rank_id = static_cast<uint8>(m->GetRankId());
            // Phase C: count online members for chat-rule gating
            // (smalltalk needs ≥3, login_greet skips solo). One pass
            // through the member map is O(members) — bounded at 75 per
            // bot guild so the cost is trivial.
            uint16 online = 0;
            // SC-P3c: also count online members that are NOT V2 bots (real
            // humans), using the same Services::Registry().has(guid) bot-
            // check the promote-human-to-leader logic uses. Ambient guild
            // chatter (smalltalk / babble / tavern_hangout) is gated on this
            // being >= 1 so bots don't babble to an empty (bot-only) guild.
            // The registry lookup is a hash-map probe and the member map is
            // bounded at 75 per bot guild, so this stays trivial.
            uint16 online_humans = 0;
            const bool reg_ready = Services::Initialized();
            for (auto const& [_, member] : g->GetMembers())
            {
                if (!member.IsOnline()) continue;
                ++online;
                if (!reg_ready ||
                    !Services::Registry().has(member.GetGUID().GetCounter()))
                    ++online_humans;
            }
            snap->guild.online_member_count = online;
            snap->guild.online_human_member_count = online_humans;
            // Phase D: surface event signals into the snapshot so the
            // idle rules don't need to round-trip the manager every tick.
            if (Services::Initialized())
            {
                snap->guild.active_event_kind   = Services::Guilds().ActiveEventKind();
                snap->guild.has_pending_callout = Services::Guilds().HasPendingCallout(snap->guild.id);
                // #4C: bot-managed flag resolved once here (world thread)
                // instead of per-tick in every guild idle rule's gate.
                snap->guild.is_bot_managed      = Services::Guilds().IsBotManaged(snap->guild.id);
                // Phase E.2.5: rival_guild_id for rivalry banter rule.
                if (auto meta = Services::Guilds().MetaForGuild(snap->guild.id))
                    snap->guild.rival_id = meta->rival_low;
            }
        }
    }
    // Owner character name for mat-share mail. Resolved once per snapshot
    // via OwnerRegistry → CharacterCache so the AI worker doesn't need to
    // round-trip those services. Empty string when bot is unowned or the
    // owner is unbound to a specific character (account-only ownership).
    if (Services::Initialized())
    {
        OwnerBinding const ob = Services::Owners().GetOwner(snap->bot_id);
        if (ob.player_guid != 0)
        {
            ObjectGuid owner_guid = ObjectGuid::Create<HighGuid::Player>(ob.player_guid);
            std::string name;
            if (sCharacterCache->GetCharacterNameByGuid(owner_guid, name))
                snap->owner_name = std::move(name);
        }
    }
    snap->social_events.has_duel_request = p->duel && p->duel->State == DUEL_STATE_CHALLENGED &&
                                  p->duel->Initiator != p;
    if (snap->social_events.has_duel_request && p->duel && p->duel->Initiator)
    {
        Player* initiator = p->duel->Initiator;
        snap->social_events.duel_initiator = initiator->GetGUID();
        // "Friend" = same group/raid OR same guild OR on the bot's social
        // friend list. Same-group is the most common case for owner-driven
        // bots (the owner duels their own bot to test combat); the friend
        // list covers the case where the owner whitelisted the bot for
        // sparring without sharing a group.
        bool friend_flag = false;
        if (Group const* g = p->GetGroup())
            if (g->IsMember(initiator->GetGUID()))
                friend_flag = true;
        if (!friend_flag && p->GetGuildId() != 0 &&
            p->GetGuildId() == initiator->GetGuildId())
            friend_flag = true;
        if (!friend_flag)
            // GetSocial returns non-const so we can't const_cast around the
            // signature; HasFriend itself doesn't mutate the social, but the
            // declaration in TC isn't const-correct. Const-cast the social
            // pointer locally — safe and contained.
            if (PlayerSocial* social = const_cast<Player*>(p)->GetSocial())
                if (social->HasFriend(initiator->GetGUID()))
                    friend_flag = true;
        snap->social_events.duel_initiator_is_friend = friend_flag;
    }
    snap->social_events.quest_share_sender    = p->GetPlayerSharingQuest();
    snap->social_events.shared_quest_id       = p->GetSharedQuestID();
    snap->social_events.has_trade_request     = p->GetTradeData() != nullptr;
    snap->quest_log.completed_quest_count = static_cast<uint16>(p->GetRewardedQuestCount());
    // ---- LFG snapshot ----
    // These sLFGMgr getters ran for EVERY bot every build. Under the operator's
    // Logger.lfg=TRACE their internal getter logging (lfg.data.player.state.get
    // / .votekick.get) dumped ~80GB of "Player: ...State: None" across Server.log
    // + LFGsystem.log in 4 days — 34K solo leveling bots that are never in LFG —
    // and the per-bot map lookups added measurable snapshot-build cost. Gate the
    // whole block on being in an actual LFG group. The sLFGMgr getters are NOT
    // safe reads: GetState/IsVoteKickActive used operator[] on std::map, INSERTING
    // a junk entry for any non-LFG guid (fixed read-only in LFGMgr too) and emitting
    // TRACE spam — calling them per-bot per-build populated the LFG stores with the
    // whole fleet. isLFGGroup() is a pure flag (no LFGMgr call, no mutation), true
    // ONLY for a real LFG dungeon group, so an owner-squad / guild-grouped bot that
    // isn't in LFG no longer touches them. A solo bot, or a non-LFG group, leaves
    // the LFG fields at defaults. (No V2 path solo-queues — A17. When autonomous
    // SOLO LFG queueing lands, proposal_id/in_queue must move out: a proposal
    // arrives before the LFG group forms.)
    if (Group const* g = p->GetGroup(); g && g->isLFGGroup())
    {
        snap->lfg.proposal_id        = sLFGMgr->GetActiveProposalIdForPlayer(p->GetGUID());
        snap->lfg.in_queue           = (sLFGMgr->GetState(p->GetGUID()) == lfg::LFG_STATE_QUEUED);
        snap->lfg.role_check_pending = sLFGMgr->IsRoleCheckPending(g->GetGUID(), p->GetGUID());
        snap->lfg.published_role     = g->GetLfgRoles(p->GetGUID());
        snap->lfg.vote_kick_active   = sLFGMgr->IsVoteKickActive(g->GetGUID());
    }

    // Restart-robust dungeon-run arming signal. The isLFGGroup()-gated block
    // above is SKIPPED for a finder-formed bot group (it does not reliably carry
    // GROUP_FLAG_LFG → isLFGGroup()==false; observed live 2026-06-25), so it can
    // never report that the group is mid-run. Combined with last_lfg_dungeon_id
    // resetting to 0 on relogin/restart, a non-leader follower reloaded inside a
    // dungeon after a crash had NO signal to re-arm dungeon-run mode and fell
    // through to generic ingroup:follow_recall (MoveFollow), which pins forever
    // against a tank that is across a single off-mesh bridge — the squad split
    // permanently at the Deadmines foundry Gap-1 bridge (tank+1 across, healer+2
    // stuck at the near endpoint, 57 min frozen). LFGMgr's per-player DUNGEON
    // state DOES survive a restart and is authoritative, so query it directly —
    // independent of GROUP_FLAG_LFG. Gate on actually being inside a dungeon
    // instance so this stays a tiny subset (no per-bot getter spam for the 34K
    // solo fleet that motivated the isLFGGroup() gate above). GetState is a
    // read-only, non-mutating getter (LFGMgr operator[]-insert bug fixed).
    if (p->GetGroup() && snap->instance_ctx.is_in_dungeon)
        snap->lfg.in_dungeon = (sLFGMgr->GetState(p->GetGUID()) == lfg::LFG_STATE_DUNGEON);

    // Owned auctions across all four houses (Neutral=1, Alliance=2, Horde=6,
    // Goblin=7). Bots typically only post in their faction's house but might
    // have legacy postings elsewhere from cross-faction visits, so we walk
    // them all. Per-bot cap matches the live cap of a few hundred — we don't
    // truncate, and the AI has constant-time access via the snapshot copy.
    {
        const ObjectGuid me = p->GetGUID();
        const time_t now = GameTime::GetGameTime();
        auto walk_house = [&](uint32 house_id, AuctionHouseObject const* aho)
        {
            if (!aho) return;
            for (uint32 id : aho->GetOwnedAuctionIds(me))
            {
                AuctionPosting const* a = const_cast<AuctionHouseObject*>(aho)->GetAuction(id);
                if (!a || a->Items.empty()) continue;
                BotSnapshot::OwnedAuction e{};
                e.auction_id   = a->Id;
                e.item_entry   = a->Items.front()->GetEntry();
                e.stack_count  = a->GetTotalItemCount();
                e.min_bid      = a->MinBid;
                e.buyout       = a->BuyoutOrUnitPrice;
                const time_t end = std::chrono::system_clock::to_time_t(a->EndTime);
                e.expires_in_sec = static_cast<int64>(end - now);
                e.has_bidder   = !a->Bidder.IsEmpty();
                e.house_id     = house_id;
                snap->auction.auctions_owned.push_back(e);
            }
        };
        for (uint32 house_id : { 1u, 2u, 6u, 7u })
            walk_house(house_id, sAuctionMgr->GetAuctionsById(house_id));
    }

    // Competing-buyout lookup for AH undercut behavior. Only fires when
    // the bot is within 10y of an auctioneer NPC — keeps the AH walk
    // (O(N) over thousands of postings) out of the hot path. Builds a
    // lowest-buyout map indexed by item_entry, filtered to the bot's
    // faction house. The State_Idle ah_post rule reads this to undercut
    // by 1 copper instead of using the band-based floor.
    //
    // Perf gate (2026-05-17): the Cell::VisitAllObjects call was running
    // on every snapshot build for every bot — ~5K calls/sec at 1000 bots ×
    // 5Hz Active tier. Most bots are nowhere near an auctioneer and have
    // nothing to post anyway. Cheap pre-gates:
    //   1. Bot must own at least one non-quest, non-soulbound bag item.
    //      No postable items → no point checking proximity. Walks
    //      `bag_items` once (already in cache, ~80 entries max).
    //   2. Bot must be near a capital / city zone (auctioneers don't
    //      spawn in the open world). Approximated via `is_indoors` —
    //      auctioneer venues are taverns / Trade Districts which flip
    //      the indoor flag. Misses outdoor faction-capital nodes
    //      (acceptable: those AH bots will rebuild at the next move
    //      tick after stepping into the building).
    bool has_postable_item = false;
    for (auto const& it : snap->inventory.bag_items)
    {
        if (it.is_quest_item) continue;
        if (it.stats.is_soulbound || it.stats.bonding == 1) continue;
        if (it.quality < 2 || it.quality > 4) continue;
        has_postable_item = true;
        break;
    }
    if (has_postable_item && snap->area.is_indoors)
    {
        // Cheap auctioneer-proximity check: walk nearby_npcs_with_flag
        // wouldn't be ready until later in the build, so use the same
        // Cell visitor pattern that drives `nearest_npc_with_flag`.
        bool at_auctioneer = false;
        {
            constexpr float kAuctioneerRange = 10.0f;
            std::list<Creature*> creatures;
            Trinity::AnyUnitInObjectRangeCheck check(p, kAuctioneerRange);
            Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
                searcher(p, creatures, check);
            Cell::VisitAllObjects(p, searcher, kAuctioneerRange);
            for (Creature* c : creatures)
            {
                if (!c) continue;
                if (c->GetNpcFlags() & UNIT_NPC_FLAG_AUCTIONEER)
                { at_auctioneer = true; break; }
            }
        }
        if (at_auctioneer)
        {
            // Faction-house selection: Alliance=2, Horde=6, Goblin=7
            // serves both. Use Goblin (7) plus the faction house so
            // bots see the merged Cata+ neutral market. Skip house 1
            // (legacy neutral) — empty on most modern realms.
            const Team my_team = p->GetEffectiveTeam();
            std::vector<uint32> houses = { 7u };
            houses.push_back(my_team == ALLIANCE ? 2u : 6u);
            // Collect bag item entries we'd post — a small set bounded
            // by bag size (~80 items), so the map lookup is cheap.
            std::unordered_map<uint32, uint64> lowest;
            for (auto const& it : snap->inventory.bag_items)
            {
                if (it.is_quest_item) continue;
                if (it.quality < 2 || it.quality > 4) continue;
                if (it.stats.is_soulbound || it.stats.bonding == 1) continue;
                lowest.emplace(it.entry, 0ULL);   // sentinel; fill below
            }
            // Also include item entries the bot already has listed —
            // the cancel-on-undercut rule uses this snapshot to detect
            // when someone else posted below us and decide to refresh.
            for (auto const& own : snap->auction.auctions_owned)
                lowest.emplace(own.item_entry, 0ULL);
            for (uint32 house_id : houses)
            {
                AuctionHouseObject* aho = sAuctionMgr->GetAuctionsById(house_id);
                if (!aho) continue;
                // Iterate the entire house; cost bounded by AH size. The
                // at_auctioneer gate ensures this fires at most once per
                // bot per AH visit (next snapshot won't repeat unless
                // the bot moved away and came back).
                for (auto it = aho->GetAuctionsBegin(); it != aho->GetAuctionsEnd(); ++it)
                {
                    AuctionPosting const& a = it->second;
                    if (a.Items.empty()) continue;
                    // Skip my own postings — the undercut logic only
                    // cares about COMPETING listings. Without this skip
                    // a bot would treat its own listing as competition
                    // and repeatedly cancel/relist itself.
                    if (a.Owner == p->GetGUID()) continue;
                    const uint32 entry = a.Items.front()->GetEntry();
                    auto target = lowest.find(entry);
                    if (target == lowest.end()) continue;
                    const uint64 buyout = a.BuyoutOrUnitPrice;
                    if (buyout == 0) continue;   // bid-only auction
                    if (target->second == 0 || buyout < target->second)
                        target->second = buyout;
                }
            }
            snap->auction.ah_competing_buyout.reserve(lowest.size());
            for (auto const& [entry, lo] : lowest)
            {
                if (lo == 0) continue;   // no competition; let band price stand
                snap->auction.ah_competing_buyout.push_back({ entry, lo });
            }

            // ---- #4B buy-side: wanted-reagent buyable listings ----
            //
            // The "wanted" set is the reagents the bot is SHORT ON for its
            // known, still-skillable recipes — exactly the items it would
            // otherwise vendor-buy (see VendorRules phase 6). When those
            // reagents are gathered-only / unavailable at vendors, the AH is
            // the only source; surfacing the cheapest current listing lets a
            // buy-side economy rule emit EconomyOp::AhBuyout to close the
            // supply->demand->gold-sink loop.
            //
            // Recipes are resolved directly from the live Player here (world
            // thread) rather than snap->spellbook.known_recipes, which isn't
            // populated until later in the build. The at_auctioneer gate
            // already bounds this to at most once per AH visit, so the cost
            // (recipe walk + one house scan) is paid rarely. We reuse the
            // `houses` list already selected above (Goblin neutral + faction).
            std::unordered_map<uint32, uint32> wanted;   // reagent entry -> still-needed units
            {
                const Difficulty diff = p->GetMap() ? p->GetMap()->GetDifficultyID() : DIFFICULTY_NONE;
                for (auto const& [spell_id, ps] : p->GetSpellMap())
                {
                    if (!ps.active || ps.disabled) continue;
                    RecipeMeta const* meta = FindRecipeMeta(spell_id);
                    if (!meta) continue;
                    // Skip recipes that can no longer grant a skill point — the
                    // bot has no profession reason to stock their reagents.
                    const uint16 cur_skill = const_cast<Player*>(p)->GetSkillValue(meta->skill_line_id);
                    const RecipeColor color = ResolveRecipeColor(spell_id, cur_skill);
                    if (color == RecipeColor::Gray || color == RecipeColor::Unknown) continue;
                    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, diff);
                    if (!si) continue;
                    for (size_t i = 0; i < si->Reagent.size(); ++i)
                    {
                        const int32 entry = si->Reagent[i];
                        const int16 need  = si->ReagentCount[i];
                        if (entry <= 0 || need <= 0) continue;
                        const uint32 have = const_cast<Player*>(p)->GetItemCount(uint32(entry), false);
                        if (have >= uint32(need)) continue;
                        // Accumulate the largest single-craft shortfall per
                        // reagent (recipes share reagents; one entry suffices).
                        uint32& w = wanted[uint32(entry)];
                        w = std::max<uint32>(w, uint32(need) - have);
                    }
                }
            }
            if (!wanted.empty())
            {
                // Cheapest BUYABLE (buyout > 0) listing per wanted reagent,
                // split by sale model:
                //   - NON-commodity (rare for reagents): a single auction
                //     bought via AhBuyout. Tracked in `best` (carries an
                //     auction_id), surfaced into buyable_listings.
                //   - COMMODITY (the vast majority of craft reagents): the
                //     stackable trade-good bucket, bought via the
                //     GetCommodityQuote -> BuyCommodity path (AhBuyCommodity).
                //     There is no per-listing auction_id; we accumulate the
                //     cheapest per-unit price + the TOTAL available quantity
                //     across all non-self listings of the entry, surfaced into
                //     buyable_commodities. #4B-1 Part 3 makes this path real —
                //     previously commodity listings were skipped entirely and
                //     the buy-side loop was inert for almost all reagents.
                struct Best { uint32 auction_id; uint64 buyout; uint32 stack; };
                std::unordered_map<uint32, Best> best;   // entry -> cheapest single-item listing
                struct Comm { uint64 unit_price; uint32 available_qty; };
                std::unordered_map<uint32, Comm> comm;   // entry -> cheapest unit + total qty
                for (uint32 house_id : houses)
                {
                    AuctionHouseObject* aho = sAuctionMgr->GetAuctionsById(house_id);
                    if (!aho) continue;
                    for (auto it = aho->GetAuctionsBegin(); it != aho->GetAuctionsEnd(); ++it)
                    {
                        AuctionPosting const& a = it->second;
                        if (a.Items.empty()) continue;
                        if (a.Owner == p->GetGUID()) continue;       // can't buy own
                        const uint64 buyout = a.BuyoutOrUnitPrice;
                        if (buyout == 0) continue;                   // bid-only; buy-side wants buyout
                        const uint32 entry = a.Items.front()->GetEntry();
                        auto w = wanted.find(entry);
                        if (w == wanted.end()) continue;
                        if (a.IsCommodity())
                        {
                            // BuyoutOrUnitPrice on a commodity auction is the
                            // PER-UNIT price; GetTotalItemCount() is the units
                            // this seller has listed. Track cheapest unit and
                            // sum available across all sellers (the commodity
                            // quote fills from cheapest first, so total supply
                            // is what bounds an affordable buy).
                            Comm& c = comm[entry];
                            c.available_qty += a.GetTotalItemCount();
                            if (c.unit_price == 0 || buyout < c.unit_price)
                                c.unit_price = buyout;
                        }
                        else
                        {
                            auto bit = best.find(entry);
                            if (bit == best.end() || buyout < bit->second.buyout)
                                best[entry] = Best{ a.Id, buyout, a.GetTotalItemCount() };
                        }
                    }
                }
                // #4B-1(b): per-unit fair-value ceiling multiple for the
                // price-pump guard. Read once for the whole scan.
                const uint32 vendor_mult =
                    Services::Config().economy_max_reagent_vendor_multiple();
                snap->auction.buyable_listings.reserve(
                    std::min(best.size(), AuctionState::kMaxBuyableListings));
                for (auto const& [entry, b] : best)
                {
                    if (snap->auction.buyable_listings.size() >=
                        AuctionState::kMaxBuyableListings)
                        break;
                    const uint64 ceiling = ReagentFairValueCeiling(
                        sObjectMgr->GetItemTemplate(entry), vendor_mult);
                    snap->auction.buyable_listings.push_back(
                        { b.auction_id, entry, b.buyout, b.stack, ceiling });
                }
                snap->auction.buyable_commodities.reserve(
                    std::min(comm.size(), AuctionState::kMaxBuyableCommodities));
                for (auto const& [entry, c] : comm)
                {
                    if (snap->auction.buyable_commodities.size() >=
                        AuctionState::kMaxBuyableCommodities)
                        break;
                    if (c.unit_price == 0 || c.available_qty == 0) continue;
                    const uint64 ceiling = ReagentFairValueCeiling(
                        sObjectMgr->GetItemTemplate(entry), vendor_mult);
                    snap->auction.buyable_commodities.push_back(
                        { entry, c.unit_price, c.available_qty, ceiling });
                }
            }
        }
    }
    if (Group const* invite = p->GetGroupInvite())
    {
        snap->social_events.has_group_invite     = true;
        snap->social_events.group_invite_leader  = invite->GetLeaderGUID();
    }
    // Skip a dead victim — Player::GetVictim() can stay set for a tick after
    // the target dies, so APL rules that gate on victim() being non-empty
    // would fire offensive abilities on the corpse and burn an intent slot
    // on InvalidTarget. Better to surface no victim and let the InCombat
    // dispatch retarget via highest_threat_attacker.
    if (Unit* victim = p->GetVictim(); victim && victim->IsAlive())
        snap->combat.victim = victim->GetGUID();
    if (ObjectGuid t = p->GetTarget(); !t.IsEmpty())
    {
        snap->combat.current_target = t;
        // Classify the selection while we're on the world thread. Mere
        // selection presence must not gate autonomy (see CombatTargetsState
        // comment) — only a live, legally-attackable selection counts as
        // "engaged-ish". Self-selection is common (default client behavior
        // on some actions) and never hostile.
        if (t != p->GetGUID())
            if (Unit* sel = ObjectAccessor::GetUnit(*p, t))
                snap->combat.current_target_hostile =
                    sel->IsAlive() && p->IsValidAttackTarget(sel);
    }

    // Ranged-attacker victim seeding. Player::GetVictim() returns m_attacking,
    // which Player::Attack() sets — but for a pure ranged class (Hunter, Mage,
    // Warlock) that engages from distance, m_attacking can be empty for a tick
    // (or longer) after combat starts: the chase generator + auto-shot haven't
    // latched a melee victim yet, the previous victim died, or the bot was
    // pulled into combat by a mob it never explicitly Attack()ed. Every APL
    // damage rule (HasLiveTarget == !victim().IsEmpty(), and DoXxx casts at
    // ctx.bot.victim()) gates on combat.victim, so a ranged hunter with a valid
    // SELECTED target but no melee victim would no-op the entire shot rotation
    // and just stand there (observed L11 BM hunter Uraimus: zero combat casts).
    // When we're in combat and the chosen target (GetTarget) is a live hostile,
    // surface it AS the victim so the rotation fires this tick instead of
    // waiting for the InCombat fallback's start_attack to round-trip through the
    // executor + next snapshot. We only do this in combat (an OOC selected
    // target must NOT trigger offensive rules) and only for a hostile, living
    // unit (never seed the victim with a friendly heal-target a healer tab'd).
    if (snap->combat.victim.IsEmpty() && p->IsInCombat() &&
        !snap->combat.current_target.IsEmpty() &&
        snap->combat.current_target != snap->guid)
    {
        if (Unit* sel = ObjectAccessor::GetUnit(*p, snap->combat.current_target);
            sel && sel->IsAlive() && p->IsValidAttackTarget(sel))
            snap->combat.victim = sel->GetGUID();
    }

    // ---- Inventory snapshot (gold + free slots + bag contents) ----
    snap->inventory.gold           = static_cast<int32>(p->GetMoney());
    snap->bags.bag_free_slots = static_cast<uint8>(p->GetFreeInventorySlotCount());

    // bag_items powers BotSnapshotView::has_item() — used by survival potion
    // checks, conjured-item refill, OOC food, and whisper command targeting.
    // Walk the main backpack + 4 bag slots, recording entry + count + slot.
    auto record_bag_slot = [&](uint8 bag, uint8 slot, Item const* item)
    {
        if (!item) return;
        InventoryItem ii{};
        ii.guid          = item->GetGUID();
        ii.bag           = bag;
        ii.slot          = slot;
        ii.entry         = item->GetEntry();
        ii.count         = static_cast<uint16>(item->GetCount());
        ii.is_quest_item  = false;     // tracked via tmpl->Bonding when needed
        ii.durability_pct = 0;
        const uint32 max_dura = *item->m_itemData->MaxDurability;
        const uint32 cur_dura = *item->m_itemData->Durability;
        if (max_dura > 0)
            ii.durability_pct = static_cast<uint8>((cur_dura * 100) / max_dura);
        // Auto-equip support: capture item level + the equipment slot the
        // bot would wear this in. Player::FindEquipSlot returns
        // NULL_SLOT (255) when the item can't be equipped (wrong class,
        // armour subclass mismatch, level requirement unmet, cursed-
        // can't-be-removed, etc).
        //
        // swap=true so a two-handed weapon arriving while the bot is
        // dual-wielding correctly returns MAINHAND (with the off-hand
        // earmarked for replacement). Previously swap=false silently
        // returned NULL_SLOT for two-handers, hiding superior 2H drops
        // from the upgrade rule. Audit 2026-05-22.
        ii.item_level = static_cast<uint16>(item->GetItemLevel(p));
        ii.equip_slot = const_cast<Player*>(p)->FindEquipSlot(item, NULL_SLOT, /*swap*/ true);
        // Clamp non-equipment destinations to "not equippable". FindEquipSlot
        // returns bag slots (30-33) for CONTAINERS and profession-tool slots
        // for tools — but snapshot equipped[] is sized EQUIPMENT_SLOT_END
        // (19), so every downstream `equipped[equip_slot]` walk (upgrade
        // counter, MaintainAutoEquipUpgrades, EquipUpgradeFire) read OUT OF
        // BOUNDS for them, and the resulting garbage comparison could mark a
        // bag-in-bag as a pending "upgrade" that CanEquipItem then refuses
        // forever (B-11 wedge, live-verified on Uraimus 2026-06-11: item
        // 60240 -> bag slot, EquipItem|ServerRefused every tick). Bag/tool
        // swaps need their own non-empty-bag transfer flow; they are NOT
        // auto-equip candidates.
        if (ii.equip_slot != 0xFF && ii.equip_slot >= EQUIPMENT_SLOT_END)
            ii.equip_slot = 0xFF;
        ii.quality    = item->GetTemplate() ? static_cast<uint8>(item->GetTemplate()->GetQuality()) : 0u;
        ii.item_class    = item->GetTemplate() ? static_cast<uint8>(item->GetTemplate()->GetClass())    : 0u;
        ii.item_subclass = item->GetTemplate() ? static_cast<uint8>(item->GetTemplate()->GetSubClass()) : 0u;
        // Container capacity for the bag-upgrade rule (B-11b). Containers
        // are excluded from regular auto-equip (equip_slot clamped above),
        // so this is the rule's only ranking signal.
        ii.container_slots = (item->GetTemplate() &&
                              item->GetTemplate()->GetClass() == ITEM_CLASS_CONTAINER)
            ? static_cast<uint8>(std::min<uint32>(item->GetTemplate()->GetContainerSlots(), 255u))
            : 0u;
        // Stat block — only meaningful for equippable items (equip_slot
        // 0..18). Skip the population for vendor trash / consumables to
        // keep snapshot construction cheap; the upgrade rule already gates
        // on equip_slot != 0xFF.
        if (ii.equip_slot != 0xFF && ii.equip_slot < EQUIPMENT_SLOT_END)
            PopulateItemStatBlock(p, item, ii.stats);
        snap->inventory.bag_items.push_back(ii);
        // Maintain the entry→count index in lock-step. Items stack across
        // multiple slots, so accumulate (FlatCountMap::add defers the sum to
        // finalize()). Tier 3.3: was unordered_map[entry] += count.
        snap->inventory.bag_count_by_entry.add(ii.entry, ii.count);
        // Aggregate consumable-class counts so AI can decide "do I have enough
        // food?" without re-resolving ItemTemplate on the worker thread.
        // Stack count per item — a single 5-stack of food bumps food_drink_count
        // by 5, matching how MaintainOocFood uses one charge per use.
        if (ItemTemplate const* tmpl = item->GetTemplate())
        {
            // SN-P0a: populate is_quest_item from the template so the
            // disenchant / mail / AH / bank / mat-share quest-protection
            // guards (which all gate on `it.is_quest_item`) actually fire.
            // Previously hardcoded false → every one of those guards was a
            // silent no-op, risking a bot DEing or mailing away a quest item.
            // A quest item is ITEM_CLASS_QUEST, or quest-bound (BIND_QUEST=4),
            // or an item that starts a quest.
            ii.is_quest_item =
                tmpl->GetClass() == ITEM_CLASS_QUEST ||
                tmpl->GetBonding() == BIND_QUEST ||
                tmpl->GetStartQuest() != 0;
            if (tmpl->GetClass() == ITEM_CLASS_CONSUMABLE)
            {
                switch (tmpl->GetSubClass())
                {
                    case ITEM_SUBCLASS_FOOD_DRINK: snap->consumables.food_drink_count += ii.count; break;
                    case ITEM_SUBCLASS_POTION:
                    {
                        snap->consumables.potion_count += ii.count;
                        // Split into health/mana by inspecting the on-use
                        // spell's primary effect. Heal-aura → health potion;
                        // mana-restore → mana potion. Walks Effects[] for
                        // ITEM_SPELLTRIGGER_ON_USE and queries SpellInfo.
                        for (ItemEffectEntry const* eff : tmpl->Effects)
                        {
                            if (!eff || eff->TriggerType != ITEM_SPELLTRIGGER_ON_USE) continue;
                            SpellInfo const* si = sSpellMgr->GetSpellInfo(uint32(eff->SpellID), DIFFICULTY_NONE);
                            if (!si) continue;
                            for (auto const& spe : si->GetEffects())
                            {
                                // SPELL_EFFECT_HEAL = 10, SPELL_EFFECT_HEAL_PCT = 67
                                if (spe.Effect == SPELL_EFFECT_HEAL ||
                                    spe.Effect == SPELL_EFFECT_HEAL_PCT)
                                { snap->consumables.health_potion_count += ii.count; goto pot_done; }
                                // SPELL_EFFECT_ENERGIZE = 30, ENERGIZE_PCT = 89
                                if (spe.Effect == SPELL_EFFECT_ENERGIZE ||
                                    spe.Effect == SPELL_EFFECT_ENERGIZE_PCT)
                                {
                                    // MiscValue is power index; 0 = mana.
                                    if (spe.MiscValue == 0)
                                    { snap->consumables.mana_potion_count += ii.count; goto pot_done; }
                                }
                            }
                        }
                        pot_done:;
                        break;
                    }
                    case ITEM_SUBCLASS_BANDAGE:    snap->consumables.bandage_count    += ii.count; break;
                    default: break;
                }
            }
            // Quest-starting items: surface for the auto-pickup rule. Skip
            // if the quest is already in the bot's log (or completed) — the
            // item only re-triggers the dialog for QUEST_STATUS_NONE.
            if (uint32 start_qid = tmpl->GetStartQuest())
            {
                if (p->GetQuestStatus(start_qid) == QUEST_STATUS_NONE)
                {
                    if (Quest const* q = sObjectMgr->GetQuestTemplate(start_qid))
                    {
                        if (const_cast<Player*>(p)->CanTakeQuest(q, false))
                        {
                            BotSnapshot::StartingItem si{};
                            si.item_entry = ii.entry;
                            si.quest_id   = start_qid;
                            snap->quest_discovery.quest_starting_items.push_back(si);
                        }
                    }
                }
            }
        }
    };
    // Spec-driven stat weights. Resolve the bot's (class, spec) → weights
    // row once per snapshot; the upgrade rule reads spec_stat_weights[i]
    // directly without a per-tick table lookup. cls/spec are populated
    // earlier in this builder.
    {
        StatPriority const& pri = StatPriorityFor(snap->identity.cls, snap->identity.spec);
        snap->stat_weights.spec_stat_weights      = pri.weights;
        snap->stat_weights.spec_weapon_dps_weight = pri.weapon_dps_weight;
    }

    // Equipment (slots 0..18). Populates EquippedItem rows + average item
    // level. average_item_level is used by gear-aware decisions and by
    // .playerbot inspect; treating empty slots as 0 means a freshly created
    // bot won't satisfy gear gates.
    uint64 total_repair = 0;
    uint8  lowest_dura_pct = 100;
    {
        uint32 ilvl_sum = 0;
        uint8  ilvl_count = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            EquippedItem& dst = snap->inventory.equipped[slot];
            dst = {};
            Item const* item = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item) continue;
            dst.entry      = item->GetEntry();
            dst.item_level = static_cast<uint16>(item->GetItemLevel(p));
            const uint32 max_dura = *item->m_itemData->MaxDurability;
            const uint32 cur_dura = *item->m_itemData->Durability;
            dst.durability_pct = (max_dura > 0)
                ? static_cast<uint8>((cur_dura * 100) / max_dura) : 100;
            // Repair-cost + lowest-durability accumulators feed the vendor-visit
            // phase bitmask below. Computed inline here so the equipped walk
            // only happens once per snapshot build.
            if (max_dura > 0 && cur_dura < max_dura)
            {
                total_repair += item->CalculateDurabilityRepairCost(/*discount*/ 1.0f);
                if (dst.durability_pct < lowest_dura_pct)
                    lowest_dura_pct = dst.durability_pct;
            }
            // First ON_USE effect spell — typically populated for trinkets
            // (engineering tinkers also live here). Stays 0 for armor/weapons
            // without an on-use. Lets InCombat call is_ready(spell) without
            // re-resolving the ItemTemplate per tick.
            if (ItemTemplate const* tmpl = item->GetTemplate())
            {
                for (ItemEffectEntry const* eff : tmpl->Effects)
                {
                    if (!eff) continue;
                    if (eff->TriggerType == ITEM_SPELLTRIGGER_ON_USE)
                    {
                        dst.on_use_spell_id = static_cast<uint32>(eff->SpellID);
                        break;
                    }
                }
            }
            // Stats for the equipped piece. Lets the upgrade rule compute
            // score(equipped) without re-resolving Item* on the worker
            // thread.
            PopulateItemStatBlock(p, item, dst.stats);
            ilvl_sum += dst.item_level;
            ++ilvl_count;
        }
        snap->inventory.average_item_level = ilvl_count > 0
            ? static_cast<uint16>(ilvl_sum / ilvl_count) : 0;
    }

    // ---- Bag-slot inspection (Phase 3 of item plan) ----
    // Walk the 4 equipped bag slots (INVENTORY_SLOT_BAG_START..END).
    // Track smallest capacity + whether any slot is empty so the
    // vendor-visit FSM can decide whether to buy a bigger bag.
    {
        uint8 smallest = 255;
        bool  any_empty = false;
        for (uint8 b = INVENTORY_SLOT_BAG_START; b < INVENTORY_SLOT_BAG_END; ++b)
        {
            const uint8 idx = b - INVENTORY_SLOT_BAG_START;
            Bag* bag = const_cast<Player*>(p)->GetBagByPos(b);
            if (!bag)
            {
                any_empty = true;
                snap->bags.equipped_bag_capacity[idx] = 0;
                snap->bags.equipped_bag_subclass[idx] = 0xFF;
                continue;
            }
            const uint8 cap = static_cast<uint8>(bag->GetBagSize());
            if (cap < smallest) smallest = cap;
            // Per-slot capacity + subclass for idle:equip_bag_upgrade's
            // destination pick (B-11b): fill an empty slot first, else
            // replace the smallest NORMAL (subclass 0) bag.
            snap->bags.equipped_bag_capacity[idx] = cap;
            snap->bags.equipped_bag_subclass[idx] = bag->GetTemplate()
                ? static_cast<uint8>(bag->GetTemplate()->GetSubClass())
                : 0u;
        }
        if (smallest == 255) smallest = 0;     // no bags equipped → 0
        snap->bags.smallest_bag_capacity = smallest;
        snap->bags.has_empty_bag_slot    = any_empty;
    }

    // ---- Vendor visit prep: phase bitmask ----
    // total_repair / lowest_dura_pct were computed inline in the equipped-items
    // walk above (single-pass design). Discount = 1.0f means "no reputation
    // discount factored in"; the rule adds a 20% safety margin against
    // bot.gold to absorb any reputation discount mismatch.
    {
        snap->vendor_visit.estimated_repair_cost = static_cast<uint32>(std::min<uint64>(total_repair, std::numeric_limits<uint32>::max()));

        // Phase bitmask. Each bit's "should we" gate combines a need
        // signal with a cost gate (where applicable). Vendor-rule reads
        // the mask and short-circuits when 0.
        uint8 mask = 0;
        const int32 gold = snap->inventory.gold;
        // bit0 — repair: HUMAN MODEL (2026-06-15): a player doesn't make a
        // special repair trip at 70% — they repair opportunistically at a hub
        // vendor, and only divert when gear is genuinely about to fail. Gate at
        // <30% so the repair signal (which drives the opportunistic 25y
        // wander_to_service redirect, NOT a dedicated trip — see travel_to_vendor)
        // fires only when it actually matters. + gold covers 1.2× cost.
        // total_repair == 0 happens for ilvl-1 starter gear (DurabilityCosts.db2
        // has no row for itemlevel 1 → CalculateDurabilityRepairCost returns 0),
        // yet such gear DOES wear to 0% and cripples the bot. Treat zero cost as
        // affordable (a free RepairAll) so the critical bit still raises — mirrors
        // the cost==0 ⇒ affordable logic already in State_Dead's spiral-escape.
        if (lowest_dura_pct < 30 &&
            (total_repair == 0 || uint64(gold) >= total_repair + (total_repair / 5)))
            mask |= 0x01;
        // bit1 — sell trash / bags full. HUMAN MODEL: having a grey item is NOT a
        // reason to abandon questing and walk to a vendor — greys are sold
        // opportunistically when already AT a vendor (idle:vendor_sell_trash, which
        // keys on bag_free_slots directly). Only GENUINELY full bags (≤2 free — the
        // point where loot acceptance stops) justify a dedicated vendor trip, so
        // this bit (which gates idle:travel_to_vendor) only sets on real fullness.
        if (snap->bags.bag_free_slots <= 2)
            mask |= 0x02;
        // bit2 — bag upgrade: at least one empty slot OR smallest capacity
        // < 14 (Runecloth tier). Pick the largest BagSizeTable row whose
        // approx_price ≤ gold/2 (keep half gold for other needs); set bit
        // only if the chosen capacity strictly exceeds the smallest equipped.
        {
            // Walk descending so we pick the largest affordable.
            uint8 target = 0;
            for (auto it = kBagSizeTable.rbegin(); it != kBagSizeTable.rend(); ++it)
            {
                if (uint64(gold) >= uint64(it->approx_price) * 6 / 5) // 1.2x reserve
                { target = it->capacity; break; }
            }
            // Trigger on either an empty slot we can fill, or a smallest
            // bag below 14 with a strictly-larger affordable target.
            const bool empty_fill =
                snap->bags.has_empty_bag_slot && target > 0;
            const bool size_upgrade =
                snap->bags.smallest_bag_capacity > 0 &&
                snap->bags.smallest_bag_capacity < 14 &&
                target > snap->bags.smallest_bag_capacity;
            if (empty_fill || size_upgrade)
                mask |= 0x04;
        }
        // bit3 — food/drink: count below target + gold + NOT on the food-buy
        // cooldown. The cooldown gate is essential here, not just at the buy
        // site: without it the bot keeps flagging "need food" → travels to a
        // vendor → can't buy (cooldown) → flag still set → travels again, i.e.
        // the Darnassus quest↔vendor oscillation. Once it has topped up, the
        // 30-min cooldown clears the need so it goes back to questing.
        constexpr uint16 kFoodComfortMin = 5;
        constexpr int32  kFoodMinGold    = 5000;     // 50s — covers L1-30 food prices
        if (snap->consumables.food_drink_count < kFoodComfortMin && gold >= kFoodMinGold)
        {
            BotAI* fai = bot_ai;
            if (!fai || fai->food_buy_off_cooldown(GameTime::GetGameTimeMS()))
                mask |= 0x08;
        }
        // bit4 — bandages: same shape, lower threshold
        constexpr uint16 kBandageComfortMin = 5;
        constexpr int32  kBandageMinGold    = 5000;
        if (snap->consumables.bandage_count < kBandageComfortMin && gold >= kBandageMinGold)
            mask |= 0x10;
        // bit5 — repair-soon (OPPORTUNISTIC): gear isn't critical yet (<30%, bit0)
        // but is heading there (<35%) and the bot can afford the repair with the
        // same 1.2× margin as bit0. This drives idle:proactive_repair_route, which
        // is gated on the bot being OOC and having NO reachable actionable quest —
        // i.e. an opportunistic top-up that YIELDS to questing. It is NOT a
        // dedicated/quest-overriding trip (the 2026-06-15 change deliberately
        // dropped low-durability as a dedicated-trip trigger because it caused
        // quest abandonment); the FORCED repair lives only in the critical /
        // death-spiral path (State_Dead / State_InCombat). PURE ARITHMETIC on
        // already-computed lowest_dura_pct/total_repair/gold — no index/navmesh
        // lookup here (the snapshot Build is the freeze surface).
        if (lowest_dura_pct < 35 &&
            (total_repair == 0 || uint64(gold) >= total_repair + (total_repair / 5)))
            mask |= 0x20;
        snap->vendor_visit.phases_pending = mask;
    }

    // Secondary stats. We pick the school that matches the bot's role:
    // casters use spell-school rating, physical classes use melee. Mastery
    // and versatility are unified across schools. GetRatingBonusValue
    // returns the post-DR percentage as a float; we scale ×100 to int16.
    {
        const bool caster = snap->identity.cls == CLASS_MAGE || snap->identity.cls == CLASS_PRIEST ||
                            snap->identity.cls == CLASS_WARLOCK || snap->identity.cls == CLASS_DRUID ||
                            snap->identity.cls == CLASS_SHAMAN || snap->identity.cls == CLASS_EVOKER ||
                            snap->identity.cls == CLASS_PALADIN; // includes paladin spell-heals
        snap->secondary_stats.crit_pct_x100   = static_cast<int16>(
            p->GetRatingBonusValue(caster ? CR_CRIT_SPELL : CR_CRIT_MELEE) * 100.f);
        snap->secondary_stats.haste_pct_x100  = static_cast<int16>(
            p->GetRatingBonusValue(caster ? CR_HASTE_SPELL : CR_HASTE_MELEE) * 100.f);
        snap->secondary_stats.mastery_pct_x100 = static_cast<int16>(
            p->GetRatingBonusValue(CR_MASTERY) * 100.f);
        snap->secondary_stats.versatility_pct_x100 = static_cast<int16>(
            p->GetRatingBonusValue(CR_VERSATILITY_DAMAGE_DONE) * 100.f);
        snap->secondary_stats.resilience_pct_x100 = static_cast<int16>(
            p->GetRatingBonusValue(CR_RESILIENCE_PLAYER_DAMAGE) * 100.f);
        snap->secondary_stats.pvp_power_pct_x100  = static_cast<int16>(
            p->GetRatingBonusValue(CR_PVP_POWER) * 100.f);
    }

    // Backpack (slots 23..38)
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        record_bag_slot(INVENTORY_SLOT_BAG_0, slot, p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    // Equipped bags (slots 19..22)
    for (uint8 bag_slot = INVENTORY_SLOT_BAG_START; bag_slot < INVENTORY_SLOT_BAG_END; ++bag_slot)
    {
        if (Bag* bag = p->GetBagByPos(bag_slot))
            for (uint32 i = 0; i < bag->GetBagSize(); ++i)
                record_bag_slot(static_cast<uint8>(bag_slot), static_cast<uint8>(i), bag->GetItemByPos(static_cast<uint8>(i)));
    }
    // Tier 3.3: sort + merge-sum the accumulated (entry, count) pairs so
    // item_count(entry) can binary-search. Must run AFTER every record_bag_slot
    // above (backpack + all equipped bags) so all stacks are counted.
    snap->inventory.bag_count_by_entry.finalize();

    // ---- Pending equipment upgrades count ----
    // Count bag items that are strict ilvl upgrades over their target slot.
    // Mirrors the auto-equip rule's predicate exactly so /upgrades and the
    // rule agree. Caps at 255 (uint8 storage; bag holds ~80 items max so
    // this is structurally well under the cap).
    {
        uint16 cnt = 0;
        for (auto const& it : snap->inventory.bag_items)
        {
            if (it.equip_slot == 0xFF) continue;
            // Mirror the auto-equip rule's Prot-warrior/paladin weapon exclusion
            // (State_Idle dungeon_auto_equip / MaintainAutoEquipUpgrades): for
            // spec 73 (Prot Warrior) / 66 (Prot Paladin) the mainhand + offhand
            // are owned EXCLUSIVELY by EnsureShieldTankWeapon (the 1H+shield
            // backfill). The score-based rule NEVER swaps those two slots — a
            // higher-score 2H would evict the shield and churn 2H<->1H+shield —
            // so a bag weapon that out-scores the equipped 1H is a PERMANENT
            // non-upgrade here. Counting it reported a "pending upgrade" that can
            // never apply: the false 6-pending seen on Prot tanks (owner-flagged
            // 2026-07-01, the classic offhand-vs-2H false alarm). Exclude them so
            // this count and the rule agree, as this block's header promises.
            if ((snap->identity.spec == 73 || snap->identity.spec == 66) &&
                (it.equip_slot == EQUIPMENT_SLOT_MAINHAND ||
                 it.equip_slot == EQUIPMENT_SLOT_OFFHAND))
                continue;
            // Defense-in-depth: equip_slot is clamped to <19 at capture, but
            // an OOB read of equipped[19] here is UB — never index past it.
            if (it.equip_slot >= snap->inventory.equipped.size()) continue;
            if (it.item_level == 0)    continue;
            if (it.quality == 0)       continue;   // skip POOR
            EquippedItem const& cur = snap->inventory.equipped[it.equip_slot];
            // Score-based upgrade test (was pure item_level, which tied — and so
            // refused — quest greens vs starter whites once 12.0 scaling floored
            // both to the same effective ilvl). The stat allocation breaks the tie.
            if (cur.entry != 0 &&
                EquipFitScore(it.stats, it.item_level, snap->stat_weights) <=
                EquipFitScore(cur.stats, cur.item_level, snap->stat_weights))
                continue;
            ++cnt;
        }
        snap->bags.upgrades_pending = static_cast<uint8>(std::min<uint16>(cnt, 255));
    }

    // ---- Group ----
    if (Group* g = p->GetGroup())
        snap->group.group_guid = g->GetGUID();

    // ---- Spellbook (sorted, binary-searchable in BotSnapshotView::knows_spell) ----
    // Filter (active && !disabled) + copy + sort of the whole SpellMap
    // (300+ entries) used to run every Build, though the result only
    // mutates on learn / level-up / respec. 30s cache (perf 0.1), gated
    // exactly like the recipe cache below — with the extra invalidation
    // that a SpellMap-size change forces an immediate rebuild (catches a
    // learn/unlearn within the window). The timer backstops same-count
    // respecs (swap one spell for another at identical total) that the
    // size check alone would miss.
    {
        BotAI* ks_ai = bot_ai;
        const uint32 ks_now = GameTime::GetGameTimeMS();
        // Key the cache on the count of ACTIVE && !disabled spells — the exact
        // set that feeds the filtered output below — NOT raw GetSpellMap().size().
        // RemoveSpell(spell, disabled=true) (talent/glyph/dual-spec change,
        // dependent-spell removal) and rank supersede flip an existing entry's
        // active/disabled flag WITHOUT changing the map size, which a size-only
        // key would miss — leaving a disabled spell in known_spells (and thus
        // BotSnapshotView::knows_spell) for up to 30s. Counting is the cheap
        // part of the scan; the sort + vector copy the cache elides is the
        // expensive part, so the perf win is preserved. The 30s timer still
        // backstops the rare same-count swap (learn A, unlearn B → identical
        // active count) the count check alone cannot see.
        size_t ks_active = 0;
        for (auto const& [spell_id, ps] : p->GetSpellMap())
            if (ps.active && !ps.disabled) ++ks_active;
        constexpr uint32 kKnownSpellsCacheIntervalMs = 30000u;
        const bool ks_throttled = ks_ai &&
            ks_ai->last_known_spells_scan_ms() != 0 &&
            ks_ai->cached_spellmap_size() == ks_active &&
            (ks_now - ks_ai->last_known_spells_scan_ms()) < kKnownSpellsCacheIntervalMs;
        if (ks_throttled)
        {
            snap->spellbook.known_spells = ks_ai->cached_known_spells();
        }
        else
        {
            snap->spellbook.known_spells.reserve(ks_active);
            for (auto const& [spell_id, ps] : p->GetSpellMap())
            {
                if (!ps.active || ps.disabled) continue;
                snap->spellbook.known_spells.push_back(spell_id);
            }
            std::sort(snap->spellbook.known_spells.begin(), snap->spellbook.known_spells.end());
            if (ks_ai)
            {
                ks_ai->mutable_cached_known_spells() = snap->spellbook.known_spells;
                ks_ai->set_cached_spellmap_size(ks_active);
                ks_ai->set_last_known_spells_scan_ms(ks_now);
                // A respec/relearn can change which spell carries the GCD;
                // re-resolve the GCD probe (perf 0.3) on the next combat
                // build rather than trusting a possibly-stale id.
                ks_ai->set_cached_gcd_probe_spell(0);
            }
        }
    }

    // ---- Diagnostic: per-level spellbook dump for low-level bots ----
    // L1-20 starter-zone bots are where the APL's hardcoded spell IDs are
    // most likely to mismatch the server's SkillLineAbility data layer
    // (classic vs retail IDs, server-specific overlays). Logging the
    // actual known-spell list at each level transition lets us audit
    // which IDs the SkillLine grants at which level (Hunter's Mark at L7,
    // Arcane Shot at L1, etc.) so APL candidate-lists can be tuned to
    // reality without guessing.
    //
    // Gate: level <= 20 + level changed since last emit. Bounded log
    // volume — at most 20 lines per bot per server lifetime.
    {
        BotAI* sb_ai = bot_ai;
        const uint8 lvl = p->GetLevel();
        if (sb_ai && lvl <= 20 && sb_ai->spellbook_diag_level() != lvl)
        {
            std::string ids;
            ids.reserve(snap->spellbook.known_spells.size() * 8);
            for (uint32 sid : snap->spellbook.known_spells)
            {
                if (!ids.empty()) ids += ',';
                ids += std::to_string(sid);
            }
            TC_LOG_INFO("playerbot.v2",
                "[bot_spells] name={} class={} race={} lvl={} count={} spells=[{}]",
                p->GetName(), uint32(p->GetClass()), uint32(p->GetRace()),
                uint32(lvl), snap->spellbook.known_spells.size(), ids);
            sb_ai->set_spellbook_diag_level(lvl);
            sb_ai->set_spellbook_diag_logged(true);
        }
    }

    // ---- Recipes + self-teleport spells (combined walk, 30s cache) ----
    // Single pass over known_spells classifying each spell into
    // recipe / teleport buckets. Previously two separate loops each
    // re-fetching the same SpellInfo; combined to halve sSpellMgr
    // lookups (perf audit a2155f31bccccc961 #2).
    //
    // Recipes: SPELL_EFFECT_CREATE_ITEM / _LOOT / _RANDOM_ITEM —
    // crafting recipes consumed by /craft + idle:auto_craft.
    // Self-teleports: SPELL_EFFECT_TELEPORT_UNITS or _LOADING_SCREEN
    // with resolvable spell_target_position — Mage city ports, DK
    // Death Gate, Druid Moonglade, etc. PORTAL spells (the GO-creating
    // variant) intentionally skipped — bot can't reliably cast a
    // portal and walk into it within a 2-3 tick window; static portal
    // GOs cover the same destinations.
    //
    // 30s cache (2026-05-17): walking all 300+ known spells + iterating
    // each spell's effect array was dominating the spellbook phase of
    // Build (~150K effect comparisons/sec at 200 bots × 5Hz × 14%
    // build_rate). Recipes/teleports only mutate on spell-learn / -
    // unlearn events (trainer visit, level-up, respec) so a 30s cache
    // is safe; bots reach a trainer + complete the train round-trip in
    // far longer than 30s.
    {
        BotAI* sc_ai = bot_ai;
        const uint32 sc_now = GameTime::GetGameTimeMS();
        constexpr uint32 kSpellCacheIntervalMs = 30000u;
        const bool sc_throttled = sc_ai &&
            sc_ai->last_spellcache_scan_ms() != 0 &&
            (sc_now - sc_ai->last_spellcache_scan_ms()) < kSpellCacheIntervalMs;
        if (sc_throttled)
        {
            snap->spellbook.known_recipes      = sc_ai->cached_recipes();
            snap->travel.self_teleport_spells  = sc_ai->cached_self_teleports();
        }
        else
        {
        const Difficulty diff = p->GetMap() ? p->GetMap()->GetDifficultyID() : DIFFICULTY_NONE;
        for (uint32 sid : snap->spellbook.known_spells)
        {
            SpellInfo const* si = sSpellMgr->GetSpellInfo(sid, diff);
            if (!si) continue;
            bool is_craft = false;
            uint32 tele_dest_map = 0;
            uint32 tele_effect_index = 0;
            for (SpellEffectInfo const& eff : si->GetEffects())
            {
                if (!is_craft &&
                    (eff.Effect == SPELL_EFFECT_CREATE_ITEM ||
                     eff.Effect == SPELL_EFFECT_CREATE_LOOT ||
                     eff.Effect == SPELL_EFFECT_CREATE_RANDOM_ITEM))
                {
                    is_craft = true;
                }
                if (tele_dest_map == 0 &&
                    (eff.Effect == SPELL_EFFECT_TELEPORT_UNITS ||
                     eff.Effect == SPELL_EFFECT_TELEPORT_WITH_SPELL_VISUAL_KIT_LOADING_SCREEN))
                {
                    if (SpellTargetPosition const* tp =
                            sSpellMgr->GetSpellTargetPosition(sid, eff.EffectIndex))
                    {
                        tele_dest_map = tp->GetMapId();
                        tele_effect_index = eff.EffectIndex;
                        (void)tele_effect_index;  // diagnostic-only
                    }
                }
                // Early exit if both classifications resolved.
                if (is_craft && tele_dest_map != 0) break;
            }
            if (is_craft) snap->spellbook.known_recipes.push_back(sid);
            if (tele_dest_map != 0)
                snap->travel.self_teleport_spells.push_back({sid, tele_dest_map});
        }
        // Stamp cache so subsequent snapshots within 30s reuse this result.
        if (sc_ai)
        {
            sc_ai->set_last_spellcache_scan_ms(sc_now);
            sc_ai->mutable_cached_recipes()         = snap->spellbook.known_recipes;
            sc_ai->mutable_cached_self_teleports()  = snap->travel.self_teleport_spells;
        }
        }
    }

    // ---- Talents + Glyphs on the active spec ----
    // 12.0 trait system: applied talents live in the ACTIVE UF::TraitConfig
    // (entries with Rank/GrantedRanks > 0), NOT in the legacy PlayerTalentMap
    // — core trait application (ApplyTraitEntry) calls LearnSpell and never
    // writes AddTalent, so GetTalentMap is structurally EMPTY on 12.0. The
    // old read made active_talents always empty: diagnostics showed
    // "0 talents" for every bot AND the idle talent rules' un-latch signal
    // (!active_talents.empty()) never fired, so failed talent commits were
    // never retried (audit B07/B19). Surface the trait-node-entry ids with
    // purchased/granted ranks instead; consumers only test non-emptiness and
    // membership, both of which keep working.
    {
        const uint8 active_group = p->GetActiveTalentGroup();
        int32 const active_cfg_id = p->m_activePlayerData->ActiveCombatTraitConfigID;
        if (UF::TraitConfig const* tcfg = p->GetTraitConfig(active_cfg_id))
        {
            snap->spellbook.active_talents.reserve(tcfg->Entries.size());
            for (auto const& te : tcfg->Entries)
            {
                if (te.Rank + te.GrantedRanks <= 0) continue;
                snap->spellbook.active_talents.push_back(uint32(te.TraitNodeEntryID));
            }
            std::sort(snap->spellbook.active_talents.begin(), snap->spellbook.active_talents.end());
        }
        std::vector<uint32> const& glyphs = p->GetGlyphs(active_group);
        snap->spellbook.active_glyphs.reserve(glyphs.size());
        for (uint32 g : glyphs)
            if (g) snap->spellbook.active_glyphs.push_back(g);
    }

    // ---- Trait config: is the active combat config Blizzard's StarterBuild? ----
    // Used by State_Idle's auto-extend rule: when bot dings to a level with new
    // trait points, re-fire ApplyStarterTalents only if the bot is still on the
    // starter build (don't wipe a custom build the owner picked). The flag
    // survives respec; only switching builds clears it.
    {
        int32 const active_id = p->m_activePlayerData->ActiveCombatTraitConfigID;
        if (UF::TraitConfig const* cfg = p->GetTraitConfig(active_id))
            snap->spellbook.is_starter_build = (cfg->CombatConfigFlags
                                      & int32(TraitCombatConfigFlags::StarterBuild)) != 0;
    }

    // ---- Skills (professions / weapons / languages / armour) ----
    // 256-slot probe; ~10-20 hits per bot. Per-bot 1Hz cache so the
    // 256 calls don't fire every Build for every bot at 2000-bot scale.
    // Skills mutate on level-up / train-spell / first-kill events,
    // never per-tick — 1Hz is plenty.
    {
        BotAI* sk_ai = bot_ai;
        const uint32 sk_now = GameTime::GetGameTimeMS();
        constexpr uint32 kSkillsScanIntervalMs = 1000u;
        const bool sk_throttled = sk_ai &&
            sk_ai->last_skills_scan_ms() != 0 &&
            (sk_now - sk_ai->last_skills_scan_ms()) < kSkillsScanIntervalMs;
        if (sk_throttled)
        {
            snap->progression.skills = sk_ai->cached_skills();
        }
        else
        {
            for (uint32 pos = 0; pos < 256; ++pos)
            {
                uint16 sid = p->GetSkillLineIdByPos(pos);
                if (!sid) continue;
                BotSnapshot::SkillEntry e{};
                e.skill_id = sid;
                e.value    = p->GetSkillRankByPos(pos);
                e.max      = p->GetSkillMaxRankByPos(pos);
                snap->progression.skills.push_back(e);
            }
            if (sk_ai)
            {
                sk_ai->mutable_cached_skills() = snap->progression.skills;
                sk_ai->set_last_skills_scan_ms(sk_now);
            }
        }
    }

    // ---- Currency wallet ----
    // Currency walk: probe sCurrencyTypesStore (~700 rows) for non-zero
    // bot quantities. Per-bot 1Hz throttle via BotAI::last_currency_scan_ms_
    // — currencies only change on quest turn-in / vendor / loot, so 1Hz
    // refresh is plenty. When throttled, the snapshot is populated from
    // the per-bot cache so consumers (whisper /currency, badge-vendor
    // rules) always see data, just up-to-1s stale.
    {
        BotAI* cu_ai = bot_ai;
        const uint32 cu_now = GameTime::GetGameTimeMS();
        constexpr uint32 kCurrencyScanIntervalMs = 1000u;
        const bool cu_throttled = cu_ai &&
            cu_ai->last_currency_scan_ms() != 0 &&
            (cu_now - cu_ai->last_currency_scan_ms()) < kCurrencyScanIntervalMs;
        if (cu_throttled)
        {
            // Re-publish cached entries — consumers see consistent data.
            snap->progression.currencies = cu_ai->cached_currencies();
        }
        else
        {
            for (uint32 cid = 0; cid < sCurrencyTypesStore.GetNumRows(); ++cid)
            {
                CurrencyTypesEntry const* ct = sCurrencyTypesStore.LookupEntry(cid);
                if (!ct) continue;
                const uint32 q = p->GetCurrencyQuantity(ct->ID);
                if (q == 0) continue;
                BotSnapshot::CurrencyEntry e{};
                e.currency_id = ct->ID;
                e.quantity    = q;
                snap->progression.currencies.push_back(e);
            }
            if (cu_ai)
            {
                cu_ai->mutable_cached_currencies() = snap->progression.currencies;
                cu_ai->set_last_currency_scan_ms(cu_now);
            }
        }
    }

    // ---- Reputations ----
    // Same per-bot 1Hz cache pattern as currencies. ReputationMgr's
    // state list often carries 100-300 factions; each entry costs a
    // sFactionStore lookup + two ReputationMgr calls. Refresh at 1Hz
    // and copy from cache on throttled ticks so /rep whisper / rep-
    // gated rules see stable data.
    {
        BotAI* rep_ai = bot_ai;
        const uint32 rep_now = GameTime::GetGameTimeMS();
        constexpr uint32 kRepScanIntervalMs = 1000u;
        const bool rep_throttled = rep_ai &&
            rep_ai->last_reputation_scan_ms() != 0 &&
            (rep_now - rep_ai->last_reputation_scan_ms()) < kRepScanIntervalMs;
        if (rep_throttled)
        {
            snap->progression.reputations = rep_ai->cached_reputations();
        }
        else if (ReputationMgr const* rm = &p->GetReputationMgr())
        {
            for (auto const& [rep_id, state] : rm->GetStateList())
            {
                FactionEntry const* fe = sFactionStore.LookupEntry(state.ID);
                if (!fe) continue;
                BotSnapshot::ReputationEntry e{};
                e.faction_id = state.ID;
                e.standing   = rm->GetReputation(fe);
                e.rank       = static_cast<uint8>(rm->GetRank(fe));
                snap->progression.reputations.push_back(e);
            }
            if (rep_ai)
            {
                rep_ai->mutable_cached_reputations() = snap->progression.reputations;
                rep_ai->set_last_reputation_scan_ms(rep_now);
            }
        }
    }

    // ---- Cooldowns + GCD ----
    // GCD: SpellHistory tracks GCD per category (most player spells share
    // category 133). Probe through known spells; first one with an active
    // GCD yields the remaining time. Used by `is_ready()` to block double-
    // casts in the same tick.
    if (SpellHistory const* hist = p->GetSpellHistory())
    {
        snap->cooldowns.gcd_remaining = Ms{0};
        const Difficulty diff = p->GetMap() ? p->GetMap()->GetDifficultyID() : DIFFICULTY_NONE;
        BotAI* gcd_ai = bot_ai;
        bool resolved = false;
        // Fast path (perf 0.3): GCD is global/category-based, so a single
        // representative known spell answers the question in O(1) — no need
        // to walk the whole spellbook. Probe the cached id first; it's
        // resolved lazily by the walk below and invalidated on respec
        // (cleared alongside the known-spells cache). This path runs in ALL
        // states so an out-of-combat bot that just finished a cast still
        // reports a lingering GCD (is_ready() relies on it).
        const uint32 probe = gcd_ai ? gcd_ai->cached_gcd_probe_spell() : 0;
        if (probe != 0)
        {
            if (SpellInfo const* si = sSpellMgr->GetSpellInfo(probe, diff))
            {
                if (hist->HasGlobalCooldown(si))
                {
                    const auto rem = hist->GetRemainingGlobalCooldown(si);
                    const auto rem_ms = std::chrono::duration_cast<std::chrono::milliseconds>(rem);
                    // Only short-circuit on a positive remaining time — matching
                    // the original loop's `>0` break. A zero reading falls
                    // through so a different-category GCD spell is still found.
                    if (rem_ms.count() > 0)
                    {
                        snap->cooldowns.gcd_remaining = Ms{rem_ms.count()};
                        resolved = true;
                    }
                }
            }
        }
        // Slow path: full spellbook walk (the original behavior). Only run it
        // when the bot is in combat or mid-cast — the only states in which a
        // GCD can be active. Out of combat with nothing casting and no cached
        // probe hit, the old walk touched every spell and found nothing; we
        // skip it and leave the field at 0 (its correct value). When we do
        // walk, cache the first GCD-bearing spell id so subsequent builds use
        // the O(1) fast path above.
        const bool casting =
            p->GetCurrentSpell(CURRENT_GENERIC_SPELL)   != nullptr ||
            p->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr ||
            p->GetCurrentSpell(CURRENT_MELEE_SPELL)     != nullptr;
        if (!resolved && (snap->vitals.in_combat || casting))
        {
            for (uint32 spell_id : snap->spellbook.known_spells)
            {
                SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, diff);
                if (!si) continue;
                if (!hist->HasGlobalCooldown(si)) continue;
                if (gcd_ai) gcd_ai->set_cached_gcd_probe_spell(spell_id);
                const auto rem = hist->GetRemainingGlobalCooldown(si);
                const auto rem_ms = std::chrono::duration_cast<std::chrono::milliseconds>(rem);
                if (rem_ms.count() > 0) { snap->cooldowns.gcd_remaining = Ms{rem_ms.count()}; break; }
            }
        }
    }
    CopyCooldowns(p, snap->cooldowns.spell_cooldowns);
    // Populate the O(1) lookup index. spell_cooldowns is already
    // de-duplicated by spell_id inside CopyCooldowns, so each insertion
    // is unique. Reserve sizing avoids re-hashing during the loop.
    // Tier 3.3: sorted flat vector + binary search (was unordered_map).
    snap->cooldowns.spell_cooldowns_index.clear();
    snap->cooldowns.spell_cooldowns_index.reserve(snap->cooldowns.spell_cooldowns.size());
    for (uint32 i = 0; i < snap->cooldowns.spell_cooldowns.size(); ++i)
        snap->cooldowns.spell_cooldowns_index.push(
            snap->cooldowns.spell_cooldowns[i].spell_id, i);
    snap->cooldowns.spell_cooldowns_index.finalize();

    // ---- Bot's own active cast ----
    // Probe the three current-spell slots; CURRENT_GENERIC_SPELL handles the
    // common cast-time spells, and CHANNELED catches Mind Flay / Penance / etc.
    for (uint32 slot : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
    {
        if (Spell* casting = p->GetCurrentSpell(static_cast<CurrentSpellTypes>(slot)))
        {
            snap->cast.is_casting           = true;
            snap->cast.current_cast_spell_id = casting->m_spellInfo->Id;
            const int32 rem            = casting->GetRemainingCastTime();
            snap->cast.current_cast_remaining = Ms{rem > 0 ? rem : 0};
            snap->cast.current_cast_target   = casting->m_targets.GetUnitTargetGUID();
            break;
        }
    }
    // Mirror the most-recent successful cast from BotAI for predicates
    // that need to enforce non-repetition (Monk Windwalker Combo
    // Strikes mastery — repeating same melee ability breaks Mastery).
    if (BotAI* lc_ai = bot_ai)
        snap->cast.last_cast_spell_id = lc_ai->last_cast_spell_id();

    // ---- Attackers (units currently aggro'd onto us) ----
    auto const& attackers = p->getAttackers();
    snap->combat.attackers.reserve(attackers.size());
    for (Unit* a : attackers)
    {
        if (!a) continue;
        // A FRIENDLY in m_attackers is always wedged state — Unit::Attack()
        // never validates hostility, so a confused unit (2026-06-11: a
        // groupmate's hunter pet mid friendly-fire cascade) can sit in the
        // list. Surfacing it would have counterattack pickers
        // (highest_threat_attacker, under-attack engage) re-target the
        // friendly and re-seed the cascade. Hostile-but-currently-
        // unattackable attackers (immunity phases) stay visible so
        // defensive rules still see the threat.
        if (a->IsFriendlyTo(p)) continue;
        NearbyUnit u{};
        u.guid     = a->GetGUID();
        u.entry    = a->GetEntry();
        // Per-viewer SCALED level: the level the BOT actually fights this unit
        // at. Creature::GetLevelForTarget clamps a scaling mob into its band by
        // the viewer's level; raw GetLevel() returns the band TOP, so bots
        // misjudged every scaling mob (12.0 ContentTuning). See
        // docs/AUDIT_DYNAMIC_LEVEL_SCALING_20260614.md.
        u.level    = a->GetLevelForTarget(p);
        u.hp       = static_cast<int32>(a->GetHealth());
        u.max_hp   = static_cast<int32>(a->GetMaxHealth());
        u.x        = a->GetPositionX();
        u.y        = a->GetPositionY();
        u.z        = a->GetPositionZ();
        u.o        = a->GetOrientation();
        Spell* casting = a->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!casting) casting = a->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (casting)
        {
            u.is_casting       = true;
            u.casting_spell_id = casting->m_spellInfo->Id;
            // CanBeInterrupted(actor, target) — bot is interrupter, attacker is target.
            // Old check (SPELL_ATTR0_NO_AURA_CANCEL) was for buff right-click removal,
            // not kicks; it returned true for nearly every cast and made kick rules fire
            // on uninterruptible boss casts.
            u.is_interruptible = casting->m_spellInfo->CanBeInterrupted(p, a, false);
            const int32 rem    = casting->GetRemainingCastTime();
            u.cast_remaining   = Ms{rem > 0 ? rem : 0};
        }
        if (Unit const* v = a->GetVictim())
            u.victim = v->GetGUID();
        if (a->GetTypeId() == TYPEID_PLAYER)
            u.is_player = true;
        // Creature type for CC target-mask gating. PickOffTargetCC picks its
        // PvE crowd-control target from the ATTACKER list, and CanBeCCd needs
        // the creature type to reject mask-violating CC (Polymorph on Undead,
        // etc.). nearby_enemies already carries this; attackers did not, so
        // an attacker-sourced CC target read type 0 (= treated as a player,
        // unmasked) and could re-introduce the A6 BAD_TARGETS spam. Players
        // legitimately stay 0 (PvP CC is DR-governed, not type-masked).
        if (Creature const* ac = a->ToCreature())
        {
            // Mirror no_xp_kill / is_pacified from nearby_enemies so
            // State_InCombat's skip_target filter can reject environmental
            // hazards that enter m_attackers via aura damage (e.g. Glubtok
            // Firewall Platters — encounter mechanics the bot can't kill).
            u.no_xp_kill = !ac->CanGiveExperience();
            u.is_pacified = ac->HasUnitFlag(UNIT_FLAG_PACIFIED);
            // Uninteractible (NOT_SELECTABLE) attackers can never be acquired as a
            // victim (IsValidAttackTarget rejects them). Mark so the self-acquire
            // and the in-combat boss-advance treat them as un-fightable — the bot
            // advances through the swarm instead of standing to die. See NearbyUnit.
            u.untargetable = ac->IsUninteractible();
            // Mob's own chase generator reports it cannot path to its target
            // (aggro-but-unreachable window — WC z-disconnected ledge pack).
            // The combat re-aim gates skip chasing these. See NearbyUnit.
            u.cannot_reach = ac->CanNotReachTarget();
            if (CreatureTemplate const* act = ac->GetCreatureTemplate())
            {
                u.creature_type = uint8(act->type);
                if (act->unit_flags & UNIT_FLAG_PACIFIED)
                    u.is_pacified = true;
                if (act->unit_flags & UNIT_FLAG_UNINTERACTIBLE)
                    u.untargetable = true;
            }
        }
        // Attacker-side CC scan — same shape as nearby_enemies. Without
        // this, attackers always read is_cc_locked=false even when the
        // group has already CC'd them, leading to bot DPS breaking poly
        // / sap on attackers held by group CC. Affix-buff propagation is
        // also valuable so M+ Bolstering on an active attacker pushes
        // the kite-affix rule.
        {
            static constexpr uint32 kAffixBuffIds[] = { 209859, 228318 };
            int best_cc_pri = -1;
            for (auto const& [aid, aapp] : a->GetAppliedAuras())
            {
                if (!aapp) continue;
                Aura const* abase = aapp->GetBase();
                if (!abase) continue;
                SpellInfo const* asi = abase->GetSpellInfo();
                if (!asi) continue;
                if (aapp->IsPositive())
                {
                    for (uint32 wid : kAffixBuffIds)
                        if (asi->Id == wid)
                        {
                            u.affix_buffs.push_back(wid);
                            break;
                        }
                    continue;
                }
                int pri = -1;
                switch (asi->Mechanic)
                {
                    case 12: pri = 8; break;
                    case 10: pri = 7; break;
                    case  5: pri = 6; break;
                    case 16: pri = 5; break;
                    case 14: pri = 4; break;
                    case  6: pri = 3; break;
                    case 24: pri = 2; break;
                    case  1: pri = 1; break;
                    default: continue;
                }
                if (pri > best_cc_pri)
                {
                    best_cc_pri = pri;
                    u.is_cc_locked = true;
                    u.cc_caster = abase->GetCasterGUID();
                }
            }
        }
        // Honest fightable-attacker tally (stalker-free): count this attacker
        // only when it can actually be fought — targetable, not pacified, alive.
        // Combat-density / pull-segmentation / AoE gates read fightable_attackers
        // instead of attackers.size() so the untargetable trigger flood (the 56
        // static Deadmines "Vanessa Lightning Stalker" 49521, 8-12 stacked at the
        // harbor) can neither jam a pull gate nor mis-fire AoE/panic. The raw
        // attackers vector keeps EVERY entry (the (0c) all-untargetable wedge
        // test + the ghost-heal both still need the untargetable records).
        if (!u.untargetable && !u.is_pacified && u.hp > 0)
            ++snap->combat.fightable_attackers;
        snap->combat.attackers.push_back(u);
    }
    // Sort: attackers currently targeting *us* come first (those generate the
    // most pressure), then by max_hp descending (biggest threat = biggest mob).
    // BotSnapshotView::highest_threat_attacker() takes attackers.front().
    {
        const ObjectGuid self = snap->guid;
        std::sort(snap->combat.attackers.begin(), snap->combat.attackers.end(),
                  [self](NearbyUnit const& a, NearbyUnit const& b)
                  {
                      const bool a_on_me = (a.victim == self);
                      const bool b_on_me = (b.victim == self);
                      if (a_on_me != b_on_me) return a_on_me;
                      return a.max_hp > b.max_hp;
                  });
    }

    // ---- Encounter detection ----
    // Heuristic: a boss-tier attacker (max_hp >= 5M) is the active encounter.
    // Lets APLs gate boss-only cooldowns (e.g. major raid CDs) on a single
    // field without each rule reimplementing the threshold check.
    {
        constexpr int32 BOSS_HP_THRESHOLD = 5'000'000;
        for (auto const& a : snap->combat.attackers)
        {
            if (a.max_hp >= BOSS_HP_THRESHOLD)
            {
                snap->dungeon_exec.active_encounter_npc_id = a.entry;
                break;
            }
        }
    }

    // ---- Dungeon execution context (Phase A of GROUP_DUNGEON_PLAN.md) ----
    // Drives idle:tank_pull, idle:dungeon_interrupt, wipe-recovery, and
    // run-completion rules. All fields zero / Empty when bot isn't in
    // an instance map.
    if (Map* mp = p->GetMap())
    {
        if (InstanceMap* im = mp->ToInstanceMap())
        {
            if (InstanceScript* is = im->GetInstanceScript())
            {
                snap->dungeon_exec.is_encounter_in_progress = is->IsEncounterInProgress();
                // Final-boss-DONE detection. The instance script tracks
                // a vector of BossInfo; GetEncounterCount() returns the
                // exact count. Run complete if EITHER
                //   (a) every decided boss is DONE (full clear), OR
                //   (b) the highest-index encounter is DONE — by convention
                //       the final boss. Covers raids where intermediate
                //       optional bosses are skipped (ICC gunship→Saurfang→
                //       Lich King without the wing detours, etc.).
                bool any_boss_known = false;
                bool all_done       = true;
                uint8 done_count    = 0;
                uint8 decided_count = 0;
                bool any_special    = false;
                const uint32 enc_count = is->GetEncounterCount();
                for (uint32 i = 0; i < enc_count; ++i)
                {
                    EncounterState st = is->GetBossState(i);
                    if (st == TO_BE_DECIDED) continue;
                    any_boss_known = true;
                    if (decided_count < 0xFFu) ++decided_count;
                    if (st != DONE) { all_done = false; }
                    if (st == DONE && done_count < 0xFFu) ++done_count;
                    if (st == SPECIAL) any_special = true;
                }
                snap->dungeon_exec.bosses_done_count  = done_count;
                snap->dungeon_exec.bosses_total_count = uint8(std::min<uint32>(enc_count, 0xFFu));
                snap->dungeon_exec.any_boss_in_special = any_special;
                bool final_done = enc_count > 0 &&
                                  is->GetBossState(enc_count - 1) == DONE;
                // 5-man DUNGEONS complete on FULL CLEAR only (owner
                // directive 2026-06-12: "all bosses should be killed").
                // The final-boss shortcut stays for RAIDS, where optional
                // wing bosses (ICC, Ulduar) would otherwise hold
                // "complete" hostage forever. Trash-proximity routing can
                // reach the final boss before an optional-path one
                // (Stockades: Hogger before Lord Overheat) — under
                // full-clear semantics the tank just keeps advancing to
                // the remaining bosses[] entries afterward.
                // Event-summoned bosses (Skyriss, Baron Rivendare, Urok
                // Doomhowl, …) have no static spawn, so their encounter never
                // leaves NOT_STARTED for a clientless bot squad — under strict
                // full-clear semantics the run would read incomplete forever
                // and the squad would never auto-leave. Exclude them: the 5-man
                // is a full clear once at most `phantom_k` encounters remain
                // unfinished and the dungeon script has declared exactly that
                // many bosses as unspawnable. phantom_k == 0 reduces this to the
                // original all_done gate. Cheap O(1) registry read — no per-tick
                // GetAdvice() churn (the builder deliberately avoids GetAdvice).
                uint8 phantom_k = 0;
                if (DungeonScript const* ds = Services::Dungeons().GetScriptFor(
                        p->GetMapId(), uint32(snap->instance_ctx.map_difficulty)))
                    phantom_k = uint8(std::min<size_t>(
                        ds->event_summoned_bosses().size(), size_t{0xFFu}));
                const uint8 not_done = uint8(decided_count - done_count);

                const bool is_raid_map = p->GetMap() && p->GetMap()->IsRaid();
                const bool full_clear = any_boss_known && (not_done <= phantom_k);
                snap->dungeon_exec.dungeon_complete =
                    full_clear || (is_raid_map && final_done);
            }
            // Members dead on this map — drives wipe detection (3+ dead
            // while encounter active is the canonical wipe signal). Walks
            // the bot's group on the bot's map and counts hp <= 0. Solo /
            // ungrouped bots stay at 0.
            if (Group const* gr = p->GetGroup())
            {
                uint8 dead = 0;
                uint32 const my_map = p->GetMapId();
                for (GroupReference const& ref : gr->GetMembers())
                {
                    Player const* m = ref.GetSource();
                    if (!m) continue;
                    if (m->GetMapId() != my_map) continue;
                    if (m->IsAlive()) continue;
                    if (++dead == 0xFFu) break;   // saturate
                }
                snap->dungeon_exec.members_dead_count = dead;
            }
            // Instance entrance position — pos to regroup at after a wipe.
            // Map::GetEntrancePos(int32&, float&, float&) returns the
            // canonical entrance map+xy; z is not surfaced by TC's API so
            // we leave it 0 — the consumer (wipe-regroup move_to) lets the
            // path-validator's UpdateAllowedPositionZ snap to terrain.
            // Returns false when no entrance is set (raid attunement bosses
            // etc.); fields stay 0 in that case.
            {
                int32 entry_mapid = 0;
                float entry_x = 0.f, entry_y = 0.f;
                if (im->GetEntrancePos(entry_mapid, entry_x, entry_y))
                {
                    snap->dungeon_exec.instance_entrance_map = uint32(entry_mapid);
                    snap->dungeon_exec.instance_entrance_x   = entry_x;
                    snap->dungeon_exec.instance_entrance_y   = entry_y;
                    snap->dungeon_exec.instance_entrance_z   = 0.f;
                }
            }
            // IN-INSTANCE entrance (audit B36): capture the bot's first
            // observed position in THIS instance run — the parent-map
            // GetEntrancePos above is the outdoor ghost-port location and
            // can never equal the instance map, which made the wipe-regroup
            // rule unreachable since it shipped. Keyed by instance id so a
            // fresh run (new lockout) re-captures.
            {
                const uint32 iid = p->GetMap()->GetInstanceId();
                if (BotAI* ie_ai = bot_ai)
                {
                    if (ie_ai->inside_entrance_instance() != iid)
                        ie_ai->set_inside_entrance(iid,
                            p->GetPositionX(), p->GetPositionY(), p->GetPositionZ());
                    snap->dungeon_exec.inside_entrance_map = p->GetMapId();
                    snap->dungeon_exec.inside_entrance_x   = ie_ai->inside_entrance_x();
                    snap->dungeon_exec.inside_entrance_y   = ie_ai->inside_entrance_y();
                    snap->dungeon_exec.inside_entrance_z   = ie_ai->inside_entrance_z();
                }
            }
            // ---- Scenario step tracking ----
            // Modern WoW scenarios are short, story-driven instance maps
            // (e.g. challenge modes, leveling story scenarios). Each has
            // a current ScenarioStep with one or more sub-objectives
            // (criteria-tree leaves). We surface the active step id +
            // step ordering so future scenario-aware dispatch can gate
            // behavior. ScenarioStepEntry::OrderIndex is the 0-based
            // step index; it doubles as a coarse "progress" indicator
            // (criteria detail lives behind a private CriteriaProgress
            // map we don't surface).
            if (mp->IsScenario())
            {
                if (InstanceScenario const* sc = im->GetInstanceScenario())
                {
                    if (ScenarioEntry const* se = sc->GetEntry())
                        snap->quest_log.scenario_step.scenario_id = se->ID;
                    if (ScenarioStepEntry const* st = sc->GetStep())
                    {
                        snap->quest_log.scenario_step.step_id = st->ID;
                        // Use OrderIndex as a coarse progress indicator —
                        // the public Scenario API doesn't expose criteria
                        // counts, so this is the best we can do without
                        // touching protected members.
                        snap->quest_log.scenario_step.current_step_progress =
                            static_cast<uint16>(st->OrderIndex);
                    }
                }
            }
        }
    }

    // Closest IsDungeonBoss enemy in nearby_enemies — drives the boss-
    // engagement and interrupt rules without each rule walking the
    // vector. We pick the first match (nearby_enemies is sorted by
    // squared distance, so this is also the closest).
    for (auto const& u : snap->combat.nearby_enemies)
    {
        if (!u.is_dungeon_boss) continue;
        if (u.hp <= 0) continue;
        snap->dungeon_exec.current_boss_guid                    = u.guid;
        snap->dungeon_exec.current_boss_entry                   = u.entry;
        snap->dungeon_exec.current_boss_hp                      = u.hp;
        snap->dungeon_exec.current_boss_max_hp                  = u.max_hp;
        snap->dungeon_exec.current_boss_casting_spell           = u.casting_spell_id;
        snap->dungeon_exec.current_boss_casting_interruptible   = u.is_interruptible;
        snap->dungeon_exec.current_boss_cast_remaining          = u.cast_remaining;
        break;
    }

    // ---- Auras: own + on victim/target ----
    // CopyAuras walks the unit's aura table (typically 10-30 entries
    // with mechanics + dispel + caster resolution per aura) — each call
    // is ~5-50 µs. When victim and current_target are the same unit
    // (extremely common — bot auto-attacking the same NPC it has
    // selected), the naive "two calls" approach walks the same unit
    // twice and produces identical vectors. Walk once, share vector.
    CopyAuras(p, snap->auras.own_auras);
    // Build the own_auras O(1) lookup index. Most rotations call
    // find_aura/has_aura for the bot's own buffs 20+ times per tick.
    // Tier 3.3: sorted flat vector + binary search (was unordered_map). Push
    // order is preserved among equal spell_ids by finalize()'s stable sort, so
    // the FIRST index wins — matching the prior emplace "first wins".
    snap->auras.own_auras_index.clear();
    snap->auras.own_auras_index.reserve(snap->auras.own_auras.size());
    for (uint32 i = 0; i < snap->auras.own_auras.size(); ++i)
        snap->auras.own_auras_index.push(snap->auras.own_auras[i].spell_id, i);
    snap->auras.own_auras_index.finalize();

    Unit* victim = p->GetVictim();
    Unit* tu = nullptr;
    if (!snap->combat.current_target.IsEmpty() && snap->combat.current_target != snap->guid)
        tu = ObjectAccessor::GetUnit(*p, snap->combat.current_target);

    if (victim)
        CopyAuras(victim, snap->auras.victim_auras);
    if (tu)
    {
        if (tu == victim)
            // Same unit — copy the vector (trivially-copyable AuraEntry,
            // O(N) memcpy) instead of walking the unit's aura table again.
            snap->auras.target_auras = snap->auras.victim_auras;
        else
            CopyAuras(tu, snap->auras.target_auras);
    }

    // ---- Outbound auras (HoTs / buffs THIS bot has on others) ----
    // Walk group members + (if present) the bot's pet, recording any aura
    // applied by this bot. Used by find_aura(spell, other_guid) so HoT
    // refresh predicates stop re-casting every tick.
    // Cost optimisation: healers maintain rolling HoTs on group members.
    // Augmentation Evoker is a support DPS that buffs the tank (Blistering
    // Scales, Ebon Might, etc.) and needs the same find_aura coverage —
    // without the scan, those buffs re-cast every tick on cooldown.
    // Tanks/melee DPS still get the pet scan (Hunter Mend Pet, etc.).
    const bool is_aug_evoker = (snap->identity.cls == CLASS_EVOKER && snap->identity.spec == 1473);
    const bool needs_group_outbound_scan =
        (snap->group.my_role == Role::Healer) || is_aug_evoker;
    // Multi-dot specs need outbound scan over nearby enemies too — the
    // `enemy_without_my_aura()` helper queries my_auras_on_others to decide
    // whether to expand a DoT to a new target. Without this scan, those
    // specs can't tell which adds already carry their bleed/curse and they
    // spam-recast on the same enemy. Keep the spec list narrow: the scan is
    // O(nearby_enemies × applied_auras_per_enemy), so we don't enable it for
    // burst/melee specs that have no use for it.
    auto needs_enemy_outbound_scan = [&]() -> bool
    {
        switch (snap->identity.cls)
        {
            case CLASS_WARLOCK:
                // Affliction (265) maintains 3 simultaneous DoTs across
                // targets; Demonology (266) cycles Doom on multiple adds.
                return snap->identity.spec == 265 || snap->identity.spec == 266;
            case CLASS_PRIEST:
                // Shadow (258) maintains SW:P + Vampiric Touch on multi.
                return snap->identity.spec == 258;
            case CLASS_DRUID:
                // Balance (102) wants Moonfire + Sunfire on every enemy.
                // Feral (103) cycles Rake bleed across adds.
                return snap->identity.spec == 102 || snap->identity.spec == 103;
            case CLASS_DEATH_KNIGHT:
                // Unholy (252) spreads Virulent Plague via Outbreak; the
                // multi-target query helps detect when to recast.
                return snap->identity.spec == 252;
            case CLASS_HUNTER:
                // Survival (255) maintains Serpent Sting on every add.
                return snap->identity.spec == 255;
            default:
                return false;
        }
    };
    const bool enemy_outbound_scan = needs_enemy_outbound_scan();
    auto record_outbound_for = [&](Unit const* tgt)
    {
        if (!tgt) return;
        const ObjectGuid me = p->GetGUID();
        for (auto const& [spell_id, app] : tgt->GetAppliedAuras())
        {
            if (!app) continue;
            Aura const* base = app->GetBase();
            if (!base) continue;
            if (base->GetCasterGUID() != me) continue;
            BotSnapshot::OutboundAura o{};
            o.target = tgt->GetGUID();
            o.spell_id = spell_id;
            const int32 dur = base->GetDuration();
            // Permanent buffs (Beacon of Light) report INT32_MAX so refresh
            // checks (`remaining <= N`) don't trigger phantom re-casts.
            o.remaining = base->IsPermanent() ? Ms{std::numeric_limits<int32>::max()}
                        : (dur > 0 ? Ms{dur} : Ms{0});
            o.stacks = base->GetStackAmount();
            snap->auras.my_auras_on_others.push_back(o);
        }
    };
    if (needs_group_outbound_scan)
    {
        if (Group* g = p->GetGroup())
        {
            for (auto const& slot : g->GetMemberSlots())
            {
                if (slot.guid == p->GetGUID()) continue;
                if (Player* mp = ObjectAccessor::FindConnectedPlayer(slot.guid))
                    record_outbound_for(mp);
            }
        }
    }
    if (Pet* pet = p->GetPet())
        record_outbound_for(pet);

    // Build the composite-key index for my_auras_on_others. Speeds up
    // enemy_without_my_aura() (a per-DoT-spec hot path) from O(N) to
    // O(1). Key = (target_guid_raw ^ rotl(spell_id,16)).
    // Tier 3.3: sorted flat vector + binary search (was unordered_map).
    snap->auras.my_auras_on_others_index.clear();
    snap->auras.my_auras_on_others_index.reserve(snap->auras.my_auras_on_others.size());
    for (uint32 i = 0; i < snap->auras.my_auras_on_others.size(); ++i)
    {
        auto const& o = snap->auras.my_auras_on_others[i];
        // GetCounter() returns the unique low-bits part of ObjectGuid;
        // sufficient as a per-entity key inside one snapshot. Composite
        // key (target_counter << 32) | spell_id — MUST match the lookup key
        // composition in BotSnapshotView (find_aura / enemy_without_my_aura).
        const uint64 key = (uint64(o.target.GetCounter()) << 32)
            | uint64(o.spell_id);
        snap->auras.my_auras_on_others_index.push(key, i);
    }
    snap->auras.my_auras_on_others_index.finalize();

    // ---- Tier 1.1 (behavior-preserving): merged enemy+friend 40y scan ----
    // Previously this was TWO separate 40y Cell::VisitAllObjects passes (one
    // with AnyUnfriendlyUnitInObjectRangeCheck for nearby_enemies, one with
    // AnyFriendlyUnitInObjectRangeCheck for nearby_friends), each walking the
    // identical set of grid cells (both radius 40.0f). We collapse them into a
    // SINGLE grid visit that collects every unit in those cells once, then
    // partition each unit into the enemy or friend scratch by applying the
    // EXACT same two check operators that the original searchers used. Because
    // (a) the cell set walked is identical (same center, same 40y radius), (b)
    // the only framework pre-filter is InSamePhase (same p->GetPhaseShift()),
    // and (c) the per-unit acceptance predicate is the unchanged check
    // operator(), each scratch ends up with identical membership AND identical
    // searcher-insertion order to the two original passes. The downstream
    // dist-sort/caps/NearbyUnit construction is left byte-for-byte unchanged,
    // so the published nearby_enemies/nearby_friends are identical.
    //
    // NOTE: the two checks are NOT strict complements — the friend check uses a
    // vertical-cylinder test extended by combat reaches while the enemy check
    // uses IsWithinDist; a unit can be accepted by one, the other, or neither.
    // We therefore collect ALL units (accept-all check) and re-apply both real
    // checks per unit rather than assuming partition-by-friendliness.
    {
        constexpr float SCAN_RADIUS = 40.0f;
        thread_local std::vector<Unit*> scratch_all;
        scratch_all.clear();
        scratch_all.reserve(128);
        // Accept-all check: every unit in the visited cells is collected, then
        // filtered below by the original per-pass checks. Must NOT impose any
        // range/alive filter of its own or it would prune units the original
        // friend check (extended radius) would have kept.
        struct AcceptAllUnitCheck { bool operator()(Unit*) const { return true; } };
        AcceptAllUnitCheck all_check;
        Trinity::UnitListSearcher<AcceptAllUnitCheck> all_searcher(p, scratch_all, all_check);
        Cell::VisitAllObjects(p, all_searcher, SCAN_RADIUS);

        // Re-apply the two ORIGINAL check operators to partition. Same objects,
        // same parameters as the pre-merge searchers — membership + order kept.
        scratch_enemies_tl().clear();
        scratch_enemies_tl().reserve(64);
        scratch_friends_tl().clear();
        scratch_friends_tl().reserve(64);
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(p, p, SCAN_RADIUS);
        Trinity::AnyFriendlyUnitInObjectRangeCheck    f_check(p, p, SCAN_RADIUS, /*playerOnly*/ false);
        for (Unit* t : scratch_all)
        {
            if (u_check(t)) scratch_enemies_tl().push_back(t);
            if (f_check(t)) scratch_friends_tl().push_back(t);
        }
    }

    // ---- Nearby enemies (40yd, capped) ----
    // Snapshot fixed footprint stays bounded; the cap prevents AoE-heavy
    // pulls from blowing up the per-bot vector. Sorted by distance so the
    // closest are kept when capped.
    {
        constexpr size_t MAX_ENEMIES = 16;
        // Tier 1.1 (behavior-preserving): consume the pre-partitioned enemy
        // scratch from the merged scan above instead of re-running a second
        // 40y Cell::VisitAllObjects. scratch_enemies_tl() holds exactly the
        // units AnyUnfriendlyUnitInObjectRangeCheck would have returned, in
        // the same order.
        std::vector<Unit*>& scratch_enemies = scratch_enemies_tl();

        // thread_local so the heap allocation amortizes across calls.
        // Pre-fix this was a stack-local vector reset to 0 every Build();
        // even with reserve() the *first* growth still allocates. With
        // 100K Builds/s pre-throttle and ~20K/s post-throttle, eliminating
        // that allocator hit per call is a measurable win on the snapshot
        // hot path.
        thread_local std::vector<std::pair<float, Unit*>> by_dist;
        by_dist.clear();
        by_dist.reserve(scratch_enemies.size());
        for (Unit* t : scratch_enemies)
        {
            if (!t || t->IsCritter() || t->IsTotem()) continue;
            // AnyUnfriendlyUnitInObjectRangeCheck only excludes IsFriendlyTo, so
            // it pulls in non-combat pets (CREATURE_TYPE_NON_COMBAT_PET = 12)
            // and battle pet wildlife — Northshire/Goldshire is full of these
            // (Orange Tabby Cat 7382, Bombay Cat 7385, etc). The bot would
            // happily pick them as victims and burn ticks emitting Frostbolt
            // intents that the spell engine rejects with BAD_TARGETS. Use the
            // same gate the engine uses so anything we surface here is actually
            // attackable. p->IsValidAttackTarget folds in faction, At-War,
            // immune-to-PC, visibility, and unit-flag checks.
            if (t->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET) continue;
            if (!p->IsValidAttackTarget(t)) continue;
            const float dx = t->GetPositionX() - p->GetPositionX();
            const float dy = t->GetPositionY() - p->GetPositionY();
            const float dz = t->GetPositionZ() - p->GetPositionZ();
            by_dist.emplace_back(dx*dx + dy*dy + dz*dz, t);
        }
        std::sort(by_dist.begin(), by_dist.end(),
                  [](auto const& a, auto const& b) { return a.first < b.first; });
        if (by_dist.size() > MAX_ENEMIES) by_dist.resize(MAX_ENEMIES);

        snap->combat.nearby_enemies.reserve(by_dist.size());
        for (auto const& [_, t] : by_dist)
        {
            NearbyUnit u{};
            u.guid   = t->GetGUID();
            u.entry  = t->GetEntry();
            u.level  = t->GetLevelForTarget(p);   // per-viewer scaled level (see attackers note / audit doc)
            u.hp     = static_cast<int32>(t->GetHealth());
            u.max_hp = static_cast<int32>(t->GetMaxHealth());
            u.x = t->GetPositionX();
            u.y = t->GetPositionY();
            u.z = t->GetPositionZ();
            u.o = t->GetOrientation();
            Spell* casting = t->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            if (!casting) casting = t->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
            if (casting)
            {
                u.is_casting       = true;
                u.casting_spell_id = casting->m_spellInfo->Id;
                u.is_interruptible = casting->m_spellInfo->CanBeInterrupted(p, t, false);
                const int32 rem    = casting->GetRemainingCastTime();
                u.cast_remaining   = Ms{rem > 0 ? rem : 0};
                // Detect interrupt casts (Counterspell / Pummel / Kick /
                // etc) by walking SpellEffects for SPELL_EFFECT_INTERRUPT_CAST.
                for (auto const& eff : casting->m_spellInfo->GetEffects())
                {
                    if (eff.Effect == SPELL_EFFECT_INTERRUPT_CAST)
                    {
                        u.is_casting_kick = true;
                        break;
                    }
                }
            }
            if (Unit const* v = t->GetVictim())
                u.victim = v->GetGUID();
            // Dungeon-boss flag — drives idle:tank_pull_next priority
            // and the dungeon engagement / wipe-detection rules.
            if (Creature const* c = t->ToCreature())
            {
                u.is_dungeon_boss = c->IsDungeonBoss();
                // Folds CREATURE_STATIC_FLAG_NO_XP + template flags_extra
                // NO_XP. Training dummies and other no-reward targets must
                // never be grind-engaged (they don't die, grant nothing, and
                // wedge the bot InCombat forever).
                u.no_xp_kill = !c->CanGiveExperience();
                // Robust, data-independent dummy catch: identify training /
                // practice / test dummies by NAME and force no_xp_kill. Their
                // NO_XP static flag is mis-authored on this realm (entry 44820's
                // wc_world StaticFlags1 lacked the bit), so CanGiveExperience()
                // returns true and the idle grind-picker re-engaged the immortal
                // dummy every Idle<->InCombat cycle (flat XP forever). The name is
                // reliable and entry-agnostic; every engage/threat path already
                // skips no_xp_kill, so this stops the wedge at the source.
                if (!u.no_xp_kill)
                {
                    std::string const& nm = c->GetName();
                    if (nm.find("Dummy") != std::string::npos ||
                        nm.find("dummy") != std::string::npos)
                        u.no_xp_kill = true;
                }
                // Pacified creatures cannot attack -> never a real threat. The
                // robust catch for mis-flagged Training Dummies that lack the
                // NO_XP static flag (so no_xp_kill is false) yet are immortal:
                // threat-pull/engage rules skip is_pacified so the bot never
                // wedges firing at an unkillable practice dummy. See NearbyUnit.
                // Check BOTH the live unit flag AND the creature_template flag:
                // the Valley-of-Trials Training Dummy (44820) has unit_flags
                // 0x20000 (PACIFIED) in its template but the LIVE unit does NOT
                // carry it (TC doesn't apply that control flag from the static
                // template / a difficulty row overrides it), so the runtime-only
                // HasUnitFlag check read false and the bot still engaged it. The
                // template flag is the reliable signal for these static props.
                u.is_pacified = c->HasUnitFlag(UNIT_FLAG_PACIFIED);
                // Uninteractible (NOT_SELECTABLE): unkillable, unacquirable hazard
                // stalkers. Engage/pull rules skip these; the in-combat advance
                // walks the tank through them. See NearbyUnit + the harbor note.
                u.untargetable = c->IsUninteractible();
                // Mob's own chase generator reports it cannot path to its target
                // (aggro-but-unreachable window — WC z-disconnected ledge pack).
                // The combat re-aim gates skip chasing these. See NearbyUnit.
                u.cannot_reach = c->CanNotReachTarget();
                if (CreatureTemplate const* ct = c->GetCreatureTemplate())
                {
                    // CreatureType is uint8 in TC; Beast=1, etc.
                    u.creature_type = uint8(ct->type);
                    if (ct->unit_flags & UNIT_FLAG_PACIFIED)
                        u.is_pacified = true;
                    if (ct->unit_flags & UNIT_FLAG_UNINTERACTIBLE)
                        u.untargetable = true;
                    // Elite / RareElite / Rare = legit high-HP, slow kills; the
                    // unkillable-target leash must never abandon them.
                    u.is_elite_or_rare =
                        ct->Classification == CreatureClassifications::Elite ||
                        ct->Classification == CreatureClassifications::RareElite ||
                        ct->Classification == CreatureClassifications::Rare;
                }
            }
            // Player role inference from primary spec. Drives the BG
            // PvP target-priority rule (focus healers) — without this,
            // u.role defaults to Tank for every enemy and the focus-
            // heal heuristic becomes random.
            if (Player const* tp = t->ToPlayer())
            {
                u.is_player = true;
                uint8  cls  = tp->GetClass();
                uint16 spec = uint16(tp->GetPrimarySpecialization());
                if (IsHealerSpec(cls, spec))    u.role = Role::Healer;
                else if (IsTankSpec(cls, spec)) u.role = Role::Tank;
                else                            u.role = Role::Dps;
                u.is_in_group = (tp->GetGroup() != nullptr);
            }
            else
            {
                u.role = Role::Unknown;
            }
            // CC-state detection — walk applied auras once, find the
            // highest-priority harmful CC mechanic. Compact 2-field
            // result drives don't-break-friendly-CC + DR-aware target
            // selection. Priority order (most disruptive first):
            //   STUN (12) > POLYMORPH (10) > FEAR (5) > INCAPACITATE
            //   (16) > SLEEP (14) > DISORIENT (6) > HORROR (24) >
            //   CHARM (1).
            // Per-unit LoS check from bot. Costly raycast (terrain +
            // model + dynamic objects) — capped via the nearby_enemies
            // limit upstream and TTL-cached (see CachedLosCheck).
            u.in_los = CachedLosCheck(p, t, GameTime::GetGameTimeMS());
            int best_cc_pri = -1;
            // Affix-buff whitelist — small static list of POSITIVE buff
            // aura ids we surface on NearbyUnit.affix_buffs so the
            // dungeon AI can react to M+ Bolstering and similar mob-
            // empowering affixes without a generic per-unit auras dump.
            // Extend as more "mob carries this buff → adjust pull rule"
            // mechanics get wired.
            static constexpr uint32 kAffixBuffIds[] = {
                209859,  // Bolstering (M+ affix mob buff)
                228318,  // Raging (M+ affix enrage buff)
            };
            for (auto const& [aid, aapp] : t->GetAppliedAuras())
            {
                if (!aapp) continue;
                Aura const* abase = aapp->GetBase();
                if (!abase) continue;
                SpellInfo const* asi = abase->GetSpellInfo();
                if (!asi) continue;
                // Positive auras — sample the affix whitelist only.
                if (aapp->IsPositive())
                {
                    for (uint32 wid : kAffixBuffIds)
                    {
                        if (asi->Id == wid)
                        {
                            u.affix_buffs.push_back(wid);
                            break;
                        }
                    }
                    continue;
                }
                int pri = -1;
                switch (asi->Mechanic)
                {
                    case 12: pri = 8; break;  // STUN
                    case 10: pri = 7; break;  // POLYMORPH
                    case  5: pri = 6; break;  // FEAR
                    case 16: pri = 5; break;  // INCAPACITATE
                    case 14: pri = 4; break;  // SLEEP
                    case  6: pri = 3; break;  // DISORIENT
                    case 24: pri = 2; break;  // HORROR
                    case  1: pri = 1; break;  // CHARM
                    default: continue;
                }
                if (pri > best_cc_pri)
                {
                    best_cc_pri = pri;
                    u.is_cc_locked = true;
                    u.cc_caster = abase->GetCasterGUID();
                }
            }
            snap->combat.nearby_enemies.push_back(u);
            // Multi-DoT specs: also record outbound auras the bot has on
            // this enemy. enemy_without_my_aura() walks the same vector to
            // decide whether to expand its DoT to a new add — without this
            // recording it would always see "no DoT here" and re-cast on
            // already-dotted enemies every tick.
            if (enemy_outbound_scan)
                record_outbound_for(t);
        }
    }

    // ---- Nearby friends (40yd, capped) ----
    // Healers and buffers query this to find off-group heal/buff targets.
    // Capped + sorted by distance like nearby_enemies. Self is excluded.
    {
        constexpr size_t MAX_FRIENDS = 12;
        // Tier 1.1 (behavior-preserving): consume the pre-partitioned friend
        // scratch from the merged scan above instead of re-running a second
        // 40y Cell::VisitAllObjects. scratch_friends_tl() holds exactly the
        // units AnyFriendlyUnitInObjectRangeCheck would have returned, in the
        // same order.
        std::vector<Unit*>& scratch_friends = scratch_friends_tl();

        // thread_local — see rationale on the parallel enemy scan above.
        thread_local std::vector<std::pair<float, Unit*>> by_dist;
        by_dist.clear();
        by_dist.reserve(scratch_friends.size());
        for (Unit* t : scratch_friends)
        {
            if (!t || t == p) continue;
            if (t->IsCritter() || t->IsTotem()) continue;
            const float dx = t->GetPositionX() - p->GetPositionX();
            const float dy = t->GetPositionY() - p->GetPositionY();
            const float dz = t->GetPositionZ() - p->GetPositionZ();
            by_dist.emplace_back(dx*dx + dy*dy + dz*dz, t);
        }
        std::sort(by_dist.begin(), by_dist.end(),
                  [](auto const& a, auto const& b) { return a.first < b.first; });

        // CombatLoop FIX A1: a friendly-MONSTER / TALKTO objective's target
        // (e.g. q29082 "Injured Infant" 50047, friendly faction 12) is a
        // FRIEND, so it lives in nearby_friends — and the distance-sorted 12-cap
        // below can evict it when the bot is surrounded by closer friendlies,
        // starving the use-on-friend / talk-credit rules in State_Idle. Before
        // the cap, stable_partition any friend whose entry matches an INCOMPLETE
        // friendly objective to the FRONT so it survives the truncate. The
        // entry set MUST mirror what the consumers check: the use-on-friend rule
        // (idle:quest_use_item_on_friend) and quest_talk rule both key on the
        // objective object_id (+ MONSTER credit aliases). Built here directly
        // from the bot's quest status map because snap->quest_log isn't
        // populated yet at this point in Build(). Reorder-then-truncate only —
        // the list is never grown past MAX_FRIENDS.
        {
            std::unordered_set<uint32> obj_friendly_entries;
            Player* mp_f = const_cast<Player*>(p);
            for (auto const& [qid, qs] : mp_f->getQuestStatusMap())
            {
                if (qs.Status != QUEST_STATUS_INCOMPLETE) continue;  // only un-finished work
                QuestImmutable const& im = GetQuestImmutable(qid, p);
                Quest const* qt = sObjectMgr->GetQuestTemplate(qid);
                if (!qt) continue;
                size_t oi = 0;
                for (auto const& tobj : qt->Objectives)
                {
                    if (tobj.Flags & QUEST_OBJECTIVE_FLAG_HIDDEN) continue;
                    if (tobj.Flags & QUEST_OBJECTIVE_FLAG_PART_OF_PROGRESS_BAR) continue;
                    if (oi >= im.objectives.size()) break;
                    QuestObjectiveEntry const& oe = im.objectives[oi];
                    ++oi;
                    if (oe.type != QUEST_OBJECTIVE_MONSTER && oe.type != QUEST_OBJECTIVE_TALKTO)
                        continue;
                    // Skip already-finished objectives — the consumer ignores
                    // them, so keeping their friendly pinned wastes a cap slot.
                    const int32 prog = tobj.IsStoringValue() ? mp_f->GetQuestObjectiveData(tobj) : 0;
                    if (prog >= oe.amount) continue;
                    if (oe.object_id > 0)
                        obj_friendly_entries.insert(uint32(oe.object_id));
                    for (uint32 a : oe.credit_alias_entries)
                        obj_friendly_entries.insert(a);
                }
            }
            if (!obj_friendly_entries.empty())
                std::stable_partition(by_dist.begin(), by_dist.end(),
                    [&](std::pair<float, Unit*> const& pr) {
                        return pr.second && obj_friendly_entries.count(pr.second->GetEntry()) != 0;
                    });
        }

        if (by_dist.size() > MAX_FRIENDS) by_dist.resize(MAX_FRIENDS);

        snap->combat.nearby_friends.reserve(by_dist.size());
        for (auto const& [_, t] : by_dist)
        {
            NearbyUnit u{};
            u.guid   = t->GetGUID();
            u.entry  = t->GetEntry();
            u.level  = t->GetLevelForTarget(p);   // per-viewer scaled level (see attackers note / audit doc)
            u.hp     = static_cast<int32>(t->GetHealth());
            u.max_hp = static_cast<int32>(t->GetMaxHealth());
            u.x = t->GetPositionX();
            u.y = t->GetPositionY();
            u.z = t->GetPositionZ();
            u.o = t->GetOrientation();
            if (Unit const* v = t->GetVictim())
                u.victim = v->GetGUID();
            // Capture npc_flags for friendly Creatures so AI can find
            // vendors/repairers/bankers/trainers/flightmasters/innkeepers
            // by capability (vs by entry id). Players keep npc_flags=0.
            if (Creature const* c = t->ToCreature())
                u.npc_flags = static_cast<uint32>(c->GetNpcFlags());
            // Player-side: capture group membership so the bot-to-player
            // invite rule can skip already-grouped candidates without
            // emitting a guaranteed-rejected invite. Also flags pending
            // invites — A inviting B fails when B already has C's invite
            // (server returns ERR_ALREADY_IN_GROUP_S the same as a member).
            if (Player const* pp = t->ToPlayer())
            {
                u.is_player = true;
                u.is_in_group = pp->GetGroup() != nullptr ||
                                pp->GetGroupInvite() != nullptr;
                u.guild_id = pp->GetGuildId();
                uint8  cls  = pp->GetClass();
                uint16 spec = uint16(pp->GetPrimarySpecialization());
                if (IsHealerSpec(cls, spec))    u.role = Role::Healer;
                else if (IsTankSpec(cls, spec)) u.role = Role::Tank;
                else                            u.role = Role::Dps;
            }
            else
            {
                u.role = Role::Unknown;
            }
            // ---- Friend-side cast / CC / affix / LoS population ----
            // Mirrors the nearby_enemies scan above. Without this:
            //   * Healing & buff rules cast through walls (in_los defaults
            //     true in NearbyUnit, so friends are always "visible").
            //   * Bots break friendly CC on poly'd allies (is_cc_locked
            //     would be false despite the aura).
            //   * Bolstering / Raging detection misses allied affix buffs.
            //   * No insight into what allies are casting (heal interrupts,
            //     dispel timing for outgoing ally cleanses, etc.).
            u.in_los = CachedLosCheck(p, t, GameTime::GetGameTimeMS());
            if (Spell* casting = t->GetCurrentSpell(CURRENT_GENERIC_SPELL)
                                  ? t->GetCurrentSpell(CURRENT_GENERIC_SPELL)
                                  : t->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            {
                u.is_casting       = true;
                u.casting_spell_id = casting->m_spellInfo->Id;
                u.is_interruptible = casting->m_spellInfo->CanBeInterrupted(p, t, false);
                const int32 rem    = casting->GetRemainingCastTime();
                u.cast_remaining   = Ms{rem > 0 ? rem : 0};
            }
            {
                static constexpr uint32 kAffixBuffIds[] = {
                    209859,  // Bolstering
                    228318,  // Raging
                };
                int best_cc_pri = -1;
                for (auto const& [aid, aapp] : t->GetAppliedAuras())
                {
                    if (!aapp) continue;
                    Aura const* abase = aapp->GetBase();
                    if (!abase) continue;
                    SpellInfo const* asi = abase->GetSpellInfo();
                    if (!asi) continue;
                    if (aapp->IsPositive())
                    {
                        for (uint32 wid : kAffixBuffIds)
                            if (asi->Id == wid)
                            {
                                u.affix_buffs.push_back(wid);
                                break;
                            }
                        continue;
                    }
                    int pri = -1;
                    switch (asi->Mechanic)
                    {
                        case 12: pri = 8; break;
                        case 10: pri = 7; break;
                        case  5: pri = 6; break;
                        case 16: pri = 5; break;
                        case 14: pri = 4; break;
                        case  6: pri = 3; break;
                        case 24: pri = 2; break;
                        case  1: pri = 1; break;
                        default: continue;
                    }
                    if (pri > best_cc_pri)
                    {
                        best_cc_pri = pri;
                        u.is_cc_locked = true;
                        u.cc_caster = abase->GetCasterGUID();
                    }
                }
            }
            snap->combat.nearby_friends.push_back(u);
        }
    }

    // ---- Hub-aware extended quest-giver scan ----
    // The 40y nearby_friends scan above caps at 12 entries, sorted by
    // distance — fine for healing / buff targeting, but it loses the
    // city-wide quest-giver picture. Capital cluster radii from DBSCAN run
    // 200-400y (Stormwind, Orgrimmar, Dalaran, etc.); a bot standing 80y
    // from the cluster centroid sees only its 12 closest neighbors —
    // typically guards / patrollers / civilian flavor NPCs — and misses
    // every actual quest giver in the city. quest_offers ends up empty,
    // idle:wander_to_quest_hub doesn't fire, the bot oscillates around
    // the centroid forever.
    //
    // When the bot is inside a known quest-hub cluster, run a SECOND scan
    // out to the hub's actual radius (clamped 100..500y) for QUESTGIVER-
    // flagged creatures only and append them. They get the same minimal
    // NearbyUnit shape (guid/entry/x/y/z/npc_flags) that offers_from
    // consumes; healing rules ignore them (combat-targeting only walks
    // entries with hp/max_hp populated, which we leave at 0 for these).
    //
    // Cost: O(creatures within hub radius) extra cell visit, gated to
    // bots actually standing inside a hub cluster — no overhead in
    // wilderness. The result is bounded by max-quest-givers-per-city
    // (~30-50) so nearby_friends doesn't balloon arbitrarily.
    // 1s per-bot throttle on the wide hub scan — perf audit found
    // this firing every world frame for capital-clustered bots
    // (200-500 bots × 50Hz × 500y Cell::VisitAllObjects = the
    // dominant world-thread cost at scale). Hub creatures don't
    // move, so 1Hz is plenty; the 40y nearby_friends scan above
    // still captures the immediate neighborhood every tick.
    if (Services::Initialized())
    {
        BotAI* hb_ai = bot_ai;
        const uint32 hub_now_ms = GameTime::GetGameTimeMS();
        // 800ms instead of 1000ms so the hub-scan period doesn't perfectly
        // co-phase with the 1Hz Idle-tier snapshot (per audit aa083c7c
        // section 2 — idle bots that align both throttles end up with
        // every snapshot missing the wide-hub data; staggering ensures
        // every 4th-5th tick captures it).
        constexpr uint32 kHubScanIntervalMs = 800u;
        const bool hub_throttled = hb_ai &&
            hb_ai->last_hub_scan_ms() != 0 &&
            (hub_now_ms - hb_ai->last_hub_scan_ms()) < kHubScanIntervalMs;

        auto const* hub = hub_throttled ? nullptr :
            Services::Hubs().GetQuestHubAtPosition(
                Position{p->GetPositionX(), p->GetPositionY(), p->GetPositionZ()},
                uint32(p->GetZoneId()));

        // Starter-zone safety net: even without a hub-DB cluster covering
        // the bot's position, low-level bots (L<20) need to find quest
        // givers that are 50-150y away — typical starter zone layout.
        // Without this widen, fresh L1 alts in Northshire / Coldridge /
        // Valley of Trials see EMPTY quest_offers and idle forever (the
        // 40y nearby_friends scan above misses the giver clusters by
        // a few yards). 150y is the empirical sweet spot — covers the
        // starter zone hubs without dragging in irrelevant NPCs.
        float forced_ext_radius = 0.0f;
        if (hub == nullptr && !hub_throttled && p->GetLevel() < 20)
            forced_ext_radius = 150.0f;

        if ((hub != nullptr && hub->mapId == p->GetMapId()) || forced_ext_radius > 0.0f)
        {
            if (hb_ai) hb_ai->set_last_hub_scan_ms(hub_now_ms);
            const float ext_radius = (hub != nullptr)
                ? std::clamp(hub->radius, 100.0f, 500.0f)
                : forced_ext_radius;
            // Skip if the standard 40y scan already covers it.
            if (ext_radius > 40.0f)
            {
                // thread_local scratch reused across builds — avoids the
                // per-node std::list heap allocs the searcher would make.
                // CreatureListSearcher templates on the container so a vector
                // works as a drop-in; clear()+reserve() retains capacity.
                // Order (searcher insertion order) is preserved.
                thread_local std::vector<Creature*> creatures;
                creatures.clear();
                creatures.reserve(64);
                Trinity::AnyUnitInObjectRangeCheck cr_check(p, ext_radius);
                Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck> cr_searcher(p, creatures, cr_check);
                Cell::VisitAllObjects(p, cr_searcher, ext_radius);

                // Dedupe against existing nearby_friends (the 40y entries are
                // already there — don't double-count). Build a small set of
                // already-seen guids; appended entries don't need to be
                // re-deduped against each other since UnitListSearcher
                // returns each unit once.
                std::unordered_set<ObjectGuid> seen;
                seen.reserve(snap->combat.nearby_friends.size() + 16);
                for (auto const& nf : snap->combat.nearby_friends)
                    seen.insert(nf.guid);

                // Both quest givers AND trainers — the
                // idle:travel_to_trainer rule needs trainer-flagged NPCs
                // visible in the wide scan so a bot in profession mode
                // can walk across the city to reach one.
                constexpr uint32 kQuestGiverFlagMask =
                    uint32(UNIT_NPC_FLAG_QUESTGIVER) | uint32(UNIT_NPC_FLAG_TRAINER);
                size_t appended = 0;
                constexpr size_t kMaxAppend = 64;
                for (Creature* c : creatures)
                {
                    if (!c || appended >= kMaxAppend) break;
                    if (!c->IsAlive()) continue;
                    if (!(uint32(c->GetNpcFlags()) & kQuestGiverFlagMask)) continue;
                    if (seen.count(c->GetGUID())) continue;
                    NearbyUnit u{};
                    u.guid      = c->GetGUID();
                    u.entry     = c->GetEntry();
                    u.level     = c->GetLevelForTarget(p);   // per-viewer scaled level
                    // hp/max_hp/victim left at 0 — healing rules require
                    // populated hp and skip these entries naturally.
                    u.x         = c->GetPositionX();
                    u.y         = c->GetPositionY();
                    u.z         = c->GetPositionZ();
                    u.o         = c->GetOrientation();
                    u.npc_flags = uint32(c->GetNpcFlags());
                    snap->combat.nearby_friends.push_back(u);
                    seen.insert(c->GetGUID());
                    ++appended;
                }
            }
        }
    }

    // ---- BG siege-vehicle wide scan (IoC / SoTA / WG) ----
    // Siege engines / demolishers / glaive throwers spawn at FIXED pads 50-80y
    // from the node they belong to (IoC Workshop: siege engines DB-spawned at
    // (774,-884) = 80y from the node (776,-804); demolishers at (774,-854) =
    // 50y) — OUTSIDE the 40y nearby_friends scan, so a bot holding/near the node
    // never sees them and idle:bg_move_to_vehicle never fires (live: Workshop
    // captured but 0 siege-engine mounts; only sparse demolisher mounts from
    // bots that wandered onto the pad). When in a battleground, scan ~110y for
    // FRIENDLY (faction set => node is held by our team) EMPTY vehicles and
    // append them WITH hp so the vehicle-mount rule can route the bot to one.
    // Gated to InBattleground + 800ms throttle (parked vehicles barely move).
    // Mirrors the hub quest-giver wide scan above.
    if (Services::Initialized() && p->InBattleground())
    {
        BotAI* vb_ai = bot_ai;
        const uint32 veh_now_ms = GameTime::GetGameTimeMS();
        const bool veh_throttled = vb_ai && vb_ai->last_bg_veh_scan_ms() != 0 &&
            (veh_now_ms - vb_ai->last_bg_veh_scan_ms()) < 800u;
        if (!veh_throttled)
        {
            if (vb_ai) vb_ai->set_last_bg_veh_scan_ms(veh_now_ms);
            constexpr float kVehScanRadius = 110.0f;
            thread_local std::vector<Creature*> veh_creatures;
            veh_creatures.clear();
            veh_creatures.reserve(32);
            Trinity::AnyUnitInObjectRangeCheck v_check(p, kVehScanRadius);
            Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck> v_searcher(p, veh_creatures, v_check);
            Cell::VisitAllObjects(p, v_searcher, kVehScanRadius);

            std::unordered_set<ObjectGuid> vseen;
            vseen.reserve(snap->combat.nearby_friends.size() + 8);
            for (auto const& nf : snap->combat.nearby_friends)
                vseen.insert(nf.guid);

            size_t v_appended = 0;
            constexpr size_t kMaxVehAppend = 16;
            for (Creature* c : veh_creatures)
            {
                if (!c || v_appended >= kMaxVehAppend) break;
                if (!c->IsAlive() || !c->IsVehicle()) continue;
                Vehicle* vk = c->GetVehicleKit();
                if (!vk || vk->GetAvailableSeatCount() == 0) continue; // occupied
                if (!c->IsFriendlyTo(p)) continue;                     // node not ours yet
                if (vseen.count(c->GetGUID())) continue;
                NearbyUnit u{};
                u.guid      = c->GetGUID();
                u.entry     = c->GetEntry();
                u.level     = c->GetLevelForTarget(p);
                u.hp        = int32(c->GetHealth());
                u.max_hp    = int32(c->GetMaxHealth());
                u.x         = c->GetPositionX();
                u.y         = c->GetPositionY();
                u.z         = c->GetPositionZ();
                u.o         = c->GetOrientation();
                u.npc_flags = uint32(c->GetNpcFlags());
                snap->combat.nearby_friends.push_back(u);
                vseen.insert(c->GetGUID());
                ++v_appended;
            }
        }
    }

    // ---- Movement: active path destination ----
    // Populates path_end_x/y/z when the bot is in a CHASE / FOLLOW / POINT
    // movement generator. Lets rules check "am I already moving here?" before
    // re-emitting MoveTo intents every tick. path_target is the chase/follow
    // unit guid when applicable; POINT moves leave it empty.
    if (MotionMaster* mm = p->GetMotionMaster())
    {
        const MovementGeneratorType t = mm->GetCurrentMovementGeneratorType();
        if (t == CHASE_MOTION_TYPE || t == FOLLOW_MOTION_TYPE || t == POINT_MOTION_TYPE)
        {
            float dx = 0.f, dy = 0.f, dz = 0.f;
            if (mm->GetDestination(dx, dy, dz))
            {
                snap->path.path_end_map_id = p->GetMapId();
                snap->path.path_end_x = dx;
                snap->path.path_end_y = dy;
                snap->path.path_end_z = dz;
            }
            if (t == CHASE_MOTION_TYPE || t == FOLLOW_MOTION_TYPE)
            {
                if (Unit const* tgt = p->GetVictim())
                    snap->path.path_target = tgt->GetGUID();
            }
        }
    }

    // ---- Quests ----
    // Active quest log: quest id + state + level/xp metadata + per-objective
    // type/progress/required counts. Objectives drive the quest-execution
    // rules in State_Idle (idle:quest_kill / quest_collect / quest_use_go /
    // quest_talk / quest_explore) so the bot knows what to actually DO for
    // a quest beyond accept→turnin. ClassifyTier checks `!quests.empty()` to
    // keep questing bots in the Active tier; without this the questing bots
    // got demoted to Idle/Hibernate (slower tick rate) the moment they
    // stopped moving (turning in, NPC dialog, looting a quest item).
    {
        Player* mp = const_cast<Player*>(p);
        auto const& qmap = mp->getQuestStatusMap();
        snap->quest_log.quests.reserve(qmap.size());
        for (auto const& [qid, qs] : qmap)
        {
            if (qs.Status == QUEST_STATUS_NONE) continue;
            Quest const* qt = sObjectMgr->GetQuestTemplate(qid);
            // Tier 1.2 (behavior-preserving): pull the immutable per-quest
            // shape (flags / source-item / per-objective static fields +
            // resolved alias/label/talk lists / ender coords / unachievable /
            // unturnable) from the process-wide quest_id-keyed cache instead
            // of re-deriving it every build. Only the volatile per-tick state
            // (state flag, bot-level-scaled level/min/xp, per-objective
            // progress) is recomputed below. The assembled QuestEntry is
            // identical to the old inline build.
            QuestImmutable const& im = GetQuestImmutable(qid, p);
            QuestEntry e{};
            e.quest_id  = qid;
            e.state     = (qs.Status == QUEST_STATUS_COMPLETE) ? uint8(1) : uint8(0);
            // Immutable copies (cache is shared; we copy out per build).
            e.flags          = im.flags;
            e.source_item_id = im.source_item_id;
            e.unachievable   = im.unachievable;
            e.unturnable     = im.unturnable;
            e.ender_resolved = im.ender_resolved;
            e.ender_map_id   = im.ender_map_id;
            e.ender_x        = im.ender_x;
            e.ender_y        = im.ender_y;
            e.ender_z        = im.ender_z;
            // Prefer the NEAREST SAME-MAP ender spawn (a multi-ender quest must
            // route to the closest turn-in, not the DB-first one — Durnan: Gremlock
            // 983y over Brock 2048y). Falls back to the first ender (above) when no
            // ender is on the bot's current map (cross-map handled by travel).
            if (im.ender_spawns.size() > 1)
            {
                float best_ender_d2 = std::numeric_limits<float>::max();
                for (auto const& es : im.ender_spawns)
                {
                    if (es.map_id != p->GetMapId()) continue;
                    const float edx = es.x - p->GetPositionX();
                    const float edy = es.y - p->GetPositionY();
                    const float ed2 = edx * edx + edy * edy;
                    if (ed2 < best_ender_d2)
                    {
                        best_ender_d2 = ed2;
                        e.ender_map_id = es.map_id;
                        e.ender_x = es.x; e.ender_y = es.y; e.ender_z = es.z;
                    }
                }
            }
            if (qt)   // matches the original `if (qt)` gate for level/xp/objectives
            {
                // 12.0 quest levels are dynamic via ContentTuning — VOLATILE
                // (bot-level dependent), so still computed per build via the
                // Player accessors that resolve contentTuningId vs scaling.
                e.level     = uint16(std::max<int32>(0, mp->GetQuestLevel(qt)));
                e.min_level = uint16(std::max<int32>(0, mp->GetQuestMinLevel(qt)));
                e.xp_reward = uint32(qt->XPValue(p));

                // Copy the cached immutable objective scaffolding, then fill
                // the VOLATILE per-objective progress. The cache was built
                // with the identical HIDDEN / PROGRESS_BAR skip filter, so
                // walking qt->Objectives with the same filter visits the same
                // objectives in the same order — index i in e.objectives lines
                // up with the i-th non-skipped template objective.
                e.objectives = im.objectives;
                size_t oi = 0;
                for (auto const& obj : qt->Objectives)
                {
                    if (obj.Flags & QUEST_OBJECTIVE_FLAG_HIDDEN) continue;
                    if (obj.Flags & QUEST_OBJECTIVE_FLAG_PART_OF_PROGRESS_BAR) continue;
                    if (oi >= e.objectives.size()) break;   // defensive; sizes match by construction
                    // GetQuestObjectiveData returns 0 for non-storing-value
                    // objective types (LEARNSPELL, MONEY, etc), which is
                    // semantically correct — those have implicit completion.
                    e.objectives[oi].progress = obj.IsStoringValue()
                                                  ? mp->GetQuestObjectiveData(obj)
                                                  : 0;
                    ++oi;
                }
            }
            snap->quest_log.quests.push_back(std::move(e));
        }
    }
    // Build the O(1) quest_id → index map. Done once after the vector
    // is fully populated so re-hashing during push_back doesn't matter.
    // Tier 3.3: sorted flat vector + binary search (was unordered_map).
    snap->quest_log.quests_index.clear();
    snap->quest_log.quests_index.reserve(snap->quest_log.quests.size());
    for (uint32 i = 0; i < snap->quest_log.quests.size(); ++i)
        snap->quest_log.quests_index.push(
            snap->quest_log.quests[i].quest_id, i);
    snap->quest_log.quests_index.finalize();

    // ---- Current objective selection ----
    // Pick the single objective the bot should chase right now. Walk active
    // (incomplete) quests in log order; for each, find the first objective
    // that:
    //   - has progress < amount (not already done)
    //   - is type-actionable (one we have an idle:quest_* rule for)
    //   - is not OPTIONAL (don't waste cycles on side-objectives)
    //   - does not have a SEQUENCED predecessor that's still incomplete
    //   - is not currently blacklisted by BotAI's stuck-detection (5-min
    //     cooldown after no progress for ~5 min). Skipping blacklisted
    //     objectives lets the bot pick a different quest's objective during
    //     the cooldown rather than wasting cycles re-trying the unreachable
    //     one. Cross-thread read of BotAI state is safe: ObjectiveTrack is
    //     a POD struct and reads are non-atomic but eventually consistent;
    //     a stale read just delays the re-skip by one tick.
    // First match wins. Updates every snapshot tick so completing one
    // objective immediately shifts focus to the next without explicit
    // bookkeeping in BotAI. Stays empty (current_quest_id == 0) when the
    // bot has nothing actionable — typically because all active quests
    // are turn-in-ready or only contain implicit objectives (LEARNSPELL,
    // MIN_REPUTATION, AREATRIGGER without coords, etc).
    {
        // bot_ai is the build-target's pre-resolved AI (function parameter).
        const uint32 now_ms = GameTime::GetGameTimeMS();
        const uint32 bot_map = p->GetMapId();
        // Track best-eligible "any" + best-eligible "same-map" candidates.
        // Prefer same-map at the end so the bot doesn't chase a cross-map
        // POI when an in-zone alternative exists. Both honour FIFO order so
        // ties go to the older quest (what the user accepted first).
        QuestObjectiveEntry best_any{};
        uint32 best_any_quest = 0;
        bool   best_any_set = false;
        QuestObjectiveEntry best_same_map{};
        uint32 best_same_map_quest = 0;
        bool   best_same_map_set = false;
        // Planar edge-distance of the chosen same-map objective. The picker selects
        // the NEAREST local actionable objective (2026-06-20), not the first in scan
        // order — see the selection loop below.
        float  best_same_map_dist = std::numeric_limits<float>::max();
        // Track best breadcrumb / DB2-criteria fallback. Two flavours:
        //
        //   a) Quest has NO `quest_objectives` rows at all (Time Trials
        //      55660 / Hero's Call breadcrumbs 26365 / Captain Grayson
        //      26371 / Withdraw to Loading Room 28169). These auto-
        //      complete on accept; the "objective" is implicit, just
        //      walk to the ender and hand in.
        //
        //   b) Quest is in state=1 (complete, turn-in pending) but the
        //      ender NPC isn't spawned in any creature row visible to
        //      the bot — script-spawned escort enders like Tarindrella
        //      (q 28725 "The Woodland Protector"), where the ender
        //      lives only inside a cave / scripted area until the
        //      player walks in and the script fires. nearest_quest_turnin
        //      can't see her since she has no spawn record, so the
        //      bot wanders. Synthesize a TALKTO targeting the registered
        //      ender entry so the POI lookup below resolves to the
        //      quest's blob (typically the cave polygon centroid) and
        //      idle:quest_path walks the bot there.
        //
        // In either case we resolve the turn-in NPC via
        // GetCreatureQuestInvolvedRelationReverseBounds and synthesize a
        // TALKTO objective so idle:quest_path emits move_to(ender).
        QuestObjectiveEntry best_breadcrumb{};
        uint32 best_breadcrumb_quest = 0;
        bool   best_breadcrumb_set = false;
        // Same-map breadcrumb winner. A complete quest whose turn-in NPC is
        // on the bot's CURRENT map is strictly preferable to one whose ender
        // is on another continent: the bot can actually walk to the former
        // (idle:walk_to_quest_ender) and hand it in, while the latter needs
        // flight/hearth the low-level bot doesn't have. Without this, the
        // FIFO pick below could fasten current_objective onto a cross-map
        // turn-in (observed: L1 "Somi" in Orgrimmar/map1 whose complete
        // quest 28608 turns in to Undertaker Mordo on map0, masking its 3
        // reachable same-map Alchemy turn-ins at Yelmak ~152y away). Track a
        // same-map candidate separately and let it win.
        QuestObjectiveEntry best_breadcrumb_sm{};
        uint32 best_breadcrumb_sm_quest = 0;
        bool   best_breadcrumb_sm_set = false;
        // Planar distance to the same-map turn-in ender, so the selection can
        // compare it against a near actionable objective (best_same_map_dist): a
        // turn-in 1100y away must NOT outrank a use-item/objective at the bot's feet.
        float  best_breadcrumb_sm_dist = std::numeric_limits<float>::max();
        for (auto const& q : snap->quest_log.quests)
        {
            // PROFESSION-SKILL GATE. A quest gated on a profession skill the
            // bot does not actually know can never be progressed OR turned in,
            // yet its objectives still surface as picker candidates. The
            // Alchemy specialization quests 29067/29481/29482 ("Potion/Elixir/
            // Transmutation Master", RequiredSkillID=171) are the live example:
            // they get force-granted at world-population setup despite the bot
            // never having learned Alchemy, and their ITEM-craft objectives
            // (Type=1) sit on the bot's CURRENT map. That makes them win
            // best_same_map over the bot's real cross-map objective (e.g. L1
            // "Somi" in Orgrimmar whose 28608 "The Shadow Grave" turns in on
            // map 0 / Tirisfal). With an unactionable current_objective set,
            // the bot neither pursues it (no reachable target) nor falls back
            // to wander/seek (current_quest_id != 0 suppresses that), so it
            // freezes in place. Skip such quests entirely from every candidate
            // bucket. This mirrors the accept-path gate (~L4426) that stops the
            // bot taking these in the first place; here it un-masks the real
            // objective for bots that were force-granted them earlier.
            // Re-evaluated each build, so a bot that later learns the
            // profession resumes the quest normally.
            if (Quest const* qt = sObjectMgr->GetQuestTemplate(q.quest_id))
                if (uint32 reqSkill = qt->GetRequiredSkill())
                    if (const_cast<Player*>(p)->GetSkillValue(reqSkill) == 0)
                        continue;
            // Quest-level blacklist (audit C17): Tier-3 stuck recovery
            // (idle:unstick:drop_goal) calls blacklist_quest(), but only the
            // OFFER scan consulted it — the picker happily re-picked the same
            // dead quest next build, making drop_goal a no-op (the bot
            // "dropped" the goal and was handed it right back). Honor it here
            // so dropping a goal actually rotates the picker to other work
            // for the blacklist window.
            if (bot_ai && bot_ai->quest_blacklisted(q.quest_id, now_ms))
                continue;
            // Level gate: skip a quest the bot is too LOW-level to actually do.
            // A bot force-granted a quest far above its level (verified live: L4
            // "Somi" holding the ~L40 Silithus quest 4494 "March of the Silithid")
            // can never progress it — yet because the picker below PREFERS a
            // same-map "zero-travel" objective, that undoable LOCAL quest wins over
            // Somi's reachable CROSS-MAP goal (its completed Tirisfal turn-in,
            // 28608, which is a low-priority breadcrumb). The bad pick sets
            // current_objective_poi.map_id == bot_map, so State_Idle's cross-map
            // zeppelin/dock cascade (gated on poi.map_id != s.map_id()) never runs
            // and the bot parks at the dock. SatisfyQuestMinLevel reuses the
            // engine's own acceptance gate (false = silent): if the bot couldn't
            // even accept this quest at its current level, don't pursue it as the
            // active objective. Legit quests (bot meets the level) are unaffected.
            if (Quest const* lvlq = sObjectMgr->GetQuestTemplate(q.quest_id))
                if (!const_cast<Player*>(p)->SatisfyQuestMinLevel(lvlq, false))
                    continue;
            // For state=1 (complete) quests, the nearest_quest_turnin
            // pathway in State_Idle takes over IF an ender NPC is
            // physically present nearby. When no ender is visible (e.g.
            // Tarindrella's escort q 28725, or anything else where the
            // turn-in is a script-spawned NPC the snapshot doesn't see)
            // we fall through into the breadcrumb branch below to put
            // the bot on a walk-to-cave path. State_Idle's quest_turnin
            // path still wins when an NPC actually shows up in
            // nearby_friends — current_objective being set doesn't
            // block the dedicated turn-in handler.
            // An INCOMPLETE quest (state==0) whose every non-optional
            // objective is already satisfied (progress>=amount, or amount==0)
            // also needs a turn-in walk: the server still reports the quest
            // INCOMPLETE because completion fires on the turn-in approach
            // (areatrigger / script / NPC hand-in) rather than from objective
            // credit alone. Without this, such a quest falls through every
            // branch below — the inner objective loop never enters its
            // `!done` body, so neither best_any nor best_same_map is set, and
            // current_quest_id stays 0. Observed live: L1 "Somi" in Orgrimmar
            // with quest 28608 "The Shadow Grave" — both ITEM objectives
            // (64581/64582) looted, quest still status=3, ender Undertaker
            // Mordo on map 0 (Tirisfal). The bot was parked with no objective.
            // Treating it as a breadcrumb routes the bot to the (cross-map)
            // ender so the elevator/zeppelin travel rules engage. Same-map
            // preference is preserved: the breadcrumb candidates are the
            // lowest-priority fallback (only assigned when no best_any /
            // best_same_map actionable objective exists), and a same-map ender
            // (best_breadcrumb_sm) still wins over a cross-map one.
            bool all_objectives_done = !q.objectives.empty();
            for (auto const& o : q.objectives)
            {
                if (o.flags & /*OPTIONAL*/ 0x0004) continue;
                const bool odone = (o.amount > 0 && o.progress >= o.amount)
                                    || (o.amount == 0);
                if (!odone) { all_objectives_done = false; break; }
            }
            const bool needs_turnin_walk = (q.state == 1)
                                           || (q.state == 0 && all_objectives_done);
            if (needs_turnin_walk || q.objectives.empty())
            {
                // USE-START-ITEM interception (2026-06-20). An INCOMPLETE,
                // objective-less quest whose StartItem the bot STILL HOLDS is a
                // "use the item on a target at the quest POI" quest, NOT a turn-in
                // (e.g. Q26118 "Seize the Ambassador": use the sledgehammer 56837 on
                // Ambassador Slaghammer at the High Seat). It has no quest_objectives
                // (the item's spell credits a hidden KillCredit), so without this it
                // falls through to the turn-in breadcrumb below and the bot walks to
                // the GIVER forever trying to hand in an incomplete quest (Durnan
                // wedged 91 min on Moira). Synthesize an ACTIONABLE use-item
                // objective anchored at the QuestPOI centroid so it ranks as local
                // work (best_same_map) and idle:quest_use_item_on_target drives it.
                if (q.state == 0 && q.objectives.empty() && q.source_item_id != 0 &&
                    p->HasItemCount(q.source_item_id, 1))
                {
                    QuestPOIData const* ui_poi = sObjectMgr->GetQuestPOIData(int32(q.quest_id));
                    float poix = 0.f, poiy = 0.f; bool have_poi = false;
                    if (ui_poi && !ui_poi->Blobs.empty() &&
                        ui_poi->Blobs.front().MapID == int32(bot_map))
                    {
                        // Centroid of the blob with the MOST points: the target area
                        // (the giver/start blob is a single point). The bot walks
                        // there and uses the item on the nearest enemy; the item
                        // spell's server-side script-target ensures only the real
                        // target (Slaghammer) is credited.
                        QuestPOIBlobData const* big = nullptr;
                        for (auto const& b : ui_poi->Blobs)
                            if (!b.Points.empty() &&
                                (!big || b.Points.size() > big->Points.size()))
                                big = &b;
                        if (big)
                        {
                            double sx = 0.0, sy = 0.0;
                            for (auto const& pt : big->Points) { sx += double(pt.X); sy += double(pt.Y); }
                            poix = float(sx / double(big->Points.size()));
                            poiy = float(sy / double(big->Points.size()));
                            have_poi = true;
                        }
                    }
                    if (have_poi)
                    {
                        const float ddx = poix - p->GetPositionX();
                        const float ddy = poiy - p->GetPositionY();
                        const float edge = std::sqrt(ddx * ddx + ddy * ddy);
                        constexpr float kFarObjective = 1500.0f;
                        if (edge <= kFarObjective && edge < best_same_map_dist)
                        {
                            QuestObjectiveEntry synth{};
                            synth.id             = 0;
                            synth.quest_id       = q.quest_id;
                            synth.type           = QUEST_OBJECTIVE_ITEM;
                            synth.object_id      = int32(q.source_item_id);
                            synth.amount         = 1;
                            synth.progress       = 0;
                            synth.use_start_item = true;
                            best_same_map        = synth;
                            best_same_map_quest  = q.quest_id;
                            best_same_map_set    = true;
                            best_same_map_dist   = edge;
                        }
                        // Never breadcrumb a use-item quest: turning it in is
                        // impossible while incomplete. If far/not-nearest it simply
                        // isn't chosen this pass (the bot does nearer work first).
                        continue;
                    }
                    // No usable POI → fall through to the normal breadcrumb path.
                }
                // Skip a turn-in/breadcrumb the wedge-remediation has blacklisted
                // (5-min, time-limited) because the bot can't path to its turn-in
                // NPC — so the picker swaps to a reachable quest instead of
                // re-choosing the unreachable breadcrumb forever. obj_id 0 is the
                // whole-quest sentinel the remediation stamps for turn-in goals.
                if (bot_ai && bot_ai->objective_blacklisted(q.quest_id, 0u, now_ms))
                    continue;
                // A same-map turn-in is the best possible breadcrumb; once
                // we have one, stop looking. Otherwise keep the first any-map
                // candidate (FIFO) but keep scanning for a same-map upgrade.
                if (best_breadcrumb_sm_set) continue;
                // Prefer creature enders; some breadcrumb finishers are
                // GameObjects (chests, banners, glyphed-pedestals — e.g.
                // 55660 "Time Trials" turns in to a pet-battle pedestal
                // GO, not an NPC). Fall through to GO ender lookup so the
                // bot at least walks to *something* to clear the slot.
                // We synthesize as QUEST_OBJECTIVE_TALKTO for NPCs and
                // QUEST_OBJECTIVE_GAMEOBJECT for GOs — both have
                // execution rules already (idle:quest_talk and
                // idle:quest_use_go respectively).
                auto cre_enders = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(q.quest_id);
                QuestObjectiveEntry synth{};
                synth.id            = 0;
                synth.quest_id      = q.quest_id;
                synth.storage_index = 0;
                synth.amount        = 1;
                synth.progress      = 0;
                synth.flags         = 0;
                if (cre_enders.begin() != cre_enders.end())
                {
                    synth.type      = QUEST_OBJECTIVE_TALKTO;
                    synth.object_id = int32(cre_enders.begin()->second);
                }
                else
                {
                    auto go_enders = sObjectMgr->GetGOQuestInvolvedRelationReverseBounds(q.quest_id);
                    if (go_enders.begin() == go_enders.end()) continue;  // truly no ender
                    synth.type      = QUEST_OBJECTIVE_GAMEOBJECT;
                    synth.object_id = int32(go_enders.begin()->second);
                }
                // Same-map preference: ender_resolved/ender_map_id were
                // populated above from the creature reverse-relation's first
                // spawn. When that spawn is on the bot's current map, this is
                // a reachable turn-in — record it as the same-map winner and
                // it takes precedence over any cross-map FIFO pick.
                if (q.ender_resolved && q.ender_map_id == bot_map)
                {
                    // A same-map turn-in normally gets TOP priority (turn in
                    // promptly = payday; this banked 21K stuck complete quests).
                    // BUT only when the ender is reasonably near. HUMAN MODEL
                    // (2026-06-15): if the ender is CROSS-ZONE far, making it the
                    // current objective strands an under-level bot trekking across
                    // the continent through deadly zones to bank ONE quest — Somi
                    // (L8) with complete 4494 whose Tanaris ender is ~7,600y away
                    // through L40 mobs looped in cross-zone combat instead of
                    // leveling in Durotar. Keep the quest, but DEMOTE a far ender
                    // to the low-priority breadcrumb bucket so local actionable
                    // work (best_same_map) wins; the bot banks it later when it is
                    // genuinely in the area (and at an appropriate level). The 1500y
                    // gate (~one zone) preserves every in-zone turn-in at top rank.
                    const float edx = q.ender_x - p->GetPositionX();
                    const float edy = q.ender_y - p->GetPositionY();
                    constexpr float kFarEnderSq = 1500.0f * 1500.0f;
                    if ((edx * edx + edy * edy) <= kFarEnderSq)
                    {
                        best_breadcrumb_sm = synth;
                        best_breadcrumb_sm_quest = q.quest_id;
                        best_breadcrumb_sm_set = true;
                        best_breadcrumb_sm_dist = std::sqrt(edx * edx + edy * edy);
                        continue;
                    }
                    // Far same-map ender: fall through to the low-priority bucket.
                }
                // Reject the any-map breadcrumb fallback when the ender sits on an
                // INSTANCE map (dungeon/raid/scenario/BfA-assault) — overland travel
                // can never reach it, so turning the quest in is impossible and the
                // bot would lock onto it (observed: L10 Tindle re-picking Q56185,
                // whose ender 152365 spawns only on map 1929, as a breadcrumb). A
                // legitimate cross-CONTINENT ender (another open map) is still
                // allowed — the travel pipeline handles those.
                bool ender_map_unreachable = false;
                if (q.ender_resolved && q.ender_map_id != bot_map)
                    if (::MapEntry const* eme = sMapStore.LookupEntry(q.ender_map_id))
                        ender_map_unreachable = eme->Instanceable();
                if (!best_breadcrumb_set && !ender_map_unreachable)
                {
                    best_breadcrumb = synth;
                    best_breadcrumb_quest = q.quest_id;
                    best_breadcrumb_set = true;
                }
                continue;
            }
            if (q.state == 1) continue;             // already complete with objectives — turn-in pathway will handle
            // Pre-resolve this quest's POI map (if any) once per quest, used
            // to bias selection toward zero-travel quests below.
            int32 quest_poi_map = -1;
            QuestPOIData const* quest_poi = sObjectMgr->GetQuestPOIData(int32(q.quest_id));
            if (quest_poi && !quest_poi->Blobs.empty())
                quest_poi_map = quest_poi->Blobs.front().MapID;
            // Reject objectives whose POI is on an INSTANCE map the bot isn't in:
            // e.g. an overland leveling bot mis-granted BfA assault content
            // (Q56185 -> map 1929). Such a target is never reachable by overland
            // travel, but best_any ignores map, so without this the picker locks
            // onto it forever (observed: L10 Tindle stranded on a map-1929 quest
            // while reachable Q432/Q433 sat ignored). Cheap MapEntry lookup; no
            // pathfinding (Build-thread safe).
            bool quest_poi_map_unreachable = false;
            if (quest_poi_map >= 0 && uint32(quest_poi_map) != bot_map)
                if (::MapEntry const* me = sMapStore.LookupEntry(uint32(quest_poi_map)))
                    quest_poi_map_unreachable = me->Instanceable();
            // Sequenced-predecessor mask: track each completed objective's
            // index. An objective with QUEST_OBJECTIVE_FLAG_SEQUENCED is
            // gated on every prior objective being complete.
            bool prior_incomplete = false;
            for (auto const& o : q.objectives)
            {
                const bool done = (o.amount > 0 && o.progress >= o.amount)
                                   || (o.amount == 0);
                if (o.flags & /*OPTIONAL*/ 0x0004) { /* skip without affecting prior_incomplete */ }
                else if (!done)
                {
                    if ((o.flags & /*SEQUENCED*/ 0x0002) && prior_incomplete)
                    {
                        // Locked behind a previous incomplete objective; can't
                        // progress this yet. Stop scanning this quest — later
                        // objectives are also locked.
                        break;
                    }
                    // Type filter: only objectives we have execution rules for.
                    // Add new types here when their idle:quest_* rule lands.
                    bool actionable = false;
                    switch (o.type)
                    {
                        case QUEST_OBJECTIVE_MONSTER:
                        case QUEST_OBJECTIVE_ITEM:
                        case QUEST_OBJECTIVE_GAMEOBJECT:
                        case QUEST_OBJECTIVE_TALKTO:
                        case QUEST_OBJECTIVE_AREATRIGGER:
                        case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
                        case QUEST_OBJECTIVE_KILL_WITH_LABEL:
                            actionable = true;
                            break;
                        default:
                            break;
                    }
                    if (actionable)
                    {
                        // Skip if BotAI has blacklisted this objective.
                        if (bot_ai && bot_ai->objective_blacklisted(q.quest_id, o.id, now_ms))
                        {
                            // Treat as "still incomplete" for SEQUENCED gating
                            // purposes, then continue scanning later objectives
                            // and other quests for an alternative.
                            prior_incomplete = true;
                            continue;
                        }
                        if (!best_any_set && !quest_poi_map_unreachable &&
                            ObjectiveHasSameMapSpawn(o, p->GetPositionX(), p->GetPositionY(), bot_map))
                        {
                            best_any = o;
                            best_any_quest = q.quest_id;
                            best_any_set = true;
                        }
                        if (quest_poi_map == int32(bot_map))
                        {
                            // LOCAL-WORK distance gate (cross-zone same-map fix):
                            // "same POI map == bot map" on a continent map can be a
                            // multi-zone trek; treat an objective as LOCAL work only
                            // when its POI area's NEAR EDGE is within one zone (~1500y).
                            //
                            // NEAREST wins (2026-06-20): pick the CLOSEST local
                            // actionable objective, not the first in scan order. The
                            // old first-match stranded a bot holding a TRIVIAL
                            // objective at its feet behind a far one earlier in the
                            // quest list (verified via [talkto_trace]: Durnan picked
                            // Q26131's TALKTO ~1493y away over Q26118's Moira
                            // Thaurissan standing ON him, then pursued the far one
                            // forever and never talked to Moira). Distance is the same
                            // cheap planar estimate the local gate already uses
                            // (POI-blob centroid / nearest-spawn index — NO pathfinding,
                            // so safe on the Build thread). Unresolved position →
                            // treated as local-but-far so it loses to any resolved,
                            // closer objective but still beats nothing.
                            float ox = 0.f, oy = 0.f, orad = 0.f;
                            float edge = 1400.0f;   // unresolved fallback (local-but-far)
                            if (EstimateObjectivePlanarPos(quest_poi, o,
                                    p->GetPositionX(), p->GetPositionY(), bot_map, ox, oy, orad))
                            {
                                const float ddx = ox - p->GetPositionX();
                                const float ddy = oy - p->GetPositionY();
                                edge = std::max(0.f,
                                    std::sqrt(ddx * ddx + ddy * ddy) - orad);
                            }
                            constexpr float kFarObjective = 1500.0f;  // ~one zone
                            if (edge <= kFarObjective && edge < best_same_map_dist)
                            {
                                best_same_map = o;
                                best_same_map_quest = q.quest_id;
                                best_same_map_set = true;
                                best_same_map_dist = edge;
                            }
                            // No early-exit: keep scanning ALL objectives/quests so a
                            // closer one later in the list can still win. A far
                            // (>1500y) objective is left for best_any as a last resort;
                            // a breadcrumb turn-in still outranks it (selection below).
                        }
                        // Continue scanning later objectives in this quest in
                        // case one of them happens to be on the bot's map.
                        // (Most quests' objectives share a single map but
                        // multi-stage quests can span maps.)
                        prior_incomplete = true;
                        continue;
                    }
                    // Non-actionable but blocking — record as "prior incomplete"
                    // so a SEQUENCED objective after it stays locked.
                    prior_incomplete = true;
                }
            }
            // Early-out: a same-map turn-in (breadcrumb) outranks ALL actionable
            // objectives (selection order below), so once one is found we can stop —
            // no actionable objective, however near, will be chosen over it. We do
            // NOT break merely on best_same_map_set: the nearest-objective search
            // must keep scanning later quests for a closer one.
            if (best_breadcrumb_sm_set) break;
        }
        // TURN-IN PRIORITY (audit C08/C14): a SAME-MAP completed-quest turn-in
        // outranks every incomplete objective. The old order put turn-ins
        // (the "breadcrumb" candidates) at the BOTTOM, so a bot holding any
        // incomplete actionable objective never banked its completed quests —
        // fleet-wide, 21,007 COMPLETE quests sat unrewarded (avg 57.8 days),
        // forfeiting the single largest XP source in the leveling loop AND
        // the follow-up quests their rewards unlock. Humans turn in promptly:
        // it's the payday. Same-map ender only — a cross-map turn-in still
        // ranks below local actionable work (travel cost), but above nothing.
        // DISTANCE-AWARE turn-in vs near objective (2026-06-20): a same-map turn-in
        // is "payday" and gets a head-start bias, but it must NOT outrank a near
        // actionable objective that is MUCH closer — else a complete quest whose
        // ender is ~1100y away beats a use-item/kill objective at the bot's feet
        // (observed: Durnan, bc_sm Q25986 ender 1168y chosen over best_same_map
        // Q26118 use-item 210y, so the sledgehammer never got used). Prefer the
        // turn-in only when it isn't dramatically farther than the near objective.
        constexpr float kTurnInBiasYards = 250.0f;
        const bool prefer_turnin = best_breadcrumb_sm_set &&
            (!best_same_map_set ||
             best_breadcrumb_sm_dist <= best_same_map_dist + kTurnInBiasYards);
        // In an active dungeon/raid INSTANCE, do NOT lock onto a personal quest
        // objective. The member must run the dungeon with the GROUP — navigation is
        // owned by the dungeon boss-priority navigator + the cohesion follow/rejoin
        // rules. A quest POI pulls a member off the group (even a same-map one inside
        // the instance, and especially a cross-continent one): it wanders to the POI,
        // gets pinned on quest mobs, never rejoins, and the tank holds for it forever
        // (observed live 2026-06-26 Deadmines: 3/5 followers latched onto Q27781 /
        // cross-continent Q25705 POIs, the run DEADLOCKED at 1 boss with the tank in
        // idle:dungeon_hold). Leave current_objective UNSET so the dungeon rules own
        // movement; quests resume automatically on exit (overland map).
        const bool in_dungeon_run = p->GetMap() && p->GetMap()->IsDungeon();
        if (in_dungeon_run)
        {
            // suppressed — current_objective stays unset; group-follow drives nav
        }
        else if (prefer_turnin)
        {
            snap->quest_log.current_objective = best_breadcrumb_sm;
            snap->quest_log.current_quest_id  = best_breadcrumb_sm_quest;
        }
        else if (best_same_map_set)
        {
            snap->quest_log.current_objective = best_same_map;
            snap->quest_log.current_quest_id  = best_same_map_quest;
        }
        else if (best_breadcrumb_set)
        {
            // No LOCAL work. A completed-quest turn-in (breadcrumb) now outranks
            // best_any (a FAR incomplete objective): the turn-in is GUARANTEED
            // progress (bank the XP — payday) and its ender is the natural travel
            // waypoint that breadcrumbs the bot toward its next zone, whereas a
            // far incomplete objective may have no navmesh route and just wedges.
            // This is the cross-zone fix's second half — without it, best_any (an
            // ungated far objective) would still win over the reachable turn-in.
            // Cross-map/far turn-in engages the travel pipeline (flight/ship/
            // portal) to the ender. (Near same-map turn-ins are best_breadcrumb_sm,
            // already handled above at top priority; near LOCAL work is
            // best_same_map, above — so this only fires when nothing is near.)
            snap->quest_log.current_objective = best_breadcrumb;
            snap->quest_log.current_quest_id  = best_breadcrumb_quest;
        }
        else if (best_any_set)
        {
            // Last resort: a far incomplete objective with no completed turn-in
            // to breadcrumb us. Pursue it (engages the travel pipeline) — better
            // than standing idle.
            snap->quest_log.current_objective = best_any;
            snap->quest_log.current_quest_id  = best_any_quest;
        }

        // [picker_dump] TEMP (2026-06-20) — definitive selector ground truth:
        // logs ALL candidate slots + the final choice every Build, throttled 3s/bot.
        // Shows whether best_same_map (near objective) is even set, its distance, and
        // whether best_breadcrumb_sm (a same-map turn-in) outranks it. REMOVE after
        // the Durnan picker root is fixed + verified.
        {
            static thread_local std::unordered_map<uint64, uint32> s_pd_last;
            const uint64 pdkey = p->GetGUID().GetCounter();
            auto it = s_pd_last.find(pdkey);
            if (it == s_pd_last.end() || now_ms - it->second > 3000u)
            {
                s_pd_last[pdkey] = now_ms;
                TC_LOG_INFO("playerbot.v2",
                    "[picker_dump] bot={} chosen_quest={} obj_type={} | same_map(set={} q={} dist={}) "
                    "bc_sm(set={} q={}) bc(set={} q={}) any(set={} q={})",
                    snap->identity.name, snap->quest_log.current_quest_id,
                    uint32(snap->quest_log.current_objective.type),
                    best_same_map_set ? 1 : 0, best_same_map_quest,
                    best_same_map_set ? best_same_map_dist : -1.f,
                    best_breadcrumb_sm_set ? 1 : 0, best_breadcrumb_sm_quest,
                    best_breadcrumb_set ? 1 : 0, best_breadcrumb_quest,
                    best_any_set ? 1 : 0, best_any_quest);
            }
        }

        // DIAG [picker_choice]: a same-map objective was chosen WHILE a cross-map
        // breadcrumb (a completed quest's turn-in) was ALSO available — the exact
        // conflict that parks a bot whose real goal is across a zeppelin (Somi:
        // L40 Silithus quest picked over the Tirisfal turn-in). Logs the winner +
        // alternatives so we can confirm which junk quest hijacks the objective.
        // Gated on this specific conflict, so it's not fleet-wide spam.
        // Per-bot dedup: this branch re-evaluates EVERY Build tick, and a bot
        // parked in the conflict steady-state used to write the identical
        // line thousands of times (observed 2026-06-13: 5,700+ lines for one
        // bot). Log only when the decision tuple changes for this bot.
        if (best_same_map_set && (best_breadcrumb_set || best_breadcrumb_sm_set))
        {
            const uint64 pc_sig =
                (uint64(snap->quest_log.current_quest_id) << 32) ^
                (uint64(best_breadcrumb_set ? best_breadcrumb_quest : 0u) << 16) ^
                uint64(best_breadcrumb_sm_set ? best_breadcrumb_sm_quest : 0u);
            // Per-bot log-dedup signature lives on the bot's own BotAI
            // (Phase 4 parallel-by-Map*): formerly a process-wide BotId-keyed
            // static map, now single-writer per worker. When the AI can't be
            // resolved we emit unconditionally (correct, just not deduped).
            BotAI* pc_ai = bot_ai;
            const uint64 last_sig = pc_ai ? pc_ai->picker_choice_log_sig() : (pc_sig ^ 1ull);
            if (last_sig != pc_sig)
            {
                if (pc_ai) pc_ai->set_picker_choice_log_sig(pc_sig);
                TC_LOG_INFO("playerbot.v2",
                    "[picker_choice] {} lvl={} map={} PICKED quest={} (same_map) WHILE "
                    "breadcrumb available | alt: any={} bc={} bc_sm={}",
                    p->GetName(), uint32(p->GetLevel()), bot_map,
                    snap->quest_log.current_quest_id,
                    best_any_set ? best_any_quest : 0u,
                    best_breadcrumb_set ? best_breadcrumb_quest : 0u,
                    best_breadcrumb_sm_set ? best_breadcrumb_sm_quest : 0u);
            }
        }

        // DIAG [picker_breadcrumb]: cross-zone fix in action — no LOCAL work, so
        // a completed-quest turn-in (breadcrumb) was chosen OVER a far incomplete
        // objective (best_any). Confirms a bot whose only same-map work is a
        // cross-zone trek now follows the reachable breadcrumb instead of wedging
        // (Tindle: Q270 turn-in at Glorin/Dun Morogh over Q27635's unreachable
        // kill). Mutually exclusive with [picker_choice] (that needs
        // best_same_map_set), so it safely shares the per-bot dedup signature.
        if (!best_same_map_set && !best_breadcrumb_sm_set
            && best_breadcrumb_set && best_any_set
            && snap->quest_log.current_quest_id == best_breadcrumb_quest)
        {
            const uint64 pb_sig =
                (uint64(best_breadcrumb_quest) << 32) ^ uint64(best_any_quest) ^ 0x1ull;
            BotAI* pb_ai = bot_ai;
            const uint64 last_sig = pb_ai ? pb_ai->picker_choice_log_sig() : (pb_sig ^ 1ull);
            if (last_sig != pb_sig)
            {
                if (pb_ai) pb_ai->set_picker_choice_log_sig(pb_sig);
                TC_LOG_INFO("playerbot.v2",
                    "[picker_breadcrumb] {} lvl={} map={} chose turn-in quest={} over "
                    "far objective quest={} (no local work)",
                    p->GetName(), uint32(p->GetLevel()), bot_map,
                    best_breadcrumb_quest, best_any_quest);
            }
        }

        // DIAG [picker_none]: the bot ended up with NO current objective even
        // though it holds a COMPLETE (state==1) quest that SHOULD have surfaced
        // a turn-in. Logs the candidate flags + every quest's state/objective-
        // count/ender so we can see exactly why a stuck bot (e.g. Somi: Undead
        // starter, cross-map turn-in) surfaces nothing and parks. Gated tightly
        // (a complete quest must be present) so it's rare, not fleet-wide spam.
        if (snap->quest_log.current_quest_id == 0)
        {
            bool has_complete = false;
            for (auto const& dq : snap->quest_log.quests)
                if (dq.state == 1) { has_complete = true; break; }
            if (has_complete)
            {
                TC_LOG_INFO("playerbot.v2",
                    "[picker_none] {} quests={} flags[sm={} any={} bc_sm={} bc={}] bot_map={}",
                    snap->identity.name, uint32(snap->quest_log.quests.size()),
                    best_same_map_set ? 1 : 0, best_any_set ? 1 : 0,
                    best_breadcrumb_sm_set ? 1 : 0, best_breadcrumb_set ? 1 : 0,
                    bot_map);
                for (auto const& dq : snap->quest_log.quests)
                    TC_LOG_INFO("playerbot.v2",
                        "[picker_none]   q{} state={} objs={} ender_resolved={} ender_map={}",
                        dq.quest_id, uint32(dq.state), uint32(dq.objectives.size()),
                        dq.ender_resolved ? 1 : 0, dq.ender_map_id);
            }
        }
    }

    // ---- Quest-target neutral expansion ----
    // The normal nearby_enemies scan uses AnyUnfriendlyUnitInObjectRangeCheck
    // which filters out neutral creatures. Many starter-zone quests target
    // *neutral* mobs (Mottled Boars in Valley of Trials, Scorpid Workers in
    // Durotar, etc.) — those NEVER appear in nearby_enemies, so the
    // inlined idle:quest_kill block at State_Idle.cpp:4258 cannot find them
    // and the bot wanders forever. This pass adds an extra creature scan
    // that gathers neutral creatures whose entry matches an in-progress
    // MONSTER / KILL_WITH_LABEL objective; the bot's IsValidAttackTarget
    // check still applies (so we don't accidentally engage immune /
    // friendly-faction NPCs), but the faction gate is relaxed to "neutral
    // is okay if it's a quest target". Hostile creatures are already in
    // nearby_enemies from the main scan — no double-add (we de-dup on guid).
    // R9b grind-when-starved: a bot with NO actionable quest (current_quest_id
    // == 0) would wander to 0 XP. With RETAIL LEVEL SCALING every nearby mob
    // awards XP, so we run the same neutral-creature scan for starved bots and
    // (in the admit filter below) accept ANY attackable creature, not only quest
    // targets — idle:engage_nearby_mob then grinds the nearest for XP. Gated to
    // the starved case so it never costs an actively-questing bot a scan.
    // C12 (audit 2026-06-10): "starved" must ALSO cover a bot whose picked
    // objective is wedged. The old gate was current_quest_id == 0 only — but a
    // bot holding an unexecutable/blacklisted objective keeps a non-zero quest
    // id forever, so it neither progressed quests NOR ground for XP (the worst
    // of both). Treat a currently-blacklisted objective as starvation so the
    // grind fallback feeds XP while the picker sorts itself out.
    BotAI* grind_ai = bot_ai;
    const bool cur_obj_blacklisted =
        grind_ai && grind_ai->current_objective_blacklisted(GameTime::GetGameTimeMS());
    // Publish to the snapshot so quest-first idle gates (HasActionableQuest) can
    // release maintenance suppression for a bot wedged on an unreachable POI.
    snap->quest_log.current_objective_blacklisted = cur_obj_blacklisted;
    const bool grind_starved =
        (snap->quest_log.current_quest_id == 0) || cur_obj_blacklisted;
    if (!snap->quest_log.quests.empty() || grind_starved)
    {
        std::unordered_set<uint32> quest_target_entries;
        quest_target_entries.reserve(16);
        for (auto const& q : snap->quest_log.quests)
        {
            if (q.state != 0) continue;  // 0 = INCOMPLETE; 1 = COMPLETE → nothing left to kill
            for (auto const& oe : q.objectives)
            {
                if (oe.progress >= oe.amount) continue;
                if (oe.type == QUEST_OBJECTIVE_MONSTER)
                {
                    if (oe.object_id > 0)
                        quest_target_entries.insert(uint32(oe.object_id));
                    for (uint32 a : oe.credit_alias_entries)
                        quest_target_entries.insert(a);
                }
                else if (oe.type == QUEST_OBJECTIVE_KILL_WITH_LABEL)
                {
                    for (uint32 t : oe.labeled_target_entries)
                        quest_target_entries.insert(t);
                }
                else if (oe.type == QUEST_OBJECTIVE_ITEM)
                {
                    // Item-collect objective: the bot must KILL the creatures that
                    // drop the quest item (creature_questitem), then loot it. Without
                    // this, a pure collect quest yields ZERO kill targets, so the
                    // scan falls back to the bot's OTHER quests' mobs and the collect
                    // quest never progresses — Morthan ground Worgen (Q24993) forever
                    // while Q24990's item 2858 droppers (Darkhounds 1547/1548/1549)
                    // went untouched. (GO/ground-spawn quest items are handled by the
                    // QuestItem*Spawn POI resolvers; this covers the kill-to-loot case.)
                    if (oe.object_id > 0)
                        for (uint32 cre : QuestItemCreatureEntries(uint32(oe.object_id)))
                            quest_target_entries.insert(cre);
                }
            }
        }
        // Counterpart diagnostic: when quest_target_entries is EMPTY but
        // the bot has incomplete quests, the scan block below is skipped
        // entirely and no [neutral_scan_miss] line ever fires. This is the
        // Uraimus/Irothoth case — bot has a kill quest in log but the
        // objective enumeration above did NOT produce a target entry.
        // Possible causes: q.state != 0 for the only kill quest, progress
        // >= amount already, all objectives are non-MONSTER types, or
        // q.objectives is empty (breadcrumb). One log line per bot per 60s.
        if (quest_target_entries.empty())
        {
            BotAI* diag_ai = bot_ai;
            if (diag_ai)
            {
                const uint32 diag_now_ms = GameTime::GetGameTimeMS();
                const uint32 last = diag_ai->last_quest_diag_ms();
                if (last == 0 || (diag_now_ms - last) >= 60u * 1000u)
                {
                    diag_ai->set_last_quest_diag_ms(diag_now_ms);
                    // Aggregate qlog structure so we can tell what kind of
                    // quests the bot has but couldn't enumerate targets for.
                    uint32 n_complete = 0, n_no_objs = 0, n_other_type = 0,
                           n_done_obj = 0, n_kill_incomplete = 0;
                    for (auto const& q : snap->quest_log.quests)
                    {
                        if (q.state != 0) { ++n_complete; continue; }
                        if (q.objectives.empty()) { ++n_no_objs; continue; }
                        for (auto const& oe : q.objectives)
                        {
                            if (oe.progress >= oe.amount) { ++n_done_obj; continue; }
                            if (oe.type == QUEST_OBJECTIVE_MONSTER ||
                                oe.type == QUEST_OBJECTIVE_KILL_WITH_LABEL)
                                ++n_kill_incomplete;   // a real kill objective we should have resolved
                            else
                                ++n_other_type;        // gather / talk / explore / use-item — no kill target expected
                        }
                    }
                    // Only log the genuinely-anomalous case: the bot HAS an
                    // incomplete kill objective yet we resolved zero target
                    // entries (broken entry/KillCredit mapping). The common
                    // states — every quest complete-awaiting-turnin, or only
                    // non-kill (gather/talk) objectives left — are normal and
                    // were ~90% of the old 24k-line spam.
                    if (n_kill_incomplete > 0)
                        TC_LOG_INFO("playerbot.v2",
                            "[no_quest_kill_targets] bot={} qlog={} "
                            "n_complete={} n_no_objs={} n_done_obj={} n_other_type={} n_kill_incomplete={}",
                            p->GetName(), uint32(snap->quest_log.quests.size()),
                            n_complete, n_no_objs, n_done_obj, n_other_type, n_kill_incomplete);
                }
            }
        }
        if (!quest_target_entries.empty() || grind_starved)
        {
            // Two-pass scan: first 80y around the BOT, then up to 160y
            // around the active POI center if the POI lives on this map.
            // Rationale (2026-05-19 wander_reason analysis): the top
            // wander-cascade pattern is "bot is at POI, objective_type=
            // MONSTER/ITEM, poi_valid=1, target not in nearby_enemies".
            // POIs in TC's quest_poi table commonly cover 50-150y polygons;
            // a bot that arrives at the POI center sits ~80-100y from
            // mobs at the polygon edge. The single 80y-around-bot scan
            // missed them, the bot fell through every quest rule, and
            // wander fired. POI-centered scan plugs that hole.
            // thread_local scratch reused across builds — avoids the
            // per-node std::list heap allocs of the two CreatureListSearcher
            // passes. clear()+reserve() retains capacity. Pass-1 (80y bot)
            // then pass-2 (160y POI) append in order; the downstream cap
            // (kMaxAppend) relies on that order, which vector preserves.
            thread_local std::vector<Creature*> q_creatures;
            q_creatures.clear();
            q_creatures.reserve(128);

            // Pass 1: 80y around the bot (catches near-bot spawns)
            {
                constexpr float kQTRadiusBot = 80.0f;
                Trinity::AnyUnitInObjectRangeCheck qt_check(p, kQTRadiusBot);
                Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
                    qt_searcher(p, q_creatures, qt_check);
                Cell::VisitAllObjects(p, qt_searcher, kQTRadiusBot);
            }

            // Pass 2: up to 160y around the POI center if the POI is
            // on the bot's current map AND we're far enough from the
            // bot that the first pass missed it. Skip when POI is on
            // a different map (cross-map travel handles that).
            auto const& poi = snap->quest_log.current_objective_poi;
            if (poi.valid && poi.map_id == snap->position.map_id)
            {
                const float pdx = poi.x - p->GetPositionX();
                const float pdy = poi.y - p->GetPositionY();
                const float pdz = poi.z - p->GetPositionZ();
                const float poi_dist_sq = pdx*pdx + pdy*pdy + pdz*pdz;
                // Fire the wider 160y pass whenever the bot is within 160y of
                // the POI center. Quest POI polygons span 50-150y; a bot sitting
                // at the polygon EDGE (50-150y from center) with its target mobs
                // on the far side was missed by both the 80y bot-scan and the
                // old <50y trigger — it then fell through every quest rule and
                // wandered. Widening the trigger to the scan radius (160y) closes
                // that gap so a bot anywhere near a large POI finds its targets.
                if (poi_dist_sq < (160.0f * 160.0f))
                {
                    // Use a TemporarySummon? No — we need a coord-anchored
                    // scan. The cleanest is to scan around the bot with a
                    // wider radius that covers (bot-to-POI distance + POI
                    // polygon radius). For a "bot near POI center" case,
                    // 160y from the bot covers a 100y-radius polygon.
                    constexpr float kQTRadiusPOI = 160.0f;
                    // thread_local scratch reused across builds (distinct from
                    // q_creatures, which we merge into below).
                    thread_local std::vector<Creature*> poi_creatures;
                    poi_creatures.clear();
                    poi_creatures.reserve(128);
                    Trinity::AnyUnitInObjectRangeCheck poi_check(p, kQTRadiusPOI);
                    Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
                        poi_searcher(p, poi_creatures, poi_check);
                    Cell::VisitAllObjects(p, poi_searcher, kQTRadiusPOI);
                    // Merge into q_creatures (de-duped via the seen set below).
                    for (Creature* c : poi_creatures)
                        q_creatures.push_back(c);
                }
            }

            std::unordered_set<ObjectGuid> already_in_enemies;
            already_in_enemies.reserve(snap->combat.nearby_enemies.size() + 16);
            for (auto const& e : snap->combat.nearby_enemies)
                already_in_enemies.insert(e.guid);

            // Diagnostic accumulators — surface why neutrals aren't admitted.
            // Throttled to 1/bot/60s via BotAI::last_quest_diag_ms.
            size_t scanned         = q_creatures.size();
            size_t skip_dead       = 0;
            size_t skip_not_target = 0;
            size_t skip_already_in = 0;
            size_t skip_not_valid  = 0;
            size_t skip_noncombat  = 0;
            uint32 sample_entry    = 0;     // entry of last creature that hit skip_not_valid
            uint32 sample_faction  = 0;

            constexpr size_t kMaxAppend = 24;     // doubled for POI pass
            size_t appended = 0;
            for (Creature* c : q_creatures)
            {
                if (!c || appended >= kMaxAppend) break;
                if (!c->IsAlive())                                   { ++skip_dead; continue; }
                // R9b: when starved (no quest target), admit ANY attackable
                // creature for grinding — level scaling makes them all give XP.
                // IsValidAttackTarget + non-combat-pet checks below still apply.
                if (!grind_starved && !quest_target_entries.count(c->GetEntry())) { ++skip_not_target; continue; }
                if (already_in_enemies.count(c->GetGUID()))          { ++skip_already_in; continue; }
                // IsValidAttackTarget covers immune / faction-friendly /
                // visibility — true for "neutral but attackable" creatures
                // (the common starter case). Friendly NPCs that happen to
                // share an entry with a quest target slip through here:
                // false = skip.
                if (!p->IsValidAttackTarget(c))
                {
                    ++skip_not_valid;
                    sample_entry   = c->GetEntry();
                    sample_faction = c->GetFaction();
                    continue;
                }
                if (c->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET)
                                                                     { ++skip_noncombat; continue; }

                NearbyUnit u{};
                u.guid   = c->GetGUID();
                u.entry  = c->GetEntry();
                u.level  = c->GetLevelForTarget(p);   // per-viewer scaled level (grind XP/target uses fought level)
                u.hp     = static_cast<int32>(c->GetHealth());
                u.max_hp = static_cast<int32>(c->GetMaxHealth());
                u.x      = c->GetPositionX();
                u.y      = c->GetPositionY();
                u.z      = c->GetPositionZ();
                u.o      = c->GetOrientation();
                if (Unit const* v = c->GetVictim()) u.victim = v->GetGUID();
                snap->combat.nearby_enemies.push_back(u);
                already_in_enemies.insert(c->GetGUID());
                ++appended;
            }

            // Per-bot diagnostic: log scan outcome when nothing got admitted
            // AND the bot has quest target entries to find. One line per bot
            // per 60s. Helps identify why "go kill 6 boars" sits forever.
            BotAI* diag_ai = bot_ai;
            // Success counterpart: when neutrals ARE admitted, log so we can
            // disambiguate "no diag = scan worked" from "no diag = code
            // path skipped". If Uraimus shows neutral_scan_ok but no
            // engagement, the bug is downstream of the snapshot.
            if (appended > 0 && diag_ai)
            {
                const uint32 ok_now_ms = GameTime::GetGameTimeMS();
                const uint32 last_ok = diag_ai->last_quest_diag_ms();
                if (last_ok == 0 || (ok_now_ms - last_ok) >= 60u * 1000u)
                {
                    diag_ai->set_last_quest_diag_ms(ok_now_ms);
                    TC_LOG_INFO("playerbot.v2",
                        "[neutral_scan_ok] bot={} appended={} scanned={} sample_entry={}",
                        p->GetName(), uint32(appended), uint32(scanned), sample_entry);
                }
            }
            if (appended == 0 && diag_ai)
            {
                const uint32 diag_now_ms = GameTime::GetGameTimeMS();
                const uint32 last = diag_ai->last_quest_diag_ms();
                if (last == 0 || (diag_now_ms - last) >= 60u * 1000u)
                {
                    diag_ai->set_last_quest_diag_ms(diag_now_ms);
                    // List quest_target_entries for visibility (cap to 6).
                    std::string targets;
                    size_t shown = 0;
                    for (uint32 e : quest_target_entries)
                    {
                        if (shown >= 6) { targets += ",..."; break; }
                        if (shown) targets.push_back(',');
                        targets += std::to_string(e);
                        ++shown;
                    }
                    // Phase footprint: number of distinct phase ids in the
                    // bot's PhaseShift. Empty/0 means PhasingHandler::OnMapChange
                    // didn't run (or ran without populating). DEFAULT_PHASE
                    // alone yields size=1 with phase 169. Quest-conditional
                    // phases (Cataclysm post-Shattering, MoP etc.) add to this.
                    PhaseShift const& ps = p->GetPhaseShift();
                    const uint32 phase_count = uint32(ps.GetPhases().size());
                    TC_LOG_INFO("playerbot.v2",
                        "[neutral_scan_miss] bot={} targets=[{}] scanned={} "
                        "dead={} not_target={} dup={} not_valid={} noncombat={} "
                        "sample_entry={} sample_faction={} phases={}",
                        p->GetName(), targets, uint32(scanned),
                        uint32(skip_dead), uint32(skip_not_target),
                        uint32(skip_already_in), uint32(skip_not_valid),
                        uint32(skip_noncombat),
                        sample_entry, sample_faction, phase_count);
                }
            }
        }
    }

    // ---- Quest POI lookup for current objective ----
    // Pre-resolve the (map, x, y, z) waypoint for the picked objective so
    // State_Idle's idle:quest_path rule can emit move_to without doing the
    // POI lookup on the worker thread. Match priority:
    //   1. QuestObjectiveID == obj.id (db row match — most precise)
    //   2. QuestObjectID == obj.object_id (creature/item/go entry match)
    //   3. ObjectiveIndex == obj.storage_index (positional fallback)
    // Use the first Point of the matched blob — for multi-point blobs (e.g.
    // patrol routes), the first point is a reasonable approach target;
    // refinement to centroid / closest point can come later.
    if (snap->quest_log.current_quest_id != 0)
    {
        // USE-START-ITEM objective: POI = the QuestPOI target-area centroid (the
        // blob with the most points; the giver/start blob is a single point). Done
        // up front because the id/object_id/storage_index blob matchers below can't
        // key off a synthesized item objective, and the ITEM-type spawn fallback
        // would mis-resolve the StartItem entry as a world item spawn.
        if (snap->quest_log.current_objective.use_start_item)
        {
            QuestPOIData const* uip = sObjectMgr->GetQuestPOIData(int32(snap->quest_log.current_quest_id));
            if (uip && !uip->Blobs.empty())
            {
                QuestPOIBlobData const* big = nullptr;
                for (auto const& b : uip->Blobs)
                    if (!b.Points.empty() &&
                        (!big || b.Points.size() > big->Points.size()))
                        big = &b;
                if (big)
                {
                    double sx = 0.0, sy = 0.0;
                    for (auto const& pt : big->Points) { sx += double(pt.X); sy += double(pt.Y); }
                    const float cenx = float(sx / double(big->Points.size()));
                    const float ceny = float(sy / double(big->Points.size()));
                    auto& cp = snap->quest_log.current_objective_poi;
                    cp.map_id = uint32(big->MapID);
                    // Anchor on the NEAREST creature spawn to the centroid: QuestPOI
                    // points are 2D, so the raw centroid sits on the GROUND floor, but
                    // the target can be UPSTAIRS (Slaghammer z511 vs ground z502). The
                    // nearest spawn is on the target's floor — its 3D position (incl. z)
                    // routes the bot UP the navmesh-verified stairs. Falls back to the
                    // 2D centroid at the bot's z when no spawn is near.
                    if (::CreatureData const* near_sp =
                            NearestCreatureSpawnToPoint(cenx, ceny, uint32(big->MapID), 30.0f))
                    {
                        cp.x = near_sp->spawnPoint.GetPositionX();
                        cp.y = near_sp->spawnPoint.GetPositionY();
                        cp.z = near_sp->spawnPoint.GetPositionZ();
                    }
                    else
                    {
                        cp.x = cenx;
                        cp.y = ceny;
                        cp.z = p->GetPositionZ();
                    }
                    cp.radius = 12.0f;
                    cp.valid  = true;
                }
            }
        }
        QuestPOIData const* poi = sObjectMgr->GetQuestPOIData(int32(snap->quest_log.current_quest_id));
        if (poi && !poi->Blobs.empty() && !snap->quest_log.current_objective.use_start_item)
        {
            auto const& obj = snap->quest_log.current_objective;
            QuestPOIBlobData const* matched = nullptr;
            // Q-P2a: synthesized breadcrumb/turn-in objectives carry id==0.
            // Many POI overview blobs also have QuestObjectiveID==0, so an
            // unguarded match here would fasten a synth objective onto the
            // wrong (overview) blob. Only use the QuestObjectiveID matcher for
            // real objectives (id!=0); synth objectives fall through to the
            // object_id / ObjectiveIndex matchers below.
            if (obj.id != 0)
                for (auto const& blob : poi->Blobs)
                    if (blob.QuestObjectiveID == int32(obj.id))
                    { matched = &blob; break; }
            if (!matched)
                for (auto const& blob : poi->Blobs)
                    if (blob.QuestObjectID == obj.object_id)
                    { matched = &blob; break; }
            if (!matched)
                for (auto const& blob : poi->Blobs)
                    if (blob.ObjectiveIndex == int32(obj.storage_index))
                    { matched = &blob; break; }
            if (matched && !matched->Points.empty())
            {
                // Single-point POI → use the point as-is (radius=0).
                // Polygon POI (zone) → centroid + max planar radius from
                // centroid to any vertex. Lets State_Idle treat the bot
                // as "at the objective" when inside the polygon, instead
                // of forcing a march to the first vertex while kill
                // targets are scattered across the zone.
                double sx = 0.0, sy = 0.0, sz = 0.0;
                for (auto const& pt : matched->Points)
                {
                    sx += double(pt.X);
                    sy += double(pt.Y);
                    sz += double(pt.Z);
                }
                const double n = double(matched->Points.size());
                const float cx = float(sx / n);
                const float cy = float(sy / n);
                const float cz = float(sz / n);
                float radius_sq = 0.f;
                for (auto const& pt : matched->Points)
                {
                    const float dx = float(pt.X) - cx;
                    const float dy = float(pt.Y) - cy;
                    const float r2 = dx*dx + dy*dy;
                    if (r2 > radius_sq) radius_sq = r2;
                }
                snap->quest_log.current_objective_poi.map_id = uint32(matched->MapID);
                snap->quest_log.current_objective_poi.x      = cx;
                snap->quest_log.current_objective_poi.y      = cy;
                snap->quest_log.current_objective_poi.z      = cz;
                snap->quest_log.current_objective_poi.radius = std::sqrt(radius_sq);
                snap->quest_log.current_objective_poi.valid  = true;
            }
        }
        // Q-P1a: QuestPOI fallback via target spawn. Many quests (especially
        // older starter content) have no quest_poi rows, or no blob matched the
        // active objective. Without a POI the bot has no waypoint and the
        // quest-POI pathing fallback in State_Idle is skipped, so the bot stands
        // still scanning for a target that is out of range (Astianon/Lazy Peons
        // case). Resolve a known spawn of the objective's target and use it as
        // the waypoint so the bot walks toward where the target actually lives.
        // Fallback only — never overrides a real QuestPOI.
        if (!snap->quest_log.current_objective_poi.valid)
        {
            auto const& fobj = snap->quest_log.current_objective;
            // CombatLoop FIX C: resolve the NEAREST same-map spawn of the
            // target entry (not a DB-arbitrary "first" spawn), so a scan-MISS
            // fallback heads to the closest cluster the bot can actually reach.
            const float    bot_x   = p->GetPositionX();
            const float    bot_y   = p->GetPositionY();
            const uint32   bot_map = p->GetMapId();
            ::CreatureData const* cspawn = nullptr;
            ::GameObjectData const* gspawn = nullptr;
            uint32 resolved_entry = 0;     // entry whose spawn we picked (hysteresis key)
            switch (fobj.type)
            {
                case QUEST_OBJECTIVE_MONSTER:
                case QUEST_OBJECTIVE_TALKTO:
                    if (fobj.object_id > 0)
                    {
                        resolved_entry = uint32(fobj.object_id);
                        cspawn = NearestCreatureSpawnByEntry(resolved_entry, bot_x, bot_y, bot_map);
                    }
                    break;
                case QUEST_OBJECTIVE_KILL_WITH_LABEL:
                    if (!fobj.labeled_target_entries.empty())
                    {
                        resolved_entry = fobj.labeled_target_entries.front();
                        cspawn = NearestCreatureSpawnByEntry(resolved_entry, bot_x, bot_y, bot_map);
                    }
                    break;
                case QUEST_OBJECTIVE_GAMEOBJECT:
                    if (fobj.object_id > 0)
                    {
                        resolved_entry = uint32(fobj.object_id);
                        gspawn = NearestGameObjectSpawnByEntry(resolved_entry, bot_x, bot_y, bot_map);
                    }
                    break;
                case QUEST_OBJECTIVE_ITEM:
                    // Item source may be a creature (drop) or a GO (gather). The
                    // item entry is rarely a creature/GO entry itself; the real
                    // link is quest_questitem (chest/herb/mob that grants it) —
                    // resolve that so e.g. the Ban'ethil relic chests are found.
                    if (fobj.object_id > 0)
                    {
                        gspawn = QuestItemGoSpawnNearest(uint32(fobj.object_id), bot_x, bot_y, bot_map);
                        if (!gspawn)
                            cspawn = QuestItemCreatureSpawnNearest(uint32(fobj.object_id), bot_x, bot_y, bot_map);
                        if (!gspawn && !cspawn)
                        {
                            resolved_entry = uint32(fobj.object_id);
                            cspawn = NearestCreatureSpawnByEntry(resolved_entry, bot_x, bot_y, bot_map);
                            if (!cspawn)
                                gspawn = NearestGameObjectSpawnByEntry(resolved_entry, bot_x, bot_y, bot_map);
                        }
                        else
                            resolved_entry = gspawn ? gspawn->id : (cspawn ? cspawn->id : 0);
                    }
                    break;
                default:
                    break;
            }
            // Hysteresis: keep the PREVIOUSLY-selected spawn for this objective
            // unless the freshly-picked one is closer by a margin > the arrive
            // radius (40y). Prevents build-to-build destination flip between two
            // near-equidistant spawns (would oscillate the bot in place).
            const auto apply_sticky = [&](uint32 cand_map, float cand_x, float cand_y, float cand_z,
                                          float radius) {
                BotAI::PoiSpawnSticky const& prev = bot_ai ? bot_ai->poi_spawn_sticky()
                                                           : BotAI::PoiSpawnSticky{};
                float use_x = cand_x, use_y = cand_y, use_z = cand_z;
                uint32 use_map = cand_map;
                const bool same_obj = prev.valid &&
                                      prev.quest_id == snap->quest_log.current_quest_id &&
                                      prev.obj_id   == fobj.id &&
                                      prev.entry    == resolved_entry &&
                                      prev.map_id   == cand_map;
                if (same_obj)
                {
                    // Distances from the bot to the previous vs the candidate
                    // spawn. Stick with the previous unless the candidate beats
                    // it by more than the 40y arrive radius.
                    const float pdx = prev.x - bot_x, pdy = prev.y - bot_y;
                    const float cdx = cand_x - bot_x, cdy = cand_y - bot_y;
                    const float prevDist = std::sqrt(pdx * pdx + pdy * pdy);
                    const float candDist = std::sqrt(cdx * cdx + cdy * cdy);
                    constexpr float kFlipMargin = 40.0f;
                    if (prevDist - candDist <= kFlipMargin)
                    {
                        use_x = prev.x; use_y = prev.y; use_z = prev.z; use_map = prev.map_id;
                    }
                }
                snap->quest_log.current_objective_poi.map_id = use_map;
                snap->quest_log.current_objective_poi.x      = use_x;
                snap->quest_log.current_objective_poi.y      = use_y;
                snap->quest_log.current_objective_poi.z      = use_z;
                snap->quest_log.current_objective_poi.radius = radius;
                snap->quest_log.current_objective_poi.valid  = true;
                if (bot_ai)
                    bot_ai->set_poi_spawn_sticky(BotAI::PoiSpawnSticky{
                        snap->quest_log.current_quest_id, fobj.id, resolved_entry,
                        use_map, use_x, use_y, use_z, true});
            };
            if (cspawn)
                apply_sticky(cspawn->mapId, cspawn->spawnPoint.GetPositionX(),
                             cspawn->spawnPoint.GetPositionY(), cspawn->spawnPoint.GetPositionZ(),
                             40.0f);  // arrive in the area, then scan/engage takes over
            else if (gspawn)
                apply_sticky(gspawn->mapId, gspawn->spawnPoint.GetPositionX(),
                             gspawn->spawnPoint.GetPositionY(), gspawn->spawnPoint.GetPositionZ(),
                             15.0f);
        }
        // Surface-over-cave Z correction. quest_poi Z is the MAP-SURFACE
        // projection; for an objective whose granting entity is in a cave/den
        // BELOW (Ban'ethil relic chests at den-floor z~1255 vs POI z~1462), the
        // bot walks to the hilltop above and never enters (Uraimus). When the
        // granting entity is resolvable, near the POI x/y, and clearly BELOW the
        // POI z, adopt the entity's real position so pathing routes into the den
        // (via the off-mesh entrance bridge) instead of climbing the hill.
        if (snap->quest_log.current_objective_poi.valid &&
            !snap->quest_log.current_objective.use_start_item)  // use-item POI is the QuestPOI centroid; don't re-adopt an entity spawn
        {
            auto const& fo = snap->quest_log.current_objective;
            ::CreatureData const* cs = nullptr;
            ::GameObjectData const* gs = nullptr;
            switch (fo.type)
            {
                case QUEST_OBJECTIVE_MONSTER:
                case QUEST_OBJECTIVE_TALKTO:
                    if (fo.object_id > 0) cs = FirstCreatureSpawnByEntry(uint32(fo.object_id));
                    break;
                case QUEST_OBJECTIVE_GAMEOBJECT:
                    if (fo.object_id > 0) gs = FirstGameObjectSpawnByEntry(uint32(fo.object_id));
                    break;
                case QUEST_OBJECTIVE_ITEM:
                    if (fo.object_id > 0)
                    {
                        gs = QuestItemGoSpawn(uint32(fo.object_id));
                        if (!gs) cs = QuestItemCreatureSpawn(uint32(fo.object_id));
                    }
                    break;
                default: break;
            }
            // An ITEM objective resolves via quest_questitem to the EXACT
            // granting chest/mob — that link is authoritative, so trust it and
            // ignore the POI x/y (the matched blob is often a DIFFERENT relic
            // chest's surface point ~180y away, which is exactly what made the
            // 150y proximity guard reject the real chest and leave Uraimus on
            // the hilltop). For MONSTER/GAMEOBJECT (first-spawn-by-entry, which
            // may be one of many scattered spawns) keep the same-area guard.
            const bool trust = (fo.type == QUEST_OBJECTIVE_ITEM);
            auto& cp = snap->quest_log.current_objective_poi;
            auto adopt = [&](uint32 m, float ex, float ey, float ez)
            {
                if (m != cp.map_id) return;
                if (!trust)
                {
                    const float ddx = ex - cp.x, ddy = ey - cp.y;
                    if (ddx*ddx + ddy*ddy > 150.0f*150.0f) return;  // same area only
                    if (ez > cp.z - 12.0f) return;                  // only when clearly BELOW the POI
                }
                cp.x = ex; cp.y = ey; cp.z = ez;
                cp.radius = std::max(cp.radius, 12.0f);
            };
            if (gs)      adopt(gs->mapId, gs->spawnPoint.GetPositionX(), gs->spawnPoint.GetPositionY(), gs->spawnPoint.GetPositionZ());
            else if (cs) adopt(cs->mapId, cs->spawnPoint.GetPositionX(), cs->spawnPoint.GetPositionY(), cs->spawnPoint.GetPositionZ());
        }
        // For a COMPLETE quest (state==1) the actionable destination is the
        // TURN-IN NPC, not the quest's objective POI. QuestPOI blobs describe
        // where to DO the quest (kill/collect spots); resolving the POI from
        // them sends the bot to the objective area instead of the ender — and
        // for a finished quest that area is frequently unreachable (observed:
        // quest 483's Gnarlpine objective POI at (9713,1538) ~200y up an
        // unpathable Teldrassil cliff, while the turn-in is in Dolanaar),
        // producing a NoPath back-and-forth and a misleading "current task" in
        // the addon. Override the POI with the resolved ender spawn so
        // quest_path, idle:walk_to_quest_ender and the addon all point at the
        // turn-in. (ender_resolved is false for script-spawned enders — those
        // keep the objective-POI fallback, unchanged.)
        {
            uint32 const* qix = snap->quest_log.quests_index.find(snap->quest_log.current_quest_id);
            if (qix && *qix < snap->quest_log.quests.size())
            {
                auto const& cq = snap->quest_log.quests[*qix];
                if (cq.state == 1 && cq.ender_resolved)
                {
                    snap->quest_log.current_objective_poi.map_id = cq.ender_map_id;
                    snap->quest_log.current_objective_poi.x      = cq.ender_x;
                    snap->quest_log.current_objective_poi.y      = cq.ender_y;
                    snap->quest_log.current_objective_poi.z      = cq.ender_z;
                    snap->quest_log.current_objective_poi.radius = 5.0f;
                    snap->quest_log.current_objective_poi.valid  = true;
                }
            }
        }
    }

    // ---- AreaTrigger waypoint for current objective ----
    // For QUEST_OBJECTIVE_AREATRIGGER (10) and AREA_TRIGGER_ENTER (19),
    // the objective's `object_id` is the AreaTrigger.db2 entry. Look up
    // the trigger's continent + position + bounds so the idle:quest_areatrigger
    // rule can walk inside without round-tripping the world thread. Sphere
    // ATs use Radius directly; box ATs use the larger of Length/Width as a
    // conservative outer-arrival bound (the box's diagonal is wider but
    // the bot only needs to be *inside*, and the server fires credit on
    // any point inside the volume).
    if (snap->quest_log.current_quest_id != 0)
    {
        auto const& obj = snap->quest_log.current_objective;
        const bool is_at_obj =
            obj.type == QUEST_OBJECTIVE_AREATRIGGER ||
            obj.type == QUEST_OBJECTIVE_AREA_TRIGGER_ENTER;
        if (is_at_obj && obj.object_id > 0)
        {
            if (AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(uint32(obj.object_id)))
            {
                snap->quest_log.current_objective_areatrigger.entry  = at->ID;
                snap->quest_log.current_objective_areatrigger.map_id = at->ContinentID;
                snap->quest_log.current_objective_areatrigger.x      = at->Pos.X;
                snap->quest_log.current_objective_areatrigger.y      = at->Pos.Y;
                snap->quest_log.current_objective_areatrigger.z      = at->Pos.Z;
                // ShapeType: 0=Sphere, 1=Box, 2=PolygonVertices, 3=Cylinder.
                // Sphere/Cylinder use Radius. Box uses the half-extents in
                // BoxLength/Width; we conservatively expose max(L,W) so a
                // bot arriving at the centroid is guaranteed inside a non-
                // degenerate box. Polygon ATs are rare in starter content
                // and fall back to Radius (DBC sets a fallback radius).
                float radius = at->Radius;
                if (at->BoxLength > radius) radius = at->BoxLength;
                if (at->BoxWidth  > radius) radius = at->BoxWidth;
                if (radius < 1.0f) radius = 5.0f;     // floor for degenerate ATs
                snap->quest_log.current_objective_areatrigger.radius = radius;
                snap->quest_log.current_objective_areatrigger.valid  = true;
            }
        }
    }

    // ---- Stuck-by-terrain tracker update ----
    // Detects bots wedged on geometry pursuing an unreachable target. Per-bot
    // state lives on BotAI (so the AI worker reads/writes; world thread here
    // only updates with the latest movement goal + distance).
    //
    // Goal selection priority: current_objective_poi > nearest quest_turnin
    // > nearest quest_offer. Each bot has one active goal at a time.
    {
        BotAI* stuck_ai = bot_ai;
        if (stuck_ai)
        {
            auto& t = stuck_ai->mutable_stuck_tracker();
            uint32 goal_map = 0;
            float  goal_x = 0.f, goal_y = 0.f, goal_z = 0.f;
            bool   has_goal = false;
            bool   goal_is_quest = false;
            // Highest priority: current objective POI — but ONLY when it is NEAR
            // enough that the bot walks to it DIRECTLY (within ~one zone, 1500y;
            // matches the picker's far-objective gate). A FAR / cross-zone POI is
            // reached via a multi-hop travel plan whose early legs intentionally
            // go AWAY from the goal (to a flight master / dock / portal). Pinning
            // the far POI then makes the tracker read "no progress" while the bot
            // correctly travels — firing Tier-3 'idle:unstick:drop_goal' which
            // blacklists the quest for 60 min MID-JOURNEY (Tindle 2026-06-19:
            // walking WEST to the Stormwind FM at -8841,490 to fly to Dun Morogh,
            // while the tracker measured distance to Glorin -3818,-827 to the NE →
            // distance only grew → quest abandoned → questless wander). For a far
            // goal we fall through to the last-move_to target below (what the bot
            // is actually walking toward, tracked as a NON-quest goal): a moving
            // bot's chunked move_to target changes each tick → goal_changed resets
            // the tracker → no false wedge; a genuinely stuck traveller still gets
            // walk-escape / teleport / hearth recovery, just never quest abandon.
            if (snap->quest_log.current_quest_id != 0 && snap->quest_log.current_objective_poi.valid &&
                snap->quest_log.current_objective_poi.map_id == p->GetMapId())
            {
                const float pdx = snap->quest_log.current_objective_poi.x - p->GetPositionX();
                const float pdy = snap->quest_log.current_objective_poi.y - p->GetPositionY();
                constexpr float kNearGoalSq = 1500.0f * 1500.0f;   // ~one zone
                if (pdx * pdx + pdy * pdy <= kNearGoalSq)
                {
                    goal_map = snap->quest_log.current_objective_poi.map_id;
                    goal_x   = snap->quest_log.current_objective_poi.x;
                    goal_y   = snap->quest_log.current_objective_poi.y;
                    goal_z   = snap->quest_log.current_objective_poi.z;
                    has_goal = true;
                    goal_is_quest = true;
                }
            }
            // Fallback: nearest quest_turnin's giver position (if that NPC
            // is in nearby_friends — only those are positioned).
            if (!has_goal && !snap->quest_discovery.quest_turnins.empty())
            {
                for (auto const& tin : snap->quest_discovery.quest_turnins)
                {
                    for (auto const& u : snap->combat.nearby_friends)
                    {
                        if (u.guid != tin.giver) continue;
                        goal_map = p->GetMapId();
                        goal_x   = u.x; goal_y = u.y; goal_z = u.z;
                        has_goal = true;
                        goal_is_quest = true;
                        break;
                    }
                    if (has_goal) break;
                }
            }
            // Final fallback: the bot's last emitted move_to destination
            // (wander / travel_to_vendor / walk_to_known_portal|dock /
            // walk_to_quest_ender / move_to_corpse / quest_path …). These
            // movement rules previously never fed the tracker, so a bot
            // wedged walking to a fixed unreachable point accrued tens of
            // thousands of [move_blocked] with no recovery. Treated as a
            // non-quest goal so Tier 3 suppresses the rule rather than
            // blacklisting a quest. The 1.5s emit window keeps it fresh
            // only while the bot is actively re-emitting the same target.
            if (!has_goal)
            {
                if (BotAI* mv_ai = bot_ai)
                {
                    float mx = 0.f, my = 0.f, mz = 0.f;
                    if (mv_ai->last_move_to_goal(mx, my, mz, GameTime::GetGameTimeMS()))
                    {
                        goal_map = p->GetMapId();
                        goal_x = mx; goal_y = my; goal_z = mz;
                        has_goal = true;
                        goal_is_quest = false;
                    }
                }
            }

            const uint32 leash_now_ms = GameTime::GetGameTimeMS();
            bool wants_to_travel = false;   // fed to the goal-agnostic leash below
            if (!has_goal)
            {
                // No goal — clear tracker so the next goal starts fresh.
                if (t.active) t = BotAI::StuckTracker{};
            }
            else
            {
                const uint32 now_ms = leash_now_ms;
                const float  bx     = snap->position.x, by = snap->position.y;
                const float  dx     = goal_x - bx, dy = goal_y - by;
                const float  cur_distance = std::sqrt(dx*dx + dy*dy);
                // The bot has a goal it has not yet reached → it should be
                // travelling. Feeds the oscillation leash (survives goal flips).
                // In combat the bot is FIGHTING, not travelling, so it doesn't
                // "want to travel" — keep the leash from counting a fight as a
                // travel-wedge (the dominant Engage "wedge" cluster was just bots
                // in normal sustained combat, stationary-but-busy). 2026-06-17.
                wants_to_travel = (cur_distance > 25.0f) && !snap->vitals.in_combat;

                // Goal changed (new target) → reset tracker.
                const bool goal_changed = !t.active ||
                    t.target_map != goal_map ||
                    std::fabs(t.target_x - goal_x) > 1.f ||
                    std::fabs(t.target_y - goal_y) > 1.f;
                if (goal_changed)
                {
                    t = BotAI::StuckTracker{};
                    t.active = true;
                    t.target_map = goal_map;
                    t.target_x = goal_x;
                    t.target_y = goal_y;
                    t.target_z = goal_z;
                    t.last_distance = cur_distance;
                    t.first_no_progress_ms = now_ms;
                    t.goal_is_quest = goal_is_quest;
                }
                else
                {
                    // Goal kind can flip while the XY stays put (e.g. a quest
                    // POI resolves to the same coords the move rule was already
                    // chasing). Keep it current so Tier 3 picks the right drop.
                    t.goal_is_quest = goal_is_quest;
                    // Real progress = distance dropped by ≥5y since last
                    // snapshot. Below that threshold we count as "no progress"
                    // since terrain wedges typically allow tiny wobbles.
                    constexpr float kProgressDelta = 5.0f;
                    if (cur_distance + kProgressDelta < t.last_distance)
                    {
                        // Made progress — reset.
                        t.last_distance = cur_distance;
                        t.no_progress_ticks = 0;
                        t.first_no_progress_ms = now_ms;
                        t.recovery_tier = 0;
                    }
                    else if (snap->vitals.in_combat)
                    {
                        // PAUSE the no-progress clock during combat. A bot standing
                        // still while fighting is legitimately busy, not stuck
                        // travelling — accruing "no progress" here is what made the
                        // wedge watchdog miscount normal sustained fights as the
                        // dominant Engage "wedge" cluster, and it would also fire
                        // stuck-recovery mid-fight. Reset so a long fight never trips
                        // the watchdog; the clock restarts fresh once combat ends and
                        // the bot is free to travel again. 2026-06-17.
                        t.no_progress_ticks = 0;
                        t.first_no_progress_ms = now_ms;
                        t.recovery_tier = 0;
                    }
                    else
                    {
                        ++t.no_progress_ticks;
                    }
                }
            }
            // Goal-agnostic oscillation leash. Updated every tick regardless of
            // goal changes, so a bot thrashing between goals it can't reach
            // (which keeps resetting the StuckTracker above) is still detected.
            stuck_ai->note_position_leash(snap->position.x, snap->position.y,
                                          leash_now_ms, wants_to_travel);
        }
    }

    // ---- Cross-quest actionable index ----
    // For every active (non-complete, non-optional, non-blacklisted)
    // objective in the bot's log, list the *currently visible* progression
    // sources. Sorted by squared planar distance ascending so the head is
    // the closest actionable thing the bot could chase.
    //
    // Doesn't replace per-type rules' inner search — those still walk
    // nearby_* and apply per-objective gates. This list lets quest_kill /
    // quest_collect_kill / quest_use_go / quest_talk consider any quest
    // in the log instead of only `current_objective`, so kills in a
    // shared zone progress every quest that wants the entry.
    if (!snap->quest_log.quests.empty())
    {
        const float bot_x = snap->position.x;
        const float bot_y = snap->position.y;
        BotAI* batch_ai = bot_ai;
        const uint32 batch_now_ms = GameTime::GetGameTimeMS();
        const uint32 bot_map_id   = p->GetMapId();
        const Difficulty diff = static_cast<Difficulty>(snap->instance_ctx.map_difficulty);
        for (auto const& q : snap->quest_log.quests)
        {
            if (q.state == 1) continue;          // turn-in pending
            // SEQUENCED-objective tracking. The picker logic at
            // BotSnapshotBuilder.cpp:2640-2710 already respects
            // QUEST_OBJECTIVE_FLAG_SEQUENCED — a SEQUENCED objective with
            // a prior incomplete objective is locked at the server; killing
            // its target awards 0 progress. Builder MUST mirror so the
            // dispatcher doesn't burn engagements on locked sub-goals.
            bool prior_incomplete = false;
            for (auto const& o : q.objectives)
            {
                if (o.flags & QUEST_OBJECTIVE_FLAG_OPTIONAL) continue;
                const bool done = (o.amount > 0 && o.progress >= o.amount) ||
                                  (o.amount == 0);
                if (done) continue;
                // SEQUENCED locked by an earlier incomplete sibling — and any
                // later SEQUENCED in this quest is also locked, so we can
                // stop scanning entirely.
                if ((o.flags & QUEST_OBJECTIVE_FLAG_SEQUENCED) && prior_incomplete)
                    break;
                if (batch_ai && batch_ai->objective_blacklisted(q.quest_id, o.id, batch_now_ms))
                { prior_incomplete = true; continue; }

                auto record = [&](ObjectGuid src, float sx, float sy, int32 oid)
                {
                    BotSnapshot::ActionableObjective ao{};
                    ao.quest_id     = q.quest_id;
                    ao.objective_id = o.id;
                    ao.type         = o.type;
                    ao.object_id    = oid;
                    const float dx = sx - bot_x, dy = sy - bot_y;
                    ao.distance_sq  = dx*dx + dy*dy;
                    ao.source_guid  = src;
                    snap->quest_log.actionable_objectives.push_back(ao);
                };

                switch (o.type)
                {
                    case QUEST_OBJECTIVE_MONSTER:
                    {
                        // Match against object_id PLUS KillCredit aliases.
                        for (auto const& u : snap->combat.nearby_enemies)
                        {
                            if (!u.guid.IsCreature() || u.hp <= 0) continue;
                            if (u.entry == uint32(o.object_id))
                            { record(u.guid, u.x, u.y, o.object_id); continue; }
                            for (uint32 alias : o.credit_alias_entries)
                                if (u.entry == alias)
                                { record(u.guid, u.x, u.y, o.object_id); break; }
                        }
                        break;
                    }
                    case QUEST_OBJECTIVE_KILL_WITH_LABEL:
                    {
                        for (auto const& u : snap->combat.nearby_enemies)
                        {
                            if (!u.guid.IsCreature() || u.hp <= 0) continue;
                            for (uint32 le : o.labeled_target_entries)
                                if (u.entry == le)
                                { record(u.guid, u.x, u.y, o.object_id); break; }
                        }
                        break;
                    }
                    case QUEST_OBJECTIVE_ITEM:
                    {
                        // Match nearby_enemies whose drop list carries the item;
                        // and nearby_objects whose GO drop list carries it.
                        for (auto const& u : snap->combat.nearby_enemies)
                        {
                            if (u.hp <= 0) continue;
                            std::vector<uint32> const* drops =
                                sObjectMgr->GetCreatureQuestItemList(u.entry, diff);
                            if (!drops) continue;
                            for (uint32 it_id : *drops)
                                if (it_id == uint32(o.object_id))
                                { record(u.guid, u.x, u.y, o.object_id); break; }
                        }
                        for (auto const& go : snap->world_objects.nearby_objects)
                        {
                            std::vector<uint32> const* drops =
                                sObjectMgr->GetGameObjectQuestItemList(go.entry);
                            if (!drops) continue;
                            for (uint32 it_id : *drops)
                                if (it_id == uint32(o.object_id))
                                { record(go.guid, go.x, go.y, o.object_id); break; }
                        }
                        break;
                    }
                    case QUEST_OBJECTIVE_GAMEOBJECT:
                    {
                        for (auto const& go : snap->world_objects.nearby_objects)
                            if (go.entry == uint32(o.object_id))
                                record(go.guid, go.x, go.y, o.object_id);
                        break;
                    }
                    case QUEST_OBJECTIVE_TALKTO:
                    {
                        for (auto const& u : snap->combat.nearby_friends)
                            if (u.entry == uint32(o.object_id))
                                record(u.guid, u.x, u.y, o.object_id);
                        break;
                    }
                    case QUEST_OBJECTIVE_AREATRIGGER:
                    case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
                    {
                        if (AreaTriggerEntry const* at =
                                sAreaTriggerStore.LookupEntry(uint32(o.object_id)))
                        {
                            if (uint32(at->ContinentID) == bot_map_id)
                                record(ObjectGuid::Empty, at->Pos.X, at->Pos.Y, o.object_id);
                        }
                        break;
                    }
                    default:
                        break;
                }
                // After processing this incomplete objective, mark the
                // quest as having an unfinished step. Subsequent SEQUENCED
                // siblings will short-circuit at the top of the loop.
                prior_incomplete = true;
            }
        }
        std::sort(snap->quest_log.actionable_objectives.begin(),
                  snap->quest_log.actionable_objectives.end(),
                  [](BotSnapshot::ActionableObjective const& a,
                     BotSnapshot::ActionableObjective const& b)
                  { return a.distance_sq < b.distance_sq; });
    }

    // ---- Quest tool discovery for current objective ----
    // The "use the bell on the cultists" / "drink the potion" pattern.
    // For each non-complete active objective, walk the bot's bag for a
    // tool item: any item whose ON_USE spell carries either:
    //   - SPELL_EFFECT_KILL_CREDIT / KILL_CREDIT2 / KILL_CREDIT_LABEL_*
    //     with MiscValue matching the objective's ObjectID, OR
    //   - SPELL_EFFECT_QUEST_COMPLETE with MiscValue matching the active
    //     quest's quest_id (whole-quest one-shot tools).
    // Quest log holds at most ~25 quests, each with a handful of objectives,
    // and bag size is bounded — overall cost ~O(items × spells × effects),
    // dominated by 5-spell items on a dense bag (~100-200 ops per snapshot).
    if (snap->quest_log.current_quest_id != 0)
    {
        auto const& obj = snap->quest_log.current_objective;
        // Avoid this lookup for objective types where the tool pattern is
        // structurally meaningless (currency, money, learn-spell). It's
        // primarily a MONSTER / ITEM / TALKTO trigger.
        const bool tool_relevant =
            obj.type == QUEST_OBJECTIVE_MONSTER ||
            obj.type == QUEST_OBJECTIVE_ITEM    ||
            obj.type == QUEST_OBJECTIVE_TALKTO  ||
            obj.type == QUEST_OBJECTIVE_KILL_WITH_LABEL;
        if (tool_relevant)
        {
            // Walk bag-slots 0..3 (Backpack INVENTORY_SLOT_BAG_0=255 plus
            // 4 actual bag slots BAG_START..BAG_END). For each item, look
            // up its precomputed tool-effect list via GetCachedToolEffects
            // (immutable per item_entry; cache lasts process lifetime).
            auto try_match_item = [&](Item const* item) -> bool
            {
                if (!item) return false;
                ItemTemplate const* tmpl = item->GetTemplate();
                if (!tmpl) return false;
                auto const& effs = GetCachedToolEffects(tmpl);
                for (QuestToolEffect const& te : effs)
                {
                    // Whole-quest tool: SPELL_EFFECT_QUEST_COMPLETE with
                    // misc_value == quest_id. Self-cast (target_entry=0).
                    if (te.effect_type == SPELL_EFFECT_QUEST_COMPLETE)
                    {
                        if (uint32(te.misc_value) == snap->quest_log.current_quest_id)
                        {
                            snap->quest_log.current_objective_tool.bag        = item->GetBagSlot();
                            snap->quest_log.current_objective_tool.slot       = item->GetSlot();
                            snap->quest_log.current_objective_tool.item_entry = tmpl->GetId();
                            snap->quest_log.current_objective_tool.spell_id   = te.spell_id;
                            snap->quest_log.current_objective_tool.target_entry = 0;
                            snap->quest_log.current_objective_tool.valid      = true;
                            return true;
                        }
                        continue;
                    }
                    // Create-item tool: SPELL_EFFECT_CREATE_ITEM / _CREATE_LOOT /
                    // _CREATE_RANDOM_ITEM. The ON_USE spell spawns an item;
                    // when that item matches the active ITEM objective's
                    // ObjectID, using the source item is the credit channel.
                    // Self-cast — the cast checks player position (the pool
                    // area trigger) server-side; the bot just needs to BE
                    // at the POI and emit use_item_by_entry.
                    if (te.effect_type == SPELL_EFFECT_CREATE_ITEM      ||
                        te.effect_type == SPELL_EFFECT_CREATE_LOOT      ||
                        te.effect_type == SPELL_EFFECT_CREATE_RANDOM_ITEM)
                    {
                        if (obj.type == QUEST_OBJECTIVE_ITEM &&
                            uint32(te.misc_value) == uint32(obj.object_id))
                        {
                            snap->quest_log.current_objective_tool.bag        = item->GetBagSlot();
                            snap->quest_log.current_objective_tool.slot       = item->GetSlot();
                            snap->quest_log.current_objective_tool.item_entry = tmpl->GetId();
                            snap->quest_log.current_objective_tool.spell_id   = te.spell_id;
                            snap->quest_log.current_objective_tool.target_entry = 0;
                            snap->quest_log.current_objective_tool.valid      = true;
                            return true;
                        }
                        continue;
                    }
                    // Per-target credit: KILL_CREDIT / KILL_CREDIT2 /
                    // KILL_CREDIT_LABEL_1 / KILL_CREDIT_LABEL_2. Cache
                    // already pre-filtered to relevant effect types.
                    // Match by ObjectID for MONSTER/TALKTO/ITEM, by label
                    // for KILL_WITH_LABEL.
                    if (uint32(te.misc_value) != uint32(obj.object_id)) continue;
                    snap->quest_log.current_objective_tool.bag        = item->GetBagSlot();
                    snap->quest_log.current_objective_tool.slot       = item->GetSlot();
                    snap->quest_log.current_objective_tool.item_entry = tmpl->GetId();
                    snap->quest_log.current_objective_tool.spell_id   = te.spell_id;
                    snap->quest_log.current_objective_tool.target_entry = uint32(te.misc_value);
                    snap->quest_log.current_objective_tool.valid      = true;
                    return true;
                }
                return false;
            };
            // Walk Backpack and equipped bags. Item::Iter is via GetBagByPos
            // / GetItemByPos; cheaper to walk the bot's PlayerBag arrays
            // directly. Player::GetItemByPos with NULL_BAG / NULL_SLOT
            // doesn't enumerate; we use the standard bag-iteration pattern.
            bool found = false;
            // INVENTORY_SLOT_ITEM_START..END (backpack body slots 23..38)
            for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            {
                if (Item* item = const_cast<Player*>(p)->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                    if ((found = try_match_item(item))) break;
            }
            if (!found)
            for (uint8 b = INVENTORY_SLOT_BAG_START; b < INVENTORY_SLOT_BAG_END && !found; ++b)
            {
                Bag* bag = const_cast<Player*>(p)->GetBagByPos(b);
                if (!bag) continue;
                for (uint32 sslot = 0; sslot < bag->GetBagSize() && !found; ++sslot)
                {
                    if (Item* item = const_cast<Player*>(p)->GetItemByPos(b, uint8(sslot)))
                        if ((found = try_match_item(item))) break;
                }
            }
        }
    }

    // ---- Open-gossip capture ----
    // After the bot opens a gossip dialog with InteractWithNpcIntent, the
    // server populates PlayerTalkClass with the menu. Snapshot it so the
    // 2-phase idle:quest_talk rule can pick a quest-credit option without
    // round-tripping the world thread. Empty (gossip_npc.IsEmpty()) when
    // no menu is currently open.
    if (PlayerMenu* menu = p->PlayerTalkClass.get())
    {
        InteractionData const& id = const_cast<PlayerMenu*>(menu)->GetInteractionData();
        if (!id.SourceGuid.IsEmpty())
        {
            // Treat any non-empty interaction source as the "talked-to NPC"
            // — the server resets this on SendCloseGossip. Even Trainer /
            // Vendor / Auction interactions go through SourceGuid; the
            // talk rule below filters on the *current objective's* target
            // entry so non-quest interactions are ignored.
            snap->gossip.gossip_npc = id.SourceGuid;
            GossipMenu const& gm = const_cast<PlayerMenu*>(menu)->GetGossipMenu();
            auto const& items = gm.GetMenuItems();
            snap->gossip.gossip_options.reserve(items.size());
            for (auto const& mi : items)
            {
                BotSnapshot::GossipMenuOption opt{};
                opt.order_index      = static_cast<uint8>(mi.OrderIndex & 0xFF);
                opt.option_npc       = static_cast<uint8>(mi.OptionNpc);
                opt.gossip_option_id = mi.GossipOptionID;
                snap->gossip.gossip_options.push_back(opt);
            }
        }
    }

    // ---- Quest turn-ins (resolve nearby givers for complete quests) ----
    // Walks nearby_friends + nearby_objects on the world thread (where
    // we have safe access to the live Creature / GameObject ptrs) and
    // tags each (giver, quest_id) pair where the giver accepts the quest's
    // turn-in. Lets AI in worker threads emit complete_quest with the
    // pre-resolved guid rather than guessing per-NPC.
    {
        std::vector<uint32> complete_q;
        complete_q.reserve(snap->quest_log.quests.size());
        for (auto const& q : snap->quest_log.quests)
            if (q.state == 1) complete_q.push_back(q.quest_id);
        if (!complete_q.empty())
        {
            auto check_unit = [&](ObjectGuid guid)
            {
                if (Creature* c = ObjectAccessor::GetCreature(*p, guid))
                {
                    for (uint32 qid : complete_q)
                        if (c->hasInvolvedQuest(qid))
                        {
                            BotSnapshot::QuestTurnIn t{};
                            t.giver = guid;
                            t.quest_id = qid;
                            snap->quest_discovery.quest_turnins.push_back(t);
                        }
                }
            };
            for (auto const& u : snap->combat.nearby_friends) check_unit(u.guid);
            for (auto const& o : snap->world_objects.nearby_objects)
            {
                if (o.go_type != GAMEOBJECT_TYPE_QUESTGIVER) continue;
                if (GameObject* go = ObjectAccessor::GetGameObject(*p, o.guid))
                {
                    for (uint32 qid : complete_q)
                        if (go->hasInvolvedQuest(qid))
                        {
                            BotSnapshot::QuestTurnIn t{};
                            t.giver = o.guid;
                            t.quest_id = qid;
                            snap->quest_discovery.quest_turnins.push_back(t);
                        }
                }
            }
        }

        // Quest offers: walk nearby creatures + GOs and surface any quest
        // they offer that the bot doesn't have. CanTakeQuest filters by
        // level / class / race / faction / prereq + quest log capacity.
        // We cap surfaced offers per snapshot (the AI only acts on the
        // first per tick; surfacing more is a small DB read but no
        // executor cost).
        constexpr size_t MAX_OFFERS = 16;
        // Resolve BotAI for the per-bot quest blacklist read. Same fall-back
        // pattern as the current_objective scope: if Services aren't up yet
        // (early init), the blacklist is simply empty for this bot.
        BotAI* offer_bot_ai = bot_ai;
        const uint32 offer_now_ms = GameTime::GetGameTimeMS();

        // Detect quest-log mutations and invalidate the per-bot
        // CanTakeQuest soft-fail cache when anything changed (accept /
        // complete-and-reward / abandon). Quest 28725 → 28726 case:
        // the bot turns in 28725, the next snapshot drops it from
        // m_QuestStatus, but Tarindrella's 28726 offer was previously
        // refused with why=prev_quest and cached for 30 s. Without
        // this flush the bot wanders off before the cache expires.
        // Signature is a cheap XOR-with-state mix; the per-quest bits
        // shift on accept (added) / complete (state flips) / reward
        // (removed). Collisions just delay a flush by one tick.
        if (offer_bot_ai)
        {
            uint64 sig = 0;
            for (auto const& q : snap->quest_log.quests)
            {
                uint64 m = uint64(q.quest_id) | (uint64(q.state) << 24);
                // Mix with a cheap rotation so order doesn't collapse
                // matching ids in different slots to the same hash.
                m = (m * 0x100000001b3ULL) ^ uint64(q.quest_id);
                sig ^= m;
            }
            if (sig != offer_bot_ai->last_quest_log_signature())
            {
                offer_bot_ai->clear_cant_take_quest_cache();
                offer_bot_ai->set_last_quest_log_signature(sig);
            }
        }
        auto offers_from = [&](ObjectGuid guid, QuestRelationResult const& rel, uint32 giver_entry)
        {
            // Per-NPC suppression: if every quest this NPC offers was
            // refused last pass, skip the whole iteration. The
            // CanTakeQuest call itself isn't free (it walks the
            // condition system + sat-check chain); skipping multiplies
            // the per-quest 30s cache savings by the average quest
            // count per NPC (~5-10 in starter zones).
            if (giver_entry != 0 && offer_bot_ai &&
                offer_bot_ai->giver_no_offers_recent(giver_entry, offer_now_ms))
                return;
            // P1 (2026-06-16): quest-log capacity gate. CanTakeQuest() below does
            // NOT check SatisfyQuestLog — only CanAddQuest (the executor's accept
            // predicate) does — so a FULL-log bot otherwise re-surfaces every
            // offer each snapshot and idle:quest_accept loops accept_quest ->
            // CanAddQuest -> reason=log_full forever with no back-off (the
            // cant_take_quest cache write lives in the skipped !CanTakeQuest
            // branch). Mirror the executor's predicate here so the surface
            // decision agrees with the accept decision; a full-log bot surfaces
            // zero offers and the abandon/turn-in rules free a slot instead.
            // SatisfyQuestLog is const; msg=false suppresses any client packet.
            if (!p->SatisfyQuestLog(false)) return;
            const size_t offers_before = snap->quest_discovery.quest_offers.size();
            for (uint32 qid : rel)
            {
                if (snap->quest_discovery.quest_offers.size() >= MAX_OFFERS) return;
                // Cheapest first: skip quests already in the log and the
                // per-bot blacklists.
                if (p->GetQuestStatus(qid) != QUEST_STATUS_NONE) continue;
                if (offer_bot_ai && offer_bot_ai->quest_blacklisted(qid, offer_now_ms))
                    continue;
                // Permanent class/race-impossible skip — these gates never flip
                // for this bot, so don't re-run the expensive CanTakeQuest or
                // re-log the refusal. Survives quest-log mutation flushes.
                if (offer_bot_ai && offer_bot_ai->quest_impossible(qid))
                    continue;
                // Recent-fail cache: when CanTakeQuest fails, we cache the
                // quest_id for 30s and skip re-evaluation. Acceptance gates
                // (quest log full, daily cooldown, exclusive group, skill
                // prereq, etc) rarely flip mid-tick, so this is purely a
                // spam-suppression for the condition system.
                if (offer_bot_ai && offer_bot_ai->cant_take_quest_recent(qid, offer_now_ms))
                    continue;
                Quest const* q = sObjectMgr->GetQuestTemplate(qid);
                if (!q) continue;
                // Cheap template-only silent pre-skips (mirror the shared
                // predicate exactly, so no drift). Kept here so these never reach
                // the CanTakeQuest diagnostic ladder below and mislog as why=?:
                //  - pure-repeatable (after turn-in GetQuestStatus is NONE again,
                //    so without this the bot re-accepts every snapshot — quest
                //    8228 looped accept→turn-in→accept); dailies are NOT excluded
                //    (their server cooldown gate handles re-acceptance).
                //  - structurally unreachable objectives (pet-battle credits,
                //    phase-locked spawns, removed legacy creatures).
                if (q->IsRepeatable()) continue;
                if (QuestHasObjectiveBotCannotComplete(qid)) continue;
                // Bot-side profession gate — diagnostic split-out. The actual
                // skip decision is made by the shared BotCanStillTakeQuestForHub
                // predicate below (which applies the SAME GetSkillValue==0 gate),
                // but we probe it here FIRST so the refusal logs why=need_profession
                // instead of the opaque CanTakeQuest catch-all. TC's SatisfyQuestSkill
                // only rejects when the skill VALUE is below RequiredSkillPoints, and
                // profession quests routinely carry RequiredSkillPoints=0 (e.g. the
                // Alchemy specialization quests 29067/29481/29482), so a character
                // that lacks the profession entirely still passes CanTakeQuest — see
                // the live "Somi" stranded-with-Alchemy-quests case. Re-evaluated each
                // tick (30s cant-take cache) since the bot may learn the skill later.
                if (uint32 reqSkill = q->GetRequiredSkill())
                {
                    if (const_cast<Player*>(p)->GetSkillValue(reqSkill) == 0)
                    {
                        if (offer_bot_ai && !offer_bot_ai->cant_take_quest_recent(qid, offer_now_ms))
                            TC_LOG_INFO("playerbot.v2",
                                "[quest_offer_refused] bot={} quest={} lvl={} why=need_profession skill={}",
                                p->GetName(), qid, uint32(p->GetLevel()), reqSkill);
                        if (offer_bot_ai)
                            offer_bot_ai->note_cant_take_quest(qid, offer_now_ms);
                        continue;
                    }
                }
                // Full acceptance check — routed through the SHARED doable
                // predicate so the offers surface decision and the hub-relocation
                // "is this hub exhausted" decision can never drift. It applies, in
                // order: not-already-rewarded, template exists, !IsRepeatable
                // (e.g. quest 8228 looped accept→turn-in→accept without this),
                // !QuestHasObjectiveBotCannotComplete (pet-battle / phase-locked /
                // removed-creature objectives), the GetSkillValue>0 profession gate
                // (re-checked, harmless), and CanTakeQuest (log capacity, daily/
                // weekly cooldown, exclusive group, skill prereqs, level, rep, etc).
                // Without the full check the bot would surface an offer, fire
                // accept_quest, the server would reject, and the offer would stay
                // visible — an infinite loop. On false, the diagnostics + 30s cache
                // below run exactly as before.
                if (!BotCanStillTakeQuestForHub(const_cast<Player*>(p), qid))
                {
                    // Permanent-impossibility check FIRST: if the refusal is a
                    // class or race gate (immutable for this bot), record it so
                    // every future snapshot skips this quest entirely (above) —
                    // no re-run of CanTakeQuest, no re-logged refusal. This is
                    // the ~50k why=class/race slice of the spam.
                    if (offer_bot_ai)
                    {
                        Player* cp = const_cast<Player*>(p);
                        if (!cp->SatisfyQuestClass(q, false) || !cp->SatisfyQuestRace(q, false))
                            offer_bot_ai->note_quest_impossible(qid);
                    }
                    // Diagnostic: probe each SatisfyQuest* gate so we can
                    // tell WHICH check failed. CanTakeQuest is opaque
                    // (returns one bool for a dozen reasons). Throttled
                    // via the per-bot recent-fail cache — emits once per
                    // (bot, quest) per 30 s, so log volume stays bounded.
                    // Gated on Logger.playerbot.v2 >= 3 (INFO) so the
                    // diagnostic only runs when someone is actively
                    // looking. Move to TC_LOG_DEBUG once L1 questing
                    // stabilises if this proves noisy at scale.
                    if (offer_bot_ai && !offer_bot_ai->cant_take_quest_recent(qid, offer_now_ms))
                    {
                        Player* mp = const_cast<Player*>(p);
                        char const* why = "?";
                        if      (!mp->SatisfyQuestStatus(q, false))               why = "status";
                        else if (!mp->SatisfyQuestLog(false))                     why = "log_full";
                        else if (!mp->SatisfyQuestClass(q, false))                why = "class";
                        else if (!mp->SatisfyQuestRace(q, false))                 why = "race";
                        else if (!mp->SatisfyQuestLevel(q, false))                why = "level";
                        else if (!mp->SatisfyQuestMinLevel(q, false))             why = "minlevel";
                        else if (!mp->SatisfyQuestMaxLevel(q, false))             why = "maxlevel";
                        else if (!mp->SatisfyQuestSkill(q, false))                why = "skill";
                        else if (!mp->SatisfyQuestPreviousQuest(q, false))        why = "prev_quest";
                        else if (!mp->SatisfyQuestDependentPreviousQuests(q, false)) why = "dep_prev";
                        else if (!mp->SatisfyQuestBreadcrumbQuest(q, false))      why = "breadcrumb";
                        else if (!mp->SatisfyQuestDependentBreadcrumbQuests(q, false)) why = "dep_breadcrumb";
                        else if (!mp->SatisfyQuestReputation(q, false))           why = "reputation";
                        else if (!mp->SatisfyQuestMinReputation(q, false))        why = "min_rep";
                        else if (!mp->SatisfyQuestMaxReputation(q, false))        why = "max_rep";
                        else if (!mp->SatisfyQuestConditions(q, false))           why = "conditions";
                        else if (!mp->SatisfyQuestTimed(q, false))                why = "timed";
                        else if (!mp->SatisfyQuestExclusiveGroup(q, false))       why = "exclusive";
                        else if (!mp->SatisfyQuestDay(q, false))                  why = "daily_cd";
                        else if (!mp->SatisfyQuestWeek(q, false))                 why = "weekly_cd";
                        else if (!mp->SatisfyQuestMonth(q, false))                why = "monthly_cd";
                        else if (!mp->SatisfyQuestSeasonal(q, false))             why = "seasonal_cd";
                        else if (!mp->SatisfyQuestExpansion(q, false))            why = "expansion";
                        else if (!mp->SatisfyQuestDependentQuests(q, false))      why = "dep_quests";
                        TC_LOG_INFO("playerbot.v2",
                            "[quest_offer_refused] bot={} quest={} lvl={} why={}",
                            p->GetName(), qid, uint32(p->GetLevel()), why);
                        // When the failure is dep_prev (DependentPreviousQuests
                        // chain unsatisfied), dump per-parent reward state so
                        // the next operator looking at this log can immediately
                        // see WHICH parent the bot is missing instead of having
                        // to guess. The DependentPreviousQuests vector is
                        // populated bi-directionally by ObjectMgr (any quest
                        // with NextQuestID==qid contributes a parent here).
                        if (std::string_view(why) == "dep_prev")
                        {
                            for (uint32 prevId : q->DependentPreviousQuests)
                            {
                                Quest const* pq = sObjectMgr->GetQuestTemplate(prevId);
                                int32 group = pq ? pq->GetExclusiveGroup() : 0;
                                bool rewarded = mp->IsQuestRewarded(prevId);
                                TC_LOG_INFO("playerbot.v2",
                                    "  dep_prev candidate quest={} group={} rewarded={}",
                                    prevId, group, rewarded ? 1 : 0);
                            }
                        }
                    }
                    if (offer_bot_ai)
                    {
                        // R8: a prerequisite-gated quest (prev_quest / dep_prev)
                        // can't become takeable until the bot completes the
                        // prereq — a quest-log change that flushes this cache —
                        // so cache it LONG to break the ~100x/session re-offer
                        // loop (8,268 prev_quest refusals from 516 distinct
                        // bot/quest pairs). Other refusals keep the 30s re-check
                        // (level/rep/cooldown can clear sooner).
                        Player* cq = const_cast<Player*>(p);
                        const bool prereq_miss =
                            !cq->SatisfyQuestPreviousQuest(q, false) ||
                            !cq->SatisfyQuestDependentPreviousQuests(q, false);
                        offer_bot_ai->note_cant_take_quest(qid, offer_now_ms,
                            prereq_miss ? BotAI::kCantTakeQuestPrereqMs
                                        : BotAI::kCantTakeQuestMs);
                    }
                    continue;
                }
                BotSnapshot::QuestTurnIn off{};
                off.giver = guid;
                off.quest_id = qid;
                // QUEST_FLAGS_AUTO_ACCEPT chain-heads (e.g. 25152) must be grabbed
                // instantly by idle:quest_auto_accept — a real client auto-grants
                // them on query; bots otherwise hesitate and wander off, leaving the
                // whole racial starter chain prev_quest-gated.
                off.auto_accept = q->IsAutoAccept();
                snap->quest_discovery.quest_offers.push_back(off);
            }
            // After walking every quest, if we added nothing, mark this
            // NPC as "no offers" for 60s so subsequent snapshot ticks
            // skip the whole iteration. NPCs only ever offer quests
            // (not turnins) and the rel list is constant per entry, so
            // this is safe across map / phase boundaries.
            if (giver_entry != 0 && offer_bot_ai &&
                snap->quest_discovery.quest_offers.size() == offers_before)
                offer_bot_ai->note_giver_no_offers(giver_entry, offer_now_ms);
        };
        for (auto const& u : snap->combat.nearby_friends)
        {
            if (snap->quest_discovery.quest_offers.size() >= MAX_OFFERS) break;
            if (Creature* c = ObjectAccessor::GetCreature(*p, u.guid))
            {
                QuestRelationResult rel = sObjectMgr->GetCreatureQuestRelations(c->GetEntry());
                offers_from(u.guid, rel, c->GetEntry());
            }
        }
        for (auto const& o : snap->world_objects.nearby_objects)
        {
            if (snap->quest_discovery.quest_offers.size() >= MAX_OFFERS) break;
            if (o.go_type != GAMEOBJECT_TYPE_QUESTGIVER) continue;
            if (GameObject* go = ObjectAccessor::GetGameObject(*p, o.guid))
            {
                QuestRelationResult rel = sObjectMgr->GetGOQuestRelations(go->GetEntry());
                offers_from(o.guid, rel, go->GetEntry());
            }
        }

        // Diagnostic: for low-level bots that ended up with EMPTY
        // quest_offers, log how many nearby quest-givers we saw vs
        // how many got filtered. Helps distinguish "no givers in
        // sight" (wider scan failed) from "all offers rejected"
        // (CanTakeQuest filtering everything). Throttled to one
        // log line per bot per 30s via the snapshot publisher's
        // cadence (~1Hz × 30 = roughly the right rate); we still
        // do the count walk every snapshot, but it's tiny.
        // Diagnostic: only fire when the bot is TRULY stuck — empty
        // quest_offers AND empty quest_log. A bot mid-quest has empty
        // offers (every quest in log is filtered out at line ~3185 by
        // GetQuestStatus != NONE), which is healthy state, not a bug.
        // Without the quests.empty() gate the diagnostic over-counts by
        // ~5–10x and drowns the real "I have nothing to do" cases.
        const bool truly_stuck =
            snap->quest_discovery.quest_offers.empty() &&
            snap->quest_log.quests.empty();
        if (truly_stuck && p->GetLevel() < 20)
        {
            uint32 giver_npc_count = 0;
            uint32 giver_go_count  = 0;
            constexpr uint32 kQuestGiverFlagMask =
                uint32(UNIT_NPC_FLAG_QUESTGIVER);
            for (auto const& u : snap->combat.nearby_friends)
                if (u.npc_flags & kQuestGiverFlagMask) ++giver_npc_count;
            for (auto const& o : snap->world_objects.nearby_objects)
                if (o.go_type == GAMEOBJECT_TYPE_QUESTGIVER) ++giver_go_count;
            // Throttle to once per 5s per bot via the AI's last-scan
            // timestamp so we don't spam every snapshot publish.
            if (offer_bot_ai)
            {
                const uint32 last_diag = offer_bot_ai->last_quest_diag_ms();
                if (last_diag == 0 || (offer_now_ms - last_diag) >= 5000u)
                {
                    offer_bot_ai->set_last_quest_diag_ms(offer_now_ms);
                    TC_LOG_INFO("playerbot.v2",
                        "[quest_truly_stuck] bot={} lvl={} givers_npc={} givers_go={} "
                        "friends_total={} objs_total={} map={} zone={}",
                        p->GetName(), uint32(p->GetLevel()),
                        giver_npc_count, giver_go_count,
                        uint32(snap->combat.nearby_friends.size()),
                        uint32(snap->world_objects.nearby_objects.size()),
                        uint32(p->GetMapId()), uint32(p->GetZoneId()));
                }
            }
        }
    }

    // ---- World-quest discovery index ----
    // Modern WoW (Legion+) tags repeatable end-game quests with
    // QUEST_FLAGS_EX_IS_WORLD_QUEST. The givers — usually environmental
    // creatures or interactable objects scattered around outdoor zones —
    // are otherwise indistinguishable from regular quest givers in the
    // snapshot's quest_offers/quest_turnins, so we scan the same
    // nearby_friends + nearby_objects sets and build a separate index
    // tagged with rewards + position so the new idle:wq_accept /
    // idle:wq_turnin rules and the `wq` whisper can iterate without a
    // re-scan. Empty when (a) no nearby giver carries a world quest, or
    // (b) the relevant DB rows simply aren't present (pre-Legion DB
    // dumps); the rules then no-op gracefully.
    {
        constexpr size_t MAX_WQ = 32;
        const uint8 wq_offer  = 0;
        const uint8 wq_turnin = 1;
        // Pre-build a cheap "is this quest in my log and complete?" set
        // and "is it in my log at all?" set so the inner branches are O(1).
        std::unordered_map<uint32, uint8> log_state;  // qid -> 1=complete, 0=incomplete
        log_state.reserve(snap->quest_log.quests.size());
        for (auto const& q : snap->quest_log.quests)
            log_state.emplace(q.quest_id, q.state);

        auto resolve_reward_currency = [](Quest const* q) -> uint32
        {
            for (uint32 cid : q->RewardCurrencyId)
                if (cid != 0) return cid;
            return 0u;
        };

        auto try_world_quest = [&](ObjectGuid giver, float gx, float gy, float gz,
                                   uint32 area_id, uint32 qid)
        {
            if (snap->quest_discovery.available_world_quests.size() >= MAX_WQ) return;
            Quest const* q = sObjectMgr->GetQuestTemplate(qid);
            if (!q || !q->IsWorldQuest()) return;
            // Determine type: turnin if accepted+complete; offer if
            // not in log AND CanTakeQuest. Otherwise skip — we don't
            // surface accepted-but-incomplete world quests as actionable
            // discovery (the bot already has them in `quests`).
            uint8 type = 0xFF;
            auto it = log_state.find(qid);
            if (it != log_state.end())
            {
                if (it->second == 1) type = wq_turnin;
                else                 return; // accepted but not complete
            }
            else
            {
                // Not in log — same gate as quest_offers above.
                if (QuestHasObjectiveBotCannotComplete(qid)) return;
                if (!const_cast<Player*>(p)->CanTakeQuest(q, false)) return;
                type = wq_offer;
            }
            BotSnapshot::WorldQuestEntry e{};
            e.giver              = giver;
            e.quest_id           = qid;
            e.type               = type;
            e.x                  = gx;
            e.y                  = gy;
            e.z                  = gz;
            e.area_id            = area_id;
            e.reward_currency_id = resolve_reward_currency(q);
            e.reward_money       = q->GetMaxMoneyReward();
            snap->quest_discovery.available_world_quests.push_back(e);
        };

        // Pass A: creature givers — both relations (offer) and involved
        // (turn-in) relations need scanning.
        for (auto const& u : snap->combat.nearby_friends)
        {
            if (snap->quest_discovery.available_world_quests.size() >= MAX_WQ) break;
            Creature* c = ObjectAccessor::GetCreature(*p, u.guid);
            if (!c) continue;
            const uint32 entry = c->GetEntry();
            const uint32 area_id = c->GetAreaId();
            for (uint32 qid : sObjectMgr->GetCreatureQuestRelations(entry))
                try_world_quest(u.guid, u.x, u.y, u.z, area_id, qid);
            for (uint32 qid : sObjectMgr->GetCreatureQuestInvolvedRelations(entry))
                try_world_quest(u.guid, u.x, u.y, u.z, area_id, qid);
        }
        // Pass B: GO givers.
        for (auto const& o : snap->world_objects.nearby_objects)
        {
            if (snap->quest_discovery.available_world_quests.size() >= MAX_WQ) break;
            if (o.go_type != GAMEOBJECT_TYPE_QUESTGIVER) continue;
            GameObject* go = ObjectAccessor::GetGameObject(*p, o.guid);
            if (!go) continue;
            const uint32 entry = go->GetEntry();
            const uint32 area_id = go->GetAreaId();
            for (uint32 qid : sObjectMgr->GetGOQuestRelations(entry))
                try_world_quest(o.guid, o.x, o.y, o.z, area_id, qid);
            for (uint32 qid : sObjectMgr->GetGOQuestInvolvedRelations(entry))
                try_world_quest(o.guid, o.x, o.y, o.z, area_id, qid);
        }
    }

    // ---- Active loot rolls ----
    // Surface in-flight group/master rolls so AI can vote without being
    // event-driven. LootRoll exposes loot_obj + list_id + vote mask via
    // the Playerbot accessors we added on the core side. is_upgrade
    // pre-computes whether the roll item is a strict ilvl upgrade for any
    // equipped slot of matching inventory type (or fills an empty slot).
    {
        for (LootRoll* roll : p->GetLootRolls())
        {
            if (!roll || !roll->IsStarted()) continue;
            LootItem const* li = roll->GetItem();
            Loot const*     lo = roll->GetLoot();
            if (!li || !lo) continue;
            BotSnapshot::LootRollEntry e{};
            e.loot_object  = lo->GetGUID();
            e.loot_list_id = static_cast<uint8>(li->LootListId);
            e.item_entry   = li->itemid;
            e.vote_mask    = static_cast<uint8>(roll->GetVoteMask());
            e.is_upgrade   = false;
            e.is_quest_item = false;
            // Quest-item match: walk this bot's active quest objectives and
            // flag the roll if any incomplete QUEST_OBJECTIVE_ITEM has the
            // rolled item as its target. Drives Need-priority in the auto-
            // roll rule so group questing closes naturally.
            for (auto const& q : snap->quest_log.quests)
            {
                if (q.state == 1) continue;          // already complete (turn-in)
                for (auto const& o : q.objectives)
                {
                    if (o.type != /*QUEST_OBJECTIVE_ITEM*/ 1) continue;
                    if (uint32(o.object_id) != li->itemid) continue;
                    if (o.progress >= o.amount) continue;
                    e.is_quest_item = true;
                    break;
                }
                if (e.is_quest_item) break;
            }
            // Need-on-upgrade check: bot must be allowed to Need-roll
            // (CanRollNeedForItem covers class/usability gates) AND the
            // item must be a stat-fit upgrade for this bot's spec. We
            // use a 5% margin against equipped score (matches the equip
            // rule margin) so the bot doesn't NEED-roll on a barely-better
            // item when human group-mates would be miffed.
            if (ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(li->itemid))
            {
                if (const_cast<Player*>(p)->CanRollNeedForItem(tmpl, p->GetMap(), false) == EQUIP_ERR_OK)
                {
                    const uint32 inv_type = tmpl->GetInventoryType();
                    const uint32 base_ilvl = tmpl->GetBaseItemLevel();
                    // Build a synthetic ItemStatBlock from the template.
                    // We can't resolve random suffix / bonus list without
                    // a live Item; for ranking purposes the base stat
                    // template is good enough — sidegrade detection is
                    // approximate. Spec weights are already on snapshot.
                    ItemStatBlock approx{};
                    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                    {
                        const int32 stat_type = tmpl->GetStatModifierBonusStat(i);
                        if (stat_type < 0) continue;
                        // Without a live Item we can't get the resolved
                        // value; use the template's StatPercentEditor as
                        // a coarse proxy. For ranking it's monotonic with
                        // the actual value (higher pct → higher value at
                        // any ilvl), which is all we need.
                        const int32 pct = tmpl->GetStatPercentEditor(i);
                        StatIndex si = StatIndexForItemMod(uint32(stat_type));
                        if (si == StatIndex::Count) continue;
                        approx.stats[size_t(si)] = static_cast<int16>(pct);
                    }
                    if (tmpl->GetClass() == ITEM_CLASS_WEAPON)
                    {
                        const float dps = tmpl->GetDPS(base_ilvl);
                        if (dps > 0.f)
                            approx.weapon_dps_x10 =
                                static_cast<uint16>(std::clamp<float>(dps * 10.f, 0.f, 65535.f));
                    }
                    auto score_block = [&](ItemStatBlock const& blk, uint16 ilvl) {
                        float v = static_cast<float>(ilvl);
                        for (size_t i = 0; i < snap->stat_weights.spec_stat_weights.size(); ++i)
                            v += static_cast<float>(blk.stats[i]) * snap->stat_weights.spec_stat_weights[i];
                        v += static_cast<float>(blk.weapon_dps_x10) *
                             snap->stat_weights.spec_weapon_dps_weight;
                        return v;
                    };
                    const float roll_score = score_block(approx, static_cast<uint16>(base_ilvl));

                    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                    {
                        // L-P0b: only evaluate slots this item can ACTUALLY be
                        // equipped in. Without this the empty-slot branch below
                        // NEED-rolled any positive-score item whenever the bot
                        // had ANY empty slot (e.g. NEED a 2H weapon because a
                        // ring slot was empty) — ninja-rolling over real
                        // players. This single check folds the slot-type match
                        // for both the empty and the occupied branches.
                        if (!InvTypeMatchesSlot(inv_type, slot)) continue;
                        EquippedItem const& eq = snap->inventory.equipped[slot];
                        if (eq.entry == 0)
                        {
                            // Empty matching-invtype slot — an upgrade if
                            // the item's score is positive (carries some
                            // stat the spec weights > 0). Fully naked
                            // bots otherwise NEED-rolled cosmetic shirts.
                            if (roll_score > 50.f)
                            {
                                e.is_upgrade = true;
                                break;
                            }
                        }
                        else
                        {
                            ItemTemplate const* eq_tmpl = sObjectMgr->GetItemTemplate(eq.entry);
                            if (!eq_tmpl) continue;
                            const float eq_score = score_block(eq.stats, eq.item_level);
                            const float margin   = std::max(50.f, eq_score * 0.05f);
                            if (roll_score > eq_score + margin)
                            {
                                e.is_upgrade = true;
                                break;
                            }
                        }
                    }
                }
            }
            snap->loot.loot_rolls.push_back(e);
        }
    }

    // ---- Mail ----
    // Snapshot the bot's mail vector. Player::GetMails() includes both
    // delivered and pending-deliver mails; we publish all and let the AI
    // gate on `deliver_in_sec <= 0`. Skipping deleted entries because the
    // server flushes them on _SaveMail; surfacing them would cause
    // mail_delete intents to retry against rows that are already gone.
    {
        const time_t now = GameTime::GetGameTime();
        snap->mailbox.unread_mail_count = p->unReadMails;
        auto const& mails = p->GetMails();
        snap->mailbox.mail.reserve(mails.size());
        for (Mail const* m : mails)
        {
            if (!m || m->state == MAIL_STATE_DELETED) continue;
            MailEntry e{};
            e.message_id     = m->messageID;
            e.money          = m->money;
            e.cod            = m->COD;
            e.deliver_in_sec = static_cast<int64>(m->deliver_time) - static_cast<int64>(now);
            e.expire_in_sec  = static_cast<int64>(m->expire_time)  - static_cast<int64>(now);
            e.message_type   = m->messageType;
            e.has_body       = (m->checked & MAIL_CHECK_MASK_HAS_BODY) != 0;
            e.is_returned    = (m->checked & MAIL_CHECK_MASK_RETURNED) != 0;
            e.item_count     = static_cast<uint16>(m->items.size());
            e.item_guid_lows.reserve(m->items.size());
            for (auto const& info : m->items)
                e.item_guid_lows.push_back(info.item_guid);
            // Mail::sender is a player LowGUID for Normal mails and an
            // entry id for AH / GM / Calendar / BlackMarket / Auction
            // system mails. The mail-drain whisper-reply rule keys off
            // (message_type == Normal) before using this as a GUID.
            e.sender_low     = uint64(m->sender);
            snap->mailbox.mail.push_back(std::move(e));
        }
    }

    // ---- Nearby GameObjects (30yd, capped, filtered by useful types) ----
    {
        constexpr float SCAN_RADIUS = 30.0f;
        constexpr size_t MAX_OBJECTS = 16;
        thread_local std::vector<GameObject*> scratch_gos;
        scratch_gos.clear();
        scratch_gos.reserve(64);
        Trinity::AllGameObjectsWithEntryInRange go_check(p, /*entry*/ 0, SCAN_RADIUS);
        Trinity::GameObjectListSearcher<Trinity::AllGameObjectsWithEntryInRange> searcher(p, scratch_gos, go_check);
        Cell::VisitGridObjects(p, searcher, SCAN_RADIUS);

        // thread_local scratch reused across builds — mirrors the
        // enemy/friend by_dist pattern. clear()+reserve() retains capacity
        // so the per-build heap alloc is paid once per worker thread.
        thread_local std::vector<std::pair<float, GameObject*>> by_dist;
        by_dist.clear();
        by_dist.reserve(scratch_gos.size());
        for (GameObject* go : scratch_gos)
        {
            if (!go) continue;
            // Filter to types AI actually drives. Skip everything else to
            // keep the vector bounded — chairs, transports, etc would
            // dominate the cap in major cities. GATHERING_NODE (50) is the
            // modern herbalism/mining node type — without it in the
            // snapshot, the `idle:gather` rule's `obj.go_type != 50`
            // continue-skip would never find a candidate and the rule
            // would never fire.
            const uint8 t = go->GetGoType();
            const bool useful =
                t == GAMEOBJECT_TYPE_MAILBOX        ||
                t == GAMEOBJECT_TYPE_CHEST          ||
                t == GAMEOBJECT_TYPE_GATHERING_NODE ||
                t == GAMEOBJECT_TYPE_FISHINGHOLE    ||
                t == GAMEOBJECT_TYPE_QUESTGIVER     ||
                t == GAMEOBJECT_TYPE_BINDER         ||
                t == GAMEOBJECT_TYPE_GOOBER         ||
                t == GAMEOBJECT_TYPE_MEETINGSTONE   ||
                t == GAMEOBJECT_TYPE_GUILD_BANK     ||
                // Quest-interaction GO types — needed by idle:quest_use_go
                // (B4) so QUEST_OBJECTIVE_GAMEOBJECT (type 2) targets are
                // visible. GENERIC covers most static quest props (banners,
                // crates, books). BUTTON covers levers / switches. SPELL_FOCUS
                // covers altars / runes that gate quest casts.
                t == GAMEOBJECT_TYPE_GENERIC        ||
                t == GAMEOBJECT_TYPE_BUTTON         ||
                t == GAMEOBJECT_TYPE_SPELL_FOCUS    ||
                // Cross-continent travel (Phase C). SPELLCASTER (22) is
                // the static portal GO; TRANSPORT (11) is mooring rooms /
                // elevators / etc; MO_TRANSPORT (15) is the seafaring
                // ships and zeppelins that move between continents. The
                // builder pre-resolves their destinations so the AI can
                // walk to them and use/board.
                t == GAMEOBJECT_TYPE_SPELLCASTER    ||
                t == GAMEOBJECT_TYPE_TRANSPORT      ||
                t == GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT ||
                // Battleground objectives. FLAGSTAND (24) covers AB/EotS/
                // BfG capturable nodes + AV banners + WSG/TP/BfG flag
                // pedestals + IoC/SoTA/SS objective markers. FLAGDROP (26)
                // covers dropped CTF flags. CAPTURE_POINT (42) is the
                // modern WG/AV-style capture object — auto_use_go_types
                // doesn't read it directly but BG node-state population
                // walks it from a separate path; including here lets
                // future scripts opt in via auto_use_go_types. Without
                // these, idle:bg_use_objective_go silently no-ops for
                // every BG that opts in, making node captures impossible.
                t == GAMEOBJECT_TYPE_FLAGSTAND      ||
                t == GAMEOBJECT_TYPE_FLAGDROP       ||
                t == GAMEOBJECT_TYPE_CAPTURE_POINT  ||
                // Modern BG mechanic GOs (BG audit N12/N25/N32/N67): the
                // Cata+ CTF flags are NEW_FLAG (36) on the pedestal and
                // NEW_FLAG_DROP (37) on the ground; EotS towers and the
                // Silvershard cart zones are CONTROL_ZONE (29). These were
                // missing from this filter, so idle:bg_use_objective_go
                // could never SEE the very GOs the WSG/TP/EotS/Kotmogu
                // scripts opt into via auto_use_go_types — flag and orb
                // pickup was structurally impossible fleet-wide.
                t == GAMEOBJECT_TYPE_NEW_FLAG       ||
                t == GAMEOBJECT_TYPE_NEW_FLAG_DROP  ||
                t == GAMEOBJECT_TYPE_CONTROL_ZONE   ||
                // DESTRUCTIBLE_BUILDING (33): IoC keep gates + SoTA defense-
                // line gates. The BG siege-vehicle fire rule scans these and
                // casts the vehicle's gate-damage spell at the standing ones
                // (is_destroyed below); without them in the snapshot a siege
                // engine could never break a gate and the General was never
                // reached (BG audit IoC / SoTA siege blockers).
                t == GAMEOBJECT_TYPE_DESTRUCTIBLE_BUILDING ||
                // TRAP (6): includes environmental hazards like bonfires
                // and pyres. Builder classifies as hazard below if the
                // trap's spell is hostile, so idle:flee_hazard can step
                // the bot out of damage range. We DO want non-hostile
                // traps in nearby_objects too (some quest scripts trigger
                // off them), so the type passes the filter unconditionally.
                t == GAMEOBJECT_TYPE_TRAP;
            if (!useful) continue;
            const float dx = go->GetPositionX() - p->GetPositionX();
            const float dy = go->GetPositionY() - p->GetPositionY();
            const float dz = go->GetPositionZ() - p->GetPositionZ();
            by_dist.emplace_back(dx*dx + dy*dy + dz*dz, go);
        }
        std::sort(by_dist.begin(), by_dist.end(),
                  [](auto const& a, auto const& b) { return a.first < b.first; });
        if (by_dist.size() > MAX_OBJECTS) by_dist.resize(MAX_OBJECTS);

        snap->world_objects.nearby_objects.reserve(by_dist.size());
        for (auto const& [_, go] : by_dist)
        {
            BotSnapshot::NearbyObject n{};
            n.guid    = go->GetGUID();
            n.entry   = go->GetEntry();
            n.go_type = go->GetGoType();
            // [bgflag_diag] TEMP — confirm CTF flag GO (type 36) is in-snapshot on
            // EotS(566)/TP(726), and which bot sees it (incl. distance via pos).
            if ((p->GetMapId() == 726 || p->GetMapId() == 566) && go->GetGoType() == 36)
            {
                static thread_local uint32 s_bfAt = 0;
                uint32 const bfn = getMSTime();
                if (bfn - s_bfAt > 5000)
                {
                    s_bfAt = bfn;
                    TC_LOG_INFO("playerbot.v2",
                        "[bgflag_diag] map={} flagGO entry={} at ({:.0f},{:.0f},{:.0f}) "
                        "seen_by={} bot_pos=({:.0f},{:.0f},{:.0f})",
                        p->GetMapId(), go->GetEntry(), go->GetPositionX(),
                        go->GetPositionY(), go->GetPositionZ(), p->GetName(),
                        p->GetPositionX(), p->GetPositionY(), p->GetPositionZ());
                }
            }
            n.x = go->GetPositionX();
            n.y = go->GetPositionY();
            n.z = go->GetPositionZ();
            // Destructible gate breach state (IoC / SoTA) so the siege-fire
            // rule skips already-down gates and advances to the next line.
            if (n.go_type == GAMEOBJECT_TYPE_DESTRUCTIBLE_BUILDING)
                n.is_destroyed =
                    go->GetDestructibleState() == GO_DESTRUCTIBLE_DESTROYED;
            // Pre-resolve teleport destination for SPELLCASTER GOs.
            // Type 22 stores its triggered spell in spellCaster.spell.
            // The spell's destination is in the spell_target_position
            // table (loaded once at server start, read-only thereafter
            // — safe to query from this snapshot path). Most type-22
            // GOs are altars / event triggers without a teleport effect;
            // for those, GetSpellTargetPosition returns null and the
            // teleport_dest_* fields stay zero.
            if (n.go_type == GAMEOBJECT_TYPE_SPELLCASTER)
            {
                if (GameObjectTemplate const* tmpl = go->GetGOInfo())
                {
                    const uint32 spell_id = tmpl->spellCaster.spell;
                    if (spell_id != 0)
                    {
                        // Resolve the teleport destination through the SHARED
                        // PortalIndex resolver, which scans all effect indices
                        // AND follows the FORCE_CAST/TRIGGER_SPELL indirection
                        // modern portals use. The GO's own spell often carries
                        // no spell_target_position: either the teleport sits on
                        // a non-zero effect index (~156 portals server-wide), or
                        // the spell merely FORCE_CASTs the real teleport (e.g.
                        // Org's Undercity portal 293684 -> 17611 -> 121862 ->
                        // map 0). Either way the old direct scan left
                        // teleport_dest_map=0, so the in-range use_portal gate
                        // (dest must match the goal map) skipped the portal and
                        // the bot walked to it but never used it. Using the same
                        // resolver as PortalIndex keeps the snapshot and the
                        // travel graph in lockstep.
                        uint32 dmap = 0;
                        float dtx = 0.f, dty = 0.f, dtz = 0.f;
                        if (Playerbot::V2::Travel::ResolvePortalSpellDest(spell_id, dmap, dtx, dty, dtz))
                        {
                            n.teleport_dest_map = dmap;
                            n.teleport_dest_x   = dtx;
                            n.teleport_dest_y   = dty;
                            n.teleport_dest_z   = dtz;
                        }
                    }
                }
            }
            // Hazard classification for TRAP-type GOs (bonfires, pyres,
            // lava grates). We mark a trap as a hazard when its trap
            // spell is hostile (SpellInfo::IsPositive() == false). The
            // hazard_radius copies trap.radius/2 to mirror TC's legacy
            // diameter quirk in GameObject.cpp:1466. idle:flee_hazard
            // uses both fields to step the bot 5y past the radius so it
            // stops standing inside a bonfire and dying.
            else if (n.go_type == GAMEOBJECT_TYPE_TRAP)
            {
                if (GameObjectTemplate const* tmpl = go->GetGOInfo())
                {
                    const uint32 trap_spell = tmpl->trap.spell;
                    if (trap_spell != 0)
                    {
                        if (SpellInfo const* si = sSpellMgr->GetSpellInfo(trap_spell, DIFFICULTY_NONE))
                        {
                            if (!si->IsPositive())
                            {
                                n.is_hazard     = true;
                                n.hazard_radius = tmpl->trap.radius / 2.0f;
                            }
                        }
                    }
                }
            }
            // Continental transport (ship / zeppelin). Same teleport_dest
            // semantics: AI walks to the transport, server auto-attaches
            // the bot as a passenger when the bounding box overlaps, the
            // transport carries them across maps, and the bot's same-map
            // rules pick up at the destination dock. We populate the
            // FIRST destination map that isn't the bot's current map —
            // most transports cycle between exactly two continents, so
            // there's no ambiguity. (Multi-stop tours like the Pandaria
            // cruise are rare and not worth special-casing.)
            // ONLY type-15 MAP_OBJ_TRANSPORT (cross-map ships/zeppelins) get a
            // cross-map destination. Type-11 GAMEOBJECT_TYPE_TRANSPORT here is a
            // local animated LIFT (Orgrimmar zeppelin-tower elevators, city
            // lifts) — same-map, no TransportTemplate. It is still pushed to
            // nearby_objects (the elevator rules find it), but with NO
            // teleport_dest_map (stays kInvalidMapId), so the cross-map board
            // rule never targets it. Old code accepted type-11 too; an elevator
            // with no template kept the default dest, and when that default was
            // 0 (== Eastern Kingdoms, a real map) the bot boarded the lift as if
            // it were the EK zeppelin and rode it up and down forever.
            else if (n.go_type == GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT)
            {
                if (TransportTemplate const* tt =
                        sTransportMgr->GetTransportTemplate(go->GetEntry()))
                {
                    for (uint32 map_id : tt->MapIds)
                    {
                        if (map_id != p->GetMapId())
                        {
                            n.teleport_dest_map = map_id;
                            // No exact dest coords — the transport drops
                            // the passenger at its dock spawn, which the
                            // AI doesn't strictly need to plan; we just
                            // need to know "this transport reaches the
                            // goal map".
                            break;
                        }
                    }
                }
            }
            // Type-11 local lift (Orgrimmar zeppelin-tower elevators, city /
            // raid lifts). We're on the WORLD THREAD here with the live navmesh
            // and the bot is physically near this elevator (it's in range of the
            // grid scan), so this is the right place to lazily derive the stop's
            // walkable board/disembark LEDGES (nearest navmesh poly beside each
            // platform-centre). Cheap after the first encounter — every stop's
            // result is cached in ElevatorIndex. The elevator rules then read the
            // ledge via ElevatorIndex::LedgeFor on the AI worker.
            else if (n.go_type == GAMEOBJECT_TYPE_TRANSPORT)
            {
                // Derive ledges for the WHOLE nearby lift cluster (not just this
                // GO), so the boardable-lift preference can compare sibling lifts
                // immediately (e.g. the two Org zeppelin lifts ~150y apart — one
                // boardable, one a pit). Cheap + cached after the first pass.
                V2::Travel::ElevatorIndex::Instance().EnsureLedgesNear(p, /*range*/ 200.0f);
            }
            snap->world_objects.nearby_objects.push_back(n);
        }

        // ---- MO-transport scan (ships / zeppelins) ----
        // Continental transports (GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT 15 = boats,
        // and the cross-map TRANSPORT 11 = zeppelins) live in Map::_transports,
        // NOT the grid, so the Cell::VisitGridObjects scan above NEVER returns
        // them. That left every cross-map boarding rule blind: 0 bots boarded a
        // ship fleet-wide despite 98K+ walk_to_known_dock fires — the dominant
        // cause of the leveling stall (bots can't reach off-continent quests).
        // Scan the map's transport set directly and add any nearby continental
        // transport (one with a destination map != current) so a bot waiting at
        // a dock can SEE its docked ship and board it. Generous radius — the bot
        // waits on the pier, tens of yards from the ship's moving pivot. Few
        // transports exist per map, so this is cheap; we bypass the 16-object
        // cap above because transports are rare and always travel-relevant.
        if (Map* tmap = p->GetMap())
        {
            constexpr float kTransportScanSq = 200.0f * 200.0f;
            for (Transport* tr : tmap->GetTransports())
            {
                if (!tr) continue;
                const uint8 tt = tr->GetGoType();
                if (tt != GAMEOBJECT_TYPE_TRANSPORT &&
                    tt != GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT) continue;
                const float tdx = tr->GetPositionX() - p->GetPositionX();
                const float tdy = tr->GetPositionY() - p->GetPositionY();
                const float tdz = tr->GetPositionZ() - p->GetPositionZ();
                if (tdx*tdx + tdy*tdy + tdz*tdz > kTransportScanSq) continue;
                BotSnapshot::NearbyObject n{};
                n.guid    = tr->GetGUID();
                n.entry   = tr->GetEntry();
                n.go_type = tt;
                n.x = tr->GetPositionX();
                n.y = tr->GetPositionY();
                n.z = tr->GetPositionZ();
                // Resolve the transport's destination continent (first MapId
                // that isn't the bot's current map) — mirrors the grid-GO
                // transport handling so the boarding rule's dest match works.
                if (TransportTemplate const* ttpl =
                        sTransportMgr->GetTransportTemplate(tr->GetEntry()))
                {
                    for (uint32 map_id : ttpl->MapIds)
                    {
                        if (map_id != p->GetMapId())
                        {
                            n.teleport_dest_map = map_id;
                            break;
                        }
                    }
                }
                snap->world_objects.nearby_objects.push_back(n);
            }
        }

        // ---- Extended gathering-node scan (200y) ----
        // The 30y nearby_objects scan above covers vendor / quest-giver
        // / chest interactions where the bot is typically at the giver.
        // For gathering nodes we want a much wider horizon so the bot can
        // SEE a node from across a clearing and walk toward it. Per the
        // user's design: 40y attract in Questing mode (opportunistic),
        // 150-200y attract in Professioning mode (active seeking).
        // Snapshot exposes nodes up to 200y; the rule picks attract radius
        // based on the bot's activity_mode.
        //
        // Cost: O(creatures+GOs within 200y) extra cell visit, gated to
        // bots that have at least one gathering skill — non-gatherers see
        // no overhead. The append is filtered to GATHERING_NODE / FISHINGHOLE
        // / CHEST(gather variant) so it doesn't pollute nearby_objects with
        // unrelated GOs.
        constexpr uint16 kHerbalismSk = 182;
        constexpr uint16 kMiningSk    = 186;
        constexpr uint16 kSkinningSk  = 393;
        constexpr uint16 kFishingSk   = 356;
        const bool has_any_gather =
            p->HasSkill(kHerbalismSk) || p->HasSkill(kMiningSk) ||
            p->HasSkill(kSkinningSk)  || p->HasSkill(kFishingSk);
        // Per-bot 1Hz throttle on the 200y gather-node Cell::VisitGridObjects.
        // Same rationale as the hub scan above: nodes are static; the
        // 30y nearby_objects scan still gives near-bot coverage every tick.
        BotAI* gs_ai = bot_ai;
        const uint32 gather_now_ms = GameTime::GetGameTimeMS();
        constexpr uint32 kGatherScanIntervalMs = 1000u;
        const bool gather_throttled = gs_ai &&
            gs_ai->last_gather_scan_ms() != 0 &&
            (gather_now_ms - gs_ai->last_gather_scan_ms()) < kGatherScanIntervalMs;
        if (has_any_gather && !gather_throttled)
        {
            if (gs_ai) gs_ai->set_last_gather_scan_ms(gather_now_ms);
            constexpr float EXT_GATHER_RADIUS = 200.0f;
            constexpr size_t EXT_GATHER_MAX = 32;
            // Same thread_local + reserve(64) pattern as the 30y scan
            // above. Distinct buffer (`ext_gos`) because both scans can
            // appear in the same Build pass; reusing the same buffer
            // would corrupt the 30y result.
            thread_local std::vector<GameObject*> scratch_ext_gos;
            scratch_ext_gos.clear();
            scratch_ext_gos.reserve(128);
            Trinity::AllGameObjectsWithEntryInRange ext_check(p, /*entry*/ 0, EXT_GATHER_RADIUS);
            Trinity::GameObjectListSearcher<Trinity::AllGameObjectsWithEntryInRange> ext_searcher(p, scratch_ext_gos, ext_check);
            Cell::VisitGridObjects(p, ext_searcher, EXT_GATHER_RADIUS);

            std::unordered_set<ObjectGuid> ext_seen;
            ext_seen.reserve(snap->world_objects.nearby_objects.size() + 16);
            for (auto const& nb : snap->world_objects.nearby_objects)
                ext_seen.insert(nb.guid);

            std::vector<std::pair<float, GameObject*>> ext_by_dist;
            ext_by_dist.reserve(scratch_ext_gos.size());
            for (GameObject* go : scratch_ext_gos)
            {
                if (!go) continue;
                const uint8 t = uint8(go->GetGoType());
                if (t != GAMEOBJECT_TYPE_GATHERING_NODE &&
                    t != GAMEOBJECT_TYPE_FISHINGHOLE) continue;
                if (ext_seen.count(go->GetGUID())) continue;
                const float dx = go->GetPositionX() - p->GetPositionX();
                const float dy = go->GetPositionY() - p->GetPositionY();
                const float dz = go->GetPositionZ() - p->GetPositionZ();
                ext_by_dist.emplace_back(dx*dx + dy*dy + dz*dz, go);
            }
            std::sort(ext_by_dist.begin(), ext_by_dist.end(),
                      [](auto const& a, auto const& b) { return a.first < b.first; });
            if (ext_by_dist.size() > EXT_GATHER_MAX) ext_by_dist.resize(EXT_GATHER_MAX);
            for (auto const& [_, go] : ext_by_dist)
            {
                BotSnapshot::NearbyObject n{};
                n.guid    = go->GetGUID();
                n.entry   = go->GetEntry();
                n.go_type = go->GetGoType();
                n.x = go->GetPositionX();
                n.y = go->GetPositionY();
                n.z = go->GetPositionZ();
                snap->world_objects.nearby_objects.push_back(n);
            }
        }

        // ---- Spell-based ground-hazard scan (G-P1c) ----
        // Modern boss "stand in fire" mechanics are NOT GameObject traps
        // (handled above). They are DynamicObject area-spell entities
        // (classic voidzones / Consecration-like ground effects) and
        // AreaTrigger entities (the modern shape-based ground effects used
        // by virtually every retail boss). Neither is reachable through the
        // GameObject scan, so without this pass the bot has no snapshot
        // signal that it is standing in a damage patch — idle:flee_hazard
        // and the in-combat dungeon-positioning path (G-P0a) both consume
        // is_hazard / hazard_radius NearbyObject entries and step the bot
        // out, so feeding these two entity classes into nearby_objects is
        // all that is required to make bots dodge fire.
        //
        // PERF: this is the hottest path (200-2000 bots, ~20K Builds/s
        // post-throttle). Hazards only matter while the bot is fighting, so
        // the ENTIRE scan is gated behind p->IsInCombat() — the ~95% idle
        // fleet pays nothing (not even the cell visit). When it does run we
        // restrict the grid map-type mask to DYNAMICOBJECT | AREATRIGGER so
        // VisitAllObjects skips creatures, players, and GameObjects
        // entirely, and we cap the appended hazards to the closest few.
        if (p->IsInCombat())
        {
            constexpr float HAZ_SCAN_RADIUS = 40.0f;
            constexpr size_t HAZ_MAX = 12;
            // Reuse the WorldObjectListSearcher + AllWorldObjectsInRange
            // pattern (same as the TC core scripts), but constrain the
            // visited grid containers to the two hazard-bearing types via
            // the searcher's mapTypeMask so the visit never touches the
            // (far larger) creature / player / GO maps.
            thread_local std::vector<WorldObject*> scratch_haz;
            scratch_haz.clear();
            scratch_haz.reserve(32);
            Trinity::AllWorldObjectsInRange haz_check(p, HAZ_SCAN_RADIUS);
            Trinity::WorldObjectListSearcher<Trinity::AllWorldObjectsInRange> haz_searcher(
                p, scratch_haz, haz_check,
                GRID_MAP_TYPE_MASK_DYNAMICOBJECT | GRID_MAP_TYPE_MASK_AREATRIGGER);
            Cell::VisitAllObjects(p, haz_searcher, HAZ_SCAN_RADIUS);

            // Each candidate carries (dist^2, position, radius). We resolve
            // the spell + caster, apply the hostile-and-non-positive gate,
            // then sort by distance and keep the closest HAZ_MAX so a
            // densely-stacked AoE encounter can't blow past the cap.
            struct HazCand { float d2; float x, y, z, radius; ObjectGuid guid; };
            std::vector<HazCand> haz_by_dist;
            haz_by_dist.reserve(scratch_haz.size());

            // Shared classifier: a ground effect is a hazard when its
            // creating spell is hostile (SpellInfo::IsPositive() == false)
            // AND its caster is NOT friendly to the bot. A friendly
            // Consecration / Healing Rain / Death and Decay cast by the bot
            // or a party member must never be flagged. When the caster is
            // absent (despawned / environmental), we treat a non-positive
            // spell as an environmental hazard (e.g. lingering fire). When
            // the spell is positive, we never flag — that is the safe
            // default for indeterminate hostility.
            auto isHostileGround = [&](SpellInfo const* si, Unit* caster) -> bool
            {
                if (!si || si->IsPositive())
                    return false;                       // positive => never a hazard
                if (!caster)
                    return true;                        // no caster => environmental hazard
                return !caster->IsFriendlyTo(p);        // hostile / neutral non-friendly only
            };

            for (WorldObject* wo : scratch_haz)
            {
                if (!wo) continue;

                if (DynamicObject const* dyn = wo->ToDynObject())
                {
                    SpellInfo const* si = dyn->GetSpellInfo();
                    if (!isHostileGround(si, dyn->GetCaster()))
                        continue;
                    const float dx = dyn->GetPositionX() - p->GetPositionX();
                    const float dy = dyn->GetPositionY() - p->GetPositionY();
                    const float dz = dyn->GetPositionZ() - p->GetPositionZ();
                    haz_by_dist.push_back({ dx*dx + dy*dy + dz*dz,
                        dyn->GetPositionX(), dyn->GetPositionY(), dyn->GetPositionZ(),
                        dyn->GetRadius(), dyn->GetGUID() });
                }
                else if (AreaTrigger const* at = wo->ToAreaTrigger())
                {
                    // AreaTrigger exposes only the spell id; resolve the
                    // SpellInfo through SpellMgr (DIFFICULTY_NONE matches the
                    // GO-trap classifier above). GetMaxSearchRadius() returns
                    // the largest extent of the trigger's current shape
                    // (sphere / box / polygon bounding radius incl. scale),
                    // which is the conservative radius we want the bot to
                    // clear.
                    SpellInfo const* si = sSpellMgr->GetSpellInfo(at->GetSpellId(), DIFFICULTY_NONE);
                    if (!isHostileGround(si, at->GetCaster()))
                        continue;
                    const float dx = at->GetPositionX() - p->GetPositionX();
                    const float dy = at->GetPositionY() - p->GetPositionY();
                    const float dz = at->GetPositionZ() - p->GetPositionZ();
                    haz_by_dist.push_back({ dx*dx + dy*dy + dz*dz,
                        at->GetPositionX(), at->GetPositionY(), at->GetPositionZ(),
                        at->GetMaxSearchRadius(), at->GetGUID() });
                }
            }

            std::sort(haz_by_dist.begin(), haz_by_dist.end(),
                      [](HazCand const& a, HazCand const& b) { return a.d2 < b.d2; });
            if (haz_by_dist.size() > HAZ_MAX) haz_by_dist.resize(HAZ_MAX);

            for (auto const& h : haz_by_dist)
            {
                BotSnapshot::NearbyObject n{};
                n.guid          = h.guid;
                // entry / go_type stay 0 — these are not GameObjects. The
                // consumer keys off is_hazard / hazard_radius, not go_type.
                n.x             = h.x;
                n.y             = h.y;
                n.z             = h.z;
                n.is_hazard     = true;
                n.hazard_radius = h.radius;
                snap->world_objects.nearby_objects.push_back(n);
            }
        }

        // ---- Quest-relevant GameObject expansion (POI-aware) ----
        // Mirrors the "quest-target neutral creature" expansion above: the
        // baseline 30y nearby_objects scan misses GameObjects that are
        // 50-150y from the bot. Many ITEM-objective quests source their
        // item from a *lootable GO* (Quest 29401/29412 "Blown Away" loots
        // item 71034 from balloons scattered around Razor Hill). The bot
        // arrives at the POI center but the balloons cluster 100+ yards
        // away across the POI polygon — outside 30y, so nearby_objects is
        // empty and idle:quest_use_go silently no-ops.
        //
        // Strategy: build the set of (a) entries directly named by an
        // active QUEST_OBJECTIVE_GAMEOBJECT, and (b) item-ids the active
        // QUEST_OBJECTIVE_ITEM rows need; then do a wider scan and admit
        // a GO if (a) matches its entry OR (b) intersects with its quest
        // item drop list. Two-pass radii (80y around bot + 160y around
        // POI when the bot is within 50y of POI) mirror the creature
        // expansion above.
        if (!snap->quest_log.quests.empty())
        {
            std::unordered_set<uint32> q_go_entries;
            std::unordered_set<uint32> q_item_ids;
            q_go_entries.reserve(8);
            q_item_ids.reserve(8);
            for (auto const& q : snap->quest_log.quests)
            {
                if (q.state != 0) continue;  // 0 = INCOMPLETE
                for (auto const& oe : q.objectives)
                {
                    if (oe.progress >= oe.amount) continue;
                    if (oe.type == QUEST_OBJECTIVE_GAMEOBJECT && oe.object_id > 0)
                        q_go_entries.insert(uint32(oe.object_id));
                    else if (oe.type == QUEST_OBJECTIVE_ITEM && oe.object_id > 0)
                        q_item_ids.insert(uint32(oe.object_id));
                }
            }

            if (!q_go_entries.empty() || !q_item_ids.empty())
            {
                std::vector<GameObject*> q_pool;

                // ---- Tier 1.3 (behavior-preserving): single-traversal reuse ----
                // The original ran TWO bot-centered VisitGridObjects passes: an
                // 80y pass (always) and a 160y pass (when sitting on a POI),
                // concatenating both into q_pool. The 160y pass re-walks every
                // cell the 80y pass already walked (same center, GameObjects,
                // AllGameObjectsWithEntryInRange uses IsWithinDist), so the
                // 80y grid traversal is pure duplicate work whenever the wide
                // pass fires.
                //
                // We reuse a SINGLE traversal: when the wide pass fires we walk
                // 160y ONCE and reconstruct q_pool with the IDENTICAL multiset
                // the two-pass code produced — every GO within 80y appears
                // TWICE (once for the would-be 80y pass, once for the 160y
                // pass) and every 80–160y GO appears once. Order within q_pool
                // does NOT affect output here because q_by_dist below is SORTED
                // by distance before the cap, so the membership after sort+cap
                // depends only on the (distance,go) MULTISET — which is
                // preserved exactly, including the duplicate-occupies-two-slots
                // cap interaction of the original. When the wide pass does NOT
                // fire we keep the plain single 80y traversal.
                bool wide_fires = false;
                {
                    auto const& poi = snap->quest_log.current_objective_poi;
                    if (poi.valid && poi.map_id == snap->position.map_id)
                    {
                        const float pdx = poi.x - p->GetPositionX();
                        const float pdy = poi.y - p->GetPositionY();
                        const float pdz = poi.z - p->GetPositionZ();
                        const float poi_dist_sq = pdx*pdx + pdy*pdy + pdz*pdz;
                        wide_fires = (poi_dist_sq < (50.0f * 50.0f));
                    }
                }

                if (!wide_fires)
                {
                    // Narrow-only: exactly the original 80y pass.
                    thread_local std::vector<GameObject*> scratch_q_gos;
                    scratch_q_gos.clear();
                    scratch_q_gos.reserve(128);
                    constexpr float kQGoRadiusBot = 80.0f;
                    Trinity::AllGameObjectsWithEntryInRange q_check(p, /*entry*/ 0, kQGoRadiusBot);
                    Trinity::GameObjectListSearcher<Trinity::AllGameObjectsWithEntryInRange> q_searcher(p, scratch_q_gos, q_check);
                    Cell::VisitGridObjects(p, q_searcher, kQGoRadiusBot);
                    q_pool.insert(q_pool.end(), scratch_q_gos.begin(), scratch_q_gos.end());
                }
                else
                {
                    // Wide fires: single 160y traversal, then rebuild the exact
                    // [80y-subset] ++ [full-160y] multiset the two passes made.
                    thread_local std::vector<GameObject*> scratch_q_gos_poi;
                    scratch_q_gos_poi.clear();
                    scratch_q_gos_poi.reserve(256);
                    constexpr float kQGoRadiusPOI = 160.0f;
                    constexpr float kQGoRadiusBot = 80.0f;
                    Trinity::AllGameObjectsWithEntryInRange q_check_poi(p, /*entry*/ 0, kQGoRadiusPOI);
                    Trinity::GameObjectListSearcher<Trinity::AllGameObjectsWithEntryInRange> q_searcher_poi(p, scratch_q_gos_poi, q_check_poi);
                    Cell::VisitGridObjects(p, q_searcher_poi, kQGoRadiusPOI);

                    // Segment 1 — reproduces the would-be 80y pass: every GO
                    // the 160y result holds that is also within 80y. We re-use
                    // the EXACT predicate AllGameObjectsWithEntryInRange applied
                    // (IsWithinDist with is3D=false / 2D), so this membership is
                    // identical to the dropped 80y pass.
                    q_pool.reserve(scratch_q_gos_poi.size() * 2);
                    for (GameObject* go : scratch_q_gos_poi)
                        if (go && p->IsWithinDist(go, kQGoRadiusBot, /*is3D*/ false))
                            q_pool.push_back(go);
                    // Segment 2 — the full 160y pass.
                    q_pool.insert(q_pool.end(), scratch_q_gos_poi.begin(), scratch_q_gos_poi.end());
                }

                std::unordered_set<ObjectGuid> already_in;
                already_in.reserve(snap->world_objects.nearby_objects.size() + 16);
                for (auto const& nb : snap->world_objects.nearby_objects)
                    already_in.insert(nb.guid);

                // Sort candidates by distance so we keep the closest ones
                // first under the per-bot append cap.
                std::vector<std::pair<float, GameObject*>> q_by_dist;
                q_by_dist.reserve(q_pool.size());
                for (GameObject* go : q_pool)
                {
                    if (!go) continue;
                    if (already_in.count(go->GetGUID())) continue;
                    const uint32 entry = go->GetEntry();
                    const bool entry_match = q_go_entries.count(entry) > 0;
                    bool item_match = false;
                    if (!entry_match && !q_item_ids.empty())
                    {
                        if (std::vector<uint32> const* drops =
                                sObjectMgr->GetGameObjectQuestItemList(entry))
                        {
                            for (uint32 item_id : *drops)
                            {
                                if (q_item_ids.count(item_id))
                                {
                                    item_match = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!entry_match && !item_match) continue;
                    const float dx = go->GetPositionX() - p->GetPositionX();
                    const float dy = go->GetPositionY() - p->GetPositionY();
                    const float dz = go->GetPositionZ() - p->GetPositionZ();
                    q_by_dist.emplace_back(dx*dx + dy*dy + dz*dz, go);
                }
                std::sort(q_by_dist.begin(), q_by_dist.end(),
                          [](auto const& a, auto const& b) { return a.first < b.first; });

                constexpr size_t kQAppendCap = 24;
                if (q_by_dist.size() > kQAppendCap) q_by_dist.resize(kQAppendCap);

                for (auto const& [_, go] : q_by_dist)
                {
                    if (already_in.count(go->GetGUID())) continue;
                    BotSnapshot::NearbyObject n{};
                    n.guid    = go->GetGUID();
                    n.entry   = go->GetEntry();
                    n.go_type = go->GetGoType();
                    n.x = go->GetPositionX();
                    n.y = go->GetPositionY();
                    n.z = go->GetPositionZ();
                    snap->world_objects.nearby_objects.push_back(n);
                    already_in.insert(go->GetGUID());
                }
            }
        }
    }

    // ---- Battleground queue state ----
    {
        snap->bg.in_battleground = p->InBattleground();
        for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            BattlegroundQueueTypeId qid = p->GetBattlegroundQueueTypeId(i);
            if (qid == BATTLEGROUND_QUEUE_NONE) continue;
            BotSnapshot::BgQueueEntry e{};
            e.bg_type_id    = qid.BattlemasterListId;
            e.joined_at_sec = p->GetBattlegroundQueueJoinTime(qid);
            // The invited-to-instance value is what HandleBattleFieldPort
            // uses to look up the actual Battleground; surfacing it lets the
            // auto-port rule skip slots that are still waiting in queue.
            e.invited_to_instance = p->IsInvitedForBattlegroundQueueType(qid)
                ? /*non-zero marker*/ 1u : 0u;
            snap->bg.queues.push_back(e);
        }
    }

    // ---- Vehicle state ----
    // Bot is on a vehicle when Player::GetVehicle() is non-null. The
    // current seat carries an ability spell ID via VehicleSeatEntry's
    // BaseSpellID; idle rules use it to fire the seat's primary spell
    // without re-walking the seat config.
    if (Vehicle* veh = p->GetVehicle())
    {
        snap->vehicle.on_vehicle = true;
        snap->vehicle.vehicle_guid = veh->GetBase()->GetGUID();
        snap->vehicle.vehicle_seat_id = int8(p->GetTransSeat());
        snap->vehicle.vehicle_entry   = veh->GetBase()->GetEntry();
        if (VehicleSeatEntry const* seat = veh->GetSeatForPassenger(p))
        {
            // Seat ability is the spell cast on seat-enter; for siege
            // engines this is the offensive cast (boulder hurl etc).
            // Some seats have no ability — leave 0.
            (void)seat;  // Seat-ability lookup deferred — uncertain field name in this build.
        }
    }

    // ---- Active Battleground state ----
    // Populated only when the bot is inside a live BG. Per-BG scripts
    // read these to decide objective focus / flag-carrier escort etc.
    if (Battleground* bg = p->GetBattleground())
    {
        // current_bg_type_id is the ACTUAL BG the player is playing in
        // (BfG/AB/WSG/etc.), not the queue type they came from. Random BG
        // queue (bg_type_id=32) resolves to one of the real BGs; advice
        // lookup needs the resolved BG so the per-BG script (nodes, roles)
        // can be found. Fall back to queueTypeId only if GetTypeID returns
        // an unknown value.
        snap->bg.current_type_id = uint16(bg->GetTypeID());
        if (snap->bg.current_type_id == 0)
        {
            auto const& players_map = bg->GetPlayers();
            auto bp_it = players_map.find(p->GetGUID());
            if (bp_it != players_map.end())
                snap->bg.current_type_id = bp_it->second.queueTypeId.BattlemasterListId;
        }
        snap->bg.status         = uint8(bg->GetStatus());
        // GetStartDelayTime returns int32 that decreases toward 0 as the
        // gate-open event approaches. Clamp to non-negative for the
        // snapshot's uint32 field.
        snap->bg.start_delay_ms = bg->GetStartDelayTime() > 0
                                  ? uint32(bg->GetStartDelayTime()) : 0u;
        // Match elapsed since gates dropped. Source: TC's
        // Battleground::GetInProgressDuration() (Battleground.h:280) —
        // returns Milliseconds(0) during prep / waiting. Cast to uint32
        // ms; ~50 day overflow is well past any realistic match length.
        snap->bg.in_progress_ms = uint32(bg->GetInProgressDuration().count());
        snap->bg.score_alliance     = bg->GetTeamScore(TEAM_ALLIANCE);
        snap->bg.score_horde        = bg->GetTeamScore(TEAM_HORDE);
        // Real match clock (audit N54). TC's GetRemainingTime() exposes
        // m_EndTime, which is 0 for the whole IN_PROGRESS phase and only
        // arms (120s) in the ending window — every consumer saw a frozen
        // garbage value, so endgame behavior (turtle/all-in with 90s left)
        // could never trigger. Publish per-type match cap minus elapsed
        // instead; when the core's ending countdown IS armed, it wins.
        // Caps mirror retail normal-BG time limits (close enough for
        // behavior bias; exactness is not required).
        {
            // Trust m_EndTime only in its documented role: the <=120s ending
            // countdown. Live 2026-06-11 it returned a CONSTANT ~297000ms
            // through entire matches (the frozen "time=297s" in every BG
            // dump), so a bare non-zero check kept publishing garbage.
            uint32 const rem_ms = bg->GetRemainingTime();
            if (rem_ms != 0 && rem_ms <= 120'000u)
                snap->bg.time_remaining_sec = rem_ms / 1000u;
            else if (bg->GetStatus() == STATUS_IN_PROGRESS)
            {
                uint32 cap_min;
                switch (snap->bg.current_type_id)
                {
                    case 2:   // Warsong Gulch
                    case 108: cap_min = 20; break;   // Twin Peaks
                    case 1:   // Alterac Valley
                    case 30:
                    case 32:  cap_min = 60; break;   // AV variants / IoC bracket
                    default:  cap_min = 25; break;   // AB/EotS/BfG/Kotmogu/SS/...
                }
                uint32 const elapsed_s = snap->bg.in_progress_ms / 1000u;
                uint32 const cap_s = cap_min * 60u;
                snap->bg.time_remaining_sec = cap_s > elapsed_s ? cap_s - elapsed_s : 0u;
            }
            else
                snap->bg.time_remaining_sec = 0u;
        }
        // SoTA attacker-team via worldstate 3690. The SoTA script
        // updates this each round; reading it lets the bot know
        // whether it should push gates (attacker) or hold the relic
        // (defender). Other BGs leave it -1 (sentinel).
        // FindBgMap, NOT GetBgMap: GetBgMap ASSERTS on null m_Map instead of
        // returning it — a bot snapshotted during BG teardown (match ended,
        // map already detached) crashed the world thread here (2026-06-11
        // 18:35, first properly-symbolized dump after the SEH passthrough).
        if (Map* bg_world = bg->FindBgMap())
        {
            // AV / IoC score override (BG coordinator review 2026-06-10):
            // neither BG ever calls Battleground::AddPoint, so
            // GetTeamScore() reads 0 for both teams all match and every
            // score consumer (turtle/all-in bias, coordinator score_delta)
            // was inert there. The authoritative score is the
            // reinforcement worldstate pair: AV 3127/3128
            // (AV_WS_*_REINFORCEMENTS), IoC 4226/4227
            // (BG_IC_*_REINFORCEMENTS).
            if (bg_world->GetId() == 30)
            {
                const int32 a_re = bg_world->GetWorldStateValue(3127);
                const int32 h_re = bg_world->GetWorldStateValue(3128);
                snap->bg.score_alliance = uint32(a_re > 0 ? a_re : 0);
                snap->bg.score_horde    = uint32(h_re > 0 ? h_re : 0);
            }
            else if (bg_world->GetId() == 628)
            {
                const int32 a_re = bg_world->GetWorldStateValue(4226);
                const int32 h_re = bg_world->GetWorldStateValue(4227);
                snap->bg.score_alliance = uint32(a_re > 0 ? a_re : 0);
                snap->bg.score_horde    = uint32(h_re > 0 ? h_re : 0);
            }
            int32 atk = bg_world->GetWorldStateValue(3690);
            // Worldstate is 0 (Alliance) / 1 (Horde) when in SoTA;
            // 0 default elsewhere — we only meaningful read it on
            // map 607 (SA), but a 0 read on a non-SA map is harmless
            // since the script doesn't check it.
            snap->bg.sota_attacker_team = int8(atk);
            // SoTA gate destruction state. Worldstates per TC
            // src/server/scripts/Battlegrounds/StrandOfTheAncients/
            // battleground_strand_of_the_ancients.cpp:141-146. Values
            // mirror BG_SA_GateState (1=OK, 2=damaged, 3=destroyed).
            // Reading on non-SA maps returns 0 (unknown), which the
            // SoTA script gates anyway via sota_attacker_team != -1.
            snap->bg.sota_gate_state[BotSnapshot::SotaGateGreen]   = uint8(bg_world->GetWorldStateValue(3623));
            snap->bg.sota_gate_state[BotSnapshot::SotaGateBlue]    = uint8(bg_world->GetWorldStateValue(3620));
            snap->bg.sota_gate_state[BotSnapshot::SotaGateRed]     = uint8(bg_world->GetWorldStateValue(3617));
            snap->bg.sota_gate_state[BotSnapshot::SotaGatePurple]  = uint8(bg_world->GetWorldStateValue(3614));
            snap->bg.sota_gate_state[BotSnapshot::SotaGateYellow]  = uint8(bg_world->GetWorldStateValue(3638));
            snap->bg.sota_gate_state[BotSnapshot::SotaGateAncient] = uint8(bg_world->GetWorldStateValue(3849));
            // IoC keep gate destruction state. Worldstates per
            // src/server/scripts/Battlegrounds/IsleOfConquest/isle_of_conquest.h:76-87.
            // Convention: CLOSED ws = 1 means gate intact; OPEN ws = 1
            // means gate destroyed. Store as bool-ish uint8 (non-zero
            // = destroyed). Non-IoC maps read 0 for both — gates appear
            // intact, IoC script gates its consumption on bg_type_id.
            auto gate_down = [&](uint32 open_ws) -> uint8 {
                return uint8(bg_world->GetWorldStateValue(int32(open_ws)) > 0 ? 1 : 0);
            };
            snap->bg.ioc_gate_destroyed[BotSnapshot::IocGateFrontA] = gate_down(4323);
            snap->bg.ioc_gate_destroyed[BotSnapshot::IocGateWestA]  = gate_down(4324);
            snap->bg.ioc_gate_destroyed[BotSnapshot::IocGateEastA]  = gate_down(4325);
            snap->bg.ioc_gate_destroyed[BotSnapshot::IocGateFrontH] = gate_down(4322);
            snap->bg.ioc_gate_destroyed[BotSnapshot::IocGateWestH]  = gate_down(4321);
            snap->bg.ioc_gate_destroyed[BotSnapshot::IocGateEastH]  = gate_down(4320);

            // AV captain alive-state. TC tracks captain liveness via
            // worldstate (battleground_alterac_valley.cpp:374-375 +
            // OnUnitKilled at :669-681 sets ws=0 on death). Two cheap
            // worldstate reads vs. the previous per-bot full creature-
            // store scan (~3000 iters × 80 bots = 240K iters/tick on AV).
            //   AV_WS_BALINDA_ALIVE  = 1351
            //   AV_WS_GALVAGAR_ALIVE = 1352
            // ws value 1 = alive, 0 = dead. Default true outside AV
            // means scripts that read these accessors on non-AV maps
            // see "alive" — they gate on bg_type_id == 1 first.
            if (bg_world->GetId() == 30u)
            {
                snap->bg.av_balinda_alive   = (bg_world->GetWorldStateValue(1351) == 1);
                snap->bg.av_galvangar_alive = (bg_world->GetWorldStateValue(1352) == 1);
            }
        }

        // Flag-carrier detection — three mechanisms across game versions:
        //
        // 1. Legacy aura-based (WotLK WSG / EotS): aura 23333 / 23335 /
        //    34976 sits on the player while they carry.
        // 2. Modern aura-based (some BGs use SPELL_AURA_BATTLEGROUND_
        //    PLAYER_POSITION 397/398 — the AuraEffect drives the minimap
        //    icon broadcast).
        // 3. Modern GameObject-based (Cata+ WSG/TP/BfG): the flag is a
        //    GAMEOBJECT_TYPE_NEW_FLAG (type 36); when picked up the GO
        //    stores the carrier via SetFlagCarrierGUID. NO aura on the
        //    player. This is what the modern Warsong Gulch script
        //    (battleground_warsong_gulch.cpp line 235) uses to find the
        //    carrier. Without this path the snapshot reports no carrier,
        //    `acts_as_fc` keeps walking bots to the enemy pedestal even
        //    after someone grabs the flag, and the FC + escorts never
        //    converge — observed as "no bot is engaging the enemy flag"
        //    on modern WSG (map 2106) and TP (map 726).
        Team my_team = p->GetEffectiveTeam();

        // Path 3: scan flag GOs on the BG map and resolve the carrier guid.
        if (Map* bg_map = bg->FindBgMap())   // FindBgMap: see teardown note at the SoTA block
        {
            // S3 (BG audit): iterate the LIVE object store, NOT
            // GetGameObjectBySpawnIdStore(). BG flag GOs are runtime-created
            // via GameObject::CreateGameObject with spawnId 0, so they are
            // NEVER inserted into the spawn-id store — this modern GO-carrier
            // path found ZERO carriers and the snapshot relied entirely on the
            // aura fallback below (which only covers the four hard-coded
            // pickup auras, missing Kotmogu / Deephaul / any other pickup
            // spell). The live store is the same one the capture-point harvest
            // below walks.
            auto const& go_store =
                bg_map->GetObjectsStore().Data.template FindContainer<GameObject>();
            for (auto const& [_carrier_scan_guid, go_ptr] : go_store)
            {
                if (!go_ptr) continue;
                if (go_ptr->GetGoType() != GAMEOBJECT_TYPE_NEW_FLAG) continue;
                ObjectGuid carrier_guid = go_ptr->GetFlagCarrierGUID();
                if (carrier_guid.IsEmpty()) continue;
                Player* carrier = ObjectAccessor::FindPlayer(carrier_guid);
                if (!carrier) continue;
                int32 hp_pct = carrier->GetMaxHealth() > 0
                    ? int32((int64(carrier->GetHealth()) * 100) / carrier->GetMaxHealth())
                    : 0;
                bool is_friendly = (carrier->GetEffectiveTeam() == my_team);
                std::vector<ObjectGuid>& bucket = is_friendly
                    ? snap->bg.all_friendly_carriers
                    : snap->bg.all_enemy_carriers;
                // Dedup before push — a single carrier referenced by
                // multiple GOs would otherwise be double-counted, then
                // the chase-distribution `guid_low % vec.size()` skews
                // toward duplicated guids.
                bool already_in = false;
                for (auto const& g : bucket)
                    if (g == carrier_guid) { already_in = true; break; }
                if (already_in) continue;
                if (is_friendly)
                {
                    // First-detected carrier writes the scalar fields;
                    // additional carriers (Kotmogu) accumulate in the
                    // all_friendly_carriers vector.
                    if (snap->bg.friendly_flag_carrier.IsEmpty())
                    {
                        snap->bg.friendly_flag_carrier   = carrier_guid;
                        snap->bg.friendly_carrier_hp_pct = hp_pct;
                        snap->bg.friendly_carrier_x      = carrier->GetPositionX();
                        snap->bg.friendly_carrier_y      = carrier->GetPositionY();
                        snap->bg.friendly_carrier_z      = carrier->GetPositionZ();
                    }
                    bucket.push_back(carrier_guid);
                }
                else
                {
                    if (snap->bg.enemy_flag_carrier.IsEmpty())
                    {
                        snap->bg.enemy_flag_carrier   = carrier_guid;
                        snap->bg.enemy_carrier_hp_pct = hp_pct;
                        snap->bg.enemy_carrier_x      = carrier->GetPositionX();
                        snap->bg.enemy_carrier_y      = carrier->GetPositionY();
                        snap->bg.enemy_carrier_z      = carrier->GetPositionZ();
                    }
                    bucket.push_back(carrier_guid);
                }
            }
        }

        // Paths 1+2: aura-based legacy detection. Runs after the GO path
        // so modern data wins if both are present; for legacy WSG / EotS
        // the GO scan finds nothing and we fall through to auras.
        for (auto const& [guid, ref] : bg->GetPlayers())
        {
            Player* other = ObjectAccessor::FindPlayer(guid);
            if (!other) continue;
            bool carries_alliance_flag = other->HasAura(23335);
            bool carries_horde_flag    = other->HasAura(23333);
            bool carries_eots_flag     = other->HasAura(34976);
            // Temple of Kotmogu orb bearers (BG audit ToK blocker): the four
            // orbs are FLAGSTAND (type 24) GOs that are Delete()d on pickup, so
            // the GO path above never sees them, and the orb pickup spells
            // (127163 / 116524) are NOT SPELL_AURA_BATTLEGROUND_PLAYER_POSITION
            // so carries_modern_flag misses them too. 116524 is the server's
            // own "is holding orb" marker (battleground_temple_of_kotmogu.cpp).
            // Without this, i_am_orb_carrier was permanently false, the center-
            // hold scoring branch never fired, and ToK scored near zero.
            bool carries_kotmogu_orb   =
                other->HasAura(116524) || other->HasAura(127163);
            bool carries_modern_flag   =
                other->HasAuraType(SPELL_AURA_BATTLEGROUND_PLAYER_POSITION) ||
                other->HasAuraType(SPELL_AURA_BATTLEGROUND_PLAYER_POSITION_FACTIONAL);
            if (!carries_alliance_flag && !carries_horde_flag &&
                !carries_eots_flag && !carries_kotmogu_orb && !carries_modern_flag)
                continue;

            int32 hp_pct = other->GetMaxHealth() > 0
                ? int32((int64(other->GetHealth()) * 100) / other->GetMaxHealth())
                : 0;

            bool is_friendly = (other->GetEffectiveTeam() == my_team);
            // Aura path runs after the GO path. If a side already has
            // its scalar populated AND this guid is already in the
            // vector, skip (dedup). Otherwise add — Kotmogu carriers
            // get aura-flagged too and we want the full set.
            std::vector<ObjectGuid>& bucket = is_friendly
                ? snap->bg.all_friendly_carriers
                : snap->bg.all_enemy_carriers;
            bool already_in = false;
            for (auto const& g : bucket)
                if (g == guid) { already_in = true; break; }
            if (already_in) continue;
            if (is_friendly)
            {
                if (snap->bg.friendly_flag_carrier.IsEmpty())
                {
                    snap->bg.friendly_flag_carrier   = guid;
                    snap->bg.friendly_carrier_hp_pct = hp_pct;
                    snap->bg.friendly_carrier_x      = other->GetPositionX();
                    snap->bg.friendly_carrier_y      = other->GetPositionY();
                    snap->bg.friendly_carrier_z      = other->GetPositionZ();
                }
                bucket.push_back(guid);
            }
            else
            {
                if (snap->bg.enemy_flag_carrier.IsEmpty())
                {
                    snap->bg.enemy_flag_carrier   = guid;
                    snap->bg.enemy_carrier_hp_pct = hp_pct;
                    snap->bg.enemy_carrier_x      = other->GetPositionX();
                    snap->bg.enemy_carrier_y      = other->GetPositionY();
                    snap->bg.enemy_carrier_z      = other->GetPositionZ();
                }
                bucket.push_back(guid);
            }
        }

        // Capture-point states for the entire BG map. We walk the map's
        // GameObject store directly; cheap on a BG map (typically <100
        // GOs total) and avoids needing per-BG-subclass virtual hooks.
        // Each type-42 (CAPTURE_POINT) GO carries a worldState1 that
        // BG subclasses update to encode AllianceCaptured / HordeCaptured /
        // ContestedAlliance / ContestedHorde / Neutral. We translate that
        // to the snapshot's compact {owner_team, is_contested} pair.
        if (Map* bgMap = bg->FindBgMap())    // FindBgMap: see teardown note at the SoTA block
        {
            // TypeListContainer exposes its per-type map via Data.FindContainer<T>().
            // Returns unordered_map<ObjectGuid, GameObject*>&.
            auto const& goMap = bgMap->GetObjectsStore().Data.template FindContainer<GameObject>();
            for (auto const& [_guid, go] : goMap)
            {
                if (!go) continue;
                // BG-P0b: Eye of the Storm towers (and several older nodes)
                // are GAMEOBJECT_TYPE_CONTROL_ZONE (29), not CAPTURE_POINT
                // (42). The old scan only read CAPTURE_POINT, so the EotS tower
                // objective logic (Attacker/Defender/Roamer + FC cap decision)
                // had NO node_states to read and never fired. Accept both GO
                // types; read worldState1 from the matching template union
                // member.
                const uint8 goType = go->GetGoType();
                if (goType != GAMEOBJECT_TYPE_CAPTURE_POINT &&
                    goType != GAMEOBJECT_TYPE_CONTROL_ZONE) continue;
                // EotS (map 566): all four tower CONTROL_ZONEs share
                // controlZone.worldState1 = 2718 (PROGRESS_BAR_SHOW — a
                // 0/1 display toggle), so this generic read reports every
                // tower permanently Neutral (BG audit N11/N34/N68). Skip
                // them here; the dedicated per-tower worldstate table
                // below publishes the real ownership.
                if (bgMap->GetId() == 566u &&
                    goType == GAMEOBJECT_TYPE_CONTROL_ZONE) continue;
                GameObjectTemplate const* tmpl = go->GetGOInfo();
                if (!tmpl) continue;
                const uint32 wsId = (goType == GAMEOBJECT_TYPE_CAPTURE_POINT)
                    ? tmpl->capturePoint.worldState1
                    : tmpl->controlZone.worldState1;
                if (!wsId) continue;
                const int32 wsVal = bgMap->GetWorldStateValue(wsId);
                BotSnapshot::BgNodeState n{};
                n.x     = go->GetPositionX();
                n.y     = go->GetPositionY();
                n.z     = go->GetPositionZ();
                n.entry = go->GetEntry();
                n.name  = tmpl->name;
                using S = WorldPackets::Battleground::BattlegroundCapturePointState;
                switch (S(wsVal))
                {
                    case S::Neutral:           n.owner_team = 0; n.is_contested = false; break;
                    case S::ContestedHorde:    n.owner_team = 2; n.is_contested = true;  break;
                    case S::ContestedAlliance: n.owner_team = 1; n.is_contested = true;  break;
                    case S::HordeCaptured:     n.owner_team = 2; n.is_contested = false; break;
                    case S::AllianceCaptured:  n.owner_team = 1; n.is_contested = false; break;
                    default:                   n.owner_team = 0; n.is_contested = false; break;
                }
                snap->bg.node_states.push_back(n);
            }

            // B26b: AV (map 30) and IoC (map 628) banners are legacy GO
            // type 1/10 (BUTTON/GOOBER) with no worldState1, so the
            // type-42/29 scan above cannot see them — node ownership was
            // invisible on both maps and the Attacker/Defender flip logic
            // ran blind. Authoritative ownership lives in per-node
            // worldstates maintained by the BG scripts (battleground_
            // alterac_valley.cpp BGAVNodeInfo / battleground_isle_of_
            // conquest.cpp nodePointInitial + UpdateNodeWorldState): each
            // listed worldstate is an exclusive 0/1 boolean, and the
            // "assault"/"conflict" states name the team DOING the flip —
            // identical semantics to ContestedAlliance/ContestedHorde
            // above. Coords are banner-GO spawn positions from the world
            // DB so they cross-reference the BattlegroundScript advice
            // nodes within the consumers' 5-yard tolerance.
            const uint32 bgMapId = bgMap->GetId();
            if (bgMapId == 30)
            {
                struct AvWsNode
                {
                    float x, y, z;
                    int32 a_ctrl, a_aslt, h_ctrl, h_aslt;
                    // 0 = graveyard (capturable both ways forever);
                    // 1/2 = bunker/tower ORIGINAL owner. A structure whose
                    // resolved owner differs from its original owner is
                    // RAZED — towers can never be re-controlled by the
                    // attacker, the control worldstate doubles as
                    // POINT_DESTROYED (battleground_alterac_valley.cpp
                    // UpdateNodeWorldState: state >= POINT_DESTROYED).
                    uint8 original_owner;
                    char const* name;
                };
                // {AllianceControl, AllianceAssault, HordeControl,
                //  HordeAssault} per BGAVNodeInfo.
                static constexpr AvWsNode AV_WS_NODES[] = {
                    {   638.5f,  -31.5f,  46.0f, 1325, 1326, 1327, 1328, 0, "Stormpike Aid Station"    },
                    {   669.0f, -294.0f,  30.0f, 1333, 1335, 1334, 1336, 0, "Stormpike Graveyard"      },
                    {    73.7f, -426.0f,  61.0f, 1302, 1304, 1301, 1303, 0, "Stonehearth Graveyard"    },
                    {  -202.5f, -113.0f,  78.0f, 1341, 1343, 1342, 1344, 0, "Snowfall Graveyard"       },
                    {  -612.5f, -397.0f,  61.0f, 1346, 1348, 1347, 1349, 0, "Iceblood Graveyard"       },
                    { -1082.5f, -347.0f,  55.0f, 1337, 1339, 1338, 1340, 0, "Frostwolf Graveyard"      },
                    { -1402.0f, -307.0f,  89.0f, 1329, 1331, 1330, 1332, 0, "Frostwolf Relief Hut"     },
                    {   678.0f, -139.0f,  64.0f, 1361, 1375, 1370, 1378, 1, "Dun Baldar South Bunker"  },
                    {   556.0f,  -84.0f,  52.0f, 1362, 1374, 1371, 1379, 1, "Dun Baldar North Bunker"  },
                    {   203.0f, -361.0f,  56.0f, 1363, 1376, 1372, 1380, 1, "Icewing Bunker"           },
                    {  -154.0f, -446.0f,  45.0f, 1364, 1377, 1373, 1381, 1, "Stonehearth Bunker"       },
                    {  -768.0f, -363.0f,  91.0f, 1368, 1390, 1385, 1395, 2, "Tower Point"              },
                    {  -572.0f, -263.0f,  75.0f, 1367, 1389, 1384, 1394, 2, "Iceblood Tower"           },
                    { -1303.0f, -317.0f, 114.0f, 1366, 1388, 1383, 1393, 2, "East Frostwolf Tower"     },
                    { -1298.0f, -267.0f, 114.0f, 1365, 1387, 1382, 1392, 2, "West Frostwolf Tower"     },
                };
                for (auto const& w : AV_WS_NODES)
                {
                    BotSnapshot::BgNodeState n{};
                    n.x = w.x; n.y = w.y; n.z = w.z;
                    n.name = w.name;
                    if (bgMap->GetWorldStateValue(w.a_aslt) == 1)      { n.owner_team = 1; n.is_contested = true;  }
                    else if (bgMap->GetWorldStateValue(w.h_aslt) == 1) { n.owner_team = 2; n.is_contested = true;  }
                    else if (bgMap->GetWorldStateValue(w.a_ctrl) == 1) { n.owner_team = 1; n.is_contested = false; }
                    else if (bgMap->GetWorldStateValue(w.h_ctrl) == 1) { n.owner_team = 2; n.is_contested = false; }
                    // Razed structure: uncontested control by the team
                    // that did NOT build it. Consumers (coordinator
                    // garrison/attack ranking, node pickers) skip these —
                    // garrisoning a destroyed tower wastes a body all
                    // match (BG coordinator review 2026-06-10).
                    if (w.original_owner != 0 && !n.is_contested &&
                        n.owner_team != 0 && n.owner_team != w.original_owner)
                        n.is_destroyed = true;
                    snap->bg.node_states.push_back(std::move(n));
                }
            }
            else if (bgMapId == 566)
            {
                // EotS towers (BG audit N11/N34/N68 + §1 is_contested): the
                // CONTROL_ZONE GOs carry no usable worldstate (see the skip
                // above), but the core script maintains per-tower control
                // worldstates (battleground_eye_of_the_storm.cpp
                // EyeOfTheStormWorldStates :38-49, applied via
                // m_PointsIconStruct :220-223): a_ctrl/h_ctrl value 1 = that
                // team controls; the UNCONTROL worldstate is 1 while the tower
                // is neutral OR being flipped (both control bits 0). So
                // is_contested = uncontrol==1 && neither team controls — a
                // tower in play that bots should push/deny rather than treat
                // as held. Coords = the four "* Cap Pt" GO spawns on map 566.
                struct EotsWsNode
                {
                    float x, y, z;
                    int32 a_ctrl, h_ctrl, uncontrol;
                    char const* name;
                };
                static constexpr EotsWsNode EOTS_WS_NODES[] = {
                    { 2050.5f, 1372.2f, 1194.6f, 2723, 2724, 2722, "Blood Elf Tower" },
                    { 2024.6f, 1742.8f, 1195.2f, 2726, 2727, 2725, "Fel Reaver Ruins" },
                    { 2282.1f, 1760.0f, 1189.7f, 2730, 2729, 2728, "Mage Tower" },
                    { 2301.0f, 1386.9f, 1197.2f, 2732, 2733, 2731, "Draenei Ruins" },
                };
                for (auto const& w : EOTS_WS_NODES)
                {
                    BotSnapshot::BgNodeState n{};
                    n.x = w.x; n.y = w.y; n.z = w.z;
                    n.name = w.name;
                    const bool a_owns = bgMap->GetWorldStateValue(w.a_ctrl) == 1;
                    const bool h_owns = bgMap->GetWorldStateValue(w.h_ctrl) == 1;
                    if (a_owns)      n.owner_team = 1;
                    else if (h_owns) n.owner_team = 2;
                    // In play (neutral or mid-flip) → contested so the
                    // Attacker/Roamer rules pressure it.
                    n.is_contested = !a_owns && !h_owns &&
                                     bgMap->GetWorldStateValue(w.uncontrol) == 1;
                    snap->bg.node_states.push_back(std::move(n));
                }
            }
            else if (bgMapId == 628)
            {
                struct IocWsNode
                {
                    float x, y, z;
                    int32 a_conf, h_conf, a_ctrl, h_ctrl;
                    char const* name;
                };
                // {ConflictA, ConflictH, ControlledA, ControlledH} per
                // nodePointInitial (the Uncontrolled worldstate is implied
                // by all four reading 0 → neutral default). Keep
                // graveyards become cappable once the keep gates fall;
                // until then they read as owner-controlled, which keeps
                // them out of the early-game flip rotation.
                static constexpr IocWsNode IOC_WS_NODES[] = {
                    {  776.23f,  -804.28f,   6.45f, 4228, 4293, 4229, 4230, "Workshop"                },
                    {  807.78f, -1000.07f, 132.38f, 4300, 4297, 4299, 4298, "Hangar"                  },
                    {  726.39f,  -360.21f,  17.82f, 4305, 4302, 4304, 4303, "Docks"                   },
                    { 1269.50f,  -400.81f,  37.63f, 4315, 4312, 4314, 4313, "Refinery"                },
                    {  251.02f, -1159.32f,  17.24f, 4310, 4307, 4309, 4308, "Quarry"                  },
                    {  299.15f,  -784.59f,  48.92f, 4342, 4343, 4339, 4340, "Alliance Keep Graveyard" },
                    { 1284.76f,  -705.67f,  48.92f, 4347, 4348, 4344, 4345, "Horde Keep Graveyard"    },
                };
                for (auto const& w : IOC_WS_NODES)
                {
                    BotSnapshot::BgNodeState n{};
                    n.x = w.x; n.y = w.y; n.z = w.z;
                    n.name = w.name;
                    if (bgMap->GetWorldStateValue(w.a_conf) == 1)      { n.owner_team = 1; n.is_contested = true;  }
                    else if (bgMap->GetWorldStateValue(w.h_conf) == 1) { n.owner_team = 2; n.is_contested = true;  }
                    else if (bgMap->GetWorldStateValue(w.a_ctrl) == 1) { n.owner_team = 1; n.is_contested = false; }
                    else if (bgMap->GetWorldStateValue(w.h_ctrl) == 1) { n.owner_team = 2; n.is_contested = false; }
                    snap->bg.node_states.push_back(std::move(n));
                }
            }
            else if (bgMapId == 2656)
            {
                // Deephaul Ravine moving-cart tracking (BG audit §1). The two
                // mine carts are NEUTRAL moving CREATURES (entries 214690 East
                // / 217346 West) — they sit in no player's hostile/friendly
                // list, so the script's follow_creature_entry path (which only
                // scans nearby_friends/nearby_enemies) never resolves them and
                // cart escorts drifted to the STATIC spawn coord while the cart
                // rolled away. Publish each live cart as a node_state with its
                // CURRENT position + control team (worldstates copied from
                // battleground_deephaul_ravine.cpp:207-211, value 1 = that team
                // controls). The node-race consumer then escorts the moving
                // cart. (SM carts are type-29 CONTROL_ZONE GOs already caught
                // by the generic harvest above, so they need no special pass.)
                struct DhrCart { uint32 entry; int32 a_ws, h_ws; char const* name; };
                static constexpr DhrCart DHR_CARTS[] = {
                    { 214690u, 25415, 25414, "East Cart" },
                    { 217346u, 25421, 25420, "West Cart" },
                };
                auto const& creMap =
                    bgMap->GetObjectsStore().Data.template FindContainer<Creature>();
                for (auto const& cart : DHR_CARTS)
                {
                    Creature const* live = nullptr;
                    for (auto const& [_cre_guid, c] : creMap)
                        if (c && c->GetEntry() == cart.entry && c->IsAlive())
                        { live = c; break; }
                    if (!live) continue;
                    BotSnapshot::BgNodeState n{};
                    n.x = live->GetPositionX();
                    n.y = live->GetPositionY();
                    n.z = live->GetPositionZ();
                    n.entry = cart.entry;
                    n.name  = cart.name;
                    if (bgMap->GetWorldStateValue(cart.a_ws) == 1)      { n.owner_team = 1; n.is_contested = false; }
                    else if (bgMap->GetWorldStateValue(cart.h_ws) == 1) { n.owner_team = 2; n.is_contested = false; }
                    else                                                { n.owner_team = 0; n.is_contested = true;  }
                    snap->bg.node_states.push_back(std::move(n));
                }
            }
        }

        // ---- per-node player pressure (BG audit N66) ----
        // One pass over the BG's players fills alliance/horde_players_near
        // (40y) on every node-state entry, so the Defender/Roamer ranking
        // can reinforce a threatened own node BEFORE the flip starts —
        // turning the "inc <node>" chat callout's information into action.
        if (!snap->bg.node_states.empty())
        {
            for (auto const& [pp_guid, pp_ref] : bg->GetPlayers())
            {
                Player const* other = ObjectAccessor::FindPlayer(pp_guid);
                if (!other || !other->IsAlive()) continue;
                const bool is_alli = other->GetEffectiveTeam() == ALLIANCE;
                const float ox = other->GetPositionX();
                const float oy = other->GetPositionY();
                for (auto& ns : snap->bg.node_states)
                {
                    const float ndx = ns.x - ox, ndy = ns.y - oy;
                    if (ndx * ndx + ndy * ndy > 40.0f * 40.0f) continue;
                    if (is_alli) { if (ns.alliance_players_near < 250) ++ns.alliance_players_near; }
                    else         { if (ns.horde_players_near    < 250) ++ns.horde_players_near; }
                }
            }
        }

        // ---- bg_role: per-slot role from the active BattlegroundScript ----
        // Mirrors the slot→role resolution in State_Idle.cpp BgDispatch so
        // the snapshot field agrees with the role the bot will actually
        // execute this tick. Reads the IMMUTABLE advice copy the AI worker
        // publishes after each cache rebuild (BotAI::published_bg_advice —
        // adversarial review 2026-06-10: reading the worker-owned
        // BgAdviceCache directly from this world-thread code raced its
        // vector reassignment; the shared_ptr mirror is tear-free). Cold
        // on the very first BG tick — bg_role stays 0 until BgDispatch
        // publishes. We deliberately do NOT call GetAdvice() here — that
        // would double the per-tick advice churn the cache exists to avoid.
        std::shared_ptr<BotAI::PublishedBgAdvice const> bg_adv_pub;
        uint8 bg_role_form_slot = 0;
        if (BotAI* bg_role_ai = bot_ai)
        {
            bg_adv_pub = bg_role_ai->published_bg_advice();
            bg_role_form_slot = bg_role_ai->formation_slot();
        }
        if (bg_adv_pub)
        {
            if (bg_adv_pub->bg_type_id == snap->bg.current_type_id
                && !bg_adv_pub->advice.role_by_slot.empty())
            {
                auto const& slots = bg_adv_pub->advice.role_by_slot;
                uint8 slot = bg_role_form_slot;
                if (slot == 0)
                {
                    // Rank-permutation parity with BgDispatch (audit N72):
                    // slot = my guid's rank in the sorted raid roster, so
                    // every slot is occupied exactly once per team. Falls
                    // back to the guid hash when ungrouped.
                    bool ranked = false;
                    if (Group const* grp = p->GetGroup())
                    {
                        uint32 rank = 0;
                        const uint64 my_low = p->GetGUID().GetCounter();
                        for (GroupReference const& ref : grp->GetMembers())
                        {
                            Player const* member = ref.GetSource();
                            if (!member) continue;
                            const uint64 m_low = member->GetGUID().GetCounter();
                            if (m_low != my_low && m_low < my_low) ++rank;
                        }
                        slot = uint8(rank % slots.size());
                        ranked = true;
                    }
                    if (!ranked)
                        slot = uint8(p->GetGUID().GetCounter() % slots.size());
                }
                BgRole r = slots[slot % slots.size()];
                // Class-aware Healer fixup (parity with State_Idle BgDispatch):
                // a non-healer-spec bot in a Healer slot demotes to Roamer
                // so the AddonControl dashboard tally is honest.
                const uint8  my_cls  = snap->identity.cls;
                const uint16 my_spec = uint16(snap->identity.spec);
                const bool   i_can_heal = IsHealerSpec(my_cls, my_spec);
                if (r == BgRole::Healer && !i_can_heal)
                    r = BgRole::Roamer;
                // FC class-preference override (mirrors State_Idle:2123-2145).
                // When the BG script declares preferred FC classes (stealth-FC
                // meta in WSG/TP, mobility-FC in EotS), bots of preferred
                // classes get force-promoted to FlagCarrier (provided their
                // current role isn't an irreplaceable healer), and non-
                // preferred classes hashed into the FC slot drop to Roamer
                // to make room. This keeps bg_role consistent with the
                // role bot will actually execute downstream.
                if (!bg_adv_pub->advice.fc_class_preference.empty())
                {
                    bool class_preferred = false;
                    for (uint8 c : bg_adv_pub->advice.fc_class_preference)
                        if (c == my_cls) { class_preferred = true; break; }
                    if (class_preferred && !i_can_heal)
                        r = BgRole::FlagCarrier;
                    else if (!class_preferred && r == BgRole::FlagCarrier)
                        r = BgRole::Roamer;
                }
                snap->bg.bg_role = uint8(r);
            }
        }

        // ---- Team-coordinator order (BG audit N60) ----
        // Copy this bot's current order from the BgTeamCoordinator plan.
        // The coordinator's Update (the only writer) runs on the world thread
        // BEFORE the parallel snapshot-build barrier; this builder body runs on
        // the build WORKER threads. The OrderFor read is race-free because it is
        // temporally disjoint from the write — orders_ is immutable for the whole
        // build phase (see BgTeamCoordinator.h threading contract). No plan entry
        // leaves order.kind == None and the AI falls back to per-bot role logic.
        if (Services::Initialized())
            if (auto const* ord = Services::BgCoordinator().OrderFor(
                    p->GetGUID().GetCounter()))
                snap->bg.order = *ord;
    }

    // ---- PvE group-coordinator order (dungeon/raid coordination) ----
    // Same writer-before-parallel-barrier contract as the BG order copy above
    // (read on build workers, write on the world thread, temporally disjoint). The
    // coordinator only plans for groups inside dungeon/raid maps; no plan
    // entry leaves pve_order.active == false and every consumer runs the
    // legacy per-bot logic.
    if (Services::Initialized())
        if (auto const* pord = Services::PveCoordinator().OrderFor(
                p->GetGUID().GetCounter()))
            snap->pve_order = *pord;

    // ---- Bank capacity ----
    // Tab count via PlayerData; free slots by walking the bank-bag bags
    // (BANK_SLOT_BAG_START..END) and counting nullptr slots inside each.
    {
        snap->bank.bank_tab_count = p->GetCharacterBankTabCount();
        uint16 free = 0;
        for (uint8 bag_slot = BANK_SLOT_BAG_START; bag_slot < BANK_SLOT_BAG_END; ++bag_slot)
        {
            Bag* bag = p->GetBagByPos(bag_slot);
            if (!bag) continue;
            for (uint32 i = 0; i < bag->GetBagSize(); ++i)
                if (!bag->GetItemByPos(static_cast<uint8>(i)))
                    ++free;
        }
        snap->bank.bank_free_slots = free;
    }

    // ---- Taxi mask ----
    // Bytewise copy of the player's taximask. ~250 bytes for the modern
    // node table; lets BotSnapshotView::is_taxi_node_known() answer
    // "should I bother emitting fly_to here?" without round-tripping the
    // intent + Result::Locked feedback.
    {
        TaxiMask const& tm = p->m_taxi.GetTaxiMask();
        snap->travel.taxi_mask.assign(tm.data(), tm.data() + tm.size());
    }

    // ---- R7: leveling-zone relocation goal synthesis ----
    // A bot that has out-levelled its zone — no current quest, no local quest
    // offers/turn-ins, no resolvable objective — is "starved". Grinding (R9b)
    // can keep it gaining XP only if mobs are nearby; a bot parked in a city /
    // at a flightmaster / on an islanded starter zone has none. The real fix is
    // RELOCATION: send the bot to a level-appropriate quest hub (Chromie-time
    // semantics) so genuine quests surface on arrival.
    //
    // We REUSE the existing cross-map travel pipeline instead of building a new
    // one: synthesize `current_objective_poi` = the chosen hub and flag it as a
    // relocation. The recommended-taxi / portal-dock anchor (below) +
    // UnifiedTravelGraph + the State_Idle travel rules then route the bot there
    // by walk/flight/portal/ship — NEVER teleport (per no-teleport-rescue).
    // grind_starved stays true (it keys off current_quest_id==0, which we leave
    // 0), so the bot still grinds scaled mobs as a fallback while it travels.
    //
    // SCOPE: synthesis runs for ALL levels (the L>=10 floor was removed so L1-9
    // bots do same-map zone-to-zone progression too). The cross-MAP path is
    // disabled for L<10 automatically — SelectLevelingHub returns invalid below
    // L10 and LevelingBandContinents is empty there — so for low bots only the
    // SAME-MAP nearest-doable scan can fire (a directly-walkable same-map hub is
    // then deferred to idle:travel_to_hub; only a bridge/island hub relocates).
    // A directly-walkable same-map hub is left to the idle:travel_to_hub rule
    // (move_to). The pick is made STICKY on BotAI + the 0010 DB columns, now with
    // QUEST-AVAILABILITY hysteresis (reuse while the target STILL has a doable
    // quest; re-pick once exhausted) so it doesn't flip-flop per tick or restart.
    // The full hub scan runs only on a re-pick — steady state is an O(1)
    // GetQuestHubById lookup plus one early-exit doable check.
    //
    // EXCLUSION: relocation NEVER fires for a grouped bot (snap->group.group_guid
    // set). That covers owner-following bots (in the owner's group) AND all-bot
    // groups — neither should be scattered to a far hub. A solo autonomous bot
    // (owner offline → ungrouped) still relocates. Manual /goto travel is handled
    // separately below and is intentionally exempt.
    // ---- Manual travel goal (owner /goto <map> <x> <y>) ----
    // Overrides quest/hub goal selection entirely while set: the owner
    // asked for this destination, so the bot travels there via the full
    // pipeline regardless of quest state. Cleared on arrival (within 50y
    // on the right map).
    if (BotAI* mt_ai = bot_ai)
    {
        auto const& mt = mt_ai->manual_travel();
        if (mt.set)
        {
            const float mdx = mt.x - p->GetPositionX();
            const float mdy = mt.y - p->GetPositionY();
            if (p->GetMapId() == mt.map_id && mdx * mdx + mdy * mdy < 50.0f * 50.0f)
            {
                mt_ai->clear_manual_travel();
                TC_LOG_INFO("playerbot.v2",
                    "[manual_travel] {} ARRIVED at goal map={} ({:.0f},{:.0f})",
                    p->GetName(), mt.map_id, mt.x, mt.y);
            }
            else
            {
                auto& cp = snap->quest_log.current_objective_poi;
                cp.map_id = mt.map_id;
                cp.x      = mt.x;
                cp.y      = mt.y;
                // The goto command stores the BOT's z as a placeholder (the
                // issuer rarely knows the destination height). A wrong goal
                // z poisons every z-aware consumer — the router's slope-
                // gated destination attach rejected all real Brill-side
                // nodes because the goal claimed to be 95y underground.
                // Resolve the true ground height at the goal (world thread;
                // terrain tiles load on demand) once the bot is on the
                // goal's map.
                float gz = mt.z;
                if (p->GetMapId() == mt.map_id)
                {
                    const float h = p->GetMap()->GetHeight(
                        p->GetPhaseShift(), mt.x, mt.y, MAX_HEIGHT, true, 500.0f);
                    if (h > INVALID_HEIGHT)
                        gz = h;
                }
                cp.z      = gz;
                cp.radius = 30.0f;
                cp.valid  = true;
                snap->quest_log.objective_is_relocation = true;
            }
        }
    }

    // ---- Fix 2 (A): trivial-aware starvation gate ----
    // A quest is trivial (grey) when its SCALED level is at/below the bot's gray
    // threshold (GetQuestLevel respects 12.0 ContentTuning). Re-doing grey quests
    // yields a trickle of XP — the live data showed bots needing ~445 quests to
    // reach L25 by churning them. Treat the bot as leveling-starved when nothing
    // locally actionable is NON-trivial, so it relocates to fresh content. The
    // trivial quest's POI is kept (set earlier) as a FALLBACK: if relocation finds
    // no hub the bot still does it instead of idling; if a hub IS found the R7
    // synthesis below overwrites the POI. See
    // docs/PLAN_FIX2_QUEST_AVAILABILITY_RELOCATION_20260614.md.
    const int32 r7_gray = int32(Trinity::XP::GetGrayLevel(uint8(p->GetLevel())));
    auto r7_quest_trivial = [&](uint32 qid) -> bool
    {
        Quest const* tq = sObjectMgr->GetQuestTemplate(qid);
        if (!tq) return false;
        const int32 ql = p->GetQuestLevel(tq);
        // ql<=0 means the quest has no ContentTuning (legacy CT 0) → GetQuestLevel
        // can't scale it; do NOT classify as trivial (would wrongly skip real
        // legacy quests). Only a positive, scaled level at/below gray is trivial.
        return ql > 0 && ql <= r7_gray;
    };
    const bool r7_cur_trivial =
        snap->quest_log.current_quest_id != 0 &&
        r7_quest_trivial(snap->quest_log.current_quest_id);
    bool r7_has_nontrivial_offer = false;
    for (auto const& off : snap->quest_discovery.quest_offers)
        if (!r7_quest_trivial(off.quest_id)) { r7_has_nontrivial_offer = true; break; }

    // FIX 6 (group/owner exclusion): never relocate a grouped bot. group_guid is
    // set for owner-following bots (owner's group) and all-bot groups alike;
    // scattering either to a far hub is wrong. Solo autonomous bots (ungrouped)
    // still relocate. Checked on the snapshot's group_guid (NOT is_moving).
    const bool r7_in_group = !snap->group.group_guid.IsEmpty();
    // FIX 2: the L>=10 floor was REMOVED so L1-9 bots do same-map zone-to-zone
    // progression. The cross-map path stays disabled for L<10 inside
    // SelectLevelingHub / LevelingBandContinents, so only the same-map scan fires
    // for low bots.
    if ((snap->quest_log.current_quest_id == 0 || r7_cur_trivial) &&
        (!snap->quest_log.current_objective_poi.valid || r7_cur_trivial) &&
        !r7_has_nontrivial_offer &&
        snap->quest_discovery.quest_turnins.empty() &&
        !r7_in_group &&
        Services::Initialized() && Services::Hubs().IsInitialized())
    {
        // Defensive: never relocate a bot that still has a completed quest it
        // could turn in on THIS map (mirrors State_Idle's has_same_map_turnin).
        bool has_same_map_turnin = false;
        for (auto const& q : snap->quest_log.quests)
            if (q.state == 1 && q.ender_resolved && q.ender_map_id == p->GetMapId())
            { has_same_map_turnin = true; break; }

        BotAI* lvl_ai = bot_ai;
        if (!has_same_map_turnin && lvl_ai)
        {
            const uint64 guid_low = p->GetGUID().GetCounter();
            const uint8  level    = uint8(p->GetLevel());
            BotAI::LevelingZoneTarget tgt = lvl_ai->leveling_target();

            (void)level;
            // FIX 3: the discriminator for "where should a starved bot go" is
            // QUEST AVAILABILITY (not a GUID-hashed expansion — content scales
            // broadly so almost any hub is level-eligible). Both the reuse check
            // and the fresh same-map scan below use the SAME shared predicate
            // HubHasDoableQuest, so a just-picked hub can't fail reuse on the very
            // next build (which would cause a tight per-build flip-flop). The
            // predicate early-exits at the first doable quest. World-thread only
            // (live Player), so its CanTakeQuest / GetSkillValue calls are safe.

            // Fix 2 (C): quest-availability hysteresis — reuse the sticky target
            // while it STILL has unfinished, takeable quests for this bot; once the
            // bot arrives and clears it, re-pick a fresh hub. Replaces the old
            // level-bracket hysteresis, which kept routing the bot to an already-
            // exhausted hub until it dinged a new band (the stall mechanism). The
            // per-tick cost is ONE hub's quest scan (bounded).
            //
            // Follow-up 2 (staleness re-pick): do NOT reuse a CROSS-MAP target the
            // bot has failed to reach for a long time (chosen_at age > 2h). Such a
            // bot is wedged en route to a far continent (live 2026-06-14: Balastan
            // marooned in Eversong with a day-old Legion target). Dropping the
            // stale target forces a fresh pick, which the same-map-first search
            // below routes to a nearer, reachable hub.
            ::Playerbot::V2::Travel::QuestHub const* hub = nullptr;
            const uint64 now_unix = uint64(GameTime::GetGameTime());
            constexpr uint64 kStaleFarTargetSecs = 2 * 3600;
            const bool stale_far_target =
                tgt.hub_id != 0 &&
                tgt.map_id != p->GetMapId() &&
                tgt.chosen_at != 0 &&
                now_unix > tgt.chosen_at &&
                (now_unix - tgt.chosen_at) > kStaleFarTargetSecs;
            const bool reuse =
                !stale_far_target &&
                tgt.hub_id != 0 &&
                (hub = Services::Hubs().GetQuestHubById(tgt.hub_id)) != nullptr &&
                HubHasDoableQuest(p, *hub);

            if (!reuse)
            {
                // Fix 2 (B) + Follow-up 1: route to the NEAREST hub with unfinished
                // takeable quests, preferring SAME-MAP (reachable by walk/flight)
                // before any cross-continent trip. GetQuestHubsForBot CANNOT be used
                // for this: its suitability score gives every cross-map hub a flat
                // 0.5 distance factor, while a far same-map hub gets 1/(1+d/1000)
                // which drops BELOW 0.5 — so cross-map hubs outrank far same-map
                // ones, which is exactly why bots relocated cross-continent 98% of
                // the time even with local content left (live 2026-06-14). So scan
                // ALL same-map hubs explicitly, nearest first, and only fall back to
                // the legacy cross-continent selector when same-map is exhausted.
                ::Playerbot::V2::Travel::QuestHub const* picked = nullptr;
                {
                    // MUST-FIX (L1-9 band gap): for LOW bots ONLY, widen the level
                    // acceptance by ±2 (relative to the hub's [minLevel,maxLevel]).
                    // The default racial starter zones bracket ~1-9 and the next
                    // same-map zone hub often starts at minLevel~10, so a strict
                    // IsAppropriateFor would leave an L9 bot with NO appropriate
                    // same-map hub and force the (disabled-for-L<10) cross-map path.
                    // L>=10 bots use the unmodified IsAppropriateFor — no widening.
                    const bool widen_low = uint8(p->GetLevel()) < 10;
                    auto appropriate = [&](::Playerbot::V2::Travel::QuestHub const& h) -> bool
                    {
                        if (!widen_low) return h.IsAppropriateFor(*snap);
                        // L<10 widening: accept the hub if the bot's level is within
                        // ±2 of the hub's [minLevel,maxLevel] band; faction must
                        // still match exactly (FactionAllows mirrors the data-level
                        // faction gate IsAppropriateFor uses).
                        const int lvl = int(snap->identity.level);
                        const int lo  = int(h.minLevel) - 2;
                        const int hi  = h.maxLevel > 0 ? int(h.maxLevel) + 2 : 0x7fffffff;
                        if (lvl < lo || lvl > hi) return false;
                        return h.FactionAllows(*snap);
                    };
                    std::vector<std::pair<float, ::Playerbot::V2::Travel::QuestHub const*>> same_map;
                    Services::Hubs().ForEach([&](::Playerbot::V2::Travel::QuestHub const& h)
                    {
                        if (h.mapId != p->GetMapId()) return;          // same continent only
                        if (!appropriate(h)) return;                   // level band (±2 if L<10) + faction
                        same_map.emplace_back(h.GetDistanceFrom(*snap), &h);
                    });
                    std::sort(same_map.begin(), same_map.end(),
                              [](auto const& a, auto const& b) { return a.first < b.first; });
                    // Nearest same-map hub that still has a doable quest. Uses the
                    // IDENTICAL predicate as the reuse check above (no drift → no
                    // per-build flip-flop). Bounded: early-exits at the first doable
                    // hit. (QuestHub pointers stay valid post-init — _questHubs is
                    // immutable and the builder + any Reload both run on the world
                    // thread.)
                    for (auto const& dh : same_map)
                        if (HubHasDoableQuest(p, *dh.second)) { picked = dh.second; break; }
                }

                ::Playerbot::V2::Travel::LevelingHubChoice choice;
                if (picked)
                {
                    choice.hub_id     = picked->hubId;
                    choice.map_id     = picked->mapId;
                    choice.bracket_lo = uint8(std::min<uint32>(picked->minLevel ? picked->minLevel : 1u, 80u));
                    choice.bracket_hi = uint8(std::min<uint32>(picked->maxLevel ? picked->maxLevel : 80u, 80u));
                }
                else
                {
                    // Cross-continent fallback — availability-aware via the SAME
                    // shared doable predicate. The lambda runs OUTSIDE the database
                    // _mutex (SelectLevelingHub collects candidates under the lock,
                    // releases it, then calls this), so the live-Player CanTakeQuest
                    // calls are never made while the lock is held.
                    choice = Services::Hubs().SelectLevelingHub(*snap,
                        [&](::Playerbot::V2::Travel::QuestHub const& h)
                        { return HubHasDoableQuest(p, h); });
                }

                if (choice.valid())
                {
                    BotAI::LevelingZoneTarget nt;
                    nt.hub_id     = choice.hub_id;
                    nt.map_id     = choice.map_id;
                    nt.bracket_lo = choice.bracket_lo;
                    nt.bracket_hi = choice.bracket_hi;
                    nt.chosen_at  = uint64(GameTime::GetGameTime());
                    lvl_ai->set_leveling_target(nt);
                    hub = Services::Hubs().GetQuestHubById(choice.hub_id);
                    // Persist (async) so the choice survives a restart and is
                    // operator-inspectable. One write per re-pick (rare).
                    CharacterDatabase.PExecute(
                        "UPDATE playerbot_v2_character SET leveling_target_hub={}, "
                        "leveling_target_map={}, leveling_bracket_lo={}, "
                        "leveling_bracket_hi={}, leveling_chosen_at=NOW() "
                        "WHERE character_guid_low={}",
                        nt.hub_id, nt.map_id, uint32(nt.bracket_lo),
                        uint32(nt.bracket_hi), guid_low);
                }
            }

            // Synthesize the travel goal. A CROSS-MAP hub always relocates (the
            // portal/dock cascade + UnifiedTravelGraph route it). A SAME-MAP hub
            // relocates ONLY when reaching it needs a non-walk bridge (areatrigger
            // teleport / ship / portal / taxi) — i.e. a navmesh-disconnected
            // island like Teldrassil (walk to Darnassus -> AT teleport to
            // Rut'theran -> flight to Darkshore). A directly-walkable same-map hub
            // is left to idle:travel_to_hub (which owns same-map arrival + the
            // note_hub_tried anti-oscillation), so we don't regress the common case.
            if (hub)
            {
                // Signal the opportunistic band to YIELD so the bot WALKS to its
                // doable hub instead of idle:equip_upgrade-in-place. Set for EVERY
                // relocation pick, including the directly-walkable same-map case
                // below where no POI is synthesized (travel_to_hub@~50 owns
                // arrival but is otherwise outranked by equip_upgrade@600 — this
                // flag is what lets it win via has_actionable_quest()).
                snap->quest_log.has_relocation_target = true;
                const bool relocate =
                    (hub->mapId != p->GetMapId()) ||
                    SameMapRelocationNeedsBridge(p, lvl_ai, *hub);
                if (relocate)
                {
                    auto& cp = snap->quest_log.current_objective_poi;
                    cp.map_id = hub->mapId;
                    cp.x      = hub->location.GetPositionX();
                    cp.y      = hub->location.GetPositionY();
                    cp.z      = hub->location.GetPositionZ();
                    cp.radius = std::max(hub->radius, 30.0f);
                    cp.valid  = true;
                    snap->quest_log.objective_is_relocation = true;
                }
            }
        }
    }

    // ---- Same-map STUCK-bridge routing (generalises R7 island-escape) ----
    // A bot whose objective POI is on THIS map but who keeps path-failing toward
    // it (path_blocked_count high) is walking at a wall / cliff / gap the navmesh
    // can't bridge — e.g. stranded on the Orgrimmar zeppelin-tower deck with a
    // ground objective 68y straight down (proven via [terrain_walk] refuse:cliff
    // g=34 from z=102; the ONLY way down is the lift). If the travel graph can
    // reach the POI via a non-walk edge, flag it so DispatchIdle drives that
    // graph route (walk to lift -> elevator down -> walk to goal) instead of the
    // doomed naive walk into the cliff. GATED ON STUCK so only already-failing
    // bots pay the cached A* FindRoute — zero cost for the common directly-
    // walkable objective. Skipped when the POI is already a relocation goal.
    if (!snap->quest_log.objective_is_relocation)
    {
        auto const& cp = snap->quest_log.current_objective_poi;
        if (cp.valid && cp.map_id == p->GetMapId())
        {
            BotAI* br_ai = bot_ai;
            if (br_ai)
            {
                const uint64 gk = BridgeGoalKey(cp.map_id, cp.x, cp.y);
                if (br_ai->reloc_bridge_goal_key() == gk)
                {
                    // Decision already LATCHED for this exact goal — apply it
                    // every tick regardless of the live path_blocked_count (which
                    // resets to 0 the moment the bot starts following the bridge
                    // route; without this the flag would flicker and the bot would
                    // bounce between the route and the doomed naive cliff-walk).
                    snap->quest_log.objective_needs_bridge = br_ai->reloc_bridge_has();
                }
                else
                {
                    const float pdx = cp.x - p->GetPositionX();
                    const float pdy = cp.y - p->GetPositionY();
                    const float poi_dist = std::sqrt(pdx * pdx + pdy * pdy);
                    const bool stalled = br_ai->poi_progress_stalled(
                        gk, poi_dist, getMSTime());
                    if (br_ai->path_blocked_count() >= 3 || stalled)
                    {
                        // Genuinely wedged (>=3 consecutive blocks) OR no
                        // distance progress for 60s (circling a multi-level
                        // interior produces SUCCESSFUL partial paths that
                        // keep the blocked counter at zero — observed: Somi
                        // pacing the UC inner ring forever while the
                        // Undervator ride to the surface never composed) —
                        // run the A* ONCE and latch the result in the
                        // per-goal cache.
                        snap->quest_log.objective_needs_bridge =
                            GraphHasBridgeRoute(p, br_ai, cp.map_id, cp.x, cp.y, cp.z);
                    }
                }
            }
        }
    }

    // Publish the probe's VALIDATED route legs to the snapshot so the AI
    // worker executes THIS route instead of recomputing one. The worker's own
    // FindRoute runs with validate_source_walk=false (the default — the costly
    // live-navmesh probe is reserved for known-wedged bots here), so for a bot
    // wedged in a disconnected pocket it would re-derive the euclidean walk-only
    // route the bot provably cannot follow — undoing the probe's work. Guarded by the goal key so stale legs from a previous
    // goal are never published. Covers both bridge consumers: the stuck
    // same-map objective and the R7 same-map hub relocation (both probe via
    // GraphHasBridgeRoute and share the BotAI legs cache).
    if (snap->quest_log.objective_needs_bridge ||
        snap->quest_log.objective_is_relocation)
    {
        if (BotAI* br_ai = bot_ai)
        {
            auto const& cp = snap->quest_log.current_objective_poi;
            const uint64 gk = BridgeGoalKey(cp.map_id, cp.x, cp.y);
            if (br_ai->reloc_bridge_goal_key() == gk &&
                !br_ai->reloc_bridge_legs().empty())
            {
                auto& out = snap->quest_log.bridge_route;
                out.reserve(br_ai->reloc_bridge_legs().size());
                for (BotAI::PlanLeg const& pl : br_ai->reloc_bridge_legs())
                {
                    QuestLogState::BridgeLeg bl;
                    bl.kind = pl.kind; bl.to_map = pl.to_map;
                    bl.to_x = pl.to_x; bl.to_y = pl.to_y; bl.to_z = pl.to_z;
                    bl.from_x = pl.from_x; bl.from_y = pl.from_y; bl.from_z = pl.from_z;
                    bl.payload = pl.payload; bl.to_taxi_node = pl.to_taxi_node;
                    out.push_back(bl);
                }
                snap->quest_log.bridge_route_goal_key = gk;
            }
        }
    }

    // ---- Recommended taxi route (Phase B) ----
    // For same-map long-distance travel, pre-compute (a) the closest known
    // FM to the bot, (b) the closest known FM to the goal, (c) whether
    // TaxiPathGraph can route between them. Idle rule fires fly_to_node
    // when the bot reaches the start FM. We *don't* run this if the bot
    // already has no known taxi nodes (fresh char) or no nearby FM.
    if (!snap->travel.taxi_mask.empty() && p->GetTeam() != TEAM_OTHER)
    {
        // 1) Resolve the long-distance travel goal (same map only). Priority
        //    order matches the auto-mount and idle rule: current quest POI
        //    > nearest visible turn-in giver. Anything < 200 y is too short
        //    to be worth a taxi hop (FM walk + flight + dismount delays
        //    would exceed walking the goal directly).
        constexpr float kTaxiThresholdSq = 200.0f * 200.0f;
        const float bx = snap->position.x, by = snap->position.y;
        float goal_x = 0.f, goal_y = 0.f;
        bool  has_goal = false;
        if (snap->quest_log.current_quest_id != 0 && snap->quest_log.current_objective_poi.valid &&
            snap->quest_log.current_objective_poi.map_id == p->GetMapId())
        {
            const float dx = snap->quest_log.current_objective_poi.x - bx;
            const float dy = snap->quest_log.current_objective_poi.y - by;
            if (dx*dx + dy*dy > kTaxiThresholdSq)
            {
                goal_x = snap->quest_log.current_objective_poi.x;
                goal_y = snap->quest_log.current_objective_poi.y;
                has_goal = true;
            }
        }
        // No turn-in fallback during a relocation / manual-travel journey:
        // the journey's POI is (deliberately) cross-map, so it fails the
        // same-map gate above — and the fallback then recommended flights
        // toward incidental turn-in givers, sending the bot to the local
        // FM while the portal pipeline pulled it the other way (observed:
        // Somi oscillating Razor Hill FM <-> Org road on her Brill trip).
        if (!has_goal && !snap->quest_log.objective_is_relocation)
        {
            for (auto const& tin : snap->quest_discovery.quest_turnins)
            {
                for (auto const& u : snap->combat.nearby_friends)
                {
                    if (u.guid != tin.giver) continue;
                    const float dx = u.x - bx, dy = u.y - by;
                    if (dx*dx + dy*dy > kTaxiThresholdSq)
                    { goal_x = u.x; goal_y = u.y; has_goal = true; }
                    break;
                }
                if (has_goal) break;
            }
        }

        if (has_goal)
        {
            // 2) Find the closest FM in nearby_friends. nearby_friends
            //    populates npc_flags so we don't need to round-trip
            //    through ObjectAccessor here.
            //
            //    Faction filtering is implicit via three layers, so
            //    we never emit fly intents toward an FM the bot can't
            //    use:
            //     - nearby_friends contains only IsFriendlyTo creatures
            //       (Trinity::AnyFriendlyUnitInObjectRangeCheck), so
            //       hostile-faction FMs are never even considered
            //       (a Horde FM in Tirisfal won't show up for an
            //       Alliance bot wandering through max-level).
            //     - GetNearestTaxiNode below takes p->GetTeam(), so
            //       even if a "neutral" FM serves both factions
            //       (Shattrath, Dalaran sanctuaries), we resolve to
            //       the bot's team's network only.
            //     - IsTaximaskNodeKnown gates start AND destination
            //       on the bot's discovered nodes — and you can only
            //       discover a node by interacting with an FM your
            //       faction can interact with. So a node on the mask
            //       is by construction usable by p->GetTeam().
            // 2+3) PROACTIVE: find the nearest KNOWN taxi node to the BOT
            //    (start) and the nearest KNOWN node to the GOAL (dest) in one
            //    pass over the (small) node store. We do NOT require a flight
            //    master to be in scan range — a real player walks to the FM and
            //    flies; so does the bot (idle:walk_to_flightmaster walks to the
            //    start node's position, then idle:fly_to_taxi fires once the FM
            //    is visible). Faction is implicit: IsTaximaskNodeKnown is only
            //    true for nodes the bot's faction discovered.
            uint32 start_node = 0; float start_distSq = std::numeric_limits<float>::max();
            float  start_x = 0.f, start_y = 0.f, start_z = 0.f;
            uint32 best_dest = 0; float best_dest_distSq = std::numeric_limits<float>::max();
            const uint16 cont = uint16(p->GetMapId());
            const bool is_alliance_taxi = p->GetTeam() == ALLIANCE;
            for (TaxiNodesEntry const* node : sTaxiNodesStore)
            {
                if (!node) continue;
                if (node->ContinentID != cont) continue;
                if (!node->IsPartOfTaxiNetwork()) continue;
                // Faction visibility instead of mask-known for the START
                // node: walking up to an undiscovered FM DISCOVERS it
                // (Player::Taxi node learn on interaction), exactly like a
                // real player. The old IsTaximaskNodeKnown filter sent
                // bots whose mask predates newer city nodes to the
                // nearest STALE node — observed: Uraimus in Darnassus
                // walked past the in-city FM toward Rut'theran (below the
                // island, unreachable mesh) because only the classic node
                // was on his mask. Mirrors ObjectMgr::GetNearestTaxiNode.
                if (!node->GetFlags().HasFlag(is_alliance_taxi
                        ? TaxiNodeFlags::ShowOnAllianceMap
                        : TaxiNodeFlags::ShowOnHordeMap))
                    continue;
                if (node->GetFlags().HasFlag(TaxiNodeFlags::IgnoreForFindNearest))
                    continue;
                const float sdx = node->Pos.X - bx, sdy = node->Pos.Y - by;
                const float sdsq = sdx*sdx + sdy*sdy;
                if (sdsq < start_distSq)
                {
                    start_distSq = sdsq; start_node = node->ID;
                    start_x = node->Pos.X; start_y = node->Pos.Y; start_z = node->Pos.Z;
                }
                // Destination must stay mask-known: TC refuses flights TO
                // undiscovered nodes (ActivateTaxiPathTo checks the mask).
                if (!p->m_taxi.IsTaximaskNodeKnown(node->ID)) continue;
                const float gdx = node->Pos.X - goal_x, gdy = node->Pos.Y - goal_y;
                const float gdsq = gdx*gdx + gdy*gdy;
                if (gdsq < best_dest_distSq) { best_dest_distSq = gdsq; best_dest = node->ID; }
            }
            // 4) Take the flight only if the destination FM is meaningfully
            //    closer to the goal than the bot already is (else just walk).
            // "Meaningfully closer" = at least 2x closer (4x in squared
            // space). The old 1.10 factor (~5% linear) recommended taxi
            // rides for marginal gains -- Uraimus shuttled Darnassus <->
            // nearby nodes chasing a few hundred yards of "improvement"
            // toward a 1,000y goal a human would simply walk to.
            if (start_node != 0 && best_dest != 0 && best_dest != start_node &&
                best_dest_distSq * 4.0f <
                    (goal_x - bx)*(goal_x - bx) + (goal_y - by)*(goal_y - by))
            {
                // 5) Verify routability via TaxiPathGraph (same call the API
                //    uses internally).
                TaxiNodesEntry const* from_e = sTaxiNodesStore.LookupEntry(start_node);
                TaxiNodesEntry const* to_e   = sTaxiNodesStore.LookupEntry(best_dest);
                if (from_e && to_e)
                {
                    std::vector<uint32> route;
                    TaxiPathGraph::GetCompleteNodeRoute(from_e, to_e, p, route);
                    if (route.size() >= 2)
                    {
                        snap->travel.recommended_taxi_dest_node = best_dest;
                        snap->travel.recommended_taxi_hop_count =
                            uint16(std::min<size_t>(route.size(), 0xFFFFu));
                        snap->travel.recommended_taxi_start_x = start_x;
                        snap->travel.recommended_taxi_start_y = start_y;
                        snap->travel.recommended_taxi_start_z = start_z;
                        // If the start node's FM creature is already in scan
                        // range, capture its guid so idle:fly_to_taxi can
                        // interact it immediately; otherwise the bot walks to
                        // the node position first and the guid is picked up on a
                        // later snapshot once the FM is visible.
                        for (auto const& u : snap->combat.nearby_friends)
                        {
                            if ((u.npc_flags & UNIT_NPC_FLAG_FLIGHTMASTER) == 0) continue;
                            const float fdx = u.x - start_x, fdy = u.y - start_y;
                            if (fdx*fdx + fdy*fdy <= 15.0f * 15.0f)
                            { snap->travel.recommended_taxi_start_fm = u.guid; break; }
                        }
                    }
                }
            }
        }
    }

    // ---- Nearest cross-map portal/transport anchor ----
    // When the bot has a current quest goal on a different map, ask the
    // global PortalIndex for the closest static portal or ship-dock on
    // the bot's current map that reaches the goal map. The AI rule
    // (idle:walk_to_known_portal) walks the bot toward this anchor; once
    // within 30 y the SPELLCASTER/TRANSPORT GO becomes visible in
    // nearby_objects and the in-range rules (use_portal /
    // walk_to_transport / wait_on_transport) take over for the final
    // approach + interaction.
    // NOTE: map 0 (Eastern Kingdoms) is a VALID destination — the `.valid`
    // flag is the unset sentinel, not map_id==0. A previous `map_id != 0`
    // guard here silently excluded every cross-map objective whose goal map
    // is Eastern Kingdoms (Undercity, Stormwind, Ironforge, all of EK), so a
    // Horde bot on Kalimdor (map 1) with an EK turn-in (e.g. L1 Undead "Somi"
    // at the Orgrimmar→Undercity zeppelin tower) never resolved a dock/portal
    // anchor and never fired idle:walk_to_known_dock — it wandered/NoPathed
    // instead. Gate on .valid + a genuinely different map only.
    // R7: also fire for a synthesized cross-map RELOCATION goal (current_quest_id
    // is 0 but objective_is_relocation marks the POI as a real travel target),
    // so an out-levelled bot resolves the dock/portal anchor toward its leveling
    // hub exactly like a cross-map quest objective.
    if ((snap->quest_log.current_quest_id != 0 || snap->quest_log.objective_is_relocation) &&
        snap->quest_log.current_objective_poi.valid &&
        snap->quest_log.current_objective_poi.map_id != p->GetMapId() &&
        Services::Initialized() && Services::Portals().IsInitialized())
    {
        // Phase E TravelPlanner: ask the portal graph for the FIRST hop
        // toward the goal map. If a direct anchor exists (current → goal),
        // NextHopMap returns goal_map and FindNearest matches that.
        // Otherwise it returns an intermediate map (e.g., goal=Argus,
        // current=Stormwind → intermediate=Dalaran-Broken Isles), and
        // FindNearest looks for an anchor toward the intermediate. This
        // routes bots SW→Dalaran→Argus instead of greedy-failing on the
        // missing direct portal.
        const uint32 next_hop = Services::Portals().NextHopMap(
            p->GetMapId(), snap->quest_log.current_objective_poi.map_id);
        const uint32 search_dest = (next_hop != V2::Travel::PortalIndex::kNoMap)
            ? next_hop
            : snap->quest_log.current_objective_poi.map_id;
        snap->travel.next_hop_dest_map = next_hop;
        // Region of the FINAL goal (RegionMapper). On a physically-split map
        // (530 Outland/Azuremyst/Eversong; 1 Kalimdor/Teldrassil) this makes
        // FindNearest pick the anchor that LANDS in the goal's landmass (e.g.
        // the Exodar portal for a Bloodmyst goal, not the Shattrath portal that
        // lands in Outland). It only applies on the FINAL hop (search_dest ==
        // goal map); intermediate multi-hop legs pass kRegionAny since the
        // goal's region doesn't describe the intermediate map.
        const uint32 goal_region = V2::Travel::RegionForPosition(
            snap->quest_log.current_objective_poi.map_id,
            snap->quest_log.current_objective_poi.x,
            snap->quest_log.current_objective_poi.y);
        const uint32 first_hop_region =
            (search_dest == snap->quest_log.current_objective_poi.map_id)
                ? goal_region
                : V2::Travel::PortalIndex::kRegionAny;
        // Pass the goal position so a final-hop dock is chosen by where it LANDS
        // (nearest the goal), not just where it boards — disambiguates the Org
        // tower's two map-0 zeppelins (Undercity/Tirisfal vs Grom'gol/Strangle-
        // thorn). Only meaningful on the final hop; 0,0 on intermediate hops.
        const bool _final_hop = (search_dest == snap->quest_log.current_objective_poi.map_id);
        const float _goal_x = _final_hop ? snap->quest_log.current_objective_poi.x : 0.f;
        const float _goal_y = _final_hop ? snap->quest_log.current_objective_poi.y : 0.f;
        V2::Travel::PortalAnchor const* a = Services::Portals().FindNearest(
                p, p->GetMapId(), search_dest, snap->position.x, snap->position.y,
                first_hop_region, _goal_x, _goal_y);
        // Fallback: if the planner suggested an intermediate but the
        // bot's PlayerCondition (faction etc.) blocks every anchor for
        // that intermediate, try the GOAL map directly. Some hubs have
        // both faction-restricted intermediate portals and a faction-
        // neutral direct portal hidden inside Mage Tower etc.; without
        // the fallback the bot would never see the direct option.
        if (!a && next_hop != V2::Travel::PortalIndex::kNoMap &&
            next_hop != snap->quest_log.current_objective_poi.map_id)
        {
            a = Services::Portals().FindNearest(
                    p, p->GetMapId(),
                    snap->quest_log.current_objective_poi.map_id,
                    snap->position.x, snap->position.y, goal_region,
                    snap->quest_log.current_objective_poi.x,
                    snap->quest_log.current_objective_poi.y);
            if (a) snap->travel.next_hop_dest_map = kInvalidMapId;  // direct route after all
        }
        if (a)
        {
            snap->travel.nearest_portal_anchor_dest_map = a->dest_map;
            snap->travel.nearest_portal_anchor_x = a->x;
            snap->travel.nearest_portal_anchor_y = a->y;
            snap->travel.nearest_portal_anchor_z = a->z;
            snap->travel.nearest_portal_anchor_kind = static_cast<uint8>(a->kind);
            snap->travel.nearest_portal_anchor_entry = a->go_entry;
        }
    }

    // DIAG [somi_state]: definitive per-tick dump of the one stuck bot's travel
    // state — current objective (quest + POI map/valid) and resolved dock anchor
    // — to see WHY it parks at the deck instead of boarding. Name-gated so it's a
    // single bot's worth of lines, not fleet spam. (Remove once Somi is home.)
    if (p->GetName() == "Somi")
    {
        // Scan the snapshot's nearby transports (type-15 ships/zeppelins) to see
        // whether 164871 is actually IN Somi's view, its resolved dest, and 3D
        // distance — i.e. whether the board loop even has a candidate.
        std::string xports;
        for (auto const& o : snap->world_objects.nearby_objects)
        {
            if (o.go_type != /*MAP_OBJ_TRANSPORT*/ 15) continue;
            const float dx = o.x - snap->position.x, dy = o.y - snap->position.y,
                        dz = o.z - snap->position.z;
            xports += fmt::format(" [entry={} dest={} dist={:.0f}]",
                o.entry, o.teleport_dest_map, std::sqrt(dx*dx + dy*dy + dz*dz));
        }
        TC_LOG_INFO("playerbot.v2",
            "[somi_state] map={} pos=({:.0f},{:.0f},{:.0f}) quest={} poi(map={} valid={}) "
            "next_hop={} anchor(has={} entry={} dest={} z={:.0f}) nearby_xports:{}",
            p->GetMapId(), snap->position.x, snap->position.y, snap->position.z,
            snap->quest_log.current_quest_id,
            snap->quest_log.current_objective_poi.map_id,
            snap->quest_log.current_objective_poi.valid ? 1 : 0,
            snap->travel.next_hop_dest_map,
            snap->travel.nearest_portal_anchor_kind != 0 ? 1 : 0,
            snap->travel.nearest_portal_anchor_entry,
            snap->travel.nearest_portal_anchor_dest_map,
            snap->travel.nearest_portal_anchor_z,
            xports.empty() ? " none" : xports.c_str());
    }

    // ---- Homebind ----
    // Snapshot the bot's bind point so the AI can decide cross-continent
    // hearth (when goal is on a different map but homebind is on the goal's
    // map, hearth saves a flight + walk). Player::m_homebind.GetMapId() is
    // 0 on a freshly-created character before login, but that's gated by
    // the snapshot only firing for in-world bots.
    snap->travel.homebind_map_id = p->m_homebind.GetMapId();
    snap->travel.homebind_x      = p->m_homebind.GetPositionX();
    snap->travel.homebind_y      = p->m_homebind.GetPositionY();
    snap->travel.homebind_z      = p->m_homebind.GetPositionZ();

    // ---- Hearthstone availability ----
    // Fresh / low-level bots may not have the Hearthstone item yet.
    // Without this gate, idle:unstick:hearth blindly emits HearthIntent,
    // which trips API::cast_spell's "not in spellbook" check every
    // 3 seconds and spams the log forever.
    {
        constexpr uint32 HEARTHSTONE_ITEM  = 6948;
        constexpr uint32 HEARTHSTONE_SPELL = 8690;
        snap->travel.has_hearthstone = p->HasItemCount(HEARTHSTONE_ITEM, 1, /*inBankAlso*/ false);
        snap->travel.hearthstone_cd_ms = 0;
        if (snap->travel.has_hearthstone)
        {
            // Hearthstone uses the spell cooldown channel; SpellHistory
            // exposes the remaining ms. 0 means ready to cast. Resolve
            // SpellInfo nullable and use the SpellInfo overload — the
            // uint32 HasCooldown overload ASSERTS on missing SpellInfo
            // (the 2026-06-11 ghost-spell crash class).
            if (SpellInfo const* hs_si =
                    sSpellMgr->GetSpellInfo(HEARTHSTONE_SPELL, DIFFICULTY_NONE))
            {
                if (p->GetSpellHistory()->HasCooldown(hs_si))
                {
                    auto const remain = p->GetSpellHistory()->GetRemainingCooldown(hs_si);
                    snap->travel.hearthstone_cd_ms =
                        static_cast<uint32>(remain.count());
                }
            }
        }
    }

    // ---- Pet ----
    if (Pet* pet = p->GetPet())
    {
        snap->pet.pet_guid      = pet->GetGUID();
        snap->pet.pet_hp        = static_cast<int32>(pet->GetHealth());
        snap->pet.pet_max_hp    = static_cast<int32>(pet->GetMaxHealth());
        snap->pet.pet_alive     = pet->IsAlive();
        snap->pet.pet_in_combat = pet->IsInCombat();
        // Primal Rage (264667) — pet-side Bloodlust, Ferocity pets only.
        // The APL needs an honest gate before pet_cast() because that emit
        // path has no spellbook check on the bot side.
        snap->pet.pet_can_bloodlust = pet->HasSpell(264667);
        snap->pet.pet_level     = pet->GetLevel();
        snap->pet.pet_name      = pet->GetName();
        if (CreatureTemplate const* tmpl = pet->GetCreatureTemplate())
            snap->pet.pet_family = tmpl->family;
        CopyAuras(pet, snap->pet.pet_auras);
        // Pet engagement target — feeds the assist-pet rule. Without
        // this the bot has no way to know who its pet is fighting.
        if (Unit const* pv = pet->GetVictim())
            snap->pet.pet_victim = pv->GetGUID();
        // Pet attackers — feeds the peel branch of assist_pet. When the
        // pet's victim is mob A but mob B (a caster) is hammering the
        // pet from range, the rule prefers B so the bot peels instead
        // of double-assisting on A. Cap 4 keeps the snapshot bounded.
        snap->pet.pet_attackers.clear();
        auto const& patk = pet->getAttackers();
        snap->pet.pet_attackers.reserve(std::min<size_t>(patk.size(), 4u));
        for (Unit* a : patk)
        {
            if (!a) continue;
            snap->pet.pet_attackers.push_back(a->GetGUID());
            if (snap->pet.pet_attackers.size() >= 4u) break;
        }
    }

    // ---- Hunter pet stable inventory ----
    // Captures every pet known to the bot's PetStable: the up-to-5 active
    // slots, the stable slots, and any unslotted pets. Drives the
    // idle:summon_stabled_pet rule so a hunter that ends up petless (died
    // and despawned, abandoned) can pull a stabled pet at the next
    // stablemaster visit. Empty for non-hunters or pre-PetStable bots.
    if (PetStable const* stable = p->GetPetStable())
    {
        snap->pet.stable_pets.reserve(8);
        for (uint8 i = 0; i < stable->ActivePets.size(); ++i)
        {
            if (!stable->ActivePets[i]) continue;
            auto const& info = stable->ActivePets[i].value();
            BotSnapshot::StablePet sp{};
            sp.pet_number  = info.PetNumber;
            sp.creature_id = info.CreatureId;
            sp.name        = info.Name;
            sp.level       = info.Level;
            sp.slot_kind   = 0;  // active
            sp.slot_index  = i;
            snap->pet.stable_pets.push_back(std::move(sp));
        }
        for (uint8 i = 0; i < stable->StabledPets.size(); ++i)
        {
            if (!stable->StabledPets[i]) continue;
            auto const& info = stable->StabledPets[i].value();
            BotSnapshot::StablePet sp{};
            sp.pet_number  = info.PetNumber;
            sp.creature_id = info.CreatureId;
            sp.name        = info.Name;
            sp.level       = info.Level;
            sp.slot_kind   = 1;  // stabled
            sp.slot_index  = i;
            snap->pet.stable_pets.push_back(std::move(sp));
        }
        for (uint8 i = 0; i < stable->UnslottedPets.size(); ++i)
        {
            auto const& info = stable->UnslottedPets[i];
            BotSnapshot::StablePet sp{};
            sp.pet_number  = info.PetNumber;
            sp.creature_id = info.CreatureId;
            sp.name        = info.Name;
            sp.level       = info.Level;
            sp.slot_kind   = 2;  // unslotted
            sp.slot_index  = i;
            snap->pet.stable_pets.push_back(std::move(sp));
        }
    }

    // (Nearby units, full inventory, equipment, quest log — populated as
    // their consumers land. Threat list / attackers come with PerceptionBuilder.)

    // Path-block telemetry — surface BotAI's monotonic path-blocked count
    // + last-block timestamp so AI worker rules can detect anchor wedge
    // and fall through without dereferencing BotAI directly. Captured at
    // build-end so any path failures during the build window are
    // included.
    if (BotAI* tele_ai = bot_ai)
    {
        snap->path_telemetry.count   = tele_ai->path_blocked_count();
        snap->path_telemetry.last_ms = tele_ai->last_path_blocked_ms();

        // Archetype projection (#4A) — surface the read-only slice idle rules
        // gate on so they never have to hop into BotAI from a worker thread.
        BotArchetype const& arch = tele_ai->archetype();
        snap->archetype.archetype_id           = arch.archetype_id;
        snap->archetype.econ_profile           = static_cast<uint8>(arch.econ_profile);
        snap->archetype.dominant_activity      = arch.dominant_activity();
        snap->archetype.role_affinity          = arch.role_affinity;
        snap->archetype.target_session_minutes = arch.target_session_minutes;
    }

    // Craft-order board projection (#4B-2(a)). World-thread read of the board so
    // economy idle rules can post / claim / fulfil without locking the board
    // from a worker thread. The "can this bot craft order X?" check uses the
    // live spellbook (Player::HasSpell) — the same authority ClaimOpenOrder
    // uses — so the snapshot's has_claimable_order can't drift from the actual
    // claim eligibility.
    if (Services::Initialized())
    {
        Services::CraftOrders().PopulateSnapshot(
            snap->bot_id, snap->craft_orders,
            [p](uint32 spell_id) { return p && p->HasSpell(spell_id); });

        // #4B-2(a) part 2: resolve ONE crafted intermediate this bot WANTS but
        // cannot make itself, for the idle:craft_order_post rule. The want is a
        // reagent the bot is short on for its OWN known, still-skillable recipes
        // that is ITSELF the product of a recipe spell the bot does NOT know — so
        // a different fleet bot must craft it. This reuses the exact #4B buy-side
        // shortfall logic (known still-skillable recipe -> reagents -> shortfall)
        // but filters to reagents that are themselves craftable-by-someone-else,
        // turning them into a demand signal on the order board instead of (or
        // alongside) an AH buy. We surface only ONE want per snapshot (lowest
        // product entry, deterministic) — the post rule caps open orders per bot
        // and the want re-surfaces next snapshot once the first is satisfied.
        //
        // SAFE-RECIPE GUARD: ProducingRecipeSpellFor only returns concrete
        // SPELL_EFFECT_CREATE_ITEM recipes (no loot/random/conjure), so the
        // posted order is always a single-item craft PlayerbotAPI can fulfil.
        if (p && snap->craft_orders.want_spell_id == 0)
        {
            const Difficulty co_diff = p->GetMap() ? p->GetMap()->GetDifficultyID() : DIFFICULTY_NONE;
            const uint32 co_vendor_mult = Services::Config().economy_max_reagent_vendor_multiple();
            uint32 best_product = 0;     // lowest-entry wanted craftable intermediate
            uint32 best_spell   = 0;     // its producing recipe spell (NOT known by us)
            uint32 best_qty     = 0;     // shortfall units
            for (auto const& [recipe_spell, ps] : p->GetSpellMap())
            {
                if (!ps.active || ps.disabled) continue;
                RecipeMeta const* meta = FindRecipeMeta(recipe_spell);
                if (!meta) continue;
                const uint16 cur_skill = const_cast<Player*>(p)->GetSkillValue(meta->skill_line_id);
                const RecipeColor color = ResolveRecipeColor(recipe_spell, cur_skill);
                if (color == RecipeColor::Gray || color == RecipeColor::Unknown) continue;
                SpellInfo const* si = sSpellMgr->GetSpellInfo(recipe_spell, co_diff);
                if (!si) continue;
                for (size_t i = 0; i < si->Reagent.size(); ++i)
                {
                    const int32 entry = si->Reagent[i];
                    const int16 need  = si->ReagentCount[i];
                    if (entry <= 0 || need <= 0) continue;
                    const uint32 reagent = uint32(entry);
                    const uint32 have = const_cast<Player*>(p)->GetItemCount(reagent, false);
                    if (have >= uint32(need)) continue;            // not short on this reagent
                    // Is this short reagent itself a craftable product?
                    const uint32 producer = ProducingRecipeSpellFor(reagent);
                    if (producer == 0) continue;                   // gathered/vendor-only — AH path handles it
                    if (p->HasSpell(producer)) continue;           // bot CAN make it itself — not an order want
                    // Deterministic pick: lowest product entry first.
                    if (best_product != 0 && reagent >= best_product) continue;
                    best_product = reagent;
                    best_spell   = producer;
                    best_qty     = uint32(need) - have;
                }
            }
            if (best_product != 0 && best_spell != 0)
            {
                // Market-derived fair payment: the #4B-1 per-unit fair-value
                // ceiling (vendor SellPrice * multiple, or quality flat ceiling)
                // times the shortfall quantity. Using the CEILING (not a floor)
                // makes the commission attractive enough that a crafter bot
                // prefers it over plain skill-up grind, while still being bounded
                // by intrinsic value so a requester can't be drained — and the
                // requester must afford it at PostOrder time (escrow debit) or the
                // post is refused server-side.
                const uint64 unit_fair = ReagentFairValueCeiling(
                    sObjectMgr->GetItemTemplate(best_product), co_vendor_mult);
                uint64 payment = unit_fair * uint64(best_qty == 0 ? 1u : best_qty);
                payment -= (payment % 100);                        // silver-align
                if (payment < 100) payment = 100;
                snap->craft_orders.want_spell_id       = best_spell;
                snap->craft_orders.want_item_entry     = best_product;
                snap->craft_orders.want_quantity       = best_qty == 0 ? 1u : best_qty;
                snap->craft_orders.want_payment_copper = payment;
            }
        }
    }

    return snap;
}

// Public forwarder (external linkage; declared in QuestReverseIndex.h). Lets
// JunkQuestResolver reuse the exact same impossible-for-bot classification the
// snapshot builder uses, so the two never drift. The anon-namespace Impl is
// visible here (enclosing-namespace lookup within this TU).
bool QuestHasObjectiveBotCannotComplete(uint32 quest_id)
{
    return QuestHasObjectiveBotCannotCompleteImpl(quest_id);
}

// ---- Shared "doable" predicate (declared in QuestDoable.h) ----------------
// The single definition of "can this bot still take this quest", reused by the
// offers builder, the R7 relocation synthesis (reuse check + same-map scan), the
// cross-continent selector (SelectLevelingHub via a callback run outside its
// lock), and the reactive idle rules (travel_to_hub / walk_to_known_hub).
// Keeping ONE definition is what stops the builder and the
// rules from disagreeing about which hubs are exhausted (per-build flip-flop /
// bot oscillation). The gate order mirrors the offers builder's per-quest
// acceptance gate (BotSnapshotBuilder offers_from ~5660-5738): cheapest checks
// first, CanTakeQuest last. The per-bot spam caches (blacklist / impossible /
// cant_take_recent / giver_no_offers) are NOT folded in here — they are
// transient throttles, not part of "is this quest fundamentally doable", and
// the offers builder keeps applying them around this call. World-thread only
// (live Player APIs).
bool BotCanStillTakeQuestForHub(Player* p, uint32 questId)
{
    if (!p) return false;
    // Already rewarded → never doable again (non-repeatable). The offers builder
    // uses GetQuestStatus != NONE which ALSO excludes in-log quests; here we use
    // GetQuestRewardStatus per the hub-availability semantics (a quest already in
    // the bot's log is "in progress", not a reason the hub is exhausted — but
    // CanTakeQuest below rejects in-log quests via SatisfyQuestStatus anyway, so
    // the two agree on the accept decision).
    if (p->GetQuestRewardStatus(questId)) return false;
    Quest const* q = sObjectMgr->GetQuestTemplate(questId);
    if (!q) return false;
    if (q->IsRepeatable()) return false;
    if (QuestHasObjectiveBotCannotComplete(questId)) return false;
    // Bot must actually KNOW the required skill (value>0) — closes the
    // points==0 profession-quest hole the core CanTakeQuest leaves open (see
    // the offers builder's need_profession gate).
    if (uint32 reqSkill = q->GetRequiredSkill())
        if (p->GetSkillValue(reqSkill) == 0)
            return false;
    return p->CanTakeQuest(q, false);
}

bool HubHasDoableQuest(Player* p, ::Playerbot::V2::Travel::QuestHub const& h)
{
    if (!p) return false;
    // PERF: a FULL quest log makes EVERY quest fail CanTakeQuest (SatisfyQuestLog
    // inside CanAddQuest), so no hub is doable — short-circuit before the scan.
    // Cheap (no template lookups) and the common reason a parked bot finds
    // nothing.
    if (!p->SatisfyQuestLog(false)) return false;
    for (uint32 qid : h.questIds)
        if (BotCanStillTakeQuestForHub(p, qid))
            return true;   // early-exit at first doable quest
    return false;
}

} // namespace Playerbot
