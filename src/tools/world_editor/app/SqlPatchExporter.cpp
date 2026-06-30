#include "SqlPatchExporter.h"

#include "../db/AnnotationModel.h"
#include "../db/AreatriggerModel.h"
#include "../db/ConditionsModel.h"
#include "../db/GraveyardModel.h"
#include "../db/MySqlClient.h"
#include "../db/SmartScriptModel.h"
#include "../db/SpawnModel.h"
#include "../db/WaypointModel.h"

#include <QDateTime>
#include <QFile>
#include <QStringConverter>
#include <QTextStream>

namespace world_editor::app
{

namespace
{

// ---- shared helpers --------------------------------------------------------

QString esc(db::MySqlClient* c, QString const& v)
{
    if (!c) return v;
    return QString::fromStdString(c->escapeString(v.toStdString()));
}

QString nullableLit(db::MySqlClient* c, QString const& v, bool isNull)
{
    if (isNull) return QStringLiteral("NULL");
    return QStringLiteral("'%1'").arg(esc(c, v));
}

QString transportClause(uint64_t v)
{
    return v == 0 ? QStringLiteral("NULL") : QString::number(qulonglong(v));
}

// ---- Annotations (mirrors CommitDialog) -----------------------------------

// playerbot_v2_world_metadata lives in the operator-configured shared playerbot
// schema (MySqlClient::qualify) — never a hardcoded DB name.
QString metaTable(db::MySqlClient* c)
{
    return QString::fromStdString(c->qualify("playerbot_v2_world_metadata"));
}

QString aInsert(db::MySqlClient* c, render::Annotation const& a)
{
    return QString(
        "INSERT INTO %1 (map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, label, notes, created_by) "
        "VALUES (%2, %3, %4, %5, %6, %7, %8, '%9', '%10', '%11');")
        .arg(metaTable(c))
        .arg(a.mapId).arg(a.zoneId).arg(uint8_t(a.kind))
        .arg(a.x, 0, 'f', 4).arg(a.y, 0, 'f', 4).arg(a.z, 0, 'f', 4)
        .arg(a.radius, 0, 'f', 4)
        .arg(esc(c, a.label)).arg(esc(c, a.notes)).arg(esc(c, a.createdBy));
}

QString aUpdate(db::MySqlClient* c, render::Annotation const& a)
{
    return QString("UPDATE %1 SET radius = %2, label = '%3', notes = '%4' WHERE id = %5;")
        .arg(metaTable(c))
        .arg(a.radius, 0, 'f', 4)
        .arg(esc(c, a.label)).arg(esc(c, a.notes))
        .arg(a.id);
}

QString aDelete(db::MySqlClient* c, render::Annotation const& a)
{
    return QString("DELETE FROM %1 WHERE id = %2;")
        .arg(metaTable(c)).arg(a.id);
}

// ---- Spawns (mirrors SpawnCommitDialog) -----------------------------------

QString crUpdate(db::MySqlClient* c, render::Spawn const& a)
{
    return QString(
        "UPDATE creature SET "
        "zoneId=%1, areaId=%2, spawnDifficulties='%3', "
        "phaseUseFlags=%4, PhaseId=%5, PhaseGroup=%6, terrainSwapMap=%7, "
        "modelid=%8, equipment_id=%9, "
        "position_x=%10, position_y=%11, position_z=%12, orientation=%13, "
        "spawntimesecs=%14, wander_distance=%15, currentwaypoint=%16, "
        "curHealthPct=%17, MovementType=%18, "
        "npcflag=%19, unit_flags=%20, unit_flags2=%21, unit_flags3=%22, "
        "ScriptName='%23', StringId='%24' WHERE guid=%25;")
        .arg(a.zoneId).arg(a.areaId).arg(esc(c, a.spawnDifficulties))
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup).arg(a.terrainSwapMap)
        .arg(a.modelid).arg(uint8_t(a.equipmentId))
        .arg(a.worldX, 0, 'f', 4).arg(a.worldY, 0, 'f', 4)
        .arg(a.worldZ, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(a.spawntimesecs).arg(a.wanderDistance, 0, 'f', 4).arg(a.currentwaypoint)
        .arg(a.curHealthPct).arg(uint8_t(a.movementType))
        .arg(a.npcflag).arg(a.unitFlags1).arg(a.unitFlags2).arg(a.unitFlags3)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.stringId)).arg(a.guid);
}

QString goUpdate(db::MySqlClient* c, render::Spawn const& a)
{
    return QString(
        "UPDATE gameobject SET "
        "zoneId=%1, areaId=%2, spawnDifficulties='%3', "
        "phaseUseFlags=%4, PhaseId=%5, PhaseGroup=%6, terrainSwapMap=%7, "
        "position_x=%8, position_y=%9, position_z=%10, orientation=%11, "
        "rotation0=%12, rotation1=%13, rotation2=%14, rotation3=%15, "
        "spawntimesecs=%16, animprogress=%17, state=%18, "
        "ScriptName='%19', StringId='%20' WHERE guid=%21;")
        .arg(a.zoneId).arg(a.areaId).arg(esc(c, a.spawnDifficulties))
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup).arg(a.terrainSwapMap)
        .arg(a.worldX, 0, 'f', 4).arg(a.worldY, 0, 'f', 4)
        .arg(a.worldZ, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(a.rotation0, 0, 'f', 6).arg(a.rotation1, 0, 'f', 6)
        .arg(a.rotation2, 0, 'f', 6).arg(a.rotation3, 0, 'f', 6)
        .arg(a.spawntimesecs).arg(uint8_t(a.animprogress)).arg(uint8_t(a.goState))
        .arg(esc(c, a.scriptName)).arg(esc(c, a.stringId)).arg(a.guid);
}

QString crDelete(render::Spawn const& a) { return QString("DELETE FROM creature WHERE guid=%1;").arg(a.guid); }
QString goDelete(render::Spawn const& a) { return QString("DELETE FROM gameobject WHERE guid=%1;").arg(a.guid); }

QString crInsert(db::MySqlClient* c, render::Spawn const& a)
{
    return QString(
        "INSERT INTO creature "
        "(guid, id, map, zoneId, areaId, spawnDifficulties, phaseUseFlags, PhaseId, "
        " PhaseGroup, terrainSwapMap, modelid, equipment_id, "
        " position_x, position_y, position_z, orientation, spawntimesecs, "
        " wander_distance, currentwaypoint, curHealthPct, MovementType, "
        " npcflag, unit_flags, unit_flags2, unit_flags3, ScriptName, StringId) "
        "VALUES (%1, %2, %3, %4, %5, '%6', %7, %8, %9, %10, %11, %12, "
        " %13, %14, %15, %16, %17, %18, %19, %20, %21, %22, %23, %24, %25, '%26', '%27');")
        .arg(a.guid).arg(a.entry).arg(a.mapId).arg(a.zoneId).arg(a.areaId)
        .arg(esc(c, a.spawnDifficulties))
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup).arg(a.terrainSwapMap)
        .arg(a.modelid).arg(uint8_t(a.equipmentId))
        .arg(a.worldX, 0, 'f', 4).arg(a.worldY, 0, 'f', 4)
        .arg(a.worldZ, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(a.spawntimesecs).arg(a.wanderDistance, 0, 'f', 4).arg(a.currentwaypoint)
        .arg(a.curHealthPct).arg(uint8_t(a.movementType))
        .arg(a.npcflag).arg(a.unitFlags1).arg(a.unitFlags2).arg(a.unitFlags3)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.stringId));
}

QString goInsert(db::MySqlClient* c, render::Spawn const& a)
{
    return QString(
        "INSERT INTO gameobject "
        "(guid, id, map, zoneId, areaId, spawnDifficulties, phaseUseFlags, PhaseId, "
        " PhaseGroup, terrainSwapMap, position_x, position_y, position_z, orientation, "
        " rotation0, rotation1, rotation2, rotation3, spawntimesecs, animprogress, "
        " state, ScriptName, StringId) "
        "VALUES (%1, %2, %3, %4, %5, '%6', %7, %8, %9, %10, %11, %12, %13, %14, "
        " %15, %16, %17, %18, %19, %20, %21, '%22', '%23');")
        .arg(a.guid).arg(a.entry).arg(a.mapId).arg(a.zoneId).arg(a.areaId)
        .arg(esc(c, a.spawnDifficulties))
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup).arg(a.terrainSwapMap)
        .arg(a.worldX, 0, 'f', 4).arg(a.worldY, 0, 'f', 4)
        .arg(a.worldZ, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(a.rotation0, 0, 'f', 6).arg(a.rotation1, 0, 'f', 6)
        .arg(a.rotation2, 0, 'f', 6).arg(a.rotation3, 0, 'f', 6)
        .arg(a.spawntimesecs).arg(uint8_t(a.animprogress)).arg(uint8_t(a.goState))
        .arg(esc(c, a.scriptName)).arg(esc(c, a.stringId));
}

// ---- Waypoint paths (mirrors WaypointCommitDialog) ------------------------

QString pInsert(db::MySqlClient* c, render::Path const& p)
{
    return QString(
        "INSERT INTO waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "VALUES (%1, %2, %3, %4, '%5');")
        .arg(p.pathId).arg(uint8_t(p.moveType)).arg(uint8_t(p.flags))
        .arg(p.velocity, 0, 'f', 4).arg(esc(c, p.comment));
}

QString pUpdate(db::MySqlClient* c, render::Path const& p)
{
    return QString(
        "UPDATE waypoint_path SET MoveType=%1, Flags=%2, Velocity=%3, Comment='%4' WHERE PathId=%5;")
        .arg(uint8_t(p.moveType)).arg(uint8_t(p.flags))
        .arg(p.velocity, 0, 'f', 4).arg(esc(c, p.comment)).arg(p.pathId);
}

QString nodeInsert(render::PathNode const& n, uint32_t pathId)
{
    return QString(
        "INSERT INTO waypoint_path_node "
        "(PathId, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay) "
        "VALUES (%1, %2, %3, %4, %5, %6, %7);")
        .arg(pathId).arg(n.nodeId)
        .arg(n.x, 0, 'f', 4).arg(n.y, 0, 'f', 4).arg(n.z, 0, 'f', 4)
        .arg(n.orientation, 0, 'f', 4).arg(n.delay);
}

QString deleteNodes(uint32_t pathId)
{
    return QString("DELETE FROM waypoint_path_node WHERE PathId=%1;").arg(pathId);
}

QString deletePath(uint32_t pathId)
{
    return QString("DELETE FROM waypoint_path WHERE PathId=%1;").arg(pathId);
}

// ---- Areatriggers (mirrors AreatriggerCommitDialog) -----------------------

QString atInsert(db::MySqlClient* c, render::Areatrigger const& a)
{
    return QString(
        "INSERT INTO areatrigger "
        "(SpawnId, AreaTriggerCreatePropertiesId, IsCustom, MapId, "
        " SpawnDifficulties, PosX, PosY, PosZ, Orientation, "
        " PhaseUseFlags, PhaseId, PhaseGroup, ScriptName, Comment, VerifiedBuild) "
        "VALUES (%1, %2, %3, %4, '%5', %6, %7, %8, %9, %10, %11, %12, '%13', '%14', %15);")
        .arg(a.spawnId).arg(a.createPropsId).arg(uint8_t(a.isCustom)).arg(a.mapId)
        .arg(esc(c, a.spawnDifficulties))
        .arg(a.x, 0, 'f', 4).arg(a.y, 0, 'f', 4)
        .arg(a.z, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.comment)).arg(a.verifiedBuild);
}

QString atUpdate(db::MySqlClient* c, render::Areatrigger const& a)
{
    return QString(
        "UPDATE areatrigger SET "
        "AreaTriggerCreatePropertiesId=%1, IsCustom=%2, SpawnDifficulties='%3', "
        "PosX=%4, PosY=%5, PosZ=%6, Orientation=%7, "
        "PhaseUseFlags=%8, PhaseId=%9, PhaseGroup=%10, "
        "ScriptName='%11', Comment='%12', VerifiedBuild=%13 WHERE SpawnId=%14;")
        .arg(a.createPropsId).arg(uint8_t(a.isCustom)).arg(esc(c, a.spawnDifficulties))
        .arg(a.x, 0, 'f', 4).arg(a.y, 0, 'f', 4)
        .arg(a.z, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.comment))
        .arg(a.verifiedBuild).arg(a.spawnId);
}

QString atDelete(render::Areatrigger const& a)
{
    return QString("DELETE FROM areatrigger WHERE SpawnId=%1;").arg(a.spawnId);
}

// ---- Graveyards (mirrors GraveyardCommitDialog) ---------------------------

QString gyInsert(db::MySqlClient* c, render::Graveyard const& g)
{
    return QString(
        "INSERT INTO world_safe_locs (ID, MapID, LocX, LocY, LocZ, Facing, TransportSpawnId, Comment) "
        "VALUES (%1, %2, %3, %4, %5, %6, %7, '%8');")
        .arg(g.id).arg(g.mapId)
        .arg(g.x, 0, 'f', 4).arg(g.y, 0, 'f', 4)
        .arg(g.z, 0, 'f', 4).arg(g.facing, 0, 'f', 4)
        .arg(transportClause(g.transportSpawnId)).arg(esc(c, g.comment));
}

QString gyUpdate(db::MySqlClient* c, render::Graveyard const& g)
{
    return QString(
        "UPDATE world_safe_locs SET MapID=%1, LocX=%2, LocY=%3, LocZ=%4, Facing=%5, "
        "TransportSpawnId=%6, Comment='%7' WHERE ID=%8;")
        .arg(g.mapId)
        .arg(g.x, 0, 'f', 4).arg(g.y, 0, 'f', 4)
        .arg(g.z, 0, 'f', 4).arg(g.facing, 0, 'f', 4)
        .arg(transportClause(g.transportSpawnId)).arg(esc(c, g.comment)).arg(g.id);
}

QString gyDelete(render::Graveyard const& g)
{
    return QString("DELETE FROM world_safe_locs WHERE ID=%1;").arg(g.id);
}

// ---- Smart scripts (mirrors SmartScriptCommitDialog) ----------------------

QString saiWhere(render::SmartScript const& a)
{
    return QString("entryorguid=%1 AND source_type=%2 AND id=%3 AND link=%4")
        .arg(qlonglong(a.entryorguid)).arg(int(a.sourceType))
        .arg(int(a.id)).arg(int(a.link));
}

QString saiInsert(db::MySqlClient* c, render::SmartScript const& a)
{
    return QString(
        "INSERT INTO smart_scripts "
        "(entryorguid, source_type, id, link, Difficulties, "
        " event_type, event_phase_mask, event_chance, event_flags, "
        " event_param1, event_param2, event_param3, event_param4, event_param5, "
        " event_param_string, "
        " action_type, action_param1, action_param2, action_param3, action_param4, "
        " action_param5, action_param6, action_param7, action_param_string, "
        " target_type, target_param1, target_param2, target_param3, target_param4, "
        " target_param_string, target_x, target_y, target_z, target_o, comment) "
        "VALUES (%1, %2, %3, %4, '%5', %6, %7, %8, %9, %10, %11, %12, %13, %14, '%15', "
        " %16, %17, %18, %19, %20, %21, %22, %23, %24, %25, %26, %27, %28, %29, %30, "
        " %31, %32, %33, %34, '%35');")
        .arg(qlonglong(a.entryorguid)).arg(int(a.sourceType))
        .arg(int(a.id)).arg(int(a.link))
        .arg(esc(c, a.difficulties))
        .arg(int(a.eventType)).arg(int(a.eventPhaseMask))
        .arg(int(a.eventChance)).arg(int(a.eventFlags))
        .arg(a.eventParam1).arg(a.eventParam2).arg(a.eventParam3)
        .arg(a.eventParam4).arg(a.eventParam5)
        .arg(esc(c, a.eventParamString))
        .arg(int(a.actionType))
        .arg(a.actionParam1).arg(a.actionParam2).arg(a.actionParam3).arg(a.actionParam4)
        .arg(a.actionParam5).arg(a.actionParam6).arg(a.actionParam7)
        .arg(nullableLit(c, a.actionParamString, a.actionParamStringIsNull))
        .arg(int(a.targetType))
        .arg(a.targetParam1).arg(a.targetParam2).arg(a.targetParam3).arg(a.targetParam4)
        .arg(nullableLit(c, a.targetParamString, a.targetParamStringIsNull))
        .arg(a.targetX, 0, 'f', 4).arg(a.targetY, 0, 'f', 4)
        .arg(a.targetZ, 0, 'f', 4).arg(a.targetO, 0, 'f', 4)
        .arg(esc(c, a.comment));
}

QString saiUpdate(db::MySqlClient* c, render::SmartScript const& a)
{
    return QString(
        "UPDATE smart_scripts SET "
        "Difficulties='%1', "
        "event_type=%2, event_phase_mask=%3, event_chance=%4, event_flags=%5, "
        "event_param1=%6, event_param2=%7, event_param3=%8, event_param4=%9, event_param5=%10, "
        "event_param_string='%11', "
        "action_type=%12, "
        "action_param1=%13, action_param2=%14, action_param3=%15, action_param4=%16, "
        "action_param5=%17, action_param6=%18, action_param7=%19, "
        "action_param_string=%20, "
        "target_type=%21, "
        "target_param1=%22, target_param2=%23, target_param3=%24, target_param4=%25, "
        "target_param_string=%26, "
        "target_x=%27, target_y=%28, target_z=%29, target_o=%30, "
        "comment='%31' WHERE %32;")
        .arg(esc(c, a.difficulties))
        .arg(int(a.eventType)).arg(int(a.eventPhaseMask))
        .arg(int(a.eventChance)).arg(int(a.eventFlags))
        .arg(a.eventParam1).arg(a.eventParam2).arg(a.eventParam3)
        .arg(a.eventParam4).arg(a.eventParam5)
        .arg(esc(c, a.eventParamString))
        .arg(int(a.actionType))
        .arg(a.actionParam1).arg(a.actionParam2).arg(a.actionParam3).arg(a.actionParam4)
        .arg(a.actionParam5).arg(a.actionParam6).arg(a.actionParam7)
        .arg(nullableLit(c, a.actionParamString, a.actionParamStringIsNull))
        .arg(int(a.targetType))
        .arg(a.targetParam1).arg(a.targetParam2).arg(a.targetParam3).arg(a.targetParam4)
        .arg(nullableLit(c, a.targetParamString, a.targetParamStringIsNull))
        .arg(a.targetX, 0, 'f', 4).arg(a.targetY, 0, 'f', 4)
        .arg(a.targetZ, 0, 'f', 4).arg(a.targetO, 0, 'f', 4)
        .arg(esc(c, a.comment))
        .arg(saiWhere(a));
}

QString saiDelete(render::SmartScript const& a)
{
    return QString("DELETE FROM smart_scripts WHERE %1;").arg(saiWhere(a));
}

// ---- Conditions (mirrors ConditionCommitDialog) ---------------------------

QString condWhere(db::MySqlClient* c, render::Condition const& a)
{
    return QString(
        "SourceTypeOrReferenceId=%1 AND SourceGroup=%2 AND SourceEntry=%3 AND SourceId=%4 "
        "AND ElseGroup=%5 AND ConditionTypeOrReference=%6 AND ConditionTarget=%7 "
        "AND ConditionValue1=%8 AND ConditionValue2=%9 AND ConditionValue3=%10 "
        "AND ConditionStringValue1='%11'")
        .arg(int(a.sourceTypeOrReferenceId)).arg(a.sourceGroup)
        .arg(int(a.sourceEntry)).arg(int(a.sourceId))
        .arg(a.elseGroup).arg(int(a.conditionTypeOrReference))
        .arg(int(a.conditionTarget))
        .arg(a.conditionValue1).arg(a.conditionValue2).arg(a.conditionValue3)
        .arg(esc(c, a.conditionStringValue1));
}

QString condInsert(db::MySqlClient* c, render::Condition const& a)
{
    return QString(
        "INSERT INTO conditions "
        "(SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, "
        " ElseGroup, ConditionTypeOrReference, ConditionTarget, "
        " ConditionValue1, ConditionValue2, ConditionValue3, ConditionStringValue1, "
        " NegativeCondition, ErrorType, ErrorTextId, ScriptName, Comment) "
        "VALUES (%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, '%11', %12, %13, %14, '%15', '%16');")
        .arg(int(a.sourceTypeOrReferenceId)).arg(a.sourceGroup)
        .arg(int(a.sourceEntry)).arg(int(a.sourceId))
        .arg(a.elseGroup).arg(int(a.conditionTypeOrReference))
        .arg(int(a.conditionTarget))
        .arg(a.conditionValue1).arg(a.conditionValue2).arg(a.conditionValue3)
        .arg(esc(c, a.conditionStringValue1))
        .arg(int(a.negativeCondition)).arg(a.errorType).arg(a.errorTextId)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.comment));
}

QString condUpdate(db::MySqlClient* c, render::Condition const& a)
{
    return QString(
        "UPDATE conditions SET NegativeCondition=%1, ErrorType=%2, ErrorTextId=%3, "
        "ScriptName='%4', Comment='%5' WHERE %6;")
        .arg(int(a.negativeCondition)).arg(a.errorType).arg(a.errorTextId)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.comment))
        .arg(condWhere(c, a));
}

QString condDelete(db::MySqlClient* c, render::Condition const& a)
{
    return QString("DELETE FROM conditions WHERE %1;").arg(condWhere(c, a));
}

// ---- per-model section emitters -------------------------------------------

// Emits the Annotations section.  Returns statement count written.
size_t emitAnnotations(QTextStream& ts, db::MySqlClient* c, db::AnnotationModel const& m)
{
    if (m.changes().empty()) return 0;
    size_t ins = 0, upd = 0, del = 0, total = 0;
    for (auto const& r : m.changes())
    {
        if      (r.kind == db::ChangeKind::Insert) ++ins;
        else if (r.kind == db::ChangeKind::Update) ++upd;
        else if (r.kind == db::ChangeKind::Delete) ++del;
    }
    if (ins + upd + del == 0) return 0;

    ts << "-- ===== Annotations (playerbot_v2_world_metadata) =====\n";
    ts << "-- " << ins << " INSERT, " << upd << " UPDATE, " << del << " DELETE\n";
    for (auto const& r : m.changes())
        if (r.kind == db::ChangeKind::Insert) { ts << aInsert(c, r.after) << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::ChangeKind::Update) { ts << aUpdate(c, r.after) << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::ChangeKind::Delete) { ts << aDelete(c, r.before)   << "\n"; ++total; }
    ts << "\n";
    return total;
}

size_t emitSpawns(QTextStream& ts, db::MySqlClient* c, db::SpawnModel const& m)
{
    if (m.changes().empty()) return 0;
    size_t ins = 0, upd = 0, del = 0, total = 0;
    for (auto const& r : m.changes())
    {
        if      (r.kind == db::SpawnChangeKind::Insert) ++ins;
        else if (r.kind == db::SpawnChangeKind::Update) ++upd;
        else if (r.kind == db::SpawnChangeKind::Delete) ++del;
    }
    if (ins + upd + del == 0) return 0;

    ts << "-- ===== Creatures / GOs (creature, gameobject) =====\n";
    ts << "-- " << ins << " INSERT, " << upd << " UPDATE, " << del << " DELETE\n";
    for (auto const& r : m.changes())
        if (r.kind == db::SpawnChangeKind::Insert)
        {
            ts << (r.after.kind == render::SpawnKind::Creature ? crInsert(c, r.after) : goInsert(c, r.after)) << "\n";
            ++total;
        }
    for (auto const& r : m.changes())
        if (r.kind == db::SpawnChangeKind::Update)
        {
            ts << (r.after.kind == render::SpawnKind::Creature ? crUpdate(c, r.after) : goUpdate(c, r.after)) << "\n";
            ++total;
        }
    for (auto const& r : m.changes())
        if (r.kind == db::SpawnChangeKind::Delete)
        {
            ts << (r.before.kind == render::SpawnKind::Creature ? crDelete(r.before) : goDelete(r.before)) << "\n";
            ++total;
        }
    ts << "\n";
    return total;
}

size_t emitWaypoints(QTextStream& ts, db::MySqlClient* c, db::WaypointModel const& m)
{
    if (m.changes().empty()) return 0;
    size_t total = 0;
    bool wroteHeader = false;
    auto header = [&] {
        if (wroteHeader) return;
        ts << "-- ===== Waypoint paths (waypoint_path, waypoint_path_node) =====\n";
        wroteHeader = true;
    };

    for (auto const& r : m.changes())
    {
        if (r.kind == db::PathChangeKind::Insert)
        {
            header();
            ts << "-- INSERT path " << r.after.pathId << " (" << r.after.nodes.size() << " nodes)\n";
            ts << pInsert(c, r.after) << "\n"; ++total;
            for (auto const& n : r.after.nodes) { ts << nodeInsert(n, r.after.pathId) << "\n"; ++total; }
        }
    }
    for (auto const& r : m.changes())
    {
        if (r.kind == db::PathChangeKind::Update)
        {
            header();
            ts << "-- UPDATE path " << r.after.pathId << " (rewrite " << r.after.nodes.size() << " nodes)\n";
            ts << pUpdate(c, r.after) << "\n"; ++total;
            ts << deleteNodes(r.after.pathId) << "\n"; ++total;
            for (auto const& n : r.after.nodes) { ts << nodeInsert(n, r.after.pathId) << "\n"; ++total; }
        }
    }
    for (auto const& r : m.changes())
    {
        if (r.kind == db::PathChangeKind::Delete)
        {
            header();
            ts << "-- DELETE path " << r.before.pathId << "\n";
            ts << deleteNodes(r.before.pathId) << "\n"; ++total;
            ts << deletePath(r.before.pathId)  << "\n"; ++total;
        }
    }
    if (wroteHeader) ts << "\n";
    return total;
}

size_t emitAreatriggers(QTextStream& ts, db::MySqlClient* c, db::AreatriggerModel const& m)
{
    if (m.changes().empty()) return 0;
    size_t ins = 0, upd = 0, del = 0, total = 0;
    for (auto const& r : m.changes())
    {
        if      (r.kind == db::AreatriggerChangeKind::Insert) ++ins;
        else if (r.kind == db::AreatriggerChangeKind::Update) ++upd;
        else if (r.kind == db::AreatriggerChangeKind::Delete) ++del;
    }
    if (ins + upd + del == 0) return 0;

    ts << "-- ===== Areatriggers (areatrigger) =====\n";
    ts << "-- " << ins << " INSERT, " << upd << " UPDATE, " << del << " DELETE\n";
    for (auto const& r : m.changes())
        if (r.kind == db::AreatriggerChangeKind::Insert) { ts << atInsert(c, r.after) << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::AreatriggerChangeKind::Update) { ts << atUpdate(c, r.after) << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::AreatriggerChangeKind::Delete) { ts << atDelete(r.before)   << "\n"; ++total; }
    ts << "\n";
    return total;
}

size_t emitGraveyards(QTextStream& ts, db::MySqlClient* c, db::GraveyardModel const& m)
{
    if (m.changes().empty()) return 0;
    size_t ins = 0, upd = 0, del = 0, total = 0;
    for (auto const& r : m.changes())
    {
        if      (r.kind == db::GraveyardChangeKind::Insert) ++ins;
        else if (r.kind == db::GraveyardChangeKind::Update) ++upd;
        else if (r.kind == db::GraveyardChangeKind::Delete) ++del;
    }
    if (ins + upd + del == 0) return 0;

    ts << "-- ===== Graveyards (world_safe_locs) =====\n";
    ts << "-- " << ins << " INSERT, " << upd << " UPDATE, " << del << " DELETE\n";
    for (auto const& r : m.changes())
        if (r.kind == db::GraveyardChangeKind::Insert) { ts << gyInsert(c, r.after) << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::GraveyardChangeKind::Update) { ts << gyUpdate(c, r.after) << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::GraveyardChangeKind::Delete) { ts << gyDelete(r.before)   << "\n"; ++total; }
    ts << "\n";
    return total;
}

size_t emitSmartScripts(QTextStream& ts, db::MySqlClient* c, db::SmartScriptModel const& m)
{
    if (m.changes().empty()) return 0;
    size_t ins = 0, upd = 0, del = 0, total = 0;
    for (auto const& r : m.changes())
    {
        if      (r.kind == db::SmartScriptChangeKind::Insert) ++ins;
        else if (r.kind == db::SmartScriptChangeKind::Update) ++upd;
        else if (r.kind == db::SmartScriptChangeKind::Delete) ++del;
    }
    if (ins + upd + del == 0) return 0;

    // Mirrors SmartScriptCommitDialog ordering: DELETE, UPDATE, INSERT --
    // lets the operator re-key a row via Delete+Add in a single patch.
    ts << "-- ===== Smart scripts (smart_scripts) =====\n";
    ts << "-- " << ins << " INSERT, " << upd << " UPDATE, " << del << " DELETE\n";
    for (auto const& r : m.changes())
        if (r.kind == db::SmartScriptChangeKind::Delete) { ts << saiDelete(r.before)   << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::SmartScriptChangeKind::Update) { ts << saiUpdate(c, r.after) << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::SmartScriptChangeKind::Insert) { ts << saiInsert(c, r.after) << "\n"; ++total; }
    ts << "\n";
    return total;
}

size_t emitConditions(QTextStream& ts, db::MySqlClient* c, db::ConditionsModel const& m)
{
    if (m.changes().empty()) return 0;
    size_t ins = 0, upd = 0, del = 0, total = 0;
    for (auto const& r : m.changes())
    {
        if      (r.kind == db::ConditionChangeKind::Insert) ++ins;
        else if (r.kind == db::ConditionChangeKind::Update) ++upd;
        else if (r.kind == db::ConditionChangeKind::Delete) ++del;
    }
    if (ins + upd + del == 0) return 0;

    // Mirrors ConditionCommitDialog ordering: DELETE, UPDATE, INSERT.
    ts << "-- ===== Conditions (conditions) =====\n";
    ts << "-- " << ins << " INSERT, " << upd << " UPDATE, " << del << " DELETE\n";
    for (auto const& r : m.changes())
        if (r.kind == db::ConditionChangeKind::Delete) { ts << condDelete(c, r.before) << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::ConditionChangeKind::Update) { ts << condUpdate(c, r.after)  << "\n"; ++total; }
    for (auto const& r : m.changes())
        if (r.kind == db::ConditionChangeKind::Insert) { ts << condInsert(c, r.after)  << "\n"; ++total; }
    ts << "\n";
    return total;
}

} // namespace

SqlPatchExporter::Result SqlPatchExporter::exportAll(
    QString const& filepath,
    db::MySqlClient* dbClient,
    db::AnnotationModel const* annot,
    db::SpawnModel const* spawn,
    db::WaypointModel const* waypoint,
    db::AreatriggerModel const* atr,
    db::GraveyardModel const* gy,
    db::SmartScriptModel const* sai,
    db::ConditionsModel const* cond)
{
    Result out;

    // Total pending count across all models -- drives the header summary
    // and short-circuits the "nothing to export" case.
    auto pending = [](auto const* m) -> size_t { return m ? m->pendingCount() : 0u; };
    size_t const totalPending =
        pending(annot) + pending(spawn) + pending(waypoint) +
        pending(atr)   + pending(gy)    + pending(sai)      + pending(cond);

    int modelsWithChanges = 0;
    if (annot    && annot->pendingCount()    > 0) ++modelsWithChanges;
    if (spawn    && spawn->pendingCount()    > 0) ++modelsWithChanges;
    if (waypoint && waypoint->pendingCount() > 0) ++modelsWithChanges;
    if (atr      && atr->pendingCount()      > 0) ++modelsWithChanges;
    if (gy       && gy->pendingCount()       > 0) ++modelsWithChanges;
    if (sai      && sai->pendingCount()      > 0) ++modelsWithChanges;
    if (cond     && cond->pendingCount()     > 0) ++modelsWithChanges;

    if (totalPending == 0)
    {
        out.errMsg = QStringLiteral("no pending changes across any model");
        return out;
    }

    QString body;
    {
        QTextStream ts(&body);
        ts.setEncoding(QStringConverter::Utf8);

        ts << "-- world_editor pending changes export\n";
        ts << "-- generated " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "Z\n";
        ts << "-- editor build " << QStringLiteral(__DATE__ " " __TIME__) << "\n";
        ts << "-- summary: " << totalPending << " changes across "
           << modelsWithChanges << " model(s)\n\n";
        ts << "START TRANSACTION;\n\n";

        if (annot)    out.statements += emitAnnotations  (ts, dbClient, *annot);
        if (spawn)    out.statements += emitSpawns       (ts, dbClient, *spawn);
        if (waypoint) out.statements += emitWaypoints    (ts, dbClient, *waypoint);
        if (atr)      out.statements += emitAreatriggers (ts, dbClient, *atr);
        if (gy)       out.statements += emitGraveyards   (ts, dbClient, *gy);
        if (sai)      out.statements += emitSmartScripts (ts, dbClient, *sai);
        if (cond)     out.statements += emitConditions   (ts, dbClient, *cond);

        ts << "COMMIT;\n";
    }

    QFile f(filepath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        out.errMsg = QStringLiteral("open %1 failed: %2").arg(filepath, f.errorString());
        out.statements = 0;
        return out;
    }
    QByteArray const bytes = body.toUtf8();
    qint64 const written = f.write(bytes);
    f.close();
    if (written != bytes.size())
    {
        out.errMsg = QStringLiteral("short write %1: wrote %2 of %3 bytes")
                       .arg(filepath).arg(qlonglong(written)).arg(qlonglong(bytes.size()));
        out.statements = 0;
        return out;
    }

    return out;
}

} // namespace world_editor::app
