/*
 * GameObjectInfoDock - read-only panel showing the full `gameobject_template`
 * row for a single GO entry, with per-type decoding of the Data0..Data35
 * vector based on the row's `type` value.
 *
 * Trigger path: MainWindow::onSpawnClicked() forwards the selected
 * spawn's entry here when kind == GameObject; creature selections clear
 * the dock so it doesn't display stale rows.
 *
 * Per-section queries swallow MySQL 1054 (missing column) errors so the
 * dock doesn't break when an unknown TC build renames a column - the
 * affected cluster simply doesn't render, the rest of the dock still
 * works.  A missing `gameobject_template` table (1146) surfaces a
 * fallback header rather than asserting any single schema layout.
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

class GameObjectInfoDock final : public QWidget
{
    Q_OBJECT

public:
    explicit GameObjectInfoDock(db::MySqlClient* dbClient,
                                QWidget* parent = nullptr);

    // Look up `entry` and render its summary.  entry=0 clears.
    void setGameObjectEntry(uint32_t entry);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Run a SELECT for the named cluster.  On MySQL 1054 (missing column)
    // the section is hidden so unknown TC builds don't break the dock.
    bool populateIdentity     (uint32_t entry);
    bool populateTypeSpecific (uint32_t entry);
    bool populateFlagsFaction (uint32_t entry);
    bool populateGold         (uint32_t entry);

    db::MySqlClient* m_db;
    QLabel*          m_header        = nullptr;
    QLabel*          m_nameLabel     = nullptr; // bold header (name + type).
    QLabel*          m_identity      = nullptr;
    QLabel*          m_typeHeader    = nullptr;
    QTableWidget*    m_typeTable     = nullptr;
    QLabel*          m_flags         = nullptr;
    QLabel*          m_gold          = nullptr;
};

} // namespace world_editor::app
