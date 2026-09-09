#include "AreatriggerModel.h"

namespace
{
bool sameAtr(world_editor::render::Areatrigger const& a,
             world_editor::render::Areatrigger const& b)
{
    // Compare only the per-spawn columns (the shape data is read-only).
    return a.spawnId            == b.spawnId
        && a.createPropsId      == b.createPropsId
        && a.isCustom           == b.isCustom
        && a.mapId              == b.mapId
        && a.spawnDifficulties  == b.spawnDifficulties
        && a.x                  == b.x
        && a.y                  == b.y
        && a.z                  == b.z
        && a.orientation        == b.orientation
        && a.phaseUseFlags      == b.phaseUseFlags
        && a.phaseId            == b.phaseId
        && a.phaseGroup         == b.phaseGroup
        && a.scriptName         == b.scriptName
        && a.comment            == b.comment
        && a.verifiedBuild      == b.verifiedBuild;
}
} // namespace

namespace world_editor::db
{

void AreatriggerModel::setBaseline(std::vector<render::Areatrigger> rows)
{
    m_baseline = rows;
    m_current  = std::move(rows);
    m_changes.assign(m_current.size(), AreatriggerChangeRecord{});
}

size_t AreatriggerModel::pendingCount() const
{
    size_t n = 0;
    for (AreatriggerChangeRecord const& c : m_changes)
        if (c.kind != AreatriggerChangeKind::None)
            ++n;
    return n;
}

int AreatriggerModel::addNew(render::Areatrigger row)
{
    if (row.spawnId <= 0)
        return -1; // caller must pre-assign a reserved SpawnId
    m_current.push_back(row);
    AreatriggerChangeRecord cr;
    cr.kind  = AreatriggerChangeKind::Insert;
    cr.after = row;
    m_changes.push_back(cr);
    return static_cast<int>(m_current.size()) - 1;
}

void AreatriggerModel::recordUpdate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return;

    AreatriggerChangeRecord& cr = m_changes[index];

    if (cr.kind == AreatriggerChangeKind::Insert)
    {
        cr.after = m_current[index];
        return;
    }
    if (cr.kind == AreatriggerChangeKind::None)
    {
        for (render::Areatrigger const& base : m_baseline)
        {
            if (base.spawnId == m_current[index].spawnId)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind  = AreatriggerChangeKind::Update;
    cr.after = m_current[index];

    if (sameAtr(cr.before, cr.after))
    {
        cr.kind   = AreatriggerChangeKind::None;
        cr.before = {};
        cr.after  = {};
    }
}

bool AreatriggerModel::replaceRow(int index, render::Areatrigger const& newRow)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;
    if (sameAtr(m_current[index], newRow))
        return false;
    m_current[index] = newRow;
    recordUpdate(index);
    return true;
}

bool AreatriggerModel::removeRow(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;

    AreatriggerChangeRecord& cr = m_changes[index];
    if (cr.kind == AreatriggerChangeKind::Insert)
    {
        // Locally-added row; drop it entirely.
        m_current.erase(m_current.begin() + index);
        m_changes.erase(m_changes.begin() + index);
        return true;
    }
    if (cr.kind == AreatriggerChangeKind::None)
    {
        for (render::Areatrigger const& base : m_baseline)
        {
            if (base.spawnId == m_current[index].spawnId)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind  = AreatriggerChangeKind::Delete;
    cr.after = {};
    return true;
}

void AreatriggerModel::revertAll()
{
    m_current = m_baseline;
    m_changes.assign(m_current.size(), AreatriggerChangeRecord{});
}

void AreatriggerModel::acceptCommit(std::vector<render::Areatrigger> committed)
{
    m_baseline = committed;
    m_current  = std::move(committed);
    m_changes.assign(m_current.size(), AreatriggerChangeRecord{});
}

AreatriggerModel::StateSnapshot AreatriggerModel::captureState() const
{
    StateSnapshot s;
    s.baseline = m_baseline;
    s.current  = m_current;
    s.changes  = m_changes;
    return s;
}

void AreatriggerModel::restoreState(StateSnapshot const& s)
{
    m_baseline = s.baseline;
    m_current  = s.current;
    m_changes  = s.changes;
}

} // namespace world_editor::db
