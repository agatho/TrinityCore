#include "FindJumpDialog.h"

#include "Bookmarks.h"
#include "BookmarksManagerDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QSettings>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <cstdio>

namespace world_editor::app
{

namespace
{
QString esc(db::MySqlClient* c, QString const& v)
{
    if (!c) return v;
    return QString::fromStdString(c->escapeString(v.toStdString()));
}

QStandardItemModel* makeTableModel(QObject* parent, QStringList const& headers)
{
    auto* m = new QStandardItemModel(parent);
    m->setHorizontalHeaderLabels(headers);
    return m;
}

QTableView* makeTableView(QWidget* parent, QStandardItemModel* model,
                          QSortFilterProxyModel*& outProxy)
{
    auto* v = new QTableView(parent);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->setSortingEnabled(true);
    outProxy = new QSortFilterProxyModel(parent);
    outProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    outProxy->setFilterKeyColumn(-1);
    outProxy->setSourceModel(model);
    v->setModel(outProxy);
    v->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    v->horizontalHeader()->setStretchLastSection(true);
    return v;
}
} // namespace

FindJumpDialog::FindJumpDialog(db::MySqlClient* dbClient,
                               uint32_t currentMapId,
                               float currentWorldX,
                               float currentWorldY,
                               QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_currentMapId(currentMapId)
{
    setWindowTitle(tr("Find & Jump"));
    setModal(true);
    resize(820, 600);

    m_tabs = new QTabWidget(this);

    // ---- Template tab ------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* v = new QVBoxLayout(page);

        m_tplFilter = new QLineEdit(page);
        m_tplFilter->setPlaceholderText(tr("entry# or name fragment, e.g. \"Stormwind Guard\" or 68"));

        m_tplModel = makeTableModel(this,
            { tr("kind"), tr("entry"), tr("name"),
              tr("spawnGuid"), tr("mapId"), tr("posX"), tr("posY"), tr("posZ") });
        m_tplView = makeTableView(this, m_tplModel, m_tplProxy);

        auto* searchBtn = new QPushButton(tr("Search"), page);
        m_tplJumpBtn = new QPushButton(tr("Jump to selected"), page);
        m_tplJumpBtn->setEnabled(false);
        m_tplStatusLbl = new QLabel(QString{}, page);

        auto* topBar = new QHBoxLayout;
        topBar->addWidget(m_tplFilter, 1);
        topBar->addWidget(searchBtn);
        topBar->addWidget(m_tplJumpBtn);
        v->addLayout(topBar);
        v->addWidget(m_tplView, 1);
        v->addWidget(m_tplStatusLbl);

        connect(searchBtn,    &QPushButton::clicked,    this, &FindJumpDialog::onSearchTemplate);
        connect(m_tplFilter,  &QLineEdit::returnPressed, this, &FindJumpDialog::onSearchTemplate);
        connect(m_tplJumpBtn, &QPushButton::clicked,    this, &FindJumpDialog::onJumpTemplate);
        connect(m_tplView,    &QTableView::doubleClicked, this,
                [this](QModelIndex const&) { onJumpTemplate(); });
        connect(m_tplView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                [this](QItemSelection const&, QItemSelection const&) {
                    m_tplJumpBtn->setEnabled(m_tplView->selectionModel()->hasSelection());
                });

        m_tabs->addTab(page, tr("By template (entry / name)"));
    }

    // ---- Guid tab ----------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* v = new QVBoxLayout(page);

        m_guidEdit = new QLineEdit(page);
        m_guidEdit->setPlaceholderText(tr("creature.guid or gameobject.guid"));
        m_guidModel = makeTableModel(this,
            { tr("kind"), tr("guid"), tr("entry"), tr("mapId"), tr("posX"), tr("posY"), tr("posZ") });
        QSortFilterProxyModel* dummyProxy = nullptr;
        m_guidView = makeTableView(this, m_guidModel, dummyProxy);

        auto* searchBtn = new QPushButton(tr("Search"), page);
        m_guidJumpBtn = new QPushButton(tr("Jump to selected"), page);
        m_guidJumpBtn->setEnabled(false);
        m_guidStatusLbl = new QLabel(QString{}, page);

        auto* topBar = new QHBoxLayout;
        topBar->addWidget(m_guidEdit, 1);
        topBar->addWidget(searchBtn);
        topBar->addWidget(m_guidJumpBtn);
        v->addLayout(topBar);
        v->addWidget(m_guidView, 1);
        v->addWidget(m_guidStatusLbl);

        connect(searchBtn,     &QPushButton::clicked,     this, &FindJumpDialog::onSearchGuid);
        connect(m_guidEdit,    &QLineEdit::returnPressed, this, &FindJumpDialog::onSearchGuid);
        connect(m_guidJumpBtn, &QPushButton::clicked,     this, &FindJumpDialog::onJumpGuid);
        connect(m_guidView,    &QTableView::doubleClicked, this,
                [this](QModelIndex const&) { onJumpGuid(); });
        connect(m_guidView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                [this](QItemSelection const&, QItemSelection const&) {
                    m_guidJumpBtn->setEnabled(m_guidView->selectionModel()->hasSelection());
                });

        m_tabs->addTab(page, tr("By spawn guid"));
    }

    // ---- Coords tab --------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* v = new QVBoxLayout(page);

        m_coordsMap = new QSpinBox(page); m_coordsMap->setRange(0, 100000);
        m_coordsX = new QDoubleSpinBox(page);
        m_coordsX->setRange(-20000, 20000); m_coordsX->setDecimals(3);
        m_coordsX->setSuffix(tr(" X (north)"));
        m_coordsY = new QDoubleSpinBox(page);
        m_coordsY->setRange(-20000, 20000); m_coordsY->setDecimals(3);
        m_coordsY->setSuffix(tr(" Y (west)"));

        m_coordsMap->setValue(int(m_currentMapId));
        m_coordsX->setValue(double(currentWorldX));
        m_coordsY->setValue(double(currentWorldY));

        auto* form = new QHBoxLayout;
        form->addWidget(new QLabel(tr("mapId")));
        form->addWidget(m_coordsMap);
        form->addWidget(new QLabel(tr("X (north)")));
        form->addWidget(m_coordsX);
        form->addWidget(new QLabel(tr("Y (west)")));
        form->addWidget(m_coordsY);

        auto* goBtn = new QPushButton(tr("Jump to (mapId, X, Y)"), page);
        v->addLayout(form);
        v->addWidget(goBtn);
        v->addStretch(1);

        connect(goBtn, &QPushButton::clicked, this, &FindJumpDialog::onJumpToCoords);

        m_tabs->addTab(page, tr("By coords"));
    }

    // ---- Bookmarks tab -----------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* v = new QVBoxLayout(page);
        m_bookmarkModel = new QStandardItemModel(this);
        // Columns mirror the persisted Bookmark struct so saveBookmarks
        // is a 1:1 cell-to-field write.  posZ is kept here so the table
        // round-trips the full record even though the jump itself only
        // needs (mapId, X, Y).
        m_bookmarkModel->setHorizontalHeaderLabels(
            { tr("Name"), tr("Folder"), tr("Tags"),
              tr("mapId"), tr("posX"), tr("posY"), tr("posZ") });
        m_bookmarkView = new QTableView(page);
        m_bookmarkView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_bookmarkView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_bookmarkView->setSelectionMode(QAbstractItemView::SingleSelection);
        m_bookmarkView->setSortingEnabled(true);
        m_bookmarkView->setModel(m_bookmarkModel);
        m_bookmarkView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        m_bookmarkView->horizontalHeader()->setStretchLastSection(true);

        m_bookmarkAddBtn    = new QPushButton(tr("Add current view..."), page);
        m_bookmarkRenameBtn = new QPushButton(tr("Rename..."),          page);
        m_bookmarkRemoveBtn = new QPushButton(tr("Remove"),             page);
        m_bookmarkManageBtn = new QPushButton(tr("Manage..."),          page);
        m_bookmarkJumpBtn   = new QPushButton(tr("Jump to selected"),   page);
        m_bookmarkRenameBtn->setEnabled(false);
        m_bookmarkRemoveBtn->setEnabled(false);
        m_bookmarkJumpBtn->setEnabled(false);

        auto* topBar = new QHBoxLayout;
        topBar->addWidget(m_bookmarkAddBtn);
        topBar->addWidget(m_bookmarkRenameBtn);
        topBar->addWidget(m_bookmarkRemoveBtn);
        topBar->addWidget(m_bookmarkManageBtn);
        topBar->addStretch(1);
        topBar->addWidget(m_bookmarkJumpBtn);
        v->addLayout(topBar);
        v->addWidget(m_bookmarkView, 1);

        connect(m_bookmarkAddBtn,    &QPushButton::clicked, this, &FindJumpDialog::onBookmarkAddCurrent);
        connect(m_bookmarkRenameBtn, &QPushButton::clicked, this, &FindJumpDialog::onBookmarkRename);
        connect(m_bookmarkRemoveBtn, &QPushButton::clicked, this, &FindJumpDialog::onBookmarkRemove);
        connect(m_bookmarkJumpBtn,   &QPushButton::clicked, this, &FindJumpDialog::onBookmarkJump);
        connect(m_bookmarkManageBtn, &QPushButton::clicked, this, &FindJumpDialog::onBookmarkManage);
        connect(m_bookmarkView, &QTableView::doubleClicked, this,
                [this](QModelIndex const&) { onBookmarkJump(); });
        connect(m_bookmarkView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, [this](QItemSelection const&, QItemSelection const&) {
                    bool const has = m_bookmarkView->selectionModel()->hasSelection();
                    m_bookmarkRenameBtn->setEnabled(has);
                    m_bookmarkRemoveBtn->setEnabled(has);
                    m_bookmarkJumpBtn->setEnabled(has);
                });

        loadBookmarks();
        m_tabs->addTab(page, tr("Bookmarks"));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_tabs, 1);
    outer->addWidget(buttons);
}

void FindJumpDialog::loadBookmarks()
{
    m_bookmarkModel->setRowCount(0);
    QVector<Bookmark> const items = bookmarks::loadAll();
    for (Bookmark const& b : items)
    {
        int const r = m_bookmarkModel->rowCount();
        m_bookmarkModel->insertRow(r);
        m_bookmarkModel->setItem(r, 0, new QStandardItem(b.name));
        m_bookmarkModel->setItem(r, 1, new QStandardItem(b.folder));
        m_bookmarkModel->setItem(r, 2, new QStandardItem(b.tags));
        m_bookmarkModel->setItem(r, 3, new QStandardItem(QString::number(b.mapId)));
        m_bookmarkModel->setItem(r, 4, new QStandardItem(QString::number(b.x, 'f', 1)));
        m_bookmarkModel->setItem(r, 5, new QStandardItem(QString::number(b.y, 'f', 1)));
        m_bookmarkModel->setItem(r, 6, new QStandardItem(QString::number(b.z, 'f', 1)));
    }
}

void FindJumpDialog::saveBookmarks() const
{
    QVector<Bookmark> items;
    items.reserve(m_bookmarkModel->rowCount());
    for (int i = 0; i < m_bookmarkModel->rowCount(); ++i)
    {
        Bookmark b;
        b.name   = m_bookmarkModel->item(i, 0)->text();
        b.folder = m_bookmarkModel->item(i, 1)->text();
        b.tags   = m_bookmarkModel->item(i, 2)->text();
        b.mapId  = m_bookmarkModel->item(i, 3)->text().toUInt();
        b.x      = m_bookmarkModel->item(i, 4)->text().toFloat();
        b.y      = m_bookmarkModel->item(i, 5)->text().toFloat();
        b.z      = m_bookmarkModel->item(i, 6)->text().toFloat();
        items.push_back(b);
    }
    bookmarks::saveAll(items);
}

void FindJumpDialog::onBookmarkAddCurrent()
{
    bool ok = false;
    // Default name includes the map id + current coords so the operator
    // gets a usable starting point.  They edit from there.
    QString const proposed = tr("map %1 at (%2, %3)")
        .arg(m_coordsMap->value())
        .arg(m_coordsX->value(), 0, 'f', 1)
        .arg(m_coordsY->value(), 0, 'f', 1);
    QString const name = QInputDialog::getText(this, tr("Add bookmark"),
        tr("Name for this view (mapId %1, X=%2, Y=%3):")
            .arg(m_coordsMap->value())
            .arg(m_coordsX->value(), 0, 'f', 1)
            .arg(m_coordsY->value(), 0, 'f', 1),
        QLineEdit::Normal, proposed, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    // Folder + tags are optional - leave blank for the "Quick" group; the
    // operator can promote into a folder later via Manage...
    QString const folder = QInputDialog::getText(this, tr("Add bookmark"),
        tr("Folder (leave blank for Quick):"), QLineEdit::Normal, QString{}, &ok);
    if (!ok) return;
    QString const tags = QInputDialog::getText(this, tr("Add bookmark"),
        tr("Tags (comma-separated, optional):"), QLineEdit::Normal, QString{}, &ok);
    if (!ok) return;
    int const r = m_bookmarkModel->rowCount();
    m_bookmarkModel->insertRow(r);
    m_bookmarkModel->setItem(r, 0, new QStandardItem(name.trimmed()));
    m_bookmarkModel->setItem(r, 1, new QStandardItem(folder.trimmed()));
    m_bookmarkModel->setItem(r, 2, new QStandardItem(tags.trimmed()));
    m_bookmarkModel->setItem(r, 3, new QStandardItem(QString::number(m_coordsMap->value())));
    m_bookmarkModel->setItem(r, 4, new QStandardItem(QString::number(m_coordsX->value(), 'f', 1)));
    m_bookmarkModel->setItem(r, 5, new QStandardItem(QString::number(m_coordsY->value(), 'f', 1)));
    m_bookmarkModel->setItem(r, 6, new QStandardItem(QStringLiteral("0.0")));
    saveBookmarks();
}

void FindJumpDialog::onBookmarkRename()
{
    QModelIndex const idx = m_bookmarkView->currentIndex();
    if (!idx.isValid()) return;
    int const row = idx.row();
    auto* item = m_bookmarkModel->item(row, 0);
    if (!item) return;
    bool ok = false;
    QString const newLabel = QInputDialog::getText(this, tr("Rename bookmark"),
        tr("New label:"), QLineEdit::Normal, item->text(), &ok);
    if (!ok || newLabel.trimmed().isEmpty()) return;
    item->setText(newLabel.trimmed());
    saveBookmarks();
}

void FindJumpDialog::onBookmarkRemove()
{
    QModelIndex const idx = m_bookmarkView->currentIndex();
    if (!idx.isValid()) return;
    m_bookmarkModel->removeRow(idx.row());
    saveBookmarks();
}

void FindJumpDialog::onBookmarkJump()
{
    QModelIndex const idx = m_bookmarkView->currentIndex();
    if (!idx.isValid()) return;
    int const row = idx.row();
    auto* mapItem = m_bookmarkModel->item(row, 3);
    auto* xItem   = m_bookmarkModel->item(row, 4);
    auto* yItem   = m_bookmarkModel->item(row, 5);
    if (!mapItem || !xItem || !yItem) return;
    emit jumpRequested(uint32_t(mapItem->text().toUInt()),
                       xItem->text().toFloat(),
                       yItem->text().toFloat(),
                       std::nullopt);
    accept();
}

void FindJumpDialog::onBookmarkManage()
{
    BookmarksManagerDialog mgr(this);
    mgr.exec();
    // The manager writes through to QSettings on every mutation; refresh
    // our in-tab snapshot so the operator sees the latest state.
    loadBookmarks();
}

void FindJumpDialog::onSearchTemplate()
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        m_tplStatusLbl->setText(tr("not connected to DB"));
        return;
    }
    QString const raw = m_tplFilter->text().trimmed();
    if (raw.isEmpty())
        return;

    m_tplModel->setRowCount(0);
    bool isNumeric = false;
    uint32_t const asEntry = uint32_t(raw.toUInt(&isNumeric));
    QString const safeName = esc(m_dbClient, raw);

    // Pull matching templates (both creature + GO) in one trip, plus a
    // representative spawn for each.  Two queries — one per kind.
    auto runOne = [&](char const* tplTable, char const* spawnTable, char const* kindLabel)
    {
        std::string predicate;
        if (isNumeric)
            predicate = "tpl.entry = " + std::to_string(asEntry);
        else
            predicate = "tpl.name LIKE '%" + safeName.toStdString() + "%'";
        // LEFT JOIN spawn LIMIT-1 per template would be cleanest but
        // MySQL doesn't support lateral joins pre-8.0.14; instead we
        // pick the FIRST spawn by guid via a correlated subquery.
        // Prefer spawns on the current map when ANY exists there.
        char sql[1024];
        std::snprintf(sql, sizeof(sql),
            "SELECT tpl.entry, tpl.name, "
            "       (SELECT s.guid FROM %s s WHERE s.id = tpl.entry "
            "        ORDER BY (s.map = %u) DESC, s.guid LIMIT 1) AS spawn_guid, "
            "       (SELECT s.map FROM %s s WHERE s.id = tpl.entry "
            "        ORDER BY (s.map = %u) DESC, s.guid LIMIT 1) AS spawn_map, "
            "       (SELECT s.position_x FROM %s s WHERE s.id = tpl.entry "
            "        ORDER BY (s.map = %u) DESC, s.guid LIMIT 1) AS spawn_x, "
            "       (SELECT s.position_y FROM %s s WHERE s.id = tpl.entry "
            "        ORDER BY (s.map = %u) DESC, s.guid LIMIT 1) AS spawn_y, "
            "       (SELECT s.position_z FROM %s s WHERE s.id = tpl.entry "
            "        ORDER BY (s.map = %u) DESC, s.guid LIMIT 1) AS spawn_z "
            "FROM %s tpl WHERE %s ORDER BY tpl.entry LIMIT 200",
            spawnTable, m_currentMapId,
            spawnTable, m_currentMapId,
            spawnTable, m_currentMapId,
            spawnTable, m_currentMapId,
            spawnTable, m_currentMapId,
            tplTable,
            predicate.c_str());
        db::QueryResult res;
        auto const err = m_dbClient->query(sql, res);
        if (!err.ok())
            return size_t(0);
        for (size_t r = 0; r < res.rowCount(); ++r)
        {
            int const rowIdx = m_tplModel->rowCount();
            m_tplModel->insertRow(rowIdx);
            m_tplModel->setItem(rowIdx, 0, new QStandardItem(QString::fromLatin1(kindLabel)));
            m_tplModel->setItem(rowIdx, 1, new QStandardItem(QString::number(res.asUInt64(r, 0).value_or(0))));
            m_tplModel->setItem(rowIdx, 2, new QStandardItem(QString::fromStdString(res.cell(r, 1))));
            // Optional spawn info: blank if no spawn exists.
            if (!res.isNull(r, 2))
            {
                m_tplModel->setItem(rowIdx, 3, new QStandardItem(QString::number(res.asInt64(r, 2).value_or(0))));
                m_tplModel->setItem(rowIdx, 4, new QStandardItem(QString::number(res.asUInt64(r, 3).value_or(0))));
                m_tplModel->setItem(rowIdx, 5, new QStandardItem(QString::number(res.asDouble(r, 4).value_or(0.0), 'f', 1)));
                m_tplModel->setItem(rowIdx, 6, new QStandardItem(QString::number(res.asDouble(r, 5).value_or(0.0), 'f', 1)));
                m_tplModel->setItem(rowIdx, 7, new QStandardItem(QString::number(res.asDouble(r, 6).value_or(0.0), 'f', 1)));
            }
            else
            {
                for (int c = 3; c <= 7; ++c)
                    m_tplModel->setItem(rowIdx, c, new QStandardItem(QStringLiteral("-")));
            }
        }
        return res.rowCount();
    };

    QApplication::setOverrideCursor(Qt::WaitCursor);
    size_t const nC = runOne("creature_template",   "creature",   "creature");
    size_t const nG = runOne("gameobject_template", "gameobject", "gameobject");
    QApplication::restoreOverrideCursor();
    m_tplStatusLbl->setText(tr("creatures=%1  gameobjects=%2").arg(nC).arg(nG));
}

void FindJumpDialog::onSearchGuid()
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        m_guidStatusLbl->setText(tr("not connected to DB"));
        return;
    }
    bool ok = false;
    qlonglong const guid = m_guidEdit->text().trimmed().toLongLong(&ok);
    if (!ok || guid == 0)
    {
        m_guidStatusLbl->setText(tr("type a numeric guid"));
        return;
    }
    m_guidModel->setRowCount(0);
    auto runOne = [&](char const* table, char const* kindLabel) -> bool
    {
        char sql[512];
        std::snprintf(sql, sizeof(sql),
            "SELECT guid, id, map, position_x, position_y, position_z "
            "FROM %s WHERE guid = %lld LIMIT 1",
            table, (long long)guid);
        db::QueryResult res;
        auto const err = m_dbClient->query(sql, res);
        if (!err.ok() || res.rowCount() == 0)
            return false;
        int const rowIdx = m_guidModel->rowCount();
        m_guidModel->insertRow(rowIdx);
        m_guidModel->setItem(rowIdx, 0, new QStandardItem(QString::fromLatin1(kindLabel)));
        m_guidModel->setItem(rowIdx, 1, new QStandardItem(QString::number(res.asInt64(0, 0).value_or(0))));
        m_guidModel->setItem(rowIdx, 2, new QStandardItem(QString::number(res.asUInt64(0, 1).value_or(0))));
        m_guidModel->setItem(rowIdx, 3, new QStandardItem(QString::number(res.asUInt64(0, 2).value_or(0))));
        m_guidModel->setItem(rowIdx, 4, new QStandardItem(QString::number(res.asDouble(0, 3).value_or(0.0), 'f', 2)));
        m_guidModel->setItem(rowIdx, 5, new QStandardItem(QString::number(res.asDouble(0, 4).value_or(0.0), 'f', 2)));
        m_guidModel->setItem(rowIdx, 6, new QStandardItem(QString::number(res.asDouble(0, 5).value_or(0.0), 'f', 2)));
        return true;
    };
    bool const foundC = runOne("creature",   "creature");
    bool const foundG = runOne("gameobject", "gameobject");
    if (!foundC && !foundG)
        m_guidStatusLbl->setText(tr("no spawn with guid=%1").arg(guid));
    else
        m_guidStatusLbl->setText(tr("found %1").arg(QString(foundC ? QStringLiteral("creature") :
                                                              foundG ? QStringLiteral("gameobject") :
                                                                       QStringLiteral("?"))));
}

void FindJumpDialog::onJumpTemplate()
{
    QModelIndex const proxyIdx = m_tplView->currentIndex();
    if (!proxyIdx.isValid()) return;
    QModelIndex const srcIdx = m_tplProxy->mapToSource(proxyIdx);
    int const row = srcIdx.row();
    auto* guidItem = m_tplModel->item(row, 3);
    auto* mapItem  = m_tplModel->item(row, 4);
    auto* xItem    = m_tplModel->item(row, 5);
    auto* yItem    = m_tplModel->item(row, 6);
    if (!guidItem || !mapItem || !xItem || !yItem) return;
    if (guidItem->text() == QStringLiteral("-"))
    {
        QMessageBox::information(this, tr("No spawn"),
            tr("This template has no spawn rows in the DB; nothing to jump to."));
        return;
    }
    bool okMap = false, okX = false, okY = false, okGuid = false;
    uint32_t const mapId = uint32_t(mapItem->text().toUInt(&okMap));
    float const x = xItem->text().toFloat(&okX);
    float const y = yItem->text().toFloat(&okY);
    qlonglong const guid = guidItem->text().toLongLong(&okGuid);
    if (!okMap || !okX || !okY) return;
    std::optional<int64_t> guidOpt;
    if (okGuid && guid != 0)
        guidOpt = int64_t(guid);
    emit jumpRequested(mapId, x, y, guidOpt);
    accept();
}

void FindJumpDialog::onJumpGuid()
{
    QModelIndex const idx = m_guidView->currentIndex();
    if (!idx.isValid()) return;
    int const row = idx.row();
    auto* guidItem = m_guidModel->item(row, 1);
    auto* mapItem  = m_guidModel->item(row, 3);
    auto* xItem    = m_guidModel->item(row, 4);
    auto* yItem    = m_guidModel->item(row, 5);
    if (!guidItem || !mapItem || !xItem || !yItem) return;
    bool okMap = false, okX = false, okY = false, okGuid = false;
    uint32_t const mapId = uint32_t(mapItem->text().toUInt(&okMap));
    float const x = xItem->text().toFloat(&okX);
    float const y = yItem->text().toFloat(&okY);
    qlonglong const guid = guidItem->text().toLongLong(&okGuid);
    if (!okMap || !okX || !okY) return;
    std::optional<int64_t> guidOpt;
    if (okGuid && guid != 0)
        guidOpt = int64_t(guid);
    emit jumpRequested(mapId, x, y, guidOpt);
    accept();
}

void FindJumpDialog::onJumpToCoords()
{
    emit jumpRequested(uint32_t(m_coordsMap->value()),
                       float(m_coordsX->value()),
                       float(m_coordsY->value()),
                       std::nullopt);
    accept();
}

} // namespace world_editor::app
