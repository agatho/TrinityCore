/*
 * CurrencyTypeDock - read-only panel showing the CurrencyType.db2 row
 * referenced by an npc_vendor inventory entry whose `type` column == 2
 * (ItemVendorType::ITEM_VENDOR_TYPE_CURRENCY).
 *
 * Trigger path: VendorInventoryDock emits currencySelected(id) on double
 * click of a row whose type cell renders as "currency".  MainWindow
 * forwards that id here.
 *
 * Modern TC keeps CurrencyType in hotfix DB2 (CurrencyType.db2) which the
 * world DB does NOT carry.  Some forks ship a mirror table; we probe
 * `currency_types_dbc` / `currency_type` / `currencytypes` in priority
 * order and render whichever first carries the id.  No table matched -
 * surface a "(no currency table)" header.
 *
 * Modern CurrencyType columns: ID, Name_lang, Description_lang, MaxQty,
 * MaxEarnablePerWeek, Quality, InventoryIcon (FileDataID), Flags,
 * CategoryID, FactionID.  Older mirrors may carry a subset.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QVBoxLayout;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class CurrencyTypeDock final : public QWidget
{
    Q_OBJECT

public:
    explicit CurrencyTypeDock(db::MySqlClient* dbClient,
                              QWidget* parent = nullptr);

    // Look up `currencyId` and render its summary.  id=0 clears.
    void setCurrency(uint32_t currencyId);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Probe `table` for the row keyed by `currencyId`.  Returns true if
    // the dock was populated.  On 1146 / 1054 we just fall through to
    // the next probe; outNote accumulates per-table miss annotations.
    bool tryPopulateFromTable(uint32_t currencyId,
                              char const* table,
                              QString& outNote);

    db::MySqlClient* m_db;
    QLabel*          m_header      = nullptr;
    QLabel*          m_nameLabel   = nullptr;   // quality-tinted name header.
    QLabel*          m_identity    = nullptr;   // ID / Quality.
    QLabel*          m_caps        = nullptr;   // MaxQty / MaxEarnablePerWeek.
    QLabel*          m_taxonomy    = nullptr;   // CategoryID / FactionID.
    QLabel*          m_description = nullptr;
    QLabel*          m_flags       = nullptr;
};

} // namespace world_editor::app
