/*
 * ConditionsModel - in-memory edit log for `conditions` rows.
 *
 * Mirrors SmartScriptModel.  Baseline DB snapshot + current (locally-
 * mutated) list + per-row ChangeRecord (Insert / Update / Delete).
 * ConditionCommitDialog turns the changelog into SQL inside a single
 * transaction.
 *
 * Identity: the composite PRIMARY KEY is the full 11-tuple
 * (SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId,
 *  ElseGroup, ConditionTypeOrReference, ConditionTarget,
 *  ConditionValue1, ConditionValue2, ConditionValue3,
 *  ConditionStringValue1).  Mutating any of those = delete+insert at
 * commit time; UPDATE WHERE clauses also key on every PK column.
 */

#pragma once

#include "../core/model/WorldEntities.h"

#include <cstdint>
#include <vector>

namespace world_editor::db
{

enum class ConditionChangeKind : uint8_t
{
    None   = 0,
    Insert = 1,
    Update = 2,
    Delete = 3,
};

struct ConditionChangeRecord
{
    ConditionChangeKind kind = ConditionChangeKind::None;
    render::Condition   before; // valid for Update + Delete
    render::Condition   after;  // valid for Insert + Update
};

class ConditionsModel
{
public:
    void setBaseline(std::vector<render::Condition> rows);

    [[nodiscard]] std::vector<render::Condition>      const& current() const noexcept { return m_current; }
    [[nodiscard]] std::vector<ConditionChangeRecord>  const& changes() const noexcept { return m_changes; }
    [[nodiscard]] size_t pendingCount() const;

    // Returns the index of the new row in current(), or -1 on rejection.
    // Rejects on duplicate composite-PK collision against existing rows.
    int  addNew(render::Condition row);
    bool replaceRow(int index, render::Condition const& newRow);
    bool removeRow(int index);

    void revertAll();
    void acceptCommit(std::vector<render::Condition> committed);

    struct StateSnapshot
    {
        std::vector<render::Condition>     baseline;
        std::vector<render::Condition>     current;
        std::vector<ConditionChangeRecord> changes;
    };
    [[nodiscard]] StateSnapshot captureState() const;
    void restoreState(StateSnapshot const& s);

private:
    void recordUpdate(int index);
    [[nodiscard]] int findInBaseline(render::Condition const& row) const;
    [[nodiscard]] int findInCurrent (render::Condition const& row) const;

    std::vector<render::Condition>      m_baseline;
    std::vector<render::Condition>      m_current;
    std::vector<ConditionChangeRecord>  m_changes;
};

} // namespace world_editor::db
