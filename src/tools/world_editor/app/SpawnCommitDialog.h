/*
 * SpawnCommitDialog - diff preview + transactional write for
 * creature + gameobject row edits.  Mirrors CommitDialog (annotations)
 * but targets the world DB tables and the wider schema.
 *
 * 3a scope: UPDATE + DELETE only.  INSERT lands in Phase 3c once GUID
 * reservation is wired (HANDOFF section 10.3).
 *
 * Backup format: a re-applyable .sql file under editor_backups/, one
 * INSERT ... ON DUPLICATE KEY UPDATE per before-image.  Re-running the
 * file restores the row whether or not it still exists in DB.
 */

#pragma once

#include "../db/MySqlClient.h"
#include "../db/SpawnModel.h"
#include "../render/NavMeshView.h"

#include <QDialog>

#include <vector>

class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QLabel;

namespace world_editor::app
{

class SpawnCommitDialog final : public QDialog
{
    Q_OBJECT

public:
    SpawnCommitDialog(db::MySqlClient* dbClient,
                      db::SpawnModel const& model,
                      uint32_t mapId,
                      QWidget* parent = nullptr);

    [[nodiscard]] std::vector<render::Spawn> const& committedRows() const noexcept
    { return m_committedRows; }

private slots:
    void onCommitClicked();

private:
    [[nodiscard]] QString buildSqlPreview() const;
    [[nodiscard]] QString buildBackupSql()  const;
    [[nodiscard]] bool    writeBackupFile(QString const& sql, QString& outPath, QString& outError) const;
    [[nodiscard]] bool    applyTransaction(QString& outError);
    [[nodiscard]] bool    refetchRows(QString& outError);
    // Pre-commit FK validation: every Insert/Update row's `entry` must
    // resolve in creature_template / gameobject_template, and its `map`
    // must match the dialog's mapId.  Returns true if all rows pass.
    [[nodiscard]] bool    validateChanges(QString& outError) const;

    db::MySqlClient*       m_dbClient = nullptr;
    db::SpawnModel const&  m_model;
    uint32_t               m_mapId    = 0;

    QPlainTextEdit*        m_sqlPreview     = nullptr;
    QCheckBox*             m_backupCheckbox = nullptr;
    QLabel*                m_statusLabel    = nullptr;
    QPushButton*           m_commitButton   = nullptr;

    std::vector<render::Spawn> m_committedRows;
};

} // namespace world_editor::app
