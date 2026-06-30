/*
 * GraveyardCommitDialog - diff preview + transactional write for
 * `world_safe_locs` row edits.  Mirrors AreatriggerCommitDialog.
 *
 * Backup format: editor_backups/<ts>_map<id>_graveyards.sql with one
 * re-applyable INSERT...ON DUPLICATE KEY UPDATE per before-image.
 */

#pragma once

#include "../db/GraveyardModel.h"
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

class GraveyardCommitDialog final : public QDialog
{
    Q_OBJECT

public:
    GraveyardCommitDialog(db::MySqlClient* dbClient,
                          db::GraveyardModel const& model,
                          uint32_t mapId,
                          QWidget* parent = nullptr);

    [[nodiscard]] std::vector<render::Graveyard> const& committedRows() const noexcept
    { return m_committedRows; }

private slots:
    void onCommitClicked();

private:
    [[nodiscard]] QString buildSqlPreview() const;
    [[nodiscard]] QString buildBackupSql()  const;
    [[nodiscard]] bool    writeBackupFile(QString const& sql, QString& outPath, QString& outError) const;
    [[nodiscard]] bool    applyTransaction(QString& outError);
    [[nodiscard]] bool    refetchRows(QString& outError);

    db::MySqlClient*             m_dbClient = nullptr;
    db::GraveyardModel const&    m_model;
    uint32_t                     m_mapId    = 0;

    QPlainTextEdit*  m_sqlPreview     = nullptr;
    QCheckBox*       m_backupCheckbox = nullptr;
    QLabel*          m_statusLabel    = nullptr;
    QPushButton*     m_commitButton   = nullptr;

    std::vector<render::Graveyard> m_committedRows;
};

} // namespace world_editor::app
