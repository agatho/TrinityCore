/*
 * WaypointModel - in-memory edit log for waypoint_path + waypoint_path_node.
 *
 * Each Path carries its own node list; node-level edits are tracked
 * inside the Path's ChangeRecord (no separate node changelog).  When
 * a path is Insert/Update, ALL its nodes are written; when Delete,
 * the node rows are removed first.  This is simpler than a per-node
 * changelog and matches how the schema is used in practice (paths
 * are rewritten as a unit when they're edited at all).
 *
 * Identity:
 *   - DB-backed paths have pathId > 0 (the real PathId).
 *   - Locally-created paths have pathId reserved from MAX+1 via
 *     MainWindow on connect / post-commit (mirrors GUID reservation
 *     for creature/gameobject; see HANDOFF section 10.3).
 */

#pragma once

#include "../core/model/WorldEntities.h"

#include <cstdint>
#include <vector>

namespace world_editor::db
{

enum class PathChangeKind : uint8_t
{
    None   = 0,
    Insert = 1,
    Update = 2,
    Delete = 3,
};

struct PathChangeRecord
{
    PathChangeKind kind = PathChangeKind::None;
    render::Path   before; // valid for Update + Delete
    render::Path   after;  // valid for Insert + Update
};

class WaypointModel
{
public:
    void setBaseline(std::vector<render::Path> rows);
    [[nodiscard]] std::vector<render::Path>      const& current() const noexcept { return m_current; }
    [[nodiscard]] std::vector<PathChangeRecord>  const& changes() const noexcept { return m_changes; }
    [[nodiscard]] size_t pendingCount() const;

    int  addNewPath(render::Path row);                   // caller passes reserved PathId
    bool replacePath(int index, render::Path const& newRow);
    bool removePath (int index);
    void revertAll();
    void acceptCommit(std::vector<render::Path> committed);

    // Find current() index for a known pathId, or -1.
    [[nodiscard]] int indexForPathId(uint32_t pathId) const;

    struct StateSnapshot
    {
        std::vector<render::Path>     baseline;
        std::vector<render::Path>     current;
        std::vector<PathChangeRecord> changes;
    };
    [[nodiscard]] StateSnapshot captureState() const;
    void restoreState(StateSnapshot const& s);

private:
    void recordUpdate(int index);

    std::vector<render::Path>      m_baseline;
    std::vector<render::Path>      m_current;
    std::vector<PathChangeRecord>  m_changes;
};

} // namespace world_editor::db
