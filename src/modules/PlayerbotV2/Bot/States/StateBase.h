// StateBase - Dispatch interface for top-level states. CONTRACTS.md §2.7.
//
// Each State_*.cpp exports one DispatchXxx free function with this signature.
// BotAI::tick selects which to call based on current BotState. State files
// are stateless — all state lives in BotAI / BotSnapshot / BotPersonality.
//
// Note 2026-05-21: BotEventInbox parameter removed. The inbox had zero
// consumers and the snapshot covers every actual rule need.

#pragma once

#include "Bot/BotTypes.h"

namespace Playerbot {

class BotAI;
class BotSnapshotView;
class GroupSnapshotView;
class BotIntentEmitter;

namespace States {

// Primary states (mutually exclusive)
void DispatchLoggingIn  (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchLoggingOut (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchIdle       (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchTravelling (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchQuesting   (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchInCombat   (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchLooting    (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchDead       (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchResurrecting(BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);

// Cross-cutting layers (compose over a primary state)
void DispatchInGroup    (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchInInstance (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchAtVendor   (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchAtMailbox  (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchAtAh       (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchDecorating (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);

} // namespace States
} // namespace Playerbot
