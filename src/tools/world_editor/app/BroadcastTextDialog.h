/*
 * BroadcastTextDialog - modal editor for `broadcast_text`.
 *
 * Schema (modern TC):
 *
 *   broadcast_text(ID            INT UNSIGNED PRIMARY KEY,
 *                  LanguageID    INT,
 *                  MaleText      TEXT,
 *                  FemaleText    TEXT,
 *                  EmoteID1      SMALLINT UNSIGNED,
 *                  EmoteID2      SMALLINT UNSIGNED,
 *                  EmoteID3      SMALLINT UNSIGNED,
 *                  EmoteDelay1   INT UNSIGNED,
 *                  EmoteDelay2   INT UNSIGNED,
 *                  EmoteDelay3   INT UNSIGNED,
 *                  SoundEntriesID1 INT UNSIGNED,
 *                  SoundEntriesID2 INT UNSIGNED,
 *                  EmotesID      INT UNSIGNED,
 *                  Flags         INT UNSIGNED,
 *                  ConditionID   INT UNSIGNED,
 *                  VerifiedBuild INT)
 *
 * This is TC's central NPC speech repository.  creature_text.BroadcastTextId,
 * gossip_menu_option.OptionBroadcastTextID, quest_offer_reward (etc.) all
 * point at IDs here, so editing one row affects every reference site.
 *
 * Schema-tolerant: we probe INFORMATION_SCHEMA.COLUMNS up-front to detect
 * which of the schema's optional columns (e.g. `MaleText` vs the older
 * `Text_0` / `MaleText_lang0`) are present and adapt the SELECT / INSERT /
 * UPDATE column lists accordingly.  Missing columns are silently dropped
 * from generated DML.
 */

#pragma once

#include <QDialog>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class BroadcastTextDialog final : public QDialog
{
    Q_OBJECT

public:
    BroadcastTextDialog(db::MySqlClient* dbClient,
                        QString const& worldDbName,
                        QWidget* parent = nullptr);

private slots:
    void onRefresh();
    void onAdd();
    void onEdit();
    void onRemove();
    void onShowReferences();
    void onSelectionChanged();

private:
    // One-shot probe of INFORMATION_SCHEMA.COLUMNS that records every
    // column the live `broadcast_text` schema actually exposes.  Generated
    // DML filters its column list through `hasCol` so missing optional
    // fields are silently skipped.
    void detectSchemaColumns();

    // `colName` ignoring case against the detected schema.
    bool hasCol(QString const& colName) const;

    // Re-runs the SELECT against the current search filter and repopulates
    // the table.
    void loadRows();

    // Returns the broadcast_text.ID of the selected row, or false if no
    // selection.
    bool currentRowId(uint32_t& idOut, int& rowOut) const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // Identical contract to the sibling dialogs.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Add/Edit modal.  When `editing` is true, ID is locked and the form
    // is pre-populated from the selected row.  Otherwise INSERT path runs
    // with a default ID = MAX(ID)+1 the modal lets the operator override.
    void openModal(bool editing, uint32_t editingId);

    // Reference scan helper.  Runs SELECT COUNT against every well-known
    // referencing table that we recognise on the current schema and
    // returns a (table, column, count) triple list for the supplied ID.
    struct RefHit { QString table; QString column; uint64_t count; };
    QList<RefHit> scanReferences(uint32_t broadcastId);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;
    bool             m_schemaDetected = false;
    QSet<QString>    m_cols;                  // Lowercased actual column set.

    QLineEdit*       m_searchEdit  = nullptr;
    QPushButton*     m_refreshBtn  = nullptr;
    QTableWidget*    m_table       = nullptr;
    QPushButton*     m_addBtn      = nullptr;
    QPushButton*     m_editBtn     = nullptr;
    QPushButton*     m_removeBtn   = nullptr;
    QPushButton*     m_refsBtn     = nullptr;
    QLabel*          m_statusLabel = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
