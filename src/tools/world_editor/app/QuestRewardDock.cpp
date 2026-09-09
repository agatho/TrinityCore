#include "QuestRewardDock.h"

#include "../db/MySqlClient.h"

#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include <string>
#include <vector>

namespace world_editor::app
{

namespace
{

// MySQL ER_BAD_FIELD_ERROR — emitted when a SELECT references a column
// the active schema does not have.  We use this to soft-degrade each
// reward cluster independently (older world DBs drop a few of the
// reward columns we project).
constexpr uint32_t kErBadFieldError = 1054;
// MySQL ER_NO_SUCH_TABLE — older shards may not have item_template at
// all (extremely rare, but cheap to defend against the same way).
constexpr uint32_t kErNoSuchTable   = 1146;

bool isSchemaMissing(uint32_t code) noexcept
{
    return code == kErBadFieldError || code == kErNoSuchTable;
}

// Pull item_template.name for a single entry.  Returns empty string on
// any failure (missing row, missing table, missing column); the caller
// then renders the id alone.
QString fetchItemName(db::MySqlClient& db, uint64_t itemId)
{
    if (itemId == 0)
        return {};
    QString const sql = QStringLiteral(
        "SELECT name FROM item_template WHERE entry = %1 LIMIT 1")
        .arg(itemId);
    db::QueryResult res;
    auto const err = db.query(sql.toStdString(), res);
    if (!err.ok() || res.rowCount() == 0)
        return {};
    return QString::fromStdString(res.cell(0, 0));
}

// Render a money amount in g/s/c.  TC stores RewardMoney as copper.
QString formatMoney(int64_t copper)
{
    if (copper == 0)
        return QStringLiteral("0");
    bool const neg = copper < 0;
    uint64_t v = static_cast<uint64_t>(neg ? -copper : copper);
    uint64_t const g = v / 10000;
    uint64_t const s = (v / 100) % 100;
    uint64_t const c = v % 100;
    QString out;
    if (g) out += QStringLiteral("%1g ").arg(g);
    if (s || g) out += QStringLiteral("%1s ").arg(s);
    out += QStringLiteral("%1c").arg(c);
    if (neg) out.prepend(QLatin1Char('-'));
    return out;
}

} // namespace

QuestRewardDock::QuestRewardDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Click a questgiver NPC to see its rewards."), this);
    m_header->setWordWrap(true);
    {
        QFont f = m_header->font();
        f.setBold(true);
        m_header->setFont(f);
    }
    root->addWidget(m_header);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_body = new QWidget(m_scroll);
    m_bodyLayout = new QVBoxLayout(m_body);
    m_bodyLayout->setContentsMargins(2, 2, 2, 2);
    m_bodyLayout->setSpacing(2);
    m_bodyLayout->addStretch(1);
    m_scroll->setWidget(m_body);
    root->addWidget(m_scroll, 1);
}

void QuestRewardDock::resetBody()
{
    // QVBoxLayout::takeAt removes & returns the layout items in order;
    // we delete the widget side-effect to actually free the rendered
    // rows.  The trailing stretch is re-added afterwards.
    while (auto* item = m_bodyLayout->takeAt(0))
    {
        if (auto* w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_bodyLayout->addStretch(1);
}

void QuestRewardDock::addSectionHeader(QString const& text)
{
    auto* lbl = new QLabel(text, m_body);
    QFont f = lbl->font();
    f.setBold(true);
    lbl->setFont(f);
    lbl->setContentsMargins(0, 6, 0, 2);
    m_bodyLayout->insertWidget(m_bodyLayout->count() - 1, lbl);
}

void QuestRewardDock::addBodyLine(QString const& text)
{
    auto* lbl = new QLabel(text, m_body);
    lbl->setWordWrap(true);
    lbl->setContentsMargins(8, 0, 0, 0);
    m_bodyLayout->insertWidget(m_bodyLayout->count() - 1, lbl);
}

void QuestRewardDock::addItemLine(uint64_t itemId, uint64_t count, QString const& itemName)
{
    if (itemName.isEmpty())
        addBodyLine(tr("item %1 x %2").arg(itemId).arg(count));
    else
        addBodyLine(tr("item %1 x %2 — %3").arg(itemId).arg(count).arg(itemName));
}

void QuestRewardDock::clear()
{
    resetBody();
    m_header->setText(tr("Click a questgiver NPC to see its rewards."));
}

void QuestRewardDock::setQuest(uint32_t questId)
{
    resetBody();
    if (questId == 0)
    {
        m_header->setText(tr("Click a questgiver NPC to see its rewards."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected."));
        return;
    }

    // --- Summary (always present: ID, LogTitle, level, XP difficulty, money). ---
    {
        QString const sql = QStringLiteral(
            "SELECT ID, LogTitle, QuestLevel, MinLevel, MaxLevel, "
            "       RewardXPDifficulty, RewardMoney "
            "FROM quest_template WHERE ID = %1 LIMIT 1").arg(questId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            m_header->setText(tr("Query failed: %1")
                .arg(QString::fromStdString(err.message)));
            return;
        }
        if (res.rowCount() == 0)
        {
            m_header->setText(tr("Quest %1 not found in quest_template.").arg(questId));
            return;
        }
        QString const title = QString::fromStdString(res.cell(0, 1));
        m_header->setText(tr("Rewards for quest %1: %2").arg(questId).arg(title));
        addSectionHeader(tr("Summary"));
        addBodyLine(tr("ID: %1").arg(questId));
        addBodyLine(tr("Title: %1").arg(title.isEmpty() ? tr("(none)") : title));
        addBodyLine(tr("Quest level: %1  (min %2, max %3)")
            .arg(res.asInt64(0, 2).value_or(0))
            .arg(res.asInt64(0, 3).value_or(0))
            .arg(res.asInt64(0, 4).value_or(0)));
        addBodyLine(tr("XP difficulty: %1").arg(res.asInt64(0, 5).value_or(0)));
        addBodyLine(tr("Money: %1").arg(formatMoney(res.asInt64(0, 6).value_or(0))));
    }

    // --- Guaranteed items (RewardItem1..4 / RewardAmount1..4). ---
    {
        addSectionHeader(tr("Items (guaranteed)"));
        QString const sql = QStringLiteral(
            "SELECT RewardItem1, RewardAmount1, "
            "       RewardItem2, RewardAmount2, "
            "       RewardItem3, RewardAmount3, "
            "       RewardItem4, RewardAmount4 "
            "FROM quest_template WHERE ID = %1 LIMIT 1").arg(questId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok() && isSchemaMissing(err.code))
        {
            addBodyLine(tr("(column not present in this schema)"));
        }
        else if (!err.ok())
        {
            addBodyLine(tr("Query failed: %1").arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addBodyLine(tr("(none)"));
        }
        else
        {
            int rendered = 0;
            for (int i = 0; i < 4; ++i)
            {
                uint64_t const item  = res.asUInt64(0, size_t(i * 2)).value_or(0);
                uint64_t const count = res.asUInt64(0, size_t(i * 2 + 1)).value_or(0);
                if (item == 0)
                    continue;
                addItemLine(item, count, fetchItemName(*m_db, item));
                ++rendered;
            }
            if (rendered == 0)
                addBodyLine(tr("(none)"));
        }
    }

    // --- Choice items (RewardChoiceItemID1..6 / RewardChoiceItemQuantity1..6). ---
    {
        addSectionHeader(tr("Items (choice)"));
        QString const sql = QStringLiteral(
            "SELECT RewardChoiceItemID1, RewardChoiceItemQuantity1, "
            "       RewardChoiceItemID2, RewardChoiceItemQuantity2, "
            "       RewardChoiceItemID3, RewardChoiceItemQuantity3, "
            "       RewardChoiceItemID4, RewardChoiceItemQuantity4, "
            "       RewardChoiceItemID5, RewardChoiceItemQuantity5, "
            "       RewardChoiceItemID6, RewardChoiceItemQuantity6 "
            "FROM quest_template WHERE ID = %1 LIMIT 1").arg(questId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok() && isSchemaMissing(err.code))
        {
            addBodyLine(tr("(column not present in this schema)"));
        }
        else if (!err.ok())
        {
            addBodyLine(tr("Query failed: %1").arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addBodyLine(tr("(none)"));
        }
        else
        {
            int rendered = 0;
            for (int i = 0; i < 6; ++i)
            {
                uint64_t const item  = res.asUInt64(0, size_t(i * 2)).value_or(0);
                uint64_t const count = res.asUInt64(0, size_t(i * 2 + 1)).value_or(0);
                if (item == 0)
                    continue;
                if (rendered == 0)
                    addBodyLine(tr("Choose 1 of:"));
                addItemLine(item, count, fetchItemName(*m_db, item));
                ++rendered;
            }
            if (rendered == 0)
                addBodyLine(tr("(none)"));
        }
    }

    // --- Reputation (RewardFactionID1..5 / RewardFactionValue1..5). ---
    {
        addSectionHeader(tr("Reputation"));
        QString const sql = QStringLiteral(
            "SELECT RewardFactionID1, RewardFactionValue1, "
            "       RewardFactionID2, RewardFactionValue2, "
            "       RewardFactionID3, RewardFactionValue3, "
            "       RewardFactionID4, RewardFactionValue4, "
            "       RewardFactionID5, RewardFactionValue5 "
            "FROM quest_template WHERE ID = %1 LIMIT 1").arg(questId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok() && isSchemaMissing(err.code))
        {
            addBodyLine(tr("(column not present in this schema)"));
        }
        else if (!err.ok())
        {
            addBodyLine(tr("Query failed: %1").arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addBodyLine(tr("(none)"));
        }
        else
        {
            int rendered = 0;
            for (int i = 0; i < 5; ++i)
            {
                uint64_t const faction = res.asUInt64(0, size_t(i * 2)).value_or(0);
                int64_t  const value   = res.asInt64 (0, size_t(i * 2 + 1)).value_or(0);
                if (faction == 0 || value == 0)
                    continue;
                addBodyLine(tr("%1%2 rep with faction %3")
                    .arg(value > 0 ? QStringLiteral("+") : QString())
                    .arg(value)
                    .arg(faction));
                ++rendered;
            }
            if (rendered == 0)
                addBodyLine(tr("(none)"));
        }
    }

    // --- Spell / Title / Honor / Arena / Skill footer block. ---
    {
        addSectionHeader(tr("Spell / Title / Honor / Skill"));
        QString const sql = QStringLiteral(
            "SELECT RewardSpell, RewardTitle, RewardArenaPoints, "
            "       RewardSkillLineID, RewardSkillPoints, "
            "       RewardHonor, RewardKillHonor "
            "FROM quest_template WHERE ID = %1 LIMIT 1").arg(questId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok() && isSchemaMissing(err.code))
        {
            addBodyLine(tr("(column not present in this schema)"));
        }
        else if (!err.ok())
        {
            addBodyLine(tr("Query failed: %1").arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addBodyLine(tr("(none)"));
        }
        else
        {
            int64_t const spell      = res.asInt64(0, 0).value_or(0);
            int64_t const title      = res.asInt64(0, 1).value_or(0);
            int64_t const arena      = res.asInt64(0, 2).value_or(0);
            int64_t const skillLine  = res.asInt64(0, 3).value_or(0);
            int64_t const skillPts   = res.asInt64(0, 4).value_or(0);
            int64_t const honor      = res.asInt64(0, 5).value_or(0);
            int64_t const killHonor  = res.asInt64(0, 6).value_or(0);
            int rendered = 0;
            if (spell)     { addBodyLine(tr("Spell: %1").arg(spell)); ++rendered; }
            if (title)     { addBodyLine(tr("Title: %1").arg(title)); ++rendered; }
            if (arena)     { addBodyLine(tr("Arena points: %1").arg(arena)); ++rendered; }
            if (skillLine) { addBodyLine(tr("Skill: line %1, +%2 points").arg(skillLine).arg(skillPts)); ++rendered; }
            if (honor)     { addBodyLine(tr("Honor: %1").arg(honor)); ++rendered; }
            if (killHonor) { addBodyLine(tr("Kill honor: %1").arg(killHonor)); ++rendered; }
            if (rendered == 0)
                addBodyLine(tr("(none)"));
        }
    }

    // --- Mail block (RewardMailTemplateID / RewardMailDelay). ---
    {
        addSectionHeader(tr("Mail"));
        QString const sql = QStringLiteral(
            "SELECT RewardMailTemplateID, RewardMailDelay "
            "FROM quest_template WHERE ID = %1 LIMIT 1").arg(questId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok() && isSchemaMissing(err.code))
        {
            addBodyLine(tr("(column not present in this schema)"));
        }
        else if (!err.ok())
        {
            addBodyLine(tr("Query failed: %1").arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addBodyLine(tr("(none)"));
        }
        else
        {
            int64_t const tmpl  = res.asInt64(0, 0).value_or(0);
            int64_t const delay = res.asInt64(0, 1).value_or(0);
            if (tmpl == 0 && delay == 0)
                addBodyLine(tr("(none)"));
            else
                addBodyLine(tr("Mail template %1, delay %2s").arg(tmpl).arg(delay));
        }
    }
}

} // namespace world_editor::app
