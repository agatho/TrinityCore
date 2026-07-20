// State_InCombat - Combat dispatch. Looks up the (class, spec) APL from the
// rotation registry and runs it. If no rotation is registered, the bot just
// engages auto-attack on its current target — never freezes.

#include "StateBase.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/ClassTables.h"                     // ClassOocHeal — combat:pve_heal_focus
#include "Bot/Dungeon/DungeonScript.h"          // DungeonAdvice — DungeonCombatPositioning (BUG G-P0a)
#include "Bot/Battleground/BattlegroundScript.h" // BattlegroundAdvice — BgCarrierHomeward (BUG BG-P0a)
#include "Bot/States/MaintainHelpers.h"          // DungeonCombatPositioning / BgCarrierHomeward
#include "Group/GroupSnapshot.h"
#include "Combat/ApRegistry.h"
#include "Combat/ApRotation.h"
#include "../Services.h"                          // Services::Dungeons().GetAdvice in combat path
#include "Util/ConfigReader.h"                    // full type for Services::Config() (combat_skip_unfightable)
#include "Travel/RepairVendorIndex.h"             // Services::RepairVendors() for critical-gear flee
#include "Travel/QuestHubDatabase.h"              // Services::Hubs() hub fallback for flee_to_repair (start-island escape)
#include "RaceMask.h"
#include "SharedDefines.h"
#include "Log.h"                                   // TC_LOG_INFO — TEMP AV push diag
#include <limits>
#include <cmath>

namespace Playerbot::States {

namespace {

// Returns true if the (class, spec) pair fights primarily in melee range.
// Used to decide whether to emit a gap-close MoveTo when victim is out of
// reach. Hunter Survival is the only melee Hunter spec.
bool IsMeleeSpec(uint8 cls, uint32 spec)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_ROGUE:
        case CLASS_DEMON_HUNTER:
        case CLASS_MONK:
            return true;
        case CLASS_HUNTER:
            return spec == 255;          // Survival
        case CLASS_DRUID:
            return spec == 103 || spec == 104; // Feral / Guardian
        case CLASS_SHAMAN:
            return spec == 263;          // Enhancement
        default:
            return false;
    }
}

// True when an attacker/victim can actually be fought. Two ingredient sets:
//  - flags: untargetable (UNIT_FLAG_UNINTERACTIBLE), pacified
//    (UNIT_FLAG_PACIFIED) or already dead — the exact tally BotSnapshot-
//    Builder counts into fightable_attackers (read as tank_fightable by
//    GroupWedgeWatchdog, fightable_attackers_count() by State_Idle). These
//    are static/scripted authoring flags (triggers, dummies) and never
//    encode path-unreachability.
//  - cannot_reach: Creature::CanNotReachTarget() — the mob's OWN Chase-
//    MovementGenerator reports it cannot path to its target (maintained
//    per-tick by TC on !isInAccessiblePlaceFor / NOPATH). TRUE precisely
//    during the aggro-but-unreachable window: the WC corridor freeze pack
//    (2026-07-03) is ordinary targetable creatures on a z-disconnected
//    ledge — flags read fightable — yet chasing it can never close and only
//    steals movement ownership from dungeon navigation every combat frame.
// Both are cheap snapshot boolean reads — no new per-tick pathfind. Caveat:
// cannot_reach covers the mob-cannot-reach-bot direction (the diagnosed
// case); the asymmetric bot-cannot-reach-mob case is handled by the
// pull-gate / disengage machinery, not here.
bool IsAttackerFightable(NearbyUnit const& u)
{
    return !u.untargetable && !u.is_pacified && u.hp > 0 && !u.cannot_reach;
}

// Names which fightable ingredient failed, for the [skip_unfightable] diag:
// "flags" = untargetable/pacified/dead, "cannot_reach" = the mob's own chase
// generator reports it cannot path to us.
char const* UnfightableReason(NearbyUnit const& u)
{
    if (u.untargetable || u.is_pacified || u.hp <= 0)
        return "flags";
    return "cannot_reach";
}

// Throttled [skip_unfightable] diag — ONE log site for both re-aim gates
// below (melee gap-close + victimless self-acquire) so the WC corridor
// freeze forensics can grep a single tag instead of chasing per-site
// duplicates (mirrors the [step_hold]/[adv_route] throttle pattern already
// used in this module).
void DiagSkipUnfightable(BotSnapshotView const& s, ObjectGuid victim,
                         char const* reason)
{
    static uint32 s_skip_unfightable_dbg_ms = 0;
    const uint32 now = s.published_at_ms();
    if (now - s_skip_unfightable_dbg_ms > 1500u)
    {
        s_skip_unfightable_dbg_ms = now;
        TC_LOG_INFO("playerbot.v2", "[skip_unfightable] bot={} victim={} reason={}",
                    s.bot_id(), victim.GetCounter(), reason);
    }
}

// Emergency-survival item check: use Healthstone or a healing potion when
// HP gets dangerously low and no defensive CD has fired. Tries items in
// priority order (Healthstone first — instant + free) and returns true if
// an item was emitted. Both items share the global Potion cooldown so we
// only fire one per emergency.
//
// Item IDs validated against WoW 12.0 client. has_item gates so we don't
// emit for items the bot doesn't carry.
bool MaybeUseSurvivalItem(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    if (s.hp_pct() > 30) return false;
    // Healthstones are instant but cancel any in-progress channel/cast. A
    // healer mid-Heal at 25% HP shouldn't interrupt themselves to pop a
    // potion — let the cast land first.
    if (s.is_casting()) return false;

    // Healthstone — instant, no GCD
    constexpr uint32 kHealthstoneItem = 5512;
    if (s.has_item(kHealthstoneItem))
    {
        emit.emit(UseItemByEntryIntent{kHealthstoneItem, ObjectGuid::Empty});
        return true;
    }
    // Algari Healing Potion (WoW 12.0). Older potions also resolve via
    // the same use-item path; the API returns InvalidTarget if missing.
    constexpr uint32 kAlgariHealingPotion = 211880;
    if (s.has_item(kAlgariHealingPotion))
    {
        emit.emit(UseItemByEntryIntent{kAlgariHealingPotion, ObjectGuid::Empty});
        return true;
    }
    return false;
}

// Defensive racial: Stoneform (Dwarf) instantly clears bleed / poison /
// disease / curse and grants +30% armor for 8s. Fires when the bot is
// carrying a clearable debuff. Returns true if a racial was cast.
bool MaybeUseDefensiveRacial(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    // Dwarf: Stoneform clears bleed/poison/disease/curse + 30% armor for 8s.
    if (s.race() == RACE_DWARF)
    {
        constexpr uint32 STONEFORM = 20594;
        if (s.knows_spell(STONEFORM) && s.is_ready(STONEFORM))
        {
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                switch (a.dispel_type)
                {
                    case DispelType::Bleed:
                    case DispelType::Poison:
                    case DispelType::Disease:
                    case DispelType::Curse:
                        emit.cast(STONEFORM);
                        return true;
                    default: break;
                }
            }
        }
    }
    // Undead: Will of the Forsaken instantly breaks fear/charm/sleep. Used
    // reactively — most fears in PvE are mind-control phases on bosses, but
    // the racial pays for itself even on a single break per encounter.
    if (s.race() == RACE_UNDEAD_PLAYER)
    {
        constexpr uint32 WILL_OF_FORSAKEN = 7744;
        if (s.knows_spell(WILL_OF_FORSAKEN) && s.is_ready(WILL_OF_FORSAKEN))
        {
            // SpellInfo::Mechanic on the harmful aura tells us what it is.
            // 5=Fear, 6=Disorient (sleep / horror lump in here in 12.0),
            // 8=Charm. Trigger on any of those.
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                if (a.mechanic == 5 /*Fear*/ ||
                    a.mechanic == 6 /*Disorient*/ ||
                    a.mechanic == 8 /*Charm*/)
                {
                    emit.cast(WILL_OF_FORSAKEN);
                    return true;
                }
            }
        }
    }
    // Draenei: Gift of the Naaru — instant 20% HP HoT over 5s. Fires when
    // bot is at 50% HP or below and not currently casting (we don't want to
    // interrupt our own heal). The HoT stacks with normal healing so it's
    // pure topup, no overheal waste.
    if (s.race() == RACE_DRAENEI || s.race() == RACE_LIGHTFORGED_DRAENEI)
    {
        constexpr uint32 GIFT_OF_THE_NAARU = 59548;
        if (s.knows_spell(GIFT_OF_THE_NAARU) && s.is_ready(GIFT_OF_THE_NAARU)
            && s.hp_pct() <= 50 && !s.is_casting())
        {
            emit.cast(GIFT_OF_THE_NAARU);
            return true;
        }
    }
    // Gnome: Escape Artist — instant root/snare break. Fires when the bot
    // carries a movement-impair debuff. Mechanic 7=Snare, 18=Root.
    if (s.race() == RACE_GNOME)
    {
        constexpr uint32 ESCAPE_ARTIST = 20589;
        if (s.knows_spell(ESCAPE_ARTIST) && s.is_ready(ESCAPE_ARTIST))
        {
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                if (a.mechanic == 7 /*Snare*/ || a.mechanic == 18 /*Root*/)
                {
                    emit.cast(ESCAPE_ARTIST);
                    return true;
                }
            }
        }
    }
    // Worgen: Darkflight — instant 40% movement speed for 10s. Fires when
    // HP is critical so the bot can attempt to disengage / kite. Only when
    // not already mounted (mounting beats it) and not currently in melee
    // (running away mid-melee just feeds boss damage; for casters / ranged
    // it buys time to reposition).
    if (s.race() == RACE_WORGEN)
    {
        constexpr uint32 DARKFLIGHT = 68992;
        if (s.knows_spell(DARKFLIGHT) && s.is_ready(DARKFLIGHT)
            && s.hp_pct() <= 30 && !s.is_mounted())
        {
            emit.cast(DARKFLIGHT);
            return true;
        }
    }
    // Night Elf: Shadowmeld — instant stealth + threat drop. The threat
    // drop is the headline feature here: when bot HP critical and we're
    // the highest threat target, dropping aggro lets the tank pick the
    // mob back up. Out of combat the bot can use it for stealth-walk by
    // shadowing the leader; that lives in State_InGroup.
    if (s.race() == RACE_NIGHTELF)
    {
        constexpr uint32 SHADOWMELD = 58984;
        if (s.knows_spell(SHADOWMELD) && s.is_ready(SHADOWMELD)
            && s.hp_pct() <= 25 && !s.is_casting())
        {
            // Skip if no nearby attackers (drop-aggro doesn't help solo).
            if (!s.raw().combat.attackers.empty())
            {
                emit.cast(SHADOWMELD);
                return true;
            }
        }
    }
    // Human: Every Man for Himself — instant break of stun/root/silence/fear/
    // charm/horror/disorient (PvP trinket equivalent). Triggers on any
    // gameplay-blocking debuff.
    if (s.race() == RACE_HUMAN)
    {
        constexpr uint32 EVERY_MAN_FOR_HIMSELF = 59752;
        if (s.knows_spell(EVERY_MAN_FOR_HIMSELF) && s.is_ready(EVERY_MAN_FOR_HIMSELF))
        {
            // Trigger on any blocking mechanic. Stun=10, Silence=15, Root=18,
            // Sap=21, all share the "we cannot act/cast" property.
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                switch (a.mechanic)
                {
                    case 5: case 6: case 7: case 8: case 10:
                    case 11: case 12: case 14: case 15: case 18:
                    case 19: case 21: case 23:
                        emit.cast(EVERY_MAN_FOR_HIMSELF);
                        return true;
                    default: break;
                }
            }
        }
    }
    return false;
}

// Trinket on-use auto-fire. Trinkets without an ON_USE effect (most older
// procs and stat sticks) leave on_use_spell_id at 0 in the snapshot — the
// view helper skips those slots. Strict gates: actively in combat with a
// live victim, not currently casting (don't interrupt), one trinket per
// tick (the view picks the first ready of TRINKET1 then TRINKET2).
bool MaybeUseOnUseTrinket(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    if (!s.in_combat() || s.victim().IsEmpty()) return false;
    if (s.is_casting()) return false;
    const uint32 sid = s.ready_on_use_trinket();
    if (sid == 0) return false;
    // PvP-aware gate: real players hoard the trinket for CC-break. If the
    // bot is fighting players (any attacker is a Player), only fire when
    // *actively* CC'd. The CC check walks own_auras for any harmful
    // mechanic (stun=10, fear=5, poly=10/disorient=6, silence=15, root=18,
    // charm=8, sleep=14, horror=24, incapacitate=16, sap=21, freeze=11) —
    // i.e. anything a Medallion of the Adventurer would remove. Without
    // this gate the bot blows a 2-5min CD on the first PvE-style offensive
    // proc and then has nothing left when the inevitable fear/poly lands.
    if (s.under_player_attack())
    {
        bool cc_active = s.is_stunned() || s.is_silenced() || s.is_rooted();
        if (!cc_active)
        {
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                switch (a.mechanic)
                {
                    case 5:  case 6:  case 8:  case 10: case 11:
                    case 12: case 14: case 15: case 16: case 18:
                    case 19: case 21: case 23: case 24:
                        cc_active = true;
                        break;
                    default: break;
                }
                if (cc_active) break;
            }
        }
        if (!cc_active)
            return false;
    }
    // Most trinkets self-buff (ObjectGuid::Empty target); a few target enemy
    // (e.g. damage-dealing trinkets). cast() with no target lets Spell.cpp
    // pick implicit targets from the spell info.
    emit.cast(sid);
    return true;
}

// Race-appropriate offensive cooldown. Fired once per cooldown cycle while
// engaged in combat — these racials are pure DPS pulses with no friend-fire
// or off-GCD penalty. Returns true if a racial was emitted (this tick is
// then spent on it).
bool MaybeUseOffensiveRacial(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    if (!s.in_combat() || s.victim().IsEmpty()) return false;

    // Spell IDs validated against WoW 12.0 client.
    constexpr uint32 BERSERKING       = 26297;     // Troll
    constexpr uint32 BLOOD_FURY       = 33697;     // Orc (melee+caster unified)
    constexpr uint32 ANCESTRAL_CALL   = 274738;    // Mag'har Orc
    constexpr uint32 FIREBLOOD        = 265221;    // Dark Iron Dwarf
    constexpr uint32 BAG_OF_TRICKS    = 312411;    // Vulpera
    constexpr uint32 ARCANE_TORRENT_MELEE = 25046; // Blood Elf (melee variant)
    constexpr uint32 ARCANE_TORRENT_CASTER = 28730;// Blood Elf (caster, mana variant)

    // Bot::Race enum follows TrinityCore SharedDefines RACE_* values.
    switch (s.race())
    {
        case RACE_TROLL:           if (s.knows_spell(BERSERKING)     && s.is_ready(BERSERKING))     { emit.cast(BERSERKING);     return true; } break;
        case RACE_ORC:             if (s.knows_spell(BLOOD_FURY)     && s.is_ready(BLOOD_FURY))     { emit.cast(BLOOD_FURY);     return true; } break;
        case RACE_MAGHAR_ORC:      if (s.knows_spell(ANCESTRAL_CALL) && s.is_ready(ANCESTRAL_CALL)) { emit.cast(ANCESTRAL_CALL); return true; } break;
        case RACE_TAUREN:
        {
            // War Stomp: 8yd PB AoE 2s stun + 5yd interrupt. Best when 3+
            // melee on us — the stun shaves a global off everyone.
            constexpr uint32 WAR_STOMP = 20549;
            if (s.knows_spell(WAR_STOMP) && s.is_ready(WAR_STOMP))
            {
                if (s.attackers_count() >= 3)
                {
                    emit.cast(WAR_STOMP);
                    return true;
                }
            }
        } break;
        case RACE_HIGHMOUNTAIN_TAUREN:
        {
            // Bull Rush: charges forward 25yd + 1s stun on enemies. Used as
            // a gap-closer for melee bots when the victim is out of range.
            constexpr uint32 BULL_RUSH = 255654;
            if (s.knows_spell(BULL_RUSH) && s.is_ready(BULL_RUSH))
            {
                if (NearbyUnit const* v = s.target_info())
                {
                    float bx, by, bz; s.position(bx, by, bz);
                    const float dx = v->x - bx, dy = v->y - by, dz = v->z - bz;
                    const float d2 = dx*dx + dy*dy + dz*dz;
                    if (d2 > 7.0f * 7.0f && d2 < 25.0f * 25.0f)
                    {
                        emit.cast(BULL_RUSH);
                        return true;
                    }
                }
            }
        } break;
        case RACE_BLOODELF:
        {
            // Arcane Torrent: 8yd PB AoE silence + small resource refund.
            // The melee variant returns 15 energy/runic-power; caster variant
            // returns 1.5% mana. Both versions silence enemy casters in 8yd —
            // most useful for interrupting boss casts when our normal kick is
            // on CD. Trigger on a nearby interruptible cast.
            const uint32 sid = s.knows_spell(ARCANE_TORRENT_CASTER) ? ARCANE_TORRENT_CASTER
                             : s.knows_spell(ARCANE_TORRENT_MELEE)  ? ARCANE_TORRENT_MELEE
                             : 0;
            if (sid && s.is_ready(sid))
            {
                // Find an interruptible caster within 8yd of the bot.
                if (NearbyUnit const* tgt = s.interruptible_caster())
                {
                    float bx, by, bz; s.position(bx, by, bz);
                    const float dx = tgt->x - bx, dy = tgt->y - by, dz = tgt->z - bz;
                    if (dx*dx + dy*dy + dz*dz <= 8.0f * 8.0f)
                    {
                        emit.cast(sid);
                        return true;
                    }
                }
            }
        } break;
        case RACE_DARK_IRON_DWARF: if (s.knows_spell(FIREBLOOD)      && s.is_ready(FIREBLOOD))      { emit.cast(FIREBLOOD);      return true; } break;
        case RACE_VULPERA:         if (s.knows_spell(BAG_OF_TRICKS)  && s.is_ready(BAG_OF_TRICKS))  { emit.cast(BAG_OF_TRICKS,   s.victim()); return true; } break;
        default: break;
    }
    return false;
}

// Mana refill: use a mana potion when a caster is below 25% mana and
// running low on pressure. Shares the Potion cooldown with healing potions
// so the API will refuse if one is already on CD — that's fine, no harm.
bool MaybeUseManaPotion(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    // POWER_MANA = 0. Non-mana classes have max_power[0] == 0 and pct stays 0.
    if (s.max_power(0) <= 0) return false;
    if (s.power_pct(0) > 25) return false;
    if (s.is_casting()) return false;     // don't interrupt a heal mid-cast

    constexpr uint32 kAlgariManaPotion = 212265;
    if (s.has_item(kAlgariManaPotion))
    {
        emit.emit(UseItemByEntryIntent{kAlgariManaPotion, ObjectGuid::Empty});
        return true;
    }
    constexpr uint32 kRefreshingManaPotion = 191383;
    if (s.has_item(kRefreshingManaPotion))
    {
        emit.emit(UseItemByEntryIntent{kRefreshingManaPotion, ObjectGuid::Empty});
        return true;
    }
    return false;
}

} // anonymous

void DispatchInCombat(BotAI& ai,
                      BotSnapshotView snapshot,
                      GroupSnapshotView group,
                      BotIntentEmitter& emit)
{
    ApPredicateContext ctx{ snapshot, group, ai.aoe_preference(), {} };
    // PvP context — populated for BG/arena ticks. Default-constructed is
    // PvE-mode (all false / empty); PvP-aware predicates gate on
    // ctx.pvp.in_battleground first. Sourcing fields from the snapshot
    // is cheap and per-tick — APL evaluation runs once per combat tick.
    ctx.pvp.in_battleground       = snapshot.in_battleground();
    // in_arena left default false — no direct accessor; arena scripts
    // can branch on bg_type_id ∈ {4,5,6,8,10,11,719,757,808,816,...}
    // if needed. Most PvP-aware predicates don't distinguish arena
    // from BG.
    ctx.pvp.friendly_flag_carrier = snapshot.bg_friendly_flag_carrier();
    ctx.pvp.enemy_flag_carrier    = snapshot.bg_enemy_flag_carrier();
    ctx.pvp.under_player_attack   = snapshot.under_player_attack();
    // bg_role from the snapshot (BG audit N57): the builder publishes the
    // same rank-permutation + fixup role BgDispatch resolves, so the
    // combat APL is no longer role-blind (healer-spec rules can gate on
    // Healer role, FC openers on FlagCarrier, etc.).
    ctx.pvp.bg_role = snapshot.raw().bg.bg_role;

    // BG-orphan disengage. When a battleground ends, the core removes every
    // player and teleports them out — but the exit teleport can fail
    // transiently (player loading / mid-teleport) and the Battleground
    // object is deleted regardless. The bots left behind kept fighting
    // each other FOREVER on the dead map (live 2026-06-11: 3 bots, 15+ min
    // of StartAttack every tick on post-match map 566), and because they
    // never left combat, no idle recovery could reach them. Detect the
    // orphan signature (BG/arena map + no Battleground claims us) and stop
    // attacking; once everyone disengages, combat drops and
    // idle:bg_orphan_escape ports the bot home.
    if (snapshot.is_bg_orphan())
    {
        emit.stop_attack();
        ai.set_last_rule_fired("combat:bg_orphan_disengage");
        return;
    }

    // Dismount FIRST when in combat. Modern TC / WoW 12.x doesn't reliably
    // auto-dismount on damage for all mount types (ground mounts, some
    // legacy mounts, transmogged mounts). A mounted bot can't cast most
    // spells — without dismounting it just sits there while enemies kill
    // it. Fire one dismount per combat-entry; next tick the snapshot
    // reflects is_mounted=false and the rotation runs normally.
    if (snapshot.is_mounted())
    {
        emit.dismount();
        ai.set_last_rule_fired("combat:dismount");
        return;
    }

    // ---- Ghost-combat self-heal ----
    // A wedged combat flag: in combat 10s+ with NO victim, NO attackers,
    // not casting, and no nearby enemy fighting the group. TC's threat
    // bookkeeping can strand a combat ref against an unreachable/evaded
    // mob (2026-06-12 Stockades: healer InCombat=true for 30+ min at full
    // HP/mana, victim empty, attackers 0 — the tank's member-in-combat
    // advance gate held the entire run hostage until a GM combatstop).
    // The legitimate "healer in combat with no victim" case is excluded
    // by the enemy-fighting-the-group scan: real fights have mobs whose
    // victim is a member (or the bot's pet). clear_ghost_combat drops the
    // PvE refs server-side, guarded there on an empty attacker list.
    if (snapshot.victim().IsEmpty() &&
        snapshot.raw().combat.attackers.empty() &&
        !snapshot.is_casting() &&
        snapshot.combat_duration_ms() >= 10'000)
    {
        bool group_engaged = false;
        const ObjectGuid mypet = snapshot.pet_guid();
        for (auto const& u : snapshot.raw().combat.nearby_enemies)
        {
            if (u.victim.IsEmpty() || u.hp <= 0) continue;
            if (u.victim == snapshot.raw().guid ||
                (!mypet.IsEmpty() && u.victim == mypet))
            { group_engaged = true; break; }
            if (group.exists())
                if (auto const* mems = group.members())
                    for (auto const& m : *mems)
                        if (m.guid == u.victim) { group_engaged = true; break; }
            if (group_engaged) break;
        }
        if (!group_engaged)
        {
            emit.stop_attack(/*clear_ghost_combat*/ true);
            ai.set_last_rule_fired("combat:ghost_combat_clear");
            return;
        }
    }

    // ---- Critical-durability flee to repair (soft-lock break) ----
    // Gear at <=20% durability does almost no weapon damage, so the bot can
    // NEITHER kill nor die quickly — it churns in one combat forever (observed:
    // Bramwell, 155s+ on quest 26389 at 0% durability, never progressing, never
    // OOC to repair). The per-victim disengages below are defeated by target-
    // switching within a pack (their victim_watch resets on each switch) AND are
    // gated on a resolved victim/quest-target; this one keys off total combat
    // DURATION and fires UNCONDITIONALLY at top level (no victim dependency),
    // EVEN on quest targets — a kill the bot physically cannot land isn't worth
    // protecting. Flee TOWARD a known repair vendor so the pack leashes (combat
    // drops) and the trip makes progress; idle:critical_repair takes over once
    // OOC. Gated out in instance/BG/group (coordinated fights recover
    // differently). Routine <30%/<35% repairs stay on the OOC vendor pipeline.
    // Trigger on EITHER a long unwinnable CHURN (>60s — the original Bramwell case,
    // where 0% gear can neither kill nor die) OR a LOW-HP losing fight (<=40% HP —
    // the FAST-DEATH case). A broken-gear bot that is being killed in well under 60s
    // never reached the duration gate, so it fell through to a pure DEATH SPIRAL:
    // die -> resurrect -> re-engage with 0% gear -> die, never OOC long enough for
    // idle:critical_repair to run (observed: Zekani, L3 Troll Hunter on Echo Isles,
    // repeated [release_corpse] with zero repair attempts). Fleeing at 40% HP gives
    // the bot a chance to leash the pack and reach a repair target BEFORE dying.
    {
        auto const& v = snapshot.raw().vitals;
        const bool low_hp = v.max_hp > 0 && uint64(v.hp) * 100u <= uint64(v.max_hp) * 40u;
        const uint8 dura = snapshot.lowest_equipped_durability_pct();
        // A bot whose gear is DESTROYED (≤5%) deals essentially no weapon damage and
        // cannot win ANY fight — yet at full HP it also never dies, so the >60s-churn
        // and <40%-HP triggers below can both miss (combat_duration resets when the
        // unkillable mob's pack switches targets). That leaves it auto-attacking a mob
        // it can't kill, forever, never breaking off to repair (observed: Morthan, L9,
        // 0% gear, looping "Auto attack" at 100% HP in the Ruins of Lordaeron). When
        // gear is destroyed, fleeing to repair is ALWAYS correct — fire unconditionally.
        if (!snapshot.is_in_instance() && !snapshot.in_battleground() &&
            !group.exists() &&
            dura <= 20 &&
            (dura <= 5 || snapshot.combat_duration_ms() > 60000 || low_hp))
        {
            // Prefer a known repair vendor; fall back to the nearest SAME-MAP quest
            // hub (hubs carry repair vendors). The hub fallback also doubles as the
            // starter-island ESCAPE: a bot stranded on a no-known-vendor starting
            // island (Echo Isles / Ammen Vale) flees toward its next hub (e.g.
            // Sen'jin), which is exactly where it must relocate to continue questing
            // AND find repair — without it, CriticalRepairGate's same-map-hub branch
            // is the only repair path and it never runs because the death loop
            // starves State_Idle. Mirrors the idle gate's vendor/hub fallback chain.
            float tx = 0.f, ty = 0.f; bool have_target = false;
            if (auto hit = Services::RepairVendors().GetNearestRepairVendor(snapshot.raw()))
            {
                tx = hit->x; ty = hit->y; have_target = true;
            }
            else if (auto const* hub = Services::Hubs().GetNearestQuestHub(snapshot.raw()))
            {
                if (hub->mapId == snapshot.map_id())
                {
                    tx = hub->location.GetPositionX();
                    ty = hub->location.GetPositionY();
                    have_target = true;
                }
            }
            if (have_target)
            {
                emit.stop_attack();
                // Call the PET off too. A pet still locked on a target keeps the
                // OWNER flagged in-combat (PetAI threat linkage), so a 0%-gear bot
                // whose pet keeps fighting never goes OOC and never reaches the
                // vendor — it just death-spirals (observed: Morthan, Imp "Azlop"
                // combat=true, owner InCombat 54min at full-then-dropping HP with
                // EMPTY victim / 0 attackers, ghost_combat_clear skipped because the
                // pet's target reads as group-engaged). FOLLOW (AttackStop + recall)
                // + PASSIVE (don't re-engage) drops the pet's combat so the owner's
                // clears and the flee/repair can actually proceed.
                if (!snapshot.pet_guid().IsEmpty())
                {
                    emit.pet_set_react_state(0 /*REACT_PASSIVE*/);
                    emit.pet_set_command_state(1 /*COMMAND_FOLLOW*/);
                }
                float bx, by, bz; snapshot.position(bx, by, bz);
                // LEASH-BREAK OVERRIDE for a PROLONGED stalemate. When combat has
                // already run >60s the flee-toward-vendor is demonstrably NOT
                // breaking it — typically because the chosen repair target sits
                // PAST the mob pack (so fleeing toward it re-aggros) or the nearest-
                // vendor pick oscillates between far candidates each tick, leaving
                // the bot jittering in place (observed: Morthan, 0% gear, 14-MIN
                // continuous combat at the Ruins of Lordaeron — advancing paths in
                // OPPOSITE directions every tick, net-zero movement, never OOC).
                // A 0%-gear bot at 100% HP can neither kill nor die, so it churns
                // forever. Flee a STABLE direction directly AWAY from the enemy
                // centroid (deterministic, never toward the pack) to break the leash;
                // once OOC, idle:critical_repair routes to the vendor. The normal
                // (short-combat) case still flees toward the vendor/hub below.
                float fx = tx, fy = ty;
                if (snapshot.combat_duration_ms() > 60000)
                {
                    double cx = 0.0, cy = 0.0; uint32 n = 0;
                    for (auto const& e : snapshot.raw().combat.nearby_enemies)
                    {
                        if (e.hp <= 0) continue;
                        cx += e.x; cy += e.y; ++n;
                    }
                    if (n > 0)
                    {
                        cx /= n; cy /= n;
                        float ax = bx - float(cx), ay = by - float(cy);
                        const float al = std::sqrt(std::max(ax*ax + ay*ay, 0.0001f));
                        constexpr float kBreak = 60.0f;   // clear typical leash range
                        fx = bx + ax / al * kBreak;
                        fy = by + ay / al * kBreak;
                    }
                }
                // Full-path so PathGenerator routes around geometry and the bot
                // leashes the pack as it gains distance (idle:critical_repair, also
                // full-path, finishes the trip once OOC). Replaces the old 15y greedy
                // hop that wedged on terrain / ran the bot into the very pack it fled.
                emit.move_to(fx, fy, bz, /*run*/ true);
                ai.set_last_rule_fired("combat:flee_to_repair");
                return;
            }
        }
    }

    // ---- Zero-yield combat watchdog ----
    // (a) No-XP victim (Training Dummy etc.): the target is attackable but
    //     never dies and awards nothing — a bot that opens on one stays
    //     InCombat FOREVER, suppressing all questing/travel (observed live:
    //     L22 + L34 bots wedged on Razor Hill dummies for hours). Disengage
    //     immediately unless it is the bot's actual quest target (a few
    //     quests require hitting no-XP props).
    // (b) No-progress: same victim, HP not dropping for 45s outside
    //     instances (evade-wedged mob, perma-immune prop) — disengage.
    //     Instances excluded: scripted immune phases are normal there.
    {
        const ObjectGuid vg = snapshot.victim();
        if (!vg.IsEmpty())
        {
            NearbyUnit const* vu = nullptr;
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == vg) { vu = &u; break; }
            // ---- Invalid / unscannable victim (vu == null) ----
            // The bot is "attacking" a victim that is NOT in nearby_enemies. The
            // hostile scan only lists units that pass Player::IsValidAttackTarget
            // (BotSnapshotBuilder), so a victim missing from it is one we can deal
            // no real damage to: an immune / not-at-war / pacified prop such as a
            // Training Dummy (entry 44820 — its PACIFIED/NO_XP data is wrong, so
            // the flag filters never caught it, AND it never enters nearby_enemies,
            // so EVERY vu-based disengage below was silently skipped and the bot
            // wedged InCombat forever firing at a target it cannot kill). A genuine
            // elite / rare / world boss we can validly fight DOES appear in
            // nearby_enemies (vu != null), so this branch can never abandon a real
            // kill. Disengage once we've been stuck on the same unscannable,
            // non-quest creature for a short window, with a long engage shield so
            // the pickers don't re-acquire it. Quest target matched by the entry
            // encoded in the victim GUID (no template lookup needed off-thread).
            if (!vu && vg.IsCreature() && !snapshot.is_in_instance() && !group.exists())
            {
                const uint32 now_ms_iv = snapshot.published_at_ms();
                bool qt_iv = false;
                if (snapshot.has_current_objective())
                {
                    auto const& obj = snapshot.current_objective();
                    if (obj.type == /*MONSTER*/ 0)
                    {
                        if (uint32(obj.object_id) == vg.GetEntry()) qt_iv = true;
                        for (uint32 a : obj.credit_alias_entries)
                            if (a == vg.GetEntry()) { qt_iv = true; break; }
                    }
                }
                if (!qt_iv)
                {
                    constexpr uint32 kInvalidVictimMs = 20000u;   // 20s is ample — nothing valid stays unscannable
                    if (ai.combat_stuck_victim() != vg)
                        ai.set_combat_stuck(vg, 0, now_ms_iv);
                    else if (now_ms_iv - ai.combat_stuck_since_ms() > kInvalidVictimMs)
                    {
                        emit.stop_attack();
                        ai.note_engage(vg, now_ms_iv, /*shield_ms*/ 5u * 60u * 1000u);
                        ai.set_combat_stuck(ObjectGuid::Empty, 0, 0);
                        ai.set_last_rule_fired("combat:disengage_invalid_victim");
                        return;
                    }
                }
            }
            if (vu)
            {
                const uint32 now_ms = snapshot.published_at_ms();
                bool quest_target = false;
                if (snapshot.has_current_objective())
                {
                    auto const& obj = snapshot.current_objective();
                    if (obj.type == /*MONSTER*/ 0)
                    {
                        if (uint32(obj.object_id) == vu->entry) quest_target = true;
                        for (uint32 a : obj.credit_alias_entries)
                            if (a == vu->entry) { quest_target = true; break; }
                    }
                }
                if ((vu->no_xp_kill || vu->is_pacified) && !quest_target)
                {
                    emit.stop_attack();
                    ai.note_engage(vg, now_ms);   // 8s re-engage shield
                    ai.set_last_rule_fired("combat:disengage_no_xp");
                    return;
                }
                // ---- Unkillable-target leash (flag-independent backstop) ----
                // The flag catch above relies on creature flags, which some
                // Training Dummies carry incorrectly (entry 44820: PACIFIED is on
                // the template's unit_flags but neither the live unit nor the
                // difficulty-merged template exposes it, so every flag read is
                // false). And the HP-not-dropping timer below is defeated by a
                // dummy whose HP oscillates (each landed hit resets it).
                //
                // The robust, flag-independent signal is HP RECOVERY: an immortal
                // dummy (and an evade-leashing mob the bot can't reach) RESETS to
                // full HP — its health jumps back UP while we fight it. A genuine
                // kill, even a slow one against a big HP pool, only ever trends
                // DOWN. So we track the MINIMUM HP seen on this victim and bail
                // only when its HP climbs far back above that floor — a condition
                // that can NEVER fire on a real fight, so it cannot abandon a
                // tough-but-killable mob. As an extra hard safety net we also
                // exempt Elite / RareElite / Rare (world bosses, rares, elite
                // quest mobs) and any group / instance fight outright: those are
                // legitimately long, high-HP, and sometimes self-heal. On a real
                // immortal we disengage with a LONG engage shield so the grind/
                // pull pickers don't re-acquire it, letting the bot resume
                // questing (which walks it out of range and breaks combat).
                if (!snapshot.is_in_instance() && !group.exists() &&
                    !quest_target && !vu->is_elite_or_rare && !vu->is_dungeon_boss)
                {
                    const int32 vmax = vu->max_hp > 0 ? vu->max_hp : 1;
                    // kStuckMs is a HARD time cap on a single NORMAL-mob fight.
                    // A normal mob dies in seconds even for an under-geared bot;
                    // 90s on one means it's immortal/evade-bugged (the Training
                    // Dummy 44820 case — it can't be flagged out because its
                    // PACIFIED/NO_XP data is wrong, and the HP-based detectors are
                    // defeated by its oscillating health). Reset ONLY on victim
                    // change so HP wobble can't keep postponing it. SAFE because
                    // elites/rares/world bosses/dungeon bosses and group fights
                    // are all exempted above — only trash mobs reach here, so this
                    // never abandons a legitimately long, high-HP kill.
                    constexpr uint32 kStuckMs = 90000u;
                    if (ai.combat_stuck_victim() != vg)
                    {
                        ai.set_combat_stuck(vg, vu->hp, now_ms);     // new victim: floor + start time
                    }
                    else if ((now_ms - ai.combat_stuck_since_ms() > kStuckMs) ||
                             (vu->hp >= ai.combat_stuck_hp() + vmax * 30 / 100))
                    {
                        // Either stuck too long on one trash mob, OR its HP
                        // recovered far above its low (reset to full) — immortal /
                        // evade-leashing, can never be killed. Disengage with a
                        // long engage shield so the grind / pull pickers don't
                        // re-acquire it; the bot then resumes questing, which walks
                        // it out of range and breaks combat.
                        emit.stop_attack();
                        ai.note_engage(vg, now_ms, /*shield_ms*/ 5u * 60u * 1000u);
                        ai.set_combat_stuck(ObjectGuid::Empty, 0, 0);
                        ai.set_last_rule_fired("combat:disengage_unkillable");
                        return;
                    }
                    else if (vu->hp < ai.combat_stuck_hp())
                    {
                        // Track a new HP low WITHOUT resetting the start time, so
                        // the time cap keeps counting through the fight.
                        ai.set_combat_stuck(vg, vu->hp, ai.combat_stuck_since_ms());
                    }
                }
                // ---- Instance-safe invulnerable watchdog ----
                // The HP-based watchdog above is gated to !is_in_instance() to
                // protect scripted immune phases (Ragnaros hammer, etc). But
                // encounter-mechanic creatures — fire platters, cosmetic props
                // — are truly invulnerable: they absorb ZERO player damage and
                // stay at 100% HP forever. A boss in an immune phase is NOT at
                // 100% HP; it took damage before the phase began. So "victim at
                // 100% HP for 30 consecutive seconds of attacks" is a safe proxy
                // for "truly invulnerable, never going to die" even in instances.
                // Disengage with a 2-minute shield so the fallback target picker
                // won't re-acquire it for the rest of the encounter.
                if (snapshot.is_in_instance() && !vu->is_dungeon_boss && !quest_target)
                {
                    const uint32 now_ms_inv = snapshot.published_at_ms();
                    const bool at_full_hp = vu->max_hp > 0 &&
                        vu->hp >= vu->max_hp - (vu->max_hp / 100);
                    if (at_full_hp)
                    {
                        if (ai.combat_stuck_victim() != vg)
                            ai.set_combat_stuck(vg, vu->hp, now_ms_inv);
                        else if (now_ms_inv - ai.combat_stuck_since_ms() > 30000u)
                        {
                            emit.stop_attack();
                            ai.note_engage(vg, now_ms_inv, /*shield_ms*/ 120u * 1000u);
                            ai.set_combat_stuck(ObjectGuid::Empty, 0, 0);
                            ai.set_last_rule_fired("combat:disengage_invulnerable");
                            return;
                        }
                    }
                    else if (ai.combat_stuck_victim() == vg)
                    {
                        // HP dropped — this is a real kill; reset tracking
                        ai.set_combat_stuck(ObjectGuid::Empty, 0, 0);
                    }
                }
                // ---- Disengage for broken gear (death-spiral break) ----
                // A bot with critically broken gear (≤10% durability — its stats
                // are gutted) that is LOSING a fight against a NON-mandatory
                // target should flee so it can repair, rather than feed the
                // death spiral. "Losing" = own HP trending into the danger band
                // OR an already-armed open-world spiral. Reuses the SAME
                // quest_target check as the no-XP branch (never abandon a
                // mandatory quest kill) and the SAME note_engage cooldown so it
                // can't thrash (8s shield + the engage rules skip the target).
                //
                // GATED OUT in instance / BG / group: never abandon a
                // coordinated fight — a group wipe is recovered by the group, not
                // by one bot fleeing.
                if (!snapshot.is_in_instance() && !snapshot.in_battleground() &&
                    !group.exists() &&
                    snapshot.lowest_equipped_durability_pct() <= 10 &&
                    !quest_target &&
                    (snapshot.hp_pct() <= 35 || ai.consecutive_same_spot_deaths() >= 1) &&
                    ai.last_engage_target() != vg)   // honor the existing shield
                {
                    emit.stop_attack();
                    ai.note_engage(vg, now_ms);   // 8s re-engage shield
                    ai.set_last_rule_fired("combat:disengage_broken_gear");
                    return;
                }
                if (!snapshot.is_in_instance())
                {
                    constexpr uint32 kNoProgressMs = 45000u;
                    if (ai.victim_watch_guid() != vg || vu->hp < ai.victim_watch_hp())
                        ai.set_victim_watch(vg, vu->hp, now_ms);
                    else if (now_ms - ai.victim_watch_since_ms() > kNoProgressMs)
                    {
                        emit.stop_attack();
                        ai.note_engage(vg, now_ms);
                        ai.set_victim_watch(ObjectGuid::Empty, 0, 0);
                        ai.set_last_rule_fired("combat:disengage_no_progress");
                        return;
                    }
                }

                // ---- LOS / range reposition (audit B04/B17) ----
                // 22% of all cast rejections were LINE_OF_SIGHT with ZERO
                // reaction: the snapshot computes per-unit in_los but no
                // combat rule consumed it, so a caster behind a pillar (or
                // a ranged bot whose target leashed beyond ~40y) spammed
                // doomed casts until the mob happened to wander. Step
                // TOWARD the victim when it is out of LOS or beyond casting
                // range — walking at the target breaks both conditions; the
                // melee gap-close handles the final approach as before.
                // (Open world + BATTLEGROUNDS: BG maps count as instances,
                // which silently disabled this for all PvP — ranged bots
                // stood still spamming doomed casts at kited / LOS-broken
                // players (BG audit N40). Dungeon/raid instances stay
                // excluded: hiding behind a pillar is often the MECHANIC
                // there and the dungeon positioning cascade owns movement.)
                if (!snapshot.is_in_instance() || snapshot.in_battleground())
                {
                    float bx, by, bz;
                    snapshot.position(bx, by, bz);
                    const float dx = vu->x - bx, dy = vu->y - by, dz = vu->z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    const bool out_of_range = dsq > 38.0f * 38.0f;
                    if (!vu->in_los || out_of_range)
                    {
                        // Wedge-guard (2026-06-15): stepping toward the victim can
                        // NEVER gain LoS when the target sits behind unreachable
                        // terrain / on a ledge above — the bot then steps into a
                        // wall forever (Somi stuck 180s at one spot on a quest-845
                        // target across a rise). The HP-based no-progress disengage
                        // above can't catch it: the bot never lands a hit to drop
                        // the victim's HP, so its watch timer never matures. When
                        // the reposition move is repeatedly path-blocked, disengage
                        // this victim so the bot re-targets or returns to its
                        // objective; the 8s note_engage shield stops an immediate
                        // re-pick of the same unreachable mob.
                        if (ai.check_anchor_wedge("combat:reposition_los",
                                                  snapshot.path_blocked_count(), now_ms))
                        {
                            emit.stop_attack();
                            ai.note_engage(vg, now_ms);
                            ai.set_victim_watch(ObjectGuid::Empty, 0, 0);
                            ai.set_last_rule_fired("combat:disengage_los_wedge");
                            return;
                        }
                        const float dist = std::sqrt(std::max(dsq, 1.0f));
                        const float step = std::min(10.0f, dist);
                        emit.move_to(bx + dx / dist * step,
                                     by + dy / dist * step,
                                     bz, /*run*/ true);
                        ai.set_last_rule_fired(!vu->in_los
                            ? "combat:reposition_los"
                            : "combat:reposition_range");
                        return;
                    }
                }
            }
        }
    }

    // ---- Healer STAY-IN-RANGE of the tank (Deadmines harbor, 2026-06-26) ----
    // ROOT CAUSE of the harbor wipe + the chronic dungeon cohesion lag: heal
    // spells cast at <=40y, but when the tank pulls beyond that (a fast boss-
    // advance, a descent into an aggro zone, the harbor stalker crossing) the
    // heal rotation still PICKS the tank and emits a heal that fails OutOfRange —
    // and the healer ROOTS itself re-casting the doomed heal every tick instead
    // of closing the gap. The tank then dies unhealed while the healer spams
    // OutOfRange (observed live: Dunghealer 46y back, 32 consecutive
    // CastSpell|OutOfRange, tank died holding at the harbor entrance at 0%). The
    // existing gap-close below is too narrow (only the SINGLE lowest member, only
    // <65%, only >45y) — a near hurt DPS as "lowest" leaves the far tank
    // un-followed. A real healer keeps the TANK in heal range first. So in a
    // dungeon, when the tank is beyond ~35y, run toward it BEFORE any cast.
    // Guarded by self-HP>40% so a near-death healer still self-heals instead of
    // chasing. This also closes the (0c) harbor cohesion gate quickly so the
    // group balls up and crosses together rather than the tank holding to death.
    if (snapshot.raw().group.my_role == Role::Healer &&
        snapshot.is_in_dungeon() && snapshot.hp_pct() > 40 && group.exists())
    {
        if (GroupMemberSummary const* tk = group.tank())
        {
            if (tk->online && tk->is_alive && tk->guid != snapshot.raw().guid &&
                tk->map_id == snapshot.map_id())
            {
                float bx, by, bz;
                snapshot.position(bx, by, bz);
                const float dx = tk->x - bx, dy = tk->y - by, dz = tk->z - bz;
                const float dsq = dx*dx + dy*dy + dz*dz;
                // Trigger only when the tank is BEYOND heal-cast range (~40y). At
                // <=40y the rotation's heals land, so following there would skip a
                // valid heal and STARVE the tank (a 35y trigger reduced gauntlet
                // healing and tipped a marginal Helix-pit fight into a wipe). Close
                // to ~30y (comfortably in range) so the next tick heals rather than
                // re-follows — no oscillation at the boundary.
                if (dsq > 41.0f * 41.0f)
                {
                    const float dist = std::sqrt(dsq);
                    const float step = std::min(25.0f, dist - 30.0f);
                    emit.move_to(bx + dx / dist * step,
                                 by + dy / dist * step,
                                 bz, /*run*/ true);
                    ai.set_last_rule_fired("combat:healer_follow_tank");
                    return;
                }
            }
        }
    }

    // ---- Healer gap-close (audit B23) ----
    // The heal pickers are now range-gated at 45y, so a wounded member
    // beyond that is no longer a rotation-wedging pick — but they'd be
    // IGNORED instead. A human healer runs toward the hurt teammate.
    // When a group member is genuinely hurt (<65%) on this map and beyond
    // the heal gate, step toward them. Mirrors the melee gap-close.
    if (snapshot.raw().group.my_role == Role::Healer && group.exists())
    {
        if (GroupMemberSummary const* low = group.lowest_hp_on_map(snapshot.map_id()))
        {
            const int32 low_pct = low->max_hp > 0 ? (low->hp * 100) / low->max_hp : 100;
            if (low_pct < 65 && low->guid != snapshot.raw().guid)
            {
                float bx, by, bz;
                snapshot.position(bx, by, bz);
                const float dx = low->x - bx, dy = low->y - by, dz = low->z - bz;
                const float dsq = dx*dx + dy*dy + dz*dz;
                // BG cap (audit N41): without an upper bound, a BG healer
                // abandoned its local fight and jogged map-wide toward
                // whichever raid member was lowest ANYWHERE (a 40-man AV
                // raid always has someone hurt 300y away). Beyond 80y in
                // a BG the wounded member belongs to a different fight —
                // stay with the local one.
                const float gap_max_sq = snapshot.in_battleground()
                    ? 80.0f * 80.0f
                    : std::numeric_limits<float>::max();
                if (dsq > 45.0f * 45.0f && dsq < gap_max_sq)
                {
                    const float dist = std::sqrt(dsq);
                    const float step = std::min(15.0f, dist - 40.0f);
                    emit.move_to(bx + dx / dist * step,
                                 by + dy / dist * step,
                                 bz, /*run*/ true);
                    ai.set_last_rule_fired("combat:healer_gap_close");
                    return;
                }
            }
        }
    }

    // Human reaction delay. Without this gate, the APL fires on the
    // SAME tick that snapshot.in_combat() first reads true — every
    // interrupt, panic CD, opener pops with sub-human precision (~50ms
    // from combat-start). Real humans react in 200-400ms. Per-bot
    // deterministic jitter (200-450ms) derived from bot_id keeps each
    // bot's reaction stable but de-synchronized from siblings, so a
    // raid of bots doesn't pop CDs in lockstep on the same world tick.
    // Auto-attack still runs server-side during this window; only
    // intent emission is gated.
    {
        const int64_t combat_ms = snapshot.combat_duration_ms();
        if (combat_ms > 0)
        {
            const uint32 jitter_ms =
                200u + (uint32(snapshot.bot_id()) * 1664525u) % 250u;
            if (combat_ms < int64_t(jitter_ms))
                return;
        }
    }

    // Carrier check — needed by the BG tactical layer below AND the
    // homeward/gap-close logic further down (hoisted from there).
    bool i_am_carrier = false;
    if (snapshot.in_battleground())
    {
        i_am_carrier = (snapshot.bg_friendly_flag_carrier() == snapshot.raw().guid);
        if (!i_am_carrier)
            for (auto const& fc : snapshot.bg_all_friendly_carriers())
                if (fc == snapshot.raw().guid) { i_am_carrier = true; break; }
    }

    // ---- PvE-coordinator kill focus ----
    // Synchronized add-burn: when the group plan designates a priority
    // kill target, every DPS switches to it TOGETHER instead of each bot
    // discovering the add on its own schedule (the old behavior smeared
    // the swap over several seconds and split damage across adds).
    // Hysteresis via PveTargetSwitch (8s per target) so the switch
    // doesn't re-emit every combat tick.
    if (!snapshot.in_battleground())
    {
        auto const& kf_po = snapshot.raw().pve_order;
        if (kf_po.active && !kf_po.kill_focus.IsEmpty() &&
            snapshot.is_alive() && !snapshot.is_casting() &&
            snapshot.victim() != kf_po.kill_focus)
        {
            for (auto const& u : snapshot.nearby_enemies())
            {
                if (u.guid != kf_po.kill_focus) continue;
                if (u.hp <= 0) break;       // corpse — next plan reassigns
                const uint32 kf_now = snapshot.published_at_ms();
                const uint64 kf_key = kf_po.kill_focus.GetCounter();
                if (!ai.action_recently_tried(
                        BotAI::ActionKind::PveTargetSwitch, kf_key, kf_now) &&
                    emit.start_attack(kf_po.kill_focus))
                {
                    ai.note_action_retry(BotAI::ActionKind::PveTargetSwitch,
                                         kf_key, kf_now);
                    ai.set_last_rule_fired("combat:pve_kill_focus");
                    return;
                }
                break;
            }
        }
    }

    // ---- BG tactical layer (BG audit N26/N27/N28/N39/N46) ----
    // Every PvP-tactical behavior used to live exclusively in State_Idle's
    // BgDispatch — structurally unreachable the moment a bot entered
    // combat, which in a BG is most of the match. State_InCombat fought
    // every battle as PvE: no focus-fire, no EFC kill, no peel, targets
    // only changed when the victim died, escorts wandered off mid-fight,
    // and a dropped flag two yards away was never clicked.
    if (snapshot.in_battleground())
    {
        const uint32 bg_now_ms = snapshot.published_at_ms();
        float bx, by, bz;
        snapshot.position(bx, by, bz);

        // (0) Siege-vehicle GATE fire in combat (BG audit IoC / SoTA). The
        // idle bg_vehicle_fire_gate rule is unreachable once the vehicle is
        // attacked — but a demolisher / siege engine ramming a DEFENDED gate
        // is in combat exactly when it most needs to fire. Mirror the idle
        // gate-fire: resolve this vehicle's seat spell and cast it at the
        // nearest STANDING target gate. Runs before the on-foot tactical
        // branches below (a seated bot has no melee/peel job).
        if (snapshot.on_vehicle())
        {
            BattlegroundAdvice const& adv = ai.bg_advice_cache().cached;
            uint32 seat_spell = 0;
            const uint32 ve = snapshot.vehicle_entry();
            if (ve != 0)
            {
                auto it = adv.vehicle_seat_spell_by_entry.find(ve);
                if (it != adv.vehicle_seat_spell_by_entry.end())
                    seat_spell = it->second;
            }
            if (seat_spell == 0) seat_spell = adv.vehicle_seat_spell;
            if (seat_spell != 0 && !adv.siege_target_go_entries.empty() &&
                !snapshot.is_casting())
            {
                BotSnapshot::NearbyObject const* gate = nullptr;
                float gate_dsq = 60.0f * 60.0f;
                for (auto const& o : snapshot.raw().world_objects.nearby_objects)
                {
                    if (o.go_type != /*DESTRUCTIBLE_BUILDING*/ 33) continue;
                    if (o.is_destroyed) continue;
                    bool match = false;
                    for (uint32 ge : adv.siege_target_go_entries)
                        if (o.entry == ge) { match = true; break; }
                    if (!match) continue;
                    const float dx = o.x - bx, dy = o.y - by;
                    const float dsq = dx * dx + dy * dy;
                    if (dsq < gate_dsq) { gate_dsq = dsq; gate = &o; }
                }
                if (gate)
                {
                    const uint64 gate_low = gate->guid.GetCounter();
                    if (!ai.action_recently_tried(BotAI::ActionKind::BgVehicleFire,
                                                  gate_low, bg_now_ms))
                    {
                        emit.cast_vehicle_at(seat_spell, gate->x, gate->y, gate->z);
                        ai.note_action_retry(BotAI::ActionKind::BgVehicleFire,
                                             gate_low, bg_now_ms);
                        ai.set_last_rule_fired("combat:bg_vehicle_fire_gate");
                        return;
                    }
                    // Gate in range, weapon on its short cooldown: HOLD here and
                    // keep battering rather than dropping to the tactical branches
                    // (a seated siege bot has no melee job anyway).
                    ai.set_last_rule_fired("combat:bg_vehicle_fire_gate_hold");
                    return;
                }
            }
        }

        // (1) Escort stickiness (N28): an FCEscort that drifted >40y from
        // its carrier mid-skirmish abandons the duel and returns — the
        // carrier dying alone loses the match, the road kill doesn't.
        // Team-coordinator orders override the legacy role test (N60
        // review): a bot ordered to DefendNode/HuntEFC/etc must NOT be
        // yanked back to the carrier by its hashed FCEscort role, and a
        // bot ORDERED to escort returns regardless of its hashed role.
        bool escort_duty;
        {
            auto const& ord = snapshot.raw().bg.order;
            escort_duty = ord.kind == BgState::BgOrder::None
                ? snapshot.raw().bg.bg_role == uint8(BgRole::FCEscort)
                : ord.kind == BgState::BgOrder::EscortFC;
        }
        if (!i_am_carrier && escort_duty &&
            !snapshot.bg_friendly_flag_carrier().IsEmpty() &&
            snapshot.bg_friendly_flag_carrier() != snapshot.raw().guid)
        {
            const float fcx = snapshot.raw().bg.friendly_carrier_x;
            const float fcy = snapshot.raw().bg.friendly_carrier_y;
            if (fcx != 0.f || fcy != 0.f)
            {
                const float dx = fcx - bx, dy = fcy - by;
                if (dx * dx + dy * dy > 40.0f * 40.0f)
                {
                    emit.follow(snapshot.bg_friendly_flag_carrier(), 8.0f);
                    ai.set_last_rule_fired("combat:bg_escort_return");
                    return;
                }
            }
        }

        // (2) Dropped-flag click (N26): NEW_FLAG_DROP (37) / FLAGDROP (26)
        // GOs are instant interactions worth more than any rotation tick —
        // returning our flag or denying theirs mid-fight is how humans
        // play CTF.
        for (auto const& o : snapshot.raw().world_objects.nearby_objects)
        {
            if (o.go_type != /*NEW_FLAG_DROP*/ 37 && o.go_type != /*FLAGDROP*/ 26)
                continue;
            const float dx = o.x - bx, dy = o.y - by;
            if (dx * dx + dy * dy > 64.0f) continue;  // 8y
            const uint64 go_low = o.guid.GetCounter();
            if (ai.action_recently_tried(BotAI::ActionKind::BgUseGo, go_low, bg_now_ms))
                continue;
            emit.use_game_object(o.guid);
            ai.note_action_retry(BotAI::ActionKind::BgUseGo, go_low, bg_now_ms);
            ai.set_last_rule_fired("combat:bg_grab_dropped_flag");
            return;
        }

        // (2b) Standing-objective interaction in combat (BG audit S1 — the
        // #1 "no bot picks up a flag" blocker). The enemy flag PEDESTAL
        // (NEW_FLAG 36), capture points (42), flagstands/orbs (24) and the
        // entry-enumerated banners (AV) are INSTANT server-side interactions
        // with NO combat restriction (Player::CanUseBattlegroundObject checks
        // only alive + faction + recently-dropped debuff). The old comment
        // here claimed "capture-point channels break on damage" — false for
        // modern instant caps. A defended WSG/TP flagroom or a contested
        // AB/BfG/EotS/AV node keeps the grabber in combat the entire time it
        // stands on the objective, so without an in-combat click the flag is
        // never lifted / the node never flips. Run the IDENTICAL scan the
        // idle dispatch uses (shared helper). Carriers are excluded — their
        // job is to run the flag home (handled by BgCarrierHomeward below),
        // not to re-click objects. Placed before the target-switch / gap-close
        // so securing the objective preempts the brawl.
        if (!i_am_carrier && snapshot.is_alive() && !snapshot.is_casting() &&
            States::BgTryUseObjectiveGo(snapshot, ai, emit,
                                        ai.bg_advice_cache().cached))
            return;

        // (2c) In-combat ADVANCE to the enemy flag (BG audit follow-up
        // 2026-06-22). On a CTF map the enemy flag sits inside the enemy
        // base, which is permanently defended — a carrier-seeker pushing in
        // is ALWAYS in combat there. The idle grab rule (idle:bg_fc_grab_flag)
        // is `!in_combat`-gated, and (2b) above only fires within the helper's
        // 12y use/approach band, so a bot fighting at 13-60y from the flag had
        // NO rule to close the gap: it brawled in place and the flag was never
        // taken (live: bots floored at ~16y from the pedestal, 0 pickups even
        // on the correct map). Mirror the idle acts_as_fc condition (no friendly
        // carrier yet, carrier-seeker role) and keep walking toward the flag
        // while fighting; once inside 12y the (2b) helper next tick approaches
        // and clicks it. Scoped to CTF by `enemy_flag` being set (node-race
        // BGs leave it zero), and capped at 60y so a mid-field skirmisher
        // isn't yanked off an unrelated fight.
        if (!i_am_carrier && snapshot.is_alive() && !snapshot.is_casting() &&
            snapshot.bg_friendly_flag_carrier().IsEmpty())
        {
            BattlegroundAdvice const& adv = ai.bg_advice_cache().cached;
            const uint8 br = snapshot.raw().bg.bg_role;
            const bool carrier_seeker =
                br == uint8(BgRole::FlagCarrier) ||
                br == uint8(BgRole::Roamer)      ||
                br == uint8(BgRole::FCEscort);
            if (carrier_seeker &&
                (adv.enemy_flag_x != 0.f || adv.enemy_flag_y != 0.f))
            {
                const float fdx = adv.enemy_flag_x - bx;
                const float fdy = adv.enemy_flag_y - by;
                const float fd2 = fdx * fdx + fdy * fdy;
                // Push to the flag from ANY distance >12y (inside 12y the (2b)
                // helper approaches+clicks). NO upper cap: WSG bases are ~620y
                // apart, so a carrier-seeker that aggroes at mid-field is ~300y
                // from the enemy flag — a 60y cap never engaged and the bot
                // brawled in place (live: fc_advance=0, 0 pickups). A flag
                // runner's job IS to run the flag, not trade blows mid-field;
                // it pushes through, fighting only incidentally. Other roles
                // (Attacker/Defender/Healer) hold the line.
                if (fd2 > 144.0f)
                {
                    emit.move_to(adv.enemy_flag_x, adv.enemy_flag_y,
                                 adv.enemy_flag_z, /*run=*/true);
                    ai.set_last_rule_fired("combat:bg_fc_advance_flag");
                    return;
                }
            }
        }

        // (2d) In-combat ADVANCE to the AV endgame target (BG audit follow-up
        // 2026-06-22). A PushEndgame order (AV captain/general rush) marches the
        // bot deep into the enemy base, which on AV is wall-to-wall elite NPCs +
        // enemy bots — so the pusher is ALWAYS in combat the whole way in. With
        // no in-combat advance the squad bogs down trading blows with trash
        // hundreds of yards short of the captain and never reaches it (live:
        // PUSH=1 for minutes, push_fires=0, score frozen at the 1-tower lead).
        // Mirror the flag-advance above: keep marching toward the ordered
        // boss-room position while fighting, and FOCUS the named captain/general
        // the instant it's visible (it grants the reinforcement swing / instant
        // win — worth more than any trash kill). Uncapped distance (the general
        // is across the map). Scoped strictly to PushEndgame + a target entry so
        // normal node/CTF combat is untouched.
        {
            auto const& ord = snapshot.raw().bg.order;
            // NOTE: deliberately NOT gated on !is_casting. A pusher stalled at
            // caster range (46y) was stuck CASTING at the out-of-LoS captain
            // every tick (328 LoS-failed casts, 0 landed) — the cast rooted it
            // so the push-move only fired in the gaps and the bot never closed.
            // Firing mid-cast lets the move_to below CANCEL the futile cast
            // (movement interrupts casting) and the bot rushes to point-blank,
            // where start_attack opens with clear LoS. At <=6y the start_attack
            // path is benign (does not cancel an in-range damaging cast).
            if (ord.kind == BgState::BgOrder::PushEndgame &&
                ord.target_entry != 0 && snapshot.is_alive())
            {
                NearbyUnit const* tgt = nullptr;
                float tgt_dsq = std::numeric_limits<float>::max();
                for (auto const& e : snapshot.raw().combat.nearby_enemies)
                {
                    if (e.entry != ord.target_entry || e.hp <= 0 ||
                        e.is_cc_locked)
                        continue;
                    const float dx = e.x - bx, dy = e.y - by;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 < tgt_dsq) { tgt_dsq = d2; tgt = &e; }
                }
                if (tgt)
                {
                    // ENGAGE-ON-LINE-OF-SIGHT. The captain (Galvangar) / general
                    // sits inside an OPEN-TOP walled garrison/courtyard (live LoS
                    // probe [avlos]: roofed=0, every interior point within 15y of
                    // Galvangar has a clear shot). The hard part was never LoS at
                    // his position — it was bots opening fire from OUTSIDE the wall
                    // (no LoS through it) and eating SPELL_FAILED_LINE_OF_SIGHT.
                    // The `in_los` gate alone fixes that: a real raycast is clear
                    // ONLY from a position with an actual shot. So engage the
                    // instant the bot has LoS at normal combat range (<=40y, max
                    // cast range) and let the class AI position — casters fire from
                    // range, melee close to 5y. The old <=6y point-blank gate was a
                    // workaround for the missing LoS check; with `in_los` it just
                    // FORCED bots to chase the (moving) captain to within 6y before
                    // attacking, so the ~5 bots that reached the courtyard circled
                    // him at 6-7y los=1 landing almost no damage (live: capalive
                    // never flipped). Below 40y + clear LoS = open fire.
                    if (tgt->in_los && tgt_dsq <= 40.0f * 40.0f)
                    {
                        emit.start_attack(tgt->guid);
                        ai.set_last_rule_fired("combat:bg_push_endgame_target");
                        return;
                    }
                    // ENTRANCE STAGING. No LoS yet (wall between us and the boss).
                    // If this boss has a known interior entrance staging point and
                    // we're within 80y but still outside, route THROUGH the entrance
                    // (to the staging point) rather than straight at the boss — a
                    // direct move_to dead-ends in the perimeter wall pockets (live:
                    // pushers pinned at ~25-44y los=0, 0 damage). Once we reach the
                    // staging point (inside) we gain LoS and the engage branch above
                    // fires. Switch back to the boss within 8y of staging (we're in).
                    {
                        float sx = 0.f, sy = 0.f, sz = 0.f;
                        if (tgt_dsq <= 80.0f * 80.0f &&
                            BgBossStagingPoint(ord.target_entry, sx, sy, sz))
                        {
                            const float sdx = sx - bx, sdy = sy - by;
                            if (sdx * sdx + sdy * sdy > 8.0f * 8.0f)
                            {
                                emit.move_to(sx, sy, sz, /*run=*/true);
                                ai.set_last_rule_fired("combat:bg_push_endgame_stage");
                                return;
                            }
                        }
                    }
                    emit.move_to(tgt->x, tgt->y, tgt->z, /*run=*/true);
                    ai.set_last_rule_fired("combat:bg_push_endgame_advance");
                    return;
                }
                // Target not in sight yet — keep pushing toward the boss room
                // through the trash rather than brawling in place. The boss room
                // is 1400-2300y across the map, far beyond the pathfinder's
                // partial-path cap, so a raw move_to never advances; leapfrog the
                // node chain (same shared mover the idle push march uses) so each
                // leg is a single navmesh-routable hop.
                if ((ord.x != 0.f || ord.y != 0.f))
                {
                    const float odx = ord.x - bx, ody = ord.y - by;
                    if (odx * odx + ody * ody > 144.0f)  // >12y
                    {
                        BgPushThroughNodes(snapshot, ai, emit,
                                           ai.bg_advice_cache().cached,
                                           ord.x, ord.y, ord.z, bg_now_ms);
                        ai.set_last_rule_fired("combat:bg_push_endgame_advance");
                        return;
                    }
                }
            }
        }

        // (3) Priority target switch (N27/N39): EFC kill > peel for our
        // carrier > enemy healer focus. Hysteresis via BgTargetSwitch (8s)
        // so the focus doesn't flip every tick. Friendly-CC'd targets are
        // skipped — breaking our own CC wastes it.
        if (!i_am_carrier)
        {
            ObjectGuid switch_to;
            char const* switch_rule = nullptr;
            const ObjectGuid efc = snapshot.bg_enemy_flag_carrier();
            const ObjectGuid our_fc = snapshot.bg_friendly_flag_carrier();
            float best_dsq = 40.0f * 40.0f;
            // EFC within 40y.
            if (!efc.IsEmpty())
                for (auto const& e : snapshot.raw().combat.nearby_enemies)
                    if (e.guid == efc && e.hp > 0 && !e.is_cc_locked)
                    {
                        const float dx = e.x - bx, dy = e.y - by;
                        if (dx * dx + dy * dy <= best_dsq)
                        { switch_to = efc; switch_rule = "combat:bg_focus_efc"; }
                        break;
                    }
            // Peel: enemy player beating on our carrier.
            if (switch_to.IsEmpty() && !our_fc.IsEmpty())
            {
                best_dsq = 40.0f * 40.0f;
                for (auto const& e : snapshot.raw().combat.nearby_enemies)
                {
                    if (!e.is_player || e.hp <= 0 || e.is_cc_locked) continue;
                    if (e.victim != our_fc) continue;
                    const float dx = e.x - bx, dy = e.y - by;
                    const float dsq = dx * dx + dy * dy;
                    if (dsq < best_dsq)
                    { best_dsq = dsq; switch_to = e.guid;
                      switch_rule = "combat:bg_peel_fc_attacker"; }
                }
            }
            // Enemy healer focus within 30y.
            if (switch_to.IsEmpty())
            {
                best_dsq = 30.0f * 30.0f;
                for (auto const& e : snapshot.raw().combat.nearby_enemies)
                {
                    if (!e.is_player || e.hp <= 0 || e.is_cc_locked) continue;
                    if (e.role != Role::Healer) continue;
                    const float dx = e.x - bx, dy = e.y - by;
                    const float dsq = dx * dx + dy * dy;
                    if (dsq < best_dsq)
                    { best_dsq = dsq; switch_to = e.guid;
                      switch_rule = "combat:bg_focus_enemy_healer"; }
                }
            }
            if (!switch_to.IsEmpty() && switch_to != snapshot.victim() &&
                !ai.action_recently_tried(BotAI::ActionKind::BgTargetSwitch,
                                          switch_to.GetCounter(), bg_now_ms))
            {
                emit.start_attack(switch_to);
                ai.note_action_retry(BotAI::ActionKind::BgTargetSwitch,
                                     switch_to.GetCounter(), bg_now_ms);
                ai.set_last_rule_fired(switch_rule);
                // No return: the rotation continues this tick against the
                // new target once the attack intent lands.
            }
        }

        // (4) Moving-cart hold in combat (BG audit §2). In SM (map 727) and
        // DHR (2656) the scoring zone is a MOVING cart (published as a live
        // node_state). A bot drifting >8y from the cart while fighting leaves
        // the zone and stops scoring, but the idle move-to-cart is combat-
        // gated. Nudge back toward the nearest own / contested cart node
        // (NON-returning — the APL still fires instants and the survival items
        // below still run while the bot eases back into the zone). Scoped to
        // the two cart maps so static-node BGs are untouched, and skipped for
        // melee that has a victim (the gap-close below owns their movement —
        // avoids a double move_to), so it drives ranged / idle bots only.
        const uint32 cart_map = snapshot.map_id();
        if (!i_am_carrier && (cart_map == 727u || cart_map == 2656u) &&
            (snapshot.victim().IsEmpty() ||
             !IsMeleeSpec(snapshot.cls(), snapshot.spec())))
        {
            const uint8 mt = snapshot.team();
            BotSnapshot::BgNodeState const* hold = nullptr;
            float best = 30.0f * 30.0f;
            for (auto const& n : snapshot.bg_node_states())
            {
                if (n.owner_team != mt && !n.is_contested) continue;  // own / contesting only
                const float dx = n.x - bx, dy = n.y - by;
                const float dsq = dx * dx + dy * dy;
                if (dsq > 64.0f && dsq < best) { best = dsq; hold = &n; }  // 8-30y drift band
            }
            if (hold)
            {
                emit.move_to(hold->x, hold->y, hold->z, /*run=*/true);
                ai.set_last_rule_fired("combat:bg_cart_hold");
            }
        }
    }

    // Survival item before everything else — a healthstone at 25% HP is
    // worth more than the next rotation tick.
    if (MaybeUseSurvivalItem(snapshot, emit)) return;

    // Mana refill for low-mana casters. Shares the potion CD with the
    // healing potion above; one will block the other in the API layer.
    if (MaybeUseManaPotion(snapshot, emit)) return;

    // Defensive racial — Stoneform-style debuff clears.
    if (MaybeUseDefensiveRacial(snapshot, emit)) return;

    // Offensive racial cooldown — Berserking, Blood Fury, etc.
    if (MaybeUseOffensiveRacial(snapshot, emit)) return;

    // On-use trinket — fire whenever ready in combat. Most trinkets are
    // damage / haste / crit pulses; gating on boss vs trash would leave
    // 5min trinket CDs unfired for entire dungeon runs.
    if (MaybeUseOnUseTrinket(snapshot, emit)) return;

    // Per-dungeon advice for this combat tick — constructed ONCE and reused by
    // the seed-loop ignore filter (below) and DungeonCombatPositioning + the
    // ignored-victim drop further down. Only paid inside instances.
    DungeonAdvice tick_advice;
    DungeonAdvice const* advice_ptr = nullptr;
    if (snapshot.is_in_instance() && Services::Initialized())
    {
        tick_advice = Services::Dungeons().GetAdvice(snapshot);
        advice_ptr = &tick_advice;
    }

    // Self-defense: if we're in combat with no victim but units are aggro'd
    // onto us, target the highest-threat attacker so the rotation has
    // something to act on. Most rotations gate their rules on victim() being
    // non-empty, so without this they'd no-op for the first tick after pull.
    //
    // The guard is ONLY victim().IsEmpty(): the old version also required
    // current_target().IsEmpty(), but a STALE selection (a mob that died /
    // evaded / left the hostile scan, still set as current_target) is non-empty
    // and PERMANENTLY blocked this self-acquire. The bot then sat InCombat with
    // attackers>0 but victim==0 forever — its offensive APL no-opping (every
    // rotation gates on victim()), doing only auto-attack (~0 dmg for a caster)
    // plus panic self-heal: a 50-60 MINUTE combat hang with zero kills / XP
    // (observed live: Velruun L4 Shaman combat_duration 3028s, Thaelyn L4 Druid
    // 3633s, both looping "Healing Surge/Regrowth (<=50% self) > Auto attack").
    // Engaging the highest-threat attacker sets victim so the rotation acts; if
    // current_target was already that attacker, start_attack simply (re)engages
    // it. start_attack validates the target and the executor's per-target
    // StartAttack refusal lockout gates any spam on a genuinely immune unit.
    if (snapshot.victim().IsEmpty())
    {
        // Pick the highest-threat attacker we have NOT already had refused.
        // combat.attackers is threat-sorted descending (BotSnapshotBuilder),
        // so the first non-refused entry is the best reachable target. Skipping
        // refused attackers is what makes the harbor wedge self-resolve: when
        // the tank aggroed ~8 evading/unreachable hostiles, IsValidAttackTarget
        // rejected every StartAttack (→ InvalidTarget → lockout, Fix #1), so
        // re-seeding the same top attacker every tick kept victim empty AND kept
        // re-issuing a doomed StartAttack. By skipping refused attackers we
        // leave victim genuinely empty ONLY when EVERY attacker is unreachable —
        // which is precisely the signal DungeonCombatPositioning's in-combat
        // boss-advance (below) keys on to drive the tank toward the boss instead
        // of standing frozen in fake combat. When at least one attacker is
        // reachable, this still engages it (no behavior change for normal pulls).
        NearbyUnit const* seed = nullptr;
        for (auto const& a : snapshot.raw().combat.attackers)
        {
            if (a.hp <= 0) continue;
            // Untargetable (UNIT_FLAG_UNINTERACTIBLE) attackers can never be
            // acquired — IsValidAttackTarget rejects them. Skipping them keeps
            // victim empty (vs churning doomed StartAttacks on a stalker swarm)
            // so the in-combat boss-advance recognizes the wedge and walks out.
            if (a.untargetable) continue;
            // [increment 1e] Skip a pacified (UNIT_FLAG_PACIFIED) attacker —
            // never a real threat — AND an attacker whose own chase generator
            // reports it cannot path to us (Creature::CanNotReachTarget(),
            // snapshot cannot_reach): the WC corridor ledge pack (2026-07-03)
            // is ordinary targetable creatures, so only this signal marks it.
            // Re-seeding either kind every empty-victim tick would re-install
            // a chase the melee gap-close below can never close. Together with
            // the hp<=0 / untargetable checks above this completes the
            // IsAttackerFightable test. Kill switch:
            // PlayerbotV2.Combat.SkipUnfightable.
            if (Services::Config().combat_skip_unfightable() &&
                (a.is_pacified || a.cannot_reach))
            {
                DiagSkipUnfightable(snapshot, a.guid, UnfightableReason(a));
                continue;
            }
            // Never SEED an unkillable IGNORED marker (e.g. Deadmines 49671
            // Vanessa cutscene double: IMMUNE_TO_NPC, boss-HP, Bloodwash pack-
            // healed). The downstream combat:drop_ignored_victim rule reads the
            // LAGGED snapshot victim and misses this re-seed — its drop window
            // never aligns with the empty-victim tick on which the seed happens —
            // so the tank/rogue grind the marker ~75s and die to ranged adds (the
            // live 06-27 harbor death-loop: tank PINNED at (-173,-582,19) on a
            // single 49671, d2rip flat, 87%->0%). Filtering at the SOURCE leaves
            // victim genuinely empty so the in-combat boss-advance / ghost-push
            // walks the group off the marker toward the boss.
            if (advice_ptr)
            {
                bool ignored = false;
                for (uint32_t ie : advice_ptr->ignore_entries)
                    if (ie == a.entry) { ignored = true; break; }
                if (ignored) continue;
            }
            if (ai.start_attack_recently_refused(a.guid.GetCounter(),
                                                 snapshot.published_at_ms()))
                continue;
            seed = &a;
            break;
        }
        if (seed)
            emit.start_attack(seed->guid);
        // Pet-assist (2026-06-19). A pet pulling mobs flags the OWNER in-combat
        // immediately (TC: owner is in combat while its pet is), so a pet-class
        // bot enters State_InCombat with NO victim and NO attackers — the mobs
        // are fighting the PET, not the bot. The idle:assist_pet rule that would
        // adopt the pet's target is structurally unreachable here (it is an
        // idle-state rule AND self-gates on !in_combat), and none of this
        // state's acquisition paths consult the pet. The bot therefore sat in
        // combat indefinitely with an empty victim, its rotation no-opping every
        // tick (Zekani, L10 BM hunter: 60+ minutes in combat, 0 XP). Adopt the
        // pet's victim / peel target so the rotation engages and the fight
        // resolves. Only when no attacker was grabbed above (real aggro on the
        // bot takes priority); SelectAssistPetTarget returns Empty unless the
        // target is a live hostile visible in our nearby_enemies. Honor the
        // engage shield: if the combat watchdog just disengaged this exact
        // target as unreachable/unkillable (no-progress / LoS-wedge), do NOT
        // re-adopt it via the pet — that would turn the forever-stall into a
        // slower watchdog-cycle loop. The pet retargets on its own; once it
        // bites a reachable mob, the shield (or a different pet_victim) lets
        // this fire again.
        else if (snapshot.has_pet())
        {
            const ObjectGuid pt = SelectAssistPetTarget(snapshot);
            const uint32 pa_now = snapshot.published_at_ms();
            const ObjectGuid pa_shielded = ai.last_engage_target();
            const bool pa_shield_on = !pa_shielded.IsEmpty() &&
                (pa_now - ai.last_engage_at_ms() < ai.last_engage_shield_ms());
            if (!pt.IsEmpty() && !(pa_shield_on && pt == pa_shielded))
                emit.start_attack(pt);
        }
    }

    // Increment 1m (2026-07-20): PROGRESS-STICKY ownership tick — the
    // in-combat counterpart of the one at the top of DungeonDispatch
    // (State_Idle.cpp; see BotAI::move_commit_note_progress()). A dungeon
    // bot mid boss-advance often stays in the InCombat FSM state for the
    // whole fight, so DungeonDispatch's idle-side tick never runs for it —
    // DungeonCombatPositioning/DungeonConvergeToFight below (this file's
    // dungeon movement entry) are the equivalent per-tick hook here. Same
    // kill switch as step-hold.
    if (snapshot.is_in_instance() && Services::Config().move_step_hold_enabled())
    {
        float mc_x, mc_y, mc_z;
        snapshot.position(mc_x, mc_y, mc_z);
        ai.move_commit_note_progress(snapshot.map_id(), mc_x, mc_y, mc_z,
                                     snapshot.published_at_ms());
    }

    // Stranded-member recovery (shared): a follower that respawned at the far
    // entrance graveyard or wedged on a disconnected poly cannot rejoin on foot and
    // would oscillate the in-combat rejoin (rule 0b inside DungeonCombatPositioning)
    // forever. Relocate it onto the group FIRST so it rejoins / rezzes. The helper's
    // own real-combat gate excludes a genuinely fighting bot (fightable attacker or
    // a real victim), so only a stalker-only false-combat straggler is teleported.
    if (snapshot.is_in_instance() &&
        DungeonRecoverStrandedFollower(snapshot, ai, group, emit,
                                       snapshot.published_at_ms()))
        return;

    // Converge-to-fight runs here too: while the bot IS in combat this resets
    // its join clock (a no-op move), so the timer is clean when it next drops
    // combat still stranded from the tank. (Genuine in-combat bots are excluded
    // by the helper's own !in_combat gate.)
    if (snapshot.is_in_instance() &&
        DungeonConvergeToFight(snapshot, ai, group, emit,
                               snapshot.published_at_ms()))
        return;

    // [BUG G-P0a] Dungeon-combat positioning. The avoidance / mechanic
    // cascade (step-out-of-fire → kite → soak → spread → stack → melee-behind)
    // used to live only in the idle dispatcher and therefore NEVER ran during
    // combat — bots stood in boss fire. Run it here, in-combat, BEFORE the
    // melee gap-close so avoidance preempts the chase. Gated on is_in_instance
    // so the GetAdvice() lookup (which constructs the per-dungeon advice
    // vectors) is only paid inside instances; out in the open world this is a
    // single bool check. Early-return on a positioning emit so the bot
    // actually moves out of danger instead of chasing its victim this tick.
    if (advice_ptr)
    {
        DungeonAdvice const& advice = *advice_ptr;
        if (DungeonCombatPositioning(snapshot, ai, group, emit, advice))
            return;
        // IGNORED-ENTRY VICTIM DROP (2026-06-27). advice.ignore_entries are
        // unkillable markers/props (e.g. Deadmines 49671 Vanessa VanCleef:
        // faction-17 hostile, IMMUNE_TO_NPC, boss-HP HealthModifier 20, AIName
        // SmartAI but ZERO scripts — every bot spell on her is rejected). The
        // unkillable-target leash EXEMPTS elites/bosses, so a bot that acquires
        // one grinds it forever AND its member-in-combat blocks tank_advance
        // (observed live 2026-06-27: tank+rogue+hunter pinned on a 49671 spawn
        // in the Helix foundry 15+ min, run frozen at 1/6, never reached the
        // harbor). If our victim is an ignored entry, DROP it and set the engage
        // shield so the last-ditch fallback (combat:fallback_attack honours the
        // shield via skip_target) cannot immediately re-grab it; the bot exits
        // her false-combat and falls through to dungeon dispatch / boss-nav.
        if (NearbyUnit const* vt = snapshot.victim_info())
            for (uint32_t ie : advice.ignore_entries)
                if (ie == vt->entry)
                {
                    ai.note_engage(snapshot.victim(),
                                   snapshot.published_at_ms(),
                                   /*shield_ms*/ 5u * 60u * 1000u);
                    emit.stop_attack();
                    ai.set_last_rule_fired("combat:drop_ignored_victim");
                    return;
                }
    }

    // ---- PvE-coordinator heal focus ----
    // The designated tank-healer pins its assigned focus (the main tank)
    // when that target is genuinely hurt — otherwise every healer in the
    // group converges on the same lowest-HP member while the tank eats
    // unhealed hits. Placed BELOW survival items / racials / combat
    // positioning so emergencies and fire-avoidance keep priority, and
    // the tick is only consumed on a successful emit (a queued/locked
    // cast falls through to the class APL instead of wasting the tick).
    // is_alive guard, not hp > 0: a released ghost main tank has hp == 1
    // and would otherwise soak front-loaded heals all corpse run.
    {
        auto const& heal_po = snapshot.raw().pve_order;
        if (heal_po.active && !heal_po.heal_focus.IsEmpty() &&
            snapshot.is_alive() && !snapshot.is_casting() &&
            heal_po.heal_focus != snapshot.raw().guid &&
            group.exists())
        {
            const uint32 hspell = ClassOocHeal(snapshot.cls(), snapshot.spec());
            if (hspell != 0 && snapshot.knows_spell(hspell) &&
                snapshot.is_ready(hspell))
            {
                if (auto const* mems = group.members())
                    for (auto const& m : *mems)
                    {
                        if (m.guid != heal_po.heal_focus) continue;
                        if (!m.online || !m.is_alive ||
                            m.map_id != snapshot.map_id())
                            break;
                        if (m.hp_pct() >= 70) break;   // APL triage covers
                        float hx, hy, hz;
                        snapshot.position(hx, hy, hz);
                        const float dxh = m.x - hx, dyh = m.y - hy;
                        if (dxh * dxh + dyh * dyh < 40.0f * 40.0f &&
                            emit.cast(hspell, m.guid))
                        {
                            ai.set_last_rule_fired("combat:pve_heal_focus");
                            return;
                        }
                        break;
                    }
            }
        }
    }

    // [BUG BG-P0a] Flag/orb-carrier homeward. A CTF/orb carrier must keep
    // running toward its capture point even while attacked (real players don't
    // stop to trade blows with the chaser). Resolve the carrier signal /
    // destination via the shared helper, reusing the per-bot BG advice cache
    // (refreshed on idle ticks). When carrying, emit a run move_to toward the
    // cap point; we deliberately do NOT return here so the APL still fires the
    // bot's instant-cast damage while it runs — but the homeward move and the
    // `!i_am_carrier` guard below together suppress the melee gap-close so the
    // carrier never peels off to chase its attacker.
    // (i_am_carrier hoisted above the BG tactical layer.)
    if (snapshot.in_battleground() && i_am_carrier)
        BgCarrierHomeward(snapshot, ai, emit, ai.bg_advice_cache().cached);

    // Gap-close: if we're a melee spec and our victim is out of melee range,
    // emit a MoveTo to the victim's position. The rotation can still fire
    // its instant-cast rules during the close — start_attack already runs
    // auto-attack once we arrive. We use the snapshot's nearby_enemies /
    // attackers for victim position rather than chasing a moved target.
    if (!snapshot.victim().IsEmpty() && !i_am_carrier &&  // BG-P0a: carrier runs home, never chases
        IsMeleeSpec(snapshot.cls(), snapshot.spec()))
    {
        constexpr float kMeleeRange = 5.0f;
        constexpr uint16 kStuckChaseTicksMax = 50;   // ~5s @ 10Hz combat tier
        NearbyUnit const* vu = snapshot.victim_info();
        if (vu)
        {
            float bx, by, bz;
            snapshot.position(bx, by, bz);
            const float dx = vu->x - bx;
            const float dy = vu->y - by;
            const float dz = vu->z - bz;
            const float d2 = dx*dx + dy*dy + dz*dz;
            // Stuck-chase tracking: count ticks where we're out of melee
            // range with the same victim. Reset on different victim or once
            // we close distance. Past the cap → drop the chase to free the
            // bot from terrain-induced infinite gap-close loops.
            if (d2 > kMeleeRange * kMeleeRange)
            {
                if (ai.stuck_chase_victim() == snapshot.victim())
                    ai.set_stuck_chase(snapshot.victim(), ai.stuck_chase_ticks() + 1);
                else
                    ai.set_stuck_chase(snapshot.victim(), 1);
                if (ai.stuck_chase_ticks() >= kStuckChaseTicksMax)
                {
                    emit.stop_attack();
                    // Re-engage shield (BG audit N44): without it the
                    // fallback target pick re-acquired the SAME kiting
                    // player next tick — a visible give-up/re-engage flap
                    // that never lands a hit. The shield makes the bot
                    // pick a different target (or objective) instead.
                    ai.note_engage(snapshot.victim(), snapshot.published_at_ms());
                    ai.set_stuck_chase(ObjectGuid::Empty, 0);
                    return;
                }
            }
            else
            {
                // Within melee — clear the counter so future losses of range
                // start a fresh window.
                if (ai.stuck_chase_ticks() > 0)
                    ai.set_stuck_chase(ObjectGuid::Empty, 0);
            }
            if (d2 > kMeleeRange * kMeleeRange && !snapshot.is_rooted())
            {
                // [increment 1e] WC corridor freeze (2026-07-03): the victim
                // may be unfightable — untargetable / pacified / dead flags,
                // or (the live WC case) an ordinary targetable creature whose
                // OWN chase generator reports it cannot path to us
                // (Creature::CanNotReachTarget → snapshot cannot_reach; the
                // ledge pack is z-disconnected from the corridor). This
                // gap-close fired every InCombat frame (already_pathing never
                // held since the live spline dest was the route step, not the
                // victim) and kept yanking the tank off its route spline
                // toward a chase that could never close. Skip the emit — fall
                // through without stop_attack / claiming the tick — so
                // DungeonCombatPositioning (route-aware) keeps movement
                // ownership and the mob is left to leash.
                if (Services::Config().combat_skip_unfightable() &&
                    !IsAttackerFightable(*vu))
                {
                    DiagSkipUnfightable(snapshot, snapshot.victim(),
                                        UnfightableReason(*vu));
                }
                else
                {
                // Default: gap-close to the victim.
                float cx = vu->x, cy = vu->y, cz = vu->z;
                // Travel-through-combat (2026-06-15): when the bot has a FAR
                // same-map travel objective it is IN TRANSIT — incidental aggro
                // must not pull it off-course chasing mobs. Live: Somi drifted
                // EAST after Durotar mobs while his quest goal was ~1200y WEST,
                // "gaining XP but never arriving". Steer the gap-close toward the
                // GOAL instead of the victim: mobs in melee are still auto-attacked
                // as the bot passes, ones it outruns leash, and it makes real
                // travel progress (the stuck-chase cap above then drops the outrun
                // victim). Only when the goal is FAR (>150y — bot not yet at the
                // quest area) AND HP is healthy (low-HP defensives/flee fired
                // earlier own that case); a NEAR goal chases normally so the bot
                // can actually kill its quest mobs.
                if (snapshot.hp_pct() > 50 &&
                    snapshot.has_current_objective() &&
                    snapshot.current_objective_poi().valid &&
                    snapshot.current_objective_poi().map_id == snapshot.map_id())
                {
                    const auto& poi = snapshot.current_objective_poi();
                    const float gdx = poi.x - bx, gdy = poi.y - by;
                    if (gdx * gdx + gdy * gdy > 150.0f * 150.0f)
                    { cx = poi.x; cy = poi.y; cz = poi.z; }
                }
                // Skip the MoveTo if we're already pathing to ~the chase target —
                // avoids spamming MoveTo every tick. Replan only if it moved >3yd.
                // Rooted bots can't move, so don't bother emitting either.
                bool already_pathing = false;
                if (snapshot.has_path_destination())
                {
                    float px, py, pz;
                    snapshot.path_destination(px, py, pz);
                    const float pdx = cx - px;
                    const float pdy = cy - py;
                    const float pdz = cz - pz;
                    const float pd2 = pdx*pdx + pdy*pdy + pdz*pdz;
                    constexpr float kReplanRange = 3.0f;
                    if (pd2 < kReplanRange * kReplanRange) already_pathing = true;
                }
                if (!already_pathing)
                    emit.move_to(cx, cy, cz, /*run*/ true);
                }
            }
        }
    }

    // Pet command: keep the active pet attacking the bot's current victim.
    // pet_attack is idempotent server-side. has_pet() short-circuits when the
    // pet is dead (pet_alive=false) so we don't fire failing commands at a
    // corpse — Hunter rotation handles Revive Pet OOC via State_Idle.
    // Only emit while the pet is on the WRONG target: an unconditional emit
    // here re-issued PetAttack every combat tick (~7 intents/sec/bot observed
    // live on Uraimus 2026-06-10), drowning the intent history and queue for
    // zero behavioral gain. pet_victim() lags by one snapshot (~250ms) after
    // the pet switches, so at most 1-2 duplicate emits slip through per
    // target change.
    if (snapshot.has_pet() && !snapshot.victim().IsEmpty() &&
        snapshot.pet_victim() != snapshot.victim())
        emit.pet_attack(snapshot.victim());

    // Battle-rez: cast a class-appropriate combat-castable rez on the first
    // dead group member. Spells:
    //   Druid Rebirth                    20484  (instant rez, brez budget)
    //   Death Knight Raise Ally          61999  (8s cast, brez budget, requires
    //                                            Corpse Dust unless talented)
    // Warlock's Soulstone Resurrection (20707) is intentionally NOT here:
    // 20707 is a *living-target* pre-buff that grants the auto-rez aura
    // (spell 3026 fires on next death). It belongs in the State_Idle buff-
    // maintenance layer, not the in-combat brez branch.
    // Only fires when not mid-cast (don't interrupt our own DPS cast for the
    // brez) and only when the dead target is on the same map. Server tracks
    // the per-encounter battle-rez budget; out-of-charges surfaces as
    // ServerRefused and the AI moves on. brez_acked dedups against re-emits
    // until the target rezzes (drops from dead_member) or leaves the group.
    if (group.exists() && !snapshot.is_casting())
    {
        uint32 brez_spell = 0;
        switch (snapshot.cls())
        {
            case CLASS_DRUID:        brez_spell = 20484; break;
            case CLASS_DEATH_KNIGHT: brez_spell = 61999; break;
            default: break;
        }
        if (brez_spell != 0 && snapshot.knows_spell(brez_spell) && snapshot.is_ready(brez_spell))
        {
            if (GroupMemberSummary const* dead = group.dead_member(snapshot.map_id()))
            {
                // Cross-caster dedup: skip if any OTHER raid member is already
                // mid-cast on a battle-rez (Rebirth/Raise Ally) targeting the
                // same dead member. With multi-Druid raids, all of them race
                // to rez the first corpse — server lets one succeed but the
                // others burn 4-8s of cast time. Snapshot's GroupMemberSummary
                // surfaces is_casting + casting_spell + casting_target so we
                // can short-circuit here.
                if (auto const* mems = group.members())
                {
                    bool already_cast = false;
                    for (auto const& m : *mems)
                    {
                        if (m.guid == snapshot.raw().guid) continue;
                        if (!m.is_casting) continue;
                        if (m.casting_target != dead->guid) continue;
                        if (m.casting_spell != 20484 &&
                            m.casting_spell != 61999) continue;
                        already_cast = true;
                        break;
                    }
                    if (already_cast) return;
                }
                if (!ai.brez_acked() || ai.brez_target() != dead->guid)
                {
                    emit.cast(brez_spell, dead->guid);
                    ai.set_brez(dead->guid, true);
                    return;
                }
            }
            else if (ai.brez_acked())
            {
                // Target rezzed (or dropped from group) — clear so the next
                // dead member kicks off a fresh attempt.
                ai.set_brez(ObjectGuid::Empty, false);
            }
        }
    }

    // While a hard cast is in flight on the bot, every additional cast intent
    // we emit gets rejected with SPELL_FAILED_SPELL_IN_PROGRESS by the engine
    // (Spell.cpp:3461). Skip APL evaluation entirely until the cast resolves.
    // We accept the small efficiency loss during the LAUNCHED window (where
    // new casts would actually be valid) in exchange for not flooding the
    // intent queue with rejected casts on every 250ms tick.
    if (snapshot.is_casting()) return;

    // Sub-L10 bots are still in the "pre-spec" era: even though Midnight
    // auto-assigns a primary spec at character creation (so spec() is non-
    // zero from L1), the spec-locked rotations were authored against the
    // L10+ ability set. Routing a L7 BM hunter to the BM rotation skips
    // Arcane Shot and Hunter's Mark — abilities the bot DOES have via base
    // SkillLineAbility but that the BM rotation doesn't list. The baseline
    // (spec=0) rotation includes the full starter kit for each class, so
    // force it for pre-L10. At L10 specs unlock their full talent rows;
    // we flip back to the spec rotation cleanly on ding.
    const uint32 rotation_spec = snapshot.level() < 10 ? 0u : snapshot.spec();
    if (ApRotation const* rot = Combat::GetRotation(snapshot.cls(), rotation_spec))
    {
        char const* rule_name = nullptr;
        if (rot->tick(ctx, emit, &rule_name))
        {
            ai.set_last_rule_fired(rule_name);
            return;
        }
    }

    // Fallback: keep the bot from looking inert. Try victim, then the
    // selected target, then the highest-threat attacker. start_attack is
    // idempotent server-side.
    ObjectGuid t = snapshot.victim();
    if (t.IsEmpty()) t = snapshot.current_target();
    if (t.IsEmpty())
    {
        if (NearbyUnit const* a = snapshot.highest_threat_attacker())
            t = a->guid;
    }
    // Skull-marked target wins for everyone in combat too (mid-fight focus
    // change). Only honor it if it's actually nearby — out-of-vision skulls
    // are stale.
    if (t.IsEmpty() && group.exists())
    {
        if (ObjectGuid skull = group.skull_target(); !skull.IsEmpty())
        {
            // Skip a dead skull — corpses linger in nearby_enemies for a few
            // seconds and we'd just re-emit start_attack on a corpse every
            // tick until raid leader re-marks.
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == skull && u.hp > 0) { t = skull; break; }
            if (t.IsEmpty())
                for (auto const& u : snapshot.raw().combat.attackers)
                    if (u.guid == skull && u.hp > 0) { t = skull; break; }
        }
    }
    // Group-assist: when nothing else is engaged but we're in combat, pick up
    // whatever the leader (or any in-combat member) is hitting. Skip if we're
    // tanking — tanks pull their own targets.
    if (t.IsEmpty() && ai.effective_role(snapshot) != Role::Tank && group.exists())
    {
        if (auto const* members = group.members())
        {
            const ObjectGuid leader = group.leader();
            ObjectGuid leader_victim;
            ObjectGuid any_combat_victim;
            // Single-pass scan: capture leader's victim and the first
            // online-in-combat member's victim in the same walk. Pre-fix
            // this was two separate passes over `members`. Tiny win in
            // absolute ms (25-member raid × 2 passes), but the pattern
            // matters — group iteration runs every combat tick.
            // Copied member victims must resolve against our own hostile
            // lists (IsValidAttackTarget-gated at snapshot build). A member
            // wedged on an invalid victim (friendly pet, groupmate) must
            // not propagate it — see ingroup:assist for the 2026-06-11
            // Deadmines bots-fighting-each-other cascade this caused.
            auto resolves_hostile = [&](ObjectGuid v) -> bool
            {
                if (v.IsEmpty()) return false;
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                    if (u.guid == v && u.hp > 0) return true;
                for (auto const& u : snapshot.raw().combat.attackers)
                    if (u.guid == v && u.hp > 0) return true;
                // Trust Creature-typed victims beyond our capped scan —
                // see ingroup:assist (the wedge class was Player/Pet guids;
                // start_attack validates at engage time regardless).
                return v.IsCreature();
            };
            for (auto const& m : *members)
            {
                if (m.guid == leader && resolves_hostile(m.victim))
                {
                    leader_victim = m.victim;
                    if (!any_combat_victim.IsEmpty()) break;
                }
                else if (any_combat_victim.IsEmpty() && m.online && m.in_combat
                         && resolves_hostile(m.victim))
                {
                    any_combat_victim = m.victim;
                    if (!leader_victim.IsEmpty()) break;
                }
            }
            t = !leader_victim.IsEmpty() ? leader_victim : any_combat_victim;
        }
    }
    // Last-ditch engage: still no target after all the fallbacks → pick the
    // closest enemy from nearby_enemies. This catches stealth-pulls where the
    // bot is in combat but neither attackers nor any group member's victim
    // points to a valid target. Without this the bot just stands there
    // looking inert until combat times out.
    if (t.IsEmpty())
    {
        // Never re-acquire a target we just disengaged as unkillable, nor a
        // no-XP / pacified prop. Without this skip the last-ditch fallback
        // immediately re-grabs the nearest enemy the tick after a disengage
        // cleared the victim — re-locking the bot onto the immortal Training
        // Dummy it was just shaken off (the grind/pull pickers honour the engage
        // shield, but this fallback did not, so the bot cycled forever).
        const uint32 now_ms2 = snapshot.published_at_ms();
        const ObjectGuid shielded = ai.last_engage_target();
        const bool shield_on = !shielded.IsEmpty() &&
            (now_ms2 - ai.last_engage_at_ms() < ai.last_engage_shield_ms());
        auto skip_target = [&](NearbyUnit const& u) -> bool
        {
            return u.no_xp_kill || u.is_pacified ||
                   (shield_on && u.guid == shielded);
        };
        // Prefer a target we can SEE — the scan returns mobs through walls,
        // and opening on one drags the party into line-of-sight cast errors
        // (same class as the tank-pull LoS fix). Fall back to any live
        // enemy only when nothing is visible (we ARE in combat with
        // something, e.g. a caster around a pillar).
        for (auto const& u : snapshot.raw().combat.nearby_enemies)
            if (u.hp > 0 && u.in_los && !skip_target(u)) { t = u.guid; break; }
        if (t.IsEmpty())
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.hp > 0 && !skip_target(u)) { t = u.guid; break; }
        // Still nothing from the enemy scan — RETALIATE against whatever is
        // actually meleeing us. attackers[] are by definition adjacent and
        // engaging, so they are reliably reachable; the nearby_enemies scan can
        // miss a mob that pulled us from outside scan range or got filtered.
        // Fixes the in-combat-with-attackers-but-victim==0 loop where a bot
        // takes damage forever without fighting back. Strictly additive (only
        // when no target was selected by any prior path). 2026-06-17.
        // (A pacified dummy can't be an attacker, but keep the skip for symmetry
        // so a shielded/no-XP guid is never re-grabbed here either.)
        if (t.IsEmpty())
            for (auto const& u : snapshot.raw().combat.attackers)
                if (u.hp > 0 && !skip_target(u)) { t = u.guid; break; }
    }
    // Dedup: if already attacking this exact target, the API's Player::Attack
    // would return false → ServerRefused. Skip the emit to avoid skewing the
    // intents_failed counter every tick of a long fight.
    if (!t.IsEmpty() && t != snapshot.victim())
    {
        // Lockout-gate: refused StartAttack (immune / phased / vehicle /
        // already-attacking) shouldn't claim the dispatch slot every InCombat
        // tick — fall through cleanly so the rule doesn't dominate the
        // intents_failed counter on a wedged engage.
        if (!emit.start_attack(t)) return;
        ai.set_last_rule_fired("combat:fallback_attack");
    }
}

} // namespace Playerbot::States
