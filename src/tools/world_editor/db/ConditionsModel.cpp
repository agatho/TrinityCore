#include "ConditionsModel.h"

namespace
{
bool sameKey(world_editor::render::Condition const& a,
             world_editor::render::Condition const& b)
{
    // Full 11-col composite PK.  ConditionStringValue1 is part of the
    // unique key so it MUST participate in identity checks; treating it
    // as side-data would let two rows with the same numeric PK columns
    // but different strings silently update each other.
    return a.sourceTypeOrReferenceId  == b.sourceTypeOrReferenceId
        && a.sourceGroup              == b.sourceGroup
        && a.sourceEntry              == b.sourceEntry
        && a.sourceId                 == b.sourceId
        && a.elseGroup                == b.elseGroup
        && a.conditionTypeOrReference == b.conditionTypeOrReference
        && a.conditionTarget          == b.conditionTarget
        && a.conditionValue1          == b.conditionValue1
        && a.conditionValue2          == b.conditionValue2
        && a.conditionValue3          == b.conditionValue3
        && a.conditionStringValue1    == b.conditionStringValue1;
}

bool sameRow(world_editor::render::Condition const& a,
             world_editor::render::Condition const& b)
{
    return sameKey(a, b)
        && a.negativeCondition == b.negativeCondition
        && a.errorType         == b.errorType
        && a.errorTextId       == b.errorTextId
        && a.scriptName        == b.scriptName
        && a.comment           == b.comment;
}
} // namespace

namespace world_editor::db
{

int ConditionsModel::findInBaseline(render::Condition const& row) const
{
    for (size_t i = 0; i < m_baseline.size(); ++i)
        if (sameKey(m_baseline[i], row))
            return static_cast<int>(i);
    return -1;
}

int ConditionsModel::findInCurrent(render::Condition const& row) const
{
    for (size_t i = 0; i < m_current.size(); ++i)
        if (sameKey(m_current[i], row))
            return static_cast<int>(i);
    return -1;
}

void ConditionsModel::setBaseline(std::vector<render::Condition> rows)
{
    m_baseline = rows;
    m_current  = std::move(rows);
    m_changes.assign(m_current.size(), ConditionChangeRecord{});
}

size_t ConditionsModel::pendingCount() const
{
    size_t n = 0;
    for (ConditionChangeRecord const& c : m_changes)
        if (c.kind != ConditionChangeKind::None)
            ++n;
    return n;
}

int ConditionsModel::addNew(render::Condition row)
{
    // Reject duplicate composite-PK collision against current rows.
    if (findInCurrent(row) >= 0)
        return -1;
    m_current.push_back(row);
    ConditionChangeRecord cr;
    cr.kind  = ConditionChangeKind::Insert;
    cr.after = row;
    m_changes.push_back(cr);
    return static_cast<int>(m_current.size()) - 1;
}

void ConditionsModel::recordUpdate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return;

    ConditionChangeRecord& cr = m_changes[index];

    if (cr.kind == ConditionChangeKind::Insert)
    {
        cr.after = m_current[index];
        return;
    }
    if (cr.kind == ConditionChangeKind::None)
    {
        int const baseIdx = findInBaseline(m_current[index]);
        if (baseIdx >= 0)
            cr.before = m_baseline[baseIdx];
    }
    cr.kind  = ConditionChangeKind::Update;
    cr.after = m_current[index];

    if (sameRow(cr.before, cr.after))
    {
        cr.kind   = ConditionChangeKind::None;
        cr.before = {};
        cr.after  = {};
    }
}

bool ConditionsModel::replaceRow(int index, render::Condition const& newRow)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;
    if (sameRow(m_current[index], newRow))
        return false;
    // If any PK column changed, the new key must not collide with
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

bool ConditionsModel::removeRow(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;

    ConditionChangeRecord& cr = m_changes[index];
    if (cr.kind == ConditionChangeKind::Insert)
    {
        // Locally-added row; drop it entirely.
        m_current.erase(m_current.begin() + index);
        m_changes.erase(m_changes.begin() + index);
        return true;
    }
    if (cr.kind == ConditionChangeKind::None)
    {
        int const baseIdx = findInBaseline(m_current[index]);
        if (baseIdx >= 0)
            cr.before = m_baseline[baseIdx];
    }
    cr.kind  = ConditionChangeKind::Delete;
    cr.after = {};
    return true;
}

void ConditionsModel::revertAll()
{
    m_current = m_baseline;
    m_changes.assign(m_current.size(), ConditionChangeRecord{});
}

void ConditionsModel::acceptCommit(std::vector<render::Condition> committed)
{
    m_baseline = committed;
    m_current  = std::move(committed);
    m_changes.assign(m_current.size(), ConditionChangeRecord{});
}

ConditionsModel::StateSnapshot ConditionsModel::captureState() const
{
    StateSnapshot s;
    s.baseline = m_baseline;
    s.current  = m_current;
    s.changes  = m_changes;
    return s;
}

void ConditionsModel::restoreState(StateSnapshot const& s)
{
    m_baseline = s.baseline;
    m_current  = s.current;
    m_changes  = s.changes;
}

} // namespace world_editor::db
