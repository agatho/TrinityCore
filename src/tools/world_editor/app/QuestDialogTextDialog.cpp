#include "QuestDialogTextDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Build a (Emote / EmoteDelay) pair group box.  Returns the widgets via
// the outparam pointers so the caller can wire load/save back to them.
QGroupBox* makeEmoteSlot(QWidget* parent, int slotNumber,
                         QSpinBox*& outEmote, QSpinBox*& outDelay,
                         char const* emoteLabel = "Emote",
                         char const* delayLabel = "EmoteDelay")
{
    auto* box  = new QGroupBox(QObject::tr("%1 %2").arg(QString::fromLatin1(emoteLabel)).arg(slotNumber), parent);
    auto* form = new QFormLayout(box);
    outEmote = new QSpinBox(box);
    outDelay = new QSpinBox(box);
    outEmote->setRange(0, std::numeric_limits<int>::max());
    outDelay->setRange(0, std::numeric_limits<int>::max());
    form->addRow(QObject::tr("%1:").arg(QString::fromLatin1(emoteLabel)),  outEmote);
    form->addRow(QObject::tr("%1:").arg(QString::fromLatin1(delayLabel)),  outDelay);
    return box;
}

} // namespace

QuestDialogTextDialog::QuestDialogTextDialog(db::MySqlClient* dbClient,
                                             QString const& worldDbName,
                                             QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Quest dialog text editor"));
    setModal(true);
    resize(900, 720);

    auto* outer = new QVBoxLayout(this);

    // -- Top row: Quest ID + Load + resolved title label ------------------
    auto* topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel(tr("Quest ID:"), this));
    m_questIdSpin = new QSpinBox(this);
    m_questIdSpin->setRange(0, std::numeric_limits<int>::max());
    topRow->addWidget(m_questIdSpin);
    m_loadBtn = new QPushButton(tr("Load"), this);
    topRow->addWidget(m_loadBtn);
    m_titleLabel = new QLabel(tr("(no quest loaded)"), this);
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    topRow->addWidget(m_titleLabel, 1);
    outer->addLayout(topRow);

    m_tabs = new QTabWidget(this);
    outer->addWidget(m_tabs, 1);

    // -- Tab 1: Offer reward ---------------------------------------------
    {
        auto* page  = new QWidget(m_tabs);
        auto* vbox  = new QVBoxLayout(page);

        auto* textBox  = new QGroupBox(tr("RewardText"), page);
        auto* textL    = new QVBoxLayout(textBox);
        m_offerText    = new QPlainTextEdit(textBox);
        m_offerText->setPlaceholderText(tr("Shown when the player turns in this quest."));
        textL->addWidget(m_offerText);
        vbox->addWidget(textBox, 1);

        auto* emoteGrid = new QGridLayout;
        for (int i = 0; i < 4; ++i)
        {
            QGroupBox* slot = makeEmoteSlot(page, i + 1, m_offerEmote[i], m_offerDelay[i]);
            emoteGrid->addWidget(slot, i / 2, i % 2);
        }
        vbox->addLayout(emoteGrid);

        auto* btnRow = new QHBoxLayout;
        m_offerSaveBtn = new QPushButton(tr("Save (UPSERT quest_offer_reward)"), page);
        m_offerSaveBtn->setEnabled(false);
        btnRow->addWidget(m_offerSaveBtn);
        btnRow->addStretch(1);
        vbox->addLayout(btnRow);

        m_offerStatus = new QLabel(tr("Load a quest ID first."), page);
        vbox->addWidget(m_offerStatus);

        m_tabs->addTab(page, tr("Offer reward"));
    }

    // -- Tab 2: Request items --------------------------------------------
    {
        auto* page  = new QWidget(m_tabs);
        auto* vbox  = new QVBoxLayout(page);

        auto* textBox  = new QGroupBox(tr("CompletionText"), page);
        auto* textL    = new QVBoxLayout(textBox);
        m_requestText  = new QPlainTextEdit(textBox);
        m_requestText->setPlaceholderText(tr("Shown while the quest is in progress / not yet complete."));
        textL->addWidget(m_requestText);
        vbox->addWidget(textBox, 1);

        auto* emoteRow = new QHBoxLayout;

        auto* completeBox  = new QGroupBox(tr("OnComplete"), page);
        auto* completeForm = new QFormLayout(completeBox);
        m_requestEmoteOnComplete = new QSpinBox(completeBox);
        m_requestDelayOnComplete = new QSpinBox(completeBox);
        m_requestEmoteOnComplete->setRange(0, std::numeric_limits<int>::max());
        m_requestDelayOnComplete->setRange(0, std::numeric_limits<int>::max());
        completeForm->addRow(tr("EmoteOnComplete:"),      m_requestEmoteOnComplete);
        completeForm->addRow(tr("EmoteOnCompleteDelay:"), m_requestDelayOnComplete);
        emoteRow->addWidget(completeBox);

        auto* incompleteBox  = new QGroupBox(tr("OnIncomplete"), page);
        auto* incompleteForm = new QFormLayout(incompleteBox);
        m_requestEmoteOnIncomplete = new QSpinBox(incompleteBox);
        m_requestDelayOnIncomplete = new QSpinBox(incompleteBox);
        m_requestEmoteOnIncomplete->setRange(0, std::numeric_limits<int>::max());
        m_requestDelayOnIncomplete->setRange(0, std::numeric_limits<int>::max());
        incompleteForm->addRow(tr("EmoteOnIncomplete:"),      m_requestEmoteOnIncomplete);
        incompleteForm->addRow(tr("EmoteOnIncompleteDelay:"), m_requestDelayOnIncomplete);
        emoteRow->addWidget(incompleteBox);

        vbox->addLayout(emoteRow);

        auto* btnRow = new QHBoxLayout;
        m_requestSaveBtn = new QPushButton(tr("Save (UPSERT quest_request_items)"), page);
        m_requestSaveBtn->setEnabled(false);
        btnRow->addWidget(m_requestSaveBtn);
        btnRow->addStretch(1);
        vbox->addLayout(btnRow);

        m_requestStatus = new QLabel(tr("Load a quest ID first."), page);
        vbox->addWidget(m_requestStatus);

        m_tabs->addTab(page, tr("Request items"));
    }

    // -- Tab 3: Details (emotes-only) ------------------------------------
    {
        auto* page  = new QWidget(m_tabs);
        auto* vbox  = new QVBoxLayout(page);

        vbox->addWidget(new QLabel(tr(
            "quest_details carries only the pickup-time emote channels.  "
            "The text shown at quest pickup lives in quest_template.QuestDescription."), page));

        auto* emoteGrid = new QGridLayout;
        for (int i = 0; i < 4; ++i)
        {
            QGroupBox* slot = makeEmoteSlot(page, i + 1, m_detailsEmote[i], m_detailsDelay[i]);
            emoteGrid->addWidget(slot, i / 2, i % 2);
        }
        vbox->addLayout(emoteGrid);
        vbox->addStretch(1);

        auto* btnRow = new QHBoxLayout;
        m_detailsSaveBtn = new QPushButton(tr("Save (UPSERT quest_details)"), page);
        m_detailsSaveBtn->setEnabled(false);
        btnRow->addWidget(m_detailsSaveBtn);
        btnRow->addStretch(1);
        vbox->addLayout(btnRow);

        m_detailsStatus = new QLabel(tr("Load a quest ID first."), page);
        vbox->addWidget(m_detailsStatus);

        m_tabs->addTab(page, tr("Details (emotes-only)"));
    }

    m_statusLabel = new QLabel(tr("Enter a quest ID and press Load."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_loadBtn,         &QPushButton::clicked, this, &QuestDialogTextDialog::onLoad);
    connect(m_offerSaveBtn,    &QPushButton::clicked, this, &QuestDialogTextDialog::onSaveOfferReward);
    connect(m_requestSaveBtn,  &QPushButton::clicked, this, &QuestDialogTextDialog::onSaveRequestItems);
    connect(m_detailsSaveBtn,  &QPushButton::clicked, this, &QuestDialogTextDialog::onSaveDetails);
}

void QuestDialogTextDialog::detectSchemaFor(QString const& table, QSet<QString>& outCols)
{
    outCols.clear();
    if (!m_db || !m_db->isConnected()) return;

    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='%s'",
        m_worldDb.toStdString().c_str(),
        table.toStdString().c_str());
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok()) return;
    for (size_t r = 0; r < res.rowCount(); ++r)
        outCols.insert(QString::fromStdString(res.cell(r, 0)).toLower());
}

bool QuestDialogTextDialog::hasCol(QSet<QString> const& cols, char const* name) const
{
    return cols.contains(QString::fromLatin1(name).toLower());
}

QString QuestDialogTextDialog::resolveQuestTitle(uint32_t questId)
{
    if (!m_db || !m_db->isConnected()) return QString();

    // Probe quest_template for the title column name first - modern TC uses
    // LogTitle, older forks used Title.  Either way, look up case-insensitively.
    QSet<QString> qtCols;
    detectSchemaFor(QStringLiteral("quest_template"), qtCols);
    QString titleCol;
    if (qtCols.contains(QStringLiteral("logtitle")))   titleCol = QStringLiteral("LogTitle");
    else if (qtCols.contains(QStringLiteral("title"))) titleCol = QStringLiteral("Title");
    if (titleCol.isEmpty()) return QString();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT %s FROM %s.quest_template WHERE ID=%u LIMIT 1",
        titleCol.toStdString().c_str(),
        m_worldDb.toStdString().c_str(), questId);
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0) return QString();
    return QString::fromStdString(res.cell(0, 0));
}

void QuestDialogTextDialog::onLoad()
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return;
    }
    m_loadedQuestId = static_cast<uint32_t>(m_questIdSpin->value());

    if (!m_schemaDetected)
    {
        detectSchemaFor(QStringLiteral("quest_offer_reward"),  m_colsOffer);
        detectSchemaFor(QStringLiteral("quest_request_items"), m_colsRequest);
        detectSchemaFor(QStringLiteral("quest_details"),       m_colsDetails);
        m_schemaDetected = true;
    }

    QString const title = resolveQuestTitle(m_loadedQuestId);
    if (title.isEmpty())
    {
        m_titleLabel->setText(tr("%1 - (unknown - not in quest_template)").arg(m_loadedQuestId));
    }
    else
    {
        m_titleLabel->setText(tr("%1 - %2").arg(m_loadedQuestId).arg(title));
    }

    loadAll();

    m_offerSaveBtn  ->setEnabled(true);
    m_requestSaveBtn->setEnabled(true);
    m_detailsSaveBtn->setEnabled(true);

    m_statusLabel->setText(tr("Loaded quest ID=%1 (offerExists=%2 requestExists=%3 detailsExists=%4)")
        .arg(m_loadedQuestId)
        .arg(m_offerExists ? tr("yes") : tr("no"))
        .arg(m_requestExists ? tr("yes") : tr("no"))
        .arg(m_detailsExists ? tr("yes") : tr("no")));
}

void QuestDialogTextDialog::loadAll()
{
    loadOfferRewardTab();
    loadRequestItemsTab();
    loadDetailsTab();
}

void QuestDialogTextDialog::loadOfferRewardTab()
{
    m_offerExists = false;
    m_offerText->setPlainText(QString());
    for (int i = 0; i < 4; ++i) { m_offerEmote[i]->setValue(0); m_offerDelay[i]->setValue(0); }

    if (m_colsOffer.isEmpty())
    {
        m_offerStatus->setText(tr("quest_offer_reward not found in schema '%1'.").arg(m_worldDb));
        return;
    }

    auto col = [&](char const* name) -> QString {
        return hasCol(m_colsOffer, name) ? QString::fromLatin1(name) : QStringLiteral("0");
    };
    auto colText = [&](char const* name) -> QString {
        return hasCol(m_colsOffer, name) ? QString::fromLatin1(name) : QStringLiteral("''");
    };

    QString const sql = QStringLiteral(
        "SELECT %1, %2, %3, %4, %5, %6, %7, %8, %9 "
        "FROM %10.quest_offer_reward WHERE ID=%11 LIMIT 1")
        .arg(colText("RewardText"))
        .arg(col("Emote1"))
        .arg(col("Emote2"))
        .arg(col("Emote3"))
        .arg(col("Emote4"))
        .arg(col("EmoteDelay1"))
        .arg(col("EmoteDelay2"))
        .arg(col("EmoteDelay3"))
        .arg(col("EmoteDelay4"))
        .arg(m_worldDb).arg(m_loadedQuestId);

    db::QueryResult res;
    auto err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        m_offerStatus->setText(tr("quest_offer_reward query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        m_offerStatus->setText(tr("No quest_offer_reward row for ID=%1 - Save will INSERT.").arg(m_loadedQuestId));
        return;
    }
    m_offerExists = true;
    m_offerText->setPlainText(QString::fromStdString(res.cell(0, 0)));
    for (int i = 0; i < 4; ++i)
    {
        m_offerEmote[i]->setValue(static_cast<int>(res.asUInt64(0, 1 + i).value_or(0)));
        m_offerDelay[i]->setValue(static_cast<int>(res.asUInt64(0, 5 + i).value_or(0)));
    }
    m_offerStatus->setText(tr("Loaded quest_offer_reward row for ID=%1 - Save will UPDATE.").arg(m_loadedQuestId));
}

void QuestDialogTextDialog::loadRequestItemsTab()
{
    m_requestExists = false;
    m_requestText->setPlainText(QString());
    m_requestEmoteOnComplete->setValue(0);
    m_requestEmoteOnIncomplete->setValue(0);
    m_requestDelayOnComplete->setValue(0);
    m_requestDelayOnIncomplete->setValue(0);

    if (m_colsRequest.isEmpty())
    {
        m_requestStatus->setText(tr("quest_request_items not found in schema '%1'.").arg(m_worldDb));
        return;
    }

    auto col = [&](char const* name) -> QString {
        return hasCol(m_colsRequest, name) ? QString::fromLatin1(name) : QStringLiteral("0");
    };
    auto colText = [&](char const* name) -> QString {
        return hasCol(m_colsRequest, name) ? QString::fromLatin1(name) : QStringLiteral("''");
    };

    QString const sql = QStringLiteral(
        "SELECT %1, %2, %3, %4, %5 "
        "FROM %6.quest_request_items WHERE ID=%7 LIMIT 1")
        .arg(colText("CompletionText"))
        .arg(col("EmoteOnComplete"))
        .arg(col("EmoteOnIncomplete"))
        .arg(col("EmoteOnCompleteDelay"))
        .arg(col("EmoteOnIncompleteDelay"))
        .arg(m_worldDb).arg(m_loadedQuestId);

    db::QueryResult res;
    auto err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        m_requestStatus->setText(tr("quest_request_items query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        m_requestStatus->setText(tr("No quest_request_items row for ID=%1 - Save will INSERT.").arg(m_loadedQuestId));
        return;
    }
    m_requestExists = true;
    m_requestText             ->setPlainText(QString::fromStdString(res.cell(0, 0)));
    m_requestEmoteOnComplete  ->setValue(static_cast<int>(res.asUInt64(0, 1).value_or(0)));
    m_requestEmoteOnIncomplete->setValue(static_cast<int>(res.asUInt64(0, 2).value_or(0)));
    m_requestDelayOnComplete  ->setValue(static_cast<int>(res.asUInt64(0, 3).value_or(0)));
    m_requestDelayOnIncomplete->setValue(static_cast<int>(res.asUInt64(0, 4).value_or(0)));
    m_requestStatus->setText(tr("Loaded quest_request_items row for ID=%1 - Save will UPDATE.").arg(m_loadedQuestId));
}

void QuestDialogTextDialog::loadDetailsTab()
{
    m_detailsExists = false;
    for (int i = 0; i < 4; ++i) { m_detailsEmote[i]->setValue(0); m_detailsDelay[i]->setValue(0); }

    if (m_colsDetails.isEmpty())
    {
        m_detailsStatus->setText(tr("quest_details not found in schema '%1'.").arg(m_worldDb));
        return;
    }

    auto col = [&](char const* name) -> QString {
        return hasCol(m_colsDetails, name) ? QString::fromLatin1(name) : QStringLiteral("0");
    };

    QString const sql = QStringLiteral(
        "SELECT %1, %2, %3, %4, %5, %6, %7, %8 "
        "FROM %9.quest_details WHERE ID=%10 LIMIT 1")
        .arg(col("Emote1"))
        .arg(col("Emote2"))
        .arg(col("Emote3"))
        .arg(col("Emote4"))
        .arg(col("EmoteDelay1"))
        .arg(col("EmoteDelay2"))
        .arg(col("EmoteDelay3"))
        .arg(col("EmoteDelay4"))
        .arg(m_worldDb).arg(m_loadedQuestId);

    db::QueryResult res;
    auto err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        m_detailsStatus->setText(tr("quest_details query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        m_detailsStatus->setText(tr("No quest_details row for ID=%1 - Save will INSERT.").arg(m_loadedQuestId));
        return;
    }
    m_detailsExists = true;
    for (int i = 0; i < 4; ++i)
    {
        m_detailsEmote[i]->setValue(static_cast<int>(res.asUInt64(0, 0 + i).value_or(0)));
        m_detailsDelay[i]->setValue(static_cast<int>(res.asUInt64(0, 4 + i).value_or(0)));
    }
    m_detailsStatus->setText(tr("Loaded quest_details row for ID=%1 - Save will UPDATE.").arg(m_loadedQuestId));
}

bool QuestDialogTextDialog::runInTransaction(QStringList const& sqls, QString const& description)
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return false;
    }
    auto err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return false;
    }
    uint64_t totalAffected = 0;
    for (QString const& sql : sqls)
    {
        uint64_t affected = 0;
        err = m_db->exec(sql.toStdString(), &affected);
        if (!err.ok())
        {
            (void)m_db->exec("ROLLBACK");
            QMessageBox::critical(this, tr("DML failed"),
                tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
            return false;
        }
        totalAffected += affected;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(totalAffected)));
    return true;
}

void QuestDialogTextDialog::onSaveOfferReward()
{
    if (m_loadedQuestId == 0)
    {
        QMessageBox::warning(this, tr("Save"), tr("Load a quest ID first."));
        return;
    }
    if (m_colsOffer.isEmpty())
    {
        QMessageBox::warning(this, tr("Save"), tr("quest_offer_reward not found in schema."));
        return;
    }

    QString const escText = QString::fromStdString(
        m_db->escapeString(m_offerText->toPlainText().toStdString()));

    // Build the (col, val) pair list dynamically against the live schema.
    struct Assignment { QString col; QString valLit; };
    QList<Assignment> assigns;
    auto add = [&](char const* name, QString const& valLit) {
        if (hasCol(m_colsOffer, name))
            assigns.append({ QString::fromLatin1(name), valLit });
    };
    add("RewardText",  QStringLiteral("'%1'").arg(escText));
    for (int i = 0; i < 4; ++i)
    {
        char n[16];
        std::snprintf(n, sizeof(n), "Emote%d", i + 1);
        add(n, QString::number(m_offerEmote[i]->value()));
        std::snprintf(n, sizeof(n), "EmoteDelay%d", i + 1);
        add(n, QString::number(m_offerDelay[i]->value()));
    }

    QString sql;
    QString description;
    if (m_offerExists)
    {
        QStringList setters;
        for (auto const& a : assigns)
            setters << QStringLiteral("%1=%2").arg(a.col).arg(a.valLit);
        if (setters.isEmpty())
        {
            QMessageBox::warning(this, tr("Save"), tr("No editable columns matched live schema."));
            return;
        }
        sql = QStringLiteral("UPDATE %1.quest_offer_reward SET %2 WHERE ID=%3")
            .arg(m_worldDb).arg(setters.join(QStringLiteral(", "))).arg(m_loadedQuestId);
        description = tr("UPDATE quest_offer_reward (ID=%1)").arg(m_loadedQuestId);
    }
    else
    {
        QStringList cols; QStringList vals;
        cols << QStringLiteral("ID");
        vals << QString::number(m_loadedQuestId);
        for (auto const& a : assigns) { cols << a.col; vals << a.valLit; }
        sql = QStringLiteral("INSERT INTO %1.quest_offer_reward (%2) VALUES (%3)")
            .arg(m_worldDb)
            .arg(cols.join(QStringLiteral(", ")))
            .arg(vals.join(QStringLiteral(", ")));
        description = tr("INSERT quest_offer_reward (ID=%1)").arg(m_loadedQuestId);
    }

    if (runInTransaction(QStringList{ sql }, description))
    {
        m_offerExists = true;
        loadOfferRewardTab();
    }
}

void QuestDialogTextDialog::onSaveRequestItems()
{
    if (m_loadedQuestId == 0)
    {
        QMessageBox::warning(this, tr("Save"), tr("Load a quest ID first."));
        return;
    }
    if (m_colsRequest.isEmpty())
    {
        QMessageBox::warning(this, tr("Save"), tr("quest_request_items not found in schema."));
        return;
    }

    QString const escText = QString::fromStdString(
        m_db->escapeString(m_requestText->toPlainText().toStdString()));

    struct Assignment { QString col; QString valLit; };
    QList<Assignment> assigns;
    auto add = [&](char const* name, QString const& valLit) {
        if (hasCol(m_colsRequest, name))
            assigns.append({ QString::fromLatin1(name), valLit });
    };
    add("CompletionText",        QStringLiteral("'%1'").arg(escText));
    add("EmoteOnComplete",       QString::number(m_requestEmoteOnComplete->value()));
    add("EmoteOnIncomplete",     QString::number(m_requestEmoteOnIncomplete->value()));
    add("EmoteOnCompleteDelay",  QString::number(m_requestDelayOnComplete->value()));
    add("EmoteOnIncompleteDelay",QString::number(m_requestDelayOnIncomplete->value()));

    QString sql;
    QString description;
    if (m_requestExists)
    {
        QStringList setters;
        for (auto const& a : assigns)
            setters << QStringLiteral("%1=%2").arg(a.col).arg(a.valLit);
        if (setters.isEmpty())
        {
            QMessageBox::warning(this, tr("Save"), tr("No editable columns matched live schema."));
            return;
        }
        sql = QStringLiteral("UPDATE %1.quest_request_items SET %2 WHERE ID=%3")
            .arg(m_worldDb).arg(setters.join(QStringLiteral(", "))).arg(m_loadedQuestId);
        description = tr("UPDATE quest_request_items (ID=%1)").arg(m_loadedQuestId);
    }
    else
    {
        QStringList cols; QStringList vals;
        cols << QStringLiteral("ID");
        vals << QString::number(m_loadedQuestId);
        for (auto const& a : assigns) { cols << a.col; vals << a.valLit; }
        sql = QStringLiteral("INSERT INTO %1.quest_request_items (%2) VALUES (%3)")
            .arg(m_worldDb)
            .arg(cols.join(QStringLiteral(", ")))
            .arg(vals.join(QStringLiteral(", ")));
        description = tr("INSERT quest_request_items (ID=%1)").arg(m_loadedQuestId);
    }

    if (runInTransaction(QStringList{ sql }, description))
    {
        m_requestExists = true;
        loadRequestItemsTab();
    }
}

void QuestDialogTextDialog::onSaveDetails()
{
    if (m_loadedQuestId == 0)
    {
        QMessageBox::warning(this, tr("Save"), tr("Load a quest ID first."));
        return;
    }
    if (m_colsDetails.isEmpty())
    {
        QMessageBox::warning(this, tr("Save"), tr("quest_details not found in schema."));
        return;
    }

    struct Assignment { QString col; QString valLit; };
    QList<Assignment> assigns;
    auto add = [&](char const* name, QString const& valLit) {
        if (hasCol(m_colsDetails, name))
            assigns.append({ QString::fromLatin1(name), valLit });
    };
    for (int i = 0; i < 4; ++i)
    {
        char n[16];
        std::snprintf(n, sizeof(n), "Emote%d", i + 1);
        add(n, QString::number(m_detailsEmote[i]->value()));
        std::snprintf(n, sizeof(n), "EmoteDelay%d", i + 1);
        add(n, QString::number(m_detailsDelay[i]->value()));
    }

    QString sql;
    QString description;
    if (m_detailsExists)
    {
        QStringList setters;
        for (auto const& a : assigns)
            setters << QStringLiteral("%1=%2").arg(a.col).arg(a.valLit);
        if (setters.isEmpty())
        {
            QMessageBox::warning(this, tr("Save"), tr("No editable columns matched live schema."));
            return;
        }
        sql = QStringLiteral("UPDATE %1.quest_details SET %2 WHERE ID=%3")
            .arg(m_worldDb).arg(setters.join(QStringLiteral(", "))).arg(m_loadedQuestId);
        description = tr("UPDATE quest_details (ID=%1)").arg(m_loadedQuestId);
    }
    else
    {
        QStringList cols; QStringList vals;
        cols << QStringLiteral("ID");
        vals << QString::number(m_loadedQuestId);
        for (auto const& a : assigns) { cols << a.col; vals << a.valLit; }
        sql = QStringLiteral("INSERT INTO %1.quest_details (%2) VALUES (%3)")
            .arg(m_worldDb)
            .arg(cols.join(QStringLiteral(", ")))
            .arg(vals.join(QStringLiteral(", ")));
        description = tr("INSERT quest_details (ID=%1)").arg(m_loadedQuestId);
    }

    if (runInTransaction(QStringList{ sql }, description))
    {
        m_detailsExists = true;
        loadDetailsTab();
    }
}

} // namespace world_editor::app
