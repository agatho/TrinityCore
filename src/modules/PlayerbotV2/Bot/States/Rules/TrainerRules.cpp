// TrainerRules - Refactor #3 pass 10. Migrates idle:trainer (auto-bulk-
// train at any UNIT_NPC_FLAG_TRAINER within 5y when bot has >= 1g) and
// idle:dual_spec_switch (owner-pinned-role -> off-spec activation) out
// of the State_Idle linear cascade. Both fire at the same priority band
// just below the guild family — utility services for a bot that's
// already parked at a service NPC or has owner-pinned role mismatch.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Bot/ClassTables.h"
#include "GameTime.h"
#include "UnitDefines.h"

namespace Playerbot {

namespace {

// ---------- idle:trainer ----------
constexpr int32 kAutoTrainMinGold = 10000;   // 1 gold in copper
constexpr float kTrainerInteract = 5.0f;

bool TrainerGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    if (s.gold() < kAutoTrainMinGold) return false;
    auto const* trainer = s.nearest_npc_with_flag(UNIT_NPC_FLAG_TRAINER);
    if (!trainer) return false;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = trainer->x - bx, dy = trainer->y - by, dz = trainer->z - bz;
    if (dx*dx + dy*dy + dz*dz > kTrainerInteract * kTrainerInteract) return false;
    // Per-NPC-guid 15min backoff after the most recent emit. Critical in
    // capital quarters where the nearest UNIT_NPC_FLAG_TRAINER flips
    // between adjacent NPCs (class trainer / profession expert / secondary
    // supply trainer all within 3-5y of each other). Without per-guid
    // backoff, a bot whose nearest trainer can't actually teach it
    // anything walks back-and-forth between adjacent trainers
    // indefinitely — observed live as bot Uraimus pinging between a
    // Leatherworking expert and an adjacent class trainer after every
    // trainer_buy_all returned ServerRefused.
    const uint32 now_ms = GameTime::GetGameTimeMS();
    if (ai.action_recently_tried(BotAI::ActionKind::TrainerLearn,
                                 trainer->guid.GetCounter(), now_ms))
        return false;
    return true;
}

bool TrainerFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    auto const* trainer = s.nearest_npc_with_flag(UNIT_NPC_FLAG_TRAINER);
    if (!trainer) return false;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = trainer->x - bx, dy = trainer->y - by, dz = trainer->z - bz;
    if (dx*dx + dy*dy + dz*dz > kTrainerInteract * kTrainerInteract)
        return false;
    emit.trainer_buy_all(trainer->guid);
    const uint32 now_ms = GameTime::GetGameTimeMS();
    ai.note_action_retry(BotAI::ActionKind::TrainerLearn,
                         trainer->guid.GetCounter(), now_ms);
    ai.set_last_rule_fired("idle:trainer");
    return true;
}

// ---------- idle:dual_spec_switch ----------
bool DualSpecGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    const Role pinned = ai.effective_role(s);
    if (pinned == Role::Unknown || pinned == Role::Dps) return false;
    if (pinned == s.my_role()) return false;
    const uint8 cls = s.cls();
    const uint32 target_spec =
        (pinned == Role::Tank)   ? TankSpecForClass(cls) :
        (pinned == Role::Healer) ? HealerSpecForClass(cls) :
                                    0u;
    if (target_spec == 0) return false;
    if (uint32(s.spec()) == target_spec) return false;
    const uint32 ds_now_ms = GameTime::GetGameTimeMS();
    const uint64 ds_key = uint64(s.bot_id());
    return !ai.action_recently_tried(BotAI::ActionKind::DualSpec, ds_key, ds_now_ms);
}

bool DualSpecFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    const Role pinned = ai.effective_role(s);
    const uint8 cls = s.cls();
    const uint32 target_spec =
        (pinned == Role::Tank)   ? TankSpecForClass(cls) :
        (pinned == Role::Healer) ? HealerSpecForClass(cls) :
                                    0u;
    if (target_spec == 0) return false;
    emit.emit(ActivateSpecIntent{target_spec});
    const uint32 ds_now_ms = GameTime::GetGameTimeMS();
    ai.note_action_retry(BotAI::ActionKind::DualSpec,
                         uint64(s.bot_id()), ds_now_ms);
    ai.set_last_rule_fired("idle:dual_spec_switch");
    return true;
}

} // anonymous namespace

void RegisterTrainerRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:trainer";
        rule.priority = 230;
        rule.gate     = &TrainerGate;
        rule.fire     = &TrainerFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:dual_spec_switch";
        rule.priority = 228;
        rule.gate     = &DualSpecGate;
        rule.fire     = &DualSpecFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
