#include "SpawnModel.h"

#include <algorithm>

namespace
{
// Field-by-field equality.  We do an exact compare so the model
// collapses a "set field A then set field A back" sequence to None.
// Coordinate values use bit-equality (operator==) so a 0.0f vs -0.0f
// could in theory survive as Update; that's acceptable.
bool sameSpawn(world_editor::render::Spawn const& a,
               world_editor::render::Spawn const& b)
{
    return a.kind == b.kind
        && a.guid == b.guid
        && a.entry == b.entry
        && a.mapId == b.mapId
        && a.zoneId == b.zoneId
        && a.areaId == b.areaId
        && a.spawnDifficulties == b.spawnDifficulties
        && a.phaseUseFlags == b.phaseUseFlags
        && a.phaseId == b.phaseId
        && a.phaseGroup == b.phaseGroup
        && a.terrainSwapMap == b.terrainSwapMap
        && a.spawntimesecs == b.spawntimesecs
        && a.worldX == b.worldX
        && a.worldY == b.worldY
        && a.worldZ == b.worldZ
        && a.orientation == b.orientation
        && a.modelid == b.modelid
        && a.equipmentId == b.equipmentId
        && a.wanderDistance == b.wanderDistance
        && a.currentwaypoint == b.currentwaypoint
        && a.curHealthPct == b.curHealthPct
        && a.movementType == b.movementType
        && a.npcflag == b.npcflag
        && a.unitFlags1 == b.unitFlags1
        && a.unitFlags2 == b.unitFlags2
        && a.unitFlags3 == b.unitFlags3
        && a.rotation0 == b.rotation0
        && a.rotation1 == b.rotation1
        && a.rotation2 == b.rotation2
        && a.rotation3 == b.rotation3
        && a.animprogress == b.animprogress
        && a.goState == b.goState
        && a.scriptName == b.scriptName
        && a.stringId == b.stringId;
}
} // namespace

namespace world_editor::db
{

void SpawnModel::setBaseline(std::vector<render::Spawn> rows)
{
    m_baseline = rows;
    m_current  = std::move(rows);
    m_changes.assign(m_current.size(), SpawnChangeRecord{});
    m_nextLocalGuid = -1;
}

size_t SpawnModel::pendingCount() const
{
    size_t n = 0;
    for (SpawnChangeRecord const& c : m_changes)
        if (c.kind != SpawnChangeKind::None)
            ++n;
    return n;
}

int SpawnModel::addNew(render::Spawn row)
{
    // Caller passes the row with its real (reserved) guid already set;
    // we do NOT overwrite.  The Insert-vs-Update distinction lives in
    // the ChangeKind, not the guid sign.  Phase 3c reserves guids
    // up-front via MainWindow's GUID range so this is safe.
    if (row.guid <= 0)
        row.guid = m_nextLocalGuid--; // fallback for edge callers
    m_current.push_back(row);
    SpawnChangeRecord cr;
    cr.kind  = SpawnChangeKind::Insert;
    cr.after = row;
    m_changes.push_back(cr);
    return static_cast<int>(m_current.size()) - 1;
}

void SpawnModel::recordUpdate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return;

    SpawnChangeRecord& cr = m_changes[index];

    if (cr.kind == SpawnChangeKind::Insert)
    {
        cr.after = m_current[index];
        return;
    }
    if (cr.kind == SpawnChangeKind::None)
    {
        // Find baseline row by (kind, guid) - guid is unique within
        // a table.
        for (render::Spawn const& base : m_baseline)
        {
            if (base.kind == m_current[index].kind && base.guid == m_current[index].guid)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind  = SpawnChangeKind::Update;
    cr.after = m_current[index];

    if (sameSpawn(cr.before, cr.after))
    {
        cr.kind = SpawnChangeKind::None;
        cr.before = {};
        cr.after  = {};
    }
}

bool SpawnModel::replaceRow(int index, render::Spawn const& newRow)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;
    if (sameSpawn(m_current[index], newRow))
        return false;
    m_current[index] = newRow;
    recordUpdate(index);
    return true;
}

bool SpawnModel::removeRow(int index)
{
    if (index < 0 || index >= static_cast<int>(m_current.size()))
        return false;

    SpawnChangeRecord& cr = m_changes[index];
    if (cr.kind == SpawnChangeKind::Insert)
    {
        // Locally-added row; just drop it.
        m_current.erase(m_current.begin() + index);
        m_changes.erase(m_changes.begin() + index);
        return true;
    }
    if (cr.kind == SpawnChangeKind::None)
    {
        for (render::Spawn const& base : m_baseline)
        {
            if (base.kind == m_current[index].kind && base.guid == m_current[index].guid)
            {
                cr.before = base;
                break;
            }
        }
    }
    cr.kind = SpawnChangeKind::Delete;
    cr.after = {};
    return true;
}

void SpawnModel::revertAll()
{
    m_current = m_baseline;
    m_changes.assign(m_current.size(), SpawnChangeRecord{});
    m_nextLocalGuid = -1;
}

void SpawnModel::acceptCommit(std::vector<render::Spawn> committed)
{
    m_baseline = committed;
    m_current  = std::move(committed);
    m_changes.assign(m_current.size(), SpawnChangeRecord{});
    m_nextLocalGuid = -1;
}

SpawnModel::StateSnapshot SpawnModel::captureState() const
{
    StateSnapshot s;
    s.baseline      = m_baseline;
    s.current       = m_current;
    s.changes       = m_changes;
    s.nextLocalGuid = m_nextLocalGuid;
    return s;
}

void SpawnModel::restoreState(StateSnapshot const& s)
{
    m_baseline      = s.baseline;
    m_current       = s.current;
    m_changes       = s.changes;
    m_nextLocalGuid = s.nextLocalGuid;
}

} // namespace world_editor::db
