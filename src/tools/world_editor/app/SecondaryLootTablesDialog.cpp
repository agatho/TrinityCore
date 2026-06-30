#include "SecondaryLootTablesDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
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

#include <array>
#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Column indices for the main drops table - mirrors CreatureLootEditDialog
// so the Add/Edit modal can read the rendered row directly.
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

// ItemType labels mirror src/server/game/Loot/LootMgr.h Type enum.
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

// Fixed allow-list of supported tables.  The QComboBox is initialised from
// this array and nothing else is ever substituted into the SQL.  Order is
// preserved in the UI.
constexpr std::array<char const*, 10> kSecondaryLootTables = {
    "gameobject_loot_template",
    "item_loot_template",
    "skinning_loot_template",
    "fishing_loot_template",
    "pickpocketing_loot_template",
    "disenchant_loot_template",
    "milling_loot_template",
    "prospecting_loot_template",
    "reference_loot_template",
    "mail_loot_template",
};

// Operator-facing description of what `Entry` means for the selected table.
char const* entryMeaningFor(QString const& table)
{
    if (table == "gameobject_loot_template")    return "gameobject_template.entry";
    if (table == "item_loot_template")          return "item_template.entry (container)";
    if (table == "skinning_loot_template")      return "creature_template.entry (skinnable)";
    if (table == "fishing_loot_template")       return "area id (fishing zone)";
    if (table == "pickpocketing_loot_template") return "creature_template.entry";
    if (table == "disenchant_loot_template")    return "disenchant id";
    if (table == "milling_loot_template")       return "herb item entry";
    if (table == "prospecting_loot_template")   return "ore item entry";
    if (table == "reference_loot_template")     return "reference id (fan-out target)";
    if (table == "mail_loot_template")          return "mail loot id";
    return "entry";
}

// Verify a candidate string is in the fixed allow-list before it goes
// anywhere near SQL.  All public callers feed this from m_tableCombo, so
// this is a defence-in-depth assertion rather than a runtime gate.
bool isAllowedTable(QString const& t)
{
    for (char const* name : kSecondaryLootTables)
        if (t == QLatin1String(name))
            return true;
    return false;
}

} // namespace

SecondaryLootTablesDialog::SecondaryLootTablesDialog(db::MySqlClient* dbClient,
                                                     QString const& worldDbName,
                                                     QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Secondary loot tables editor"));
    setModal(true);
    resize(1100, 600);

    auto* outer = new QVBoxLayout(this);

    // -- Table picker row --------------------------------------------
    auto* tableRow = new QHBoxLayout;
    tableRow->addWidget(new QLabel(tr("Loot table:"), this));
    m_tableCombo = new QComboBox(this);
    for (char const* name : kSecondaryLootTables)
        m_tableCombo->addItem(QString::fromLatin1(name));
    tableRow->addWidget(m_tableCombo, 1);
    outer->addLayout(tableRow);

    // -- Entry filter row --------------------------------------------
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Loot entry:"), this));
    m_entrySpin = new QSpinBox(this);
    m_entrySpin->setRange(0, std::numeric_limits<int>::max());
    m_entrySpin->setValue(0);
    filterRow->addWidget(m_entrySpin);
    m_loadBtn = new QPushButton(tr("Load"), this);
    filterRow->addWidget(m_loadBtn);
    m_entryLbl = new QLabel(this);
    m_entryLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    filterRow->addWidget(m_entryLbl, 1);
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
    m_table->setSortingEnabled(false);
    outer->addWidget(m_table, 1);

    // -- Action buttons ----------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add drop..."), this);
    m_editBtn   = new QPushButton(tr("Edit drop"), this);
    m_removeBtn = new QPushButton(tr("Remove drop"), this);
    m_addBtn   ->setEnabled(false);
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_tableCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SecondaryLootTablesDialog::onTableChanged);
    connect(m_loadBtn,    &QPushButton::clicked, this, &SecondaryLootTablesDialog::onLoad);
    connect(m_addBtn,     &QPushButton::clicked, this, &SecondaryLootTablesDialog::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &SecondaryLootTablesDialog::onEdit);
    connect(m_removeBtn,  &QPushButton::clicked, this, &SecondaryLootTablesDialog::onRemove);
    connect(m_table,      &QTableWidget::itemSelectionChanged,
            this, &SecondaryLootTablesDialog::onSelectionChanged);

    // Prime the entry-meaning label + status from initial combo selection.
    onTableChanged(m_tableCombo->currentIndex());
}

QString SecondaryLootTablesDialog::currentTable() const
{
    QString const t = m_tableCombo ? m_tableCombo->currentText() : QString();
    // Defence-in-depth: the combo is populated only from the allow-list,
    // but if anything ever mutates the model unexpectedly we hard-fail
    // back to the first known-good name rather than substitute garbage.
    if (!isAllowedTable(t))
        return QString::fromLatin1(kSecondaryLootTables.front());
    return t;
}

void SecondaryLootTablesDialog::onTableChanged(int /*idx*/)
{
    m_loadedEntry = 0;
    m_table->setRowCount(0);
    m_addBtn->setEnabled(false);
    onSelectionChanged();
    m_entryLbl->setText(tr("(no entry loaded; Entry = %1)").arg(entryMeaningFor(currentTable())));
    m_statusLabel->setText(tr("Selected table: %1 - enter an Entry and press Load.")
        .arg(currentTable()));
}

void SecondaryLootTablesDialog::onLoad()
{
    uint32_t const entry = static_cast<uint32_t>(m_entrySpin->value());
    m_loadedEntry = entry;
    if (entry == 0)
        m_entryLbl->setText(tr("(no entry loaded; Entry = %1)").arg(entryMeaningFor(currentTable())));
    else
        m_entryLbl->setText(tr("%1 = %2").arg(entryMeaningFor(currentTable())).arg(entry));
    loadDrops();
    m_addBtn->setEnabled(entry != 0);
    onSelectionChanged();
}

void SecondaryLootTablesDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0 && m_loadedEntry != 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
}

bool SecondaryLootTablesDialog::currentRowItem(uint32_t& itemOut, int& itemTypeOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* itemCell = m_table->item(row, COL_ITEM);
    auto* typeCell = m_table->item(row, COL_ITEMTYPE);
    if (!itemCell) return false;
    itemOut     = static_cast<uint32_t>(itemCell->data(Qt::DisplayRole).toULongLong());
    itemTypeOut = typeCell ? typeCell->data(Qt::UserRole).toInt() : 0;
    rowOut = row;
    return true;
}

void SecondaryLootTablesDialog::loadDrops()
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
        m_statusLabel->setText(tr("No entry loaded."));
        m_loading = false;
        return;
    }

    QString const table = currentTable();

    char sql[768];
    std::snprintf(sql, sizeof(sql),
        "SELECT ItemType, Item, Chance, QuestRequired, LootMode, GroupId, "
        "       MinCount, MaxCount, COALESCE(Comment, '') "
        "FROM %s.%s "
        "WHERE Entry=%u "
        "ORDER BY GroupId, Chance DESC, Item",
        m_worldDb.toStdString().c_str(), table.toStdString().c_str(), m_loadedEntry);
    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("%1 query failed: %2")
            .arg(table).arg(QString::fromStdString(err.message)));
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

    m_statusLabel->setText(tr("Table=%1 Entry=%2 rows=%3")
        .arg(table).arg(m_loadedEntry).arg(res.rowCount()));
    m_loading = false;
    onSelectionChanged();
}

bool SecondaryLootTablesDialog::runInTransaction(QStringList const& sqls,
                                                 QString const& description)
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

void SecondaryLootTablesDialog::onAdd()
{
    openModal(std::numeric_limits<uint32_t>::max());
}

void SecondaryLootTablesDialog::onEdit()
{
    uint32_t item = 0;
    int itemType  = 0;
    int row = -1;
    if (!currentRowItem(item, itemType, row)) return;
    openModal(item);
}

void SecondaryLootTablesDialog::onRemove()
{
    uint32_t item = 0;
    int itemType  = 0;
    int row = -1;
    if (!currentRowItem(item, itemType, row)) return;

    QString const table = currentTable();

    auto const choice = QMessageBox::question(this, tr("Remove drop"),
        tr("Delete %1 row Entry=%2, ItemType=%3, Item=%4?")
            .arg(table).arg(m_loadedEntry).arg(itemType).arg(item),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.%2 "
        "WHERE Entry=%3 AND ItemType=%4 AND Item=%5")
        .arg(m_worldDb).arg(table).arg(m_loadedEntry).arg(itemType).arg(item);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE %1 (Entry=%2, ItemType=%3, Item=%4)")
                .arg(table).arg(m_loadedEntry).arg(itemType).arg(item)))
        loadDrops();
}

void SecondaryLootTablesDialog::openModal(uint32_t editingItem)
{
    bool const isEdit = (editingItem != std::numeric_limits<uint32_t>::max());
    QString const table = currentTable();

    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? tr("Edit drop - %1").arg(table)
                              : tr("Add drop - %1").arg(table));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    // ItemType and Item are pinned on edit because they belong to the
    // composite PK (Entry, ItemType, Item).
    auto* itemTypeSpin = new QSpinBox(&dlg);
    itemTypeSpin->setRange(0, 3);
    itemTypeSpin->setValue(0);
    itemTypeSpin->setSuffix(tr("  (0=Item,1=Reference,2=Currency,3=TrackingQuest)"));
    itemTypeSpin->setEnabled(!isEdit);
    form->addRow(tr("ItemType:"), itemTypeSpin);

    auto* itemSpin = new QSpinBox(&dlg);
    itemSpin->setRange(0, std::numeric_limits<int>::max());
    itemSpin->setEnabled(!isEdit);
    form->addRow(tr("Item:"), itemSpin);

    // Chance can be negative (TC convention for grouped drops where the
    // absolute value encodes the group share).
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
    lootModeSpin->setValue(1);
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
        QString const upd = QStringLiteral(
            "UPDATE %1.%2 SET "
            "Chance=%3, QuestRequired=%4, LootMode=%5, "
            "GroupId=%6, MinCount=%7, MaxCount=%8, Comment='%9' "
            "WHERE Entry=%10 AND ItemType=%11 AND Item=%12")
            .arg(m_worldDb).arg(table).arg(chanceStr).arg(quest).arg(lootMode)
            .arg(groupId).arg(minCount).arg(maxCount).arg(escComment)
            .arg(m_loadedEntry).arg(itemType).arg(item);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE %1 (Entry=%2, ItemType=%3, Item=%4)")
                    .arg(table).arg(m_loadedEntry).arg(itemType).arg(item)))
            loadDrops();
    }
    else
    {
        // PK pre-check: friendlier than a raw duplicate-key error.
        char checkSql[384];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.%s "
            "WHERE Entry=%u AND ItemType=%d AND Item=%u",
            m_worldDb.toStdString().c_str(), table.toStdString().c_str(),
            m_loadedEntry, itemType, item);
        db::QueryResult cRes;
        auto err = m_db->query(checkSql, cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add drop"),
                tr("%1 row already exists for Entry=%2, ItemType=%3, Item=%4.")
                    .arg(table).arg(m_loadedEntry).arg(itemType).arg(item));
            return;
        }

        QString const ins = QStringLiteral(
            "INSERT INTO %1.%2 "
            "(Entry, ItemType, Item, Chance, QuestRequired, LootMode, "
            " GroupId, MinCount, MaxCount, Comment) "
            "VALUES (%3, %4, %5, %6, %7, %8, %9, %10, %11, '%12')")
            .arg(m_worldDb).arg(table).arg(m_loadedEntry).arg(itemType).arg(item)
            .arg(chanceStr).arg(quest).arg(lootMode).arg(groupId)
            .arg(minCount).arg(maxCount).arg(escComment);
        if (runInTransaction(QStringList{ ins },
                tr("INSERT %1 (Entry=%2, ItemType=%3, Item=%4)")
                    .arg(table).arg(m_loadedEntry).arg(itemType).arg(item)))
            loadDrops();
    }
}

} // namespace world_editor::app
