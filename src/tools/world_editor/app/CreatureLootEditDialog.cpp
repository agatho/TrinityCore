#include "CreatureLootEditDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Column indices for the main drops table.  Single source of truth so
// the Add/Edit modal can read the selected row's cells off the rendered
// view without re-querying MySQL.
enum Col : int
{
    COL_ITEMTYPE     = 0,
    COL_ITEM         = 1,
    COL_CHANCE       = 2,
    COL_QUEST        = 3,
    COL_LOOTMODE     = 4,
    COL_GROUPID      = 5,
    COL_MINCOUNT     = 6,
    COL_MAXCOUNT     = 7,
    COL_COMMENT      = 8,
    COL_COUNT        = 9,
};

// ItemType enum mirrors src/server/game/Loot/LootMgr.h Type.  We keep
// the labelling here so the editor surfaces the meaningful name in the
// table column without pulling in the worldserver header.
char const* itemTypeLabel(int t)
{
    switch (t)
    {
        case 0: return "Item";
        case 1: return "Reference";
        case 2: return "Currency";
        case 3: return "TrackingQuest";
        default: return "?";
    }
}

} // namespace

CreatureLootEditDialog::CreatureLootEditDialog(db::MySqlClient* dbClient,
                                               QString const& worldDbName,
                                               QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Creature loot editor"));
    setModal(true);
    resize(1100, 600);

    auto* outer = new QVBoxLayout(this);

    // -- Top filter row: creature entry spinner + Load + name label ----
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Creature entry:"), this));
    m_entrySpin = new QSpinBox(this);
    m_entrySpin->setRange(0, std::numeric_limits<int>::max());
    m_entrySpin->setValue(0);
    filterRow->addWidget(m_entrySpin);
    m_loadBtn = new QPushButton(tr("Load"), this);
    filterRow->addWidget(m_loadBtn);
    m_creatureLbl = new QLabel(tr("(no creature loaded)"), this);
    m_creatureLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    filterRow->addWidget(m_creatureLbl, 1);
    outer->addLayout(filterRow);

    // -- Main drops table --------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("ItemType"), tr("Item"), tr("Chance"), tr("QuestRequired"),
        tr("LootMode"), tr("GroupId"), tr("MinCount"), tr("MaxCount"),
        tr("Comment") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);  // ORDER BY GroupId, Chance DESC, Item is the canonical view.
    outer->addWidget(m_table, 1);

    // -- Action buttons ----------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add drop..."), this);
    m_editBtn   = new QPushButton(tr("Edit drop"), this);
    m_removeBtn = new QPushButton(tr("Remove drop"), this);
    m_lookupBtn = new QPushButton(tr("Lookup item template"), this);
    m_addBtn   ->setEnabled(false);  // requires a loaded creature
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_lookupBtn->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_lookupBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Enter a creature entry and press Load."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_loadBtn,    &QPushButton::clicked, this, &CreatureLootEditDialog::onLoad);
    connect(m_entrySpin,  qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int){ /* deferred to Load button to avoid spam queries */ });
    connect(m_addBtn,     &QPushButton::clicked, this, &CreatureLootEditDialog::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &CreatureLootEditDialog::onEdit);
    connect(m_removeBtn,  &QPushButton::clicked, this, &CreatureLootEditDialog::onRemove);
    connect(m_lookupBtn,  &QPushButton::clicked, this, &CreatureLootEditDialog::onLookupItem);
    connect(m_table,      &QTableWidget::itemSelectionChanged,
            this, &CreatureLootEditDialog::onSelectionChanged);
}

void CreatureLootEditDialog::onLoad()
{
    uint32_t const entry = static_cast<uint32_t>(m_entrySpin->value());
    m_loadedEntry = entry;
    refreshCreatureName(entry);
    loadDrops();
    bool const hasCreature = (entry != 0);
    m_addBtn->setEnabled(hasCreature);
    onSelectionChanged();
}

void CreatureLootEditDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0 && m_loadedEntry != 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
    m_lookupBtn->setEnabled(has);
}

bool CreatureLootEditDialog::currentRowItem(uint32_t& itemOut, int& itemTypeOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* itemCell = m_table->item(row, COL_ITEM);
    auto* typeCell = m_table->item(row, COL_ITEMTYPE);
    if (!itemCell) return false;
    itemOut     = static_cast<uint32_t>(itemCell->data(Qt::DisplayRole).toULongLong());
    // ItemType is stored as the raw integer in Qt::UserRole; the visible
    // text is the human label from itemTypeLabel().
    itemTypeOut = typeCell ? typeCell->data(Qt::UserRole).toInt() : 0;
    rowOut = row;
    return true;
}

void CreatureLootEditDialog::refreshCreatureName(uint32_t entry)
{
    if (!m_db || !m_db->isConnected())
    {
        m_creatureLbl->setText(tr("DB not connected."));
        return;
    }
    if (entry == 0)
    {
        m_creatureLbl->setText(tr("(no creature loaded)"));
        return;
    }

    // Try name1 (modern TC schema) first, fall back to name (older 3.3.5
    // schema) when the column is absent or the query errors.  Mirrors
    // NpcVendorDialog's defensive fallback.
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COALESCE(name1, '') FROM %s.creature_template WHERE entry=%u",
        m_worldDb.toStdString().c_str(), entry);
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok())
    {
        std::snprintf(sql, sizeof(sql),
            "SELECT COALESCE(name, '') FROM %s.creature_template WHERE entry=%u",
            m_worldDb.toStdString().c_str(), entry);
        err = m_db->query(sql, res);
    }
    if (!err.ok() || res.rowCount() == 0)
    {
        m_creatureLbl->setText(tr("%1 - (unknown / no creature_template row)").arg(entry));
        return;
    }
    QString const name = QString::fromStdString(res.cell(0, 0));
    m_creatureLbl->setText(tr("%1 - %2").arg(entry).arg(name));
}

void CreatureLootEditDialog::loadDrops()
{
    m_loading = true;
    m_table->setRowCount(0);

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }
    if (m_loadedEntry == 0)
    {
        m_statusLabel->setText(tr("No creature loaded."));
        m_loading = false;
        return;
    }

    char sql[768];
    std::snprintf(sql, sizeof(sql),
        "SELECT ItemType, Item, Chance, QuestRequired, LootMode, GroupId, "
        "       MinCount, MaxCount, COALESCE(Comment, '') "
        "FROM %s.creature_loot_template "
        "WHERE Entry=%u "
        "ORDER BY GroupId, Chance DESC, Item",
        m_worldDb.toStdString().c_str(), m_loadedEntry);
    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("creature_loot_template query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        int      const itemType = static_cast<int>(res.asUInt64(r, 0).value_or(0));
        uint32_t const item     = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(0));
        double   const chance   = res.asDouble(r, 2).value_or(0.0);
        uint32_t const quest    = static_cast<uint32_t>(res.asUInt64(r, 3).value_or(0));
        uint32_t const lootMode = static_cast<uint32_t>(res.asUInt64(r, 4).value_or(0));
        uint32_t const groupId  = static_cast<uint32_t>(res.asUInt64(r, 5).value_or(0));
        uint32_t const minCount = static_cast<uint32_t>(res.asUInt64(r, 6).value_or(0));
        uint32_t const maxCount = static_cast<uint32_t>(res.asUInt64(r, 7).value_or(0));
        QString  const comment  = QString::fromStdString(res.cell(r, 8));

        // ItemType uses a labelled text cell with the raw int parked in
        // UserRole so the operator sees "Item" / "Reference" / "Currency"
        // but Edit/Remove still recover the numeric value.
        {
            auto* c = new QTableWidgetItem(
                tr("%1 (%2)").arg(itemTypeLabel(itemType)).arg(itemType));
            c->setData(Qt::UserRole, itemType);
            m_table->setItem(int(r), COL_ITEMTYPE, c);
        }
        auto setU = [&](int col, uint32_t v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, qulonglong(v));
            m_table->setItem(int(r), col, c);
        };
        setU(COL_ITEM, item);
        {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, chance);
            m_table->setItem(int(r), COL_CHANCE, c);
        }
        setU(COL_QUEST,    quest);
        setU(COL_LOOTMODE, lootMode);
        setU(COL_GROUPID,  groupId);
        setU(COL_MINCOUNT, minCount);
        setU(COL_MAXCOUNT, maxCount);
        m_table->setItem(int(r), COL_COMMENT, new QTableWidgetItem(comment));
    }

    m_statusLabel->setText(tr("Entry=%1 rows=%2").arg(m_loadedEntry).arg(res.rowCount()));
    m_loading = false;
    onSelectionChanged();
}

bool CreatureLootEditDialog::runInTransaction(QStringList const& sqls, QString const& description)
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

void CreatureLootEditDialog::onAdd()
{
    openModal(std::numeric_limits<uint32_t>::max());
}

void CreatureLootEditDialog::onEdit()
{
    uint32_t item = 0;
    int itemType  = 0;
    int row = -1;
    if (!currentRowItem(item, itemType, row)) return;
    openModal(item);
}

void CreatureLootEditDialog::onRemove()
{
    uint32_t item = 0;
    int itemType  = 0;
    int row = -1;
    if (!currentRowItem(item, itemType, row)) return;

    auto const choice = QMessageBox::question(this, tr("Remove drop"),
        tr("Delete creature_loot_template row Entry=%1, ItemType=%2, Item=%3?")
            .arg(m_loadedEntry).arg(itemType).arg(item),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.creature_loot_template "
        "WHERE Entry=%2 AND ItemType=%3 AND Item=%4")
        .arg(m_worldDb).arg(m_loadedEntry).arg(itemType).arg(item);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE creature_loot_template (Entry=%1, ItemType=%2, Item=%3)")
                .arg(m_loadedEntry).arg(itemType).arg(item)))
        loadDrops();
}

void CreatureLootEditDialog::onLookupItem()
{
    uint32_t item = 0;
    int itemType  = 0;
    int row = -1;
    if (!currentRowItem(item, itemType, row)) return;

    // MainWindow does NOT today expose openItemInfoDock(); the spec
    // forbids introducing one.  Surface the request via qDebug + status
    // label so the operator can see it landed and route via the existing
    // LootTableDock workflow if needed.
    qDebug() << "CreatureLootEditDialog: lookup item template Entry="
             << m_loadedEntry << " Item=" << item;
    m_statusLabel->setText(tr("Lookup item template requested: Entry=%1 Item=%2 "
        "(open ItemInfoDock manually via spawn workflow)")
        .arg(m_loadedEntry).arg(item));
}

void CreatureLootEditDialog::openModal(uint32_t editingItem)
{
    bool const isEdit = (editingItem != std::numeric_limits<uint32_t>::max());

    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? tr("Edit drop") : tr("Add drop"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    // ItemType is part of the composite PK alongside Item, so disable it
    // on edit (operator deletes + re-adds to change the type).
    auto* itemTypeSpin = new QSpinBox(&dlg);
    itemTypeSpin->setRange(0, 3);
    itemTypeSpin->setValue(0);
    itemTypeSpin->setSuffix(tr("  (0=Item,1=Reference,2=Currency,3=TrackingQuest)"));
    itemTypeSpin->setEnabled(!isEdit);
    form->addRow(tr("ItemType:"), itemTypeSpin);

    auto* itemSpin = new QSpinBox(&dlg);
    itemSpin->setRange(0, std::numeric_limits<int>::max());
    itemSpin->setEnabled(!isEdit);  // composite PK Item is immutable post-INSERT.
    form->addRow(tr("Item:"), itemSpin);

    // Chance may legitimately be negative when expressed as -groupChance
    // for AE-grouped drops (TC convention).  Range -100..100 mirrors the
    // spec.
    auto* chanceSpin = new QDoubleSpinBox(&dlg);
    chanceSpin->setRange(-100.0, 100.0);
    chanceSpin->setDecimals(4);
    chanceSpin->setSingleStep(0.1);
    chanceSpin->setValue(100.0);
    form->addRow(tr("Chance:"), chanceSpin);

    auto* questCheck = new QCheckBox(&dlg);
    form->addRow(tr("QuestRequired:"), questCheck);

    auto* lootModeSpin = new QSpinBox(&dlg);
    lootModeSpin->setRange(0, 0xFFFF);
    lootModeSpin->setValue(1);  // TC default = LOOT_MODE_DEFAULT (1).
    form->addRow(tr("LootMode:"), lootModeSpin);

    auto* groupIdSpin = new QSpinBox(&dlg);
    groupIdSpin->setRange(0, 0xFF);
    form->addRow(tr("GroupId:"), groupIdSpin);

    auto* minCountSpin = new QSpinBox(&dlg);
    minCountSpin->setRange(0, std::numeric_limits<int>::max());
    minCountSpin->setValue(1);
    form->addRow(tr("MinCount:"), minCountSpin);

    auto* maxCountSpin = new QSpinBox(&dlg);
    maxCountSpin->setRange(0, std::numeric_limits<int>::max());
    maxCountSpin->setValue(1);
    form->addRow(tr("MaxCount:"), maxCountSpin);

    auto* commentEdit = new QLineEdit(&dlg);
    commentEdit->setMaxLength(255);
    form->addRow(tr("Comment:"), commentEdit);

    if (isEdit)
    {
        int const row = m_table->currentRow();
        if (row >= 0)
        {
            auto getU = [&](int col) -> uint32_t {
                auto* c = m_table->item(row, col);
                return c
                    ? static_cast<uint32_t>(c->data(Qt::DisplayRole).toULongLong())
                    : 0u;
            };
            auto getD = [&](int col) -> double {
                auto* c = m_table->item(row, col);
                return c ? c->data(Qt::DisplayRole).toDouble() : 0.0;
            };
            auto* typeCell = m_table->item(row, COL_ITEMTYPE);
            int const itemType = typeCell ? typeCell->data(Qt::UserRole).toInt() : 0;
            itemTypeSpin->setValue(itemType);
            itemSpin    ->setValue(static_cast<int>(editingItem));
            chanceSpin  ->setValue(getD(COL_CHANCE));
            questCheck  ->setChecked(getU(COL_QUEST) != 0);
            lootModeSpin->setValue(static_cast<int>(getU(COL_LOOTMODE)));
            groupIdSpin ->setValue(static_cast<int>(getU(COL_GROUPID)));
            minCountSpin->setValue(static_cast<int>(getU(COL_MINCOUNT)));
            maxCountSpin->setValue(static_cast<int>(getU(COL_MAXCOUNT)));
            auto* commentCell = m_table->item(row, COL_COMMENT);
            commentEdit ->setText(commentCell ? commentCell->text() : QString());
        }
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    if (minCountSpin->value() > maxCountSpin->value())
    {
        QMessageBox::warning(this, tr("Invalid count range"),
            tr("MinCount (%1) must not exceed MaxCount (%2).")
                .arg(minCountSpin->value()).arg(maxCountSpin->value()));
        return;
    }

    QString const escComment = QString::fromStdString(
        m_db->escapeString(commentEdit->text().toStdString()));
    QString const chanceStr = QString::number(chanceSpin->value(), 'f', 4);
    int      const itemType = itemTypeSpin->value();
    uint32_t const item     = static_cast<uint32_t>(itemSpin->value());
    int const quest         = questCheck->isChecked() ? 1 : 0;
    int const lootMode      = lootModeSpin->value();
    int const groupId       = groupIdSpin->value();
    int const minCount      = minCountSpin->value();
    int const maxCount      = maxCountSpin->value();

    if (isEdit)
    {
        // ItemType + Item are PK-pinned on edit; only the mutable fields
        // get rewritten.  Switching the type requires Remove + Add by
        // design (UI disables itemTypeSpin in edit mode).
        QString const upd = QStringLiteral(
            "UPDATE %1.creature_loot_template SET "
            "Chance=%2, QuestRequired=%3, LootMode=%4, "
            "GroupId=%5, MinCount=%6, MaxCount=%7, Comment='%8' "
            "WHERE Entry=%9 AND ItemType=%10 AND Item=%11")
            .arg(m_worldDb).arg(chanceStr).arg(quest).arg(lootMode)
            .arg(groupId).arg(minCount).arg(maxCount).arg(escComment)
            .arg(m_loadedEntry).arg(itemType).arg(item);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE creature_loot_template (Entry=%1, ItemType=%2, Item=%3)")
                    .arg(m_loadedEntry).arg(itemType).arg(item)))
            loadDrops();
    }
    else
    {
        // PK collision check.  INSERT would fail server-side but a
        // friendly QMessageBox beats a raw MySQL duplicate-key error.
        char checkSql[320];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.creature_loot_template "
            "WHERE Entry=%u AND ItemType=%d AND Item=%u",
            m_worldDb.toStdString().c_str(), m_loadedEntry, itemType, item);
        db::QueryResult cRes;
        auto err = m_db->query(checkSql, cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add drop"),
                tr("creature_loot_template row already exists for "
                   "Entry=%1, ItemType=%2, Item=%3.")
                    .arg(m_loadedEntry).arg(itemType).arg(item));
            return;
        }

        QString const ins = QStringLiteral(
            "INSERT INTO %1.creature_loot_template "
            "(Entry, ItemType, Item, Chance, QuestRequired, LootMode, "
            " GroupId, MinCount, MaxCount, Comment) "
            "VALUES (%2, %3, %4, %5, %6, %7, %8, %9, %10, '%11')")
            .arg(m_worldDb).arg(m_loadedEntry).arg(itemType).arg(item)
            .arg(chanceStr).arg(quest).arg(lootMode).arg(groupId)
            .arg(minCount).arg(maxCount).arg(escComment);
        if (runInTransaction(QStringList{ ins },
                tr("INSERT creature_loot_template (Entry=%1, ItemType=%2, Item=%3)")
                    .arg(m_loadedEntry).arg(itemType).arg(item)))
            loadDrops();
    }
}

} // namespace world_editor::app
