#include "AreatriggerCommitDialog.h"

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

QString formatUpdate(db::MySqlClient* c, render::Areatrigger const& a)
{
    // The PK is SpawnId alone, but IsCustom is part of the lookup join
    // against create_properties (composite PK there) - we still allow
    // editing IsCustom in case the operator needs to promote a row.
    return QString(
        "UPDATE areatrigger SET "
        "AreaTriggerCreatePropertiesId=%1, IsCustom=%2, "
        "SpawnDifficulties='%3', "
        "PosX=%4, PosY=%5, PosZ=%6, Orientation=%7, "
        "PhaseUseFlags=%8, PhaseId=%9, PhaseGroup=%10, "
        "ScriptName='%11', Comment='%12', VerifiedBuild=%13 "
        "WHERE SpawnId=%14;")
        .arg(a.createPropsId).arg(uint8_t(a.isCustom))
        .arg(esc(c, a.spawnDifficulties))
        .arg(a.x, 0, 'f', 4).arg(a.y, 0, 'f', 4)
        .arg(a.z, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.comment))
        .arg(a.verifiedBuild)
        .arg(a.spawnId);
}

QString formatInsert(db::MySqlClient* c, render::Areatrigger const& a)
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
        .arg(esc(c, a.scriptName)).arg(esc(c, a.comment))
        .arg(a.verifiedBuild);
}

QString formatDelete(render::Areatrigger const& a)
{
    return QString("DELETE FROM areatrigger WHERE SpawnId=%1;").arg(a.spawnId);
}

QString formatBackup(db::MySqlClient* c, render::Areatrigger const& a)
{
    // Re-applyable full-row INSERT...ON DUPLICATE KEY UPDATE for
    // before-image rollback.
    return QString(
        "INSERT INTO areatrigger "
        "(SpawnId, AreaTriggerCreatePropertiesId, IsCustom, MapId, "
        " SpawnDifficulties, PosX, PosY, PosZ, Orientation, "
        " PhaseUseFlags, PhaseId, PhaseGroup, ScriptName, Comment, VerifiedBuild) "
        "VALUES (%1, %2, %3, %4, '%5', %6, %7, %8, %9, %10, %11, %12, '%13', '%14', %15) "
        "ON DUPLICATE KEY UPDATE "
        "AreaTriggerCreatePropertiesId=VALUES(AreaTriggerCreatePropertiesId), "
        "IsCustom=VALUES(IsCustom), MapId=VALUES(MapId), "
        "SpawnDifficulties=VALUES(SpawnDifficulties), "
        "PosX=VALUES(PosX), PosY=VALUES(PosY), PosZ=VALUES(PosZ), "
        "Orientation=VALUES(Orientation), "
        "PhaseUseFlags=VALUES(PhaseUseFlags), PhaseId=VALUES(PhaseId), "
        "PhaseGroup=VALUES(PhaseGroup), ScriptName=VALUES(ScriptName), "
        "Comment=VALUES(Comment), VerifiedBuild=VALUES(VerifiedBuild);")
        .arg(a.spawnId).arg(a.createPropsId).arg(uint8_t(a.isCustom)).arg(a.mapId)
        .arg(esc(c, a.spawnDifficulties))
        .arg(a.x, 0, 'f', 4).arg(a.y, 0, 'f', 4)
        .arg(a.z, 0, 'f', 4).arg(a.orientation, 0, 'f', 4)
        .arg(uint8_t(a.phaseUseFlags)).arg(a.phaseId).arg(a.phaseGroup)
        .arg(esc(c, a.scriptName)).arg(esc(c, a.comment))
        .arg(a.verifiedBuild);
}

} // namespace

AreatriggerCommitDialog::AreatriggerCommitDialog(db::MySqlClient* dbClient,
                                                 db::AreatriggerModel const& model,
                                                 uint32_t mapId,
                                                 QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_model(model), m_mapId(mapId)
{
    setWindowTitle(tr("Commit areatrigger changes"));
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

    connect(m_commitButton, &QPushButton::clicked, this, &AreatriggerCommitDialog::onCommitClicked);
    connect(buttons,        &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel(tr(
        "The following SQL will run inside a single transaction. Review before committing.")));
    outer->addWidget(m_sqlPreview, 1);
    outer->addWidget(m_backupCheckbox);
    outer->addWidget(m_statusLabel);
    outer->addWidget(buttons);
}

QString AreatriggerCommitDialog::buildSqlPreview() const
{
    QString out;
    QTextStream ts(&out);
    ts << "BEGIN;\n\n";

    size_t ins = 0, upd = 0, del = 0;
    for (db::AreatriggerChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::AreatriggerChangeKind::Insert) ++ins;
        else if (c.kind == db::AreatriggerChangeKind::Update) ++upd;
        else if (c.kind == db::AreatriggerChangeKind::Delete) ++del;
    }

    if (ins > 0)
    {
        ts << "-- " << ins << " INSERT(s)\n";
        for (db::AreatriggerChangeRecord const& c : m_model.changes())
            if (c.kind == db::AreatriggerChangeKind::Insert)
                ts << formatInsert(m_dbClient, c.after) << "\n";
        ts << "\n";
    }
    if (upd > 0)
    {
        ts << "-- " << upd << " UPDATE(s)\n";
        for (db::AreatriggerChangeRecord const& c : m_model.changes())
            if (c.kind == db::AreatriggerChangeKind::Update)
                ts << formatUpdate(m_dbClient, c.after) << "\n";
        ts << "\n";
    }
    if (del > 0)
    {
        ts << "-- " << del << " DELETE(s)\n";
        for (db::AreatriggerChangeRecord const& c : m_model.changes())
            if (c.kind == db::AreatriggerChangeKind::Delete)
                ts << formatDelete(c.before) << "\n";
        ts << "\n";
    }

    ts << "COMMIT;\n";
    return out;
}

QString AreatriggerCommitDialog::buildBackupSql() const
{
    QString out;
    QTextStream ts(&out);
    ts << "-- world_editor areatrigger backup\n";
    ts << "-- map_id: " << m_mapId << "\n";
    ts << "-- generated: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "Z\n\n";
    ts << "BEGIN;\n\n";
    for (db::AreatriggerChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::AreatriggerChangeKind::Update
         || c.kind == db::AreatriggerChangeKind::Delete)
        {
            ts << formatBackup(m_dbClient, c.before) << "\n";
        }
    }
    ts << "\nCOMMIT;\n";
    return out;
}

bool AreatriggerCommitDialog::writeBackupFile(QString const& sql, QString& outPath, QString& outError) const
{
    QString const dirPath = QStringLiteral("editor_backups");
    QDir().mkpath(dirPath);
    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const fileName = QStringLiteral("%1/%2_map%3_areatriggers.sql")
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

bool AreatriggerCommitDialog::validateChanges(QString& outError) const
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        outError = tr("not connected to DB");
        return false;
    }
    // Collect unique (createPropsId, isCustom) pairs across all
    // Insert/Update rows.  Map mismatches caught first (no DB needed).
    std::vector<std::pair<uint32_t, uint8_t>> pairs;
    QStringList mapMismatches;
    for (db::AreatriggerChangeRecord const& c : m_model.changes())
    {
        if (c.kind != db::AreatriggerChangeKind::Insert
         && c.kind != db::AreatriggerChangeKind::Update)
            continue;
        render::Areatrigger const& a = c.after;
        if (a.mapId != m_mapId)
        {
            mapMismatches << QString("SpawnId=%1 map=%2 (expected %3)")
                .arg(a.spawnId).arg(a.mapId).arg(m_mapId);
        }
        if (a.createPropsId == 0)
        {
            outError = tr("areatrigger SpawnId=%1 has createPropsId=0")
                .arg(a.spawnId);
            return false;
        }
        pairs.emplace_back(a.createPropsId, a.isCustom);
    }
    if (!mapMismatches.isEmpty())
    {
        outError = tr("map id mismatch on %1 row(s): %2")
            .arg(mapMismatches.size()).arg(mapMismatches.join(QStringLiteral("; ")));
        return false;
    }
    if (pairs.empty()) return true;

    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());

    // Probe each (Id, IsCustom) pair via OR'd predicate.  Could batch
    // into a tuple-in-list if we ever cared; pair count per commit is
    // small in practice.
    QStringList predicates;
    predicates.reserve(int(pairs.size()));
    for (auto const& [id, isCustom] : pairs)
    {
        predicates << QString("(Id=%1 AND IsCustom=%2)").arg(id).arg(int(isCustom));
    }
    std::string const sql = "SELECT Id, IsCustom FROM areatrigger_create_properties WHERE "
        + predicates.join(QStringLiteral(" OR ")).toStdString();
    db::QueryResult res;
    auto const err = m_dbClient->query(sql, res);
    if (!err.ok())
    {
        outError = tr("areatrigger_create_properties probe failed: %1")
            .arg(QString::fromStdString(err.message));
        return false;
    }
    std::vector<std::pair<uint32_t, uint8_t>> found;
    found.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        found.emplace_back(
            uint32_t(res.asUInt64(r, 0).value_or(0)),
            uint8_t(res.asUInt64(r, 1).value_or(0)));
    }
    std::sort(found.begin(), found.end());
    std::vector<std::pair<uint32_t, uint8_t>> missing;
    std::set_difference(pairs.begin(), pairs.end(),
                        found.begin(), found.end(),
                        std::back_inserter(missing));
    if (!missing.empty())
    {
        QStringList ms;
        for (auto const& [id, isCustom] : missing)
            ms << QString("(%1,%2)").arg(id).arg(int(isCustom));
        outError = tr("missing create_properties FK pairs: %1").arg(ms.join(QStringLiteral(", ")));
        return false;
    }
    return true;
}

bool AreatriggerCommitDialog::applyTransaction(QString& outError)
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

    for (db::AreatriggerChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::AreatriggerChangeKind::Delete)
        {
            err = m_dbClient->exec(formatDelete(c.before).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "DELETE"); }
        }
    }
    for (db::AreatriggerChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::AreatriggerChangeKind::Update)
        {
            err = m_dbClient->exec(formatUpdate(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "UPDATE"); }
        }
    }
    for (db::AreatriggerChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::AreatriggerChangeKind::Insert)
        {
            err = m_dbClient->exec(formatInsert(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "INSERT"); }
        }
    }

    err = m_dbClient->exec("COMMIT");
    if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "COMMIT"); }

    return refetchRows(outError);
}

bool AreatriggerCommitDialog::refetchRows(QString& outError)
{
    char sql[1500];
    std::snprintf(sql, sizeof(sql),
        "SELECT a.SpawnId, a.AreaTriggerCreatePropertiesId, a.IsCustom, a.MapId, "
        "       a.SpawnDifficulties, a.PosX, a.PosY, a.PosZ, a.Orientation, "
        "       COALESCE(a.PhaseUseFlags, 0), COALESCE(a.PhaseId, 0), "
        "       COALESCE(a.PhaseGroup, 0), "
        "       a.ScriptName, COALESCE(a.Comment, ''), a.VerifiedBuild, "
        "       COALESCE(p.Shape, 0), "
        "       COALESCE(p.ShapeData0, 0), COALESCE(p.ShapeData1, 0), "
        "       COALESCE(p.ShapeData2, 0), COALESCE(p.ShapeData3, 0), "
        "       COALESCE(p.ShapeData4, 0), COALESCE(p.ShapeData5, 0), "
        "       COALESCE(p.ShapeData6, 0), COALESCE(p.ShapeData7, 0) "
        "FROM areatrigger a "
        "LEFT JOIN areatrigger_create_properties p "
        "       ON p.Id = a.AreaTriggerCreatePropertiesId "
        "      AND p.IsCustom = a.IsCustom "
        "WHERE a.MapId = %u", m_mapId);
    db::QueryResult res;
    db::QueryError const e = m_dbClient->query(sql, res);
    if (!e.ok())
    {
        outError = QString::fromStdString(e.message);
        return false;
    }
    m_committedRows.clear();
    m_committedRows.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::Areatrigger a;
        a.spawnId            = res.asInt64 (r, 0).value_or(0);
        a.createPropsId      = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(0));
        a.isCustom           = static_cast<uint8_t>(res.asUInt64(r, 2).value_or(0));
        a.mapId              = static_cast<uint32_t>(res.asUInt64(r, 3).value_or(m_mapId));
        a.spawnDifficulties  = QString::fromStdString(res.cell(r, 4));
        a.x                  = static_cast<float>(res.asDouble(r, 5).value_or(0.0));
        a.y                  = static_cast<float>(res.asDouble(r, 6).value_or(0.0));
        a.z                  = static_cast<float>(res.asDouble(r, 7).value_or(0.0));
        a.orientation        = static_cast<float>(res.asDouble(r, 8).value_or(0.0));
        a.phaseUseFlags      = static_cast<uint8_t>(res.asUInt64(r, 9).value_or(0));
        a.phaseId            = static_cast<uint32_t>(res.asUInt64(r, 10).value_or(0));
        a.phaseGroup         = static_cast<uint32_t>(res.asUInt64(r, 11).value_or(0));
        a.scriptName         = QString::fromStdString(res.cell(r, 12));
        a.comment            = QString::fromStdString(res.cell(r, 13));
        a.verifiedBuild      = static_cast<uint32_t>(res.asUInt64(r, 14).value_or(0));
        a.shape              = uint8_t(res.asUInt64(r, 15).value_or(0));
        for (int k = 0; k < 8; ++k)
            a.shapeData[k] = float(res.asDouble(r, 16 + k).value_or(0.0));
        m_committedRows.push_back(std::move(a));
    }
    return true;
}

void AreatriggerCommitDialog::onCommitClicked()
{
    m_commitButton->setEnabled(false);
    m_statusLabel->setText(tr("preparing backup..."));
    QApplication::processEvents();

    if (m_backupCheckbox->isChecked())
    {
        bool const hasBackupable = std::any_of(
            m_model.changes().begin(), m_model.changes().end(),
            [](db::AreatriggerChangeRecord const& c) {
                return c.kind == db::AreatriggerChangeKind::Update
                    || c.kind == db::AreatriggerChangeKind::Delete;
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
