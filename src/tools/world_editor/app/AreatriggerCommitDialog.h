/*
 * AreatriggerCommitDialog - diff preview + transactional write for
 * `areatrigger` row edits.  Mirrors SpawnCommitDialog but targets the
 * `areatrigger` table (Phase 7b).
 *
 * Scope: UPDATE + DELETE + INSERT.  Backup .sql files are written
 * under editor_backups/<ts>_map<id>_areatriggers.sql so the operator
 * can roll back manually if needed.
 *
 * The areatrigger_create_properties row is NEVER written here.  Its
 * Shape/ShapeData* columns are render-only inputs from this dialog's
 * perspective; a single create_properties row is shared by many
 * spawns and editing it would be a much bigger blast radius.
 */

#pragma once

#include "../db/AreatriggerModel.h"
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

class AreatriggerCommitDialog final : public QDialog
{
    Q_OBJECT

public:
    AreatriggerCommitDialog(db::MySqlClient* dbClient,
                            db::AreatriggerModel const& model,
                            uint32_t mapId,
                            QWidget* parent = nullptr);

    [[nodiscard]] std::vector<render::Areatrigger> const& committedRows() const noexcept
    { return m_committedRows; }

private slots:
    void onCommitClicked();

private:
    [[nodiscard]] QString buildSqlPreview() const;
    [[nodiscard]] QString buildBackupSql()  const;
    [[nodiscard]] bool    writeBackupFile(QString const& sql, QString& outPath, QString& outError) const;
    [[nodiscard]] bool    applyTransaction(QString& outError);
    [[nodiscard]] bool    refetchRows(QString& outError);
    // Pre-commit FK validation: every (createPropsId, isCustom) on an
    // Insert/Update row must resolve in areatrigger_create_properties,
    // and every MapId must match m_mapId.
    [[nodiscard]] bool    validateChanges(QString& outError) const;

    db::MySqlClient*               m_dbClient = nullptr;
    db::AreatriggerModel const&    m_model;
    uint32_t                       m_mapId    = 0;

    QPlainTextEdit*                m_sqlPreview     = nullptr;
    QCheckBox*                     m_backupCheckbox = nullptr;
    QLabel*                        m_statusLabel    = nullptr;
    QPushButton*                   m_commitButton   = nullptr;

    std::vector<render::Areatrigger> m_committedRows;
};

} // namespace world_editor::app
