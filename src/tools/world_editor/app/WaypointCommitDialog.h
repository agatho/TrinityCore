/*
 * WaypointCommitDialog - diff preview + transactional write for
 * waypoint_path + waypoint_path_node rows.
 *
 * INSERT: full path header + ordered node list.  PathId is pre-reserved
 * (HANDOFF section 10.3 GUID-style approach extended to PathId).
 * UPDATE: rewrites the path's nodes wholesale - DELETE existing nodes
 * for this PathId then re-INSERT.  Simpler than diffing per-node and
 * matches the "paths are rewritten as a unit" mental model.
 * DELETE: removes path header + cascades via DELETE on the node table.
 *
 * Backup: standard INSERT...ON DUPLICATE KEY UPDATE for both tables in
 * editor_backups/<ts>_paths.sql.
 */

#pragma once

#include "../db/MySqlClient.h"
#include "../db/WaypointModel.h"

#include <QDialog>

#include <vector>

class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QLabel;

namespace world_editor::app
{

class WaypointCommitDialog final : public QDialog
{
    Q_OBJECT

public:
    WaypointCommitDialog(db::MySqlClient* dbClient,
                         db::WaypointModel const& model,
                         uint32_t mapId,
                         QWidget* parent = nullptr);

    [[nodiscard]] std::vector<render::Path> const& committedRows() const noexcept
    { return m_committedRows; }

private slots:
    void onCommitClicked();

private:
    [[nodiscard]] QString buildSqlPreview() const;
    [[nodiscard]] QString buildBackupSql()  const;
    [[nodiscard]] bool    writeBackupFile(QString const& sql, QString& outPath, QString& outError) const;
    [[nodiscard]] bool    applyTransaction(QString& outError);
    [[nodiscard]] bool    refetchRows(QString& outError);

    db::MySqlClient*           m_dbClient = nullptr;
    db::WaypointModel const&   m_model;
    uint32_t                   m_mapId    = 0;

    QPlainTextEdit*            m_sqlPreview     = nullptr;
    QCheckBox*                 m_backupCheckbox = nullptr;
    QLabel*                    m_statusLabel    = nullptr;
    QPushButton*               m_commitButton   = nullptr;
    std::vector<render::Path>  m_committedRows;
};

} // namespace world_editor::app
