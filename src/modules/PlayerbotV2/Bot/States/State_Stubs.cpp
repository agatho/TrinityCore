// State_Stubs.cpp - Empty Dispatch* implementations for all states except
// Idle (which has its own file). As each state gains real logic it migrates
// to its own State_<Name>.cpp file per MODULE_LAYOUT.md.
//
// This file's existence is a deliberate compromise: 14 empty functions don't
// each deserve a file, but the dispatch surface needs to link cleanly. When
// any of these grows past 30 lines, split it out.

#include "StateBase.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Group/GroupSnapshot.h"

namespace Playerbot::States {

void DispatchLoggingIn  (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchLoggingOut (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchTravelling (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchQuesting   (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchLooting    (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchResurrecting(BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}

void DispatchInInstance (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchAtVendor   (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchAtMailbox  (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchAtAh       (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}
void DispatchDecorating (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&) {}

} // namespace Playerbot::States
