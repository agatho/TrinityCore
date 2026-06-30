/*
 * AccessRequirementDialog - modal editor for `access_requirement`.
 *
 * access_requirement gates entry to instances/raids by minimum level,
 * required items (typically keys), required prerequisite quests (alliance +
 * horde branches), required achievement, and a custom failure message.
 * Keyed by (mapId, difficulty) composite PK so the same map can carry
 * different gates per Normal/Heroic/Mythic/etc.
 *
 * Canonical schema:
 *   access_requirement(mapId                 INT UNSIGNED,
 *                      difficulty            TINYINT UNSIGNED,
 *                      level_min             TINYINT UNSIGNED,
 *                      level_max             TINYINT UNSIGNED,
 *                      item                  INT UNSIGNED,
 *                      item2                 INT UNSIGNED,
 *                      quest_done_A          INT UNSIGNED,
 *                      quest_done_H          INT UNSIGNED,
 *                      completed_achievement INT UNSIGNED,
 *                      quest_failed_text     VARCHAR(255),
 *                      comment               TEXT,
 *                      PRIMARY KEY (mapId, difficulty))
 *
 * Layout mirrors WorldSafeLocsDialog: top filter (mapId + difficulty +
 * Load + show-all-when-zero), main QTableWidget over the SELECT-LIMIT-5000,
 * Add/Edit/Remove + "Lookup item template" action buttons.
 *
 * All DML wraps START TRANSACTION / COMMIT / ROLLBACK.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <cstdint>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }
namespace world_editor { class MainWindow; }

namespace world_editor::app
{

class AccessRequirementDialog final : public QDialog
{
    Q_OBJECT

public:
    AccessRequirementDialog(db::MySqlClient* dbClient,
                            QString const& worldDbName,
                            ::world_editor::MainWindow* mainWindow,
                            QWidget* parent = nullptr);

private slots:
    void onLoadClicked();
    void onAddClicked();
    void onEditClicked();
    void onRemoveClicked();
    void onLookupItemClicked();
    void onSelectionChanged();

private:
    void loadRows();

    // Returns the (mapId, difficulty) PK of the selected row.
    // rowOut surfaces the row index for cell read-back.
    bool currentRowPk(uint32_t& mapIdOut, uint32_t& difficultyOut, int& rowOut) const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // Rolls back and surfaces a QMessageBox on any error path.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal.  When editing == true, the modal pre-populates
    // from the selected row and emits an UPDATE WHERE mapId=? AND difficulty=?;
    // otherwise emits an INSERT and aborts on PK collision.
    void openModal(bool editing);

    db::MySqlClient*             m_db         = nullptr;
    ::world_editor::MainWindow*  m_mainWindow = nullptr;
    QString                      m_worldDb;

    QSpinBox*        m_mapFilter        = nullptr;
    QSpinBox*        m_difficultyFilter = nullptr;
    QPushButton*     m_loadBtn          = nullptr;
    QTableWidget*    m_table            = nullptr;
    QPushButton*     m_addBtn           = nullptr;
    QPushButton*     m_editBtn          = nullptr;
    QPushButton*     m_removeBtn        = nullptr;
    QPushButton*     m_lookupItemBtn    = nullptr;
    QLabel*          m_statusLabel      = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
