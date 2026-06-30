/*
 * GameEventEditDialog - modal editor for the `game_event` table plus the
 * two membership-link tables (`game_event_creature`, `game_event_gameobject`).
 *
 * Two-pane layout:
 *   Left  - list of every game_event row (eventEntry + holiday + description).
 *   Right - QTabWidget:
 *             1. Properties  : per-column editors for the selected event row
 *                              (start/end timestamps, occurence, length,
 *                              holiday, holidayStage, description, world_event,
 *                              announce).
 *             2. Creatures   : SELECT g.guid, g.eventEntry, c.id1, c.map FROM
 *                              game_event_creature g LEFT JOIN creature c ON
 *                              c.guid = g.guid WHERE g.eventEntry = ?
 *             3. GameObjects : same shape against game_event_gameobject /
 *                              gameobject.
 *
 * All DML is wrapped in START TRANSACTION / COMMIT, with ROLLBACK on any
 * error path.  The dialog is non-modal so the operator can hop back to the
 * main viewer to cross-reference creature/GO guids while editing.
 *
 * Schema notes:
 *   game_event.eventEntry            : tinyint UNSIGNED  (0..255)
 *   game_event_creature.eventEntry   : tinyint  SIGNED   (-128..127, negative
 *                                                         removes-during-event)
 *   game_event_gameobject.eventEntry : tinyint  SIGNED
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>

class QTabWidget;
class QTableWidget;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QPlainTextEdit;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class GameEventEditDialog final : public QDialog
{
    Q_OBJECT

public:
    GameEventEditDialog(db::MySqlClient* dbClient,
                        QString const& worldDbName,
                        QWidget* parent = nullptr);

private slots:
    void onEventRowChanged();
    void onNewEvent();
    void onDeleteEvent();
    void onSaveProperties();
    void onAddCreatureLink();
    void onAddGameObjectLink();
    void onRemoveCreatureLink();
    void onRemoveGameObjectLink();

private:
    // Populate the left-pane event list from `game_event`.  Reselects the
    // previously-active eventEntry if it still exists.
    void loadEvents(int preserveEntry = -1);

    // Populate the right-pane properties + creature/GO tabs for the
    // currently selected event row.  No-op if nothing is selected.
    void loadPropertiesForSelected();
    void loadCreatureLinks(int eventEntry);
    void loadGameObjectLinks(int eventEntry);

    // Returns the eventEntry of the currently selected row, or -1.
    [[nodiscard]] int selectedEventEntry() const;

    // Run `sql` inside an explicit START TRANSACTION / COMMIT.  Rolls back +
    // surfaces a QMessageBox on any error.  Returns true on commit.
    bool runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut = nullptr);

    db::MySqlClient* m_db        = nullptr;
    QString          m_worldDb;

    // Left pane.
    QTableWidget*    m_eventTable  = nullptr;
    QPushButton*     m_newBtn      = nullptr;
    QPushButton*     m_deleteBtn   = nullptr;

    // Right pane.
    QTabWidget*      m_tabs        = nullptr;
    QTableWidget*    m_creatureTable    = nullptr;
    QTableWidget*    m_gameObjectTable  = nullptr;
    QPushButton*     m_addCreatureBtn   = nullptr;
    QPushButton*     m_rmCreatureBtn    = nullptr;
    QPushButton*     m_addGoBtn         = nullptr;
    QPushButton*     m_rmGoBtn          = nullptr;

    // Properties editors - one row per game_event column.  start_time and
    // end_time use QLineEdit (TIMESTAMP, NULL-able, free-form text) so the
    // operator can clear them to NULL by leaving the field empty.
    QLineEdit*       m_startTimeEdit    = nullptr;
    QLineEdit*       m_endTimeEdit      = nullptr;
    QSpinBox*        m_occurenceSpin    = nullptr;  // minutes (bigint, clamped to int max)
    QSpinBox*        m_lengthSpin       = nullptr;
    QSpinBox*        m_holidaySpin      = nullptr;
    QSpinBox*        m_holidayStageSpin = nullptr;
    QPlainTextEdit*  m_descriptionEdit  = nullptr;
    QSpinBox*        m_worldEventSpin   = nullptr;
    QSpinBox*        m_announceSpin     = nullptr;
    QPushButton*     m_saveBtn          = nullptr;

    QLabel*          m_statusLabel      = nullptr;
};

} // namespace world_editor::app
