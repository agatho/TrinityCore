/*
 * AnnotationModel - in-memory edit log for playerbot_v2_world_metadata.
 *
 * The editor keeps a "baseline" snapshot of the rows fetched from DB,
 * a "current" list (baseline + local mutations), and a derived changelog
 * (insert / update / delete) that the commit dialog turns into SQL.
 *
 * Identity model:
 *   - DB-backed rows have id > 0.
 *   - Locally-created rows have id = -(N+1) where N is the count of
 *     locally created rows at the time of creation. The commit dialog
 *     replaces these with real auto_increment ids returned by INSERT.
 *
 * Mutation policy:
 *   - position (x/y/z) and kind are immutable per the upstream design
 *     comment in WorldMetadata.h: "Other fields (kind, position) are
 *     intentionally not mutable - for those, delete + readd." We honour
 *     the same rule and only allow editing radius / label / notes.
 *
 * Threading: not thread-safe. Lives on the UI thread.
 */

#pragma once

#include "../core/model/WorldEntities.h"

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace world_editor::db
{

enum class ChangeKind : uint8_t
{
    None   = 0,
    Insert = 1,
    Update = 2,
    Delete = 3,
};

// One per row, indexed in lock-step with AnnotationModel::current().
struct ChangeRecord
{
    ChangeKind             kind = ChangeKind::None;
    render::Annotation     before;     // valid for Update + Delete
    render::Annotation     after;      // valid for Insert + Update
};

class AnnotationModel
{
public:
    // Replace the baseline AND current state with a fresh load. Clears
    // any pending changes.
    void setBaseline(std::vector<render::Annotation> rows);

    [[nodiscard]] std::vector<render::Annotation> const& current() const noexcept { return m_current; }
    [[nodiscard]] std::vector<ChangeRecord>       const& changes() const noexcept { return m_changes; }

    [[nodiscard]] size_t pendingCount() const;

    // Mutators - each returns the index of the affected row in current().
    int addNew(render::Annotation row);                              // appends; assigns negative id.
    bool editRadius(int index, float newRadius);                     // returns true if changed.
    bool editLabel (int index, QString const& newLabel);
    bool editNotes (int index, QString const& newNotes);
    bool removeRow (int index);                                      // marks delete or undoes insert.

    // Clear all pending edits, snap back to baseline.
    void revertAll();

    // Drop the changelog AFTER a successful commit; the new baseline
    // becomes the current state. Call this once SQL succeeds.
    void acceptCommit(std::vector<render::Annotation> committed);

    // Undo/redo support.  Captures the full mutable state of the model;
    // restoreState() rebuilds the model from a captured snapshot.  No
    // delta diff -- the snapshot is the entire (baseline+current+changes)
    // tuple plus the local-id counter.
    struct StateSnapshot
    {
        std::vector<render::Annotation> baseline;
        std::vector<render::Annotation> current;
        std::vector<ChangeRecord>       changes;
        int                             nextLocalId = -1;
    };
    [[nodiscard]] StateSnapshot captureState() const;
    void restoreState(StateSnapshot const& s);

private:
    void recordUpdate(int index);

    std::vector<render::Annotation> m_baseline; // last known DB state
    std::vector<render::Annotation> m_current;  // baseline + local edits
    std::vector<ChangeRecord>       m_changes;  // size == m_current.size()
    int                             m_nextLocalId = -1; // counts down as new rows are added.
};

} // namespace world_editor::db
