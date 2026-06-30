#include "CommitDialog.h"

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
#include <QTextStream>
#include <QVBoxLayout>

#include <cstdio>

namespace world_editor::app
{

namespace
{
// playerbot_v2_world_metadata lives in the operator-configured shared playerbot
// schema (MySqlClient::qualify, from the connection's "Shared playerbot DB"
// field) — never a hardcoded DB name. Shared across all world DBs.
QString metaTable(db::MySqlClient* client)
{
    return QString::fromStdString(client->qualify("playerbot_v2_world_metadata"));
}

QString escapeSqlString(db::MySqlClient* client, QString const& v)
{
    if (!client)
        return v;
    return QString::fromStdString(client->escapeString(v.toStdString()));
}

QString formatInsert(db::MySqlClient* client, render::Annotation const& a)
{
    return QString(
        "INSERT INTO %1 "
        "(map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, label, notes, created_by) "
        "VALUES (%2, %3, %4, %5, %6, %7, %8, '%9', '%10', '%11');")
        .arg(metaTable(client))
        .arg(a.mapId).arg(a.zoneId).arg(uint8_t(a.kind))
        .arg(a.x, 0, 'f', 4).arg(a.y, 0, 'f', 4).arg(a.z, 0, 'f', 4)
        .arg(a.radius, 0, 'f', 4)
        .arg(escapeSqlString(client, a.label))
        .arg(escapeSqlString(client, a.notes))
        .arg(escapeSqlString(client, a.createdBy));
}

QString formatUpdate(db::MySqlClient* client, render::Annotation const& a)
{
    return QString(
        "UPDATE %1 SET radius = %2, label = '%3', notes = '%4' WHERE id = %5;")
        .arg(metaTable(client))
        .arg(a.radius, 0, 'f', 4)
        .arg(escapeSqlString(client, a.label))
        .arg(escapeSqlString(client, a.notes))
        .arg(a.id);
}

QString formatDelete(db::MySqlClient* client, render::Annotation const& a)
{
    return QString("DELETE FROM %1 WHERE id = %2;")
        .arg(metaTable(client))
        .arg(a.id);
}

QString formatBackupInsert(db::MySqlClient* client, render::Annotation const& a)
{
    // Backup uses INSERT ... ON DUPLICATE KEY UPDATE so a re-apply
    // restores the row whether or not it still exists in DB.
    return QString(
        "INSERT INTO %1 "
        "(id, map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, label, notes, created_by) "
        "VALUES (%2, %3, %4, %5, %6, %7, %8, %9, '%10', '%11', '%12') "
        "ON DUPLICATE KEY UPDATE "
        "radius = VALUES(radius), label = VALUES(label), notes = VALUES(notes);")
        .arg(metaTable(client))
        .arg(a.id).arg(a.mapId).arg(a.zoneId).arg(uint8_t(a.kind))
        .arg(a.x, 0, 'f', 4).arg(a.y, 0, 'f', 4).arg(a.z, 0, 'f', 4)
        .arg(a.radius, 0, 'f', 4)
        .arg(escapeSqlString(client, a.label))
        .arg(escapeSqlString(client, a.notes))
        .arg(escapeSqlString(client, a.createdBy));
}
} // namespace

CommitDialog::CommitDialog(db::MySqlClient* dbClient,
                           db::AnnotationModel const& model,
                           uint32_t mapId,
                           QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_model(model), m_mapId(mapId)
{
    setWindowTitle(tr("Commit annotation changes"));
    setModal(true);
    resize(720, 540);

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

    connect(m_commitButton, &QPushButton::clicked, this, &CommitDialog::onCommitClicked);
    connect(buttons,        &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel(tr(
        "The following SQL will run inside a single transaction. Review before committing.")));
    outer->addWidget(m_sqlPreview, 1);
    outer->addWidget(m_backupCheckbox);
    outer->addWidget(m_statusLabel);
    outer->addWidget(buttons);
}

QString CommitDialog::buildSqlPreview() const
{
    QString out;
    QTextStream ts(&out);
    ts << "BEGIN;\n\n";

    size_t inserts = 0, updates = 0, deletes = 0;
    for (db::ChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ChangeKind::Insert) ++inserts;
        if (c.kind == db::ChangeKind::Update) ++updates;
        if (c.kind == db::ChangeKind::Delete) ++deletes;
    }

    if (inserts > 0)
    {
        ts << "-- " << inserts << " INSERT(s)\n";
        for (db::ChangeRecord const& c : m_model.changes())
            if (c.kind == db::ChangeKind::Insert)
                ts << formatInsert(m_dbClient, c.after) << "\n";
        ts << "\n";
    }
    if (updates > 0)
    {
        ts << "-- " << updates << " UPDATE(s)\n";
        for (db::ChangeRecord const& c : m_model.changes())
            if (c.kind == db::ChangeKind::Update)
                ts << formatUpdate(m_dbClient, c.after) << "\n";
        ts << "\n";
    }
    if (deletes > 0)
    {
        ts << "-- " << deletes << " DELETE(s)\n";
        for (db::ChangeRecord const& c : m_model.changes())
            if (c.kind == db::ChangeKind::Delete)
                ts << formatDelete(m_dbClient, c.before) << "\n";
        ts << "\n";
    }

    ts << "COMMIT;\n";
    return out;
}

QString CommitDialog::buildBackupSql() const
{
    QString out;
    QTextStream ts(&out);
    ts << "-- world_editor backup\n";
    ts << "-- map_id: " << m_mapId << "\n";
    ts << "-- generated: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "Z\n";
    ts << "-- Re-apply with: mysql -u <user> -p < THIS_FILE\n\n";
    ts << "BEGIN;\n\n";
    for (db::ChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ChangeKind::Update || c.kind == db::ChangeKind::Delete)
            ts << formatBackupInsert(m_dbClient, c.before) << "\n";
    }
    ts << "\nCOMMIT;\n";
    return out;
}

bool CommitDialog::writeBackupFile(QString const& sql, QString& outPath, QString& outError) const
{
    QString const dirPath = QStringLiteral("editor_backups");
    QDir().mkpath(dirPath);

    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const fileName = QStringLiteral("%1/%2_map%3_annotations.sql")
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

bool CommitDialog::applyTransaction(QString& outError)
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        outError = tr("not connected to DB");
        return false;
    }

    auto failWith = [&](db::QueryError const& e, char const* phase) -> bool
    {
        outError = QString("[%1] %2 (during %3)")
                    .arg(e.code).arg(QString::fromStdString(e.message))
                    .arg(QString::fromLatin1(phase));
        return false;
    };

    db::QueryError err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok()) return failWith(err, "START");

    // Deletes first: avoids edge cases where an UPDATE refreshes a row
    // we're then asked to delete.
    for (db::ChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ChangeKind::Delete)
        {
            err = m_dbClient->exec(formatDelete(m_dbClient, c.before).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return failWith(err, "DELETE"); }
        }
    }

    // Updates next.
    for (db::ChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ChangeKind::Update)
        {
            err = m_dbClient->exec(formatUpdate(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return failWith(err, "UPDATE"); }
        }
    }

    // Inserts last; collect their new ids so the post-commit viewer
    // refresh has authoritative ids.
    for (db::ChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ChangeKind::Insert)
        {
            err = m_dbClient->exec(formatInsert(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return failWith(err, "INSERT"); }
        }
    }

    err = m_dbClient->exec("COMMIT");
    if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return failWith(err, "COMMIT"); }

    // Re-read the table to refresh the viewer.
    std::string const metaTbl = m_dbClient->qualify("playerbot_v2_world_metadata");
    char sqlBuf[400];
    std::snprintf(sqlBuf, sizeof(sqlBuf),
        "SELECT id, map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, "
        "       label, notes, created_by "
        "FROM %s "
        "WHERE map_id = %u", metaTbl.c_str(), m_mapId);
    db::QueryResult res;
    err = m_dbClient->query(sqlBuf, res);
    if (!err.ok()) return failWith(err, "post-commit SELECT");

    m_committedRows.clear();
    m_committedRows.reserve(res.rowCount());
    auto const idxId        = res.columnIndex("id");
    auto const idxMap       = res.columnIndex("map_id");
    auto const idxZone      = res.columnIndex("zone_id");
    auto const idxKind      = res.columnIndex("kind");
    auto const idxX         = res.columnIndex("pos_x");
    auto const idxY         = res.columnIndex("pos_y");
    auto const idxZ         = res.columnIndex("pos_z");
    auto const idxR         = res.columnIndex("radius");
    auto const idxLabel     = res.columnIndex("label");
    auto const idxNotes     = res.columnIndex("notes");
    auto const idxCreatedBy = res.columnIndex("created_by");
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::Annotation a;
        a.id     = res.asInt64 (r, *idxId).value_or(0);
        a.mapId  = static_cast<uint32_t>(res.asUInt64(r, *idxMap).value_or(m_mapId));
        a.zoneId = idxZone ? static_cast<uint32_t>(res.asUInt64(r, *idxZone).value_or(0)) : 0;
        uint64_t const kindRaw = res.asUInt64(r, *idxKind).value_or(0);
        a.kind   = (kindRaw < static_cast<uint64_t>(render::AnnotationKind::Count_))
                 ? static_cast<render::AnnotationKind>(kindRaw)
                 : render::AnnotationKind::Unknown;
        a.x      = static_cast<float>(res.asDouble(r, *idxX).value_or(0.0));
        a.y      = static_cast<float>(res.asDouble(r, *idxY).value_or(0.0));
        a.z      = static_cast<float>(res.asDouble(r, *idxZ).value_or(0.0));
        a.radius = static_cast<float>(res.asDouble(r, *idxR).value_or(10.0));
        if (idxLabel     && !res.isNull(r, *idxLabel))     a.label     = QString::fromStdString(res.cell(r, *idxLabel));
        if (idxNotes     && !res.isNull(r, *idxNotes))     a.notes     = QString::fromStdString(res.cell(r, *idxNotes));
        if (idxCreatedBy && !res.isNull(r, *idxCreatedBy)) a.createdBy = QString::fromStdString(res.cell(r, *idxCreatedBy));
        m_committedRows.push_back(std::move(a));
    }
    return true;
}

void CommitDialog::onCommitClicked()
{
    m_commitButton->setEnabled(false);
    m_statusLabel->setText(tr("preparing backup..."));
    QApplication::processEvents();

    if (m_backupCheckbox->isChecked())
    {
        bool const hasBackupable = std::any_of(
            m_model.changes().begin(), m_model.changes().end(),
            [](db::ChangeRecord const& c) {
                return c.kind == db::ChangeKind::Update || c.kind == db::ChangeKind::Delete;
            });
        if (hasBackupable)
        {
            QString const backupSql = buildBackupSql();
            QString backupPath, backupErr;
            if (!writeBackupFile(backupSql, backupPath, backupErr))
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
