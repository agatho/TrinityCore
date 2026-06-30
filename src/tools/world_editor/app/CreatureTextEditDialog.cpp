#include "CreatureTextEditDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
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

// Column indices for the main texts table.  Single source of truth so the
// Add/Edit modal can read the selected row's cells off the rendered view
// without re-querying MySQL.
enum Col : int
{
    COL_GROUPID         = 0,
    COL_ID              = 1,
    COL_TEXT            = 2,
    COL_TYPE            = 3,
    COL_LANGUAGE        = 4,
    COL_PROBABILITY     = 5,
    COL_EMOTE           = 6,
    COL_DURATION        = 7,
    COL_SOUND           = 8,
    COL_BROADCASTTEXTID = 9,
    COL_TEXTRANGE       = 10,
    COL_COMMENT         = 11,
    COL_COUNT           = 12,
};

// ChatMsg subset that creature_text.Type maps onto.  Anything outside the
// known set renders as "Unknown (N)" so operators can still inspect & edit
// rows without us hiding the actual value.
struct TextType { uint32_t value; char const* label; };
constexpr TextType kKnownTypes[] = {
    { 12, "Say" },
    { 14, "Yell" },
    { 15, "TextEmote" },
    { 16, "Whisper" },
    { 41, "BossEmote" },
};

QString friendlyTypeLabel(uint32_t type)
{
    for (auto const& t : kKnownTypes)
        if (t.value == type)
            return QStringLiteral("%1 (%2)").arg(t.label).arg(type);
    return QStringLiteral("Unknown (%1)").arg(type);
}

} // namespace

CreatureTextEditDialog::CreatureTextEditDialog(db::MySqlClient* dbClient,
                                               QString const& worldDbName,
                                               QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Creature text editor"));
    setModal(true);
    resize(1200, 600);

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

    // -- Main texts table --------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("GroupID"), tr("ID"), tr("Text"), tr("Type"), tr("Language"),
        tr("Probability"), tr("Emote"), tr("Duration"), tr("Sound"),
        tr("BroadcastTextId"), tr("TextRange"), tr("Comment") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);  // ORDER BY GroupID, ID is the canonical view.
    outer->addWidget(m_table, 1);

    // -- Action buttons ----------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn      = new QPushButton(tr("Add text..."), this);
    m_editBtn     = new QPushButton(tr("Edit text"), this);
    m_removeBtn   = new QPushButton(tr("Remove text"), this);
    m_newGroupBtn = new QPushButton(tr("New group"), this);
    m_addBtn     ->setEnabled(false);
    m_editBtn    ->setEnabled(false);
    m_removeBtn  ->setEnabled(false);
    m_newGroupBtn->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_newGroupBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Enter a creature entry and press Load."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_loadBtn,     &QPushButton::clicked, this, &CreatureTextEditDialog::onLoad);
    connect(m_addBtn,      &QPushButton::clicked, this, &CreatureTextEditDialog::onAdd);
    connect(m_editBtn,     &QPushButton::clicked, this, &CreatureTextEditDialog::onEdit);
    connect(m_removeBtn,   &QPushButton::clicked, this, &CreatureTextEditDialog::onRemove);
    connect(m_newGroupBtn, &QPushButton::clicked, this, &CreatureTextEditDialog::onNewGroup);
    connect(m_table,       &QTableWidget::itemSelectionChanged,
            this, &CreatureTextEditDialog::onSelectionChanged);
}

void CreatureTextEditDialog::onLoad()
{
    uint32_t const entry = static_cast<uint32_t>(m_entrySpin->value());
    m_loadedEntry = entry;
    refreshCreatureName(entry);
    loadTexts();
    bool const hasCreature = (entry != 0);
    m_addBtn     ->setEnabled(hasCreature);
    m_newGroupBtn->setEnabled(hasCreature);
    onSelectionChanged();
}

void CreatureTextEditDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0 && m_loadedEntry != 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
}

bool CreatureTextEditDialog::currentRowKey(uint32_t& groupOut, uint32_t& idOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* groupCell = m_table->item(row, COL_GROUPID);
    auto* idCell    = m_table->item(row, COL_ID);
    if (!groupCell || !idCell) return false;
    groupOut = static_cast<uint32_t>(groupCell->data(Qt::DisplayRole).toULongLong());
    idOut    = static_cast<uint32_t>(idCell   ->data(Qt::DisplayRole).toULongLong());
    rowOut   = row;
    return true;
}

void CreatureTextEditDialog::refreshCreatureName(uint32_t entry)
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
    // schema) when the column is absent or the query errors.  Mirrors the
    // sibling CreatureLootEditDialog defensive fallback.
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

void CreatureTextEditDialog::loadTexts()
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
        "SELECT GroupID, ID, COALESCE(Text, ''), Type, Language, Probability, "
        "       Emote, Duration, Sound, BroadcastTextId, TextRange, "
        "       COALESCE(comment, '') "
        "FROM %s.creature_text "
        "WHERE CreatureID=%u "
        "ORDER BY GroupID, ID",
        m_worldDb.toStdString().c_str(), m_loadedEntry);
    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("creature_text query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const groupId    = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        uint32_t const id         = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(0));
        QString  const text       = QString::fromStdString(res.cell(r, 2));
        uint32_t const type       = static_cast<uint32_t>(res.asUInt64(r, 3).value_or(0));
        uint32_t const language   = static_cast<uint32_t>(res.asUInt64(r, 4).value_or(0));
        double   const prob       = res.asDouble(r, 5).value_or(0.0);
        uint32_t const emote      = static_cast<uint32_t>(res.asUInt64(r, 6).value_or(0));
        uint32_t const duration   = static_cast<uint32_t>(res.asUInt64(r, 7).value_or(0));
        uint32_t const sound      = static_cast<uint32_t>(res.asUInt64(r, 8).value_or(0));
        uint32_t const bcastText  = static_cast<uint32_t>(res.asUInt64(r, 9).value_or(0));
        uint32_t const textRange  = static_cast<uint32_t>(res.asUInt64(r, 10).value_or(0));
        QString  const comment    = QString::fromStdString(res.cell(r, 11));

        auto setU = [&](int col, uint32_t v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, qulonglong(v));
            m_table->setItem(int(r), col, c);
        };
        setU(COL_GROUPID, groupId);
        setU(COL_ID,      id);
        m_table->setItem(int(r), COL_TEXT, new QTableWidgetItem(text));
        {
            // Friendly label in DisplayRole, raw type kept in UserRole so
            // edit-mode can pre-populate the combobox accurately.
            auto* c = new QTableWidgetItem(friendlyTypeLabel(type));
            c->setData(Qt::UserRole, qulonglong(type));
            m_table->setItem(int(r), COL_TYPE, c);
        }
        setU(COL_LANGUAGE, language);
        {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, prob);
            m_table->setItem(int(r), COL_PROBABILITY, c);
        }
        setU(COL_EMOTE,           emote);
        setU(COL_DURATION,        duration);
        setU(COL_SOUND,           sound);
        setU(COL_BROADCASTTEXTID, bcastText);
        setU(COL_TEXTRANGE,       textRange);
        m_table->setItem(int(r), COL_COMMENT, new QTableWidgetItem(comment));
    }

    m_statusLabel->setText(tr("CreatureID=%1 rows=%2").arg(m_loadedEntry).arg(res.rowCount()));
    m_loading = false;
    onSelectionChanged();
}

bool CreatureTextEditDialog::runInTransaction(QStringList const& sqls, QString const& description)
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

uint32_t CreatureTextEditDialog::nextFreeGroupId(uint32_t creatureId)
{
    if (!m_db || !m_db->isConnected() || creatureId == 0)
        return 0;

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COALESCE(MAX(GroupID), -1) FROM %s.creature_text WHERE CreatureID=%u",
        m_worldDb.toStdString().c_str(), creatureId);
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return 0;

    // COALESCE(MAX(...), -1) maps the empty-table case to "next = 0".
    auto const maxOpt = res.asInt64(0, 0);
    int64_t const maxVal = maxOpt.value_or(-1);
    int64_t const next = maxVal + 1;
    if (next < 0 || next > 255)
        return 0;
    return static_cast<uint32_t>(next);
}

void CreatureTextEditDialog::onAdd()
{
    openModal(false, 0, 0);
}

void CreatureTextEditDialog::onEdit()
{
    uint32_t group = 0;
    uint32_t id    = 0;
    int row = -1;
    if (!currentRowKey(group, id, row)) return;
    openModal(true, group, id);
}

void CreatureTextEditDialog::onRemove()
{
    uint32_t group = 0;
    uint32_t id    = 0;
    int row = -1;
    if (!currentRowKey(group, id, row)) return;

    auto const choice = QMessageBox::question(this, tr("Remove text"),
        tr("Delete creature_text row CreatureID=%1, GroupID=%2, ID=%3?")
            .arg(m_loadedEntry).arg(group).arg(id),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.creature_text WHERE CreatureID=%2 AND GroupID=%3 AND ID=%4")
        .arg(m_worldDb).arg(m_loadedEntry).arg(group).arg(id);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE creature_text (CreatureID=%1, GroupID=%2, ID=%3)")
                .arg(m_loadedEntry).arg(group).arg(id)))
        loadTexts();
}

void CreatureTextEditDialog::onNewGroup()
{
    if (m_loadedEntry == 0) return;

    uint32_t const newGroup = nextFreeGroupId(m_loadedEntry);
    // Probe row seeds the bucket; operator edits Text/Type afterwards via
    // the Edit button.  Defaults mirror TC's "Say, neutral lang, 100%" path.
    QString const ins = QStringLiteral(
        "INSERT INTO %1.creature_text "
        "(CreatureID, GroupID, ID, Text, Type, Language, Probability, "
        " Emote, Duration, Sound, BroadcastTextId, TextRange, comment) "
        "VALUES (%2, %3, 0, '', 12, 0, 100.0, 0, 0, 0, 0, 0, 'world_editor: new group')")
        .arg(m_worldDb).arg(m_loadedEntry).arg(newGroup);
    if (runInTransaction(QStringList{ ins },
            tr("INSERT creature_text (new GroupID=%1)").arg(newGroup)))
        loadTexts();
}

void CreatureTextEditDialog::openModal(bool editing, uint32_t editingGroup, uint32_t editingId)
{
    QDialog dlg(this);
    dlg.setWindowTitle(editing ? tr("Edit text") : tr("Add text"));
    dlg.setModal(true);
    dlg.resize(600, 500);

    auto* outerL = new QVBoxLayout(&dlg);
    auto* form   = new QFormLayout;
    outerL->addLayout(form);

    auto* groupSpin = new QSpinBox(&dlg);
    groupSpin->setRange(0, 255);
    groupSpin->setEnabled(!editing);   // composite PK GroupID is immutable post-INSERT.
    form->addRow(tr("GroupID:"), groupSpin);

    auto* idSpin = new QSpinBox(&dlg);
    idSpin->setRange(0, 255);
    idSpin->setEnabled(!editing);      // composite PK ID is immutable post-INSERT.
    form->addRow(tr("ID:"), idSpin);

    auto* textEdit = new QPlainTextEdit(&dlg);
    textEdit->setMinimumHeight(120);
    form->addRow(tr("Text:"), textEdit);

    auto* typeCombo = new QComboBox(&dlg);
    for (auto const& t : kKnownTypes)
        typeCombo->addItem(QStringLiteral("%1 (%2)").arg(t.label).arg(t.value),
                           qulonglong(t.value));
    form->addRow(tr("Type:"), typeCombo);

    auto* langSpin = new QSpinBox(&dlg);
    langSpin->setRange(0, 255);
    form->addRow(tr("Language:"), langSpin);

    auto* probSpin = new QDoubleSpinBox(&dlg);
    probSpin->setRange(0.0, 100.0);
    probSpin->setDecimals(2);
    probSpin->setSingleStep(1.0);
    probSpin->setValue(100.0);
    form->addRow(tr("Probability:"), probSpin);

    auto* emoteSpin = new QSpinBox(&dlg);
    emoteSpin->setRange(0, std::numeric_limits<int>::max());
    form->addRow(tr("Emote:"), emoteSpin);

    auto* durationSpin = new QSpinBox(&dlg);
    durationSpin->setRange(0, std::numeric_limits<int>::max());
    form->addRow(tr("Duration:"), durationSpin);

    auto* soundSpin = new QSpinBox(&dlg);
    soundSpin->setRange(0, std::numeric_limits<int>::max());
    form->addRow(tr("Sound:"), soundSpin);

    auto* bcastSpin = new QSpinBox(&dlg);
    bcastSpin->setRange(0, std::numeric_limits<int>::max());
    form->addRow(tr("BroadcastTextId:"), bcastSpin);

    auto* rangeSpin = new QSpinBox(&dlg);
    rangeSpin->setRange(0, 255);
    form->addRow(tr("TextRange:"), rangeSpin);

    auto* commentEdit = new QLineEdit(&dlg);
    commentEdit->setMaxLength(255);
    form->addRow(tr("Comment:"), commentEdit);

    auto setTypeCombo = [&](uint32_t typeValue) {
        for (int i = 0; i < typeCombo->count(); ++i)
        {
            if (typeCombo->itemData(i).toULongLong() == qulonglong(typeValue))
            {
                typeCombo->setCurrentIndex(i);
                return;
            }
        }
        // Type isn't in the known set - append a one-shot "Unknown (N)"
        // entry so the operator can save the row without losing the value.
        typeCombo->addItem(QStringLiteral("Unknown (%1)").arg(typeValue),
                           qulonglong(typeValue));
        typeCombo->setCurrentIndex(typeCombo->count() - 1);
    };

    if (editing)
    {
        int const row = m_table->currentRow();
        if (row >= 0)
        {
            auto getU = [&](int col) -> uint32_t {
                auto* c = m_table->item(row, col);
                return c ? static_cast<uint32_t>(c->data(Qt::DisplayRole).toULongLong()) : 0u;
            };
            auto getD = [&](int col) -> double {
                auto* c = m_table->item(row, col);
                return c ? c->data(Qt::DisplayRole).toDouble() : 0.0;
            };
            groupSpin   ->setValue(static_cast<int>(editingGroup));
            idSpin      ->setValue(static_cast<int>(editingId));
            auto* textCell = m_table->item(row, COL_TEXT);
            textEdit    ->setPlainText(textCell ? textCell->text() : QString());
            uint32_t const typeVal = m_table->item(row, COL_TYPE)
                ? static_cast<uint32_t>(m_table->item(row, COL_TYPE)->data(Qt::UserRole).toULongLong())
                : 0u;
            setTypeCombo(typeVal);
            langSpin    ->setValue(static_cast<int>(getU(COL_LANGUAGE)));
            probSpin    ->setValue(getD(COL_PROBABILITY));
            emoteSpin   ->setValue(static_cast<int>(getU(COL_EMOTE)));
            durationSpin->setValue(static_cast<int>(getU(COL_DURATION)));
            soundSpin   ->setValue(static_cast<int>(getU(COL_SOUND)));
            bcastSpin   ->setValue(static_cast<int>(getU(COL_BROADCASTTEXTID)));
            rangeSpin   ->setValue(static_cast<int>(getU(COL_TEXTRANGE)));
            auto* commentCell = m_table->item(row, COL_COMMENT);
            commentEdit ->setText(commentCell ? commentCell->text() : QString());
        }
    }
    else
    {
        // Default Type = Say (12) per the most common bucket on first INSERT.
        setTypeCombo(12);
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    outerL->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    uint32_t const groupId    = static_cast<uint32_t>(groupSpin->value());
    uint32_t const rowId      = static_cast<uint32_t>(idSpin->value());
    uint32_t const typeVal    = static_cast<uint32_t>(typeCombo->currentData().toULongLong());
    uint32_t const language   = static_cast<uint32_t>(langSpin->value());
    double   const probValue  = probSpin->value();
    uint32_t const emote      = static_cast<uint32_t>(emoteSpin->value());
    uint32_t const duration   = static_cast<uint32_t>(durationSpin->value());
    uint32_t const sound      = static_cast<uint32_t>(soundSpin->value());
    uint32_t const bcastText  = static_cast<uint32_t>(bcastSpin->value());
    uint32_t const textRange  = static_cast<uint32_t>(rangeSpin->value());

    QString const escText    = QString::fromStdString(
        m_db->escapeString(textEdit->toPlainText().toStdString()));
    QString const escComment = QString::fromStdString(
        m_db->escapeString(commentEdit->text().toStdString()));
    QString const probStr    = QString::number(probValue, 'f', 4);

    if (editing)
    {
        QString const upd = QStringLiteral(
            "UPDATE %1.creature_text SET "
            "Text='%2', Type=%3, Language=%4, Probability=%5, Emote=%6, "
            "Duration=%7, Sound=%8, BroadcastTextId=%9, TextRange=%10, comment='%11' "
            "WHERE CreatureID=%12 AND GroupID=%13 AND ID=%14")
            .arg(m_worldDb).arg(escText).arg(typeVal).arg(language).arg(probStr)
            .arg(emote).arg(duration).arg(sound).arg(bcastText).arg(textRange)
            .arg(escComment).arg(m_loadedEntry).arg(editingGroup).arg(editingId);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE creature_text (CreatureID=%1, GroupID=%2, ID=%3)")
                    .arg(m_loadedEntry).arg(editingGroup).arg(editingId)))
            loadTexts();
    }
    else
    {
        // PK collision check: friendly QMessageBox beats a raw duplicate-key
        // error from MySQL.
        char checkSql[384];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.creature_text "
            "WHERE CreatureID=%u AND GroupID=%u AND ID=%u",
            m_worldDb.toStdString().c_str(), m_loadedEntry, groupId, rowId);
        db::QueryResult cRes;
        auto err = m_db->query(checkSql, cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add text"),
                tr("creature_text row already exists for CreatureID=%1, GroupID=%2, ID=%3.")
                    .arg(m_loadedEntry).arg(groupId).arg(rowId));
            return;
        }

        QString const ins = QStringLiteral(
            "INSERT INTO %1.creature_text "
            "(CreatureID, GroupID, ID, Text, Type, Language, Probability, "
            " Emote, Duration, Sound, BroadcastTextId, TextRange, comment) "
            "VALUES (%2, %3, %4, '%5', %6, %7, %8, %9, %10, %11, %12, %13, '%14')")
            .arg(m_worldDb).arg(m_loadedEntry).arg(groupId).arg(rowId)
            .arg(escText).arg(typeVal).arg(language).arg(probStr).arg(emote)
            .arg(duration).arg(sound).arg(bcastText).arg(textRange).arg(escComment);
        if (runInTransaction(QStringList{ ins },
                tr("INSERT creature_text (CreatureID=%1, GroupID=%2, ID=%3)")
                    .arg(m_loadedEntry).arg(groupId).arg(rowId)))
            loadTexts();
    }
}

} // namespace world_editor::app
