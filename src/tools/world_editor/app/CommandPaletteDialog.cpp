#include "CommandPaletteDialog.h"

#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QShowEvent>
#include <QVBoxLayout>
#include <Qt>

#include <algorithm>

namespace world_editor::app
{

QString CommandPaletteDialog::stripMnemonics(QString const& text)
{
    // Walk char-by-char so we collapse "&&" -> "&" while dropping the
    // single-ampersand Qt-mnemonic markers.  Cheaper than a regex.
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

void CommandPaletteDialog::collectFromMenu(QMenu* menu, QString const& path)
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
            collectFromMenu(sub, childPath);
            continue;
        }

        // Leaf action.
        QString const actionLabel = stripMnemonics(act->text());
        if (actionLabel.trimmed().isEmpty()) continue;

        Entry e;
        e.action    = act;
        e.menuPath  = path;
        e.fullText  = path.isEmpty() ? actionLabel
                                     : QStringLiteral("%1 > %2").arg(path, actionLabel);
        e.lowerFull = e.fullText.toLower();
        m_entries.append(std::move(e));
    }
}

CommandPaletteDialog::CommandPaletteDialog(QMenuBar* sourceBar, QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Popup)
{
    setModal(true);
    setObjectName(QStringLiteral("CommandPaletteDialog"));
    // Subtle border so the frameless overlay reads as a distinct surface.
    setStyleSheet(QStringLiteral(
        "QDialog#CommandPaletteDialog { background: palette(window); "
        "border: 1px solid palette(mid); } "
        "QLineEdit { padding: 8px 10px; font-size: 14px; } "
        "QListWidget::item { padding: 6px 10px; }"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Type a command. Esc to dismiss."));
    m_filter->setClearButtonEnabled(true);
    outer->addWidget(m_filter);

    m_list = new QListWidget(this);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setFocusPolicy(Qt::NoFocus); // arrow-keys arrive at QLineEdit; we forward them
    outer->addWidget(m_list, 1);

    setLayout(outer);
    resize(640, 420);

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
            collectFromMenu(topMenu, topLabel);
        }
    }

    // Restore the recent-commands history from QSettings.
    QSettings settings;
    m_recent = settings.value(QStringLiteral("command_palette/recent")).toStringList();
    if (m_recent.size() > kRecentMax)
        m_recent = m_recent.mid(0, kRecentMax);

    connect(m_filter, &QLineEdit::textChanged, this, &CommandPaletteDialog::onFilterChanged);
    connect(m_filter, &QLineEdit::returnPressed, this, &CommandPaletteDialog::onItemActivated);
    connect(m_list, &QListWidget::itemActivated, this, &CommandPaletteDialog::onItemActivated);
    connect(m_list, &QListWidget::itemClicked, this, &CommandPaletteDialog::onItemActivated);

    // Filter key-events on the QLineEdit so Up/Down arrows navigate the
    // list instead of moving the text cursor.
    m_filter->installEventFilter(this);

    renderRecent();
}

void CommandPaletteDialog::positionOver(QWidget* host)
{
    if (!host) return;
    QRect const hostGeo = host->geometry();
    QPoint const hostTopLeft = host->mapToGlobal(QPoint(0, 0));
    int const w = std::min(width(), hostGeo.width() - 40);
    int const x = hostTopLeft.x() + (hostGeo.width() - w) / 2;
    int const y = hostTopLeft.y() + 80; // a little below the menu bar
    resize(w, height());
    move(x, y);
}

void CommandPaletteDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    m_filter->clear();        // start fresh each open
    m_filter->setFocus(Qt::PopupFocusReason);
    renderRecent();
}

bool CommandPaletteDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_filter && event->type() == QEvent::KeyPress)
    {
        auto* ke = static_cast<QKeyEvent*>(event);
        int const key = ke->key();
        if (key == Qt::Key_Down || key == Qt::Key_Up
            || key == Qt::Key_PageDown || key == Qt::Key_PageUp)
        {
            // Forward to the list so selection moves.
            int const count = m_list->count();
            if (count == 0)
                return true;
            int row = m_list->currentRow();
            int const step = (key == Qt::Key_PageDown || key == Qt::Key_PageUp) ? 8 : 1;
            if (key == Qt::Key_Down || key == Qt::Key_PageDown)
                row = std::min(count - 1, (row < 0 ? 0 : row + step));
            else
                row = std::max(0, (row < 0 ? 0 : row - step));
            m_list->setCurrentRow(row);
            return true;
        }
        if (key == Qt::Key_Escape)
        {
            reject();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPaletteDialog::onFilterChanged(QString const& text)
{
    QString const needle = text.trimmed();
    if (needle.isEmpty())
    {
        renderRecent();
        return;
    }
    rebuildList(needle);
}

void CommandPaletteDialog::rebuildList(QString const& needle)
{
    QString const lowerNeedle = needle.toLower();
    QString const lowerNeedleSpaced = QStringLiteral(" ") + lowerNeedle;

    QVector<Match> matches;
    matches.reserve(m_entries.size());

    for (int i = 0; i < m_entries.size(); ++i)
    {
        Entry const& e = m_entries.at(i);
        int const pos = e.lowerFull.indexOf(lowerNeedle);
        if (pos < 0) continue;

        Match m;
        m.entryIndex = i;
        m.position   = pos;

        if (pos == 0)
        {
            // Whole fullText starts with needle - strongest match.
            m.rank = 0;
        }
        else
        {
            // Find the action-text segment (after the last " > ").  If the
            // action label itself starts with needle, rank as prefix-match.
            int const sepIdx = e.lowerFull.lastIndexOf(QStringLiteral(" > "));
            int const actionStart = sepIdx < 0 ? 0 : sepIdx + 3;
            if (pos == actionStart)
                m.rank = 1;
            else if (e.lowerFull.indexOf(lowerNeedleSpaced) >= 0)
                m.rank = 2; // word-boundary midword
            else
                m.rank = 3; // pure substring
        }
        matches.append(m);
    }

    std::sort(matches.begin(), matches.end(), [](Match const& a, Match const& b) {
        if (a.rank != b.rank) return a.rank < b.rank;
        if (a.position != b.position) return a.position < b.position;
        return a.entryIndex < b.entryIndex;
    });

    if (matches.size() > kDisplayCap)
        matches.resize(kDisplayCap);

    m_list->clear();
    for (Match const& mm : matches)
    {
        Entry const& e = m_entries.at(mm.entryIndex);
        auto* item = new QListWidgetItem(e.fullText, m_list);
        item->setData(Qt::UserRole, mm.entryIndex);
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void CommandPaletteDialog::renderRecent()
{
    m_list->clear();
    if (m_entries.isEmpty()) return;

    // Build a lookup from fullText -> entry index so recent paths re-bind
    // to their actions (recent stores text only, not pointers).
    QHash<QString, int> byFull;
    byFull.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i)
        byFull.insert(m_entries.at(i).fullText, i);

    int shown = 0;
    for (QString const& path : std::as_const(m_recent))
    {
        auto it = byFull.constFind(path);
        if (it == byFull.constEnd()) continue;
        auto* item = new QListWidgetItem(path, m_list);
        item->setData(Qt::UserRole, it.value());
        if (++shown >= kRecentMax) break;
    }
    if (m_list->count() > 0)
    {
        m_list->setCurrentRow(0);
        return;
    }

    // No recent history yet -> seed the list with the first N entries so the
    // palette isn't blank on first launch.
    int const seedN = std::min<int>(kDisplayCap, m_entries.size());
    for (int i = 0; i < seedN; ++i)
    {
        Entry const& e = m_entries.at(i);
        auto* item = new QListWidgetItem(e.fullText, m_list);
        item->setData(Qt::UserRole, i);
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void CommandPaletteDialog::pushRecent(QString const& fullText)
{
    m_recent.removeAll(fullText);
    m_recent.prepend(fullText);
    while (m_recent.size() > kRecentMax)
        m_recent.removeLast();

    QSettings settings;
    settings.setValue(QStringLiteral("command_palette/recent"), m_recent);
}

void CommandPaletteDialog::onItemActivated()
{
    QListWidgetItem* item = m_list->currentItem();
    if (!item && m_list->count() > 0)
        item = m_list->item(0);
    if (!item) { reject(); return; }

    int const idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_entries.size()) { reject(); return; }

    Entry const& e = m_entries.at(idx);
    QString const fullText = e.fullText;
    QPointer<QAction> act = e.action;

    // Close BEFORE triggering so any modal the action opens isn't parented
    // over our soon-to-be-destroyed overlay.
    accept();

    if (act && act->isEnabled())
    {
        pushRecent(fullText);
        act->trigger();
    }
}

} // namespace world_editor::app
