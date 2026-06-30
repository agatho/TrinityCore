#include "SmartScriptModel.h"

namespace
{
bool sameKey(world_editor::render::SmartScript const& a,
             world_editor::render::SmartScript const& b)
{
    return a.entryorguid == b.entryorguid
        && a.sourceType  == b.sourceType
        && a.id          == b.id
        && a.link        == b.link;
}

bool sameRow(world_editor::render::SmartScript const& a,
             world_editor::render::SmartScript const& b)
{
    // Treat nullable strings as equivalent when both sides are
    // effectively empty (NULL or "").  This keeps no-op edits on a
    // NULL-valued column from being flagged as a change.
    auto sameNullable = [](QString const& lhs, bool lhsNull,
                           QString const& rhs, bool rhsNull) -> bool
    {
        bool const lEmpty = lhsNull || lhs.isEmpty();
        bool const rEmpty = rhsNull || rhs.isEmpty();
        if (lEmpty && rEmpty) return true;
        if (lEmpty != rEmpty) return false;
        return lhs == rhs;
    };

    return sameKey(a, b)
        && a.difficulties     == b.difficulties
        && a.eventType        == b.eventType
        && a.eventPhaseMask   == b.eventPhaseMask
        && a.eventChance      == b.eventChance
        && a.eventFlags       == b.eventFlags
        && a.eventParam1      == b.eventParam1
        && a.eventParam2      == b.eventParam2
        && a.eventParam3      == b.eventParam3
        && a.eventParam4      == b.eventParam4
        && a.eventParam5      == b.eventParam5
        && a.eventParamString == b.eventParamString
        && a.actionType       == b.actionType
        && a.actionParam1     == b.actionParam1
        && a.actionParam2     == b.actionParam2
        && a.actionParam3     == b.actionParam3
        && a.actionParam4     == b.actionParam4
        && a.actionParam5     == b.actionParam5
        && a.actionParam6     == b.actionParam6
        && a.actionParam7     == b.actionParam7
        && sameNullable(a.actionParamString, a.actionParamStringIsNull,
                        b.actionParamString, b.actionParamStringIsNull)
        && a.targetType       == b.targetType
        && a.targetParam1     == b.targetParam1
        && a.targetParam2     == b.targetParam2
        && a.targetParam3     == b.targetParam3
        && a.targetParam4     == b.targetParam4
        && sameNullable(a.targetParamString, a.targetParamStringIsNull,
                        b.targetParamString, b.targetParamStringIsNull)
        && a.targetX          == b.targetX
        && a.targetY          == b.targetY
        && a.targetZ          == b.targetZ
        && a.targetO          == b.targetO
        && a.comment          == b.comment;
}
} // namespace

namespace world_editor::db
{

int SmartScriptModel::findInBaseline(render::SmartScript const& row) const
{
    for (size_t i = 0; i < m_baseline.size(); ++i)
        if (sameKey(m_baseline[i], row))
            return static_cast<int>(i);
    return -1;
}

int SmartScriptModel::findInCurrent(render::SmartScript const& row) const
{
    for (size_t i = 0; i < m_current.size(); ++i)
        if (sameKey(m_current[i], row))
            return static_cast<int>(i);
    return -1;
}

void SmartScriptModel::setBaseline(std::vector<render::SmartScript> rows)
{
    m_baseline = rows;
    m_current  = std::move(rows);
    m_changes.assign(m_current.size(), SmartScriptChangeRecord{});
}

size_t SmartScriptModel::pendingCount() const
{
    size_t n = 0;
    for (SmartScriptChangeRecord const& c : m_changes)
        if (c.kind != SmartScriptChangeKind::None)
            ++n;
    return n;
}

int SmartScriptModel::addNew(render::SmartScript row)
{
    // Reject duplicate composite-PK collision against current rows.
    if (findInCurrent(row) >= 0)
        return -1;
    m_current.push_back(row);
    SmartScriptChangeRecord cr;
    cr.kind  = SmartScriptChangeKind::Insert;
    cr.after = row;
    m_changes.push_back(cr);
    return static_cast<int>(m_current.size()) - 1;
}

void SmartScriptModel::recordUpdate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return;

    SmartScriptChangeRecord& cr = m_changes[index];

    if (cr.kind == SmartScriptChangeKind::Insert)
    {
        cr.after = m_current[index];
        return;
    }
    if (cr.kind == SmartScriptChangeKind::None)
    {
        int const baseIdx = findInBaseline(m_current[index]);
        if (baseIdx >= 0)
            cr.before = m_baseline[baseIdx];
    }
    cr.kind  = SmartScriptChangeKind::Update;
    cr.after = m_current[index];

    if (sameRow(cr.before, cr.after))
    {
        cr.kind   = SmartScriptChangeKind::None;
        cr.before = {};
        cr.after  = {};
    }
}

bool SmartScriptModel::replaceRow(int index, render::SmartScript const& newRow)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;
    if (sameRow(m_current[index], newRow))
        return false;
    // If the composite PK changed, the new key must not collide with
    // another existing row (other than this index).
    if (!sameKey(m_current[index], newRow))
    {
        for (size_t i = 0; i < m_current.size(); ++i)
        {
            if (static_cast<int>(i) == index) continue;
            if (sameKey(m_current[i], newRow))
                return false;
        }
    }
    m_current[index] = newRow;
    recordUpdate(index);
    return true;
}

bool SmartScriptModel::removeRow(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;

    SmartScriptChangeRecord& cr = m_changes[index];
    if (cr.kind == SmartScriptChangeKind::Insert)
    {
        // Locally-added row; drop it entirely.
        m_current.erase(m_current.begin() + index);
        m_changes.erase(m_changes.begin() + index);
        return true;
    }
    if (cr.kind == SmartScriptChangeKind::None)
    {
        int const baseIdx = findInBaseline(m_current[index]);
        if (baseIdx >= 0)
            cr.before = m_baseline[baseIdx];
    }
    cr.kind  = SmartScriptChangeKind::Delete;
    cr.after = {};
    return true;
}

void SmartScriptModel::revertAll()
{
    m_current = m_baseline;
    m_changes.assign(m_current.size(), SmartScriptChangeRecord{});
}

void SmartScriptModel::acceptCommit(std::vector<render::SmartScript> committed)
{
    m_baseline = committed;
    m_current  = std::move(committed);
    m_changes.assign(m_current.size(), SmartScriptChangeRecord{});
}

SmartScriptModel::StateSnapshot SmartScriptModel::captureState() const
{
    StateSnapshot s;
    s.baseline = m_baseline;
    s.current  = m_current;
    s.changes  = m_changes;
    return s;
}

void SmartScriptModel::restoreState(StateSnapshot const& s)
{
    m_baseline = s.baseline;
    m_current  = s.current;
    m_changes  = s.changes;
}

} // namespace world_editor::db
