// BotCoordinationBus - see header.

#include "BotCoordinationBus.h"

#include "Log.h"

namespace Playerbot::V2 {

namespace {
constexpr size_t kMaxSignalIndex = 64;
}

void BotCoordinationBus::Subscribe(CoordSignal signal, Handler handler, CoordDispatch mode)
{
    const size_t idx = static_cast<size_t>(signal);
    if (idx >= kMaxSignalIndex)
    {
        TC_LOG_ERROR("playerbot.v2",
            "[BotCoordinationBus] Subscribe out-of-range signal={}", idx);
        return;
    }
    if (subs_by_signal_.size() <= idx)
        subs_by_signal_.resize(idx + 1);
    subs_by_signal_[idx].push_back({std::move(handler), mode});
}

void BotCoordinationBus::Publish(CoordEvent const& ev)
{
    const size_t idx = static_cast<size_t>(ev.kind);
    if (idx == 0 || idx >= subs_by_signal_.size()) return;
    auto const& subs = subs_by_signal_[idx];
    for (auto const& s : subs)
    {
        if (s.mode == CoordDispatch::Sync)
        {
            try { s.fn(ev); }
            catch (std::exception const& e)
            {
                TC_LOG_ERROR("playerbot.v2",
                    "[BotCoordinationBus] Sync handler threw for signal={}: {}",
                    idx, e.what());
            }
            catch (...)
            {
                TC_LOG_ERROR("playerbot.v2",
                    "[BotCoordinationBus] Sync handler threw unknown exception for signal={}",
                    idx);
            }
        }
    }
    // Async handlers: queue the event; the actual fan-out happens in
    // DrainAsync. We re-publish per-async-handler so a single async
    // event hitting multiple handlers is one queue entry per handler.
    bool has_async = false;
    for (auto const& s : subs) { if (s.mode == CoordDispatch::Async) { has_async = true; break; } }
    if (has_async)
        pending_async_.push_back(ev);
}

void BotCoordinationBus::DrainAsync()
{
    if (pending_async_.empty()) return;
    // Snapshot to a local vector — handlers MAY publish new events
    // (e.g. group formation publishes follow-up signals) and we
    // don't want unbounded recursion in this call.
    std::vector<CoordEvent> work;
    work.swap(pending_async_);
    for (auto const& ev : work)
    {
        const size_t idx = static_cast<size_t>(ev.kind);
        if (idx == 0 || idx >= subs_by_signal_.size()) continue;
        auto const& subs = subs_by_signal_[idx];
        for (auto const& s : subs)
        {
            if (s.mode != CoordDispatch::Async) continue;
            try { s.fn(ev); }
            catch (std::exception const& e)
            {
                TC_LOG_ERROR("playerbot.v2",
                    "[BotCoordinationBus] Async handler threw for signal={}: {}",
                    idx, e.what());
            }
            catch (...)
            {
                TC_LOG_ERROR("playerbot.v2",
                    "[BotCoordinationBus] Async handler threw unknown exception for signal={}",
                    idx);
            }
        }
    }
}

} // namespace Playerbot::V2
