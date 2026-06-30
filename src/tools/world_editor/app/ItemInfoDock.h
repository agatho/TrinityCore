/*
 * ItemInfoDock - read-only panel showing the full `item_template` row
 * for a single item id.
 *
 * Trigger path: LootTableDock and VendorInventoryDock emit itemSelected(id)
 * when the operator double-clicks a row that carries an item id; the
 * MainWindow forwards that id here.
 *
 * Modern TC has been migrating item data to hotfix DB2s (Item.db2 +
 * ItemSparse.db2 + ItemEffect.db2 + ItemSearchName.db2 ...) but the
 * legacy `item_template` mirror is still present in most world DBs.
 * We query item_template first; on missing-table (MySQL 1146) we
 * surface a fallback "(item_template not present; hotfix DB not
 * linked into editor)" message rather than asserting any single
 * schema layout.
 *
 * Per-section queries swallow MySQL 1054 (missing column) errors so
 * the dock doesn't break when an unknown TC build renames or drops
 * a column - the affected cluster simply doesn't render, the rest
 * of the dock still works.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QTableWidget;
class QVBoxLayout;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class ItemInfoDock final : public QWidget
{
    Q_OBJECT

public:
    explicit ItemInfoDock(db::MySqlClient* dbClient,
                          QWidget* parent = nullptr);

    // Look up `itemId` and render its summary.  id=0 clears.
    void setItem(uint32_t itemId);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Run a SELECT for the named cluster.  On MySQL 1054 (missing column)
    // the row is rendered as "(schema mismatch)" and the section label is
    // hidden so unknown TC builds don't break the dock.  On 1146 (table
    // missing) the caller has already short-circuited by the time these
    // are invoked.  Returns true if rendered, false otherwise.
    bool populateIdentity   (uint32_t itemId);
    bool populateLevels     (uint32_t itemId);
    bool populateStats      (uint32_t itemId);
    bool populateDamage     (uint32_t itemId);
    bool populateResistances(uint32_t itemId);
    bool populatePrice      (uint32_t itemId);
    bool populateBag        (uint32_t itemId);
    bool populateQuestUse   (uint32_t itemId);

    db::MySqlClient* m_db;
    QLabel*          m_header        = nullptr;
    QLabel*          m_nameLabel     = nullptr;    // quality-tinted header.
    QLabel*          m_identity      = nullptr;
    QLabel*          m_levels        = nullptr;
    QLabel*          m_statsHeader   = nullptr;
    QTableWidget*    m_statsTable    = nullptr;
    QLabel*          m_damage        = nullptr;
    QLabel*          m_resistances   = nullptr;
    QLabel*          m_price         = nullptr;
    QLabel*          m_bag           = nullptr;
    QLabel*          m_questUse      = nullptr;
};

} // namespace world_editor::app
