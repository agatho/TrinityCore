// MaintainRules - Refactor #3 pass 15. Migrates the Maintain* helper
// family + pet_heal out of the State_Idle linear cascade. The helper
// bodies live in `Bot/States/State_Idle.cpp` (now externally callable
// via `Bot/States/MaintainHelpers.h`); each rule's fire path is a
// 1-line wrapper that calls the helper and reports the legacy tag.
//
// Priority band 671..696: above OocConsumable family (600..670), below
// the high-pri preemption band (>=700) — preserves the inline order.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/States/MaintainHelpers.h"

namespace Playerbot {

// Pet-assist target selection — peel-before-assist (see MaintainHelpers.h).
// Defined at States scope (not in the anonymous block below) so State_InCombat
// can call it too: when a pet pulls, the owner is flagged in-combat at once,
// routing ticks to DispatchInCombat and making the idle:assist_pet rule (gated
// on !in_combat) unreachable — a pet-class bot then stands in combat with no
// victim (Zekani, BM hunter, 60min / 0 XP). The in-combat caller reuses this.
namespace States {

ObjectGuid SelectAssistPetTarget(BotSnapshotView const& s)
{
    ObjectGuid const pv = s.pet_victim();
    ObjectGuid peel;
    for (ObjectGuid const& aguid : s.pet_attackers())
    {
        if (aguid.IsEmpty()) continue;
        if (aguid == pv) continue;  // not a peel candidate
        // Must be a live hostile in our snapshot or the engagement
        // loop has nothing to chase.
        for (auto const& e : s.raw().combat.nearby_enemies)
        {
            if (e.guid != aguid) continue;
            if (e.hp <= 0) break;
            peel = aguid;
            break;
        }
        if (!peel.IsEmpty()) break;
    }
    if (!peel.IsEmpty()) return peel;
    // Fall back to standard assist on pet's victim (also gated on
    // visibility in nearby_enemies).
    if (pv.IsEmpty()) return ObjectGuid::Empty;
    for (auto const& e : s.raw().combat.nearby_enemies)
        if (e.guid == pv) return e.hp > 0 ? pv : ObjectGuid::Empty;
    return ObjectGuid::Empty;
}

} // namespace States

namespace {

using namespace ::Playerbot::States;

// Trivial true-gate so the registry always lets the helper decide.
// Helper functions are themselves gate-and-fire combined: they check
// internal conditions and only emit when applicable. Wrapping each as
// a fire-only rule preserves the legacy "call-then-test-return" shape.
bool AlwaysGate(BotSnapshotView const&, BotAI&, GroupSnapshotView const&, uint32)
{
    return true;
}

// ---------- idle:starter_talents ----------
bool StarterTalentsFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const&,
                        BotIntentEmitter& emit, uint32)
{
    if (!MaintainStarterTalents(s, ai, emit)) return false;
    ai.set_last_rule_fired("idle:starter_talents");
    return true;
}

// ---------- idle:starter_extend ----------
bool StarterExtendFire(BotSnapshotView const& s, BotAI& ai,
                       GroupSnapshotView const&,
                       BotIntentEmitter& emit, uint32)
{
    if (!MaintainStarterTalentsExtend(s, ai, emit)) return false;
    ai.set_last_rule_fired("idle:starter_extend");
    return true;
}

// ---------- idle:apply_context_talents ----------
bool ContextTalentsFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const&,
                        BotIntentEmitter& emit, uint32)
{
    if (!MaintainContextTalents(s, ai, emit)) return false;
    ai.set_last_rule_fired("idle:apply_context_talents");
    return true;
}

// ---------- idle:auto_equip ----------
bool AutoEquipFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const&,
                   BotIntentEmitter& emit, uint32)
{
    if (!MaintainAutoEquipUpgrades(s, ai, emit)) return false;
    ai.set_last_rule_fired("idle:auto_equip");
    return true;
}

// ---------- idle:equip_bag_upgrade ----------
bool BagUpgradeFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&,
                    BotIntentEmitter& emit, uint32)
{
    if (!MaintainBagUpgrade(s, ai, emit)) return false;
    ai.set_last_rule_fired("idle:equip_bag_upgrade");
    return true;
}

// ---------- idle:ooc_rez ----------
bool OocRezFire(BotSnapshotView const& s, BotAI& ai,
                GroupSnapshotView const& g,
                BotIntentEmitter& emit, uint32)
{
    if (!MaintainOocRez(s, g, emit)) return false;
    ai.set_last_rule_fired("idle:ooc_rez");
    return true;
}

// ---------- idle:ooc_heal ----------
bool OocHealFire(BotSnapshotView const& s, BotAI& ai,
                 GroupSnapshotView const& g,
                 BotIntentEmitter& emit, uint32)
{
    if (!MaintainOocHeal(s, g, emit, ai.effective_role(s))) return false;
    ai.set_last_rule_fired("idle:ooc_heal");
    return true;
}

// ---------- idle:ooc_dispel ----------
bool OocDispelFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const& g,
                   BotIntentEmitter& emit, uint32)
{
    if (!MaintainOocDispel(s, g, emit)) return false;
    ai.set_last_rule_fired("idle:ooc_dispel");
    return true;
}

// ---------- idle:self_buff ----------
bool SelfBuffFire(BotSnapshotView const& s, BotAI& ai,
                  GroupSnapshotView const& g,
                  BotIntentEmitter& emit, uint32)
{
    if (!MaintainSelfBuff(s, g, emit)) return false;
    ai.set_last_rule_fired("idle:self_buff");
    return true;
}

// ---------- idle:soulstone ----------
bool SoulstoneFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const& g,
                   BotIntentEmitter& emit, uint32)
{
    if (!MaintainSoulstone(s, g, emit)) return false;
    ai.set_last_rule_fired("idle:soulstone");
    return true;
}

// ---------- idle:pet ----------
bool PetFire(BotSnapshotView const& s, BotAI& ai,
             GroupSnapshotView const&,
             BotIntentEmitter& emit, uint32)
{
    if (!MaintainPet(s, ai, emit)) return false;
    ai.set_last_rule_fired("idle:pet");
    return true;
}

// ---------- idle:pet_recall ----------
// Pet at low HP and engaged with mobs the bot can't help fight
// (attacker on a ledge, behind a wall, or simply out of the bot's
// engage radius). Without recall the pet eats damage until it dies.
// Issue COMMAND_FOLLOW: pet drops its attack target and runs back
// to the owner. If the attacker is melee and gives chase, pet
// arrives near owner with attacker in tow — bot can engage at full
// HP. If the attacker is ranged, it loses leash and de-aggros.
//
// Conservative thresholds (avoid recall-spam):
//   - pet alive, in combat, HP < 25% (genuine emergency)
//   - pet has at least 1 attacker (we know it's taking damage)
//   - NONE of the attackers are in our 40y engage range
//     (= bot can't help fight them → recall is the only save)
//   - bot OOC (mid-combat we shouldn't take over pet control)
//   - 10s retry cooldown so we don't re-emit every tick while the
//     pet is still pathing back
//
// We deliberately do NOT also flip react state to PASSIVE — the
// pet still defends itself if hit en route, which is fine because
// the existing assist_pet rule will get the bot engaging too if
// any attacker comes into range.
bool PetRecallGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!s.has_pet()) return false;
    if (!s.pet_in_combat()) return false;
    if (s.in_combat()) return false;
    if (!s.is_alive()) return false;
    if (s.pet_hp_pct() <= 0 || s.pet_hp_pct() >= 25) return false;
    auto const& patk = s.pet_attackers();
    if (patk.empty()) return false;
    // Any attacker reachable to the bot (in engage range)? If yes,
    // assist_pet/peel can handle it; recall would be defeatist.
    float bx, by, bz; s.position(bx, by, bz);
    constexpr float kEngageSq = 40.0f * 40.0f;
    for (auto const& e : s.raw().combat.nearby_enemies)
    {
        for (auto const& a : patk)
        {
            if (e.guid != a) continue;
            if (e.hp <= 0) continue;
            const float dx = e.x - bx, dy = e.y - by, dz = e.z - bz;
            if (dx*dx + dy*dy + dz*dz <= kEngageSq) return false;
        }
    }
    // 10s retry cooldown — pet pathing back takes a few seconds and
    // we don't want to re-emit every tick.
    const uint32 now_ms = s.published_at_ms();
    // Per-bot recall state is keyed by ActionKind; the action key
    // itself just needs a stable disambiguator within the bot's
    // history. Fixed 1 since there is only one recall channel.
    constexpr uint64 key = 1u;
    return !ai.action_recently_tried(BotAI::ActionKind::PetRecall, key, now_ms);
}

bool PetRecallFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const&,
                   BotIntentEmitter& emit, uint32)
{
    constexpr uint8 COMMAND_FOLLOW_LOCAL = 1;
    if (!emit.pet_set_command_state(COMMAND_FOLLOW_LOCAL)) return false;
    const uint32 now_ms = s.published_at_ms();
    // Per-bot recall state is keyed by ActionKind; the action key
    // itself just needs a stable disambiguator within the bot's
    // history. Fixed 1 since there is only one recall channel.
    constexpr uint64 key = 1u;
    ai.note_action_retry(BotAI::ActionKind::PetRecall, key, now_ms);
    ai.set_last_rule_fired("idle:pet_recall");
    return true;
}

// ---------- idle:assist_pet ----------
// Pet-class bots (hunter / warlock / DK / shaman elementals / evoker /
// mage water-elemental) previously stood idle while their pet got
// chewed on by a mob: snapshot only tracked pet_in_combat (bool) and
// nothing about who the pet was fighting. Real players see "pet on
// a mob" and assist — open up, share threat, kill it together.
//
// Gate: pet alive + in combat + bot itself NOT in combat / casting /
// mounted / dead. Pet's victim must resolve to a hostile in the bot's
// nearby_enemies (else it's out-of-snapshot and we'd just spam
// start_attack on a guid the engagement loop can't reach).
//
// Fire: emit start_attack on the pet's victim. The engagement loop
// takes over (path-find / cast / melee). If the bot was already
// targeting that guid this tick, the intent dedupes server-side.
// Pick the target the bot should engage to support its pet. Peel
// before assist:
//   1. Walk pet_attackers; among those visible in nearby_enemies,
//      prefer the first one whose guid != pet_victim — that's the
//      mob nuking the pet while pet is busy on someone else.
//   2. Otherwise fall back to pet_victim (standard assist case:
//      pet is engaged and the same mob is the only attacker).
// Returns Empty when nothing actionable exists.
// NOTE: SelectAssistPetTarget is defined in the Playerbot::States namespace
// (above this anonymous block, declared in MaintainHelpers.h) so State_InCombat
// can reuse it — see the assist-pet target acquisition in DispatchInCombat.

bool AssistPetGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!s.has_pet()) return false;
    if (!s.pet_in_combat()) return false;
    if (s.in_combat()) return false;                  // already fighting
    if (s.is_casting()) return false;                 // don't break a cast
    if (!s.is_alive()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.in_battleground()) return false;            // BG scripts own engagement
    // Pet must have either a victim or at least one visible attacker
    // we could peel; SelectAssistPetTarget returns Empty when neither.
    return !SelectAssistPetTarget(s).IsEmpty();
}

bool AssistPetFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const&,
                   BotIntentEmitter& emit, uint32)
{
    ObjectGuid const target = SelectAssistPetTarget(s);
    if (target.IsEmpty()) return false;
    if (!emit.start_attack(target)) return false;
    ai.set_last_rule_fired(
        target == s.pet_victim() ? "idle:assist_pet" : "idle:assist_pet_peel");
    return true;
}

// ---------- idle:pet_heal (hunter Mend Pet) ----------
bool PetHealGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    constexpr uint8 CLASS_HUNTER_LOCAL = 3;
    if (s.cls() != CLASS_HUNTER_LOCAL) return false;
    if (!s.has_pet()) return false;
    if (s.in_combat() || s.is_casting()) return false;
    // Combat-flicker gate: Mend Pet is a 1.5s cast that breaks if a mob
    // hits the bot or the pet. Matches the sibling pattern in
    // MaintainOocFood / OocBandageGate / OocEatFoodGate / GatherGate.
    if (s.recently_in_combat(5000)) return false;
    const int32 pet_hp = s.pet_hp_pct();
    if (pet_hp <= 0 || pet_hp >= 80) return false;
    constexpr uint32 MEND_PET = 136;
    return s.knows_spell(MEND_PET) && s.is_ready(MEND_PET);
}

bool PetHealFire(BotSnapshotView const& s, BotAI& ai,
                 GroupSnapshotView const&,
                 BotIntentEmitter& emit, uint32)
{
    constexpr uint32 MEND_PET = 136;
    if (!emit.cast(MEND_PET, s.pet_guid())) return false;
    ai.set_last_rule_fired("idle:pet_heal");
    return true;
}

// ---------- idle:group_utility ----------
bool GroupUtilityFire(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const& g,
                      BotIntentEmitter& emit, uint32)
{
    if (!MaintainGroupUtility(s, g, emit)) return false;
    ai.set_last_rule_fired("idle:group_utility");
    return true;
}

// ---------- idle:conjured_item ----------
bool ConjuredItemFire(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const&,
                      BotIntentEmitter& emit, uint32)
{
    if (!MaintainConjuredItem(s, emit)) return false;
    ai.set_last_rule_fired("idle:conjured_item");
    return true;
}

// ---------- idle:ooc_food ----------
bool OocFoodFire(BotSnapshotView const& s, BotAI& ai,
                 GroupSnapshotView const&,
                 BotIntentEmitter& emit, uint32)
{
    if (!MaintainOocFood(s, ai, emit)) return false;
    ai.set_last_rule_fired("idle:ooc_food");
    return true;
}

} // anonymous namespace

void RegisterMaintainRules(IdleRuleRegistry& r)
{
    // Higher priority fires first within bottom dispatch — preserve the
    // legacy inline order (starter_talents -> ooc_food).
    auto add = [&r](char const* name, int prio,
                    bool (*fire)(BotSnapshotView const&, BotAI&,
                                 GroupSnapshotView const&,
                                 BotIntentEmitter&, uint32))
    {
        IdleRule rule;
        rule.name     = name;
        rule.priority = prio;
        rule.gate     = &AlwaysGate;
        rule.fire     = fire;
        r.register_rule(std::move(rule));
    };
    add("idle:starter_talents",       696, &StarterTalentsFire);
    add("idle:starter_extend",        694, &StarterExtendFire);
    add("idle:apply_context_talents", 692, &ContextTalentsFire);
    add("idle:auto_equip",            690, &AutoEquipFire);
    add("idle:equip_bag_upgrade",     689, &BagUpgradeFire);
    add("idle:ooc_rez",               688, &OocRezFire);
    add("idle:ooc_heal",              686, &OocHealFire);
    add("idle:ooc_dispel",            684, &OocDispelFire);
    add("idle:self_buff",             682, &SelfBuffFire);
    add("idle:soulstone",             680, &SoulstoneFire);
    // 731 (was 678, inside the OOC maintenance band BELOW the quest funnel).
    // A pet class is crippled without its pet — a pet-less Warlock/Hunter does a
    // fraction of its damage and has no tank, so it cannot kill even normal
    // quest mobs and dies in a loop (observed: Morthan L7 Warlock, pet-less,
    // unable to clear normal Scarlet Zealots/Missionaries in Tirisfal q24981).
    // At 678 the summon was STARVED: idle:pursue_quest_goal (698) and the rest
    // of the quest funnel (698-730) fired first every OOC tick, walking the bot
    // back into combat before MaintainPet could run — so it never re-summoned.
    // Raised ABOVE the quest funnel (below critical_repair 735 / survival 830+):
    // a pet-less bot summons its pet BEFORE re-engaging. PetFire returns false
    // immediately when the bot already HAS a pet, so this only affects pet-less
    // bots; the 30s pet_summon backoff (after each attempt) keeps the quest
    // funnel from being starved if the summon can't currently land; and
    // pursue_quest_goal gates on !is_casting(), so it won't cancel the 6s summon
    // cast mid-flight. The recently_in_combat(5s) gate still defers the cast to a
    // genuine OOC window (e.g. after a graveyard rez).
    add("idle:pet",                   731, &PetFire);
    // pet_heal uses a real gate (cheap), the rest use AlwaysGate.
    {
        IdleRule rule;
        rule.name     = "idle:pet_heal";
        rule.priority = 676;
        rule.gate     = &PetHealGate;
        rule.fire     = &PetHealFire;
        r.register_rule(std::move(rule));
    }
    // assist_pet sits at survival-tier priority (707) so it fires
    // BEFORE assist_friend_pvp (705) and well above all maintain /
    // social rules: a pet under attack is the bot's own emergency,
    // not a peer's. Real gate is cheap (≤3 ObjectGuid compares).
    {
        IdleRule rule;
        rule.name     = "idle:assist_pet";
        rule.priority = 707;
        rule.gate     = &AssistPetGate;
        rule.fire     = &AssistPetFire;
        r.register_rule(std::move(rule));
    }
    // pet_recall fires ABOVE assist_pet (708) because if the pet is
    // dying AND no attacker is reachable, recall is the only save.
    // Both rules check pet_in_combat — recall takes precedence when
    // its conditions hold, otherwise assist_pet handles the normal
    // case.
    {
        IdleRule rule;
        rule.name     = "idle:pet_recall";
        rule.priority = 708;
        rule.gate     = &PetRecallGate;
        rule.fire     = &PetRecallFire;
        r.register_rule(std::move(rule));
    }
    add("idle:group_utility",         674, &GroupUtilityFire);
    add("idle:conjured_item",         672, &ConjuredItemFire);
    add("idle:ooc_food",              671, &OocFoodFire);
}

} // namespace Playerbot
