#include "BookmarksManagerDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace world_editor::app
{

namespace
{
// Column layout for both folder rows and leaf rows.  Folder rows put the
// group name in col 0 and leave the rest blank; leaf rows put their data
// across cols 0-6.
enum Col { ColName = 0, ColFolder = 1, ColTags = 2, ColMap = 3, ColX = 4, ColY = 5, ColZ = 6, ColCount = 7 };

// Sentinel folder label shown in the tree for bookmarks whose persisted
// folder field is empty.  Matches the "Quick" terminology used by the
// View -> Bookmarks submenu so the operator sees the same name in both
// places.
QString quickFolderLabel() { return QStringLiteral("Quick"); }

// Folder rows store nothing in UserRole; leaf rows store a marker so we
// can tell them apart cheaply.
constexpr int kIsLeafRole = Qt::UserRole + 1;

bool isLeaf(QTreeWidgetItem const* item)
{
    return item && item->data(0, kIsLeafRole).toBool();
}

// Find-or-insert a top-level folder group, sorted alphabetically with
// "Quick" pinned to the top so the empty-folder bookmarks stay first.
QTreeWidgetItem* findOrInsertGroup(QTreeWidget* tree, QString const& folder)
{
    QString const label = folder.isEmpty() ? quickFolderLabel() : folder;
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        if (tree->topLevelItem(i)->text(ColName) == label)
            return tree->topLevelItem(i);

    auto* group = new QTreeWidgetItem(QStringList{} << label);
    // Folder rows are NOT editable; leaf rows are.  Make sure to mark
    // them up-front before insertion so flags propagate.
    group->setFlags(Qt::ItemIsEnabled);
    QFont bold = group->font(ColName);
    bold.setBold(true);
    group->setFont(ColName, bold);

    // Pin "Quick" to the top, otherwise alpha-sort.
    if (label == quickFolderLabel())
    {
        tree->insertTopLevelItem(0, group);
        return group;
    }
    int insertAt = tree->topLevelItemCount();
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        QString const t = tree->topLevelItem(i)->text(ColName);
        if (t == quickFolderLabel()) continue;
        if (label < t) { insertAt = i; break; }
    }
    tree->insertTopLevelItem(insertAt, group);
    return group;
}
} // namespace

BookmarksManagerDialog::BookmarksManagerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage bookmarks"));
    setModal(true);
    resize(900, 560);

    auto* outer = new QVBoxLayout(this);

    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Filter (name or tag):"), this));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("type to narrow the list - matches name or any tag"));
    filterRow->addWidget(m_filter, 1);
    outer->addLayout(filterRow);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(ColCount);
    m_tree->setHeaderLabels(
        { tr("Name"), tr("Folder"), tr("Tags"), tr("mapId"),
          tr("X"), tr("Y"), tr("Z") });
    m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Edit-on-double-click: rows reveal QLineEdit inline for every
    // editable column.
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked
                            | QAbstractItemView::EditKeyPressed
                            | QAbstractItemView::AnyKeyPressed);
    outer->addWidget(m_tree, 1);

    auto* btnRow = new QHBoxLayout;
    m_addBtn   = new QPushButton(tr("Add new"),         this);
    m_delBtn   = new QPushButton(tr("Delete selected"), this);
    m_moveBtn  = new QPushButton(tr("Move to folder..."), this);
    m_applyBtn = new QPushButton(tr("Apply"),           this);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_delBtn);
    btnRow->addWidget(m_moveBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(m_applyBtn);
    outer->addLayout(btnRow);

    auto* closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    outer->addWidget(closeBox);

    connect(m_addBtn,   &QPushButton::clicked, this, &BookmarksManagerDialog::onAdd);
    connect(m_delBtn,   &QPushButton::clicked, this, &BookmarksManagerDialog::onDelete);
    connect(m_moveBtn,  &QPushButton::clicked, this, &BookmarksManagerDialog::onMoveToFolder);
    connect(m_applyBtn, &QPushButton::clicked, this, &BookmarksManagerDialog::onApply);
    connect(m_filter,   &QLineEdit::textChanged, this, &BookmarksManagerDialog::onFilterChanged);
    connect(m_tree,     &QTreeWidget::itemChanged, this, &BookmarksManagerDialog::onItemChanged);

    rebuildTree();
}

void BookmarksManagerDialog::rebuildTree()
{
    m_suppressEdits = true;
    m_tree->clear();
    QVector<Bookmark> const all = bookmarks::loadAll();
    for (Bookmark const& b : all)
        insertLeafFor(b);
    m_tree->expandAll();
    m_suppressEdits = false;
    applyFilterToTree();
}

QTreeWidgetItem* BookmarksManagerDialog::insertLeafFor(Bookmark const& b)
{
    QTreeWidgetItem* group = findOrInsertGroup(m_tree, b.folder);
    auto* leaf = new QTreeWidgetItem(group);
    leaf->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    leaf->setData(0, kIsLeafRole, true);
    leaf->setText(ColName,   b.name);
    leaf->setText(ColFolder, b.folder);
    leaf->setText(ColTags,   b.tags);
    leaf->setText(ColMap,    QString::number(b.mapId));
    leaf->setText(ColX,      QString::number(b.x, 'f', 2));
    leaf->setText(ColY,      QString::number(b.y, 'f', 2));
    leaf->setText(ColZ,      QString::number(b.z, 'f', 2));
    return leaf;
}

void BookmarksManagerDialog::applyFilterToTree()
{
    QString const needle = m_filter->text();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* group = m_tree->topLevelItem(i);
        int visibleLeaves = 0;
        for (int j = 0; j < group->childCount(); ++j)
        {
            QTreeWidgetItem* leaf = group->child(j);
            Bookmark b;
            b.name = leaf->text(ColName);
            b.tags = leaf->text(ColTags);
            bool const match = bookmarks::matchesFilter(b, needle);
            leaf->setHidden(!match);
            if (match) ++visibleLeaves;
        }
        // Hide an entire folder if nothing in it survived the filter,
        // unless the filter is empty (then we always show all groups).
        group->setHidden(!needle.trimmed().isEmpty() && visibleLeaves == 0);
    }
}

QVector<Bookmark> BookmarksManagerDialog::collectFromTree() const
{
    QVector<Bookmark> out;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* group = m_tree->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j)
        {
            QTreeWidgetItem* leaf = group->child(j);
            if (!isLeaf(leaf)) continue;
            Bookmark b;
            b.name   = leaf->text(ColName);
            // Trust the explicit folder cell on the leaf; if the operator
            // edited it inline but didn't move the row, that's still the
            // authoritative folder for the persisted record.
            b.folder = leaf->text(ColFolder);
            b.tags   = leaf->text(ColTags);
            b.mapId  = leaf->text(ColMap).toUInt();
            b.x      = leaf->text(ColX).toFloat();
            b.y      = leaf->text(ColY).toFloat();
            b.z      = leaf->text(ColZ).toFloat();
            out.push_back(b);
        }
    }
    return out;
}

void BookmarksManagerDialog::onAdd()
{
    bool ok = false;
    QString const name = QInputDialog::getText(this, tr("New bookmark"),
        tr("Name:"), QLineEdit::Normal, tr("New bookmark"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    Bookmark b;
    b.name = name.trimmed();
    insertLeafFor(b);
    m_tree->expandAll();
    onApply();
}

void BookmarksManagerDialog::onDelete()
{
    QList<QTreeWidgetItem*> const selected = m_tree->selectedItems();
    if (selected.isEmpty())
        return;
    int removed = 0;
    for (QTreeWidgetItem* item : selected)
    {
        if (!isLeaf(item)) continue;
        QTreeWidgetItem* parent = item->parent();
        if (!parent) continue;
        delete parent->takeChild(parent->indexOfChild(item));
        ++removed;
        // If the folder is now empty, drop it too -- keeps the tree tidy.
        if (parent->childCount() == 0)
            delete m_tree->takeTopLevelItem(m_tree->indexOfTopLevelItem(parent));
    }
    if (removed > 0)
        onApply();
}

void BookmarksManagerDialog::onMoveToFolder()
{
    QList<QTreeWidgetItem*> const selected = m_tree->selectedItems();
    if (selected.isEmpty())
        return;
    // Build a set of existing folder names to seed the input combo.
    QSet<QString> existing;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QString const t = m_tree->topLevelItem(i)->text(ColName);
        if (t != quickFolderLabel())
            existing.insert(t);
    }
    QStringList options = existing.values();
    std::sort(options.begin(), options.end());
    options.prepend(QString{});  // "" == Quick / no folder
    bool ok = false;
    QString folder = QInputDialog::getItem(this, tr("Move to folder"),
        tr("Folder (blank = Quick / no folder):"), options, 0, /*editable*/ true, &ok);
    if (!ok)
        return;
    folder = folder.trimmed();
    int moved = 0;
    for (QTreeWidgetItem* item : selected)
    {
        if (!isLeaf(item)) continue;
        m_suppressEdits = true;
        item->setText(ColFolder, folder);
        m_suppressEdits = false;
        ++moved;
    }
    if (moved == 0)
        return;
    // Easiest correct way to physically reparent: snapshot+rebuild.
    QVector<Bookmark> const snap = collectFromTree();
    bookmarks::saveAll(snap);
    rebuildTree();
    emit bookmarksChanged();
}

void BookmarksManagerDialog::onApply()
{
    QVector<Bookmark> const snap = collectFromTree();
    bookmarks::saveAll(snap);
    emit bookmarksChanged();
}

void BookmarksManagerDialog::onFilterChanged(QString const& /*text*/)
{
    applyFilterToTree();
}

void BookmarksManagerDialog::onItemChanged(QTreeWidgetItem* item, int /*column*/)
{
    if (m_suppressEdits)
        return;
    if (!isLeaf(item))
        return;
    // Inline edits persist immediately (no Apply required) so the View
    // submenu reflects the change on next open.
    onApply();
}

} // namespace world_editor::app
