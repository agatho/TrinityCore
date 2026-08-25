// HunterPetRules - Refactor #3 pass 10. Migrates idle:tame_beast (petless
// hunter auto-Tame on a nearby same/lower-level Beast) and idle:pet_swap_
// from_stable (petless hunter at a stablemaster swaps the first stabled
// pet into the active slot) out of the State_Idle cascade. Both fire
// only for the Hunter class — the gates short-circuit cheaply.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "GameTime.h"

namespace Playerbot {

namespace {

constexpr uint8 CLASS_HUNTER_LOCAL = 3;

// ---------- idle:tame_beast ----------
constexpr uint32 SPELL_TAME_BEAST = 1515;

bool TameBeastGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    if (s.cls() != CLASS_HUNTER_LOCAL) return false;
    if (s.has_pet()) return false;
    if (s.level() < 10) return false;
    if (s.in_combat() || s.is_casting() || s.raw().movement.is_mounted) return false;
    // Combat-flicker gate: Tame Beast is a 6s channel that breaks on
    // damage. Don't start it in the brief OOC window between pulls — the
    // tame attempt would die to the next mob's first hit, and TameBeast
    // has a 5min CD when it fully completes (no CD when interrupted, so
    // bot would retry indefinitely, exhausting the engageable beasts list).
    if (s.recently_in_combat(5000)) return false;
    return s.knows_spell(SPELL_TAME_BEAST) && s.is_ready(SPELL_TAME_BEAST);
}

bool TameBeastFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    const uint32 tb_now_ms = GameTime::GetGameTimeMS();
    float bx, by, bz; s.position(bx, by, bz);
    for (auto const& u : s.raw().combat.nearby_enemies)
    {
        if (!u.guid.IsCreature()) continue;
        if (u.creature_type != /*Beast*/ 1) continue;
        if (u.hp <= 0) continue;
        if (u.level > s.level()) continue;
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        if (dx*dx + dy*dy + dz*dz > 30.0f * 30.0f) continue;
        const uint64 tb_key = u.guid.GetCounter();
        if (ai.action_recently_tried(BotAI::ActionKind::TameBeast,
                                      tb_key, tb_now_ms))
            continue;
        if (emit.cast(SPELL_TAME_BEAST, u.guid))
        {
            ai.note_action_retry(BotAI::ActionKind::TameBeast,
                                 tb_key, tb_now_ms);
            ai.set_last_rule_fired("idle:tame_beast");
            return true;
        }
    }
    return false;
}

// ---------- idle:pet_swap_from_stable ----------
constexpr uint32 UNIT_NPC_FLAG_STABLEMASTER_LOCAL = 0x00400000u;
constexpr float kStableInteract = 5.0f;

bool PetSwapGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    if (s.cls() != CLASS_HUNTER_LOCAL) return false;
    if (s.has_pet()) return false;
    if (!s.has_stabled_pets()) return false;
    if (s.in_combat() || s.is_casting()) return false;
    auto const* sm = s.nearest_npc_with_flag(UNIT_NPC_FLAG_STABLEMASTER_LOCAL);
    if (!sm) return false;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = sm->x - bx, dy = sm->y - by, dz = sm->z - bz;
    return dx*dx + dy*dy + dz*dz <= kStableInteract * kStableInteract;
}

bool PetSwapFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    auto const* sm = s.nearest_npc_with_flag(UNIT_NPC_FLAG_STABLEMASTER_LOCAL);
    if (!sm) return false;
    const uint32 ps_now_ms = GameTime::GetGameTimeMS();
    const uint64 ps_key = sm->guid.GetCounter();
    if (ai.action_recently_tried(BotAI::ActionKind::PetSwap, ps_key, ps_now_ms))
        return false;
    BotSnapshot::StablePet const* pick = nullptr;
    for (auto const& sp : s.stable_pets())
    {
        if (sp.slot_kind == 0) continue;
        pick = &sp;
        break;
    }
    if (!pick) return false;
    emit.emit(HunterPetIntent{SwapPetToSlotIntent{
        pick->pet_number,
        /*dst_slot=*/0}});
    ai.note_action_retry(BotAI::ActionKind::PetSwap, ps_key, ps_now_ms);
    ai.set_last_rule_fired("idle:pet_swap_from_stable");
    return true;
}

} // anonymous namespace

void RegisterHunterPetRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:tame_beast";
        rule.priority = 226;
        rule.gate     = &TameBeastGate;
        rule.fire     = &TameBeastFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:pet_swap_from_stable";
        rule.priority = 224;
        rule.gate     = &PetSwapGate;
        rule.fire     = &PetSwapFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
