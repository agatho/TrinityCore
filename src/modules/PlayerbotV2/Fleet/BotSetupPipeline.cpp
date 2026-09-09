#include "BotSetupPipeline.h"
#include "StarterQuestAutocomplete.h"
#include "../Bot/Gear/BotGearGenerator.h"
#include "../Bot/World/CapitalsTable.h"
#include "../Bot/World/ZonesByLevel.h"
#include "../Bot/World/MountsByRace.h"
#include "../Bot/World/HunterPetsByRace.h"
#include "../Bot/World/LearnFlightpaths.h"
#include "../Bot/World/ZonePicker.h"
#include "QuestDef.h"
#include "../Services.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "Map.h"
#include "MapManager.h"        // sMapMgr->FindBaseNonInstanceMap for spawn Z validation
#include "TerrainMgr.h"        // sTerrainMgr->GetAreaId for race-capital homebind
#include "PhasingHandler.h"    // GetEmptyPhaseShift() for ground-height query
#include "PlayerbotMovement.h"  // BotMovement::SafeTeleport for placement
#include "ObjectMgr.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Pet.h"
#include "CharmInfo.h"
#include "RaceMask.h"
#include "ReputationMgr.h"
#include "DB2Stores.h"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::Fleet {

PipelineFailureRing& PipelineFailureRing::Instance()
{
    static PipelineFailureRing g;
    return g;
}

void PipelineFailureRing::Record(uint64 char_guid_low, PipelineFailureEntry entry)
{
    std::lock_guard lk(mtx_);
    auto& r = rings_[char_guid_low];
    r.entries[r.head] = std::move(entry);
    r.head = (r.head + 1) % kCap;
    if (r.size < kCap) ++r.size;
}

namespace {

// Read+write helpers for the setup_pipeline_state column on playerbot_v2_character.
uint8 ReadState(uint64 char_guid_low)
{
    auto res = CharacterDatabase.PQuery(
        "SELECT setup_pipeline_state, distribution_level FROM playerbot_v2_character WHERE character_guid_low={}",
        char_guid_low);
    if (!res || !res->GetRowCount()) return 0;
    Field* fields = res->Fetch();
    return fields[0].GetUInt8();
}

uint8 ReadDistLevel(uint64 char_guid_low)
{
    auto res = CharacterDatabase.PQuery(
        "SELECT distribution_level FROM playerbot_v2_character WHERE character_guid_low={}", char_guid_low);
    if (!res || !res->GetRowCount()) return 0;
    return res->Fetch()[0].GetUInt8();
}

// Curated large general-purpose bags (BagFamily=0, no class/level requirement,
// stable item entries across patches). Earlier table mis-mapped item IDs to
// names — entry 4238 is actually "Linen Cloth" (a trade good, not a bag) and
// 4240 is "Linen Bag" (6-slot). That left freshly-distributed L1-9 bots with
// no equipped bags at all, and L10+ bots with 4× 6-slot Linen Bags = 24 slots
// total, well below what the auto-buy-bigger-bag rule considers acceptable —
// hence the "bot running back and forth between vendors trying to upgrade"
// behavior the user reported on bot Saruius.
//
// All entries below verified against Wowhead by name and slot count.
constexpr uint32 BAG_FROSTWEAVE = 41599;   // 20-slot (WotLK Tailoring)
constexpr uint32 BAG_EMBERSILK  = 54443;   // 22-slot (Cataclysm Tailoring)
constexpr uint32 BAG_ROYAL      = 75274;   // 24-slot (Mists of Pandaria)
constexpr uint32 BAG_HEXWEAVE   = 114821;  // 30-slot (Warlords of Draenor)
constexpr uint32 BAG_WILDERCLOTH = 193532; // 32-slot (Dragonflight)

// All distribution bots get a 30-slot bag in every bag slot — 120 slots
// total. The auto-buy-bigger-bag rule is gated on smallest_bag_capacity <
// 16, so 30 stays comfortably above the threshold for the bot's lifetime
// and the vendor-restock loop never re-arms.
//
// The level-tier scaffold is kept for future per-tier scaling if profession
// quests / bag-of-holding rewards land later, but right now every tier
// returns the same Hexweave entry.
uint32 BagEntryForLevel(uint8 /*level*/)
{
    return BAG_HEXWEAVE;
}

// Bind the bot's homebind (hearthstone destination) to a capital. Critical
// for low-level / starter bots: GlobalStuckRescue teleports a wedged bot to
// its Player::m_homebind, so a bot left homebound to the wrong continent (e.g.
// an Undead created/marooned in Orgrimmar) gets repeatedly rescued to the
// wrong place. Setting the homebind to the RACE-correct capital makes every
// future rescue land on the right continent.
//
// areaId is resolved from the capital coords via the terrain manager (loads
// terrain on demand — does NOT require the destination map to be resident),
// so this is correct even when called before the placement teleport completes.
void BindHomebindToCapital(Player* bot, ::Playerbot::V2::World::CapitalEntry const& cap)
{
    if (!bot) return;
    uint32 const area_id = sTerrainMgr.GetAreaId(
        PhasingHandler::GetEmptyPhaseShift(), cap.map_id, cap.x, cap.y, cap.z);
    WorldLocation const loc(cap.map_id, cap.x, cap.y, cap.z, cap.o);
    bot->SetHomebind(loc, area_id);
}

// Resolve the race-correct capital (falls back to faction default for races
// not in the race->capital table, e.g. Pandaren / new allied races).
::Playerbot::V2::World::CapitalEntry const& CapitalForBot(Player* bot)
{
    ::Playerbot::V2::World::CapitalEntry const* cap =
        ::Playerbot::V2::World::CapitalForRace(bot->GetRace());
    return cap ? *cap
               : ::Playerbot::V2::World::CapitalForFaction(bot->GetTeam() == ALLIANCE);
}

} // anonymous

void BotSetupPipeline::PersistState(uint64 char_guid_low, uint8 state, uint8 distribution_level)
{
    // DirectPExecute (sync) instead of PExecute (async). The async worker's
    // queue can backlog under heavy population load — when it does, the next
    // tick's sync ReadState reads the OLD value before the queued UPDATE
    // commits, so RunFor re-runs the entire pipeline (re-firing
    // DoPlaceAndTravel's TeleportTo) which queues another UPDATE. We
    // observed 60+ minute commit lag for some bots before the in-memory
    // setup_done_cache_ short-circuit was added.
    //
    // Sync-write costs the world thread one round-trip (~ms on a local DB),
    // but it's bounded — at most once per bot per pipeline completion, vs.
    // the unbounded re-execution loop the async path produced under load.
    CharacterDatabase.DirectPExecute(
        "UPDATE playerbot_v2_character SET setup_pipeline_state={}, distribution_level={}, "
        "distribution_at=NOW() WHERE character_guid_low={}",
        uint32(state), uint32(distribution_level), char_guid_low);
}

bool BotSetupPipeline::DoSetLevel(Player* bot, uint8 target_level)
{
    if (bot->GetLevel() >= target_level) return true;

    // Auto-complete the racial / class intro questline BEFORE leveling. DK /
    // DH / Evoker bots are created at their class start level (8/8/10) inside
    // their starter zones (Acherus / Mardum / Forbidden Reach); the starter
    // quests grant baseline class abilities (Death Coil, Eye Beam, Living
    // Flame, etc.) that GiveLevel + LearnSpellLevelDependent can't replace.
    // Allied races have similar recruitment lines that grant heritage
    // armor / racial abilities. Doing this BEFORE GiveLevel keeps the bot
    // at the natural starter level so CanTakeQuest's level gates pass.
    //
    // RunFor caps at 5s wall-clock per call. If it bails before processing
    // every candidate (all_processed=false), return false here so the
    // SetupBit::SetLevel bit stays unset and the pipeline re-enters next
    // tick to finish the remaining quests. Already-rewarded quests are
    // skipped on re-entry so partial progress isn't redone.
    //
    // Hard cap on retries: kMaxSetLevelAttempts (5) protects against a
    // bot whose quest tree is permanently broken (one quest always trips
    // the time budget). After the cap, log + proceed to GiveLevel anyway -
    // the bot misses some starter abilities but the pipeline isn't
    // permanently stuck for that bot. Counter is keyed by bot guid in a
    // static map; world-thread-only access so no synchronization needed.
    constexpr uint32 kMaxSetLevelAttempts = 5;
    static std::unordered_map<uint64, uint32> setlevel_attempts;

    uint64 const guid_low = bot->GetGUID().GetCounter();
    auto rfr = StarterQuestAutocomplete::RunFor(bot);
    if (rfr.all_processed)
    {
        // All candidate quests processed - clear the retry counter and
        // proceed to GiveLevel.
        setlevel_attempts.erase(guid_low);
    }
    else if (rfr.completed > 0)
    {
        // Made progress this call - reward at least one quest. Reset the
        // attempt counter so we keep grinding through the candidate list
        // (RunFor returns one quest per call by design to spread the
        // SaveToDB load over multiple ticks). Pipeline re-enters next
        // tick to handle the next quest.
        setlevel_attempts.erase(guid_low);
        return false;
    }
    else
    {
        // No progress this call. Either every candidate is somehow blocked
        // (CanTakeQuest fails for all), or the time budget hit before any
        // quest could complete. Count consecutive zero-progress calls and
        // give up after kMaxSetLevelAttempts so a permanently-stuck bot
        // doesn't loop forever.
        uint32 const attempts = ++setlevel_attempts[guid_low];
        if (attempts < kMaxSetLevelAttempts)
            return false;
        TC_LOG_ERROR("playerbot.v2",
            "[BotSetupPipeline] DoSetLevel for {} (guid={}): RunFor made no "
            "progress in {} consecutive attempts. Proceeding to GiveLevel "
            "anyway; bot may miss some starter abilities. Inspect quest "
            "data via .playerbot diag.",
            bot->GetName(), guid_low, attempts);
        setlevel_attempts.erase(guid_low);
    }

    // Idempotent: only level UP. Distribution never down-levels.
    bot->GiveLevel(target_level);
    bot->InitTalentForLevel();
    return true;
}

// Ensure the bot has adequate bags: every bag slot filled with a 30-slot bag,
// REPLACING undersized bags. Bags are equipped DIRECTLY into empty slots via
// EquipNewItem (no backpack store) so this works even when the backpack is full
// — the catch-22 that otherwise permanently strands a bagless bot (can't store a
// bag to equip it, can't free a backpack slot without more bag room). Called
// from DoGrantStarterKit (distribution setup) AND the hygiene gear-backfill, so
// bots that never ran distribution (manually-created / pre-fix test bots) also
// get bags. World-thread only (mutates the live Player inventory).
void BotSetupPipeline::EnsureBags(Player* bot)
{
    if (!bot) return;
    constexpr uint32 kTargetBagSlots = 30;       // matches BAG_HEXWEAVE
    const uint32 bag_entry = BagEntryForLevel(bot->GetLevel());
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        if (Item* existing = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            Bag* eb = existing->ToBag();
            if (!eb || eb->GetBagSize() >= kTargetBagSlots) continue;   // already adequate
            // Drain the undersized bag into general inventory before destroying
            // it; abort (keep the small bag) if anything can't be moved, so we
            // never data-lose items.
            bool drain_ok = true;
            for (uint32 i = 0; i < eb->GetBagSize(); ++i)
            {
                Item* contained = bot->GetItemByPos(slot, uint8(i));
                if (!contained) continue;
                ItemPosCountVec dst;
                if (bot->CanStoreItem(NULL_BAG, NULL_SLOT, dst, contained, false) != EQUIP_ERR_OK)
                { drain_ok = false; break; }
                bot->RemoveItem(slot, uint8(i), true);
                bot->StoreItem(dst, contained, true);
            }
            if (!drain_ok) continue;
            bot->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
        }
        // Direct equip into the empty bag slot — no backpack store, so a full
        // backpack can't block it (that's the whole point of EquipNewItem here).
        uint16 eq_dest;
        if (bot->CanEquipNewItem(slot, eq_dest, bag_entry, /*swap=*/false) == EQUIP_ERR_OK)
            bot->EquipNewItem(eq_dest, bag_entry, ItemContext::NONE, /*update=*/true);
    }
}

bool BotSetupPipeline::DoGrantStarterKit(Player* bot)
{
    // Money: top up to at least 500g. Don't reduce existing wealth.
    constexpr uint64 STARTER_GOLD_COPPER = 500ULL * 100ULL * 100ULL;  // 5,000,000c
    if (bot->GetMoney() < STARTER_GOLD_COPPER)
        bot->SetMoney(STARTER_GOLD_COPPER);

    // Faction reputation pre-seeding. A freshly-leveled distribution bot
    // teleported to its faction capital starts at default reputation
    // (typically Neutral with the home cities for non-native races, or
    // Friendly only for races native to that capital). Vendors require
    // Friendly+; without this pre-seed, a L40 Tauren in Stormwind can't
    // buy from any vendor.
    //
    // Set the bot Friendly (3000) with all home-faction city reputations.
    // FactionEntry IDs (Faction.db2):
    //   Alliance: Stormwind 72, Ironforge 47, Darnassus 69, Exodar 930,
    //             Gilneas 1134, Tushui Pandaren 1353
    //   Horde:    Orgrimmar 76, Darkspear Trolls 530, Thunder Bluff 81,
    //             Undercity 68, Bilgewater Cartel 1133, Silvermoon 911,
    //             Huojin Pandaren 1352
    bool alliance = bot->GetTeam() == ALLIANCE;
    static constexpr uint32 kAllianceReps[] = { 72, 47, 69, 930, 1134, 1353 };
    static constexpr uint32 kHordeReps[]    = { 76, 530, 81, 68, 1133, 911, 1352 };
    constexpr int32 kFriendly = 3000;
    auto const& reps = alliance ? std::span<uint32 const>(kAllianceReps)
                                : std::span<uint32 const>(kHordeReps);
    for (uint32 rep_id : reps)
    {
        if (FactionEntry const* fe = sFactionStore.LookupEntry(rep_id))
        {
            // Only raise; don't lower if the bot already exalted from gameplay.
            int32 current = bot->GetReputationMgr().GetReputation(fe);
            if (current < kFriendly)
                bot->GetReputationMgr().ModifyReputation(fe, kFriendly - current,
                                                          /*spillOverOnly=*/false,
                                                          /*noSpillover=*/true);
        }
    }

    // Bags: ensure every bag slot carries a 30-slot bag (see EnsureBags — uses
    // EquipNewItem so it works even with a full backpack).
    EnsureBags(bot);
    return true;
}

bool BotSetupPipeline::DoGenerateGear(Player* bot)
{
    Gear::GearGenerationContext ctx;
    ctx.level  = bot->GetLevel();
    ctx.cls    = bot->GetClass();
    ctx.spec   = uint16(AsUnderlyingType(bot->GetPrimarySpecialization()));
    ctx.bot_id = bot->GetGUID().GetCounter();

    auto gear = Gear::GenerateGearFor(ctx);
    // Diagnostic tally (B-undergearing 2026-06-17): bots observed wearing
    // ilvl-5 starter gear at L16+ despite this stage "succeeding". Capture the
    // store/equip failure codes so we can see WHY generated gear never reaches
    // the body (CanEquipItem returning non-OK with swap=true was silently
    // dropped, latching the bit as done while the bot kept starter gear).
    uint32 gen_n = uint32(gear.size()), store_fail = 0, equip_fail = 0, equipped_n = 0;
    InventoryResult first_eqfail = EQUIP_ERR_OK;
    for (auto const& g : gear)
    {
        // Add the item to inventory first.
        ItemPosCountVec dest;
        InventoryResult res = bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1);
        if (res != EQUIP_ERR_OK) { ++store_fail; continue; }
        Item* it = bot->StoreNewItem(dest, g.item_entry, true);
        if (!it) { ++store_fail; continue; }
        // Equip via Player::SwapItem — NOT the old RemoveItem+EquipItem pair.
        // CRITICAL (2026-06-17): when the target equip slot already holds the
        // starter item, calling EquipItem(occupied_slot, newItem) takes the
        // stack-merge ELSE branch in Player::EquipItem (Player.cpp:11758-11784)
        // which does pItem2->SetCount(+1) and DESTROYS the incoming item — so
        // every generated upgrade whose slot held starter gear was silently
        // destroyed and the bot kept starter gear (THE under-gearing root:
        // Varethon L16 stuck at ItemLevel 5). SwapItem does the proper
        // unequip + re-store-displaced-item dance (the same path the proven
        // API::equip_item uses). CanEquipItem(swap=true) is the exact gate
        // SwapItem applies internally, so we pre-check it for a truthful tally
        // and to skip items the bot genuinely can't wear (left in bags for the
        // State_Idle auto_equip retry).
        uint16 eq_dest;
        InventoryResult eqres = bot->CanEquipItem(g.slot, eq_dest, it, /*swap=*/true);
        if (eqres == EQUIP_ERR_OK)
        {
            const uint16 src = (uint16(it->GetBagSlot()) << 8) | it->GetSlot();
            const uint16 dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | g.slot;
            bot->SwapItem(src, dst);
            bot->AutoUnequipOffhandIfNeed();
            ++equipped_n;
        }
        else
        {
            ++equip_fail;
            if (first_eqfail == EQUIP_ERR_OK) first_eqfail = eqres;
        }
    }
    // Only log when the bot did NOT end up fully geared — keeps this to a few
    // lines per problem bot rather than one per healthy spawn.
    if (equipped_n < gen_n)
        TC_LOG_INFO("playerbot.v2",
            "[GearGen] {} L{} cls{} spec{}: generated={} equipped={} store_fail={} equip_fail={} first_equip_err={}",
            bot->GetName(), ctx.level, ctx.cls, ctx.spec, gen_n, equipped_n, store_fail, equip_fail, uint32(first_eqfail));
    return true;
}

bool BotSetupPipeline::DoAutoEquip(Player* /*bot*/)
{
    // Folded into DoGenerateGear above — this stage is a no-op kept for the
    // bitmask. Returns true so the bit advances on first run.
    return true;
}

bool BotSetupPipeline::DoApplyTalents(Player* /*bot*/)
{
    // No-op at the pipeline layer. The existing `idle:apply_context_talents`
    // State_Idle rule applies the right talent context (Raid / Leveling)
    // automatically on the bot's first AI tick — same path State_Idle already
    // takes for naturally-leveled bots. We just mark this step complete so
    // the pipeline advances; the talents land within seconds of the bot
    // entering world.
    return true;
}

bool BotSetupPipeline::DoLearnProfessions(Player* bot)
{
    // Grant 2 primary professions (one gathering + one crafting that consume
    // each other's output) and the secondaries (Cooking, Fishing) at level-
    // appropriate skill ranks. Earlier this was a no-op, on the theory that
    // bots would learn professions organically via idle:trainer when standing
    // near a trainer NPC. Empirically that doesn't happen — bots in starter
    // zones rarely pass profession trainers (mostly clustered in capitals),
    // and even those that do learn end up with skill=1 forever because they
    // never reach a node to gather and never have recipes to craft. After
    // hours of distribution, 0 / 1237 online bots had any profession skill
    // above value 1.
    //
    // The grant here gives every L10+ bot a working profession kit —
    // skill rank scaled to level (level × 5 capped to the corresponding
    // step's max). Recipes are still acquired organically via idle:trainer
    // when the bot reaches a trainer; the skill foundation here just makes
    // sure idle:gather can fire (its gating predicate is `has_skill`),
    // and lets bots gather raw materials passively as they cross zones.
    if (!bot) return true;
    const uint8 lvl = bot->GetLevel();
    if (lvl < 10) return true;

    // Skill rank step. Each step caps at +75 over the previous (Apprentice
    // 75 → Journeyman 150 → Expert 225 → Artisan 300 → Master 375 → Grand
    // Master 450). Modern WoW expansion-specific profession lines (Outland,
    // Northrend, Cata, Mists, Draenor, Legion, Kul Tiran, Zandalari,
    // Shadowlands, Dragon Isles, Khaz Algar) are separate skill_lines; we
    // only seed the classic line here, leaving expansion-specific
    // progression to the trainer rule when the bot eventually visits a
    // capital.
    uint16 step;
    uint16 max_val;
    if      (lvl < 20)  { step = 1; max_val = 75; }
    else if (lvl < 40)  { step = 2; max_val = 150; }
    else if (lvl < 55)  { step = 3; max_val = 225; }
    else if (lvl < 70)  { step = 4; max_val = 300; }
    else                { step = 5; max_val = 375; }
    const uint16 cur_val = std::min<uint16>(uint16(lvl) * 5, max_val);

    struct ProfPair
    {
        uint16 gather_skill;
        uint32 gather_apprentice;
        uint16 craft_skill;
        uint32 craft_apprentice;
    };
    // Apprentice rank spells (vanilla; these IDs are stable across all
    // modern expansions because they teach the base skill_line). Modern
    // expansion-specific profession spells layer on top via trainers.
    static constexpr ProfPair kPairs[] = {
        // 0: Mining + Blacksmithing — plate classes
        {186, 2580, 164, 2018},
        // 1: Mining + Engineering — gadgety dps
        {186, 2580, 202, 4036},
        // 2: Herbalism + Alchemy — caster
        {182, 2366, 171, 2259},
        // 3: Skinning + Leatherworking — leather classes
        {393, 8613, 165, 2108},
        // 4: Herbalism + Inscription — caster alt
        {182, 2366, 773, 45357},
        // 5: Mining + Jewelcrafting — utility
        {186, 2580, 755, 25229},
        // 6: Tailoring + Enchanting — cloth (no gather; cloth drops from mobs)
        {  0,    0, 197, 3908},
    };
    // Class-flavored pick (deterministic by class so bots of same class
    // form a coherent profession ecosystem).
    uint8 pick_idx;
    switch (bot->GetClass())
    {
        case CLASS_WARRIOR:       pick_idx = 0; break;  // Mining + BS
        case CLASS_PALADIN:       pick_idx = 0; break;
        case CLASS_DEATH_KNIGHT:  pick_idx = 0; break;
        case CLASS_HUNTER:        pick_idx = 3; break;  // Skinning + LW
        case CLASS_ROGUE:         pick_idx = 3; break;
        case CLASS_MONK:          pick_idx = 3; break;
        case CLASS_DEMON_HUNTER:  pick_idx = 3; break;
        case CLASS_DRUID:         pick_idx = 2; break;  // Herb + Alch
        case CLASS_SHAMAN:        pick_idx = 2; break;
        case CLASS_PRIEST:        pick_idx = 6; break;  // Tailor + Ench
        case CLASS_MAGE:          pick_idx = 6; break;
        case CLASS_WARLOCK:       pick_idx = 6; break;
        case CLASS_EVOKER:        pick_idx = 4; break;  // Herb + Inscription
        default:                  pick_idx = 0; break;
    }
    ProfPair const& pair = kPairs[pick_idx];

    auto grant_skill = [&](uint16 skill_id, uint32 spell_id)
    {
        if (skill_id == 0) return;
        if (spell_id != 0 && !bot->HasSpell(spell_id))
            bot->LearnSpell(spell_id, /*dependent=*/false);
        // Only-raise. On a pipeline re-run, a bot that has organically
        // skilled this profession past our seed value (e.g. crafted up to
        // 200 while we only seed 5*level) must NOT be down-leveled. Never
        // lower the current value, the max, or the step — clamp each of our
        // seed targets up to whatever the bot already has. GetPureSkillValue
        // returns the raw value without temp/perm bonuses, which is exactly
        // what SetSkill stores.
        const uint16 have_cur  = bot->GetPureSkillValue(skill_id);
        const uint16 have_max  = bot->GetPureMaxSkillValue(skill_id);
        const uint16 have_step = bot->GetSkillStep(skill_id);
        const uint16 new_cur   = std::max<uint16>(cur_val, have_cur);
        const uint16 new_max   = std::max<uint16>(max_val, have_max);
        const uint16 new_step  = std::max<uint16>(step,    have_step);
        // Nothing to do if the bot already meets or exceeds every target.
        if (have_cur >= new_cur && have_max >= new_max && have_step >= new_step)
            return;
        bot->SetSkill(skill_id, new_step, new_cur, new_max);
    };

    grant_skill(pair.gather_skill, pair.gather_apprentice);
    grant_skill(pair.craft_skill,  pair.craft_apprentice);

    // Secondaries — every bot gets Cooking + Fishing, regardless of class
    // pick. First Aid (skill 129) was removed in BfA 8.0, so it's not
    // granted here even though bandages still exist as items. Apprentice
    // ranks: Cooking 2550, Fishing 7732.
    grant_skill(/*Cooking*/  185, 2550);
    grant_skill(/*Fishing*/  356, 7732);

    // Expansion-specific profession lines. Modern WoW splits each base
    // profession (Mining 186, Alchemy 171, …) into a per-expansion child
    // skill_line (Khaz Algar Alchemy, Midnight Alchemy, …). Each child has
    // ParentSkillLineID == the base profession skill. Without these, an
    // L80+ bot has the classic base skill but cannot use current-expansion
    // reagents, gather current-expansion nodes, or craft current-tier items.
    //
    // We seed each child at 25 with a 100 cap so the bot has a foundation
    // but still skills up via gathering / crafting.
    //
    // DISCOVERY (runtime, self-correcting): rather than hand-maintain a
    // numeric ID table (the previous one had cross-profession and even
    // language-skill IDs — e.g. a Mining row contained Blacksmithing 2454
    // and the BfA columns 2462/2464 were Demonic/Goblin language skills),
    // scan sSkillLineStore once and group every child skill_line under its
    // ParentSkillLineID. This always matches whatever the loaded client
    // DB2 actually ships — including faction-shared BfA lines — so it can
    // never grant a wrong line. Built lazily on first call; the world
    // thread is the only caller of the setup pipeline, so no locking needed.
    static const std::unordered_map<uint16, std::vector<uint16>> kChildLines = []
    {
        std::unordered_map<uint16, std::vector<uint16>> map;
        for (SkillLineEntry const* sl : sSkillLineStore)
        {
            if (!sl || sl->ParentSkillLineID == 0)
                continue;
            // ParentSkillLineID points at the base profession skill_line.
            // Group this child under it. We grant ALL children of the base
            // profession (gathering nodes & crafting reagents are scaled by
            // dynamic per-zone level, so any expansion zone may be the bot's
            // destination — granting every child mirrors a maxed real player
            // and is cosmetic clutter at worst).
            map[uint16(sl->ParentSkillLineID)].push_back(uint16(sl->ID));
        }
        return map;
    }();

    auto extend_for_base = [&](uint16 base_skill)
    {
        if (base_skill == 0) return;
        auto it = kChildLines.find(base_skill);
        if (it == kChildLines.end()) return;
        for (uint16 child_id : it->second)
        {
            constexpr uint16 cap = 100;
            const uint16 seed_cur  = std::min<uint16>(cap, 25);
            // Only-raise (same rationale as grant_skill above): never
            // down-level a child line the bot has organically skilled up.
            const uint16 have_cur  = bot->GetPureSkillValue(child_id);
            const uint16 have_max  = bot->GetPureMaxSkillValue(child_id);
            const uint16 have_step = bot->GetSkillStep(child_id);
            const uint16 new_cur   = std::max<uint16>(seed_cur, have_cur);
            const uint16 new_max   = std::max<uint16>(cap,      have_max);
            const uint16 new_step  = std::max<uint16>(uint16(1), have_step);
            if (have_cur >= new_cur && have_max >= new_max && have_step >= new_step)
                continue;
            bot->SetSkill(child_id, new_step, new_cur, new_max);
        }
    };

    extend_for_base(pair.gather_skill);
    extend_for_base(pair.craft_skill);
    extend_for_base(/*Cooking*/ 185);
    extend_for_base(/*Fishing*/ 356);

    TC_LOG_INFO("playerbot.v2",
        "[BotSetupPipeline] professions granted: pick={} gather_skill={} craft_skill={} step={} val={}/{} for {} L{}",
        uint32(pick_idx), uint32(pair.gather_skill), uint32(pair.craft_skill),
        uint32(step), uint32(cur_val), uint32(max_val), bot->GetName(), uint32(lvl));
    return true;
}

bool BotSetupPipeline::DoAcquireMount(Player* bot)
{
    if (bot->GetLevel() < 20) return true;  // gated

    // Train the riding *skill* (Apprentice / Journeyman / Expert / Artisan /
    // Master) at the level-appropriate tier. This is a skill, not a racial
    // mount spell — the spell IDs are documented Blizzard skill-tier IDs
    // that have been stable across many patches.
    uint32 riding_spell = World::RidingSpellForLevel(bot->GetLevel());
    if (riding_spell && !bot->HasSpell(riding_spell))
        bot->LearnSpell(riding_spell, false);

    // We intentionally do NOT pre-grant the racial mount via LearnSpell.
    // Racial mount spell IDs drift across patches and were previously
    // hand-coded — that introduced a maintenance treadmill. Instead the bot
    // walks to a mount vendor in its capital and buys the mount item
    // organically (see Phase C `idle:buy_mount_in_capital` rule). The
    // riding skill above is enough to actually use any mount the bot
    // subsequently learns from a vendor / quest / drop.

    // Hunter pet: distribution-leveled hunters skip their racial intro quest
    // (which is where retail hunters get their first pet). Without a pet,
    // Call Pet (883) and pet-dependent abilities all silently fail. Spawn a
    // race-themed starter pet so the bot has one — same creature template
    // each race's racial questline would have given them.
    if (bot->GetClass() == CLASS_HUNTER && bot->GetPetGUID().IsEmpty())
    {
        uint32 pet_entry = World::HunterPetEntryForRace(bot->GetRace());
        if (!pet_entry)
        {
            // Unmapped race (allied) — fall back to faction-generic.
            pet_entry = (bot->GetTeam() == ALLIANCE)
                          ? World::GenericAllianceHunterPet
                          : World::GenericHordeHunterPet;
        }
        if (pet_entry)
        {
            // CreateTamedPetFrom(creatureEntry) does the heavy lifting —
            // looks up the creature_template, instantiates a Pet, calls
            // CreateBaseAtCreatureInfo + InitTamedPet (level-scales stats,
            // claims a pet stable slot). We follow up with the standard
            // post-tame steps from Spell::EffectTameCreature: AddToMap,
            // SetMinion, SavePetToDB, PetSpellInitialize.
            if (Pet* pet = bot->CreateTamedPetFrom(pet_entry, /*spell_id=*/0))
            {
                pet->SetLevel(bot->GetLevel());
                Map* pet_map = pet->GetMap();
                if (!pet_map)
                {
                    // Race vs distribution: bot not yet on map → pet has
                    // no map either. Bail out cleanly; the next
                    // pipeline tick re-enters DoAcquireMount and tries
                    // again once the bot is in-world.
                    delete pet;
                    return false;
                }
                pet_map->AddToMap(pet->ToCreature());
                bot->SetMinion(pet, true);
                pet->SavePetToDB(PET_SAVE_AS_CURRENT);
                bot->PetSpellInitialize();

                // Persist characters.summonedPetNumber immediately. Without
                // this, the very next pipeline step (DoPlaceAndTravel ->
                // TeleportTo capital) unsummons the pet and clears
                // m_petStable->CurrentPetIndex; the next periodic
                // Player::SaveToDB then writes summonedPetNumber=0 because
                // GetCurrentPet() returns null. On the bot's NEXT login,
                // Player::_LoadPetStable calls
                //   if (Pet::GetLoadPetInfo(stable, 0, summonedPetNumber, {}).first)
                //       m_temporaryUnsummonedPetNumber = summonedPetNumber;
                // — assigning ZERO when summonedPetNumber=0, so
                // ResummonPetTemporaryUnSummonedIfAny() early-returns and
                // the pet (still in character_pet, slot=0) is never auto-
                // summoned. Bot is left petless forever, idle:pet wedges.
                //
                // Direct UPDATE here captures the pet number while we
                // still have the Pet* in scope. DirectPExecute (sync) so
                // the row commits before TeleportTo runs and before the
                // pet object is potentially destroyed.
                if (CharmInfo const* ci = pet->GetCharmInfo())
                {
                    CharacterDatabase.DirectPExecute(
                        "UPDATE characters SET summonedPetNumber = {} WHERE guid = {}",
                        ci->GetPetNumber(), bot->GetGUID().GetCounter());
                }

                TC_LOG_INFO("playerbot.v2",
                    "[BotSetupPipeline] hunter pet spawned: race={} entry={} for {} L{}",
                    uint32(bot->GetRace()), pet_entry, bot->GetName(), uint32(bot->GetLevel()));
            }
            else
            {
                TC_LOG_WARN("playerbot.v2",
                    "[BotSetupPipeline] hunter pet creation failed: race={} entry={} for {} (creature_template missing or invalid)",
                    uint32(bot->GetRace()), pet_entry, bot->GetName());
            }
        }
    }
    return true;
}

bool BotSetupPipeline::DoPlaceAndTravel(Player* bot)
{
    uint8 const level = bot->GetLevel();
    bool alliance = bot->GetTeam() == ALLIANCE;

    // L<10 bots: distribution normally gates bracket lo at 10, but bots can
    // still reach this path (legacy rows, JIT/queue fill, manually-created
    // low chars). The old code bare-`return true`d here, leaving the bot
    // wherever it happened to be — and crucially leaving its homebind
    // pointing at the wrong capital/continent. GlobalStuckRescue then keeps
    // re-binding the bot to that wrong homebind (the "Undead Somi marooned in
    // Orgrimmar" bug). Place + homebind the bot at its RACE-correct starter
    // capital so rescues land on the right continent. No leveling/gear/mount
    // — the bot stays new-char-like, mirroring RunStarterOnly's intent.
    if (level < 10)
    {
        ::Playerbot::V2::World::CapitalEntry const& dest = CapitalForBot(bot);
        Playerbot::BotMovement::SafeTeleport(bot, dest.map_id, dest.x, dest.y, dest.z, dest.o, /*options*/ 0);
        BindHomebindToCapital(bot, dest);
        TC_LOG_INFO("playerbot.v2",
            "[BotSetupPipeline] Bot {} L{} (low-level) placed + homebound at race capital '{}' (map {})",
            bot->GetName(), uint32(level), dest.name, dest.map_id);
        return true;
    }

    // 1. Learn every faction-permitted flightpath. Lets the bot taxi
    //    anywhere immediately instead of grinding flight masters
    //    organically. Cheap (~600 SetTaximaskNode bit ops).
    const uint32 fp_unlocked = ::Playerbot::V2::World::LearnAllFactionFlightpaths(bot);

    // 2. Clear quest log of starter / racial-intro residue. The bot
    //    was leveled artificially via GiveLevel; its quest log still
    //    holds whatever the L1 character creation seeded. Carrying
    //    "Speak to the Innkeeper" at L40 hides level-appropriate
    //    quest givers that prerequisite-check on the chain.
    {
        std::vector<uint32> to_drop;
        to_drop.reserve(MAX_QUEST_LOG_SIZE);
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 qid = bot->GetQuestSlotQuestId(slot);
            if (qid != 0) to_drop.push_back(qid);
        }
        for (uint32 qid : to_drop)
        {
            bot->RemoveActiveQuest(qid, /*disabledQuestRemove=*/false);
            bot->TakeQuestSourceItem(qid, true);
            // Daily/weekly state can persist; safe to ignore here since
            // we're scrubbing the log not the cooldowns.
        }
    }

    // 3. Pick a level-appropriate destination zone. Round-robins across
    //    multiple candidates per bracket via the bot's GUID seed so 200
    //    L40 bots don't all spawn at the same coords.
    auto dest = ::Playerbot::V2::World::PickDestination(
        level, alliance, bot->GetGUID().GetCounter());

    // 4. Accept the expansion-gate starter quest if the destination is
    //    in an expansion zone (TBC/WotLK/MoP/WoD/Legion/BfA/SL/DF/TWW).
    //    Without this, the local hub's first quests are gated behind
    //    the prerequisite chain and the bot arrives to an empty quest
    //    log forever. Acceptance is best-effort — quest may be missing
    //    on this build / version, in which case we just log and move on.
    if (dest.starter_quest != 0)
    {
        if (Quest const* q = sObjectMgr->GetQuestTemplate(dest.starter_quest))
        {
            if (bot->CanTakeQuest(q, /*msg=*/false))
            {
                bot->AddQuestAndCheckCompletion(q, /*questGiver=*/nullptr);
                TC_LOG_INFO("playerbot.v2",
                    "[BotSetupPipeline] Bot {} L{} accepted expansion starter quest {} for zone '{}'",
                    bot->GetName(), uint32(level), dest.starter_quest, dest.zone);
            }
            else
            {
                TC_LOG_DEBUG("playerbot.v2",
                    "[BotSetupPipeline] Bot {} L{} cannot take starter quest {} (prerequisite/level/race) for '{}'; arriving anyway",
                    bot->GetName(), uint32(level), dest.starter_quest, dest.zone);
            }
        }
        else
        {
            TC_LOG_DEBUG("playerbot.v2",
                "[BotSetupPipeline] Bot {} L{}: starter quest {} not in template store; skipping",
                bot->GetName(), uint32(level), dest.starter_quest);
        }
    }

    // 5. Teleport to destination zone hub via the unified BotMovement
    //    helper. SafeTeleport probes the destination map for ground
    //    height (when loaded) and snaps to ground+2 when the requested
    //    Z is >50y above the floor — the same defence the inline probe
    //    here used to provide before being unified, plus extra
    //    diagnostic logging. Centralising in BotMovement means every
    //    cross-map teleport (placement, BG, summon, hearth) gets the
    //    same protection without per-call-site duplication.
    //
    //    The "regular travel mode" requirement applies AFTER arrival —
    //    once the bot is at the hub it autonomously taxi/walks via
    //    existing idle rules (wander_to_quest_hub, walk_to_taxi,
    //    portal cascade). This initial teleport bridges from "L1
    //    starter coords" to a useful starting hub; without it, a
    //    freshly-leveled L40 bot would spend hours walking out of
    //    Northshire.
    Playerbot::BotMovement::SafeTeleport(bot, dest.map_id,
                                         dest.x, dest.y, dest.z, dest.o,
                                         /*options*/ 0);

    TC_LOG_INFO("playerbot.v2",
        "[BotSetupPipeline] Bot {} L{} placed at '{}' (map {}, fp_unlocked={}, starter_q={})",
        bot->GetName(), uint32(level), dest.zone, dest.map_id,
        fp_unlocked, dest.starter_quest);
    return true;
}

bool BotSetupPipeline::RunFor(Player* bot, uint8 target_level)
{
    if (!bot) return false;
    uint64 char_guid = bot->GetGUID().GetCounter();
    uint8 state = ReadState(char_guid);
    if (state == SetupBit::AllDone) return true;  // already finished

    auto step = [&](uint8 bit, auto fn, char const* name) {
        if (state & bit) return;
        if (fn(bot)) state |= bit;
        else
        {
            TC_LOG_WARN("playerbot.v2", "[BotSetupPipeline] step {} failed for char {} L{}",
                        name, bot->GetName(), uint32(bot->GetLevel()));
            // Per-bot diagnostic ring. Surfaces the same failure to /diag so
            // the GM can see which step is misbehaving without scraping
            // Server.log. No early exit — pipeline continues so other steps
            // make progress.
            PipelineFailureRing::Instance().Record(char_guid,
                PipelineFailureEntry{
                    GameTime::GetGameTimeMS(),
                    bit,
                    state,
                    std::string{name}
                });
        }
    };

    if (!(state & SetupBit::SetLevel))
    {
        if (DoSetLevel(bot, target_level)) state |= SetupBit::SetLevel;

        // HARD GATE: DoSetLevel deferred (it early-returned false because the
        // starter-quest autocomplete still has candidates to process — see the
        // `rfr.completed > 0` / retry-counter branches). The bot is NOT yet at
        // target_level, so EVERY downstream step (gear, talents, professions,
        // mount, placement) would compute against the PRE-leveled level and
        // latch its done-bit at that wrong level — permanently mis-levelling
        // the bot's gear/skills. Persist whatever progress we have (just the
        // SetLevel bit, still unset here) and bail. The next pipeline tick
        // re-enters DoSetLevel and resumes; downstream steps only run once the
        // SetLevel bit is actually set (bot at target_level).
        if (!(state & SetupBit::SetLevel))
        {
            PersistState(char_guid, state, target_level);
            return false;
        }
    }
    step(SetupBit::StarterKit,       [&](Player* p){ return DoGrantStarterKit(p); }, "StarterKit");
    step(SetupBit::GenerateGear,     [&](Player* p){ return DoGenerateGear(p); },   "GenerateGear");
    step(SetupBit::AutoEquip,        [&](Player* p){ return DoAutoEquip(p); },      "AutoEquip");
    step(SetupBit::ApplyTalents,     [&](Player* p){ return DoApplyTalents(p); },   "ApplyTalents");
    step(SetupBit::LearnProfessions, [&](Player* p){ return DoLearnProfessions(p); },"LearnProfessions");
    step(SetupBit::AcquireMount,     [&](Player* p){ return DoAcquireMount(p); },   "AcquireMount");
    step(SetupBit::PlaceAndTravel,   [&](Player* p){ return DoPlaceAndTravel(p); }, "PlaceAndTravel");

    PersistState(char_guid, state, target_level);

    if (state == SetupBit::AllDone)
    {
        TC_LOG_INFO("playerbot.v2", "[BotSetupPipeline] complete for char {} L{} (guid={})",
                    bot->GetName(), uint32(bot->GetLevel()), char_guid);
        return true;
    }
    return false;
}

bool BotSetupPipeline::RunStarterOnly(Player* bot)
{
    if (!bot) return false;
    uint64 const char_guid = bot->GetGUID().GetCounter();
    if (ReadState(char_guid) == SetupBit::AllDone) return true;

    // Complete the class/racial starter quest chain. StarterQuestAutocomplete
    // rewards ONE quest per call (to spread SaveToDB load), so re-enter until
    // all_processed. Same retry/give-up policy as DoSetLevel.
    constexpr uint32 kMaxAttempts = 5;
    static std::unordered_map<uint64, uint32> attempts;
    auto rfr = StarterQuestAutocomplete::RunFor(bot);
    if (!rfr.all_processed)
    {
        if (rfr.completed > 0) { attempts.erase(char_guid); return false; }   // progress — re-enter next tick
        if (++attempts[char_guid] < kMaxAttempts) return false;               // no progress yet — retry
        attempts.erase(char_guid);
        TC_LOG_ERROR("playerbot.v2",
            "[BotSetupPipeline] RunStarterOnly for {} (guid={}): no quest progress in {} attempts; "
            "relocating anyway (bot may miss some starter abilities).",
            bot->GetName(), char_guid, kMaxAttempts);
    }
    else
    {
        attempts.erase(char_guid);
    }

    // Relocate out of the no-navmesh starter zone to the faction capital. Keep
    // the bot at its current (start) level — no GiveLevel / gear / mount, so it
    // stays new-char-like. Race-specific capital when known, else faction default.
    ::Playerbot::V2::World::CapitalEntry const& dest = CapitalForBot(bot);
    Playerbot::BotMovement::SafeTeleport(bot, dest.map_id, dest.x, dest.y, dest.z, dest.o, /*options*/ 0);
    // Re-home to the same capital so GlobalStuckRescue (which teleports a
    // wedged bot to its Player::m_homebind) rescues to the RIGHT continent
    // rather than wherever the char was created / previously marooned.
    BindHomebindToCapital(bot, dest);

    // Mark the pipeline fully done so reconcile stops re-processing this bot.
    // distribution_level stays 0 — this bot is intentionally not in distribution.
    PersistState(char_guid, SetupBit::AllDone, ReadDistLevel(char_guid));
    TC_LOG_INFO("playerbot.v2",
        "[BotSetupPipeline] RunStarterOnly complete for {} (guid={}): starter quests done, relocated to {}.",
        bot->GetName(), char_guid, dest.name);
    return true;
}

void BotSetupPipeline::Reset(Player* bot)
{
    if (!bot) return;
    uint64 char_guid = bot->GetGUID().GetCounter();
    PersistState(char_guid, 0, ReadDistLevel(char_guid));
}

} // namespace Playerbot::V2::Fleet
