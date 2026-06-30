/*
 * CommitDialog - diff preview + transactional write for annotation edits.
 *
 * Per HANDOFF_NATIVE_EDITOR.md section 10.2 (explicit Commit with diff
 * preview) and 10.8 (per-edit SQL files for git review): on commit we
 *   1. Write a backup .sql file (before-images of updated + deleted rows
 *      plus the original ids of inserted rows) to editor_backups/.
 *   2. Open a transaction.
 *   3. Apply INSERT / UPDATE / DELETE statements in order.
 *   4. COMMIT on success or ROLLBACK on any error.
 *   5. Hand the committed row list back to MainWindow so it can refresh
 *      the viewer.
 */

#pragma once

#include "../db/AnnotationModel.h"
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

class CommitDialog final : public QDialog
{
    Q_OBJECT

public:
    CommitDialog(db::MySqlClient* dbClient,
                 db::AnnotationModel const& model,
                 uint32_t mapId,
                 QWidget* parent = nullptr);

    // Available only after exec() returns Accepted - the post-commit
    // state (re-queried from DB) to push into the viewer.
    [[nodiscard]] std::vector<render::Annotation> const& committedRows() const noexcept
    { return m_committedRows; }

private slots:
    void onCommitClicked();

private:
    [[nodiscard]] QString buildSqlPreview() const;
    [[nodiscard]] QString buildBackupSql()  const;
    [[nodiscard]] bool    writeBackupFile(QString const& sql, QString& outPath, QString& outError) const;
    [[nodiscard]] bool    applyTransaction(QString& outError);

    db::MySqlClient*           m_dbClient = nullptr;
    db::AnnotationModel const& m_model;
    uint32_t                   m_mapId    = 0;

    QPlainTextEdit*            m_sqlPreview     = nullptr;
    QCheckBox*                 m_backupCheckbox = nullptr;
    QLabel*                    m_statusLabel    = nullptr;
    QPushButton*               m_commitButton   = nullptr;

    std::vector<render::Annotation> m_committedRows;
};

} // namespace world_editor::app
