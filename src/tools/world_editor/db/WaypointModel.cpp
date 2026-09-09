#include "WaypointModel.h"

namespace
{
bool samePath(world_editor::render::Path const& a, world_editor::render::Path const& b)
{
    if (a.pathId != b.pathId) return false;
    if (a.moveType != b.moveType) return false;
    if (a.flags != b.flags) return false;
    if (a.velocity != b.velocity) return false;
    if (a.comment != b.comment) return false;
    if (a.nodes.size() != b.nodes.size()) return false;
    for (size_t i = 0; i < a.nodes.size(); ++i)
    {
        auto const& na = a.nodes[i];
        auto const& nb = b.nodes[i];
        if (na.nodeId != nb.nodeId) return false;
        if (na.x != nb.x || na.y != nb.y || na.z != nb.z) return false;
        if (na.orientation != nb.orientation) return false;
        if (na.delay != nb.delay) return false;
    }
    return true;
}
} // namespace

namespace world_editor::db
{

void WaypointModel::setBaseline(std::vector<render::Path> rows)
{
    m_baseline = rows;
    m_current  = std::move(rows);
    m_changes.assign(m_current.size(), PathChangeRecord{});
}

size_t WaypointModel::pendingCount() const
{
    size_t n = 0;
    for (PathChangeRecord const& c : m_changes)
        if (c.kind != PathChangeKind::None)
            ++n;
    return n;
}

int WaypointModel::indexForPathId(uint32_t pathId) const
{
    for (size_t i = 0; i < m_current.size(); ++i)
        if (m_current[i].pathId == pathId)
            return static_cast<int>(i);
    return -1;
}

int WaypointModel::addNewPath(render::Path row)
{
    m_current.push_back(row);
    PathChangeRecord cr;
    cr.kind  = PathChangeKind::Insert;
    cr.after = row;
    m_changes.push_back(cr);
    return static_cast<int>(m_current.size()) - 1;
}

void WaypointModel::recordUpdate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return;
    PathChangeRecord& cr = m_changes[index];

    if (cr.kind == PathChangeKind::Insert)
    {
        cr.after = m_current[index];
        return;
    }
    if (cr.kind == PathChangeKind::None)
    {
        // Locate baseline by PathId.
        for (render::Path const& base : m_baseline)
        {
            if (base.pathId == m_current[index].pathId)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind  = PathChangeKind::Update;
    cr.after = m_current[index];

    if (samePath(cr.before, cr.after))
    {
        cr.kind = PathChangeKind::None;
        cr.before = {};
        cr.after  = {};
    }
}

bool WaypointModel::replacePath(int index, render::Path const& newRow)
{
    if (index < 0 || index >= static_cast<int>(m_current.size())) return false;
    if (samePath(m_current[index], newRow)) return false;
    m_current[index] = newRow;
    recordUpdate(index);
    return true;
}

bool WaypointModel::removePath(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size())) return false;
    PathChangeRecord& cr = m_changes[index];
    if (cr.kind == PathChangeKind::Insert)
    {
        m_current.erase(m_current.begin() + index);
        m_changes.erase(m_changes.begin() + index);
        return true;
    }
    if (cr.kind == PathChangeKind::None)
    {
        for (render::Path const& base : m_baseline)
        {
            if (base.pathId == m_current[index].pathId)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind = PathChangeKind::Delete;
    cr.after = {};
    return true;
}

void WaypointModel::revertAll()
{
    m_current = m_baseline;
    m_changes.assign(m_current.size(), PathChangeRecord{});
}

void WaypointModel::acceptCommit(std::vector<render::Path> committed)
{
    m_baseline = committed;
    m_current  = std::move(committed);
    m_changes.assign(m_current.size(), PathChangeRecord{});
}

WaypointModel::StateSnapshot WaypointModel::captureState() const
{
    StateSnapshot s;
    s.baseline = m_baseline;
    s.current  = m_current;
    s.changes  = m_changes;
    return s;
}

void WaypointModel::restoreState(StateSnapshot const& s)
{
    m_baseline = s.baseline;
    m_current  = s.current;
    m_changes  = s.changes;
}

} // namespace world_editor::db
