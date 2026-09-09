/*
 * SpawnModel - in-memory edit log for creature + gameobject rows.
 *
 * Mirrors AnnotationModel: holds the baseline DB snapshot, the current
 * (locally-mutated) list, and a per-row ChangeRecord enum (Insert /
 * Update / Delete). SpawnCommitDialog turns the changelog into SQL.
 *
 * Identity model:
 *   - DB-backed rows have guid > 0.
 *   - Locally-created rows have guid <= 0; SpawnCommitDialog assigns
 *     the real auto_increment guid range on commit (Phase 3c work).
 *     Phase 3a does not yet support Insert, only Update + Delete.
 *
 * One model holds BOTH creature and gameobject rows. The kind enum on
 * each Spawn drives which DB table the commit code targets.
 */

#pragma once

#include "../core/model/WorldEntities.h"

#include <cstdint>
#include <vector>

namespace world_editor::db
{

enum class SpawnChangeKind : uint8_t
{
    None   = 0,
    Insert = 1,
    Update = 2,
    Delete = 3,
};

struct SpawnChangeRecord
{
    SpawnChangeKind     kind = SpawnChangeKind::None;
    render::Spawn       before; // valid for Update + Delete
    render::Spawn       after;  // valid for Insert + Update
};

class SpawnModel
{
public:
    void setBaseline(std::vector<render::Spawn> rows);

    [[nodiscard]] std::vector<render::Spawn>       const& current() const noexcept { return m_current; }
    [[nodiscard]] std::vector<SpawnChangeRecord>   const& changes() const noexcept { return m_changes; }
    [[nodiscard]] size_t pendingCount() const;

    // Mutators. Each returns true if the row was actually changed (a
    // no-op write is collapsed back to None).
    int  addNew(render::Spawn row);                 // Phase 3c
    bool replaceRow(int index, render::Spawn const& newRow);
    bool removeRow(int index);

    void revertAll();
    void acceptCommit(std::vector<render::Spawn> committed);

    struct StateSnapshot
    {
        std::vector<render::Spawn>     baseline;
        std::vector<render::Spawn>     current;
        std::vector<SpawnChangeRecord> changes;
        int64_t                        nextLocalGuid = -1;
    };
    [[nodiscard]] StateSnapshot captureState() const;
    void restoreState(StateSnapshot const& s);

private:
    void recordUpdate(int index);

    std::vector<render::Spawn>     m_baseline;
    std::vector<render::Spawn>     m_current;
    std::vector<SpawnChangeRecord> m_changes;
    int64_t                        m_nextLocalGuid = -1;
};

} // namespace world_editor::db
