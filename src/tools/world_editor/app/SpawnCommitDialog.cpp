#include "SpawnCommitDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdio>

namespace world_editor::app
{

namespace
{

QString esc(db::MySqlClient* c, QString const& v)
{
    if (!c) return v;
    return QString::fromStdString(c->escapeString(v.toStdString()));
}

QString formatCreatureUpdate(db::MySqlClient* c, render::Spawn const& a)
{
    return QString(
        "UPDATE creature SET "
        "zoneId=%1, areaId=%2, "
        "spawnDifficulties='%3', "
        "phaseUseFlags=%4, PhaseId=%5, PhaseGroup=%6, terrainSwapMap=%7, "
        "modelid=%8, equipment_id=%9, "
        "position_x=%10, position_y=%11, position_z=%12, orientation=%13, "
        "spawntimesecs=%14, wander_distance=%15, currentwaypoint=%16, "
        "curHealthPct=%17, MovementType=%18, "
        "npcflag=%19, unit_flags=%20, unit_flags2=%21, unit_flags3=%22, "
        "ScriptName='%23', StringId='%24' "
        "WHERE guid=%25;")
        .arg(a.zoneId).arg(a.areaId)
        .arg(esc(c, a.spawnDifficulties))
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup).arg(a.terrainSwapMap)
        .arg(a.modelid).arg(uint8_t(a.equipmentId))
        .arg(a.worldX, 0, 'f', 4).arg(a.worldY, 0, 'f', 4)
        .arg(a.worldZ, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(a.spawntimesecs).arg(a.wanderDistance, 0, 'f', 4).arg(a.currentwaypoint)
        .arg(a.curHealthPct).arg(uint8_t(a.movementType))
        .arg(a.npcflag).arg(a.unitFlags1).arg(a.unitFlags2).arg(a.unitFlags3)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.stringId))
        .arg(a.guid);
}

QString formatGameObjectUpdate(db::MySqlClient* c, render::Spawn const& a)
{
    return QString(
        "UPDATE gameobject SET "
        "zoneId=%1, areaId=%2, "
        "spawnDifficulties='%3', "
        "phaseUseFlags=%4, PhaseId=%5, PhaseGroup=%6, terrainSwapMap=%7, "
        "position_x=%8, position_y=%9, position_z=%10, orientation=%11, "
        "rotation0=%12, rotation1=%13, rotation2=%14, rotation3=%15, "
        "spawntimesecs=%16, animprogress=%17, state=%18, "
        "ScriptName='%19', StringId='%20' "
        "WHERE guid=%21;")
        .arg(a.zoneId).arg(a.areaId)
        .arg(esc(c, a.spawnDifficulties))
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup).arg(a.terrainSwapMap)
        .arg(a.worldX, 0, 'f', 4).arg(a.worldY, 0, 'f', 4)
        .arg(a.worldZ, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(a.rotation0, 0, 'f', 6).arg(a.rotation1, 0, 'f', 6)
        .arg(a.rotation2, 0, 'f', 6).arg(a.rotation3, 0, 'f', 6)
        .arg(a.spawntimesecs).arg(uint8_t(a.animprogress)).arg(uint8_t(a.goState))
        .arg(esc(c, a.scriptName)).arg(esc(c, a.stringId))
        .arg(a.guid);
}

QString formatCreatureDelete(render::Spawn const& a)
{
    return QString("DELETE FROM creature WHERE guid=%1;").arg(a.guid);
}

QString formatGameObjectDelete(render::Spawn const& a)
{
    return QString("DELETE FROM gameobject WHERE guid=%1;").arg(a.guid);
}

QString formatCreatureInsert(db::MySqlClient* c, render::Spawn const& a)
{
    // Explicit guid: we set it from the pre-reserved range so the
    // post-commit refresh finds the row at the same guid the operator
    // already saw in the editor (no auto_increment surprise).
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

QString formatGameObjectInsert(db::MySqlClient* c, render::Spawn const& a)
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

QString formatCreatureBackup(db::MySqlClient* c, render::Spawn const& a)
{
    // Full-row INSERT...ON DUPLICATE KEY UPDATE for re-applyability.
    return QString(
        "INSERT INTO creature "
        "(guid, id, map, zoneId, areaId, spawnDifficulties, phaseUseFlags, PhaseId, "
        " PhaseGroup, terrainSwapMap, modelid, equipment_id, "
        " position_x, position_y, position_z, orientation, spawntimesecs, "
        " wander_distance, currentwaypoint, curHealthPct, MovementType, "
        " npcflag, unit_flags, unit_flags2, unit_flags3, ScriptName, StringId) "
        "VALUES (%1, %2, %3, %4, %5, '%6', %7, %8, %9, %10, %11, %12, "
        " %13, %14, %15, %16, %17, %18, %19, %20, %21, %22, %23, %24, %25, '%26', '%27') "
        "ON DUPLICATE KEY UPDATE "
        "id=VALUES(id), map=VALUES(map), zoneId=VALUES(zoneId), areaId=VALUES(areaId), "
        "spawnDifficulties=VALUES(spawnDifficulties), "
        "phaseUseFlags=VALUES(phaseUseFlags), PhaseId=VALUES(PhaseId), "
        "PhaseGroup=VALUES(PhaseGroup), terrainSwapMap=VALUES(terrainSwapMap), "
        "modelid=VALUES(modelid), equipment_id=VALUES(equipment_id), "
        "position_x=VALUES(position_x), position_y=VALUES(position_y), "
        "position_z=VALUES(position_z), orientation=VALUES(orientation), "
        "spawntimesecs=VALUES(spawntimesecs), wander_distance=VALUES(wander_distance), "
        "currentwaypoint=VALUES(currentwaypoint), curHealthPct=VALUES(curHealthPct), "
        "MovementType=VALUES(MovementType), npcflag=VALUES(npcflag), "
        "unit_flags=VALUES(unit_flags), unit_flags2=VALUES(unit_flags2), "
        "unit_flags3=VALUES(unit_flags3), ScriptName=VALUES(ScriptName), "
        "StringId=VALUES(StringId);")
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

QString formatGameObjectBackup(db::MySqlClient* c, render::Spawn const& a)
{
    return QString(
        "INSERT INTO gameobject "
        "(guid, id, map, zoneId, areaId, spawnDifficulties, phaseUseFlags, PhaseId, "
        " PhaseGroup, terrainSwapMap, position_x, position_y, position_z, orientation, "
        " rotation0, rotation1, rotation2, rotation3, spawntimesecs, animprogress, "
        " state, ScriptName, StringId) "
        "VALUES (%1, %2, %3, %4, %5, '%6', %7, %8, %9, %10, %11, %12, %13, %14, "
        " %15, %16, %17, %18, %19, %20, %21, '%22', '%23') "
        "ON DUPLICATE KEY UPDATE "
        "id=VALUES(id), map=VALUES(map), zoneId=VALUES(zoneId), areaId=VALUES(areaId), "
        "spawnDifficulties=VALUES(spawnDifficulties), "
        "phaseUseFlags=VALUES(phaseUseFlags), PhaseId=VALUES(PhaseId), "
        "PhaseGroup=VALUES(PhaseGroup), terrainSwapMap=VALUES(terrainSwapMap), "
        "position_x=VALUES(position_x), position_y=VALUES(position_y), "
        "position_z=VALUES(position_z), orientation=VALUES(orientation), "
        "rotation0=VALUES(rotation0), rotation1=VALUES(rotation1), "
        "rotation2=VALUES(rotation2), rotation3=VALUES(rotation3), "
        "spawntimesecs=VALUES(spawntimesecs), animprogress=VALUES(animprogress), "
        "state=VALUES(state), ScriptName=VALUES(ScriptName), StringId=VALUES(StringId);")
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

} // namespace

SpawnCommitDialog::SpawnCommitDialog(db::MySqlClient* dbClient,
                                     db::SpawnModel const& model,
                                     uint32_t mapId,
                                     QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_model(model), m_mapId(mapId)
{
    setWindowTitle(tr("Commit spawn changes"));
    setModal(true);
    resize(820, 600);

    m_sqlPreview = new QPlainTextEdit(this);
    m_sqlPreview->setReadOnly(true);
    m_sqlPreview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_sqlPreview->setPlainText(buildSqlPreview());

    m_backupCheckbox = new QCheckBox(
        tr("Write before-images to editor_backups/ before applying"), this);
    m_backupCheckbox->setChecked(true);

    m_statusLabel = new QLabel(QString{}, this);
    m_statusLabel->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    m_commitButton = buttons->addButton(tr("Commit"), QDialogButtonBox::AcceptRole);
    m_commitButton->setDefault(true);
    buttons->addButton(QDialogButtonBox::Cancel);

    connect(m_commitButton, &QPushButton::clicked, this, &SpawnCommitDialog::onCommitClicked);
    connect(buttons,        &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel(tr(
        "The following SQL will run inside a single transaction. Review before committing.")));
    outer->addWidget(m_sqlPreview, 1);
    outer->addWidget(m_backupCheckbox);
    outer->addWidget(m_statusLabel);
    outer->addWidget(buttons);
}

QString SpawnCommitDialog::buildSqlPreview() const
{
    QString out;
    QTextStream ts(&out);
    ts << "BEGIN;\n\n";

    size_t crIns = 0, goIns = 0;
    size_t crUpd = 0, crDel = 0, goUpd = 0, goDel = 0;
    for (db::SpawnChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SpawnChangeKind::Insert)
        {
            if (c.after.kind == render::SpawnKind::Creature) ++crIns; else ++goIns;
        }
        else if (c.kind == db::SpawnChangeKind::Update)
        {
            if (c.after.kind == render::SpawnKind::Creature) ++crUpd; else ++goUpd;
        }
        else if (c.kind == db::SpawnChangeKind::Delete)
        {
            if (c.before.kind == render::SpawnKind::Creature) ++crDel; else ++goDel;
        }
    }

    if (crIns + goIns > 0)
    {
        ts << "-- " << (crIns + goIns) << " INSERT(s)\n";
        for (db::SpawnChangeRecord const& c : m_model.changes())
        {
            if (c.kind == db::SpawnChangeKind::Insert)
                ts << (c.after.kind == render::SpawnKind::Creature
                      ? formatCreatureInsert(m_dbClient, c.after)
                      : formatGameObjectInsert(m_dbClient, c.after)) << "\n";
        }
        ts << "\n";
    }
    if (crUpd + goUpd > 0)
    {
        ts << "-- " << (crUpd + goUpd) << " UPDATE(s)\n";
        for (db::SpawnChangeRecord const& c : m_model.changes())
        {
            if (c.kind == db::SpawnChangeKind::Update)
                ts << (c.after.kind == render::SpawnKind::Creature
                      ? formatCreatureUpdate(m_dbClient, c.after)
                      : formatGameObjectUpdate(m_dbClient, c.after)) << "\n";
        }
        ts << "\n";
    }
    if (crDel + goDel > 0)
    {
        ts << "-- " << (crDel + goDel) << " DELETE(s)\n";
        for (db::SpawnChangeRecord const& c : m_model.changes())
        {
            if (c.kind == db::SpawnChangeKind::Delete)
                ts << (c.before.kind == render::SpawnKind::Creature
                      ? formatCreatureDelete(c.before)
                      : formatGameObjectDelete(c.before)) << "\n";
        }
        ts << "\n";
    }

    ts << "COMMIT;\n";
    return out;
}

QString SpawnCommitDialog::buildBackupSql() const
{
    QString out;
    QTextStream ts(&out);
    ts << "-- world_editor spawn backup\n";
    ts << "-- map_id: " << m_mapId << "\n";
    ts << "-- generated: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "Z\n\n";
    ts << "BEGIN;\n\n";
    for (db::SpawnChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SpawnChangeKind::Update || c.kind == db::SpawnChangeKind::Delete)
        {
            ts << (c.before.kind == render::SpawnKind::Creature
                  ? formatCreatureBackup(m_dbClient, c.before)
                  : formatGameObjectBackup(m_dbClient, c.before)) << "\n";
        }
    }
    ts << "\nCOMMIT;\n";
    return out;
}

bool SpawnCommitDialog::writeBackupFile(QString const& sql, QString& outPath, QString& outError) const
{
    QString const dirPath = QStringLiteral("editor_backups");
    QDir().mkpath(dirPath);
    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const fileName = QStringLiteral("%1/%2_map%3_spawns.sql")
        .arg(dirPath).arg(stamp).arg(m_mapId);
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        outError = f.errorString();
        return false;
    }
    QByteArray const bytes = sql.toUtf8();
    if (f.write(bytes) != bytes.size())
    {
        outError = f.errorString();
        return false;
    }
    f.close();
    outPath = fileName;
    return true;
}

bool SpawnCommitDialog::validateChanges(QString& outError) const
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        outError = tr("not connected to DB");
        return false;
    }
    // Collect every entry referenced by Insert/Update rows, partitioned
    // by kind, then do one IN(...) probe per kind.  Also catches map-id
    // mismatches without a DB round-trip.
    std::vector<uint32_t> creatureEntries;
    std::vector<uint32_t> goEntries;
    QStringList           mapMismatches;
    for (db::SpawnChangeRecord const& c : m_model.changes())
    {
        if (c.kind != db::SpawnChangeKind::Insert
         && c.kind != db::SpawnChangeKind::Update)
            continue;
        render::Spawn const& s = c.after;
        if (s.mapId != m_mapId)
        {
            mapMismatches << QString("guid=%1 entry=%2 map=%3 (expected %4)")
                    .arg(s.guid).arg(s.entry).arg(s.mapId).arg(m_mapId);
        }
        if (s.entry == 0)
        {
            outError = tr("spawn guid=%1 has entry=0 (no template selected)")
                .arg(s.guid);
            return false;
        }
        if (s.kind == render::SpawnKind::Creature)
            creatureEntries.push_back(s.entry);
        else
            goEntries.push_back(s.entry);
    }
    if (!mapMismatches.isEmpty())
    {
        outError = tr("map id mismatch on %1 row(s): %2")
            .arg(mapMismatches.size()).arg(mapMismatches.join(QStringLiteral("; ")));
        return false;
    }
    auto probe = [&](char const* table, std::vector<uint32_t>& entries,
                     QString const& kindLabel) -> bool
    {
        if (entries.empty()) return true;
        // De-dup so the IN(...) list stays tight.
        std::sort(entries.begin(), entries.end());
        entries.erase(std::unique(entries.begin(), entries.end()), entries.end());

        QString inList;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (i) inList += QLatin1Char(',');
            inList += QString::number(entries[i]);
        }
        std::string const sql = "SELECT entry FROM " + std::string(table)
            + " WHERE entry IN (" + inList.toStdString() + ")";
        db::QueryResult res;
        auto const err = m_dbClient->query(sql, res);
        if (!err.ok())
        {
            outError = tr("%1_template probe failed: %2")
                .arg(kindLabel, QString::fromStdString(err.message));
            return false;
        }
        std::vector<uint32_t> found;
        found.reserve(res.rowCount());
        for (size_t r = 0; r < res.rowCount(); ++r)
            found.push_back(uint32_t(res.asUInt64(r, 0).value_or(0)));
        std::sort(found.begin(), found.end());

        std::vector<uint32_t> missing;
        std::set_difference(entries.begin(), entries.end(),
                            found.begin(), found.end(),
                            std::back_inserter(missing));
        if (!missing.empty())
        {
            QStringList ms;
            ms.reserve(int(missing.size()));
            for (uint32_t e : missing) ms << QString::number(e);
            outError = tr("%1: %2 entry(ies) missing from %3_template: %4")
                .arg(kindLabel)
                .arg(missing.size())
                .arg(kindLabel)
                .arg(ms.join(QStringLiteral(", ")));
            return false;
        }
        return true;
    };
    if (!probe("creature_template",   creatureEntries, QStringLiteral("creature")))   return false;
    if (!probe("gameobject_template", goEntries,       QStringLiteral("gameobject"))) return false;
    return true;
}

bool SpawnCommitDialog::applyTransaction(QString& outError)
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        outError = tr("not connected to DB");
        return false;
    }
    if (!validateChanges(outError))
        return false;
    auto fail = [&](db::QueryError const& e, char const* phase) -> bool
    {
        outError = QString("[%1] %2 (during %3)")
                   .arg(e.code).arg(QString::fromStdString(e.message))
                   .arg(QString::fromLatin1(phase));
        return false;
    };

    db::QueryError err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok()) return fail(err, "START");

    // Deletes first so a later UPDATE never touches a row we're meant
    // to remove.
    for (db::SpawnChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SpawnChangeKind::Delete)
        {
            QString const sql = (c.before.kind == render::SpawnKind::Creature)
                              ? formatCreatureDelete(c.before)
                              : formatGameObjectDelete(c.before);
            err = m_dbClient->exec(sql.toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "DELETE"); }
        }
    }
    // Updates next.
    for (db::SpawnChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SpawnChangeKind::Update)
        {
            QString const sql = (c.after.kind == render::SpawnKind::Creature)
                              ? formatCreatureUpdate(m_dbClient, c.after)
                              : formatGameObjectUpdate(m_dbClient, c.after);
            err = m_dbClient->exec(sql.toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "UPDATE"); }
        }
    }
    // Inserts last.  We've already pre-assigned the guid from
    // MainWindow's reserved range, so there's no auto_increment race.
    for (db::SpawnChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SpawnChangeKind::Insert)
        {
            QString const sql = (c.after.kind == render::SpawnKind::Creature)
                              ? formatCreatureInsert(m_dbClient, c.after)
                              : formatGameObjectInsert(m_dbClient, c.after);
            err = m_dbClient->exec(sql.toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "INSERT"); }
        }
    }

    err = m_dbClient->exec("COMMIT");
    if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "COMMIT"); }

    return refetchRows(outError);
}

bool SpawnCommitDialog::refetchRows(QString& outError)
{
    // Re-pull the entire map's spawn list.  Mirrors
    // MainWindow::reloadSpawnsForMap query shape; the model is
    // re-baselined from this.
    auto pullKind = [&](char const* table, render::SpawnKind kind) -> bool
    {
        char sql[2048];
        if (kind == render::SpawnKind::Creature)
        {
            std::snprintf(sql, sizeof(sql),
                "SELECT guid, id, map, zoneId, areaId, spawnDifficulties, "
                "       phaseUseFlags, PhaseId, PhaseGroup, terrainSwapMap, "
                "       modelid, equipment_id, position_x, position_y, position_z, "
                "       orientation, spawntimesecs, wander_distance, currentwaypoint, "
                "       curHealthPct, MovementType, npcflag, "
                "       unit_flags, unit_flags2, unit_flags3, "
                "       ScriptName, StringId, VerifiedBuild "
                "FROM %s WHERE map = %u", table, m_mapId);
        }
        else
        {
            std::snprintf(sql, sizeof(sql),
                "SELECT guid, id, map, zoneId, areaId, spawnDifficulties, "
                "       phaseUseFlags, PhaseId, PhaseGroup, terrainSwapMap, "
                "       position_x, position_y, position_z, orientation, "
                "       rotation0, rotation1, rotation2, rotation3, "
                "       spawntimesecs, animprogress, state, "
                "       ScriptName, StringId, VerifiedBuild "
                "FROM %s WHERE map = %u", table, m_mapId);
        }
        db::QueryResult res;
        db::QueryError const e = m_dbClient->query(sql, res);
        if (!e.ok())
        {
            outError = QString::fromStdString(e.message);
            return false;
        }
        for (size_t r = 0; r < res.rowCount(); ++r)
        {
            render::Spawn s;
            s.kind = kind;
            auto getU = [&](char const* col) -> uint64_t {
                return res.asUInt64(r, *res.columnIndex(col)).value_or(0);
            };
            auto getI = [&](char const* col) -> int64_t {
                return res.asInt64(r, *res.columnIndex(col)).value_or(0);
            };
            auto getF = [&](char const* col) -> float {
                return float(res.asDouble(r, *res.columnIndex(col)).value_or(0.0));
            };
            auto getS = [&](char const* col) -> QString {
                return QString::fromStdString(res.cell(r, *res.columnIndex(col)));
            };
            s.guid             = getI("guid");
            s.entry            = uint32_t(getU("id"));
            s.mapId            = uint32_t(getU("map"));
            s.zoneId           = uint16_t(getU("zoneId"));
            s.areaId           = uint16_t(getU("areaId"));
            s.spawnDifficulties = getS("spawnDifficulties");
            s.phaseUseFlags    = uint8_t(getU("phaseUseFlags"));
            s.phaseId          = uint32_t(getU("PhaseId"));
            s.phaseGroup       = uint32_t(getU("PhaseGroup"));
            s.terrainSwapMap   = int32_t(getI("terrainSwapMap"));
            s.worldX           = getF("position_x");
            s.worldY           = getF("position_y");
            s.worldZ           = getF("position_z");
            s.orientation      = getF("orientation");
            s.spawntimesecs    = uint32_t(getU("spawntimesecs"));
            s.scriptName       = getS("ScriptName");
            s.stringId         = getS("StringId");
            s.verifiedBuild    = uint32_t(getU("VerifiedBuild"));
            if (kind == render::SpawnKind::Creature)
            {
                s.modelid         = uint32_t(getU("modelid"));
                s.equipmentId     = uint8_t(getU("equipment_id"));
                s.wanderDistance  = getF("wander_distance");
                s.currentwaypoint = uint32_t(getU("currentwaypoint"));
                s.curHealthPct    = uint32_t(getU("curHealthPct"));
                s.movementType    = uint8_t(getU("MovementType"));
                s.npcflag         = getU("npcflag");
                s.unitFlags1      = uint32_t(getU("unit_flags"));
                s.unitFlags2      = uint32_t(getU("unit_flags2"));
                s.unitFlags3      = uint32_t(getU("unit_flags3"));
            }
            else
            {
                s.rotation0    = getF("rotation0");
                s.rotation1    = getF("rotation1");
                s.rotation2    = getF("rotation2");
                s.rotation3    = getF("rotation3");
                s.animprogress = uint8_t(getU("animprogress"));
                s.goState      = uint8_t(getU("state"));
            }
            m_committedRows.push_back(std::move(s));
        }
        return true;
    };

    m_committedRows.clear();
    if (!pullKind("creature",   render::SpawnKind::Creature))   return false;
    if (!pullKind("gameobject", render::SpawnKind::GameObject)) return false;
    return true;
}

void SpawnCommitDialog::onCommitClicked()
{
    m_commitButton->setEnabled(false);
    m_statusLabel->setText(tr("preparing backup..."));
    QApplication::processEvents();

    if (m_backupCheckbox->isChecked())
    {
        bool const hasBackupable = std::any_of(
            m_model.changes().begin(), m_model.changes().end(),
            [](db::SpawnChangeRecord const& c) {
                return c.kind == db::SpawnChangeKind::Update
                    || c.kind == db::SpawnChangeKind::Delete;
            });
        if (hasBackupable)
        {
            QString backupPath, backupErr;
            if (!writeBackupFile(buildBackupSql(), backupPath, backupErr))
            {
                QMessageBox::critical(this, tr("Backup write failed"),
                    tr("Aborting commit. Could not write backup file:\n%1").arg(backupErr));
                m_statusLabel->setText(tr("aborted: backup failed"));
                m_commitButton->setEnabled(true);
                return;
            }
            m_statusLabel->setText(tr("backup -> %1").arg(backupPath));
            QApplication::processEvents();
        }
    }

    QString applyErr;
    if (!applyTransaction(applyErr))
    {
        QMessageBox::critical(this, tr("Commit failed"),
            tr("Transaction rolled back.\n%1").arg(applyErr));
        m_statusLabel->setText(tr("failed: %1").arg(applyErr));
        m_commitButton->setEnabled(true);
        return;
    }
    m_statusLabel->setText(tr("committed."));
    accept();
}

} // namespace world_editor::app
