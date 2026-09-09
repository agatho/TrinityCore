// GroupSnapshotBuilder - Constructs a GroupSnapshot for a member's bot. Runs
// on the world thread once per snapshot publish; the resulting object is
// immutable and safe for AI worker consumption.

#pragma once

#include "GroupSnapshot.h"
#include <memory>

class Player;

namespace Playerbot {

class GroupSnapshotBuilder
{
public:
    // Returns nullptr if the player isn't grouped or isn't in the world.
    // Otherwise builds a fresh GroupSnapshot summarizing every member.
    static std::shared_ptr<GroupSnapshot const> Build(Player* leader_member, SnapshotVer version);
};

} // namespace Playerbot
