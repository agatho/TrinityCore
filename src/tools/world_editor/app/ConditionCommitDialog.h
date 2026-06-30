/*
 * ConditionCommitDialog - diff preview + transactional write for
 * `conditions` row edits.  Mirrors SmartScriptCommitDialog but targets
 * the 11-column composite-PK `conditions` table.
 *
 * Scope: UPDATE + DELETE + INSERT inside a single transaction with
 * automatic ROLLBACK on the first error.  Backup .sql files written
 * under editor_backups/<ts>_conditions.sql so the operator can roll
 * back manually if needed.
 *
 * Validation: range checks only (ConditionTarget <= 255).  No FK
 * probes -- conditions reference too many tables (creature_template,
 * gameobject_template, gossip_menu, areatrigger_template, ...) and the
 * source-type discriminator decides which one applies on a per-row
 * basis.  The operator owns referential correctness.
 *
 * After commit this dialog refetches every key tuple in the changeset
 * so the caller can swap its baseline for the post-commit truth.
 */

#pragma once

#include "../db/ConditionsModel.h"
#include "../db/MySqlClient.h"
#include "../render/NavMeshView.h"

#include <QDialog>

#include <vector>

class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QLabel;

namespace world_editor::app
{

class ConditionCommitDialog final : public QDialog
{
    Q_OBJECT

public:
    ConditionCommitDialog(db::MySqlClient* dbClient,
                          db::ConditionsModel const& model,
                          QWidget* parent = nullptr);

    // Post-commit refetched rows.  Empty until applyTransaction
    // succeeds.  The caller is expected to merge these back into its
    // model via acceptCommit(committedRows()).
    [[nodiscard]] std::vector<render::Condition> const& committedRows() const noexcept
    { return m_committedRows; }

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
    db::ConditionsModel const&     m_model;

    QPlainTextEdit*                m_sqlPreview     = nullptr;
    QCheckBox*                     m_backupCheckbox = nullptr;
    QLabel*                        m_statusLabel    = nullptr;
    QPushButton*                   m_commitButton   = nullptr;

    std::vector<render::Condition> m_committedRows;
};

} // namespace world_editor::app
