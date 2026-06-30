/*
 * SpawnSearchDialog - multi-criteria spawn finder.
 *
 * Form-driven query against creature / gameobject + their templates +
 * (optional) loot tables.  Any field left blank is dropped from the
 * WHERE clause; remaining clauses are AND-joined.  Supports:
 *   - name fragment (LIKE %x% against creature_template.name or
 *     gameobject_template.name)
 *   - exact entry# (creature.id / gameobject.id)
 *   - npcflag bit (creature_template.npcflag & bit != 0)
 *   - faction (creature.faction)
 *   - map id (creature.map / gameobject.map; 0 means any)
 *   - drops item-id (matches creature_loot_template.item OR
 *     gameobject_loot_template.item joined through tpl.lootid)
 *   - kind (creature only / GO only / both)
 *
 * Double-clicking a result row (or pressing the Jump button) emits
 * jumpRequested(mapId, x, y, optional<guid>) so MainWindow's existing
 * onJumpRequested slot can pan the viewer.
 *
 * Triggered via Tools -> Spawn search... (Ctrl+Shift+F).
 */

#pragma once

#include "../db/MySqlClient.h"

#include <QDialog>

#include <cstdint>
#include <optional>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QTableWidget;
class QLabel;
class QPushButton;

namespace world_editor::app
{

class SpawnSearchDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SpawnSearchDialog(db::MySqlClient* dbClient,
                               uint32_t          currentMapId,
                               QWidget*          parent = nullptr);

signals:
    void jumpRequested(uint32_t mapId, float worldX, float worldY,
                       std::optional<int64_t> guid);

private slots:
    void onSearch();
    void onJumpSelected();

private:
    void emitJumpFromRow(int row);

    db::MySqlClient* m_dbClient    = nullptr;
    uint32_t         m_currentMapId = 0;

    QLineEdit*    m_nameEdit       = nullptr;
    QSpinBox*     m_entrySpin      = nullptr;     // 0 => skip
    QComboBox*    m_npcFlagCombo   = nullptr;     // 0 => skip
    QSpinBox*     m_factionSpin    = nullptr;     // 0 => skip
    QSpinBox*     m_mapSpin        = nullptr;     // 0 => any map
    QSpinBox*     m_itemSpin       = nullptr;     // 0 => skip
    QComboBox*    m_kindCombo      = nullptr;     // creature / go / both

    QPushButton*  m_searchBtn      = nullptr;
    QPushButton*  m_jumpBtn        = nullptr;
    QTableWidget* m_results        = nullptr;
    QLabel*       m_statusLbl      = nullptr;
};

} // namespace world_editor::app
