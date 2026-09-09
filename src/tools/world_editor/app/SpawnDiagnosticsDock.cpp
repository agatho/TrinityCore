#include "SpawnDiagnosticsDock.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cstdio>

namespace world_editor::app
{

namespace
{

QTableWidget* makeTable(QWidget* parent, QStringList const& headers)
{
    auto* t = new QTableWidget(parent);
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->setAlternatingRowColors(true);
    return t;
}

void fillTable(QTableWidget* t, db::QueryResult const& res)
{
    t->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        for (size_t c = 0; c < res.columnCount(); ++c)
        {
            QString const text = QString::fromStdString(res.cell(r, c));
            t->setItem(int(r), int(c), new QTableWidgetItem(text));
        }
    }
}

void setPlaceholder(QTableWidget* t, QString const& msg)
{
    t->setRowCount(1);
    t->setColumnCount(1);
    t->setHorizontalHeaderLabels({ msg });
    t->setItem(0, 0, new QTableWidgetItem(QString{}));
}

void restoreHeaders(QTableWidget* t, QStringList const& headers)
{
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
}

} // namespace

SpawnDiagnosticsDock::SpawnDiagnosticsDock(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    m_headerLabel = new QLabel(tr("(no spawn selected)"), this);
    m_headerLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    outer->addWidget(m_headerLabel);

    m_tabs       = new QTabWidget(this);
    m_smartTable = makeTable(this, {
        tr("entryorguid"), tr("src"), tr("id"), tr("link"),
        tr("event"), tr("action"), tr("target"), tr("comment") });
    m_linkTable  = makeTable(this, { tr("guid"), tr("linkedGuid"), tr("linkType") });
    m_eventTable = makeTable(this, { tr("eventEntry"), tr("guid") });
    m_transTable = makeTable(this, {
        tr("guid"), tr("entry"), tr("name"), tr("ScriptName") });

    // SmartScripts + LinkedRespawn + GameEvent get action bars above
    // their tables so the operator can add/edit/remove rows without
    // dropping to mysql client.  Transport stays read-only because
    // transport rows have implications that the editor doesn't yet
    // know how to express safely.
    auto wrapWithToolbar = [&](QTableWidget* table,
                               std::initializer_list<QPushButton*> btns) -> QWidget*
    {
        auto* w = new QWidget(this);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        auto* h = new QHBoxLayout;
        for (auto* btn : btns) h->addWidget(btn);
        h->addStretch(1);
        v->addLayout(h);
        v->addWidget(table, 1);
        return w;
    };

    auto* addSmartBtn    = new QPushButton(tr("Add row..."),    this);
    auto* editSmartBtn   = new QPushButton(tr("Edit selected..."), this);
    auto* removeSmartBtn = new QPushButton(tr("Remove selected"),  this);
    auto* smartTab = wrapWithToolbar(m_smartTable, { addSmartBtn, editSmartBtn, removeSmartBtn });

    auto* addLinkBtn    = new QPushButton(tr("Add link..."),    this);
    auto* removeLinkBtn = new QPushButton(tr("Remove selected"), this);
    auto* linkTab = wrapWithToolbar(m_linkTable, { addLinkBtn, removeLinkBtn });

    auto* addEventBtn    = new QPushButton(tr("Add to event..."), this);
    auto* removeEventBtn = new QPushButton(tr("Remove selected"), this);
    auto* eventTab = wrapWithToolbar(m_eventTable, { addEventBtn, removeEventBtn });

    auto* addTransBtn    = new QPushButton(tr("Add row..."),    this);
    auto* editTransBtn   = new QPushButton(tr("Edit selected..."), this);
    auto* removeTransBtn = new QPushButton(tr("Remove selected"),  this);
    auto* transTab = wrapWithToolbar(m_transTable, { addTransBtn, editTransBtn, removeTransBtn });

    m_tabs->addTab(smartTab,     tr("SmartScripts"));
    m_tabs->addTab(linkTab,      tr("LinkedRespawn"));
    m_tabs->addTab(eventTab,     tr("GameEvent"));
    m_tabs->addTab(transTab,     tr("Transport"));
    outer->addWidget(m_tabs, 1);

    connect(addTransBtn,    &QPushButton::clicked, this, &SpawnDiagnosticsDock::onAddTransportClicked);
    connect(editTransBtn,   &QPushButton::clicked, this, &SpawnDiagnosticsDock::onEditTransportClicked);
    connect(removeTransBtn, &QPushButton::clicked, this, &SpawnDiagnosticsDock::onRemoveTransportClicked);

    connect(addSmartBtn,    &QPushButton::clicked, this, &SpawnDiagnosticsDock::onAddSmartScriptClicked);
    connect(editSmartBtn,   &QPushButton::clicked, this, &SpawnDiagnosticsDock::onEditSmartScriptClicked);
    connect(removeSmartBtn, &QPushButton::clicked, this, &SpawnDiagnosticsDock::onRemoveSmartScriptClicked);
    connect(addLinkBtn,    &QPushButton::clicked, this, &SpawnDiagnosticsDock::onAddLinkedRespawnClicked);
    connect(removeLinkBtn, &QPushButton::clicked, this, &SpawnDiagnosticsDock::onRemoveLinkedRespawnClicked);
    connect(addEventBtn,   &QPushButton::clicked, this, &SpawnDiagnosticsDock::onAddGameEventClicked);
    connect(removeEventBtn,&QPushButton::clicked, this, &SpawnDiagnosticsDock::onRemoveGameEventClicked);
}

void SpawnDiagnosticsDock::clear()
{
    m_hasSelection = false;
    m_headerLabel->setText(tr("(no spawn selected)"));
    m_headerLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    m_smartTable->setRowCount(0);
    m_linkTable->setRowCount(0);
    m_eventTable->setRowCount(0);
    m_transTable->setRowCount(0);
}

void SpawnDiagnosticsDock::refresh()
{
    if (m_hasSelection)
        setSelection(m_kind, m_guid, m_entry);
}

void SpawnDiagnosticsDock::setSelection(render::SpawnKind kind, int64_t guid, uint32_t entry)
{
    m_kind  = kind;
    m_guid  = guid;
    m_entry = entry;
    m_hasSelection = true;
    m_headerLabel->setText(tr("selected: %1 guid=%2 entry=%3")
        .arg(kind == render::SpawnKind::Creature
             ? QStringLiteral("creature") : QStringLiteral("gameobject"))
        .arg(guid).arg(entry));
    m_headerLabel->setStyleSheet(QStringLiteral("color: #f9b34a;"));

    if (!m_dbClient || !m_dbClient->isConnected())
    {
        setPlaceholder(m_smartTable, tr("(not connected to DB)"));
        setPlaceholder(m_linkTable,  tr("(not connected to DB)"));
        setPlaceholder(m_eventTable, tr("(not connected to DB)"));
        setPlaceholder(m_transTable, tr("(not connected to DB)"));
        return;
    }
    restoreHeaders(m_smartTable, {
        tr("entryorguid"), tr("src"), tr("id"), tr("link"),
        tr("event"), tr("action"), tr("target"), tr("comment") });
    restoreHeaders(m_linkTable,  { tr("guid"), tr("linkedGuid"), tr("linkType") });
    restoreHeaders(m_eventTable, { tr("eventEntry"), tr("guid") });
    restoreHeaders(m_transTable, {
        tr("guid"), tr("entry"), tr("name"), tr("ScriptName") });

    runSmartScripts(kind, guid, entry);
    runLinkedRespawn(guid);
    runGameEvent(kind, guid);
    runTransport(kind, entry);
}

void SpawnDiagnosticsDock::runSmartScripts(render::SpawnKind kind, int64_t guid, uint32_t entry)
{
    // source_type semantics: 0=creature, 1=gameobject, 9=action_list.
    // Entry-scoped scripts use entryorguid=entry; per-spawn override
    // scripts use entryorguid=-guid (TC convention).  We pull both.
    uint8_t const sourceType = (kind == render::SpawnKind::Creature) ? 0 : 1;
    char sql[768];
    std::snprintf(sql, sizeof(sql),
        "SELECT entryorguid, source_type, id, link, event_type, action_type, "
        "       target_type, COALESCE(comment, '') "
        "FROM smart_scripts "
        "WHERE (source_type = %u AND entryorguid = %u) "
        "   OR (source_type = %u AND entryorguid = -%lld) "
        "ORDER BY entryorguid, source_type, id, link",
        unsigned(sourceType), entry,
        unsigned(sourceType), static_cast<long long>(guid));
    db::QueryResult res;
    auto const err = m_dbClient->query(sql, res);
    if (!err.ok())
    {
        setPlaceholder(m_smartTable,
            tr("query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    fillTable(m_smartTable, res);
    m_tabs->setTabText(0, tr("SmartScripts (%1)").arg(res.rowCount()));
}

void SpawnDiagnosticsDock::runLinkedRespawn(int64_t guid)
{
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT guid, linkedGuid, linkType FROM linked_respawn "
        "WHERE guid = %lld OR linkedGuid = %lld",
        static_cast<long long>(guid), static_cast<long long>(guid));
    db::QueryResult res;
    auto const err = m_dbClient->query(sql, res);
    if (!err.ok())
    {
        setPlaceholder(m_linkTable,
            tr("query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    fillTable(m_linkTable, res);
    m_tabs->setTabText(1, tr("LinkedRespawn (%1)").arg(res.rowCount()));
}

void SpawnDiagnosticsDock::runGameEvent(render::SpawnKind kind, int64_t guid)
{
    char const* table = (kind == render::SpawnKind::Creature)
        ? "game_event_creature" : "game_event_gameobject";
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT eventEntry, guid FROM %s WHERE guid = %lld ORDER BY eventEntry",
        table, static_cast<long long>(guid));
    db::QueryResult res;
    auto const err = m_dbClient->query(sql, res);
    if (!err.ok())
    {
        setPlaceholder(m_eventTable,
            tr("query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    fillTable(m_eventTable, res);
    m_tabs->setTabText(2, tr("GameEvent (%1)").arg(res.rowCount()));
}

void SpawnDiagnosticsDock::runTransport(render::SpawnKind kind, uint32_t entry)
{
    // Only meaningful for gameobjects.  For creatures, fall through
    // empty.
    if (kind != render::SpawnKind::GameObject)
    {
        m_transTable->setRowCount(0);
        m_tabs->setTabText(3, tr("Transport (n/a)"));
        return;
    }
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT guid, entry, COALESCE(name, ''), ScriptName "
        "FROM transports WHERE entry = %u",
        entry);
    db::QueryResult res;
    auto const err = m_dbClient->query(sql, res);
    if (!err.ok())
    {
        setPlaceholder(m_transTable,
            tr("query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    fillTable(m_transTable, res);
    m_tabs->setTabText(3, tr("Transport (%1)").arg(res.rowCount()));
}

void SpawnDiagnosticsDock::onAddLinkedRespawnClicked()
{
    if (!m_hasSelection)
    {
        QMessageBox::information(this, tr("No spawn"),
            tr("Select a spawn first; the link will originate at that spawn."));
        return;
    }
    bool ok = false;
    // bigint unsigned guid can exceed 32-bit; use getText so we don't
    // truncate.  Accept any non-empty numeric input; parse as long long.
    QString const guidStr = QInputDialog::getText(this,
        tr("Add linked respawn"),
        tr("Linked target GUID (the spawn whose respawn cascades the selected one):"),
        QLineEdit::Normal, QString{}, &ok);
    if (!ok || guidStr.trimmed().isEmpty()) return;
    bool parseOk = false;
    qlonglong const target = guidStr.trimmed().toLongLong(&parseOk);
    if (!parseOk || target == 0)
    {
        QMessageBox::warning(this, tr("Bad GUID"),
            tr("Could not parse '%1' as a numeric GUID.").arg(guidStr));
        return;
    }
    // linkType: 0=CR->CR, 1=GO->CR, 2=CR->GO, 3=GO->GO  (TC convention,
    // see linked_respawn enum in core).  The selected spawn is the
    // dependent (`guid`) and target is the master (`linkedGuid`).
    QStringList const types = {
        tr("0 = creature -> creature"),
        tr("1 = creature -> gameobject"),
        tr("2 = gameobject -> creature"),
        tr("3 = gameobject -> gameobject") };
    QString const chosen = QInputDialog::getItem(this,
        tr("Link type"),
        tr("How does the dependent's respawn relate to the master's?"),
        types, 0, false, &ok);
    if (!ok || chosen.isEmpty()) return;
    int const linkType = chosen.left(1).toInt();
    emit addLinkedRespawnRequested(qlonglong(m_guid), target, linkType);
}

void SpawnDiagnosticsDock::onRemoveLinkedRespawnClicked()
{
    int const row = m_linkTable->currentRow();
    if (row < 0)
    {
        QMessageBox::information(this, tr("No row"),
            tr("Select a row in the LinkedRespawn table to remove."));
        return;
    }
    auto* guidItem     = m_linkTable->item(row, 0);
    auto* linkTypeItem = m_linkTable->item(row, 2);
    if (!guidItem || !linkTypeItem) return;
    bool okG = false, okT = false;
    qlonglong const guid     = guidItem->text().toLongLong(&okG);
    int       const linkType = linkTypeItem->text().toInt(&okT);
    if (!okG || !okT) return;
    emit removeLinkedRespawnRequested(guid, linkType);
}

void SpawnDiagnosticsDock::onAddGameEventClicked()
{
    if (!m_hasSelection)
    {
        QMessageBox::information(this, tr("No spawn"),
            tr("Select a spawn first; it will be bound to a game event."));
        return;
    }
    bool ok = false;
    int const eventEntry = QInputDialog::getInt(this,
        tr("Add to game event"),
        tr("Game event entry (signed tinyint - negative entries inverted-mask):"),
        1, -128, 127, 1, &ok);
    if (!ok) return;
    emit addGameEventRequested(eventEntry);
}

void SpawnDiagnosticsDock::onRemoveGameEventClicked()
{
    int const row = m_eventTable->currentRow();
    if (row < 0)
    {
        QMessageBox::information(this, tr("No row"),
            tr("Select a row in the GameEvent table to remove."));
        return;
    }
    auto* item = m_eventTable->item(row, 0);
    if (!item) return;
    bool ok = false;
    int const eventEntry = item->text().toInt(&ok);
    if (!ok) return;
    emit removeGameEventRequested(eventEntry);
}

void SpawnDiagnosticsDock::onAddSmartScriptClicked()
{
    if (!m_hasSelection)
    {
        QMessageBox::information(this, tr("No spawn"),
            tr("Select a spawn first; the new SAI row will start with entryorguid=entry."));
        return;
    }
    emit addSmartScriptRequested();
}

void SpawnDiagnosticsDock::onEditSmartScriptClicked()
{
    int const row = m_smartTable->currentRow();
    if (row < 0)
    {
        QMessageBox::information(this, tr("No row"),
            tr("Select a row in the SmartScripts table to edit."));
        return;
    }
    auto* eItem = m_smartTable->item(row, 0);  // entryorguid
    auto* sItem = m_smartTable->item(row, 1);  // src (source_type)
    auto* iItem = m_smartTable->item(row, 2);  // id
    auto* lItem = m_smartTable->item(row, 3);  // link
    if (!eItem || !sItem || !iItem || !lItem) return;
    bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
    qlonglong const eo = eItem->text().toLongLong(&ok1);
    int const sr = sItem->text().toInt(&ok2);
    int const id = iItem->text().toInt(&ok3);
    int const lk = lItem->text().toInt(&ok4);
    if (!ok1 || !ok2 || !ok3 || !ok4) return;
    emit editSmartScriptRequested(eo, sr, id, lk);
}

void SpawnDiagnosticsDock::onRemoveSmartScriptClicked()
{
    int const row = m_smartTable->currentRow();
    if (row < 0)
    {
        QMessageBox::information(this, tr("No row"),
            tr("Select a row in the SmartScripts table to remove."));
        return;
    }
    auto* eItem = m_smartTable->item(row, 0);
    auto* sItem = m_smartTable->item(row, 1);
    auto* iItem = m_smartTable->item(row, 2);
    auto* lItem = m_smartTable->item(row, 3);
    if (!eItem || !sItem || !iItem || !lItem) return;
    bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
    qlonglong const eo = eItem->text().toLongLong(&ok1);
    int const sr = sItem->text().toInt(&ok2);
    int const id = iItem->text().toInt(&ok3);
    int const lk = lItem->text().toInt(&ok4);
    if (!ok1 || !ok2 || !ok3 || !ok4) return;
    emit removeSmartScriptRequested(eo, sr, id, lk);
}

void SpawnDiagnosticsDock::onAddTransportClicked()
{
    // Transport rows are gameobject-only; warn if the selected spawn
    // is a creature.  The handler in MainWindow seeds the form with
    // the selected GO's guid+entry when applicable.
    if (m_hasSelection && m_kind != render::SpawnKind::GameObject)
    {
        QMessageBox::information(this, tr("Wrong kind"),
            tr("Transports are gameobject rows.  Select a gameobject spawn first "
               "to seed the new row's guid+entry, or proceed for a manual entry."));
    }
    emit addTransportRequested();
}

void SpawnDiagnosticsDock::onEditTransportClicked()
{
    int const row = m_transTable->currentRow();
    if (row < 0)
    {
        QMessageBox::information(this, tr("No row"),
            tr("Select a row in the Transport table to edit."));
        return;
    }
    auto* guidItem = m_transTable->item(row, 0);
    if (!guidItem) return;
    bool ok = false;
    qlonglong const guid = guidItem->text().toLongLong(&ok);
    if (!ok) return;
    emit editTransportRequested(guid);
}

void SpawnDiagnosticsDock::onRemoveTransportClicked()
{
    int const row = m_transTable->currentRow();
    if (row < 0)
    {
        QMessageBox::information(this, tr("No row"),
            tr("Select a row in the Transport table to remove."));
        return;
    }
    auto* guidItem = m_transTable->item(row, 0);
    if (!guidItem) return;
    bool ok = false;
    qlonglong const guid = guidItem->text().toLongLong(&ok);
    if (!ok) return;
    emit removeTransportRequested(guid);
}

} // namespace world_editor::app
