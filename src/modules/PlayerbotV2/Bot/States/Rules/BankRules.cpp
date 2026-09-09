// BankRules - Refactor #3 pass 6. Migrates the bank-deposit, guild-bank-
// deposit-money, and taxi-discover idle rules out of the State_Idle
// linear cascade.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "UnitDefines.h"

namespace Playerbot {

namespace {

constexpr float kInteractSq = 5.0f * 5.0f;

NearbyUnit const* InRangeBanker(BotSnapshotView const& s)
{
    auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_BANKER);
    if (!npc) return nullptr;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
    return (dx*dx + dy*dy + dz*dz <= kInteractSq) ? npc : nullptr;
}

// ---------- idle:bank_deposit ----------
bool BankDepositGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    if (s.bag_free_slots() > 4) return false;
    if (s.bank_free_slots() == 0) return false;
    return InRangeBanker(s) != nullptr;
}

bool BankDepositFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    auto const* npc = InRangeBanker(s);
    if (!npc) return false;
    const uint64 banker_low = npc->guid.GetCounter();
    if (ai.action_recently_tried(BotAI::ActionKind::BankDeposit, banker_low, now_ms))
        return false;
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.is_quest_item) continue;
        if (it.bag >= 19 && it.bag <= 22 && it.slot == 0xFF) continue;
        emit.bank_deposit_item(npc->guid, it.bag, it.slot);
        ai.note_action_retry(BotAI::ActionKind::BankDeposit, banker_low, now_ms);
        ai.set_last_rule_fired("idle:bank_deposit");
        return true;
    }
    return false;
}

// ---------- idle:guild_bank_deposit_money ----------
bool GuildBankDepositGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    if (!s.in_guild()) return false;
    if (s.gold() < 500000) return false;   // 50g
    if (s.in_combat()) return false;
    auto const* vault = s.nearest_object_of_type(/*GAMEOBJECT_TYPE_GUILD_BANK*/ 34);
    if (!vault) return false;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = vault->x - bx, dy = vault->y - by, dz = vault->z - bz;
    return dx*dx + dy*dy + dz*dz <= kInteractSq;
}

bool GuildBankDepositFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    auto const* vault = s.nearest_object_of_type(/*GAMEOBJECT_TYPE_GUILD_BANK*/ 34);
    if (!vault) return false;
    const uint64 vault_low = vault->guid.GetCounter();
    if (ai.action_recently_tried(BotAI::ActionKind::GuildBankDep, vault_low, now_ms))
        return false;
    const uint64 ceiling = 500000;
    const uint64 overage = uint64(s.gold()) - ceiling;
    const uint64 deposit = (overage * 80) / 100;
    if (deposit < 100000) return false;   // at least 10g
    emit.emit(GuildBankDepositMoneyIntent{vault->guid, deposit});
    ai.note_action_retry(BotAI::ActionKind::GuildBankDep, vault_low, now_ms);
    ai.set_last_rule_fired("idle:guild_bank_deposit_money");
    return true;
}

// ---------- idle:taxi_discover ----------
// The original block has an "else" branch that clears state when no FM is
// nearby. That side effect can't run inside fire() because gate() blocks
// us when no FM exists. We accept the small behavior gap: state clears
// when the bot enters a state where the rule registry doesn't dispatch
// taxi_discover (which still naturally happens on out-of-range).
bool TaxiDiscoverGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    auto const* fm = s.nearest_npc_with_flag(UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!fm)
    {
        if (!ai.last_taxi_discover_fm().IsEmpty())
            ai.set_last_taxi_discover_fm(ObjectGuid::Empty);
        return false;
    }
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = fm->x - bx, dy = fm->y - by, dz = fm->z - bz;
    if (dx*dx + dy*dy + dz*dz > kInteractSq)
    {
        if (!ai.last_taxi_discover_fm().IsEmpty())
            ai.set_last_taxi_discover_fm(ObjectGuid::Empty);
        return false;
    }
    return ai.last_taxi_discover_fm() != fm->guid;
}

bool TaxiDiscoverFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    auto const* fm = s.nearest_npc_with_flag(UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!fm) return false;
    emit.discover_taxi_node(fm->guid);
    ai.set_last_taxi_discover_fm(fm->guid);
    ai.set_last_rule_fired("idle:taxi_discover");
    return true;
}

} // anonymous namespace

void RegisterBankRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:bank_deposit";
        rule.priority = 380;
        rule.gate     = &BankDepositGate;
        rule.fire     = &BankDepositFire;
        // Banker interaction is a single transaction at the banker NPC.
        // 3s throttle is well under the time it takes the bot to walk
        // there and approach.
        rule.min_interval_ms = 3000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_bank_deposit_money";
        rule.priority = 370;
        rule.gate     = &GuildBankDepositGate;
        rule.fire     = &GuildBankDepositFire;
        // Guild bank deposit happens at most every guild visit; 3s is
        // generous.
        rule.min_interval_ms = 3000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:taxi_discover";
        rule.priority = 360;
        rule.gate     = &TaxiDiscoverGate;
        rule.fire     = &TaxiDiscoverFire;
        // Taxi discovery is a one-shot per flightmaster visit; the bot
        // is stationary at the flightmaster while it fires.
        rule.min_interval_ms = 3000;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
