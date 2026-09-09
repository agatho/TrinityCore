/*
 * SmartScriptModel - in-memory edit log for `smart_scripts` rows.
 *
 * Mirrors AreatriggerModel: baseline DB snapshot + current (locally-
 * mutated) list + per-row ChangeRecord (Insert / Update / Delete).
 * SmartScriptCommitDialog turns the changelog into SQL inside a single
 * transaction.
 *
 * Identity: the composite PRIMARY KEY is
 * (entryorguid, source_type, id, link).  All four columns participate
 * in lookup; setBaseline / replaceRow / removeRow / acceptCommit all
 * match rows by the full 4-tuple.  This differs from the single-PK
 * tables (areatrigger, world_safe_locs) where SpawnId alone suffices.
 *
 * Reserved (entryorguid, source_type, id, link) tuples for new
 * placements are owner-assigned -- the most common pattern is a fresh
 * id for an existing entryorguid (creature template) or a chained
 * link>0 row continuing an existing id.  We do not pre-reserve from
 * MAX(id)+1 because the picker UI lets the operator drive that.
 */

#pragma once

#include "../core/model/WorldEntities.h"

#include <cstdint>
#include <vector>

namespace world_editor::db
{

enum class SmartScriptChangeKind : uint8_t
{
    None   = 0,
    Insert = 1,
    Update = 2,
    Delete = 3,
};

struct SmartScriptChangeRecord
{
    SmartScriptChangeKind kind = SmartScriptChangeKind::None;
    render::SmartScript   before; // valid for Update + Delete
    render::SmartScript   after;  // valid for Insert + Update
};

class SmartScriptModel
{
public:
    void setBaseline(std::vector<render::SmartScript> rows);

    [[nodiscard]] std::vector<render::SmartScript>      const& current() const noexcept { return m_current; }
    [[nodiscard]] std::vector<SmartScriptChangeRecord>  const& changes() const noexcept { return m_changes; }
    [[nodiscard]] size_t pendingCount() const;

    // Returns the index of the new row in current(), or -1 on rejection.
    // Rejects on duplicate composite-PK collision against existing rows.
    int  addNew(render::SmartScript row);
    bool replaceRow(int index, render::SmartScript const& newRow);
    bool removeRow(int index);

    void revertAll();
    void acceptCommit(std::vector<render::SmartScript> committed);

    struct StateSnapshot
    {
        std::vector<render::SmartScript>     baseline;
        std::vector<render::SmartScript>     current;
        std::vector<SmartScriptChangeRecord> changes;
    };
    [[nodiscard]] StateSnapshot captureState() const;
    void restoreState(StateSnapshot const& s);

private:
    void recordUpdate(int index);
    [[nodiscard]] int findInBaseline(render::SmartScript const& row) const;
    [[nodiscard]] int findInCurrent (render::SmartScript const& row) const;

    std::vector<render::SmartScript>      m_baseline;
    std::vector<render::SmartScript>      m_current;
    std::vector<SmartScriptChangeRecord>  m_changes;
};

} // namespace world_editor::db
