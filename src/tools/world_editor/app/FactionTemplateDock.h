/*
 * FactionTemplateDock - read-only panel summarizing a creature's
 * faction template: parent faction, flag bits, the four friend/enemy
 * id slots, and the friendly/enemy group bitmasks.
 *
 * Motivating question: "why is this NPC attacking my bot?".  The
 * faction template encodes both the inherent aggression bits (the
 * Flags column with HATES_ALL_EXCEPT_FRIENDS, etc.) and the matrix
 * relationships via FactionGroup / FriendGroup / EnemyGroup masks
 * plus the 4-slot enemy/friend id arrays.
 *
 * The world DB does NOT carry faction template data on stock TC
 * (it lives in FactionTemplate.db2 / FactionTemplate.dbc).  Some
 * forks ship mirror tables (`faction_template_dbc`, `faction_template`,
 * `serverside_faction_template`); we probe each in order and render
 * whichever first carries the id, mirroring SpellInfoDock /
 * VendorInventoryDock.
 *
 * Trigger path: MainWindow::onSpawnClicked forwards a creature entry;
 * we resolve creature_template.faction (NOT creature.faction — that
 * column does not exist in this schema) and then look up the template.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QVBoxLayout;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class FactionTemplateDock final : public QWidget
{
    Q_OBJECT

public:
    explicit FactionTemplateDock(db::MySqlClient* dbClient,
                                 QWidget* parent = nullptr);

    // Resolve creature_template.faction for `entry` then render the
    // template's relationships.  entry=0 clears.
    void setCreatureEntry(uint32_t entry);
    void clear();

    // Late-bind the DB client; the dock is built before the connection
    // is ready.  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Look up the faction-template row in `table`; return true and
    // populate the form on success.  Returns false on missing row /
    // missing table / missing column so the caller can fall through.
    // Schema-absence notes accumulate in `outNote`.
    bool tryPopulateFromTable(uint32_t factionTemplateId,
                              char const* table,
                              QString& outNote);

    // Best-effort lookup of a parent faction's display name from a
    // `faction_dbc` style mirror.  Empty string when no such table /
    // no such id.
    QString resolveFactionName(uint32_t factionId);

    // Best-effort lookup of a faction-template's friendly label (the
    // parent faction's name, if known).  Used to annotate the four
    // enemy/friend id slots.  Empty string when not resolvable.
    QString resolveFactionTemplateLabel(uint32_t factionTemplateId,
                                        char const* templateTable);

    db::MySqlClient* m_db;
    QLabel*          m_header    = nullptr;
    QLabel*          m_identity  = nullptr;   // template id + parent faction.
    QLabel*          m_flags     = nullptr;   // Flags hex + decoded names.
    QLabel*          m_groups    = nullptr;   // Faction/Friend/Enemy group masks.
    QLabel*          m_relations = nullptr;   // 4 friends + 4 enemies, labelled.
    QLabel*          m_notes     = nullptr;   // schema-absence trail.
};

} // namespace world_editor::app
