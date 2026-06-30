#include "BotIntentEmitter.h"
#include "BotAI.h"
#include "Threading/IntentQueue.h"
#include "../Services.h"
#include "../Diagnostics/PerfCounters.h"

namespace Playerbot {

bool BotIntentEmitter::push(Intent i)
{
    if (!queue_) return false;
    if (!queue_->push(std::move(i)))
    {
        // Per-bot ring overflow. The intent is silently dropped — the
        // rotation will re-fire next tick. We track here so SystemStatus
        // surfaces a sustained backlog as a real signal rather than
        // silently degrading bot responsiveness.
        if (Services::Initialized())
            Services::Perf().record_intent_dropped();
        return false;
    }
    return true;
}

bool BotIntentEmitter::start_attack(ObjectGuid target)
{
    // Per-target lockout against the StartAttack ServerRefused loop. See
    // BotAI::kStartAttackLockoutMs. The lockout is set by the executor
    // when the API call returns ServerRefused (Player::Attack failed —
    // typically immune / phased / faction-locked target). Inside the
    // window the rule's emit becomes a silent no-op; outside, one retry
    // is allowed in case the underlying condition cleared.
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->start_attack_recently_refused(target.GetCounter(), now_ms))
            return false;
    }
    return emit(StartAttackIntent{target});
}

bool BotIntentEmitter::move_to(float x, float y, float z, bool run)
{
    // Per-bot dedup: skip if the previous move_to went to ~the same XYZ
    // within the lockout window. See BotAI::kMoveToEmitLockoutMs / the long
    // comment on note_move_to_emitted for the rationale (spline thrash,
    // Detour mid-curve reset, visible stutter).
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->move_to_recently_emitted(x, y, z, now_ms))
            return false;
        const bool pushed = emit(MoveToIntent{x, y, z, run});
        if (pushed)
            ai_->note_move_to_emitted(x, y, z, now_ms);
        return pushed;
    }
    return emit(MoveToIntent{x, y, z, run});
}

bool BotIntentEmitter::cast(uint32 spell_id, ObjectGuid target)
{
    // Optimistic per-spell emit dedup. The snapshot's spell_cooldowns
    // table reflects server-side cooldowns one world-tick after the cast
    // lands; in the ~200-1000ms gap, the AI worker may re-tick on the
    // same stale snapshot, see "is_ready=true" for a spell whose cast
    // just landed, and re-emit. Server rejects with SPELL_FAILED_NOT_READY
    // (91) — every rejection logs to Server.log and burns one intent slot.
    //
    // Solution: track per-spell emit timestamps in BotAI; drop duplicate
    // emits within kCastEmitLockoutMs (~1.5s, > snapshot cadence). The
    // next snapshot's real cooldown table picks up before the lockout
    // expires, so the AI never sees a "ready" window the server disagrees
    // with.
    //
    // Skipped when ai_ is null (test harness etc.) — the dedup is
    // strictly an optimization; correctness comes from the snapshot CDs.
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->cast_recently_emitted(spell_id, now_ms))
            return false;
        const bool pushed = emit(CastSpellIntent{spell_id, target});
        if (pushed)
            ai_->note_cast_emitted(spell_id, now_ms);
        return pushed;
    }
    return emit(CastSpellIntent{spell_id, target});
}

bool BotIntentEmitter::ah_buyout(ObjectGuid auctioneer, uint32 auction_id, uint64 price)
{
    // Per-auction_id dedup so the buy-side economy rule can fire every tick
    // it wants the listing without double-spending: one buyout emit per
    // auction per 30s window. Inside the window the emit is a silent no-op;
    // once the executor settles (Result::Ok) the snapshot's on-demand AH
    // scan drops the consumed listing so the rule naturally stops re-emitting.
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->action_recently_tried(BotAI::ActionKind::AhBuyout, auction_id, now_ms))
            return false;
        const bool pushed = emit(EconomyIntent{EconomyOp::AhBuyout{auctioneer, auction_id, price}});
        if (pushed)
            ai_->note_action_retry(BotAI::ActionKind::AhBuyout, auction_id, now_ms);
        return pushed;
    }
    return emit(EconomyIntent{EconomyOp::AhBuyout{auctioneer, auction_id, price}});
}

bool BotIntentEmitter::ah_bid(ObjectGuid auctioneer, uint32 auction_id, uint64 bid)
{
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->action_recently_tried(BotAI::ActionKind::AhBid, auction_id, now_ms))
            return false;
        const bool pushed = emit(EconomyIntent{EconomyOp::AhBid{auctioneer, auction_id, bid}});
        if (pushed)
            ai_->note_action_retry(BotAI::ActionKind::AhBid, auction_id, now_ms);
        return pushed;
    }
    return emit(EconomyIntent{EconomyOp::AhBid{auctioneer, auction_id, bid}});
}

bool BotIntentEmitter::ah_buy_commodity(ObjectGuid auctioneer, uint32 item_entry,
                                        uint32 quantity, uint64 max_total)
{
    // Per-item_entry dedup (AhBuyCommodity 30s). Unlike AhBuyout/AhBid the key
    // is the reagent ENTRY, not an auction_id: commodity listings collapse into
    // one bucket, so the buy-side rule wants one purchase per wanted reagent per
    // 30s window. Inside the window the emit is a silent no-op; once the
    // executor settles (Result::Ok) the on-demand AH scan re-derives the bot's
    // remaining shortfall so the rule naturally stops re-emitting for a topped-
    // up reagent.
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->action_recently_tried(BotAI::ActionKind::AhBuyCommodity, item_entry, now_ms))
            return false;
        const bool pushed = emit(EconomyIntent{EconomyOp::AhBuyCommodity{auctioneer, item_entry, quantity, max_total}});
        if (pushed)
            ai_->note_action_retry(BotAI::ActionKind::AhBuyCommodity, item_entry, now_ms);
        return pushed;
    }
    return emit(EconomyIntent{EconomyOp::AhBuyCommodity{auctioneer, item_entry, quantity, max_total}});
}

} // namespace Playerbot
