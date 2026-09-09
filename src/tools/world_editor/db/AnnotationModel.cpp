#include "AnnotationModel.h"

#include <algorithm>

namespace world_editor::db
{

void AnnotationModel::setBaseline(std::vector<render::Annotation> rows)
{
    m_baseline = rows;
    m_current  = std::move(rows);
    m_changes.assign(m_current.size(), ChangeRecord{});
    m_nextLocalId = -1;
}

size_t AnnotationModel::pendingCount() const
{
    size_t n = 0;
    for (ChangeRecord const& c : m_changes)
        if (c.kind != ChangeKind::None)
            ++n;
    return n;
}

int AnnotationModel::addNew(render::Annotation row)
{
    row.id = m_nextLocalId--;
    m_current.push_back(row);
    ChangeRecord cr;
    cr.kind  = ChangeKind::Insert;
    cr.after = row;
    m_changes.push_back(cr);
    return static_cast<int>(m_current.size()) - 1;
}

void AnnotationModel::recordUpdate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return;

    ChangeRecord& cr = m_changes[index];

    if (cr.kind == ChangeKind::Insert)
    {
        // Editing a locally-inserted row - keep it as Insert; just
        // refresh the .after payload.
        cr.after = m_current[index];
        return;
    }

    if (cr.kind == ChangeKind::None)
    {
        // First edit of a DB-backed row - capture the baseline.
        // Find baseline by id (DB id is unique).
        for (render::Annotation const& base : m_baseline)
        {
            if (base.id == m_current[index].id)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind  = ChangeKind::Update;
    cr.after = m_current[index];

    // If the new value equals baseline, collapse back to None.
    if (cr.before.radius == cr.after.radius
        && cr.before.label == cr.after.label
        && cr.before.notes == cr.after.notes)
    {
        cr.kind = ChangeKind::None;
        cr.before = {};
        cr.after  = {};
    }
}

bool AnnotationModel::editRadius(int index, float newRadius)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;
    if (m_current[index].radius == newRadius)
        return false;
    m_current[index].radius = newRadius;
    recordUpdate(index);
    return true;
}

bool AnnotationModel::editLabel(int index, QString const& newLabel)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;
    if (m_current[index].label == newLabel)
        return false;
    m_current[index].label = newLabel;
    recordUpdate(index);
    return true;
}

bool AnnotationModel::editNotes(int index, QString const& newNotes)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;
    if (m_current[index].notes == newNotes)
        return false;
    m_current[index].notes = newNotes;
    recordUpdate(index);
    return true;
}

bool AnnotationModel::removeRow(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;

    ChangeRecord& cr = m_changes[index];
    if (cr.kind == ChangeKind::Insert)
    {
        // Removing a locally-added row - just drop it.
        m_current.erase(m_current.begin() + index);
        m_changes.erase(m_changes.begin() + index);
        return true;
    }

    // Mark for delete, capture before-image for backup.
    if (cr.kind == ChangeKind::None)
    {
        for (render::Annotation const& base : m_baseline)
        {
            if (base.id == m_current[index].id)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind = ChangeKind::Delete;
    cr.after = {};

    // Remove from m_current so the viewer hides it; keep the changelog
    // entry alive separately for the commit pass.  We sync this by
    // collapsing: instead of deleting m_changes[i], we swap the row to
    // a "pending delete" sidecar.  Simpler: keep both in lockstep and
    // hide deleted rows via NavMeshView post-filter.  We'll filter in
    // MainWindow::pushAnnotationsToViewer() when feeding the viewer.
    return true;
}

void AnnotationModel::revertAll()
{
    m_current = m_baseline;
    m_changes.assign(m_current.size(), ChangeRecord{});
    m_nextLocalId = -1;
}

void AnnotationModel::acceptCommit(std::vector<render::Annotation> committed)
{
    m_baseline = committed;
    m_current  = std::move(committed);
    m_changes.assign(m_current.size(), ChangeRecord{});
    m_nextLocalId = -1;
}

AnnotationModel::StateSnapshot AnnotationModel::captureState() const
{
    StateSnapshot s;
    s.baseline    = m_baseline;
    s.current     = m_current;
    s.changes     = m_changes;
    s.nextLocalId = m_nextLocalId;
    return s;
}

void AnnotationModel::restoreState(StateSnapshot const& s)
{
    m_baseline    = s.baseline;
    m_current     = s.current;
    m_changes     = s.changes;
    m_nextLocalId = s.nextLocalId;
}

} // namespace world_editor::db
