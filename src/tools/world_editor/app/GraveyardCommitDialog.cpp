#include "GraveyardCommitDialog.h"

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

QString transportClause(uint64_t v)
{
    // 0 in the model = NULL in DB; any other value writes the literal.
    return v == 0 ? QStringLiteral("NULL") : QString::number(qulonglong(v));
}

QString formatUpdate(db::MySqlClient* c, render::Graveyard const& g)
{
    return QString(
        "UPDATE world_safe_locs SET "
        "MapID=%1, LocX=%2, LocY=%3, LocZ=%4, Facing=%5, "
        "TransportSpawnId=%6, Comment='%7' "
        "WHERE ID=%8;")
        .arg(g.mapId)
        .arg(g.x, 0, 'f', 4).arg(g.y, 0, 'f', 4)
        .arg(g.z, 0, 'f', 4).arg(g.facing, 0, 'f', 4)
        .arg(transportClause(g.transportSpawnId))
        .arg(esc(c, g.comment))
        .arg(g.id);
}

QString formatInsert(db::MySqlClient* c, render::Graveyard const& g)
{
    return QString(
        "INSERT INTO world_safe_locs "
        "(ID, MapID, LocX, LocY, LocZ, Facing, TransportSpawnId, Comment) "
        "VALUES (%1, %2, %3, %4, %5, %6, %7, '%8');")
        .arg(g.id).arg(g.mapId)
        .arg(g.x, 0, 'f', 4).arg(g.y, 0, 'f', 4)
        .arg(g.z, 0, 'f', 4).arg(g.facing, 0, 'f', 4)
        .arg(transportClause(g.transportSpawnId))
        .arg(esc(c, g.comment));
}

QString formatDelete(render::Graveyard const& g)
{
    return QString("DELETE FROM world_safe_locs WHERE ID=%1;").arg(g.id);
}

QString formatBackup(db::MySqlClient* c, render::Graveyard const& g)
{
    return QString(
        "INSERT INTO world_safe_locs "
        "(ID, MapID, LocX, LocY, LocZ, Facing, TransportSpawnId, Comment) "
        "VALUES (%1, %2, %3, %4, %5, %6, %7, '%8') "
        "ON DUPLICATE KEY UPDATE "
        "MapID=VALUES(MapID), LocX=VALUES(LocX), LocY=VALUES(LocY), "
        "LocZ=VALUES(LocZ), Facing=VALUES(Facing), "
        "TransportSpawnId=VALUES(TransportSpawnId), Comment=VALUES(Comment);")
        .arg(g.id).arg(g.mapId)
        .arg(g.x, 0, 'f', 4).arg(g.y, 0, 'f', 4)
        .arg(g.z, 0, 'f', 4).arg(g.facing, 0, 'f', 4)
        .arg(transportClause(g.transportSpawnId))
        .arg(esc(c, g.comment));
}

} // namespace

GraveyardCommitDialog::GraveyardCommitDialog(db::MySqlClient* dbClient,
                                             db::GraveyardModel const& model,
                                             uint32_t mapId,
                                             QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_model(model), m_mapId(mapId)
{
    setWindowTitle(tr("Commit graveyard changes"));
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

    connect(m_commitButton, &QPushButton::clicked, this, &GraveyardCommitDialog::onCommitClicked);
    connect(buttons,        &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel(tr(
        "The following SQL will run inside a single transaction. Review before committing.")));
    outer->addWidget(m_sqlPreview, 1);
    outer->addWidget(m_backupCheckbox);
    outer->addWidget(m_statusLabel);
    outer->addWidget(buttons);
}

QString GraveyardCommitDialog::buildSqlPreview() const
{
    QString out;
    QTextStream ts(&out);
    ts << "BEGIN;\n\n";

    size_t ins = 0, upd = 0, del = 0;
    for (db::GraveyardChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::GraveyardChangeKind::Insert) ++ins;
        else if (c.kind == db::GraveyardChangeKind::Update) ++upd;
        else if (c.kind == db::GraveyardChangeKind::Delete) ++del;
    }

    if (ins > 0)
    {
        ts << "-- " << ins << " INSERT(s)\n";
        for (db::GraveyardChangeRecord const& c : m_model.changes())
            if (c.kind == db::GraveyardChangeKind::Insert)
                ts << formatInsert(m_dbClient, c.after) << "\n";
        ts << "\n";
    }
    if (upd > 0)
    {
        ts << "-- " << upd << " UPDATE(s)\n";
        for (db::GraveyardChangeRecord const& c : m_model.changes())
            if (c.kind == db::GraveyardChangeKind::Update)
                ts << formatUpdate(m_dbClient, c.after) << "\n";
        ts << "\n";
    }
    if (del > 0)
    {
        ts << "-- " << del << " DELETE(s)\n";
        for (db::GraveyardChangeRecord const& c : m_model.changes())
            if (c.kind == db::GraveyardChangeKind::Delete)
                ts << formatDelete(c.before) << "\n";
        ts << "\n";
    }

    ts << "COMMIT;\n";
    return out;
}

QString GraveyardCommitDialog::buildBackupSql() const
{
    QString out;
    QTextStream ts(&out);
    ts << "-- world_editor graveyard backup\n";
    ts << "-- map_id: " << m_mapId << "\n";
    ts << "-- generated: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "Z\n\n";
    ts << "BEGIN;\n\n";
    for (db::GraveyardChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::GraveyardChangeKind::Update
         || c.kind == db::GraveyardChangeKind::Delete)
        {
            ts << formatBackup(m_dbClient, c.before) << "\n";
        }
    }
    ts << "\nCOMMIT;\n";
    return out;
}

bool GraveyardCommitDialog::writeBackupFile(QString const& sql, QString& outPath, QString& outError) const
{
    QString const dirPath = QStringLiteral("editor_backups");
    QDir().mkpath(dirPath);
    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const fileName = QStringLiteral("%1/%2_map%3_graveyards.sql")
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

bool GraveyardCommitDialog::applyTransaction(QString& outError)
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        outError = tr("not connected to DB");
        return false;
    }
    auto fail = [&](db::QueryError const& e, char const* phase) -> bool
    {
        outError = QString("[%1] %2 (during %3)")
                   .arg(e.code).arg(QString::fromStdString(e.message))
                   .arg(QString::fromLatin1(phase));
        return false;
    };

    db::QueryError err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok()) return fail(err, "START");

    for (db::GraveyardChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::GraveyardChangeKind::Delete)
        {
            err = m_dbClient->exec(formatDelete(c.before).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "DELETE"); }
        }
    }
    for (db::GraveyardChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::GraveyardChangeKind::Update)
        {
            err = m_dbClient->exec(formatUpdate(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "UPDATE"); }
        }
    }
    for (db::GraveyardChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::GraveyardChangeKind::Insert)
        {
            err = m_dbClient->exec(formatInsert(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "INSERT"); }
        }
    }

    err = m_dbClient->exec("COMMIT");
    if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "COMMIT"); }

    return refetchRows(outError);
}

bool GraveyardCommitDialog::refetchRows(QString& outError)
{
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT ID, COALESCE(MapID, 0), COALESCE(LocX, 0), COALESCE(LocY, 0), "
        "       COALESCE(LocZ, 0), COALESCE(Facing, 0), "
        "       COALESCE(TransportSpawnId, 0), COALESCE(Comment, '') "
        "FROM world_safe_locs WHERE MapID = %u", m_mapId);
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
        render::Graveyard g;
        g.id               = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        g.mapId            = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(m_mapId));
        g.x                = static_cast<float>(res.asDouble(r, 2).value_or(0.0));
        g.y                = static_cast<float>(res.asDouble(r, 3).value_or(0.0));
        g.z                = static_cast<float>(res.asDouble(r, 4).value_or(0.0));
        g.facing           = static_cast<float>(res.asDouble(r, 5).value_or(0.0));
        g.transportSpawnId = res.asUInt64(r, 6).value_or(0);
        g.comment          = QString::fromStdString(res.cell(r, 7));
        m_committedRows.push_back(std::move(g));
    }
    return true;
}

void GraveyardCommitDialog::onCommitClicked()
{
    m_commitButton->setEnabled(false);
    m_statusLabel->setText(tr("preparing backup..."));
    QApplication::processEvents();

    if (m_backupCheckbox->isChecked())
    {
        bool const hasBackupable = std::any_of(
            m_model.changes().begin(), m_model.changes().end(),
            [](db::GraveyardChangeRecord const& c) {
                return c.kind == db::GraveyardChangeKind::Update
                    || c.kind == db::GraveyardChangeKind::Delete;
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
