#include "QuestGiverLinkageDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <array>
#include <cstdio>
#include <limits>
#include <string>

namespace world_editor::app
{

namespace
{

// Canonical list of supported tables.  The combo box's userData carries
// the index into this allowlist, and selectedTable() returns one of
// these strings verbatim -- there is no path by which a free-form
// operator string ever lands in the table-name substitution slot.
struct TableSpec
{
    char const* tableName;   // exact DB table name
    bool        creatureSide; // true -> creature_template; false -> gameobject_template
};

constexpr std::array<TableSpec, 4> kTableSpecs{ {
    { "creature_queststarter",   true  },
    { "creature_questender",     true  },
    { "gameobject_queststarter", false },
    { "gameobject_questender",   false },
} };

enum Col : int
{
    COL_ID        = 0,
    COL_QUEST     = 1,
    COL_NAME      = 2,
    COL_COUNT     = 3,
};

} // namespace

QuestGiverLinkageDialog::QuestGiverLinkageDialog(db::MySqlClient* dbClient,
                                                 QString const& worldDbName,
                                                 QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Quest-giver linkage editor"));
    setModal(true);
    resize(900, 600);

    auto* outer = new QVBoxLayout(this);

    // -- Top filter row --------------------------------------------------
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Table:"), this));
    m_tableCombo = new QComboBox(this);
    for (size_t i = 0; i < kTableSpecs.size(); ++i)
    {
        m_tableCombo->addItem(QString::fromLatin1(kTableSpecs[i].tableName),
                              static_cast<int>(i));
    }
    filterRow->addWidget(m_tableCombo);

    filterRow->addSpacing(12);
    filterRow->addWidget(new QLabel(tr("Filter by quest ID:"), this));
    m_questFilter = new QSpinBox(this);
    m_questFilter->setRange(0, std::numeric_limits<int>::max());
    m_questFilter->setValue(0);
    m_questFilter->setToolTip(tr("0 = ignore (no quest filter)"));
    filterRow->addWidget(m_questFilter);

    filterRow->addWidget(new QLabel(tr("Filter by id:"), this));
    m_idFilter = new QSpinBox(this);
    m_idFilter->setRange(0, std::numeric_limits<int>::max());
    m_idFilter->setValue(0);
    m_idFilter->setToolTip(tr("0 = ignore (no template-entry filter)"));
    filterRow->addWidget(m_idFilter);

    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    filterRow->addWidget(m_refreshBtn);
    filterRow->addStretch(1);
    outer->addLayout(filterRow);

    // -- Main table ------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({ tr("id"), tr("quest"), tr("id name") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false); // composite-PK ordering is canonical
    outer->addWidget(m_table, 1);

    // -- Action buttons --------------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn       = new QPushButton(tr("Add link..."),                this);
    m_removeBtn    = new QPushButton(tr("Remove link"),                this);
    m_questInfoBtn = new QPushButton(tr("Show quest info"),            this);
    m_lookupBtn    = new QPushButton(tr("Lookup quest-giver template"),this);
    m_removeBtn   ->setEnabled(false);
    m_questInfoBtn->setEnabled(false);
    m_lookupBtn   ->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_questInfoBtn);
    btnRow->addWidget(m_lookupBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Loading..."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_tableCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QuestGiverLinkageDialog::onTableChanged);
    connect(m_refreshBtn,  &QPushButton::clicked, this, &QuestGiverLinkageDialog::onRefresh);
    connect(m_addBtn,      &QPushButton::clicked, this, &QuestGiverLinkageDialog::onAdd);
    connect(m_removeBtn,   &QPushButton::clicked, this, &QuestGiverLinkageDialog::onRemove);
    connect(m_questInfoBtn,&QPushButton::clicked, this, &QuestGiverLinkageDialog::onShowQuestInfo);
    connect(m_lookupBtn,   &QPushButton::clicked, this, &QuestGiverLinkageDialog::onLookupTemplate);
    connect(m_table,       &QTableWidget::itemSelectionChanged,
            this, &QuestGiverLinkageDialog::onSelectionChanged);

    loadRows();
}

QString QuestGiverLinkageDialog::selectedTable() const
{
    int const idx = m_tableCombo->currentData().toInt();
    if (idx < 0 || idx >= static_cast<int>(kTableSpecs.size()))
        return QString::fromLatin1(kTableSpecs[0].tableName);
    return QString::fromLatin1(kTableSpecs[idx].tableName);
}

bool QuestGiverLinkageDialog::isCreatureSide() const
{
    int const idx = m_tableCombo->currentData().toInt();
    if (idx < 0 || idx >= static_cast<int>(kTableSpecs.size()))
        return true;
    return kTableSpecs[idx].creatureSide;
}

void QuestGiverLinkageDialog::onTableChanged(int /*index*/)
{
    loadRows();
}

void QuestGiverLinkageDialog::onRefresh()
{
    loadRows();
}

void QuestGiverLinkageDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0;
    m_removeBtn   ->setEnabled(has);
    m_questInfoBtn->setEnabled(has);
    m_lookupBtn   ->setEnabled(has);
}

bool QuestGiverLinkageDialog::currentRowKey(uint32_t& idOut, uint32_t& questOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* idCell = m_table->item(row, COL_ID);
    auto* qCell  = m_table->item(row, COL_QUEST);
    if (!idCell || !qCell) return false;
    idOut    = static_cast<uint32_t>(idCell->data(Qt::DisplayRole).toULongLong());
    questOut = static_cast<uint32_t>(qCell ->data(Qt::DisplayRole).toULongLong());
    return true;
}

void QuestGiverLinkageDialog::loadRows()
{
    m_loading = true;
    m_table->setRowCount(0);

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }

    QString const tbl  = selectedTable();
    bool const   creat = isCreatureSide();
    QString const tplT = creat ? QStringLiteral("creature_template")
                               : QStringLiteral("gameobject_template");

    uint32_t const questFilter = static_cast<uint32_t>(m_questFilter->value());
    uint32_t const idFilter    = static_cast<uint32_t>(m_idFilter   ->value());

    // Optional WHERE chain.  Both filters are uint32 spinboxes so the
    // values are safe to inject as decimal literals; we cap them with
    // sprintf to make that explicit.
    std::string where;
    if (questFilter > 0)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "l.quest = %u", questFilter);
        where = buf;
    }
    if (idFilter > 0)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "l.id = %u", idFilter);
        if (!where.empty()) where += " AND ";
        where += buf;
    }

    // LIMIT 5000 mirrors WorldSafeLocsDialog; per-table linkage rows fit
    // easily under that ceiling for stock TC data, and the filter row
    // narrows further when needed.
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "SELECT l.id, l.quest, COALESCE(t.name, '') "
        "FROM %s.%s l "
        "LEFT JOIN %s.%s t ON t.entry = l.id "
        "%s%s "
        "ORDER BY l.id, l.quest LIMIT 5000",
        m_worldDb.toStdString().c_str(), tbl.toStdString().c_str(),
        m_worldDb.toStdString().c_str(), tplT.toStdString().c_str(),
        where.empty() ? "" : "WHERE ",
        where.c_str());

    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("%1 query failed: %2")
            .arg(tbl).arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const id    = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        uint32_t const quest = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(0));

        auto* idCell = new QTableWidgetItem;
        idCell->setData(Qt::DisplayRole, qulonglong(id));
        m_table->setItem(int(r), COL_ID, idCell);

        auto* qCell = new QTableWidgetItem;
        qCell->setData(Qt::DisplayRole, qulonglong(quest));
        m_table->setItem(int(r), COL_QUEST, qCell);

        m_table->setItem(int(r), COL_NAME,
            new QTableWidgetItem(QString::fromStdString(res.cell(r, 2))));
    }

    m_statusLabel->setText(tr("%1: rows=%2 (capped at 5000)")
        .arg(tbl).arg(res.rowCount()));
    m_loading = false;
    onSelectionChanged();
}

bool QuestGiverLinkageDialog::runInTransaction(QStringList const& sqls, QString const& description)
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

void QuestGiverLinkageDialog::onAdd()
{
    QString const tbl = selectedTable();

    // Modal: two spinboxes (id, quest) under a QFormLayout, identical
    // shape to the rest of the editor's "add row" modals.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add link to %1").arg(tbl));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* idSpin = new QSpinBox(&dlg);
    idSpin->setRange(0, std::numeric_limits<int>::max());
    idSpin->setValue(0);
    form->addRow(tr("id (%1.entry):").arg(isCreatureSide()
        ? QStringLiteral("creature_template") : QStringLiteral("gameobject_template")),
        idSpin);

    auto* questSpin = new QSpinBox(&dlg);
    questSpin->setRange(0, std::numeric_limits<int>::max());
    questSpin->setValue(0);
    form->addRow(tr("quest (quest_template.ID):"), questSpin);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);

    if (dlg.exec() != QDialog::Accepted) return;

    uint32_t const id    = static_cast<uint32_t>(idSpin   ->value());
    uint32_t const quest = static_cast<uint32_t>(questSpin->value());
    if (id == 0 || quest == 0)
    {
        QMessageBox::warning(this, tr("Add link"),
            tr("Both id and quest must be > 0."));
        return;
    }

    QString const sql = QStringLiteral("INSERT INTO %1.%2 (id, quest) VALUES (%3, %4)")
        .arg(m_worldDb).arg(tbl).arg(id).arg(quest);
    if (runInTransaction(QStringList{ sql },
            tr("INSERT %1 (id=%2, quest=%3)").arg(tbl).arg(id).arg(quest)))
        loadRows();
}

void QuestGiverLinkageDialog::onRemove()
{
    uint32_t id = 0, quest = 0;
    if (!currentRowKey(id, quest)) return;
    QString const tbl = selectedTable();

    auto const choice = QMessageBox::question(this, tr("Remove link"),
        tr("Delete row %1 (id=%2, quest=%3)?").arg(tbl).arg(id).arg(quest),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.%2 WHERE id=%3 AND quest=%4")
        .arg(m_worldDb).arg(tbl).arg(id).arg(quest);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE %1 (id=%2, quest=%3)").arg(tbl).arg(id).arg(quest)))
        loadRows();
}

void QuestGiverLinkageDialog::onShowQuestInfo()
{
    uint32_t id = 0, quest = 0;
    if (!currentRowKey(id, quest)) return;

    // quest_template stores LogTitle in modern TC; older forks use
    // Title.  Both columns coexist (Title is the offer-time line);
    // pick the non-empty one with COALESCE so we render something
    // useful either way.
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT COALESCE(NULLIF(LogTitle, ''), COALESCE(Title, '')) AS title, "
        "       MinLevel, MaxLevel, QuestType "
        "FROM %s.quest_template WHERE ID = %u LIMIT 1",
        m_worldDb.toStdString().c_str(), quest);

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Quest info"),
            tr("quest_template query failed: %1")
                .arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        QMessageBox::information(this, tr("Quest info"),
            tr("No quest_template row for ID=%1.").arg(quest));
        return;
    }

    QString const title    = QString::fromStdString(res.cell(0, 0));
    uint32_t const minLvl  = static_cast<uint32_t>(res.asUInt64(0, 1).value_or(0));
    uint32_t const maxLvl  = static_cast<uint32_t>(res.asUInt64(0, 2).value_or(0));
    uint32_t const qType   = static_cast<uint32_t>(res.asUInt64(0, 3).value_or(0));

    QMessageBox::information(this, tr("Quest %1").arg(quest),
        tr("Title:     %1\nMinLevel:  %2\nMaxLevel:  %3\nQuestType: %4")
            .arg(title).arg(minLvl).arg(maxLvl).arg(qType));
}

void QuestGiverLinkageDialog::onLookupTemplate()
{
    uint32_t id = 0, quest = 0;
    if (!currentRowKey(id, quest)) return;

    bool const creat = isCreatureSide();
    QString const tplT = creat ? QStringLiteral("creature_template")
                               : QStringLiteral("gameobject_template");

    char sql[384];
    if (creat)
    {
        std::snprintf(sql, sizeof(sql),
            "SELECT entry, COALESCE(name, ''), COALESCE(subname, ''), "
            "       minlevel, maxlevel "
            "FROM %s.creature_template WHERE entry = %u LIMIT 1",
            m_worldDb.toStdString().c_str(), id);
    }
    else
    {
        std::snprintf(sql, sizeof(sql),
            "SELECT entry, COALESCE(name, ''), type, displayId, "
            "       COALESCE(IconName, '') "
            "FROM %s.gameobject_template WHERE entry = %u LIMIT 1",
            m_worldDb.toStdString().c_str(), id);
    }

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        qDebug() << "QuestGiverLinkageDialog::onLookupTemplate: query failed:"
                 << QString::fromStdString(err.message);
        m_statusLabel->setText(tr("%1 lookup failed: %2")
            .arg(tplT).arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        QMessageBox::information(this, tr("Template lookup"),
            tr("No %1 row for entry=%2.").arg(tplT).arg(id));
        return;
    }

    if (creat)
    {
        QMessageBox::information(this, tr("creature_template %1").arg(id),
            tr("entry:    %1\nname:     %2\nsubname:  %3\nminlevel: %4\nmaxlevel: %5")
                .arg(static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0)))
                .arg(QString::fromStdString(res.cell(0, 1)))
                .arg(QString::fromStdString(res.cell(0, 2)))
                .arg(static_cast<uint32_t>(res.asUInt64(0, 3).value_or(0)))
                .arg(static_cast<uint32_t>(res.asUInt64(0, 4).value_or(0))));
    }
    else
    {
        QMessageBox::information(this, tr("gameobject_template %1").arg(id),
            tr("entry:     %1\nname:      %2\ntype:      %3\ndisplayId: %4\nIconName:  %5")
                .arg(static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0)))
                .arg(QString::fromStdString(res.cell(0, 1)))
                .arg(static_cast<uint32_t>(res.asUInt64(0, 2).value_or(0)))
                .arg(static_cast<uint32_t>(res.asUInt64(0, 3).value_or(0)))
                .arg(QString::fromStdString(res.cell(0, 4))));
    }
}

} // namespace world_editor::app
