/*
 * Playerbot V2 GM commands. Wired only when BUILD_PLAYERBOT_V2 is on.
 * Per v2/CONFIG.md §8.2 (admin commands).
 */

#include "ScriptMgr.h"

#if TRINITY_PLAYERBOT_V2

#include "Chat.h"
#include "ChatCommand.h"
#include "CharacterCache.h"
#include "GameObject.h"          // .playerbot bgzones — control-zone census
#include "Group.h"
#include "GroupMgr.h"
#include "Map.h"                 // .playerbot bgzones — object store + player list
#include "MapReference.h"
#include "GridDefines.h"         // INVALID_HEIGHT — .playerbot terrainprofile void check
#include "ObjectAccessor.h"
#include "Player.h"
#include "TypeContainerVisitor.h"
#include "PlayerbotAPI.h"
#include "PlayerbotMovement.h"  // BotMovement::SafeTeleport for GM summonall
#include "RBAC.h"
#include "SpellHistory.h"
#include "World.h"
#include "Bot/Battleground/BgTeamCoordinator.h"
#include "Bot/Dungeon/PveGroupCoordinator.h"
#include "LFGMgr.h"
#include "Diagnostics/BotInspector.h"
#include "Diagnostics/BotSmokeTest.h"
#include "Fleet/BotAccountMgr.h"
#include "Fleet/BotCharacterFactory.h"
#include "Fleet/BotComposition.h"
#include "Fleet/BotIdentityRegistry.h"
#include "Fleet/BotPopulationManager.h"
#include "Fleet/BotSetupPipeline.h"
#include "Fleet/OwnerRegistry.h"
#include "Bot/BotAI.h"
#include "Bot/Formation.h"
#include "Bot/BotIntentEmitter.h"
#include "Threading/IntentQueue.h"
#include "Combat/ApRegistry.h"
#include "Bot/BotRegistry.h"
#include "Bot/BotPersonality.h"
#include "Bot/BotRng.h"
#include "Session/BotSessionMgr.h"
#include "Threading/TickScheduler.h"
#include "PathGenerator.h"      // /playerbot pathcompare
#include "dtQueryFilterTC.h"    // /playerbot roadstats RoadStats counters
#include "Threading/SnapshotPublisher.h"
#include "Util/ConfigReader.h"
#include "Services.h"
#include "Diagnostics/PerfCounters.h"
#include "Bot/BotTypes.h"
#include "Bot/Dungeon/DungeonScript.h"
#include "Bot/Battleground/BattlegroundScript.h"
#include "Bot/BotSnapshot.h"
#include "Bot/BotSnapshotView.h"
#include "Session/AddonControl.h"
#include "World/WorldMetadata.h"
#include "ObjectMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "DB2Stores.h"
#include <array>
#include <map>

using namespace Trinity::ChatCommands;

namespace
{
// Resolve a character name to its low GUID (== BotId). Works for both online
// and offline characters by falling back to sCharacterCache. Returns 0 when
// no character with that name exists. This lets GMs run `.playerbot mark
// <name>` against a character that has never logged in — a chicken-and-egg
// fix for the original mark/unmark which required `FindConnectedPlayerByName`.
Playerbot::BotId ResolveCharacterId(std::string const& name)
{
    if (name.empty()) return 0;
    if (Player const* p = ObjectAccessor::FindConnectedPlayerByName(name))
        return p->GetGUID().GetCounter();
    ObjectGuid g = sCharacterCache->GetCharacterGuidByName(name);
    return g.IsEmpty() ? 0 : g.GetCounter();
}

// Map-object-store visitor collecting CONTROL_ZONE gameobjects for the
// `.playerbot bgzones` diagnostic. File-scope because local classes can't
// carry the template Visit overload TypeContainerVisitor requires.
struct ControlZoneCollector
{
    std::vector<GameObject*> zones;
    template<class T>
    void Visit(std::unordered_map<ObjectGuid, T*>&) { }
    void Visit(std::unordered_map<ObjectGuid, GameObject*>& m)
    {
        for (auto const& [guid, go] : m)
            if (go && go->GetGoType() == GAMEOBJECT_TYPE_CONTROL_ZONE)
                zones.push_back(go);
    }
};

// Lower-case a string in place (no locale, ASCII only — race/class names).
std::string ToLowerAscii(std::string s)
{
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return s;
}

// Parse a race name OR a numeric race id. Returns 0 on unrecognized input.
// Aliases match common short forms: 'elf' -> NightElf, 'be' -> BloodElf, etc.
uint8 ParseRace(std::string const& token)
{
    if (token.empty()) return 0;
    if (std::isdigit(static_cast<unsigned char>(token.front())))
        return uint8(std::strtoul(token.c_str(), nullptr, 10));
    std::string s = ToLowerAscii(token);
    if (s == "human")                         return RACE_HUMAN;
    if (s == "orc")                           return RACE_ORC;
    if (s == "dwarf")                         return RACE_DWARF;
    if (s == "nightelf" || s == "elf")        return RACE_NIGHTELF;
    if (s == "undead"   || s == "forsaken")   return RACE_UNDEAD_PLAYER;
    if (s == "tauren")                        return RACE_TAUREN;
    if (s == "gnome")                         return RACE_GNOME;
    if (s == "troll")                         return RACE_TROLL;
    if (s == "goblin")                        return RACE_GOBLIN;
    if (s == "bloodelf" || s == "be")         return RACE_BLOODELF;
    if (s == "draenei")                       return RACE_DRAENEI;
    if (s == "worgen")                        return RACE_WORGEN;
    if (s == "pandaren") /* alliance default */ return RACE_PANDAREN_ALLIANCE;
    if (s == "pandarena")                     return RACE_PANDAREN_ALLIANCE;
    if (s == "pandarenh")                     return RACE_PANDAREN_HORDE;
    // Legion / BfA allied races
    if (s == "nightborne"  || s == "nb")      return RACE_NIGHTBORNE;
    if (s == "highmountain"|| s == "hmt")     return RACE_HIGHMOUNTAIN_TAUREN;
    if (s == "voidelf"     || s == "ve")      return RACE_VOID_ELF;
    if (s == "lightforged" || s == "ld")      return RACE_LIGHTFORGED_DRAENEI;
    if (s == "zandalari"   || s == "zt")      return RACE_ZANDALARI_TROLL;
    if (s == "kultiran"    || s == "kt")      return RACE_KUL_TIRAN;
    if (s == "darkiron"    || s == "did")     return RACE_DARK_IRON_DWARF;
    if (s == "vulpera")                       return RACE_VULPERA;
    if (s == "magharorc"   || s == "magh")    return RACE_MAGHAR_ORC;
    if (s == "mechagnome"  || s == "mech")    return RACE_MECHAGNOME;
    // Dragonflight / TWW / Midnight
    if (s == "dracthyr"    || s == "dra")     return RACE_DRACTHYR_ALLIANCE; // alliance default
    if (s == "dracthyra")                     return RACE_DRACTHYR_ALLIANCE;
    if (s == "dracthyrh")                     return RACE_DRACTHYR_HORDE;
    if (s == "earthen"     || s == "ear")     return RACE_EARTHEN_DWARF_ALLIANCE; // alliance default
    if (s == "earthena")                      return RACE_EARTHEN_DWARF_ALLIANCE;
    if (s == "earthenh")                      return RACE_EARTHEN_DWARF_HORDE;
    if (s == "haranir"     || s == "harronir") return uint8(86); // RACE_HARRONIR — pending TC enable
    return 0;
}

// Parse a class name OR a numeric class id. 0 on failure.
uint8 ParseClass(std::string const& token)
{
    if (token.empty()) return 0;
    if (std::isdigit(static_cast<unsigned char>(token.front())))
        return uint8(std::strtoul(token.c_str(), nullptr, 10));
    std::string s = ToLowerAscii(token);
    if (s == "warrior")                          return CLASS_WARRIOR;
    if (s == "paladin"   || s == "pala")         return CLASS_PALADIN;
    if (s == "hunter"    || s == "hunt")         return CLASS_HUNTER;
    if (s == "rogue")                            return CLASS_ROGUE;
    if (s == "priest")                           return CLASS_PRIEST;
    if (s == "deathknight" || s == "dk")         return CLASS_DEATH_KNIGHT;
    if (s == "shaman"    || s == "sham")         return CLASS_SHAMAN;
    if (s == "mage")                             return CLASS_MAGE;
    if (s == "warlock"   || s == "lock" || s == "wlock") return CLASS_WARLOCK;
    if (s == "monk")                             return CLASS_MONK;
    if (s == "druid")                            return CLASS_DRUID;
    if (s == "demonhunter" || s == "dh")         return CLASS_DEMON_HUNTER;
    if (s == "evoker"    || s == "evk")          return CLASS_EVOKER;
    return 0;
}

// Static-validation pass for `.playerbot smoketest dungeon`. Walks each
// registered DungeonScript (or one, when `only_map` != 0) and verifies:
//   1. `bosses[]` non-empty (warns when empty — playable without it but
//      tank-advance falls back to boss-Cell scan only).
//   2. Every boss creature_template entry resolves via sObjectMgr.
//   3. Every mandatory_interrupt_spells id resolves via sSpellMgr.
//   4. Every dangerous_auras id resolves via sSpellMgr.
// Counts PASS/WARN/FAIL per script and prints a summary table. Runs
// synchronously on the calling thread — no bots involved.
bool RunDungeonValidation(ChatHandler* handler, uint32 only_map)
{
    auto& mgr = Playerbot::Services::Dungeons();
    if (mgr.size() == 0)
    {
        handler->SendSysMessage("No dungeon scripts registered.");
        return false;
    }
    // Snapshot view requires a real bot, but get_advice via empty arg is
    // not safe. Instead, call the per-script's get_advice with a stub —
    // we only need the raw advice fields, not snapshot-conditional ones.
    // Most scripts ignore the snapshot anyway (their advice is static).
    // For scripts that DO consult the snapshot (rare), we'll miss those
    // branches — acceptable for a static-validation pass.
    uint32 total_scripts = 0;
    uint32 pass_count    = 0;
    uint32 warn_count    = 0;
    uint32 fail_count    = 0;
    std::vector<std::string> fail_lines;
    std::vector<std::string> warn_lines;

    mgr.for_each_script([&](Playerbot::DungeonScript const& script)
    {
        if (only_map != 0 && script.map_id() != only_map) return;
        ++total_scripts;

        // We don't have a real snapshot to pass, but most scripts don't
        // consult the view — so passing a null reference would crash.
        // Use a default-constructed BotSnapshot wrapped in a view. The
        // view's accessors return zero/empty so any code path is safe.
        Playerbot::BotSnapshot empty_snap{};
        Playerbot::BotSnapshotView empty_view(empty_snap);
        Playerbot::DungeonAdvice a = script.get_advice(empty_view);

        uint32 local_warn = 0, local_fail = 0;
        std::string detail;

        if (a.bosses.empty())
        {
            ++local_warn;
            detail += "no_bosses ";
        }
        // Map_id sanity — catches typos (LostCityOfTolvir 754→755 bug
        // class). Map.db2 lookup is cheap.
        if (!sMapStore.LookupEntry(script.map_id()))
        {
            ++local_fail;
            detail += fmt::format("bad_map_id={} ", script.map_id());
        }
        for (uint32 e : a.bosses)
        {
            if (!sObjectMgr->GetCreatureTemplate(e))
            {
                ++local_fail;
                detail += fmt::format("bad_boss_entry={} ", e);
            }
        }
        for (uint32 s : a.mandatory_interrupt_spells)
        {
            if (!sSpellMgr->GetSpellInfo(s, DIFFICULTY_NONE))
            {
                ++local_warn;  // many spell ids are diff-specific; warn not fail
                detail += fmt::format("unk_intr={} ", s);
            }
        }
        for (uint32 s : a.dangerous_auras)
        {
            if (!sSpellMgr->GetSpellInfo(s, DIFFICULTY_NONE))
            {
                ++local_warn;
                detail += fmt::format("unk_aura={} ", s);
            }
        }

        if (local_fail > 0)
        {
            ++fail_count;
            fail_lines.push_back(fmt::format("[FAIL] {} (map={}): {}",
                script.name(), script.map_id(), detail));
        }
        else if (local_warn > 0)
        {
            ++warn_count;
            warn_lines.push_back(fmt::format("[WARN] {} (map={}): {}",
                script.name(), script.map_id(), detail));
        }
        else
        {
            ++pass_count;
        }
    });

    if (total_scripts == 0)
    {
        handler->PSendSysMessage(
            "No script found for map_id=%u. Use `.playerbot smoketest dungeon all` "
            "to list all registered dungeons.", only_map);
        return false;
    }

    handler->PSendSysMessage(
        "==== Dungeon validation: %u scripts | PASS=%u WARN=%u FAIL=%u ====",
        total_scripts, pass_count, warn_count, fail_count);
    for (auto const& l : fail_lines) handler->SendSysMessage(l);
    for (auto const& l : warn_lines) handler->SendSysMessage(l);
    if (fail_count == 0 && warn_count == 0)
        handler->SendSysMessage("All scripts clean.");

    return fail_count == 0;
}

// Static-validation pass for `.playerbot smoketest bg`. Walks each
// registered BattlegroundScript and verifies:
//   1. role_by_slot non-empty (else the bot never gets a job).
//   2. role_by_slot does not contain only Free / silent entries.
//   3. Coords are non-NaN and in plausible ranges (|x|,|y| < 20000 — TC's
//      map bounds; |z| < 1500 — Hyjal top elevation).
//   4. If endgame_creature_entry set, the entry resolves via sObjectMgr.
//   5. If vehicle_creature_entries set, each resolves via sObjectMgr.
//   6. If FlagCarrier role present, auto_use_go_types non-empty (the bot
//      needs a pickup mechanism — modern NEW_FLAG (36) etc.).
//   7. Nodes with follow_creature_entry set must resolve to a real entry.
//   8. Per-vehicle seat-spell IDs (vehicle_seat_spell + by_entry) must
//      resolve via sSpellMgr.
//   9. AliasToBaseBg coverage: walk a curated set of BattlemasterList
//      IDs from SharedDefines and assert TryGetScriptFor returns non-null.
// Runs synchronously on the calling thread — no bots involved.
bool RunBgValidation(ChatHandler* handler, uint32 only_id)
{
    auto& mgr = Playerbot::Services::Battlegrounds();
    if (mgr.size() == 0)
    { handler->SendSysMessage("No battleground scripts registered."); return false; }

    uint32 total_scripts = 0, pass_count = 0, warn_count = 0, fail_count = 0;
    std::vector<std::string> fail_lines, warn_lines;

    auto coord_sane = [](float x, float y, float z) -> bool {
        if (std::isnan(x) || std::isnan(y) || std::isnan(z)) return false;
        if (std::fabs(x) > 20000.0f) return false;
        if (std::fabs(y) > 20000.0f) return false;
        if (std::fabs(z) > 1500.0f)  return false;
        return true;
    };

    mgr.for_each_script([&](Playerbot::BattlegroundScript const& script)
    {
        if (only_id != 0 && script.bg_type_id() != only_id) return;
        ++total_scripts;

        // Both faction passes — most scripts branch on s.is_horde().
        // We don't have a real BotSnapshotView; an empty view's accessors
        // return zero/false safely. Note: the script may produce
        // faction-symmetric advice; we accept that.
        Playerbot::BotSnapshot empty_snap{};
        Playerbot::BotSnapshotView empty_view(empty_snap);
        Playerbot::BattlegroundAdvice a = script.get_advice(empty_view);

        uint32 local_warn = 0, local_fail = 0;
        std::string detail;

        if (a.role_by_slot.empty())
        { ++local_fail; detail += "no_roles "; }
        else
        {
            bool any_real = false;
            for (auto r : a.role_by_slot) if (r != Playerbot::BgRole::Free) { any_real = true; break; }
            if (!any_real) { ++local_warn; detail += "all_roles_free "; }
        }

        if ((a.enemy_flag_x != 0.f || a.enemy_flag_y != 0.f) &&
            !coord_sane(a.enemy_flag_x, a.enemy_flag_y, a.enemy_flag_z))
        { ++local_fail; detail += "bad_enemy_flag_coord "; }
        if ((a.own_flag_x != 0.f || a.own_flag_y != 0.f) &&
            !coord_sane(a.own_flag_x, a.own_flag_y, a.own_flag_z))
        { ++local_fail; detail += "bad_own_flag_coord "; }
        if ((a.home_base_x != 0.f || a.home_base_y != 0.f) &&
            !coord_sane(a.home_base_x, a.home_base_y, a.home_base_z))
        { ++local_fail; detail += "bad_home_base_coord "; }
        if ((a.endgame_target_x != 0.f || a.endgame_target_y != 0.f) &&
            !coord_sane(a.endgame_target_x, a.endgame_target_y, a.endgame_target_z))
        { ++local_fail; detail += "bad_endgame_coord "; }

        // FC role requires a pickup mechanism (auto_use_go_types) OR live
        // flag-carrier detection via auras. Modern CTF needs NEW_FLAG (36).
        bool has_fc_role = false;
        for (auto r : a.role_by_slot)
            if (r == Playerbot::BgRole::FlagCarrier || r == Playerbot::BgRole::OrbCarrier)
            { has_fc_role = true; break; }
        if (has_fc_role && a.auto_use_go_types.empty())
        { ++local_warn; detail += "fc_role_no_pickup_types "; }

        // Endgame creature must resolve. Stub get_advice on horde-stub
        // gave us one side's entry; we accept either side. If non-zero
        // and the lookup fails, that's a fail.
        if (a.endgame_creature_entry != 0 &&
            !sObjectMgr->GetCreatureTemplate(a.endgame_creature_entry))
        { ++local_fail; detail += fmt::format("bad_endgame_entry={} ",
                                              a.endgame_creature_entry); }

        for (uint32_t e : a.vehicle_creature_entries)
        {
            if (!sObjectMgr->GetCreatureTemplate(e))
            { ++local_fail; detail += fmt::format("bad_vehicle_entry={} ", e); }
        }

        if (a.vehicle_seat_spell != 0 &&
            !sSpellMgr->GetSpellInfo(a.vehicle_seat_spell, DIFFICULTY_NONE))
        { ++local_warn; detail += fmt::format("unk_seat_spell={} ",
                                              a.vehicle_seat_spell); }
        for (auto const& [entry, spell] : a.vehicle_seat_spell_by_entry)
        {
            if (!sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE))
            { ++local_warn; detail += fmt::format("unk_seat_spell_{}={} ", entry, spell); }
        }

        // Nodes — coord sanity + follow_creature_entry resolution.
        for (auto const& n : a.nodes)
        {
            if ((n.x != 0.f || n.y != 0.f) && !coord_sane(n.x, n.y, n.z))
            { ++local_fail; detail += fmt::format("bad_node_coord({}) ", n.name); }
            if (n.follow_creature_entry != 0 &&
                !sObjectMgr->GetCreatureTemplate(n.follow_creature_entry))
            { ++local_fail; detail += fmt::format("bad_follow_entry={} ", n.follow_creature_entry); }
        }

        // fc_class_preference class-ids must be in [1, 13] (Warrior..Evoker).
        // A typo (e.g. 14, 0) would silently no-op without this check.
        for (uint8_t c : a.fc_class_preference)
            if (c == 0 || c > 13)
            { ++local_warn; detail += fmt::format("bad_fc_class={} ", c); }

        if (local_fail > 0)
        {
            ++fail_count;
            fail_lines.push_back(fmt::format("[FAIL] {} (id={}): {}",
                script.name(), script.bg_type_id(), detail));
        }
        else if (local_warn > 0)
        {
            ++warn_count;
            warn_lines.push_back(fmt::format("[WARN] {} (id={}): {}",
                script.name(), script.bg_type_id(), detail));
        }
        else
        {
            ++pass_count;
        }
    });

    if (total_scripts == 0)
    {
        handler->PSendSysMessage(
            "No script found for bg_type_id=%u. Use `.playerbot smoketest bg all` "
            "to list all registered BGs.", only_id);
        return false;
    }

    // Alias-coverage assertion when running 'all'. Curated list of
    // BattlemasterList IDs from SharedDefines.h — excludes queue
    // meta-IDs (RB 32, RATED_* 100..102, RANDOM_EPIC 901, RANDOM_BG 1029)
    // and PvE / open-world IDs (SS_VS_TM 789, CI 887) which legitimately
    // have no bot script.
    uint32 uncovered = 0;
    std::vector<std::string> uncovered_lines;
    if (only_id == 0)
    {
        static constexpr uint16_t kAllBgIds[] = {
              1,   2,   3,   4,   5,   6,   7,   8,   9,  10,
             11,  30, 108, 120, 699, 708, 719, 754, 757, 803,
            808, 816, 846, 847, 849, 853, 857, 858, 859, 860,
            861, 862, 866, 868, 869, 870, 871, 872, 873, 874,
            875, 879, 880, 881, 882, 883, 884, 885, 886, 890,
            894, 897, 902, 903, 904, 905, 906, 907, 908, 909,
            910, 911, 912, 913,
            1014, 1017, 1018, 1019, 1020, 1021, 1022, 1025,
            1030, 1033, 1036, 1037, 1039, 1041
        };
        for (uint16_t bid : kAllBgIds)
        {
            if (mgr.TryGetScriptFor(bid) == nullptr)
            {
                ++uncovered;
                uncovered_lines.push_back(fmt::format("id={}", bid));
            }
        }
    }

    handler->PSendSysMessage(
        "==== BG validation: %u scripts | PASS=%u WARN=%u FAIL=%u | uncovered_ids=%u ====",
        total_scripts, pass_count, warn_count, fail_count, uncovered);
    for (auto const& l : fail_lines) handler->SendSysMessage(l);
    for (auto const& l : warn_lines) handler->SendSysMessage(l);
    if (uncovered > 0)
    {
        handler->PSendSysMessage("Uncovered BattlemasterList ids:");
        std::string joined;
        for (size_t i = 0; i < uncovered_lines.size(); ++i)
        {
            if (i) joined += ", ";
            joined += uncovered_lines[i];
        }
        handler->SendSysMessage(joined);
    }
    if (fail_count == 0 && warn_count == 0 && uncovered == 0)
        handler->SendSysMessage("All BG scripts clean and every BML id covered.");

    return fail_count == 0 && uncovered == 0;
}

} // anonymous namespace

class playerbot_v2_commandscript : public CommandScript
{
public:
    playerbot_v2_commandscript() : CommandScript("playerbot_v2_commandscript") {}

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable v2Table =
        {
            { "inspect",       HandleInspect,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "status",        HandleStatus,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "activity",      HandleActivity,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "levels",        HandleLevels,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "maps",          HandleMaps,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "quests",        HandleQuests,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "crafting",      HandleCrafting,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "stuck",         HandleStuck,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bg",            HandleBg,            rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "mark",          HandleMark,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "unmark",        HandleUnmark,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "adopt",         HandleAdopt,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "disown",        HandleDisown,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "owner",         HandleOwner,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "squadsave",     HandleSquadSave,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "squadload",     HandleSquadLoad,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "list",          HandleList,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "count",         HandleCount,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "login",         HandleLogin,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "logout",        HandleLogout,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "loginall",      HandleLoginAll,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "logoutall",     HandleLogoutAll,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "reload",        HandleReload,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "create",        HandleCreate,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "spawn",         HandleSpawn,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dist",          HandleDist,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "summonall",     HandleSummonAll,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "levelall",      HandleLevelAll,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "delete",        HandleDelete,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "level",         HandleLevel,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "reset",         HandleReset,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "money",         HandleMoney,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "goto",          HandleGoto,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "listrotations", HandleListRotations, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "resetstats",    HandleResetStats,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "population",    HandlePopulation,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "pipeline",      HandlePipeline,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "health",        HandleHealth,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "fleethealth",   HandleFleetHealth,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgstats",       HandleBgStats,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgcoord",       HandleBgCoord,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgzones",       HandleBgZones,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "pvecoord",      HandlePveCoord,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "group",         HandleGroup,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dungeontest",   HandleDungeonTest,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dungeonreset",  HandleDungeonReset,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dungeondump",   HandleDungeonDump,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bginfo",        HandleBgInfo,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "diag",          HandleDiag,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "wedges",        HandleWedges,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "reseed",        HandleReseed,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "smoketest",     HandleSmoketest,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "wipefleet",     HandleWipeFleet,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "roadstats",     HandleRoadStats,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "roadreset",     HandleRoadReset,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "pathcompare",   HandlePathCompare,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "terrainprofile",HandleTerrainProfile,rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "meta",          HandleMeta,          rbac::RBAC_PERM_COMMAND_GM, Console::No  },
            // Player-facing — gated to RBAC_PERM_JOIN_NORMAL_BG (the
            // "is a regular player" perm every account has by default)
            // so a normal user can summon their own alts without GM
            // rights. Account-match check inside the handler still
            // prevents cross-account abuse, and PlayerbotV2.MaxAltsAsBots
            // caps total simultaneously-owned alts per account.
            { "summon",        HandleSummon,        rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "self",          HandleSelf,          rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
        };
        static ChatCommandTable root =
        {
            { "playerbot", v2Table },
        };
        return root;
    }

    static bool HandleInspect(ChatHandler* handler, std::string const& target_name)
    {
        if (target_name.empty())
        {
            handler->SendSysMessage("Usage: .playerbot inspect <character_name>");
            return false;
        }

        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p)
        {
            handler->PSendSysMessage("Player '%s' not online.", target_name.c_str());
            return false;
        }

        const Playerbot::BotId id = p->GetGUID().GetCounter();
        const std::string report = Playerbot::Diagnostics::Inspect(id);
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // .playerbot terrainprofile <x1> <y1> <z1> <x2> <y2> <z2> <botname>
    // Samples the ACTUAL walkable collision/height surface (terrain .map + VMap
    // .vmo, exactly what Map::GetHeight sees) every 0.5y along the A->B segment,
    // to answer: is a navmesh GAP a walkable-but-unmeshed surface (VMap-walk can
    // cross it) vs a genuine void (needs an off-mesh jump) vs a wall (needs
    // routing)? The bot arg supplies the Map + PhaseShift (must be on the target
    // map). Per sample: surface Z, step delta, slope, and a wall LoS-raycast
    // between consecutive surface points. Verdict classifies the whole segment.
    static bool HandleTerrainProfile(ChatHandler* handler,
        float x1, float y1, float z1, float x2, float y2, float z2,
        std::string const& botname)
    {
        Player* p = ObjectAccessor::FindConnectedPlayerByName(botname);
        if (!p)
        {
            handler->PSendSysMessage("terrainprofile: bot '%s' not online (need a bot on the target map for its Map+phase).", botname.c_str());
            return false;
        }
        Map* m = p->GetMap();
        if (!m) { handler->SendSysMessage("terrainprofile: no map."); return false; }
        PhaseShift const& ph = p->GetPhaseShift();

        const float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
        const float hdist = std::sqrt(dx * dx + dy * dy);
        if (hdist < 0.1f) { handler->SendSysMessage("terrainprofile: segment too short."); return false; }

        // 0.5y sampling (capped so the SOAP reply stays bounded). Thresholds mirror
        // API::TryTerrainWalkFallback so the verdict matches what the crawler would do.
        constexpr float kSampleStep = 0.5f;
        const int steps = std::min(160, std::max(2, int(hdist / kSampleStep)));
        constexpr float kCliff = 3.0f;     // |dz| between 0.5y samples above this = cliff/drop (== kStepClimb)
        constexpr float kSteepDeg = 50.0f; // slope above this = steep (Recast walkable ~50-60 deg)
        constexpr float kEye = 2.0f;       // raycast height above surface for wall detection

        handler->PSendSysMessage("=== terrainprofile map=%u (%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f) hdist=%.1fy steps=%d ===",
            m->GetId(), x1, y1, z1, x2, y2, z2, hdist, steps);

        float prevSurf = z1, prevX = x1, prevY = y1;
        bool anyVoid = false, anyWall = false, anyCliff = false;
        float maxStep = 0.f, maxSlope = 0.f, minSurf = 1e9f, maxSurf = -1e9f;
        int firstVoid = -1, firstWall = -1, firstCliff = -1;

        for (int i = 0; i <= steps; ++i)
        {
            const float t = float(i) / float(steps);
            const float px = x1 + dx * t;
            const float py = y1 + dy * t;
            // Search from ~3y above the INTERPOLATED expected level (anchors to the
            // A->B surface line; robust for near-level gaps, won't drift through a void).
            const float refZ = (z1 + dz * t) + 3.0f;
            const float surf = m->GetHeight(ph, px, py, refZ, /*vmap*/ true, 50.0f);

            if (surf <= INVALID_HEIGHT)
            {
                anyVoid = true; if (firstVoid < 0) firstVoid = i;
                handler->PSendSysMessage("[%2d] h=%4.1f (%.1f,%.1f) surf=VOID", i, t * hdist, px, py);
                prevX = px; prevY = py; continue;
            }
            if (surf < minSurf) minSurf = surf;
            if (surf > maxSurf) maxSurf = surf;

            float dstep = 0.f, slopeDeg = 0.f;
            char const* flag = "";
            if (i > 0)
            {
                dstep = surf - prevSurf;
                const float seg = std::sqrt((px - prevX) * (px - prevX) + (py - prevY) * (py - prevY));
                slopeDeg = seg > 0.01f ? (std::atan2(std::fabs(dstep), seg) * 57.2958f) : 90.f;
                if (std::fabs(dstep) > maxStep) maxStep = std::fabs(dstep);
                if (slopeDeg > maxSlope) maxSlope = slopeDeg;
                if (std::fabs(dstep) > kCliff) { anyCliff = true; if (firstCliff < 0) firstCliff = i; flag = "CLIFF"; }
                else if (slopeDeg > kSteepDeg) flag = "steep";
                // Wall: is the straight line between the two surface points blocked?
                if (!m->isInLineOfSight(ph, prevX, prevY, prevSurf + kEye, px, py, surf + kEye,
                        LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::M2))
                { anyWall = true; if (firstWall < 0) firstWall = i; flag = "WALL"; }
            }
            handler->PSendSysMessage("[%2d] h=%4.1f (%.1f,%.1f) surf=%.2f dz=%+.2f slope=%2.0f %s",
                i, t * hdist, px, py, surf, dstep, slopeDeg, flag);
            prevSurf = surf; prevX = px; prevY = py;
        }

        char const* verdict;
        if (anyVoid)        verdict = "VOID (surface gap -> true void, needs off-mesh JUMP)";
        else if (anyWall)   verdict = "WALL (collision blocks -> needs ROUTING, VMap-walk cannot cross)";
        else if (anyCliff)  verdict = "CLIFF (>3y step -> drop/wall face, not a flat gap)";
        else if (maxSlope > kSteepDeg) verdict = "STEEP (surface present but slope >50deg -> borderline)";
        else                verdict = "CONTINUOUS WALKABLE (surface, no wall/void, gentle slope) -> VMap-walk VIABLE";
        handler->PSendSysMessage("=== VERDICT: %s ===", verdict);
        handler->PSendSysMessage("    surfZ=[%.2f..%.2f] maxStep=%.2f maxSlope=%.0f void@%d wall@%d cliff@%d",
            minSurf, maxSurf, maxStep, maxSlope, firstVoid, firstWall, firstCliff);
        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::SystemStatus();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleActivity(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetActivity();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleLevels(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetLevels();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleMaps(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetMaps();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleQuests(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetQuests();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleCrafting(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetCrafting();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleStuck(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetStuck();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleBg(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetBg();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleMark(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        Playerbot::Services::Lifecycle().mark_as_bot(id);
        // Bind ownership to the GM running the command when invoked
        // in-game (not from console). The whisper-command authority
        // model uses owner_account_id to decide who can issue squad
        // commands, so binding at mark-time gives the GM immediate
        // control without a separate adopt step. Console invocation
        // (no Player session) leaves the binding empty — useful for
        // batch-creating bots that an admin will assign later.
        if (Player const* gm = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr)
        {
            Playerbot::Services::Owners().SetOwner(
                id,
                handler->GetSession()->GetAccountId(),
                /*player_guid=*/ 0u);   // any char on the GM's account
            handler->PSendSysMessage("Owner: account %u (any char on it).",
                                     handler->GetSession()->GetAccountId());
            (void)gm;
        }
        // Already-online characters get the registry entry without a re-login,
        // so the AI ticks can start at the next world update. Offline characters
        // get the marker only — the next time someone logs them in (manually or
        // via .playerbot login), OnPlayerLogin will see is_bot()==true and
        // attach the AI.
        if (Player const* p = ObjectAccessor::FindConnectedPlayerByName(target_name))
        {
            if (!Playerbot::Services::Registry().has(id))
            {
                Playerbot::BotPersonality personality =
                    Playerbot::Services::Config().random_personality()
                        ? Playerbot::RandomPersonality(Playerbot::SeedForBot(id))
                        : Playerbot::DefaultPersonality();
                Playerbot::Services::Registry().register_bot(
                    id, personality,
                    Playerbot::BotRng{Playerbot::SeedForBot(id)});
                Playerbot::Services::Scheduler().register_bot(
                    id, Playerbot::ActivityTier::Idle);
            }
            handler->PSendSysMessage("Marked %s (online) as a V2 bot — AI active.",
                                     p->GetName().c_str());
        }
        else
        {
            handler->PSendSysMessage("Marked '%s' as a V2 bot. AI activates on next login.",
                                     target_name.c_str());
        }
        return true;
    }

    static bool HandleUnmark(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        Playerbot::Services::Lifecycle().unmark_as_bot(id);
        // If the character is online and currently registered, drop the
        // registry entry too so the AI stops driving them this session.
        if (Playerbot::Services::Registry().has(id))
        {
            Playerbot::Services::Scheduler().unregister_bot(id);
            Playerbot::Services::Snapshots().remove(id);
            Playerbot::Services::Registry().unregister_bot(id);
        }
        handler->PSendSysMessage("Unmarked '%s'. AI detached.", target_name.c_str());
        // Owner binding becomes meaningless without the bot — clear it so
        // a future re-mark of the same character starts clean.
        Playerbot::Services::Owners().ClearOwner(id);
        return true;
    }

    // Adopt / disown / owner — owner binding management. Decoupled from
    // mark/unmark so an admin can transfer ownership (e.g. between two
    // accounts on the same realm) without touching the bot's identity.

    static bool HandleAdopt(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        { handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
          return false; }
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        { handler->PSendSysMessage("'%s' is not a marked bot. Use .playerbot mark first.",
                                   target_name.c_str());
          return false; }
        // Console invocation has no session → can't determine an owner.
        // Tell the operator to adopt from in-game (or use a SQL UPDATE
        // for batch ownership transfer).
        if (!handler->GetSession())
        { handler->SendSysMessage("Adopt requires an in-game session (console can't own bots).");
          return false; }
        const uint32 account = handler->GetSession()->GetAccountId();
        Playerbot::Services::Owners().SetOwner(id, account, /*player_guid=*/ 0u);
        handler->PSendSysMessage("Owner of '%s' set to account %u.",
                                 target_name.c_str(), account);
        return true;
    }

    static bool HandleDisown(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        { handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
          return false; }
        Playerbot::Services::Owners().ClearOwner(id);
        handler->PSendSysMessage("Owner of '%s' cleared.", target_name.c_str());
        return true;
    }

    static bool HandleOwner(ChatHandler* handler, Optional<std::string> player_arg)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        // Default target: the GM running the command.
        uint32 account = 0;
        std::string label = "<unknown>";
        if (player_arg)
        {
            // Resolve the named player → account_id via DB. Online lookup
            // first (fast); fall back to characters → account.
            if (Player const* p = ObjectAccessor::FindConnectedPlayerByName(*player_arg))
            {
                account = p->GetSession()->GetAccountId();
                label = p->GetName();
            }
            else
            {
                CharacterCacheEntry const* cce =
                    sCharacterCache->GetCharacterCacheByName(*player_arg);
                if (!cce)
                { handler->PSendSysMessage("No character named '%s' exists.",
                                           player_arg->c_str());
                  return false; }
                account = cce->AccountId;
                label = cce->Name;
            }
        }
        else
        {
            if (!handler->GetSession())
            { handler->SendSysMessage("Specify a player name when using from console.");
              return false; }
            account = handler->GetSession()->GetAccountId();
            label = handler->GetSession()->GetPlayer()
                  ? handler->GetSession()->GetPlayer()->GetName()
                  : "console-self";
        }

        std::vector<Playerbot::BotId> const owned =
            Playerbot::Services::Owners().BotsOwnedBy(account);
        if (owned.empty())
        { handler->PSendSysMessage("'%s' (account %u) owns 0 bots.",
                                   label.c_str(), account);
          return true; }
        handler->PSendSysMessage("'%s' (account %u) owns %zu bot(s):",
                                 label.c_str(), account, owned.size());
        // Print up to 20 names to keep it readable; full list belongs in
        // a future paginated `.playerbot squad <player>` view.
        size_t shown = 0;
        for (Playerbot::BotId id : owned)
        {
            if (shown++ >= 20) { handler->PSendSysMessage("  ... (%zu more)",
                                                          owned.size() - 20);
                                  break; }
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            CharacterCacheEntry const* cce =
                sCharacterCache->GetCharacterCacheByGuid(g);
            handler->PSendSysMessage("  %s (id=%llu)",
                cce ? cce->Name.c_str() : "<unknown>",
                static_cast<unsigned long long>(id));
        }
        return true;
    }

    static bool HandleSquadSave(ChatHandler* handler, std::string const& preset_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        if (!handler->GetSession())
        { handler->SendSysMessage("Squad save: requires an in-game session.");
          return false; }
        if (preset_name.empty())
        { handler->SendSysMessage("Squad save: usage <preset_name>"); return false; }
        const uint32 account = handler->GetSession()->GetAccountId();
        const size_t n = Playerbot::Services::Owners().SaveSquadPreset(account, preset_name);
        if (n == 0)
        { handler->PSendSysMessage("Squad save: 0 bots — nothing to save.");
          return false; }
        handler->PSendSysMessage("Squad save: saved %zu bot(s) under preset '%s'.",
                                 n, preset_name.c_str());
        return true;
    }

    static bool HandleSquadLoad(ChatHandler* handler, std::string const& preset_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        if (!handler->GetSession())
        { handler->SendSysMessage("Squad load: requires an in-game session.");
          return false; }
        if (preset_name.empty())
        { handler->SendSysMessage("Squad load: usage <preset_name>"); return false; }
        const uint32 account = handler->GetSession()->GetAccountId();
        // 1) Write the preset into each owned bot's character row.
        const size_t n = Playerbot::Services::Owners().LoadSquadPreset(account, preset_name);
        if (n == 0)
        { handler->PSendSysMessage("Squad load: preset '%s' not found or applied to 0 bots.",
                                   preset_name.c_str());
          return false; }
        // 2) Push the now-current persisted state to every live bot's
        //    in-memory BotAI so the formation re-snaps without a relog.
        //    Re-emits FollowIntent if the bot is in active Follow mode
        //    so the new offset takes effect immediately.
        size_t live_applied = 0;
        for (Playerbot::BotId id : Playerbot::Services::Owners().BotsOwnedBy(account))
        {
            Playerbot::BotAI* ai = Playerbot::Services::Registry().ai(id);
            if (!ai) continue;
            const auto s = Playerbot::Services::Owners().LoadSquadState(id);
            ai->set_formation_type(static_cast<Playerbot::FormationType>(s.formation_type));
            ai->set_formation_slot(s.formation_slot);
            ai->set_follow_distance(s.follow_distance);
            ai->set_verbose_logging(s.owner_verbose);
            // Re-emit follow with new offset if the bot is currently
            // tracking someone — gives instant visual feedback.
            if (ai->owner_command() == Playerbot::BotAI::OwnerCommand::Follow &&
                !ai->owner_target().IsEmpty())
            {
                Playerbot::IntentQueue* q = Playerbot::Services::Registry().intents(id);
                Playerbot::IntentId* next = Playerbot::Services::Registry().next_intent_id(id);
                if (q && next)
                {
                    Playerbot::FormationOffset const off =
                        Playerbot::ComputeFormationOffset(
                            ai->formation_type(), ai->formation_slot(),
                            ai->follow_distance() > 0.f ? ai->follow_distance() : 5.0f);
                    Playerbot::BotIntentEmitter emit(q, id, /*source*/ 0, next);
                    emit.follow(ai->owner_target(), off.distance, off.angle_radians);
                }
            }
            ++live_applied;
        }
        handler->PSendSysMessage("Squad load: %zu bot(s) updated (%zu live re-snapped).",
                                 n, live_applied);
        return true;
    }

    static bool HandleCount(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const size_t marked     = Playerbot::Services::Lifecycle().size();
        const size_t registered = Playerbot::Services::Registry().size();
        // Walk marked ids and count how many resolve to a connected Player —
        // gives the actual "in-world right now" number, distinct from
        // registered (registered counts every bot whose AI exists, including
        // ones whose Player* may have despawned mid-tick).
        const auto ids = Playerbot::Services::Lifecycle().snapshot_ids();
        size_t online = 0;
        for (auto id : ids)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            if (ObjectAccessor::FindConnectedPlayer(g)) ++online;
        }
        // Pool stats: how many PBV2_NNNN accounts exist and how many slots
        // they're holding. Free slots = pool_size * 10 - total_chars (ceiling
        // of growable; AcquireSlot creates a new account when all are full).
        const size_t pool_acct = Playerbot::Services::Accounts().pool_size();
        const size_t pool_chrs = Playerbot::Services::Accounts().total_chars();
        const size_t cap       = pool_acct * Playerbot::V2::BotAccountMgr::MAX_CHARS_PER_ACCOUNT;
        const size_t free_now  = cap > pool_chrs ? cap - pool_chrs : 0;

        handler->PSendSysMessage("Bots: %zu marked / %zu registered / %zu in-world.",
                                 marked, registered, online);
        handler->PSendSysMessage("Pool: %zu account(s) / %zu chars / %zu free slot(s) before next account auto-create.",
                                 pool_acct, pool_chrs, free_now);
        return true;
    }

    static bool HandleList(ChatHandler* handler, Optional<uint32> page_arg)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        constexpr size_t kPageSize = 30;
        const size_t page = page_arg.value_or(0u);
        const auto ids = Playerbot::Services::Lifecycle().snapshot_ids();
        if (ids.empty())
        {
            handler->SendSysMessage("No marked bots in playerbot_v2_character.");
            return true;
        }
        // Class id → 3-letter tag for compact line formatting. Indexed by the
        // SharedDefines CLASS_* values 1..13 (rows 0/12 are empty/Monk gap).
        static char const* const kClassTag[] = {
            "---", "WAR", "PAL", "HUN", "ROG", "PRI", "DK ",
            "SHA", "MAG", "WLK", "MNK", "DRU", "DH ", "EVK"
        };
        const size_t first = page * kPageSize;
        if (first >= ids.size())
        {
            handler->PSendSysMessage("Page %zu out of range (have %zu page(s)).",
                                     page, (ids.size() + kPageSize - 1) / kPageSize);
            return true;
        }
        const size_t last = std::min(first + kPageSize, ids.size());
        handler->PSendSysMessage("Marked bots %zu-%zu of %zu (page %zu):",
                                 first + 1, last, ids.size(), page);
        for (size_t i = first; i < last; ++i)
        {
            const Playerbot::BotId id = ids[i];
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            CharacterCacheEntry const* ce = sCharacterCache->GetCharacterCacheByGuid(g);
            const bool online = ObjectAccessor::FindConnectedPlayer(g) != nullptr;
            const bool registered = Playerbot::Services::Registry().has(id);
            char const* state = online ? (registered ? "ON+AI" : "ON   ")
                                       : (registered ? "REG  " : "off  ");
            if (ce)
            {
                handler->PSendSysMessage(" #%llu  %s  L%-2u %s  %s",
                    static_cast<unsigned long long>(id),
                    state,
                    ce->Level,
                    kClassTag[ce->Class < 14 ? ce->Class : 0],
                    ce->Name.c_str());
            }
            else
            {
                // Cache miss usually means the row was deleted out from under us
                // — keep the line so the GM can spot orphans and unmark them.
                handler->PSendSysMessage(" #%llu  %s  <cache miss — orphan?>",
                    static_cast<unsigned long long>(id), state);
            }
        }
        if (last < ids.size())
            handler->PSendSysMessage("Use .playerbot list %zu for the next page.",
                                     page + 1);
        return true;
    }

    static bool HandleLogin(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        auto res = Playerbot::Services::SessionMgr().LoginBot(g);
        if (!res.ok)
        {
            handler->PSendSysMessage("Login refused: %s", res.reason.c_str());
            return false;
        }
        handler->PSendSysMessage("Spawn-login submitted for '%s' — bot enters world on next DB callback.",
                                 target_name.c_str());
        return true;
    }

    static bool HandleLogout(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        if (!Playerbot::Services::SessionMgr().LogoutBot(g))
        {
            handler->PSendSysMessage("No active V2 bot session for '%s'.", target_name.c_str());
            return false;
        }
        handler->PSendSysMessage("Logout requested for '%s'.", target_name.c_str());
        return true;
    }

    static bool HandleLoginAll(ChatHandler* handler, Optional<uint32> cap_arg)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        // Default cap: 100 simultaneous V2 bot sessions. Tunable per-call to
        // avoid flooding the character DB async pool with login holders. The
        // cap counts active sessions (in-flight + in-world); we stop submitting
        // when reached so a re-run picks up where we stopped.
        const uint32 cap = cap_arg.value_or(100u);
        auto r = Playerbot::Services::SessionMgr().LoginAll(cap);
        handler->PSendSysMessage("login_all: %u attempted / %u submitted / %u already in-world (cap %u).",
                                 r.attempted, r.succeeded, r.skipped_already_in_world, cap);
        return true;
    }

    static bool HandleLogoutAll(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const uint32 n = Playerbot::Services::SessionMgr().LogoutAll();
        handler->PSendSysMessage("logout_all: %u V2 bot session(s) kicked.", n);
        return true;
    }

    // .playerbot create [name] [race] [class] [gender]
    //
    // Creates a new bot character on a pool account ("PBV2_NNNN", up to 10
    // chars per account, auto-allocated by BotAccountMgr), marks it as V2,
    // and auto-submits a headless login.
    //
    // All args optional. Anything missing is filled by Blizzlike random
    // (BotComposition) — race/class weights from public WoW census, with
    // race/class restrictions enforced (DH=NE/BE only, Evoker=Dracthyr only).
    // Race/class accept names ("warrior","nightelf","be","dh") or numeric ids.
    static bool HandleCreate(ChatHandler* handler,
                             Optional<std::string> nameArg,
                             Optional<std::string> raceArg,
                             Optional<std::string> classArg,
                             Optional<uint32>      genderArg)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }

        // ownerSession is optional. BotCharacterFactory::Create accepts a null
        // session (it spins up a temporary BotSession owned by the bot-pool
        // account to own the new character, and defaults name-validation locale
        // to enUS). This lets `.playerbot create` run from a sessionless caller
        // — the SOAP/RA remote console or the server CLI — which is required for
        // headless fleet automation. Only the name locale differs from an
        // in-game GM invocation.
        WorldSession* sess = handler->GetSession();

        // Resolve race/class/gender/name. If any are missing or unparseable,
        // fall back to a Blizzlike Roll which respects race/class restrictions.
        // Explicit args win — Roll receives them as hints so the rolled
        // fields are constrained around them (e.g., specifying race=nightelf
        // rolls a class from NE's legal subset, not a random class that
        // might be invalid for NE).
        uint8 race   = raceArg   ? ParseRace(*raceArg)   : 0;
        uint8 cls    = classArg  ? ParseClass(*classArg) : 0;
        uint8 gender = uint8(genderArg.value_or(0xFFu));    // sentinel = unset
        std::string name = nameArg ? *nameArg : std::string{};

        const bool needsRoll = (race == 0) || (cls == 0) || (gender == 0xFF) || name.empty();
        if (needsRoll)
        {
            auto picked = Playerbot::V2::BotComposition::Roll(race, cls, gender, name);
            if (picked.race == 0)
            {
                handler->SendSysMessage("BotComposition::Roll failed — possibly invalid race/class hint. See log.");
                return false;
            }
            race = picked.race; cls = picked.cls; gender = picked.gender; name = picked.name;
        }

        auto r = Playerbot::V2::BotCharacterFactory::Create(sess, name, race, cls, gender);
        if (!r.ok)
        {
            handler->PSendSysMessage("create failed: %s", r.reason.c_str());
            return false;
        }
        handler->PSendSysMessage("Created '%s' (guid %s) — race %u class %u gender %u — marked as V2 bot.",
                                 name.c_str(), r.guid.ToString().c_str(), race, cls, gender);

        // Auto-login so the GM sees the bot in-world without a second command.
        auto login = Playerbot::Services::SessionMgr().LoginBot(r.guid);
        if (!login.ok)
        {
            handler->PSendSysMessage("Spawn-login refused: %s — use `.playerbot login %s` later.",
                                     login.reason.c_str(), name.c_str());
            return true;
        }
        handler->PSendSysMessage("Spawn-login submitted — bot enters world on next DB callback.");
        return true;
    }

    // .playerbot dist
    //
    // Print the configured Blizzlike distribution weights so operators can
    // see what to expect from `.playerbot create` (no args) and `.playerbot
    // spawn`. Read-only diagnostic; does not change anything.
    static bool HandleDist(ChatHandler* handler)
    {
        const std::string desc = Playerbot::V2::BotComposition::DescribeWeights();
        for (auto const& line : Trinity::Tokenize(desc, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // .playerbot summonall
    //
    // Teleport every in-world V2 bot to the GM's current position. Skips
    // offline bots (use `.playerbot loginall` first). Skips bots in BG/arena
    // because the cross-instance teleport requires the GM-summon machinery
    // and a partial implementation here would silently break BG queues.
    static bool HandleSummonAll(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        Player* gm = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!gm)
        {
            handler->SendSysMessage("summonall requires an in-game GM (need a Player to teleport to).");
            return false;
        }

        const uint32 dstMap = gm->GetMapId();
        const float  dx = gm->GetPositionX();
        const float  dy = gm->GetPositionY();
        const float  dz = gm->GetPositionZ();
        const float  do_ = gm->GetOrientation();

        uint32 summoned = 0, skipped = 0, offline = 0;
        for (Playerbot::BotId id : Playerbot::Services::Lifecycle().snapshot_ids())
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player* p = ObjectAccessor::FindConnectedPlayer(g);
            if (!p) { ++offline; continue; }
            if (p->GetMap() && p->GetMap()->IsBattlegroundOrArena()) { ++skipped; continue; }
            if (p->IsBeingTeleported()) { ++skipped; continue; }

            Playerbot::BotMovement::SafeTeleport(p, dstMap, dx, dy, dz, do_,
                                                 /*options*/ 0);
            ++summoned;
        }

        handler->PSendSysMessage("summonall: %u summoned / %u skipped (BG/teleporting) / %u offline.",
                                 summoned, skipped, offline);
        return true;
    }

    // .playerbot levelall <level>
    //
    // Apply .playerbot level to every in-world V2 bot. Same Player::GiveLevel
    // + InitTalentForLevel + SetXP(0) + apply_starter_talents pipeline as the
    // single-target command. Skips offline bots and bots in combat.
    static bool HandleLevelAll(ChatHandler* handler, uint32 level)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const uint32 maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        if (level < 1 || level > maxLevel)
        {
            handler->PSendSysMessage("level must be 1..%u.", maxLevel);
            return false;
        }

        uint32 leveled = 0, in_combat = 0, offline = 0, talents_applied = 0;
        for (Playerbot::BotId id : Playerbot::Services::Lifecycle().snapshot_ids())
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player* p = ObjectAccessor::FindConnectedPlayer(g);
            if (!p) { ++offline; continue; }
            if (p->IsInCombat()) { ++in_combat; continue; }

            p->GiveLevel(uint8(level));
            p->InitTalentForLevel();
            p->SetXP(0);
            ++leveled;

            if (level >= 10)
            {
                Playerbot::API api(p);
                if (api.apply_starter_talents() == Playerbot::Result::Ok)
                    ++talents_applied;
            }
        }

        handler->PSendSysMessage("levelall: %u leveled to %u (talents applied: %u) / %u in combat / %u offline.",
                                 leveled, level, talents_applied, in_combat, offline);
        return true;
    }

    // .playerbot spawn [count]
    //
    // Batch-create N fully-random Blizzlike bots in one shot. Each gets a
    // unique generated name, race, class, gender, on a pool account from
    // BotAccountMgr. Each is auto-logged-in. Default count: 5. Hard cap: 50
    // per invocation to avoid surprise mass-spawn.
    static bool HandleSpawn(ChatHandler* handler, Optional<uint32> countArg)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        // Sessionless callers (SOAP/RA console, server CLI) are allowed — the
        // factory owns each new character with a temporary bot-pool BotSession.
        // See HandleCreate for the rationale; enables headless mass-spawn.
        WorldSession* sess = handler->GetSession();

        constexpr uint32 kDefaultCount = 5;
        constexpr uint32 kHardCap      = 50;
        const uint32 requested = countArg.value_or(kDefaultCount);
        const uint32 count     = std::min(requested, kHardCap);

        uint32 created = 0, login_ok = 0, login_fail = 0;
        for (uint32 i = 0; i < count; ++i)
        {
            auto picked = Playerbot::V2::BotComposition::Roll();
            if (picked.race == 0) break;

            auto r = Playerbot::V2::BotCharacterFactory::Create(
                sess, picked.name, picked.race, picked.cls, picked.gender);
            if (!r.ok)
            {
                handler->PSendSysMessage("[%u/%u] create failed: %s", i+1, count, r.reason.c_str());
                continue;
            }
            ++created;
            auto login = Playerbot::Services::SessionMgr().LoginBot(r.guid);
            if (login.ok) ++login_ok; else ++login_fail;
        }

        handler->PSendSysMessage("spawn: created %u/%u bots; auto-login %u ok / %u failed%s.",
                                 created, count, login_ok, login_fail,
                                 (requested > kHardCap) ? " (clamped to 50)" : "");
        return true;
    }

    // .playerbot money <name> <copper>
    //
    // Set a bot's money to <copper> (1g = 10000c). Useful for testing the
    // vendor/auction/train/repair auto-rules which gate on bot.gold. Caps
    // at MAX_MONEY_AMOUNT (~10b copper, far above any realistic test value).
    static bool HandleMoney(ChatHandler* handler,
                            std::string const& target_name,
                            uint64 copper)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p)
        {
            handler->PSendSysMessage("'%s' is not in-world.", target_name.c_str());
            return false;
        }
        const Playerbot::BotId id = p->GetGUID().GetCounter();
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            handler->PSendSysMessage("'%s' is not marked as a V2 bot.", target_name.c_str());
            return false;
        }
        const uint64 capped = std::min<uint64>(copper, MAX_MONEY_AMOUNT);
        p->SetMoney(capped);
        handler->PSendSysMessage("'%s' money set to %llu copper (%llug %llus %lluc).",
                                 target_name.c_str(),
                                 static_cast<unsigned long long>(capped),
                                 static_cast<unsigned long long>(capped / 10000),
                                 static_cast<unsigned long long>((capped % 10000) / 100),
                                 static_cast<unsigned long long>(capped % 100));
        return true;
    }

    // .playerbot reset <name>
    //
    // Make a bot ready for the next test run: stop combat, interrupt any
    // cast, drop all auras, full HP/mana, clear cooldowns. Eliminates the
    // multi-step cycle of healing + buff-clearing + waiting for the combat
    // timer between iterative tests.
    static bool HandleReset(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p)
        {
            handler->PSendSysMessage("'%s' is not in-world.", target_name.c_str());
            return false;
        }
        const Playerbot::BotId id = p->GetGUID().GetCounter();
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            handler->PSendSysMessage("'%s' is not marked as a V2 bot — refusing to reset a "
                                     "non-bot character.", target_name.c_str());
            return false;
        }

        // Order matters: interrupt cast first so the spell-end packet doesn't
        // re-apply auras we're about to drop. CombatStop ends combat (clears
        // threat lists + drops PvP combat flag). RemoveAllAuras then strips
        // buffs/debuffs/HoTs/DoTs. SetFullHealth + SetPower restore resources.
        // GetSpellHistory()->ResetAllCooldowns flushes the cooldown map +
        // global cooldown — same primitive used by the /cdreset whisper.
        p->InterruptNonMeleeSpells(true);
        p->CombatStop();
        p->RemoveAllAuras();
        p->SetFullHealth();
        for (uint32 power = 0; power < MAX_POWERS; ++power)
            p->SetPower(Powers(power), p->GetMaxPower(Powers(power)));
        p->GetSpellHistory()->ResetAllCooldowns();

        handler->PSendSysMessage("Reset '%s' — combat dropped, auras cleared, HP/mana full, "
                                 "cooldowns flushed.", target_name.c_str());
        return true;
    }

    // .playerbot level <name> <level>
    //
    // Instantly level an in-world V2 bot via Player::GiveLevel. Bot must be
    // in-world (use `.playerbot login` first); level must be 1..MaxPlayerLevel.
    // GiveLevel handles stat/skill/talent/spell scaling in one shot — no
    // per-level loop needed. The packet it sends drops via BotSession::SendPacket.
    static bool HandleLevel(ChatHandler* handler,
                            std::string const& target_name,
                            uint32 level)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p)
        {
            handler->PSendSysMessage("'%s' is not in-world. Run `.playerbot login %s` first.",
                                     target_name.c_str(), target_name.c_str());
            return false;
        }
        const Playerbot::BotId id = p->GetGUID().GetCounter();
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            handler->PSendSysMessage("'%s' is not marked as a V2 bot — refusing to level a "
                                     "non-bot character via `.playerbot level`.", target_name.c_str());
            return false;
        }
        const uint32 maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        if (level < 1 || level > maxLevel)
        {
            handler->PSendSysMessage("level must be 1..%u (configured MaxPlayerLevel).", maxLevel);
            return false;
        }
        const uint8 oldLevel = p->GetLevel();
        p->GiveLevel(uint8(level));
        p->InitTalentForLevel();           // refresh talent slots after the jump
        p->SetXP(0);                        // reset progress within the new level

        // Auto-apply the spec's starter talent build so a freshly-leveled
        // bot has actual abilities, not blank talent slots. Skipped during
        // combat (apply_starter_talents returns Locked then) and below the
        // first talent unlock level (10).
        const char* talents_status = "skipped (combat or <lvl 10)";
        if (level >= 10 && !p->IsInCombat())
        {
            Playerbot::API api(p);
            const auto r = api.apply_starter_talents();
            talents_status = r == Playerbot::Result::Ok ? "applied" :
                             r == Playerbot::Result::Locked ? "locked (combat)" :
                             r == Playerbot::Result::NotKnown ? "no active trait config" :
                             "failed";
        }

        handler->PSendSysMessage("'%s' leveled %u → %u (max %u, talents: %s).",
                                 target_name.c_str(), oldLevel, level, maxLevel, talents_status);
        return true;
    }

    // .playerbot delete <name>
    //
    // Cleanup for test bots: refuses if the bot is in-world (use
    // `.playerbot logout` first), then unmarks, deletes the character row,
    // and removes the character cache entry. Hard-delete (deleteFinally
    // = true), so the row does not enter the soft-delete pool.
    static bool HandleDelete(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            handler->PSendSysMessage("'%s' is not marked as a V2 bot — refusing to delete a "
                                     "non-bot character via `.playerbot delete`. Use core "
                                     "`.character delete` if you really mean it.", target_name.c_str());
            return false;
        }
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        if (ObjectAccessor::FindConnectedPlayer(g))
        {
            handler->PSendSysMessage("'%s' is currently in-world. Run `.playerbot logout %s` "
                                     "first, then re-run delete.", target_name.c_str(), target_name.c_str());
            return false;
        }

        // Unregister AI + unmark before deleting the character row so the
        // registry has no dangling reference if delete fails midway.
        if (Playerbot::Services::Registry().has(id))
        {
            Playerbot::Services::Scheduler().unregister_bot(id);
            Playerbot::Services::Snapshots().remove(id);
            Playerbot::Services::Registry().unregister_bot(id);
        }
        Playerbot::Services::Lifecycle().unmark_as_bot(id);

        // Resolve account id BEFORE deletion (cache lookup goes stale after).
        const uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(g);

        // Hard delete: deleteFinally=true forces CHAR_DELETE_REMOVE regardless
        // of CONFIG_CHARDELETE_METHOD, so test bots don't pollute the soft-
        // delete pool. updateRealmChars=true keeps realm character counts
        // accurate so .playerbot create's CHARACTERS_PER_ACCOUNT cap stays sane.
        Player::DeleteFromDB(g, accountId, /*updateRealmChars*/true, /*deleteFinally*/true);

        // Free a slot in the bot account pool so a future .playerbot create
        // can land here. No-op if accountId isn't a pool account (e.g.,
        // legacy bots created on the GM's account before the pool existed).
        Playerbot::Services::Accounts().note_character_removed(accountId);

        handler->PSendSysMessage("Deleted '%s' (guid %s) — unmarked + character row removed.",
                                 target_name.c_str(), g.ToString().c_str());
        return true;
    }

    static bool HandleReload(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        // ConfigReader::reload re-reads playerbot.conf via sConfigMgr. HR-yes
        // keys take effect immediately on next read; HR-no keys are cached
        // by the subsystems that consumed them at init and won't update
        // until a worldserver restart. See playerbot.conf.dist for the per-
        // key HR tag.
        const bool ok = Playerbot::Services::Config().reload();
        handler->PSendSysMessage("playerbot.conf reload: %s", ok ? "ok" : "failed (see server log)");
        return true;
    }

    // .playerbot goto <name> <map> <x> <y> — set a manual cross-map travel
    // goal on a bot. Drives the SAME pipeline as the owner /goto whisper:
    // the builder synthesizes a relocation objective and the travel rules
    // route via walk/taxi/portal/ship/areatrigger — never a teleport.
    // Console-friendly so journeys are testable via SOAP.
    static bool HandleGoto(ChatHandler* handler, std::string const& target_name,
                           uint32 map_id, float x, float y)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        Playerbot::BotAI* ai = Playerbot::Services::Registry().ai(id);
        if (!ai)
        {
            handler->PSendSysMessage("'%s' is not an active V2 bot session.", target_name.c_str());
            return false;
        }
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        ai->set_manual_travel(map_id, x, y, bot ? bot->GetPositionZ() : 0.f);
        handler->PSendSysMessage("'%s': travel goal set to map %u (%.0f, %.0f) — "
                                 "journey via flights/portals/ships.",
                                 target_name.c_str(), map_id, x, y);
        return true;
    }

    static bool HandleListRotations(ChatHandler* handler)
    {
        auto rotations = Playerbot::Combat::ListRotations();
        handler->PSendSysMessage("Registered rotations (%zu):", rotations.size());
        for (auto const& r : rotations)
            handler->PSendSysMessage("  cls=%u spec=%u rules=%zu", r.cls, r.spec, r.rules);
        return true;
    }

    static bool HandleResetStats(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return true;
        }
        Playerbot::Services::Perf().reset();
        handler->SendSysMessage("PlayerbotV2 perf counters reset.");
        return true;
    }

    static bool HandlePopulation(ChatHandler* handler, Optional<std::string> sub)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return true; }

        auto& pop = Playerbot::Services::Population();

        if (sub && (*sub == "force" || *sub == "rebalance"))
        {
            pop.ForceReconcile();
            handler->SendSysMessage("Population: forced reconciliation triggered.");
            return true;
        }

        auto snap = pop.Snapshot();
        handler->PSendSysMessage("Population: target=%u actual=%u", snap.total_target, snap.total_actual);
        handler->SendSysMessage("Bracket  | A_tgt A_cur | H_tgt H_cur | Delta");
        for (auto const& b : snap.buckets)
        {
            int32 a_delta = int32(b.alliance_target) - int32(b.alliance_actual);
            int32 h_delta = int32(b.horde_target) - int32(b.horde_actual);
            handler->PSendSysMessage("L%u-%u  |  %3u   %3u  |  %3u   %3u  | A%+d H%+d",
                uint32(b.level_lo), uint32(b.level_hi),
                b.alliance_target, b.alliance_actual,
                b.horde_target, b.horde_actual,
                a_delta, h_delta);
        }
        return true;
    }

    static bool HandlePipeline(ChatHandler* handler, std::string const& target_name)
    {
        if (target_name.empty())
        { handler->SendSysMessage("Usage: .playerbot pipeline <character_name>"); return false; }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p) { handler->PSendSysMessage("'%s' not in-world.", target_name.c_str()); return false; }
        Playerbot::V2::Fleet::BotSetupPipeline pipe;
        pipe.Reset(p);
        handler->PSendSysMessage("Pipeline state for '%s' reset; will re-run on next population tick.",
                                  target_name.c_str());
        return true;
    }

    static bool HandleHealth(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::HealthReport();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleFleetHealth(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetHealthOneScreen();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleBgStats(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::BgOutcomeReport();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleBgInfo(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::BgInfoReport();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // #1B: `.playerbot wedges` — print the runtime wedge watchdog's current
    // active-wedge list grouped by root-cause category plus per-category
    // lifetime episode totals. Lets the operator see which bots are stuck
    // (and why) without tailing the log or noticing a frozen bot by eye.
    static bool HandleWedges(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::WedgesReport();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // Control-zone diagnostic: `.playerbot bgzones <online-character>` lists
    // every CONTROL_ZONE gameobject on that character's map (EotS towers,
    // EPL-style proximity points) with its live capture-bar value,
    // controlling team, and a per-team census of players inside the capture
    // radius — applying the SAME eligibility tests the core ControlZone
    // heartbeat uses (alive + IsOutdoorPvPActive). Built 2026-06-11 to
    // root-cause "EotS towers never flip despite a team standing on them".
    static bool HandleBgZones(ChatHandler* handler, std::string const& target_name)
    {
        if (target_name.empty())
        {
            handler->SendSysMessage("Usage: .playerbot bgzones <online_character>");
            return true;
        }
        Player* anchor = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!anchor)
        {
            handler->PSendSysMessage("Player '{}' not online.", target_name);
            return true;
        }
        Map* map = anchor->GetMap();

        ControlZoneCollector worker;
        TypeContainerVisitor<ControlZoneCollector, MapStoredObjectTypesContainer> visitor(worker);
        visitor.Visit(map->GetObjectsStore());

        // NOTE: this branch's ChatHandler::PSendSysMessage is printf-style;
        // build lines with fmt::format and send them whole (first version
        // used {} placeholders through PSendSysMessage and printed literal
        // braces with every argument dropped).
        handler->SendSysMessage(fmt::format("Control zones on map {} (instance {}): {}",
            map->GetId(), map->GetInstanceId(), worker.zones.size()).c_str());
        for (GameObject* go : worker.zones)
        {
            float const radius = float(go->GetGOInfo()->controlZone.radius);
            uint32 inRadius = 0, eligA = 0, eligH = 0, deadIn = 0, noPvp = 0, wrongPhase = 0;
            std::string phase_example;
            for (auto const& ref : map->GetPlayers())
            {
                Player* p = ref.GetSource();
                if (!p || !go->IsWithinDist(p, radius)) continue;
                ++inRadius;
                // Phase gate FIRST — the core's PlayerListSearcher filters by
                // the GO's PhaseShift before any eligibility test, so a
                // phase-mismatched player is INVISIBLE to the capture
                // heartbeat no matter how eligible. Live evidence 2026-06-11
                // (EotS inst 6): 18 horde inside BE Tower, bar frozen at
                // 50.0 — phase mismatch is the prime suspect (Chromie-time
                // phasing from the leveling fleet?).
                if (!p->InSamePhase(go))
                {
                    ++wrongPhase;
                    if (phase_example.empty())
                        phase_example = p->GetName();
                    continue;
                }
                if (!p->IsAlive())            { ++deadIn; continue; }
                if (!p->IsOutdoorPvPActive()) { ++noPvp;  continue; }
                if (p->GetTeamId() == TEAM_ALLIANCE) ++eligA; else ++eligH;
            }
            handler->SendSysMessage(fmt::format(
                "  go={} '{}' pos=({:.0f},{:.0f},{:.0f}) r={:.0f} value={:.1f} team={} | "
                "inside={} eligA={} eligH={} dead={} noPvp={} wrongPhase={}{} ticking={}",
                go->GetEntry(), go->GetGOInfo()->name,
                go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(),
                radius, go->GetControlZoneValue(),
                int32(go->GetControllingTeam()),
                inRadius, eligA, eligH, deadIn, noPvp, wrongPhase,
                phase_example.empty() ? std::string{}
                                      : fmt::format(" (e.g. {})", phase_example),
                go->HasFlag(GO_FLAG_NOT_SELECTABLE) ? 0 : 1).c_str());
        }
        return true;
    }

    // Team-coordinator plan dump (BG audit N60): one line per active
    // (BG instance, team) with the current order distribution.
    static bool HandleBgCoord(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 services not initialized.");
            return true;
        }
        const std::string report =
            Playerbot::Services::BgCoordinator().DebugDump();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // Form a party from named online players (bots or not):
    //   .playerbot group <leader> <member> [member...]
    // Testability surface for the dungeon/raid coordinator and group
    // behavior work — TC's own `.group join` can only ADD to an existing
    // group, so there was no console path to create one from scratch.
    // Mirrors LFGMgr's direct group creation (Group::Create + AddMember,
    // no invite round-trip — headless bot sessions can't click invites
    // from the console anyway).
    static bool HandleGroup(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            handler->SendSysMessage(
                "Usage: .playerbot group <leader> <member> [member...]");
            return true;
        }
        std::vector<std::string> names;
        for (auto const& tok : Trinity::Tokenize(args, ' ', false))
            if (!tok.empty())
                names.emplace_back(tok);
        if (names.size() < 2)
        {
            handler->SendSysMessage(
                "Usage: .playerbot group <leader> <member> [member...]");
            return true;
        }
        // Pure-bot groups never block formation: the BotGroupBuilder /
        // social-invite AI constantly re-groups idle bots, so any test
        // group has to be able to YANK its members out of those. Groups
        // containing a human are left untouched (skip with a message).
        auto detach_if_bot_group = [&](Player* p) -> bool
        {
            Group* g = p->GetGroup();
            if (!g) return true;
            for (auto const& slot : g->GetMemberSlots())
                if (!Playerbot::Services::Lifecycle().is_bot(
                        slot.guid.GetCounter()))
                    return false;   // human group — hands off
            g->RemoveMember(p->GetGUID());
            return p->GetGroup() == nullptr;
        };
        Player* leader = ObjectAccessor::FindPlayerByName(names[0]);
        if (!leader)
        {
            handler->PSendSysMessage("Leader '%s' is not online.",
                                     names[0].c_str());
            return true;
        }
        Group* grp = leader->GetGroup();
        if (grp && !detach_if_bot_group(leader))
        {
            handler->PSendSysMessage(
                "Leader '%s' is in a group with humans — not touching it.",
                names[0].c_str());
            return true;
        }
        grp = leader->GetGroup();
        if (!grp)
        {
            grp = new Group();
            if (!grp->Create(leader))
            {
                delete grp;
                handler->PSendSysMessage("Group creation failed for '%s'.",
                                         names[0].c_str());
                return true;
            }
            sGroupMgr->AddGroup(grp);
        }
        uint32 added = 0;
        for (size_t i = 1; i < names.size(); ++i)
        {
            Player* m = ObjectAccessor::FindPlayerByName(names[i]);
            if (!m)
            { handler->PSendSysMessage("'%s' is not online — skipped.", names[i].c_str()); continue; }
            if (m->GetGroup() == grp)
            { ++added; continue; }
            if (m->GetGroup() && !detach_if_bot_group(m))
            { handler->PSendSysMessage("'%s' is grouped with humans — skipped.", names[i].c_str()); continue; }
            if (grp->IsFull() && !grp->isRaidGroup())
                grp->ConvertToRaid();
            if (grp->AddMember(m))
                ++added;
            else
                handler->PSendSysMessage("AddMember failed for '%s'.", names[i].c_str());
        }
        grp->BroadcastGroupUpdate();
        handler->PSendSysMessage("Group: leader %s + %u member(s).",
                                 leader->GetName().c_str(), added);
        return true;
    }

    // .playerbot dungeontest <lfg_dungeon_id> [level]
    //
    // Stands up (or refreshes) a FIXED pure-bot dungeon squad — 1 tank
    // (Prot Warrior), 1 healer (Holy Priest), 3 DPS (Frost Mage / Sin
    // Rogue / BM Hunter), all Alliance Humans — then LFG-queues all five
    // for the named dungeon so the dungeon-finder forms the group, ports
    // them in, and the bots auto-run it (is_lfg auto-activation). This is
    // the headless equivalent of a player queuing a 5-stack, and the only
    // way to verify dungeon play without a human to whisper /lfg.
    //
    // Idempotent + self-converging: members that aren't in-world yet are
    // created/logged-in this call (report says "preparing"); re-run once
    // they're online to spec/gear/level + queue. The target level is
    // clamped into the dungeon's ContentTuning bracket so the LFG level
    // gate (LFGMgr::GetLockedDungeons) can't reject the queue.
    // .playerbot dungeonreset — force the dungeontest squad out of any
    // instance back to the Alliance staging area and clear their LFG state,
    // so the next `dungeontest` re-queues a fresh run. The run-protection
    // guard in dungeontest (correctly) refuses to disturb a live run; this is
    // the explicit "tear the current run down" escape hatch for the harness —
    // e.g. after a server restart reloaded the squad mid-run (their saved
    // position is inside the instance, but last_lfg_dungeon_id resets on
    // relogin so they can no longer auto-run), or simply to retry from
    // scratch. This is a deliberate test reset, NOT a stuck-bot rescue.
    // Dump every registered DungeonScript as a machine-readable line for the
    // offline route generator (tools/gen_dungeon_routes.py). One line per script:
    //   DDUMP|<map_id>|<lfg_dungeonId>|<name>|<boss1,boss2,...>
    // map_id + bosses come from the script (get_advice with an empty snapshot —
    // the same safe static-extraction the dungeon smoketest uses); lfg_dungeonId
    // is the first LFGDungeons.db2 row on that map (used to look up the LFG
    // entrance in wc_world.lfg_dungeon_template). 0 => no LFG entry (generator
    // falls back / skips). Read-only.
    static bool HandleDungeonDump(ChatHandler* handler)
    {
        using namespace Playerbot;
        if (!Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        auto& mgr = Services::Dungeons();
        uint32 count = 0;
        mgr.for_each_script([&](DungeonScript const& script)
        {
            BotSnapshot empty_snap{};
            BotSnapshotView empty_view(empty_snap);
            DungeonAdvice a = script.get_advice(empty_view);

            uint32 lfg_id = 0;
            for (LFGDungeonsEntry const* d : sLFGDungeonsStore)
                if (d && uint32(d->MapID) == script.map_id())
                { lfg_id = d->ID; break; }

            std::string bosses;
            for (uint32 b : a.bosses)
            {
                if (!bosses.empty()) bosses += ",";
                bosses += std::to_string(b);
            }
            handler->PSendSysMessage("DDUMP|%u|%u|%s|%s",
                script.map_id(), lfg_id, script.name(), bosses.c_str());
            ++count;
        });
        handler->PSendSysMessage("DDUMP_END|%u scripts", count);
        return true;
    }

    static bool HandleDungeonReset(ChatHandler* handler)
    {
        using namespace Playerbot;
        if (!Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        static char const* const kNames[5] =
            { "Dungtank", "Dunghealer", "Dungmage", "Dungrogue", "Dunghunter" };
        static constexpr float kSX = -8985.0f, kSY = 511.0f, kSZ = 79.8f;
        uint32 moved = 0, cleared = 0;
        std::string status;
        for (char const* nm : kNames)
        {
            BotId id = ResolveCharacterId(nm);
            if (!id) { status += fmt::format("{}=absent ", nm); continue; }
            ObjectGuid og = ObjectGuid::Create<HighGuid::Player>(id);
            // Clear LFG state regardless of online status so the next
            // solo-queue starts from a clean slate (mirrors the LeaveLfg in
            // the dungeontest queue loop).
            sLFGMgr->LeaveLfg(og);
            ++cleared;
            Player* p = ObjectAccessor::FindConnectedPlayer(og);
            if (!p) { status += fmt::format("{}=offline ", nm); continue; }
            // Drop any (LFG or pure-bot) group so the re-queue is solo + clean.
            if (Group* grp = p->GetGroup())
                grp->RemoveMember(og);
            p->CombatStop(true);
            p->TeleportTo(0, kSX, kSY, kSZ, 0.0f);
            ++moved;
            status += fmt::format("{}=staged ", nm);
        }
        handler->PSendSysMessage(
            "dungeonreset: %u LFG-cleared, %u staged to (%.0f,%.0f). [%s]",
            cleared, moved, kSX, kSY, status.c_str());
        return true;
    }

    static bool HandleDungeonTest(ChatHandler* handler, uint32 dungeonId,
                                  Optional<uint32> levelArg)
    {
        using namespace Playerbot;
        if (!Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        LFGDungeonsEntry const* dun = sLFGDungeonsStore.LookupEntry(dungeonId);
        if (!dun)
        { handler->PSendSysMessage("dungeontest: no LFGDungeons row %u.", dungeonId); return false; }

        // Resolve the dungeon's level bracket from ContentTuning (12.0 has
        // no MinLevel/MaxLevel on LFGDungeons — it scales via ContentTuning,
        // the exact source LFGMgr's level gate reads). Pass an empty redirect
        // mask (Chromie CtrOptions is a stub on this server anyway).
        int16 minL = 1, maxL = uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
        if (Optional<ContentTuningLevels> lv =
                sDB2Manager.GetContentTuningData(dun->ContentTuningID, {}))
        { if (lv->MaxLevel > 0) { minL = lv->MinLevel; maxL = lv->MaxLevel; } }
        uint32 level = levelArg ? *levelArg : uint32(maxL);
        if (level < uint32(minL)) level = uint32(minL);
        if (level > uint32(maxL)) level = uint32(maxL);

        struct Spec { char const* name; uint8 cls; uint32 spec; Role role; };
        // Race 1 = Human (Alliance); all five classes are legal for Human in
        // 12.0. Spec ids are ChrSpecialization.db2 (Prot 73 / Holy 257 /
        // Frost 64 / Assassination 259 / Beast Mastery 253).
        static const Spec kSquad[5] = {
            { "Dungtank",   CLASS_WARRIOR, 73u,  Role::Tank   },
            { "Dunghealer", CLASS_PRIEST,  257u, Role::Healer },
            { "Dungmage",   CLASS_MAGE,    64u,  Role::Dps    },
            { "Dungrogue",  CLASS_ROGUE,   259u, Role::Dps    },
            { "Dunghunter", CLASS_HUNTER,  253u, Role::Dps    },
        };

        // --- Run protection (idempotency) ---------------------------------
        // If the squad is already inside an active LFG run for THIS dungeon,
        // re-invoking the command (the harness re-runs it to monitor progress)
        // must NOT disturb it. The per-member position-reset below ejects any
        // bot found on a dungeon map to the staging area — which, on a squad
        // that had JUST been ported in, tore the live run apart and desynced
        // LFG (the group state stayed "In dungeon" while the bodies were yanked
        // back to map 0, so the run never advanced). Detect the live run by LFG
        // state (LFG_STATE_DUNGEON) on the dungeon's own map and short-circuit:
        // report and leave the run alone. Only when the run has genuinely
        // collapsed (the instance is empty of the squad) do we fall through to
        // re-stage + re-queue — and LeaveLfg() below clears any stale state.
        {
            uint32 inRun = 0;
            std::string rs;
            for (Spec const& m : kSquad)
            {
                BotId rid = ResolveCharacterId(m.name);
                if (!rid) continue;
                ObjectGuid rog = ObjectGuid::Create<HighGuid::Player>(rid);
                Player* rp = ObjectAccessor::FindConnectedPlayer(rog);
                if (!rp) continue;
                if (sLFGMgr->GetState(rog) == lfg::LFG_STATE_DUNGEON &&
                    rp->GetMap() && rp->GetMap()->IsDungeon() &&
                    int16(rp->GetMapId()) == dun->MapID)
                {
                    ++inRun;
                    rs += fmt::format("{}=running ", m.name);
                }
            }
            // tank + healer + at least one dps still inside ⇒ treat as a live
            // run and refuse to touch it (re-run is a no-op while running).
            if (inRun >= 3)
            {
                handler->PSendSysMessage(
                    "dungeontest '%s' (id %u): squad already in an ACTIVE run — "
                    "%u/5 in the dungeon. Not disturbing. [%s]",
                    dun->Name.Str[sWorld->GetDefaultDbcLocale()], dungeonId,
                    inRun, rs.c_str());
                return true;
            }
        }

        // Detach a bot from any pure-bot group so it can solo-queue LFG (the
        // social-invite AI constantly re-groups idle bots). Humans untouched.
        auto detach_if_bot_group = [&](Player* p)
        {
            Group* g = p->GetGroup();
            if (!g) return;
            for (auto const& slot : g->GetMemberSlots())
                if (!Services::Lifecycle().is_bot(slot.guid.GetCounter()))
                    return;
            g->RemoveMember(p->GetGUID());
        };

        std::vector<Player*> ready;
        uint32 preparing = 0, created = 0;
        std::string status;
        for (Spec const& m : kSquad)
        {
            BotId id = ResolveCharacterId(m.name);
            bool justCreated = false;
            if (!id)
            {
                auto r = V2::BotCharacterFactory::Create(
                    handler->GetSession(), m.name, /*race Human*/ 1, m.cls, /*gender*/ 0);
                if (!r.ok)
                { status += fmt::format("{}=createfail({}) ", m.name, r.reason); continue; }
                id = r.guid.GetCounter();
                ++created;
                justCreated = true;
            }
            Services::Lifecycle().mark_as_bot(id);
            // Shield the squad from the population overflow-kick: at a low
            // TotalTarget these dedicated test bots are pool inventory and get
            // logged out the instant online > target — which made them flicker
            // offline mid-setup and never reach the LFG queue. A rolling 10-min
            // kick-protection lease (refreshed every command call) takes them
            // off the shaper's books until they're LFG-queued (the queue state
            // is itself a kick exemption). Same lease BotQueueFiller gives JIT
            // BG/LFG match-fill spawns.
            Services::Population().ProtectFromKick(uint64(id), 10u * 60u * 1000u);

            // Do NOT log a just-created char in on the same call: the character
            // row isn't DB-committed yet, so BeginLogin loads nothing and the
            // BotSession sits in sessions_ forever as an "in flight" zombie that
            // never enters world. Defer login to the next call (row committed by
            // then). For an existing-but-offline member: if a (possibly zombie)
            // session is already in flight, clear it first — a fresh login on a
            // committed row reliably lands the bot in-world; otherwise submit one.
            ObjectGuid og = ObjectGuid::Create<HighGuid::Player>(id);
            Player* p = ObjectAccessor::FindConnectedPlayer(og);
            if (!p)
            {
                if (justCreated)
                { ++preparing; status += fmt::format("{}=created ", m.name); continue; }
                if (Services::SessionMgr().IsHeadless(og))
                {
                    Services::SessionMgr().LogoutBot(og);   // drop stale in-flight; relogin next call
                    status += fmt::format("{}=reset ", m.name);
                }
                else
                {
                    Services::SessionMgr().LoginBot(og);    // fresh submit on a committed row
                    status += fmt::format("{}=login ", m.name);
                }
                ++preparing;
                continue;
            }
            // Position reset: runs before the alive/combat gates so that bots
            // stranded inside a dungeon or BG from a prior run are staged out
            // first (mobs would re-aggro instantly and keep them in incombat
            // indefinitely if we tried to queue them from inside the instance).
            static constexpr float kSX = -8985.0f, kSY = 511.0f, kSZ = 79.8f;
            if (p->GetMap()->IsDungeon() || p->GetMap()->IsBattleground())
            {
                // Stranded in old instance — eject to the Alliance staging area.
                p->CombatStop(true);
                p->TeleportTo(0, kSX, kSY, kSZ, 0.0f);
                ++preparing;
                status += fmt::format("{}=staged ", m.name);
                continue;
            }
            // Overworld bad state: in water, in combat, or underground/at-sea-level.
            // Teleport is skipped when already near staging to avoid loop, but
            // CombatStop is ALWAYS called when in combat — stale refs from a prior
            // dungeon run can persist after the staging teleport and must be cleared.
            {
                float px, py, pz;
                p->GetPosition(px, py, pz);
                const float sdx = px - kSX, sdy = py - kSY;
                const bool nearStaging = sdx * sdx + sdy * sdy < 500.0f * 500.0f;
                if (p->IsInWater() || p->IsInCombat() || pz < 5.0f)
                {
                    p->CombatStop(true);
                    if (!nearStaging)
                    {
                        p->TeleportTo(0, kSX, kSY, kSZ, 0.0f);
                        ++preparing;
                        status += fmt::format("{}=staged ", m.name);
                        continue;
                    }
                    // Near staging but still in combat (stale refs) — CombatStop
                    // was called above; re-check after a tick via another invocation.
                    if (p->IsInCombat())
                    { ++preparing; status += fmt::format("{}=incombat ", m.name); continue; }
                }
            }
            // Alive-gate: a dead/ghost member can't LFG-queue (the API rejects
            // it), so the finder forms an incomplete group and the proposal
            // lapses — the squad reverts to open-world questing and the run
            // never starts. Revive any downed member in place (these are
            // dedicated test bots; a real player would corpse-run, but for the
            // harness an instant rez is correct) and don't count it ready until
            // it is actually alive again.
            if (!p->IsAlive())
            {
                p->ResurrectPlayer(1.0f);
                p->SpawnCorpseBones();
                ++preparing;
                status += fmt::format("{}=revive ", m.name);
                continue;
            }

            // Setup: level + gear via the standard pipeline, then force the
            // role spec LAST so the bot actually tanks/heals (role inference
            // reads GetPrimarySpecialization). RunFor is incremental and
            // persists its own per-bot state — the pop manager calls it ONCE
            // per tick. We mirror that: exactly one step per command invocation
            // (this command is idempotent/re-run, so setup converges across
            // calls). A tight RunFor loop here ran the full gear+travel pipeline
            // for 5 bots synchronously on the world thread in a single tick and
            // contributed to a world-thread-hang crash — never loop it. No
            // Reset(): that would wipe progress every call and never finish.
            // Force exact level match before running the setup pipeline: if the
            // bot leveled past the target (e.g. gained XP fighting mobs while
            // ejected in the overworld), GiveLevel it back down. The LFG
            // eligibility gate rejects anyone above the dungeon bracket max.
            if (uint32(p->GetLevel()) != level)
                p->GiveLevel(uint8(level));
            const bool setupDone = V2::Fleet::BotSetupPipeline{}.RunFor(p, uint8(level));
            if (!setupDone || uint32(p->GetLevel()) != level)
            { ++preparing; status += fmt::format("{}=setup ", m.name); continue; }
            if (ChrSpecializationEntry const* se = sChrSpecializationStore.LookupEntry(m.spec))
                if (se->ClassID == p->GetClass() &&
                    uint32(p->GetPrimarySpecialization()) != m.spec)
                {
                    p->ActivateTalentGroup(se);
                    API(p).apply_starter_talents();
                }
            ready.push_back(p);
        }

        if (ready.size() < 5)
        {
            handler->PSendSysMessage(
                "dungeontest '%s' (id %u, lvl %u, bracket %d-%d): %u/5 ready, "
                "%u created, %u preparing. Re-run in a few seconds to queue. [%s]",
                dun->Name.Str[sWorld->GetDefaultDbcLocale()], dungeonId, level,
                int(minL), int(maxL), uint32(ready.size()), created, preparing,
                status.c_str());
            return true;
        }

        // All five geared/specced + in-world → solo-queue each for the
        // dungeon with its role. The finder matches the 5, sends a proposal
        // (bots auto-accept, idle:lfg_proposal_accept), ports them in, and
        // dungeon-run auto-activates for the whole is_lfg group.
        uint32 queued = 0;
        for (size_t i = 0; i < 5; ++i)
        {
            Player* p = ready[i];
            BotId id = p->GetGUID().GetCounter();
            detach_if_bot_group(p);
            // Clear any stale LFG state from a prior run (e.g. PROPOSAL or
            // DUNGEON that persisted across a server restart). Without this,
            // lfg_queue() silently rejects the bot if it's already in a
            // terminal-but-stale LFG state.
            sLFGMgr->LeaveLfg(p->GetGUID());
            IntentQueue* q = Services::Registry().intents(id);
            IntentId* next = Services::Registry().next_intent_id(id);
            if (!q || !next) continue;
            BotIntentEmitter emit(q, id, /*source*/ 0, next);
            if (emit.lfg_queue(dungeonId, kSquad[i].role))
            {
                if (BotAI* ai = Services::Registry().ai(id))
                    ai->set_last_lfg_dungeon_id(dungeonId);
                ++queued;
            }
        }
        handler->PSendSysMessage(
            "dungeontest '%s' (id %u): squad ready at lvl %u — LFG-queued %u/5 "
            "(tank+healer+3dps). Finder will form the group + port them in.",
            dun->Name.Str[sWorld->GetDefaultDbcLocale()], dungeonId, level, queued);
        return true;
    }

    // PvE group-coordinator plan dump: one line per coordinated
    // dungeon/raid group with the current duty distribution.
    static bool HandlePveCoord(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 services not initialized.");
            return true;
        }
        const std::string report =
            Playerbot::Services::PveCoordinator().DebugDump();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // ----- Road-aware pathfinding telemetry --------------------------------
    //
    // dtQueryFilterTC maintains atomic counters incremented per
    // CalculatePath via dtQueryFilterTC::TallyPath (called from
    // PathGenerator::CalculatePath after the path stabilises).
    //
    // Modes:
    //   .playerbot roadstats              — global summary
    //   .playerbot roadstats <mapId>      — single-map breakdown
    //   .playerbot roadstats list         — table of every map with tallied paths
    //
    // Reports: paths run, paths with ≥1 road poly (% of total),
    // road poly fraction, paths in instance, paths with road bonus
    // disabled, paths with slope penalty > 10%.

    static std::string FormatRoadStatsLine(char const* label,
                                           dtQueryFilterTC::RoadStats const& st)
    {
        return Trinity::StringFormat(
            "{} paths_run={} with_road={} ({:.1f}%) disabled={} ({:.1f}%) "
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
    }

    static bool HandleRoadStats(ChatHandler* handler, Optional<std::string> argOpt)
    {
        std::string arg = argOpt ? *argOpt : std::string{};
        // trim
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t'))
            arg.erase(arg.begin());
        while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t'))
            arg.pop_back();

        if (arg == "list")
        {
            auto maps = dtQueryFilterTC::ListMapsWithStats();
            if (maps.empty())
            {
                handler->SendSysMessage("Roadstats list: no paths tallied yet.");
                return true;
            }
            std::vector<std::pair<uint32, dtQueryFilterTC::RoadStats>> entries;
            entries.reserve(maps.size());
            for (uint32 m : maps)
                entries.emplace_back(m, dtQueryFilterTC::SampleStatsForMap(m));
            std::sort(entries.begin(), entries.end(),
                [](auto const& a, auto const& b) { return a.second.pathsRun > b.second.pathsRun; });

            handler->PSendSysMessage("Roadstats list: %zu maps with tallied paths",
                                     entries.size());
            for (auto const& [m, st] : entries)
            {
                std::string label = Trinity::StringFormat("  map={}:", m);
                handler->SendSysMessage(FormatRoadStatsLine(label.c_str(), st));
            }
            return true;
        }

        if (!arg.empty())
        {
            uint32 mapId = 0;
            try { mapId = static_cast<uint32>(std::stoul(arg)); }
            catch (...) { mapId = 0; }
            if (mapId == 0)
            {
                handler->PSendSysMessage(
                    "Roadstats: unknown arg '%s'. Use no arg (global), <mapId>, or 'list'.",
                    arg.c_str());
                return false;
            }
            dtQueryFilterTC::RoadStats st = dtQueryFilterTC::SampleStatsForMap(mapId);
            if (st.pathsRun == 0)
            {
                handler->PSendSysMessage("Roadstats map=%u: no paths tallied for this map.", mapId);
                return true;
            }
            std::string label = Trinity::StringFormat("Roadstats map={}:", mapId);
            handler->SendSysMessage(FormatRoadStatsLine(label.c_str(), st));
            return true;
        }

        dtQueryFilterTC::RoadStats st = dtQueryFilterTC::SampleStats();
        handler->SendSysMessage(FormatRoadStatsLine("Roadstats:", st));
        return true;
    }

    static bool HandleRoadReset(ChatHandler* handler)
    {
        dtQueryFilterTC::ResetStats();
        handler->SendSysMessage("Roadstats: counters reset.");
        return true;
    }

    // A/B compare: run the same pathfind twice — once with road bonus,
    // once without — and report the difference in length + road poly
    // count. Lets the operator confirm road bias is producing visibly
    // different routes for a specific destination.
    //
    // Usage:
    //   .playerbot pathcompare <x> <y> <z>
    //   .playerbot pathcompare preset <name>
    //   .playerbot pathcompare preset       (lists available presets)
    static bool HandlePathCompare(ChatHandler* handler, Optional<std::string> argOpt)
    {
        Player* p = handler->GetPlayer();
        if (!p)
        {
            handler->SendSysMessage("Pathcompare: must be run in-world (needs a source unit).");
            return false;
        }

        struct Preset {
            char const* name;
            uint32 mapId;
            float x, y, z;
            char const* note;
        };
        constexpr Preset presets[] = {
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

        std::string arg = argOpt ? *argOpt : std::string{};
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t'))
            arg.erase(arg.begin());
        while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t'))
            arg.pop_back();

        float x = 0.0f, y = 0.0f, z = 0.0f;

        if (arg.rfind("preset", 0) == 0)
        {
            std::string presetName;
            auto sp = arg.find(' ');
            if (sp != std::string::npos)
                presetName = arg.substr(sp + 1);

            if (presetName.empty())
            {
                handler->SendSysMessage("Pathcompare presets:");
                for (auto const& pr : presets)
                    handler->PSendSysMessage("  %s (map %u) — %s",
                                             pr.name, pr.mapId, pr.note);
                return true;
            }

            Preset const* chosen = nullptr;
            for (auto const& pr : presets)
                if (presetName == pr.name) { chosen = &pr; break; }
            if (!chosen)
            {
                handler->PSendSysMessage(
                    "Pathcompare: unknown preset '%s'. Use .playerbot pathcompare preset (no name) to list.",
                    presetName.c_str());
                return false;
            }
            if (p->GetMapId() != chosen->mapId)
                handler->PSendSysMessage(
                    "Pathcompare: warning — you are on map %u, preset is for map %u. Pathfind may fail.",
                    p->GetMapId(), chosen->mapId);
            x = chosen->x; y = chosen->y; z = chosen->z;
        }
        else if (std::sscanf(arg.c_str(), "%f %f %f", &x, &y, &z) != 3)
        {
            handler->SendSysMessage(
                "Pathcompare: usage: <x> <y> <z> | preset <name> | preset (list)");
            return false;
        }

        auto runOne = [&](bool disableBonus) -> std::tuple<bool, float, uint32> {
            PathGenerator pg(p);
            pg.SetDisableRoadBonus(disableBonus);
            bool ok = pg.CalculatePath(x, y, z, false);
            float len = pg.GetPathLength();
            uint32 nPoints = static_cast<uint32>(pg.GetPath().size());
            return { ok, len, nPoints };
        };

        dtQueryFilterTC::RoadStats stBefore = dtQueryFilterTC::SampleStats();
        auto [okWith, lenWith, ptsWith] = runOne(false);
        dtQueryFilterTC::RoadStats stMid = dtQueryFilterTC::SampleStats();
        auto [okWithout, lenWithout, ptsWithout] = runOne(true);
        dtQueryFilterTC::RoadStats stAfter = dtQueryFilterTC::SampleStats();

        const uint64 roadWith    = stMid.roadPolysVisited    - stBefore.roadPolysVisited;
        const uint64 roadWithout = stAfter.roadPolysVisited  - stMid.roadPolysVisited;
        const uint64 totalWith   = stMid.totalPolysVisited   - stBefore.totalPolysVisited;
        const uint64 totalWithout= stAfter.totalPolysVisited - stMid.totalPolysVisited;

        handler->PSendSysMessage("Pathcompare to (%.1f, %.1f, %.1f):", x, y, z);
        handler->PSendSysMessage(
            "  with road:    ok=%d len=%.1fy points=%u road_polys=%llu/%llu",
            okWith ? 1 : 0, lenWith, ptsWith,
            static_cast<unsigned long long>(roadWith),
            static_cast<unsigned long long>(totalWith));
        handler->PSendSysMessage(
            "  without road: ok=%d len=%.1fy points=%u road_polys=%llu/%llu",
            okWithout ? 1 : 0, lenWithout, ptsWithout,
            static_cast<unsigned long long>(roadWithout),
            static_cast<unsigned long long>(totalWithout));

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
        handler->PSendSysMessage("  delta_length=%+.1fy delta_road_polys=%+lld  -> %s",
                                 lenDelta, static_cast<long long>(roadDelta), verdict);
        return true;
    }

    static bool HandleDiag(ChatHandler* handler, std::string const& target_name)
    {
        if (target_name.empty())
        { handler->SendSysMessage("Usage: .playerbot diag <character_name>"); return false; }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p) { handler->PSendSysMessage("'%s' not in-world.", target_name.c_str()); return false; }
        const std::string report =
            Playerbot::Diagnostics::DiagBot(p->GetGUID().GetCounter());
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleReseed(ChatHandler* handler, Optional<std::string> confirm_arg)
    {
        // Destructive operation — require explicit "confirm" arg to run, so a
        // typo of `.playerbot reseed` doesn't wipe the entire distribution
        // metadata. Resets distribution_level + setup_pipeline_state for ALL
        // V2 characters; the next population tick re-shapes the fleet from
        // scratch and re-runs the per-bot setup pipeline.
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return true; }

        if (!confirm_arg || *confirm_arg != "confirm")
        {
            handler->SendSysMessage(
                "Reseed: clears distribution_level + setup_pipeline_state on every V2 bot, "
                "then triggers a full re-shape. Run `.playerbot reseed confirm` to proceed.");
            return true;
        }

        CharacterDatabase.PExecute(
            "UPDATE playerbot_v2_character SET distribution_level=0, setup_pipeline_state=0");
        Playerbot::Services::Population().ForceReconcile();
        handler->SendSysMessage("Reseed: wiped distribution + pipeline state. Force-reconciling now.");
        return true;
    }

    // .playerbot wipefleet confirm
    //
    // Hard-deletes EVERY V2 bot character: full DeleteFromDB on each
    // character_guid, drops the playerbot_v2_character rows, frees pool
    // account slots. The next population tick sees an empty fleet and
    // respawns from scratch with the current pipeline + composition + gear
    // generator + talent seed - so existing bots that pre-date today's
    // fixes (Norithyn-style equip-swap broken, conjure-loop, etc.) get
    // replaced with fresh bots that reflect the current system state.
    //
    // Online bots are kicked via SessionMgr::LogoutBot first; their actual
    // DB-side deletion happens on the SAME call (DeleteFromDB tolerates a
    // session-still-being-closed because the row writes are idempotent
    // and the in-memory Player gets cleaned up on the next world tick).
    //
    // Destructive: requires explicit "confirm" arg. Pool ACCOUNTS are
    // kept (BotAccountMgr is told each character was removed so the
    // per-account character count drops, and the pool reuses these
    // accounts for the fresh wave of spawns).
    static bool HandleWipeFleet(ChatHandler* handler, Optional<std::string> confirm_arg)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return true; }

        if (!confirm_arg || *confirm_arg != "confirm")
        {
            handler->SendSysMessage(
                "WipeFleet: hard-deletes ALL V2 bot characters and respawns from scratch. "
                "Pool accounts are kept. This is the right command after pipeline fixes "
                "that need fresh characters to take effect (vs `reseed` which only re-runs "
                "the pipeline on existing chars). Run `.playerbot wipefleet confirm`.");
            return true;
        }

        // Snapshot every V2 character_guid + its account before mutating
        // anything. Reading the row first lets us free the pool-account
        // slot via BotAccountMgr after the character is gone.
        auto res = CharacterDatabase.PQuery(
            "SELECT pv.character_guid_low, c.account FROM playerbot_v2_character pv "
            "JOIN characters c ON c.guid = pv.character_guid_low");
        if (!res || !res->GetRowCount())
        {
            handler->SendSysMessage("WipeFleet: no V2 bots to delete.");
            return true;
        }

        uint32 logged_out = 0;
        uint32 deleted    = 0;
        uint32 skipped    = 0;
        do
        {
            Field* f = res->Fetch();
            uint64 guid_low = f[0].GetUInt64();
            uint32 account  = f[1].GetUInt32();
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(guid_low);

            // Online: kick first. Server processes the logout asynchronously
            // but the row deletion below is independent of in-memory state.
            if (ObjectAccessor::FindConnectedPlayer(g))
            {
                Playerbot::Services::SessionMgr().LogoutBot(g);
                ++logged_out;
            }

            // Mirror HandleDelete's unmark + unregister sequence so the
            // V2 services don't keep dangling references to the soon-to-be
            // deleted character.
            if (Playerbot::Services::Registry().has(guid_low))
            {
                Playerbot::Services::Scheduler().unregister_bot(guid_low);
                Playerbot::Services::Snapshots().remove(guid_low);
                Playerbot::Services::Registry().unregister_bot(guid_low);
            }
            Playerbot::Services::Lifecycle().unmark_as_bot(guid_low);

            // Hard delete (deleteFinally=true bypasses the soft-delete pool).
            // Skip if the character is somehow gone already (shouldn't happen
            // since we just queried the row, but defensive).
            if (sCharacterCache->GetCharacterAccountIdByGuid(g) != 0 ||
                CharacterDatabase.PQuery("SELECT 1 FROM characters WHERE guid={}", guid_low))
            {
                Player::DeleteFromDB(g, account, /*updateRealmChars*/true,
                                                 /*deleteFinally*/true);
                Playerbot::Services::Accounts().note_character_removed(account);
                ++deleted;
            }
            else
            {
                ++skipped;
            }
        } while (res->NextRow());

        // Drop residual playerbot_v2_character rows in one shot. Some may
        // already be gone if Player::DeleteFromDB cascaded into our table
        // (it doesn't, currently — but harmless to run unconditionally).
        CharacterDatabase.DirectExecute(
            "DELETE FROM playerbot_v2_character");

        // Seed the fleet with a burst of immediate spawns so the rebuild is
        // visible right away. The default per-cycle throttle (5 spawns / 60s)
        // is appropriate for steady-state shaping but feels like "nothing
        // happens" right after a wipe. We loop ForceReconcile() up to 10
        // times - each call resets budgets to 5 and runs Reconcile, so we
        // get ~50 fresh spawns on the spot. After the burst, the regular
        // 60s reconcile takes over and fills toward the configured target
        // at the steady rate.
        auto& pop = Playerbot::Services::Population();
        constexpr int kBurstCycles = 10;
        for (int i = 0; i < kBurstCycles; ++i)
            pop.ForceReconcile();

        handler->PSendSysMessage(
            "WipeFleet: kicked %u online, deleted %u characters, skipped %u missing. "
            "Burst-spawned ~%d new bots; reconcile continues at %u/cycle until target met.",
            logged_out, deleted, skipped,
            kBurstCycles * 5,
            5u);
        return true;
    }

    // .playerbot smoketest [count] [sub]
    //
    // End-to-end regression harness. With no args (or a numeric count) it
    // launches a fresh run: spawns N bots, waits up to 60s for every bot's
    // setup_pipeline to reach AllDone, then checks pipeline failures, intent
    // history, dropped-intent counter, and DB queue depth. Default count: 5.
    //
    // Sub-commands:
    //   .playerbot smoketest result   — print the most recent finished run
    //   .playerbot smoketest status   — print whether a run is in flight
    //
    // Because the wait phase needs the world thread to keep ticking (so
    // async login + setup-pipeline DB callbacks can fire), the test runs on
    // a detached worker thread and writes the verdict to Server.log under
    // logger "playerbot.v2.smoketest". The command returns immediately;
    // re-run with the "result" sub-command to print the final report once
    // the worker logs PASS/FAIL.
    static bool HandleSmoketest(ChatHandler* handler, Optional<std::string> arg)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        // Sub-command dispatch — string args take precedence over numeric.
        if (arg)
        {
            std::string const& s = *arg;
            if (s == "result" || s == "results")
            {
                auto r = Playerbot::V2::Diagnostics::Snapshot();
                const std::string out = Playerbot::V2::Diagnostics::Render(r);
                for (auto const& line : Trinity::Tokenize(out, '\n', true))
                    handler->SendSysMessage(std::string{line});
                return true;
            }
            if (s == "status")
            {
                handler->PSendSysMessage("Smoketest %s.",
                    Playerbot::V2::Diagnostics::IsRunning() ? "running" : "idle");
                return true;
            }
            // .playerbot smoketest dungeon [<map_id>|all]
            //
            // Static validation pass over the DungeonScript registry: for
            // each (or one) registered script, check that (a) bosses[] is
            // non-empty, (b) every boss creature entry resolves via
            // sObjectMgr->GetCreatureTemplate, (c) every interrupt spell
            // resolves via sSpellMgr->GetSpellInfo. Catches typo'd ids
            // before runtime. No bots spawned. Result printed in-line.
            if (s.starts_with("dungeon"))
            {
                std::string rest = s.size() > 7 ? s.substr(7) : std::string();
                while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
                uint32 only_map = 0;
                if (!rest.empty() && rest != "all")
                {
                    char* end = nullptr;
                    const unsigned long n = std::strtoul(rest.c_str(), &end, 10);
                    if (end == rest.c_str())
                    {
                        handler->SendSysMessage(
                            "Usage: .playerbot smoketest dungeon [<map_id>|all]");
                        return false;
                    }
                    only_map = uint32(n);
                }
                return RunDungeonValidation(handler, only_map);
            }
            // .playerbot smoketest bglive [count]   (Wave C)
            //
            // Live BG-aware smoketest. Spawns N bots (default 10) and waits
            // the standard 60s window — same as `.playerbot smoketest`, but
            // additionally asserts that at least one bot has fired an
            // `idle:bg_*` rule during the window. Requires the BG queue-
            // filler to be enabled in playerbot.conf — without it, bots
            // never queue and the assertion will fail with the canonical
            // "0 of N bots fired any idle:bg_* rule" message.
            //
            // Match this BEFORE the static "bg" sub-command since the prefix
            // "bg" is a substring of "bglive". Without this ordering the
            // static path would swallow the request.
            if (s.starts_with("bglive"))
            {
                std::string rest = s.size() > 6 ? s.substr(6) : std::string();
                while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
                Playerbot::V2::Diagnostics::LaunchOptions opts;
                opts.count = 10;            // larger pool than default 5
                opts.timeout_ms = 60'000;
                opts.sample_bg = true;
                if (!rest.empty())
                {
                    char* end = nullptr;
                    const unsigned long n = std::strtoul(rest.c_str(), &end, 10);
                    if (end == rest.c_str())
                    {
                        handler->SendSysMessage(
                            "Usage: .playerbot smoketest bglive [count]");
                        return false;
                    }
                    opts.count = uint32(n);
                }
                if (Playerbot::V2::Diagnostics::IsRunning())
                {
                    handler->SendSysMessage(
                        "Smoketest already running. Wait for completion or "
                        "use `.playerbot smoketest status`.");
                    return false;
                }
                WorldSession* sess = handler->GetSession();
                if (!sess)
                {
                    handler->SendSysMessage(
                        "Live smoketest requires an in-game GM session.");
                    return false;
                }
                if (!Playerbot::V2::Diagnostics::Launch(
                        sess->GetAccountId(), opts))
                {
                    handler->SendSysMessage("Live smoketest launch refused.");
                    return false;
                }
                handler->PSendSysMessage(
                    "Live BG smoketest started: count=%u timeout=%us. "
                    "Adds an `idle:bg_*` rule-fire assertion to the standard "
                    "pipeline. Result via `.playerbot smoketest result` "
                    "after ~%us.",
                    opts.count, opts.timeout_ms / 1000u, opts.timeout_ms / 1000u);
                return true;
            }
            // .playerbot smoketest bg [<bml_id>|all]
            // Static validation over the BattlegroundScript registry.
            if (s.starts_with("bg"))
            {
                std::string rest = s.size() > 2 ? s.substr(2) : std::string();
                while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
                uint32 only_id = 0;
                if (!rest.empty() && rest != "all")
                {
                    char* end = nullptr;
                    const unsigned long n = std::strtoul(rest.c_str(), &end, 10);
                    if (end == rest.c_str())
                    {
                        handler->SendSysMessage(
                            "Usage: .playerbot smoketest bg [<bml_id>|all]");
                        return false;
                    }
                    only_id = uint32(n);
                }
                return RunBgValidation(handler, only_id);
            }
        }

        if (Playerbot::V2::Diagnostics::IsRunning())
        {
            handler->SendSysMessage(
                "Smoketest already running. Wait for completion or use "
                "`.playerbot smoketest status`.");
            return false;
        }

        // Numeric arg path. Parse the optional count; clamp matches Launch().
        Playerbot::V2::Diagnostics::LaunchOptions opts;
        opts.count = 5;
        if (arg)
        {
            char* end = nullptr;
            const unsigned long n = std::strtoul(arg->c_str(), &end, 10);
            if (end == arg->c_str())
            {
                handler->PSendSysMessage(
                    "Usage: .playerbot smoketest [count] | result | status");
                return false;
            }
            opts.count = uint32(n);
        }

        // BotCharacterFactory needs a WorldSession for locale + audit.
        // Console invocation can't supply one — bail with a friendly hint.
        WorldSession* sess = handler->GetSession();
        if (!sess)
        {
            handler->SendSysMessage(
                "Smoketest requires an in-game GM session "
                "(BotCharacterFactory needs a WorldSession). Run from in-game.");
            return false;
        }

        const bool launched =
            Playerbot::V2::Diagnostics::Launch(sess->GetAccountId(), opts);
        if (!launched)
        {
            handler->SendSysMessage("Smoketest launch refused (already running?).");
            return false;
        }

        handler->PSendSysMessage(
            "Smoketest started: count=%u timeout=%us. Worker thread will log "
            "PASS/FAIL to Server.log (logger 'playerbot.v2.smoketest'). Run "
            "`.playerbot smoketest result` after ~%us for the full report.",
            opts.count, opts.timeout_ms / 1000u, opts.timeout_ms / 1000u);
        return true;
    }

    // .playerbot summon <character_name>
    //
    // Player-facing "summon one of my OWN alts as a bot" command. Distinct
    // from .playerbot mark + .playerbot login (the GM lifecycle pair) in
    // three ways:
    //   1) Requires an in-game session — there's no notion of "your alts"
    //      from console.
    //   2) Enforces account match: the named character must live on the
    //      same account as the issuing player. Without this gate any GM
    //      could conscript anyone's character on the realm.
    //   3) Auto-marks + auto-adopts on success: a single command end-to-end
    //      from "I want to play with my alt" to "AI-controlled bot is
    //      following me." No separate mark/adopt/login dance.
    //
    // Account-wide-progress is the design intent: a player levels their
    // main while their alts (logged in as bots via this command) gain XP
    // from group-completed quests / dungeons. Owner binding scopes whisper
    // commands so the player's other alts can also issue squad commands
    // (player_guid=0 = any character on the account).
    static bool HandleSummon(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        if (target_name.empty())
        {
            handler->SendSysMessage("Usage: .playerbot summon <character_name>");
            return false;
        }
        WorldSession* sess = handler->GetSession();
        if (!sess)
        {
            handler->SendSysMessage(
                "`.playerbot summon` is in-game only (needs your account "
                "context to verify alt ownership).");
            return false;
        }
        Player const* me = sess->GetPlayer();
        if (!me)
        {
            handler->SendSysMessage("No player attached to your session.");
            return false;
        }

        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.",
                                     target_name.c_str());
            return false;
        }
        if (id == me->GetGUID().GetCounter())
        {
            // Summoning yourself collapses into the Wave-F bot-AI-on-self
            // toggle, which is a different command. Refuse explicitly so
            // the user doesn't accidentally hand their main to the AI.
            handler->SendSysMessage(
                "That's your current character. Use `.playerbot self on` "
                "to drive your own character with the AI instead.");
            return false;
        }

        ObjectGuid const target_guid = ObjectGuid::Create<HighGuid::Player>(id);
        const uint32 my_account     = sess->GetAccountId();
        const uint32 target_account = sCharacterCache->GetCharacterAccountIdByGuid(target_guid);
        if (target_account == 0)
        {
            handler->PSendSysMessage("'%s' has no resolvable account.",
                                     target_name.c_str());
            return false;
        }
        if (target_account != my_account)
        {
            // Hard fail with no leak: don't reveal which account owns the
            // target. The player either owns this alt or doesn't — that's
            // all they need to know.
            handler->PSendSysMessage("'%s' is not on your account.",
                                     target_name.c_str());
            return false;
        }
        // Refuse if the alt is already logged in as a live player (would
        // cause two sessions for the same GUID). The owner is using that
        // character right now and presumably doesn't want it taken over.
        if (Player const* online = ObjectAccessor::FindConnectedPlayerByName(target_name))
        {
            handler->PSendSysMessage("'%s' is currently online (logged in by "
                                     "you or another session).",
                                     online->GetName().c_str());
            return false;
        }

        // Per-account summon cap. Default 5 = one regular dungeon group.
        // Counts CURRENTLY owned alts; logged-out-but-marked alts still
        // count (they hold a worldsession + DB row at runtime). The
        // .playerbot self attachment doesn't count against this cap
        // because the player is using their own character, not a bot.
        const uint8 cap = Playerbot::Services::Config().max_alts_as_bots();
        if (cap == 0)
        {
            handler->SendSysMessage(
                "Alt-summon is disabled on this realm "
                "(PlayerbotV2.MaxAltsAsBots = 0).");
            return false;
        }
        const auto owned = Playerbot::Services::Owners().BotsOwnedBy(my_account);
        // Filter to alts that are currently marked as bots; the binding
        // can outlive an unmark and we don't want to double-count those.
        size_t active_alts = 0;
        for (Playerbot::BotId b : owned)
            if (Playerbot::Services::Lifecycle().is_bot(b)) ++active_alts;
        if (active_alts >= cap)
        {
            handler->PSendSysMessage(
                "Alt-summon cap reached: you already have %zu of %u alts "
                "active as bots. Logout one (`.playerbot self off` or "
                "`/w <alt> logout`) before summoning another.",
                active_alts, unsigned(cap));
            return false;
        }

        // Auto-mark + auto-adopt in one shot. The bot character record is
        // shared with the regular V2 bot pool, but ownership is bound to
        // the summoning player's account so squad/whisper commands route
        // back to them rather than to any GM who happens to be online.
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            Playerbot::Services::Lifecycle().mark_as_bot(id);
        }
        Playerbot::Services::Owners().SetOwner(id, my_account, /*player_guid=*/0u);

        // Headless login. The world thread will process the async DB
        // callback that finishes the spawn; the bot enters world on the
        // next tick after that callback fires.
        auto res = Playerbot::Services::SessionMgr().LoginBot(target_guid);
        if (!res.ok)
        {
            handler->PSendSysMessage("Summon failed: %s", res.reason.c_str());
            return false;
        }

        handler->PSendSysMessage(
            "Summoning your alt '%s' — AI-controlled bot enters world on "
            "the next DB callback. You (and any other char on your account) "
            "can whisper `follow`, `stop`, `engage`, `squad`, `role` to it.",
            target_name.c_str());
        return true;
    }

    // .playerbot self on | off | status
    //
    // First-person testing toggle: attach (or detach) the V2 AI to the
    // issuing player's OWN character. Lets the player observe AI decisions
    // from inside the bot — vastly deeper insight than reading logs after
    // the fact ("why did it stop?", "why didn't it kick?", "where was the
    // pull heuristic going to send me?").
    //
    // Mechanics: this is a thin wrapper over mark/adopt + register/unregister,
    // scoped to "myself" so the user doesn't have to type their own name.
    // The player's client input still flows through the network normally
    // — IntentExecutor and the client compete on movement and ability use,
    // and the player wins when they actively press a key (the AI's movement
    // intents will arrive late and overwrite, but only when the client is
    // idle). For pure observation, the player can simply not move and watch.
    //
    // Status sub-form prints whether AI is currently attached to self
    // without changing anything, useful for verifying the toggle.
    static bool HandleSelf(ChatHandler* handler, std::string const& arg)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        WorldSession* sess = handler->GetSession();
        if (!sess || !sess->GetPlayer())
        {
            handler->SendSysMessage(
                "`.playerbot self` is in-game only (needs your character).");
            return false;
        }
        Player* me = sess->GetPlayer();
        const Playerbot::BotId id = me->GetGUID().GetCounter();
        const bool is_bot      = Playerbot::Services::Lifecycle().is_bot(id);
        const bool is_attached = Playerbot::Services::Registry().has(id);

        std::string mode = arg;
        for (char& c : mode) if (c >= 'A' && c <= 'Z') c = char(c + 32);
        if (mode.empty() || mode == "status")
        {
            handler->PSendSysMessage(
                "Self-AI: marked=%s attached=%s (use `on`/`off` to toggle).",
                is_bot ? "yes" : "no",
                is_attached ? "yes" : "no");
            return true;
        }
        if (mode == "on")
        {
            if (is_attached)
            {
                handler->SendSysMessage("Self-AI is already attached.");
                return true;
            }
            // Mark + adopt to self (account-bound). On-mark code path
            // already registers + schedules the AI for the online player,
            // so a single call wires everything.
            if (!is_bot)
                Playerbot::Services::Lifecycle().mark_as_bot(id);
            Playerbot::Services::Owners().SetOwner(
                id, sess->GetAccountId(), /*player_guid=*/ 0u);
            if (!Playerbot::Services::Registry().has(id))
            {
                Playerbot::BotPersonality personality =
                    Playerbot::Services::Config().random_personality()
                        ? Playerbot::RandomPersonality(Playerbot::SeedForBot(id))
                        : Playerbot::DefaultPersonality();
                Playerbot::Services::Registry().register_bot(
                    id, personality,
                    Playerbot::BotRng{Playerbot::SeedForBot(id)});
                Playerbot::Services::Scheduler().register_bot(
                    id, Playerbot::ActivityTier::Idle);
            }
            handler->SendSysMessage(
                "Self-AI: ON. AI now drives your character. Your client "
                "input still works — pressing movement keys interrupts the "
                "AI's move intents. Use `.playerbot self off` to detach.");
            return true;
        }
        if (mode == "off")
        {
            if (!is_bot && !is_attached)
            {
                handler->SendSysMessage("Self-AI is not attached.");
                return true;
            }
            if (Playerbot::Services::Registry().has(id))
            {
                Playerbot::Services::Scheduler().unregister_bot(id);
                Playerbot::Services::Snapshots().remove(id);
                Playerbot::Services::Registry().unregister_bot(id);
            }
            if (is_bot)
                Playerbot::Services::Lifecycle().unmark_as_bot(id);
            Playerbot::Services::Owners().ClearOwner(id);
            handler->SendSysMessage(
                "Self-AI: OFF. AI detached. You are back to manual control.");
            return true;
        }
        handler->PSendSysMessage("Usage: .playerbot self on | off | status");
        return false;
    }

    // ----------------------------------------------------------------------
    // .playerbot meta — world-metadata editor.
    //
    // Subcommands:
    //   add <kind> [radius]      — capture current pos + zone, insert row
    //   add_at <kind> <m> <x> <y> <z> [radius]  — explicit coords
    //   list [<kind>] [<r>]      — show rows near caller (default r=200y, all kinds)
    //   delete <id>              — remove row by id
    //   reload                   — re-read DB into in-memory cache
    //   help                     — print kinds + usage
    //
    // Kinds: road, crossroad, city, village, hub, danger, vendor, mailbox,
    //        innkeeper, other (see WorldMetadata.h::WorldMetadataKind).
    //
    // The handler is in-world only — capturing the GM's position requires
    // a Player*. Console-only operators use add_at + map coords.
    static bool HandleMeta(ChatHandler* handler, Optional<std::string> argOpt)
    {
        using namespace ::Playerbot::V2::World;

        std::string args = argOpt ? *argOpt : std::string();
        // Trim leading whitespace
        size_t s = 0;
        while (s < args.size() && std::isspace(static_cast<unsigned char>(args[s]))) ++s;
        if (s) args.erase(0, s);

        // Auto-export helper: write the full metadata cache to a flat
        // CSV after every successful add/delete so the next
        // mmaps_generator regen automatically picks up the new
        // waypoints. Default destination matches the generator's
        // auto-discovery path (<data>/world_metadata.csv).
        auto auto_export = [](WorldMetadataStore& s) {
            // Use M:/WorldofWarcraft/world_metadata.csv literally —
            // that's the inputDirectory the GM-curated regens use. If
            // operators want a different path they invoke `meta export
            // <path>` explicitly.
            std::FILE* f = std::fopen("M:/WorldofWarcraft/world_metadata.csv", "wb");
            if (!f) return;
            auto rows = s.Snapshot();
            std::fprintf(f,
                "# playerbot_v2_world_metadata export — schema_version=9\n"
                "# columns: id,map_id,zone_id,kind,kind_name,x,y,z,radius,label,notes\n");
            for (auto const& r : rows)
            {
                std::fprintf(f, "%llu,%u,%u,%u,%s,%.3f,%.3f,%.3f,%.2f,%s,%s\n",
                    static_cast<unsigned long long>(r.id),
                    r.map_id, r.zone_id, uint32(r.kind), KindToString(r.kind),
                    r.x, r.y, r.z, r.radius,
                    r.label.c_str(), r.notes.c_str());
            }
            std::fclose(f);
        };

        auto print_help = [&]() {
            handler->SendSysMessage("Playerbot meta — world knowledge editor.");
            handler->SendSysMessage("CREATE / MUTATE:");
            handler->SendSysMessage("  add <kind> [radius]            capture current pos");
            handler->SendSysMessage("  add_at <kind> <m> <x> <y> <z>  explicit coords");
            handler->SendSysMessage("  edit <id> <field>=<value>      radius|label|notes");
            handler->SendSysMessage("  delete <id>");
            handler->SendSysMessage("  delete_near <r> [<kind>]       preview bulk delete");
            handler->SendSysMessage("  delete_near_confirm <r> [<k>]  apply bulk delete");
            handler->SendSysMessage("QUERY:");
            handler->SendSysMessage("  list [<kind>] [<radius_yards>] 30 nearest");
            handler->SendSysMessage("  nearest [<kind>]               single closest");
            handler->SendSysMessage("  count                          total + per-kind summary");
            handler->SendSysMessage("  stats                          per-map per-kind breakdown");
            handler->SendSysMessage("  status                         cache/CSV/DB health check");
            handler->SendSysMessage("STATE:");
            handler->SendSysMessage("  reload                         re-read DB + re-export CSV");
            handler->SendSysMessage("  export [<path>]                dump cache to CSV");
            handler->SendSysMessage("Kinds: road, crossroad, city, village, hub,");
            handler->SendSysMessage("       danger, vendor, mailbox, innkeeper, other");
            handler->SendSysMessage("Prefix-match accepted (eg 'ro' -> road, 'vi' -> village).");
            handler->SendSysMessage("Every mutation auto-exports M:/WorldofWarcraft/world_metadata.csv");
            handler->SendSysMessage("for the next mmaps_generator regen.");
        };

        if (args.empty() || args == "help")
        {
            print_help();
            return true;
        }

        if (args == "examples")
        {
            handler->SendSysMessage("=== Common meta workflows ===");
            handler->SendSysMessage("Annotate a road from Shadowglen to Dolanaar:");
            handler->SendSysMessage("  (ride a mount along the road, stop every 10-15y)");
            handler->SendSysMessage("  .playerbot meta add road");
            handler->SendSysMessage("Mark Dolanaar as a hub for low-level bots:");
            handler->SendSysMessage("  (stand in Dolanaar center)");
            handler->SendSysMessage("  .playerbot meta add hub 80");
            handler->SendSysMessage("Annotate Stormwind cathedral district as city:");
            handler->SendSysMessage("  (stand in cathedral square)");
            handler->SendSysMessage("  .playerbot meta add city 250");
            handler->SendSysMessage("Mark a danger zone with a label:");
            handler->SendSysMessage("  .playerbot meta add danger 60");
            handler->SendSysMessage("  .playerbot meta nearest danger   (note the id)");
            handler->SendSysMessage("  .playerbot meta edit <id> label=DeadminesPath");
            handler->SendSysMessage("Verify your edits:");
            handler->SendSysMessage("  .playerbot meta count");
            handler->SendSysMessage("  .playerbot meta nearest road");
            handler->SendSysMessage("  .playerbot meta status");
            handler->SendSysMessage("After annotation, regen mmaps (worldserver stopped):");
            handler->SendSysMessage("  mmaps_generator.exe <mapId> --input <DataDir> --output <DataDir>");
            return true;
        }

        // Tokenize on whitespace.
        std::vector<std::string> tok;
        {
            std::string cur;
            for (char c : args)
            {
                if (std::isspace(static_cast<unsigned char>(c)))
                {
                    if (!cur.empty()) { tok.push_back(std::move(cur)); cur.clear(); }
                }
                else cur.push_back(c);
            }
            if (!cur.empty()) tok.push_back(std::move(cur));
        }

        std::string const& sub = tok[0];

        auto& store = WorldMetadataStore::Instance();

        if (sub == "sanity")
        {
            // Run consistency checks on the cached rows. Reports
            // problematic rows but doesn't auto-fix — operator decides.
            //   * radius <= 0 — placeholder bug
            //   * (x,y,z) == (0,0,0) — likely never-populated row
            //   * duplicate (map_id, kind, x, y) within 1y — likely
            //     accidental double-add
            auto rows = store.Snapshot();
            uint32 bad_radius = 0, zero_coords = 0, dupes = 0;
            std::vector<uint64> dupe_ids;
            for (size_t i = 0; i < rows.size(); ++i)
            {
                auto const& r = rows[i];
                if (r.radius <= 0.f) ++bad_radius;
                if (r.x == 0.f && r.y == 0.f && r.z == 0.f) ++zero_coords;
                for (size_t j = i + 1; j < rows.size(); ++j)
                {
                    auto const& r2 = rows[j];
                    if (r2.map_id != r.map_id) continue;
                    if (r2.kind   != r.kind)   continue;
                    const float dx = r2.x - r.x;
                    const float dy = r2.y - r.y;
                    if (dx*dx + dy*dy < 1.0f)
                    {
                        ++dupes;
                        if (dupe_ids.size() < 5)
                            dupe_ids.push_back(r2.id);
                    }
                }
            }
            handler->PSendSysMessage(
                "=== Meta sanity ===\n"
                "  bad_radius (<=0): {}\n"
                "  zero_coords (0,0,0): {}\n"
                "  near-duplicates: {}",
                bad_radius, zero_coords, dupes);
            if (!dupe_ids.empty())
            {
                std::string ids;
                for (size_t i = 0; i < dupe_ids.size(); ++i)
                {
                    if (i) ids += ',';
                    ids += std::to_string(dupe_ids[i]);
                }
                handler->PSendSysMessage("  first 5 dupe ids: {}", ids);
            }
            return true;
        }

        if (sub == "status")
        {
            // Quick health summary: in-memory cache size + on-disk CSV
            // size + DB row count via direct SELECT. Use to sanity-check
            // that the three are in sync.
            handler->PSendSysMessage("=== Meta status ===");
            handler->PSendSysMessage("  in-memory cache: {} rows", store.Size());
            // CSV file size (existence + bytes)
            auto csv_path = "M:/WorldofWarcraft/world_metadata.csv";
            std::FILE* fp = std::fopen(csv_path, "rb");
            if (fp)
            {
                std::fseek(fp, 0, SEEK_END);
                long sz = std::ftell(fp);
                std::fclose(fp);
                handler->PSendSysMessage("  on-disk CSV   : {} bytes ({})",
                                          sz, csv_path);
            }
            else
            {
                handler->PSendSysMessage("  on-disk CSV   : MISSING ({})", csv_path);
            }
            // DB row count via direct SELECT
            QueryResult res = CharacterDatabase.Query(
                "SELECT COUNT(*) FROM playerbot_v2_world_metadata");
            if (res)
            {
                handler->PSendSysMessage("  DB row count  : {}", res->Fetch()[0].GetUInt64());
            }
            else
            {
                handler->SendSysMessage("  DB row count  : (query failed)");
            }
            return true;
        }

        if (sub == "count")
        {
            // Quick aggregate — total + per-kind, no per-map breakdown.
            auto rows = store.Snapshot();
            std::array<uint32, uint32(WorldMetadataKind::Count_)> by_kind{};
            for (auto const& r : rows)
                if (uint32(r.kind) < by_kind.size())
                    ++by_kind[uint32(r.kind)];
            std::string line = fmt::format("Meta: {} total —", rows.size());
            for (uint32 k = 1; k < uint32(WorldMetadataKind::Count_); ++k)
                if (by_kind[k] > 0)
                    line += fmt::format(" {}={}", KindToString(WorldMetadataKind(k)), by_kind[k]);
            handler->SendSysMessage(line);
            return true;
        }

        if (sub == "stats")
        {
            // Per-map per-kind row counts. Useful for "did the operator
            // populate roads in zones X / Y?" pre-flight before a regen.
            auto rows = store.Snapshot();
            std::map<uint32, std::array<uint32, uint32(WorldMetadataKind::Count_)>> by_map;
            for (auto const& r : rows)
            {
                auto& a = by_map[r.map_id];
                if (uint32(r.kind) < a.size()) ++a[uint32(r.kind)];
            }
            handler->PSendSysMessage("Meta stats: total {} row(s) across {} map(s)",
                rows.size(), by_map.size());
            for (auto const& [mid, arr] : by_map)
            {
                std::string line = fmt::format("  map {}:", mid);
                for (uint32 k = 1; k < uint32(WorldMetadataKind::Count_); ++k)
                {
                    if (arr[k] > 0)
                        line += fmt::format(" {}={}", KindToString(WorldMetadataKind(k)), arr[k]);
                }
                handler->SendSysMessage(line);
            }
            return true;
        }

        if (sub == "reload")
        {
            int64 n = store.ReloadFromDb();
            // Re-export the CSV so mmaps_generator sees the same state
            // a direct DB edit may have introduced. Without this an
            // operator who hand-edited the table via SQL and then ran
            // `meta reload` would have a stale CSV on disk and the next
            // regen would tag based on the old data.
            auto_export(store);
            handler->PSendSysMessage("Meta: reloaded {} record(s) from DB (CSV re-exported)", n);
            return true;
        }

        if (sub == "export")
        {
            // Dump entire cache to a flat CSV at the path given as arg 1
            // (default M:\WorldofWarcraft\world_metadata.csv). mmaps_generator
            // reads this file at regen time when --road-overrides is passed.
            std::string out_path = (tok.size() >= 2)
                ? tok[1]
                : std::string("M:/WorldofWarcraft/world_metadata.csv");
            auto rows = store.Snapshot();
            std::FILE* f = std::fopen(out_path.c_str(), "wb");
            if (!f)
            {
                handler->PSendSysMessage("Meta export: cannot open '{}'", out_path);
                return false;
            }
            std::fprintf(f,
                "# playerbot_v2_world_metadata export — schema_version=9\n"
                "# columns: id,map_id,zone_id,kind,kind_name,x,y,z,radius,label,notes\n");
            for (auto const& r : rows)
            {
                std::fprintf(f, "%llu,%u,%u,%u,%s,%.3f,%.3f,%.3f,%.2f,%s,%s\n",
                    static_cast<unsigned long long>(r.id),
                    r.map_id, r.zone_id, uint32(r.kind), KindToString(r.kind),
                    r.x, r.y, r.z, r.radius,
                    r.label.c_str(), r.notes.c_str());
            }
            std::fclose(f);
            handler->PSendSysMessage("Meta: exported {} row(s) to {}",
                                    rows.size(), out_path);
            return true;
        }

        if (sub == "delete")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("Usage: .playerbot meta delete <id>");
                return false;
            }
            const uint64 id = std::strtoull(tok[1].c_str(), nullptr, 10);
            const bool ok = store.Delete(id);
            if (ok) auto_export(store);
            handler->PSendSysMessage("Meta: delete id={} {}", id, ok ? "OK" : "(not found)");
            return ok;
        }

        if (sub == "delete_near")
        {
            // .playerbot meta delete_near <radius_yards> [<kind>]
            // Bulk delete every row within radius of caller, optionally
            // filtered to a single kind. Confirmation: prints how many
            // would match BEFORE deleting; operator can re-run with
            // "delete_near_confirm" to actually delete. Same args.
            // Safer than allowing the chat-typo to nuke 50 road points.
            Player* p = handler->GetPlayer();
            if (!p || tok.size() < 2)
            {
                handler->SendSysMessage("Usage: .playerbot meta delete_near <radius> [<kind>]");
                return false;
            }
            const float radius = float(std::strtod(tok[1].c_str(), nullptr));
            if (radius <= 0.0f) return false;
            WorldMetadataKind kfilter = WorldMetadataKind::Unknown;
            if (tok.size() >= 3) kfilter = ParseKind(tok[2]);
            auto rows = (kfilter == WorldMetadataKind::Unknown)
                ? store.RecordsForMap(p->GetMapId())
                : store.RecordsForMapAndKind(p->GetMapId(), kfilter);
            const float bx = p->GetPositionX();
            const float by = p->GetPositionY();
            const float r2 = radius * radius;
            std::vector<uint64> hit_ids;
            for (auto const& r : rows)
            {
                const float dx = r.x - bx;
                const float dy = r.y - by;
                if (dx*dx + dy*dy <= r2) hit_ids.push_back(r.id);
            }
            handler->PSendSysMessage(
                "Meta delete_near: would delete {} row(s) within {:.0f}y (kind={}). "
                "Run `delete_near_confirm <radius> [<kind>]` to apply.",
                hit_ids.size(), radius,
                kfilter == WorldMetadataKind::Unknown ? "any" : KindToString(kfilter));
            return true;
        }

        if (sub == "delete_near_confirm")
        {
            Player* p = handler->GetPlayer();
            if (!p || tok.size() < 2) return false;
            const float radius = float(std::strtod(tok[1].c_str(), nullptr));
            if (radius <= 0.0f) return false;
            WorldMetadataKind kfilter = WorldMetadataKind::Unknown;
            if (tok.size() >= 3) kfilter = ParseKind(tok[2]);
            auto rows = (kfilter == WorldMetadataKind::Unknown)
                ? store.RecordsForMap(p->GetMapId())
                : store.RecordsForMapAndKind(p->GetMapId(), kfilter);
            const float bx = p->GetPositionX();
            const float by = p->GetPositionY();
            const float r2 = radius * radius;
            size_t deleted = 0;
            for (auto const& r : rows)
            {
                const float dx = r.x - bx;
                const float dy = r.y - by;
                if (dx*dx + dy*dy <= r2)
                    if (store.Delete(r.id)) ++deleted;
            }
            if (deleted) auto_export(store);
            handler->PSendSysMessage("Meta delete_near: deleted {} row(s)", deleted);
            return true;
        }

        if (sub == "edit")
        {
            // .playerbot meta edit <id> <field>=<value> [<field>=<value> ...]
            // field can be radius | label | notes. Position+kind are not
            // editable (delete+readd if needed).
            if (tok.size() < 3)
            {
                handler->SendSysMessage(
                    "Usage: .playerbot meta edit <id> "
                    "<radius=N|label=Foo|notes=Bar>");
                return false;
            }
            const uint64 id = std::strtoull(tok[1].c_str(), nullptr, 10);
            if (id == 0)
            {
                handler->SendSysMessage("Meta edit: bad id");
                return false;
            }
            bool any_change = false;
            for (size_t i = 2; i < tok.size(); ++i)
            {
                std::string const& kv = tok[i];
                auto eq = kv.find('=');
                if (eq == std::string::npos) continue;
                std::string key = kv.substr(0, eq);
                std::string val = kv.substr(eq + 1);
                if (key == "radius")
                {
                    float r = float(std::strtod(val.c_str(), nullptr));
                    if (r > 0.0f && store.UpdateRadius(id, r))
                    {
                        any_change = true;
                        handler->PSendSysMessage("Meta edit #{}: radius={:.0f}", id, r);
                    }
                }
                else if (key == "label")
                {
                    if (store.UpdateLabel(id, val))
                    {
                        any_change = true;
                        handler->PSendSysMessage("Meta edit #{}: label='{}'", id, val);
                    }
                }
                else if (key == "notes")
                {
                    if (store.UpdateNotes(id, val))
                    {
                        any_change = true;
                        handler->PSendSysMessage("Meta edit #{}: notes='{}'", id, val);
                    }
                }
                else
                {
                    handler->PSendSysMessage("Meta edit: unknown field '{}'", key);
                }
            }
            if (any_change) auto_export(store);
            return any_change;
        }

        if (sub == "nearest")
        {
            // .playerbot meta nearest [<kind>]
            // Print the single closest metadata point to the GM (any
            // kind if none specified). Useful for "where is my edit?"
            // sanity checks during editing.
            Player* p = handler->GetPlayer();
            if (!p)
            {
                handler->SendSysMessage("Meta nearest: must be in-world.");
                return false;
            }
            WorldMetadataKind kfilter = WorldMetadataKind::Unknown;
            if (tok.size() >= 2)
                kfilter = ParseKind(tok[1]);
            auto rows = (kfilter == WorldMetadataKind::Unknown)
                ? store.RecordsForMap(p->GetMapId())
                : store.RecordsForMapAndKind(p->GetMapId(), kfilter);
            if (rows.empty())
            {
                handler->PSendSysMessage("Meta nearest: no rows on map {} (kind={})",
                    p->GetMapId(),
                    kfilter == WorldMetadataKind::Unknown ? "any" : KindToString(kfilter));
                return true;
            }
            const float bx = p->GetPositionX();
            const float by = p->GetPositionY();
            WorldMetadataRecord const* best = nullptr;
            float best_dsq = std::numeric_limits<float>::infinity();
            for (auto const& r : rows)
            {
                const float dx = r.x - bx;
                const float dy = r.y - by;
                const float d2 = dx*dx + dy*dy;
                if (d2 < best_dsq) { best_dsq = d2; best = &r; }
            }
            if (best)
            {
                handler->PSendSysMessage(
                    "Meta nearest: #{} {} dist={:.1f}y r={:.0f} pos=({:.1f},{:.1f},{:.1f}) {}",
                    best->id, KindToString(best->kind),
                    std::sqrt(best_dsq), best->radius,
                    best->x, best->y, best->z,
                    best->label.empty() ? "" : best->label.c_str());
            }
            return true;
        }

        if (sub == "list")
        {
            Player* p = handler->GetPlayer();
            if (!p)
            {
                handler->SendSysMessage("Meta list: must be in-world (needs a position).");
                return false;
            }
            // Optional kind filter, optional radius
            WorldMetadataKind kfilter = WorldMetadataKind::Unknown;
            float rfilter = 200.0f;
            for (size_t i = 1; i < tok.size(); ++i)
            {
                WorldMetadataKind k = ParseKind(tok[i]);
                if (k != WorldMetadataKind::Unknown) { kfilter = k; continue; }
                float r = 0.0f;
                if (std::sscanf(tok[i].c_str(), "%f", &r) == 1 && r > 0.0f)
                    rfilter = r;
            }
            auto rows = (kfilter == WorldMetadataKind::Unknown)
                ? store.RecordsForMap(p->GetMapId())
                : store.RecordsForMapAndKind(p->GetMapId(), kfilter);
            const float bx = p->GetPositionX();
            const float by = p->GetPositionY();
            const float r2 = rfilter * rfilter;
            std::vector<std::pair<float, WorldMetadataRecord const*>> hits;
            for (auto const& r : rows)
            {
                const float dx = r.x - bx;
                const float dy = r.y - by;
                const float d2 = dx*dx + dy*dy;
                if (d2 <= r2) hits.emplace_back(d2, &r);
            }
            std::sort(hits.begin(), hits.end(),
                [](auto const& a, auto const& b) { return a.first < b.first; });
            handler->PSendSysMessage(
                "Meta list (map={}, radius={:.0f}y, kind={}): {} hit(s)",
                p->GetMapId(), rfilter,
                kfilter == WorldMetadataKind::Unknown ? "any" : KindToString(kfilter),
                hits.size());
            for (size_t i = 0; i < hits.size() && i < 30; ++i)
            {
                auto const* r = hits[i].second;
                handler->PSendSysMessage(
                    "  #{}  {} r={:.0f}  dist={:.0f}y  ({:.1f}, {:.1f}, {:.1f})  {}",
                    r->id, KindToString(r->kind), r->radius,
                    std::sqrt(hits[i].first), r->x, r->y, r->z,
                    r->label.empty() ? "" : r->label.c_str());
            }
            if (hits.size() > 30)
                handler->PSendSysMessage("  ... ({} more)", hits.size() - 30);
            return true;
        }

        if (sub == "add" || sub == "add_at")
        {
            const bool explicit_coords = (sub == "add_at");
            const size_t needTok = explicit_coords ? 6 : 2;   // at least: add_at <k> <m> <x> <y> <z>; add <k>
            if (tok.size() < needTok)
            {
                if (explicit_coords)
                    handler->SendSysMessage("Usage: .playerbot meta add_at <kind> <map> <x> <y> <z> [radius]");
                else
                    handler->SendSysMessage("Usage: .playerbot meta add <kind> [radius]");
                return false;
            }
            WorldMetadataRecord r;
            r.kind = ParseKind(tok[1]);
            if (r.kind == WorldMetadataKind::Unknown)
            {
                handler->PSendSysMessage(
                    "Meta add: unknown kind '{}'. Try: road, crossroad, city, village, "
                    "hub, danger, vendor, mailbox, innkeeper, other.",
                    tok[1]);
                return false;
            }
            if (explicit_coords)
            {
                r.map_id = uint32(std::strtoul(tok[2].c_str(), nullptr, 10));
                r.x = float(std::strtod(tok[3].c_str(), nullptr));
                r.y = float(std::strtod(tok[4].c_str(), nullptr));
                r.z = float(std::strtod(tok[5].c_str(), nullptr));
                if (tok.size() >= 7)
                    r.radius = float(std::strtod(tok[6].c_str(), nullptr));
            }
            else
            {
                Player* p = handler->GetPlayer();
                if (!p)
                {
                    handler->SendSysMessage("Meta add: must be in-world for position capture. Use add_at for explicit coords.");
                    return false;
                }
                r.map_id = p->GetMapId();
                r.zone_id = p->GetZoneId();
                r.x = p->GetPositionX();
                r.y = p->GetPositionY();
                r.z = p->GetPositionZ();
                if (tok.size() >= 3)
                    r.radius = float(std::strtod(tok[2].c_str(), nullptr));
            }
            // Default radii per kind (when caller didn't specify):
            if (r.radius <= 0.0f)
            {
                switch (r.kind)
                {
                    case WorldMetadataKind::Road:      r.radius = 8.0f;   break;
                    case WorldMetadataKind::Crossroad: r.radius = 15.0f;  break;
                    case WorldMetadataKind::City:      r.radius = 200.0f; break;
                    case WorldMetadataKind::Village:   r.radius = 80.0f;  break;
                    case WorldMetadataKind::Hub:       r.radius = 60.0f;  break;
                    case WorldMetadataKind::Danger:    r.radius = 50.0f;  break;
                    default:                           r.radius = 10.0f;  break;
                }
            }
            if (Player* p = handler->GetPlayer())
                r.created_by = p->GetName();
            const bool ok = store.Insert(r);
            if (ok) auto_export(store);
            if (ok)
                handler->PSendSysMessage(
                    "Meta: added #{} kind={} map={} pos=({:.1f},{:.1f},{:.1f}) r={:.0f}",
                    r.id, KindToString(r.kind), r.map_id,
                    r.x, r.y, r.z, r.radius);
            else
                handler->SendSysMessage("Meta add: DB insert failed (see server log).");
            return ok;
        }

        handler->PSendSysMessage("Meta: unknown subcommand '{}'. Run '.playerbot meta help'.", sub);
        return false;
    }
};

// PlayerScript hook bridge for the PlayerbotControl addon. We listen for
// addon-language whispers (LANG_ADDON / LANG_ADDON_LOGGED) and hand the
// body to AddonControl::OnAddonWhisper. The hook only fires for whispers
// the player initiates — the addon sends to its own player name (self-
// whisper), which still flows through this hook before the WhisperAddon
// path attempts delivery.
//
// We intentionally don't try to BLOCK the original whisper path; the
// receiver is the player themselves and TC's WhisperAddon already exits
// silently when the receiver's session hasn't registered the prefix.
// Side-effect-wise the original code is a no-op for self-targeted addon
// whispers we don't ack, so observation-only is fine.
class playerbot_v2_addon_listener : public PlayerScript
{
public:
    playerbot_v2_addon_listener() : PlayerScript("playerbot_v2_addon_listener") {}

    void OnChat(Player* player, uint32 /*type*/, uint32 lang,
                std::string& msg, Player* receiver) override
    {
        if (lang != LANG_ADDON && lang != LANG_ADDON_LOGGED) return;
        // OnChat doesn't carry the prefix — AddonControl filters on body
        // shape ("1|<seqHex>|...|MTYPE|"). Pass empty prefix.
        Playerbot::V2::AddonControl::OnAddonWhisper(
            player, receiver, /*prefix=*/ std::string{}, msg);
    }
};

void AddSC_playerbot_v2_commandscript()
{
    new playerbot_v2_commandscript();
    new playerbot_v2_addon_listener();
}

#else

void AddSC_playerbot_v2_commandscript() {}

#endif // TRINITY_PLAYERBOT_V2
