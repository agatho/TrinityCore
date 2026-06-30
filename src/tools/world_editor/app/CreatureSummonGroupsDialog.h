/*
 * CreatureSummonGroupsDialog - modal editor for `creature_summon_groups`.
 *
 * creature_summon_groups defines named groups of summons that a creature
 * or script can spawn.  Consumed by SAI / SmartScript SUMMON_CREATURE_GROUP
 * actions and by creature_template_summon.
 *
 * Modern TC schema (probed via INFORMATION_SCHEMA.COLUMNS):
 *
 *   creature_summon_groups(summonerId    INT UNSIGNED,
 *                          summonerType  TINYINT UNSIGNED,  -- 0/1/2
 *                          groupId       TINYINT UNSIGNED,
 *                          entry         INT UNSIGNED,
 *                          position_x    FLOAT,
 *                          position_y    FLOAT,
 *                          position_z    FLOAT,
 *                          orientation   FLOAT,
 *                          summonType    TINYINT UNSIGNED,
 *                          summonTime    INT UNSIGNED)
 *
 *   Composite PK = (summonerId, summonerType, groupId, entry, position_x).
 *
 * summonerType encoding (matches src/server/game/Entities/Object/Object.h
 * SummonerType enum):
 *   0 = CREATURE_SUMMON_GROUP            (summonerId = creature_template.entry
 *                                         or creature.guid for templated)
 *   1 = CREATURE_SUMMON_GROUP_BY_GUID    (summonerId = creature.guid)
 *   2 = GAMEOBJECT_SUMMON_GROUP          (summonerId = gameobject_template.entry)
 *
 * Layout:
 *   - Top filter row: QSpinBox summonerId + QSpinBox summonerType +
 *     QSpinBox groupId + Load button.  When all three are zero the table
 *     shows the first 5000 rows ORDER BY summonerId, summonerType, groupId,
 *     entry.  Any non-zero filter narrows the WHERE clause.
 *   - Main: QTableWidget over (summonerId, summonerType, groupId, entry,
 *     position_x/y/z, orientation, summonType, summonTime).
 *   - Action row: Add summon... / Edit summon / Remove summon /
 *     Lookup summon entry / Jump to position (with operator-supplied MapID).
 *
 * Add/Edit modal carries every column; on Edit the five composite-PK
 * columns are disabled (re-key requires Remove + Add).
 *
 * Jump to position emits jumpRequested(mapId, position_x, position_y).
 * The schema carries no map context so MainWindow forwards whatever MapID
 * the operator sets in the dialog's MapID spinbox (default 0).
 *
 * All DML wraps START TRANSACTION / COMMIT / ROLLBACK so a failure mid-
 * statement never leaves the table in a half-written state.
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

namespace world_editor::app
{

class CreatureSummonGroupsDialog final : public QDialog
{
    Q_OBJECT

public:
    CreatureSummonGroupsDialog(db::MySqlClient* dbClient,
                               QString const& worldDbName,
                               QWidget* parent = nullptr);

signals:
    // Operator clicked "Jump to position".  MainWindow forwards this to
    // onJumpRequested(mapId, x, y, std::nullopt) - mapId comes from the
    // dialog's operator-supplied spinbox since the schema does not carry one.
    void jumpRequested(uint32_t mapId, float worldX, float worldY);

private slots:
    void onLoad();
    void onAdd();
    void onEdit();
    void onRemove();
    void onLookupEntry();
    void onJump();
    void onSelectionChanged();

private:
    // Reload table for current filter (summonerId/summonerType/groupId).
    void loadRows();

    // Returns the composite PK tuple stored on the selected row.  Surfaces
    // the row index too so callers can read mutable cells directly from
    // the rendered table without re-querying MySQL.
    bool currentRowPk(uint32_t& summonerIdOut, int& summonerTypeOut,
                      int& groupIdOut, uint32_t& entryOut,
                      double& posXOut, int& rowOut) const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // Rolls back and surfaces a QMessageBox on any error path.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal.  isEdit pre-populates from the selected row
    // and emits an UPDATE; otherwise emits an INSERT.
    void openModal(bool isEdit);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;

    QSpinBox*        m_filterSummonerId   = nullptr;
    QSpinBox*        m_filterSummonerType = nullptr;
    QSpinBox*        m_filterGroupId      = nullptr;
    QSpinBox*        m_jumpMapSpin        = nullptr;
    QPushButton*     m_loadBtn            = nullptr;
    QTableWidget*    m_table              = nullptr;
    QPushButton*     m_addBtn             = nullptr;
    QPushButton*     m_editBtn            = nullptr;
    QPushButton*     m_removeBtn          = nullptr;
    QPushButton*     m_lookupBtn          = nullptr;
    QPushButton*     m_jumpBtn            = nullptr;
    QLabel*          m_statusLabel        = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
