/*
 * SmartScriptCommitDialog - diff preview + transactional write for
 * `smart_scripts` row edits.  Mirrors AreatriggerCommitDialog but
 * targets the composite-PK `smart_scripts` table.
 *
 * Scope: UPDATE + DELETE + INSERT.  Backup .sql files are written
 * under editor_backups/<ts>_smart_scripts.sql so the operator can
 * roll back manually if needed.
 *
 * Validation:
 *   - source_type=0 (creature):    entryorguid > 0 must exist in
 *     creature_template.entry, OR entryorguid < 0 must reference a
 *     real creature.guid (per-spawn SAI override convention).
 *   - source_type=1 (gameobject):  same against gameobject_template /
 *     gameobject.
 *   - source_type=9 (action list): no FK to validate.
 *
 * After commit, this dialog re-fetches the rows for every
 * (entryorguid, source_type) "scope" touched by the changeset, so
 * the caller can swap its baseline for the post-commit truth.
 */

#pragma once

#include "../db/MySqlClient.h"
#include "../db/SmartScriptModel.h"
#include "../render/NavMeshView.h"

#include <QDialog>

#include <utility>
#include <vector>

class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QLabel;

namespace world_editor::app
{

class SmartScriptCommitDialog final : public QDialog
{
    Q_OBJECT

public:
    SmartScriptCommitDialog(db::MySqlClient* dbClient,
                            db::SmartScriptModel const& model,
                            QWidget* parent = nullptr);

    // Post-commit refetched rows, scoped to every (entryorguid,
    // source_type) pair that appeared in the changeset.  Empty until
    // applyTransaction succeeds.
    [[nodiscard]] std::vector<render::SmartScript> const& committedRows() const noexcept
    { return m_committedRows; }

    // The (entryorguid, source_type) pairs that were refetched.
    // Caller can use these to merge committedRows back into a larger
    // unrelated baseline.
    [[nodiscard]] std::vector<std::pair<int64_t, uint8_t>> const& refetchedScopes() const noexcept
    { return m_refetchedScopes; }

private slots:
    void onCommitClicked();

private:
    [[nodiscard]] QString buildSqlPreview() const;
    [[nodiscard]] QString buildBackupSql()  const;
    [[nodiscard]] bool    writeBackupFile(QString const& sql, QString& outPath, QString& outError) const;
    [[nodiscard]] bool    applyTransaction(QString& outError);
    [[nodiscard]] bool    refetchRows(QString& outError);
    [[nodiscard]] bool    validateChanges(QString& outError) const;

    db::MySqlClient*               m_dbClient = nullptr;
    db::SmartScriptModel const&    m_model;

    QPlainTextEdit*                m_sqlPreview     = nullptr;
    QCheckBox*                     m_backupCheckbox = nullptr;
    QLabel*                        m_statusLabel    = nullptr;
    QPushButton*                   m_commitButton   = nullptr;

    std::vector<render::SmartScript>          m_committedRows;
    std::vector<std::pair<int64_t, uint8_t>>  m_refetchedScopes;
};

} // namespace world_editor::app
