/*
 * AreatriggerModel - in-memory edit log for `areatrigger` rows.
 *
 * Mirrors SpawnModel: baseline DB snapshot + current (locally-mutated)
 * list + per-row ChangeRecord (Insert / Update / Delete).
 * AreatriggerCommitDialog turns the changelog into SQL inside a single
 * transaction.
 *
 * Identity: the PRIMARY KEY on `areatrigger` is SpawnId alone. Reserved
 * SpawnIds for new placements are assigned up-front from MainWindow's
 * MAX(SpawnId)+1 block so there is no auto_increment race (HANDOFF
 * section 10.3 - same pattern as creature.guid + waypoint_path.PathId).
 *
 * Shape data (joined from areatrigger_create_properties) is render-only;
 * editing it would mutate a shared row across many spawns, so this model
 * never writes back the shape columns.
 */

#pragma once

#include "../core/model/WorldEntities.h"

#include <cstdint>
#include <vector>

namespace world_editor::db
{

enum class AreatriggerChangeKind : uint8_t
{
    None   = 0,
    Insert = 1,
    Update = 2,
    Delete = 3,
};

struct AreatriggerChangeRecord
{
    AreatriggerChangeKind kind = AreatriggerChangeKind::None;
    render::Areatrigger   before; // valid for Update + Delete
    render::Areatrigger   after;  // valid for Insert + Update
};

class AreatriggerModel
{
public:
    void setBaseline(std::vector<render::Areatrigger> rows);

    [[nodiscard]] std::vector<render::Areatrigger>      const& current() const noexcept { return m_current; }
    [[nodiscard]] std::vector<AreatriggerChangeRecord>  const& changes() const noexcept { return m_changes; }
    [[nodiscard]] size_t pendingCount() const;

    // Returns the index of the new row in current(), or -1 on rejection.
    int  addNew(render::Areatrigger row);
    bool replaceRow(int index, render::Areatrigger const& newRow);
    bool removeRow(int index);

    void revertAll();
    void acceptCommit(std::vector<render::Areatrigger> committed);

    struct StateSnapshot
    {
        std::vector<render::Areatrigger>     baseline;
        std::vector<render::Areatrigger>     current;
        std::vector<AreatriggerChangeRecord> changes;
    };
    [[nodiscard]] StateSnapshot captureState() const;
    void restoreState(StateSnapshot const& s);

private:
    void recordUpdate(int index);

    std::vector<render::Areatrigger>      m_baseline;
    std::vector<render::Areatrigger>      m_current;
    std::vector<AreatriggerChangeRecord>  m_changes;
};

} // namespace world_editor::db
