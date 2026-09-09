/*
 * CreatureEquipTemplateDialog - modal editor for `creature_equip_template`.
 *
 * Schema (modern TC):
 *
 *   creature_equip_template(CreatureID INT UNSIGNED,
 *                           ID         TINYINT UNSIGNED,
 *                           ItemID1            INT UNSIGNED,
 *                           AppearanceModID1   SMALLINT UNSIGNED,
 *                           ItemVisual1        SMALLINT UNSIGNED,
 *                           ItemID2            INT UNSIGNED,
 *                           AppearanceModID2   SMALLINT UNSIGNED,
 *                           ItemVisual2        SMALLINT UNSIGNED,
 *                           ItemID3            INT UNSIGNED,
 *                           AppearanceModID3   SMALLINT UNSIGNED,
 *                           ItemVisual3        SMALLINT UNSIGNED,
 *                           VerifiedBuild      INT UNSIGNED)
 *
 * Composite PK = (CreatureID, ID).  A single creature_template can carry
 * multiple equip variants (TC picks one randomly when the creature spawns).
 * Slots map: 1 = main hand, 2 = off hand, 3 = ranged.
 *
 * Schema-tolerant: we probe INFORMATION_SCHEMA.COLUMNS to detect whether
 * the legacy 3.3.5-style `entry` column is in use instead of `CreatureID`
 * and bind that name throughout.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <cstdint>

class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class CreatureEquipTemplateDialog final : public QDialog
{
    Q_OBJECT

public:
    CreatureEquipTemplateDialog(db::MySqlClient* dbClient,
                                QString const& worldDbName,
                                QWidget* parent = nullptr);

private slots:
    void onLoad();
    void onAdd();
    void onEdit();
    void onRemove();
    void onLookupItem();
    void onSelectionChanged();

private:
    // Resolves the creature_template name (name1 first, then name) and
    // updates the top label.  Mirrors the sibling dialogs' defensive
    // fallback.
    void refreshCreatureName(uint32_t entry);

    // Probe INFORMATION_SCHEMA once: settle on `CreatureID` vs `entry`.
    void detectCreatureIdColumn();

    // Reload the equip-set table for the currently-loaded creature.
    void loadEquipSets();

    // Returns the selected ID (the composite-PK secondary key) and row.
    bool currentRowKey(uint32_t& idOut, int& rowOut) const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // ROLLBACKs and surfaces a QMessageBox on any error path.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal.  When `editing` is true, pre-populate from
    // the supplied ID row and UPDATE on Ok; otherwise INSERT.
    void openModal(bool editing, uint32_t editingId);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;
    uint32_t         m_loadedEntry = 0;
    // Resolved at first DB touch.  Defaults to the modern column name.
    QString          m_creatureIdCol = QStringLiteral("CreatureID");
    bool             m_schemaDetected = false;

    QSpinBox*        m_entrySpin   = nullptr;
    QPushButton*     m_loadBtn     = nullptr;
    QLabel*          m_creatureLbl = nullptr;
    QTableWidget*    m_table       = nullptr;
    QPushButton*     m_addBtn      = nullptr;
    QPushButton*     m_editBtn     = nullptr;
    QPushButton*     m_removeBtn   = nullptr;
    QPushButton*     m_lookupBtn   = nullptr;
    QLabel*          m_statusLabel = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
