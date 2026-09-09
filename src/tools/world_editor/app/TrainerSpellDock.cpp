#include "TrainerSpellDock.h"

#include "../db/MySqlClient.h"

#include <QHeaderView>
#include <QLabel>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace world_editor::app
{

namespace
{
    // TC trainer.Type enum.  0=class, 1=mounts, 2=tradeskills, 3=pets.
    QString prettyTrainerType(uint64_t t, uint64_t requirement)
    {
        switch (t)
        {
            case 0: return QObject::tr("CLASS (classId=%1)").arg(requirement);
            case 1: return QObject::tr("MOUNTS");
            case 2: return QObject::tr("TRADESKILLS (skillId=%1)").arg(requirement);
            case 3: return QObject::tr("PETS");
            default: return QObject::tr("type=%1 (unknown)").arg(t);
        }
    }

    // Format copper -> "Ng Ns Nc".  0 renders as "-" so the column doesn't
    // become a wall of zeros for trainers that don't gate on money.
    QString prettyMoney(uint64_t copper)
    {
        if (copper == 0) return QStringLiteral("-");
        uint64_t const g = copper / 10000;
        uint64_t const s = (copper / 100) % 100;
        uint64_t const c = copper % 100;
        QString out;
        if (g) out += QString::number(g) + QStringLiteral("g ");
        if (s) out += QString::number(s) + QStringLiteral("s ");
        if (c) out += QString::number(c) + QStringLiteral("c");
        return out.trimmed();
    }

    // True if `table.column` exists.  Used to tolerate schema drift:
    // upstream TC has trainer.Requirement; some forks drop it.  Same
    // for trainer_spell.SpellID vs SpellId.  We probe once per call
    // (lightweight INFORMATION_SCHEMA query) instead of caching.
    bool columnExists(db::MySqlClient* db, std::string const& table, std::string const& column)
    {
        if (!db || !db->isConnected()) return false;
        std::string const sql =
            "SELECT 1 FROM information_schema.columns "
            "WHERE table_schema = DATABASE() AND table_name = '" + table +
            "' AND column_name = '" + column + "' LIMIT 1";
        db::QueryResult res;
        auto const err = db->query(sql, res);
        return err.ok() && res.rowCount() > 0;
    }

    bool tableExists(db::MySqlClient* db, std::string const& table)
    {
        if (!db || !db->isConnected()) return false;
        std::string const sql =
            "SELECT 1 FROM information_schema.tables "
            "WHERE table_schema = DATABASE() AND table_name = '" + table + "' LIMIT 1";
        db::QueryResult res;
        auto const err = db->query(sql, res);
        return err.ok() && res.rowCount() > 0;
    }
}

TrainerSpellDock::TrainerSpellDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Click a trainer NPC to see its offerings."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({
        tr("SpellID"), tr("ReqLvl"), tr("Cost"),
        tr("ReqSkill"), tr("Rank"), tr("Prereqs") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);
}

void TrainerSpellDock::clear()
{
    m_table->setRowCount(0);
    m_header->setText(tr("Click a trainer NPC to see its offerings."));
}

void TrainerSpellDock::setTrainerForCreature(uint32_t creatureEntry)
{
    m_table->setRowCount(0);
    if (creatureEntry == 0)
    {
        m_header->setText(tr("Click a trainer NPC to see its offerings."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected."));
        return;
    }

    // Some TC forks carry `Requirement` on the trainer header; others
    // drop it.  Probe so we don't fail the whole header query on
    // schemas that only carry (Id, Type, Greeting).
    bool const hasRequirement = columnExists(m_db, "trainer", "Requirement");
    QString const headerCols = hasRequirement
        ? QStringLiteral("t.Id, t.Type, t.Requirement, t.Greeting")
        : QStringLiteral("t.Id, t.Type, 0 AS Requirement, t.Greeting");

    // Resolve creature -> trainer.  Upstream uses creature_default_trainer;
    // some forks shipped creature_trainer instead; the most permissive
    // fork-fallback is trainer.Id == creature.entry.  Try in that order.
    auto querySingle = [&](QString const& sql, db::QueryResult& out) -> bool {
        auto const err = m_db->query(sql.toStdString(), out);
        if (!err.ok())
        {
            m_header->setText(tr("Query failed: %1")
                .arg(QString::fromStdString(err.message)));
            return false;
        }
        return true;
    };

    db::QueryResult headerRes;
    bool resolved = false;

    if (tableExists(m_db, "creature_default_trainer"))
    {
        QString const sql = QStringLiteral(
            "SELECT %1 FROM trainer t "
            "JOIN creature_default_trainer cdt ON cdt.TrainerID = t.Id "
            "WHERE cdt.CreatureID = %2 LIMIT 1")
            .arg(headerCols).arg(creatureEntry);
        if (!querySingle(sql, headerRes)) return;
        resolved = (headerRes.rowCount() > 0);
    }

    if (!resolved && tableExists(m_db, "creature_trainer"))
    {
        QString const sql = QStringLiteral(
            "SELECT %1 FROM trainer t "
            "JOIN creature_trainer ct ON ct.TrainerID = t.Id "
            "WHERE ct.CreatureID = %2 LIMIT 1")
            .arg(headerCols).arg(creatureEntry);
        if (!querySingle(sql, headerRes)) return;
        resolved = (headerRes.rowCount() > 0);
    }

    if (!resolved)
    {
        // Final fallback: direct trainer.Id == creature.entry binding.
        QString const sql = QStringLiteral(
            "SELECT %1 FROM trainer t WHERE t.Id = %2 LIMIT 1")
            .arg(headerCols).arg(creatureEntry);
        if (!querySingle(sql, headerRes)) return;
        resolved = (headerRes.rowCount() > 0);
    }

    if (!resolved)
    {
        m_header->setText(tr("Entry %1 has no trainer row (neither "
            "creature_default_trainer / creature_trainer nor direct trainer.Id match).")
            .arg(creatureEntry));
        return;
    }

    uint64_t const trainerId     = headerRes.asUInt64(0, 0).value_or(0);
    uint64_t const trainerType   = headerRes.asUInt64(0, 1).value_or(0);
    uint64_t const requirement   = headerRes.asUInt64(0, 2).value_or(0);

    m_header->setText(tr("Trainer for creature %1: id=%2, type=%3, requirement=%4")
        .arg(creatureEntry)
        .arg(trainerId)
        .arg(prettyTrainerType(trainerType, requirement))
        .arg(requirement));

    // trainer_spell column casing varies (SpellID upstream / SpellId on
    // older forks).  Probe and pick the right name; everything else
    // (MoneyCost, ReqSkillLine, ReqSkillRank, ReqAbility1..3, ReqLevel)
    // is stable across schemas.
    QString const spellCol = columnExists(m_db, "trainer_spell", "SpellID")
        ? QStringLiteral("SpellID") : QStringLiteral("SpellId");

    QString const spellSql = QStringLiteral(
        "SELECT ts.%1, ts.MoneyCost, ts.ReqSkillLine, ts.ReqSkillRank, "
        "       ts.ReqAbility1, ts.ReqAbility2, ts.ReqAbility3, "
        "       ts.ReqLevel "
        "FROM trainer_spell ts "
        "WHERE ts.TrainerId = %2 "
        "ORDER BY ts.ReqLevel, ts.%1 LIMIT 2000").arg(spellCol).arg(trainerId);

    db::QueryResult res;
    auto const err = m_db->query(spellSql.toStdString(), res);
    if (!err.ok())
    {
        m_header->setText(m_header->text() + QStringLiteral("  [spells query failed: ")
            + QString::fromStdString(err.message) + QStringLiteral("]"));
        return;
    }

    if (res.rowCount() == 0)
    {
        m_header->setText(m_header->text() + QStringLiteral("  (no trainer_spell rows)"));
        return;
    }

    m_header->setText(m_header->text() + QStringLiteral("  - %1 spell(s)")
        .arg(res.rowCount()));

    // Skill name lookup is best-effort: skill_line lives in DB2 and is
    // only mirrored on some forks.  Missing mirror renders the raw id.
    bool const hasSkillLine = tableExists(m_db, "skill_line");
    auto skillName = [this, hasSkillLine](uint64_t skillId) -> QString {
        if (skillId == 0) return QStringLiteral("-");
        if (!hasSkillLine || !m_db || !m_db->isConnected())
            return QStringLiteral("#%1").arg(skillId);
        QString const sql = QStringLiteral(
            "SELECT DisplayName FROM skill_line WHERE ID = %1 LIMIT 1").arg(skillId);
        db::QueryResult r;
        if (m_db->query(sql.toStdString(), r).ok() && r.rowCount() > 0)
        {
            QString const name = QString::fromStdString(r.cell(0, 0));
            if (!name.isEmpty()) return QStringLiteral("%1 (#%2)").arg(name).arg(skillId);
        }
        return QStringLiteral("#%1").arg(skillId);
    };

    m_table->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        auto setCell = [this, r](int col, QString const& text) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(int(r), col, item);
        };
        uint64_t const spellId   = res.asUInt64(r, 0).value_or(0);
        uint64_t const money     = res.asUInt64(r, 1).value_or(0);
        uint64_t const reqSkill  = res.asUInt64(r, 2).value_or(0);
        uint64_t const reqRank   = res.asUInt64(r, 3).value_or(0);
        uint64_t const prereq1   = res.asUInt64(r, 4).value_or(0);
        uint64_t const prereq2   = res.asUInt64(r, 5).value_or(0);
        uint64_t const prereq3   = res.asUInt64(r, 6).value_or(0);
        uint64_t const reqLevel  = res.asUInt64(r, 7).value_or(0);

        QStringList prereqs;
        if (prereq1) prereqs << QString::number(prereq1);
        if (prereq2) prereqs << QString::number(prereq2);
        if (prereq3) prereqs << QString::number(prereq3);

        setCell(0, QString::number(spellId));
        setCell(1, reqLevel == 0 ? QStringLiteral("-") : QString::number(reqLevel));
        setCell(2, prettyMoney(money));
        setCell(3, skillName(reqSkill));
        setCell(4, reqRank == 0 ? QStringLiteral("-") : QString::number(reqRank));
        setCell(5, prereqs.isEmpty() ? QStringLiteral("-") : prereqs.join(QStringLiteral(", ")));
    }
}

} // namespace world_editor::app
