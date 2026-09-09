#include "WaypointCommitDialog.h"

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

QString formatPathHeaderInsert(db::MySqlClient* c, render::Path const& p)
{
    return QString(
        "INSERT INTO waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "VALUES (%1, %2, %3, %4, '%5');")
        .arg(p.pathId).arg(uint8_t(p.moveType)).arg(uint8_t(p.flags))
        .arg(p.velocity, 0, 'f', 4).arg(esc(c, p.comment));
}

QString formatPathHeaderUpdate(db::MySqlClient* c, render::Path const& p)
{
    return QString(
        "UPDATE waypoint_path SET MoveType=%1, Flags=%2, Velocity=%3, Comment='%4' "
        "WHERE PathId=%5;")
        .arg(uint8_t(p.moveType)).arg(uint8_t(p.flags))
        .arg(p.velocity, 0, 'f', 4).arg(esc(c, p.comment))
        .arg(p.pathId);
}

QString formatNodeInsert(render::PathNode const& n, uint32_t pathId)
{
    return QString(
        "INSERT INTO waypoint_path_node "
        "(PathId, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay) "
        "VALUES (%1, %2, %3, %4, %5, %6, %7);")
        .arg(pathId).arg(n.nodeId)
        .arg(n.x, 0, 'f', 4).arg(n.y, 0, 'f', 4).arg(n.z, 0, 'f', 4)
        .arg(n.orientation, 0, 'f', 4).arg(n.delay);
}

QString formatDeleteNodes(uint32_t pathId)
{
    return QString("DELETE FROM waypoint_path_node WHERE PathId=%1;").arg(pathId);
}

QString formatDeletePath(uint32_t pathId)
{
    return QString("DELETE FROM waypoint_path WHERE PathId=%1;").arg(pathId);
}

QString formatPathBackup(db::MySqlClient* c, render::Path const& p)
{
    QString out;
    QTextStream ts(&out);
    ts << QString(
        "INSERT INTO waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "VALUES (%1, %2, %3, %4, '%5') "
        "ON DUPLICATE KEY UPDATE MoveType=VALUES(MoveType), Flags=VALUES(Flags), "
        "Velocity=VALUES(Velocity), Comment=VALUES(Comment);\n")
        .arg(p.pathId).arg(uint8_t(p.moveType)).arg(uint8_t(p.flags))
        .arg(p.velocity, 0, 'f', 4).arg(esc(c, p.comment));
    ts << formatDeleteNodes(p.pathId) << "\n";
    for (auto const& n : p.nodes)
        ts << formatNodeInsert(n, p.pathId) << "\n";
    return out;
}

} // namespace

WaypointCommitDialog::WaypointCommitDialog(db::MySqlClient* dbClient,
                                           db::WaypointModel const& model,
                                           uint32_t mapId,
                                           QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_model(model), m_mapId(mapId)
{
    setWindowTitle(tr("Commit waypoint paths"));
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

    auto* buttons = new QDialogButtonBox(this);
    m_commitButton = buttons->addButton(tr("Commit"), QDialogButtonBox::AcceptRole);
    m_commitButton->setDefault(true);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(m_commitButton, &QPushButton::clicked, this, &WaypointCommitDialog::onCommitClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel(tr("Review SQL. UPDATEs rewrite the path's nodes wholesale.")));
    outer->addWidget(m_sqlPreview, 1);
    outer->addWidget(m_backupCheckbox);
    outer->addWidget(m_statusLabel);
    outer->addWidget(buttons);
}

QString WaypointCommitDialog::buildSqlPreview() const
{
    QString out;
    QTextStream ts(&out);
    ts << "BEGIN;\n\n";
    for (db::PathChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::PathChangeKind::Insert)
        {
            ts << "-- INSERT path " << c.after.pathId << " (" << c.after.nodes.size() << " nodes)\n";
            ts << formatPathHeaderInsert(m_dbClient, c.after) << "\n";
            for (auto const& n : c.after.nodes)
                ts << formatNodeInsert(n, c.after.pathId) << "\n";
            ts << "\n";
        }
    }
    for (db::PathChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::PathChangeKind::Update)
        {
            ts << "-- UPDATE path " << c.after.pathId << " (rewrite " << c.after.nodes.size() << " nodes)\n";
            ts << formatPathHeaderUpdate(m_dbClient, c.after) << "\n";
            ts << formatDeleteNodes(c.after.pathId) << "\n";
            for (auto const& n : c.after.nodes)
                ts << formatNodeInsert(n, c.after.pathId) << "\n";
            ts << "\n";
        }
    }
    for (db::PathChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::PathChangeKind::Delete)
        {
            ts << "-- DELETE path " << c.before.pathId << "\n";
            ts << formatDeleteNodes(c.before.pathId) << "\n";
            ts << formatDeletePath(c.before.pathId) << "\n";
        }
    }
    ts << "\nCOMMIT;\n";
    return out;
}

QString WaypointCommitDialog::buildBackupSql() const
{
    QString out;
    QTextStream ts(&out);
    ts << "-- world_editor waypoint backup\n";
    ts << "-- generated: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "Z\n\n";
    ts << "BEGIN;\n\n";
    for (db::PathChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::PathChangeKind::Update || c.kind == db::PathChangeKind::Delete)
            ts << formatPathBackup(m_dbClient, c.before) << "\n";
    }
    ts << "COMMIT;\n";
    return out;
}

bool WaypointCommitDialog::writeBackupFile(QString const& sql, QString& outPath, QString& outError) const
{
    QString const dirPath = QStringLiteral("editor_backups");
    QDir().mkpath(dirPath);
    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const fileName = QStringLiteral("%1/%2_map%3_paths.sql")
        .arg(dirPath).arg(stamp).arg(m_mapId);
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    { outError = f.errorString(); return false; }
    QByteArray const bytes = sql.toUtf8();
    if (f.write(bytes) != bytes.size()) { outError = f.errorString(); return false; }
    f.close();
    outPath = fileName;
    return true;
}

bool WaypointCommitDialog::applyTransaction(QString& outError)
{
    if (!m_dbClient || !m_dbClient->isConnected())
    { outError = tr("not connected"); return false; }
    auto fail = [&](db::QueryError const& e, char const* phase) -> bool {
        outError = QString("[%1] %2 (during %3)").arg(e.code)
                  .arg(QString::fromStdString(e.message)).arg(QString::fromLatin1(phase));
        return false;
    };

    db::QueryError err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok()) return fail(err, "BEGIN");

    // Deletes first (full path purge).
    for (db::PathChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::PathChangeKind::Delete)
        {
            err = m_dbClient->exec(formatDeleteNodes(c.before.pathId).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "DELETE nodes"); }
            err = m_dbClient->exec(formatDeletePath(c.before.pathId).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "DELETE path"); }
        }
    }
    // Updates: header + node-rewrite.
    for (db::PathChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::PathChangeKind::Update)
        {
            err = m_dbClient->exec(formatPathHeaderUpdate(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "UPDATE header"); }
            err = m_dbClient->exec(formatDeleteNodes(c.after.pathId).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "DELETE old nodes"); }
            for (auto const& n : c.after.nodes)
            {
                err = m_dbClient->exec(formatNodeInsert(n, c.after.pathId).toStdString());
                if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "INSERT new node"); }
            }
        }
    }
    // Inserts: header + nodes.
    for (db::PathChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::PathChangeKind::Insert)
        {
            err = m_dbClient->exec(formatPathHeaderInsert(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "INSERT header"); }
            for (auto const& n : c.after.nodes)
            {
                err = m_dbClient->exec(formatNodeInsert(n, c.after.pathId).toStdString());
                if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "INSERT node"); }
            }
        }
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "COMMIT"); }

    return refetchRows(outError);
}

bool WaypointCommitDialog::refetchRows(QString& outError)
{
    // Re-pull paths for this map (same shape as MainWindow::reloadPathsForMap).
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "SELECT wp.PathId, wp.MoveType, wp.Flags, "
        "       COALESCE(wp.Velocity, 0), COALESCE(wp.Comment, '') "
        "FROM waypoint_path wp "
        "JOIN (SELECT DISTINCT ca.PathId AS pid "
        "      FROM creature_addon ca "
        "      JOIN creature c ON c.guid = ca.guid "
        "      WHERE c.map = %u AND ca.PathId > 0) used "
        "ON used.pid = wp.PathId", m_mapId);
    db::QueryResult res;
    auto err = m_dbClient->query(sql, res);
    if (!err.ok()) { outError = QString::fromStdString(err.message); return false; }

    m_committedRows.clear();
    std::vector<uint32_t> ids;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::Path p;
        p.pathId   = uint32_t(res.asUInt64(r, 0).value_or(0));
        p.moveType = uint8_t (res.asUInt64(r, 1).value_or(0));
        p.flags    = uint8_t (res.asUInt64(r, 2).value_or(0));
        p.velocity = float(res.asDouble(r, 3).value_or(0.0));
        p.comment  = QString::fromStdString(res.cell(r, 4));
        ids.push_back(p.pathId);
        m_committedRows.push_back(std::move(p));
    }
    if (m_committedRows.empty()) return true;

    std::string in_list;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (i) in_list += ',';
        in_list += std::to_string(ids[i]);
    }
    std::string nodeSql =
        "SELECT PathId, NodeId, PositionX, PositionY, PositionZ, "
        "       COALESCE(Orientation, 0), Delay "
        "FROM waypoint_path_node WHERE PathId IN (" + in_list + ") "
        "ORDER BY PathId, NodeId";
    db::QueryResult nodes;
    err = m_dbClient->query(nodeSql, nodes);
    if (!err.ok()) { outError = QString::fromStdString(err.message); return false; }
    size_t cursor = 0;
    for (size_t r = 0; r < nodes.rowCount(); ++r)
    {
        uint32_t const pid = uint32_t(nodes.asUInt64(r, 0).value_or(0));
        while (cursor < m_committedRows.size() && m_committedRows[cursor].pathId != pid)
            ++cursor;
        if (cursor >= m_committedRows.size()) break;
        render::PathNode n;
        n.nodeId      = int(nodes.asUInt64(r, 1).value_or(0));
        n.x           = float(nodes.asDouble(r, 2).value_or(0.0));
        n.y           = float(nodes.asDouble(r, 3).value_or(0.0));
        n.z           = float(nodes.asDouble(r, 4).value_or(0.0));
        n.orientation = float(nodes.asDouble(r, 5).value_or(0.0));
        n.delay       = uint32_t(nodes.asUInt64(r, 6).value_or(0));
        m_committedRows[cursor].nodes.push_back(std::move(n));
    }
    return true;
}

void WaypointCommitDialog::onCommitClicked()
{
    m_commitButton->setEnabled(false);
    m_statusLabel->setText(tr("preparing..."));
    QApplication::processEvents();

    if (m_backupCheckbox->isChecked())
    {
        bool const hasBackupable = std::any_of(
            m_model.changes().begin(), m_model.changes().end(),
            [](db::PathChangeRecord const& c) {
                return c.kind == db::PathChangeKind::Update
                    || c.kind == db::PathChangeKind::Delete;
            });
        if (hasBackupable)
        {
            QString backupPath, backupErr;
            if (!writeBackupFile(buildBackupSql(), backupPath, backupErr))
            {
                QMessageBox::critical(this, tr("Backup failed"),
                    tr("Aborting commit:\n%1").arg(backupErr));
                m_statusLabel->setText(tr("aborted"));
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
            tr("Transaction rolled back:\n%1").arg(applyErr));
        m_statusLabel->setText(tr("failed: %1").arg(applyErr));
        m_commitButton->setEnabled(true);
        return;
    }
    m_statusLabel->setText(tr("committed"));
    accept();
}

} // namespace world_editor::app
