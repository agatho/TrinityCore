/*
 * SpellInfoDock - read-only panel showing core attributes of a spell id.
 *
 * Modern TC keeps the canonical spell data in DB2 / hotfix files
 * (Spell.db2, SpellEffect.db2, SpellMisc.db2, ...) which the world DB
 * does NOT carry.  Some forks / older snapshots, however, ship mirror
 * tables in the world DB (`spell_dbc`, legacy `spell_template`, or
 * TC's own `serverside_spell`).  This dock probes those tables in
 * order and renders whichever schema first returns a row for the
 * queried spell id; if none of them carries the id we surface a
 * "no spell info table" message rather than asserting a single layout.
 *
 * Trigger path: the SmartScriptFlowDialog emits spellReferenced(id)
 * whenever the operator selects a rule whose action references a
 * spell (SMART_ACTION_CAST=11, SMART_ACTION_REMOVE_AURASFROMSPELL=15,
 * SMART_ACTION_ADD_AURA, ...).  MainWindow forwards that id here.
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

class SpellInfoDock final : public QWidget
{
    Q_OBJECT

public:
    explicit SpellInfoDock(db::MySqlClient* dbClient,
                           QWidget* parent = nullptr);

    // Look up `spellId` and render its summary + effects.  id=0 clears.
    void setSpell(uint32_t spellId);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Try a SELECT against `table` projecting the union of common
    // spell-info columns; returns true if the row was found AND the
    // dock has been populated.  Returns false on no-row OR missing
    // table (1146) / missing column (1054) so the caller can fall
    // through to the next probe.  `outNoteMissing` carries a "(table
    // missing in this schema)" annotation for the header when both
    // 1146 and 1054 fire.
    bool tryPopulateFromTable(uint32_t spellId,
                              char const* table,
                              QString& outNoteMissing);
    // Look up SpellEffect-style rows from a sibling mirror table.
    // Same MySQL 1146/1054 fall-through contract.
    void tryPopulateEffects(uint32_t spellId, char const* effectTable);

    db::MySqlClient* m_db;
    QLabel*          m_header        = nullptr;
    QLabel*          m_summary       = nullptr;   // ID/name/school/timing block.
    QTableWidget*    m_effects       = nullptr;   // per-effect rows.
    QLabel*          m_effectsHeader = nullptr;
};

} // namespace world_editor::app
