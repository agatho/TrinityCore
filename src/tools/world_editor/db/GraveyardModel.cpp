#include "GraveyardModel.h"

namespace
{
bool sameGy(world_editor::render::Graveyard const& a,
            world_editor::render::Graveyard const& b)
{
    return a.id               == b.id
        && a.mapId            == b.mapId
        && a.x                == b.x
        && a.y                == b.y
        && a.z                == b.z
        && a.facing           == b.facing
        && a.transportSpawnId == b.transportSpawnId
        && a.comment          == b.comment;
}
} // namespace

namespace world_editor::db
{

void GraveyardModel::setBaseline(std::vector<render::Graveyard> rows)
{
    m_baseline = rows;
    m_current  = std::move(rows);
    m_changes.assign(m_current.size(), GraveyardChangeRecord{});
}

size_t GraveyardModel::pendingCount() const
{
    size_t n = 0;
    for (GraveyardChangeRecord const& c : m_changes)
        if (c.kind != GraveyardChangeKind::None)
            ++n;
    return n;
}

int GraveyardModel::addNew(render::Graveyard row)
{
    if (row.id == 0)
        return -1;
    m_current.push_back(row);
    GraveyardChangeRecord cr;
    cr.kind  = GraveyardChangeKind::Insert;
    cr.after = row;
    m_changes.push_back(cr);
    return static_cast<int>(m_current.size()) - 1;
}

void GraveyardModel::recordUpdate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return;

    GraveyardChangeRecord& cr = m_changes[index];

    if (cr.kind == GraveyardChangeKind::Insert)
    {
        cr.after = m_current[index];
        return;
    }
    if (cr.kind == GraveyardChangeKind::None)
    {
        for (render::Graveyard const& base : m_baseline)
        {
            if (base.id == m_current[index].id)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind  = GraveyardChangeKind::Update;
    cr.after = m_current[index];

    if (sameGy(cr.before, cr.after))
    {
        cr.kind   = GraveyardChangeKind::None;
        cr.before = {};
        cr.after  = {};
    }
}

bool GraveyardModel::replaceRow(int index, render::Graveyard const& newRow)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;
    if (sameGy(m_current[index], newRow))
        return false;
    m_current[index] = newRow;
    recordUpdate(index);
    return true;
}

bool GraveyardModel::removeRow(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;

    GraveyardChangeRecord& cr = m_changes[index];
    if (cr.kind == GraveyardChangeKind::Insert)
    {
        m_current.erase(m_current.begin() + index);
        m_changes.erase(m_changes.begin() + index);
        return true;
    }
    if (cr.kind == GraveyardChangeKind::None)
    {
        for (render::Graveyard const& base : m_baseline)
        {
            if (base.id == m_current[index].id)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind  = GraveyardChangeKind::Delete;
    cr.after = {};
    return true;
}

void GraveyardModel::revertAll()
{
    m_current = m_baseline;
    m_changes.assign(m_current.size(), GraveyardChangeRecord{});
}

void GraveyardModel::acceptCommit(std::vector<render::Graveyard> committed)
{
    m_baseline = committed;
    m_current  = std::move(committed);
    m_changes.assign(m_current.size(), GraveyardChangeRecord{});
}

GraveyardModel::StateSnapshot GraveyardModel::captureState() const
{
    StateSnapshot s;
    s.baseline = m_baseline;
    s.current  = m_current;
    s.changes  = m_changes;
    return s;
}

void GraveyardModel::restoreState(StateSnapshot const& s)
{
    m_baseline = s.baseline;
    m_current  = s.current;
    m_changes  = s.changes;
}

} // namespace world_editor::db
