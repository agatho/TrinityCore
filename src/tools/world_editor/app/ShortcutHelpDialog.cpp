#include "ShortcutHelpDialog.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace world_editor::app
{

namespace
{
enum Col { ColPath = 0, ColAction = 1, ColShortcut = 2, ColDescription = 3, ColCount = 4 };
} // namespace

QString ShortcutHelpDialog::stripMnemonics(QString const& text)
{
    // Walk char-by-char so we collapse "&&" -> "&" while dropping the
    // single-ampersand Qt-mnemonic markers.  Cheaper than a regex and
    // avoids the Qt-locale-aware shortcut parsing.
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i)
    {
        QChar const c = text.at(i);
        if (c == QLatin1Char('&'))
        {
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('&'))
            {
                out.append(QLatin1Char('&'));
                ++i;
            }
            // else: drop the mnemonic marker.
            continue;
        }
        out.append(c);
    }
    return out;
}

void ShortcutHelpDialog::collectFromMenu(QMenu* menu, QString const& path, QVector<Row>& out) const
{
    if (!menu) return;

    QList<QAction*> const acts = menu->actions();
    for (QAction* act : acts)
    {
        if (!act || act->isSeparator()) continue;

        // Submenu: recurse with extended path.
        if (QMenu* sub = act->menu())
        {
            QString const subLabel = stripMnemonics(sub->title());
            QString const childPath = path.isEmpty()
                ? subLabel
                : QStringLiteral("%1 > %2").arg(path, subLabel);
            collectFromMenu(sub, childPath, out);
            continue;
        }

        // Leaf action.
        Row r;
        r.menuPath    = path;
        r.actionText  = stripMnemonics(act->text());
        // QKeySequence::toString with PortableText keeps the human-readable
        // form ("Ctrl+S") rather than the platform-glyph variant.
        QKeySequence const ks = act->shortcut();
        r.shortcut    = ks.isEmpty() ? QString() : ks.toString(QKeySequence::NativeText);
        // Description: prefer toolTip if it differs from text, else statusTip,
        // else whatsThis.  Empty is fine - column just stays blank.
        QString tip = act->toolTip();
        if (tip == act->text()) tip.clear(); // Qt auto-fills toolTip from text.
        if (tip.isEmpty()) tip = act->statusTip();
        if (tip.isEmpty()) tip = act->whatsThis();
        r.description = stripMnemonics(tip);

        // Skip nameless actions (defensive - shouldn't happen for menu-bar
        // entries but Qt allows them).
        if (r.actionText.trimmed().isEmpty()) continue;

        out.append(r);
    }
}

ShortcutHelpDialog::ShortcutHelpDialog(QMenuBar* sourceBar, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard shortcuts"));
    setModal(true);
    resize(880, 560);

    auto* outer = new QVBoxLayout(this);

    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Filter:"), this));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("type to narrow - matches menu path or action text"));
    m_filter->setClearButtonEnabled(true);
    filterRow->addWidget(m_filter, 1);
    outer->addLayout(filterRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels(
        { tr("Menu path"), tr("Action"), tr("Shortcut"), tr("Description") });
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setSectionResizeMode(ColPath,        QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColAction,      QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColShortcut,    QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColDescription, QHeaderView::Stretch);
    outer->addWidget(m_table, 1);

    auto* closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    outer->addWidget(closeBox);

    connect(m_filter, &QLineEdit::textChanged, this, &ShortcutHelpDialog::onFilterChanged);

    // Walk the menu bar.  Top-level items are always submenus; their title
    // becomes the first segment of menuPath.
    if (sourceBar)
    {
        QList<QAction*> const topActs = sourceBar->actions();
        for (QAction* act : topActs)
        {
            if (!act || act->isSeparator()) continue;
            QMenu* topMenu = act->menu();
            if (!topMenu) continue;
            QString const topLabel = stripMnemonics(topMenu->title());
            collectFromMenu(topMenu, topLabel, m_rows);
        }
    }

    // Populate the table once - filtering is applied via row-hide, not
    // repopulation, so sort state survives a filter change.
    m_table->setSortingEnabled(false);
    m_table->setRowCount(m_rows.size());
    for (int i = 0; i < m_rows.size(); ++i)
    {
        Row const& r = m_rows.at(i);
        m_table->setItem(i, ColPath,        new QTableWidgetItem(r.menuPath));
        m_table->setItem(i, ColAction,      new QTableWidgetItem(r.actionText));
        m_table->setItem(i, ColShortcut,    new QTableWidgetItem(r.shortcut));
        m_table->setItem(i, ColDescription, new QTableWidgetItem(r.description));
    }
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(ColPath, Qt::AscendingOrder);
    m_table->resizeColumnsToContents();
}

void ShortcutHelpDialog::onFilterChanged(QString const& text)
{
    QString const needle = text.trimmed();
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        if (needle.isEmpty())
        {
            m_table->setRowHidden(row, false);
            continue;
        }
        QTableWidgetItem const* p = m_table->item(row, ColPath);
        QTableWidgetItem const* a = m_table->item(row, ColAction);
        QTableWidgetItem const* d = m_table->item(row, ColDescription);
        bool const match =
            (p && p->text().contains(needle, Qt::CaseInsensitive)) ||
            (a && a->text().contains(needle, Qt::CaseInsensitive)) ||
            (d && d->text().contains(needle, Qt::CaseInsensitive));
        m_table->setRowHidden(row, !match);
    }
}

} // namespace world_editor::app
