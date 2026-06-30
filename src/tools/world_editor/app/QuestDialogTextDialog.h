/*
 * QuestDialogTextDialog - unified editor for the three quest-text aux tables:
 *
 *   quest_offer_reward(ID, Emote1..4, EmoteDelay1..4, RewardText, VerifiedBuild)
 *       Text + emotes the questgiver plays on completion turn-in.
 *
 *   quest_request_items(ID, EmoteOnComplete, EmoteOnIncomplete,
 *                       EmoteOnCompleteDelay, EmoteOnIncompleteDelay,
 *                       CompletionText, VerifiedBuild)
 *       Text + 2 emotes shown while the quest is in progress.
 *
 *   quest_details(ID, Emote1..4, EmoteDelay1..4, VerifiedBuild)
 *       Emotes only - text is in quest_template.QuestDescription.  Shown when
 *       the player accepts/views the quest at pickup.
 *
 * ID matches quest_template.ID.  Schemas drift between forks (some carry
 * fewer emote slots / older _wdb suffixes / etc.) so on first load we probe
 * INFORMATION_SCHEMA.COLUMNS for each table and adapt the INSERT/UPDATE
 * column lists.  Missing optional columns are silently dropped from DML.
 *
 * Each of the three tabs has its own Save button performing a single-row
 * UPSERT (INSERT if absent, UPDATE if present) wrapped in
 * START TRANSACTION / COMMIT / ROLLBACK so a failure mid-statement never
 * leaves the table in a half-written state.
 *
 * Layout:
 *   - Top row: Quest ID spinbox + Load button + read-only "<ID> - <Title>"
 *     label (probed from quest_template.LogTitle or .Title, schema-tolerant).
 *   - QTabWidget:
 *       Offer reward    -> RewardText QPlainTextEdit + 4x (Emote / EmoteDelay)
 *                          + Save UPSERT.
 *       Request items   -> CompletionText QPlainTextEdit + 2x (Emote / Delay)
 *                          for OnComplete / OnIncomplete + Save UPSERT.
 *       Details (emotes-only)
 *                       -> 4x (Emote / EmoteDelay) + Save UPSERT.
 */

#pragma once

#include <QDialog>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class QuestDialogTextDialog final : public QDialog
{
    Q_OBJECT

public:
    QuestDialogTextDialog(db::MySqlClient* dbClient,
                          QString const& worldDbName,
                          QWidget* parent = nullptr);

private slots:
    void onLoad();
    void onSaveOfferReward();
    void onSaveRequestItems();
    void onSaveDetails();

private:
    // One-shot probe of INFORMATION_SCHEMA.COLUMNS for one table.  Records
    // the lowercased column-name set so DML generation can skip columns the
    // live schema lacks.  Idempotent - subsequent calls re-probe on demand.
    void detectSchemaFor(QString const& table, QSet<QString>& outCols);

    bool hasCol(QSet<QString> const& cols, char const* name) const;

    // quest_template.LogTitle (modern) / .Title (legacy) lookup.  Empty
    // string means the quest ID is unknown to quest_template.
    QString resolveQuestTitle(uint32_t questId);

    // Re-runs the three tab loads + the title probe.
    void loadAll();

    // Helpers that re-populate one tab's widgets from a single SELECT.  Each
    // returns true when a row was found (and sets `outFound`), false when
    // the table has no row for the loaded quest (Save will INSERT in that
    // case).  Sets every widget to zero / empty when no row exists.
    void loadOfferRewardTab();
    void loadRequestItemsTab();
    void loadDetailsTab();

    // Wrap `sqls` in START TRANSACTION / COMMIT, ROLLBACK + QMessageBox on
    // the first error.  Returns true on COMMIT.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;

    // Per-table schema column sets, lowercased.  Probed lazily on Load.
    QSet<QString>    m_colsOffer;
    QSet<QString>    m_colsRequest;
    QSet<QString>    m_colsDetails;
    bool             m_schemaDetected = false;

    // Cached existence flags for the currently-loaded quest ID across each
    // tab.  Drives the INSERT vs UPDATE branch in the per-tab Save UPSERT.
    uint32_t         m_loadedQuestId  = 0;
    bool             m_offerExists    = false;
    bool             m_requestExists  = false;
    bool             m_detailsExists  = false;

    // -- Top row -----------------------------------------------------------
    QSpinBox*    m_questIdSpin = nullptr;
    QPushButton* m_loadBtn     = nullptr;
    QLabel*      m_titleLabel  = nullptr;

    QTabWidget*  m_tabs        = nullptr;

    // -- Offer reward tab --------------------------------------------------
    QPlainTextEdit* m_offerText        = nullptr;
    QSpinBox*       m_offerEmote[4]    = { nullptr, nullptr, nullptr, nullptr };
    QSpinBox*       m_offerDelay[4]    = { nullptr, nullptr, nullptr, nullptr };
    QPushButton*    m_offerSaveBtn     = nullptr;
    QLabel*         m_offerStatus      = nullptr;

    // -- Request items tab -------------------------------------------------
    QPlainTextEdit* m_requestText      = nullptr;
    QSpinBox*       m_requestEmoteOnComplete   = nullptr;
    QSpinBox*       m_requestEmoteOnIncomplete = nullptr;
    QSpinBox*       m_requestDelayOnComplete   = nullptr;
    QSpinBox*       m_requestDelayOnIncomplete = nullptr;
    QPushButton*    m_requestSaveBtn   = nullptr;
    QLabel*         m_requestStatus    = nullptr;

    // -- Details tab -------------------------------------------------------
    QSpinBox*       m_detailsEmote[4]  = { nullptr, nullptr, nullptr, nullptr };
    QSpinBox*       m_detailsDelay[4]  = { nullptr, nullptr, nullptr, nullptr };
    QPushButton*    m_detailsSaveBtn   = nullptr;
    QLabel*         m_detailsStatus    = nullptr;

    QLabel*         m_statusLabel      = nullptr;
};

} // namespace world_editor::app
