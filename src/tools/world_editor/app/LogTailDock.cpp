#include "LogTailDock.h"

#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

namespace world_editor::app
{

namespace
{
// QSettings key for the persisted tail-file path.
constexpr char kSettingsKey[] = "paths/log_tail_file";

// Pick a foreground color based on simple substring match.  Returns an
// invalid QColor when the default (theme) color should be used so the
// caller can short-circuit on the common "no special tint" case.
QColor pickColorForLine(QString const& line)
{
    if (line.contains(QStringLiteral("ERROR"), Qt::CaseSensitive))
        return QColor(0xE0, 0x40, 0x40);   // red
    if (line.contains(QStringLiteral("WARN"), Qt::CaseSensitive))
        return QColor(0xE0, 0xA0, 0x30);   // orange
    if (line.contains(QStringLiteral("DEBUG"), Qt::CaseSensitive))
        return QColor(0x90, 0x90, 0x90);   // gray
    return QColor{};                       // invalid -> default theme color
}
} // namespace

LogTailDock::LogTailDock(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // Top row: path edit + Set... + Pause/Resume.
    auto* row = new QHBoxLayout();
    row->setSpacing(4);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Path to worldserver log file..."));
    m_pathEdit->setToolTip(tr(
        "Absolute path to the log file the dock should tail.  Edits commit on "
        "Enter / focus-out and immediately rebind the poll target."));
    connect(m_pathEdit, &QLineEdit::editingFinished, this, &LogTailDock::onPathEdited);
    row->addWidget(m_pathEdit, /*stretch=*/1);

    m_setButton = new QPushButton(tr("Set..."), this);
    m_setButton->setToolTip(tr("Browse for a log file via the file dialog."));
    connect(m_setButton, &QPushButton::clicked, this, &LogTailDock::onSetClicked);
    row->addWidget(m_setButton);

    m_pauseButton = new QPushButton(tr("Pause"), this);
    m_pauseButton->setToolTip(tr("Toggle auto-polling.  Buffer is kept on pause."));
    m_pauseButton->setCheckable(true);
    connect(m_pauseButton, &QPushButton::clicked, this, &LogTailDock::onTogglePause);
    row->addWidget(m_pauseButton);

    root->addLayout(row);

    // Read-only text body with the last N lines, monospaced for log feel.
    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setMaximumBlockCount(kMaxLines);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    {
        QFont f = m_text->font();
        f.setStyleHint(QFont::Monospace);
        f.setFamily(QStringLiteral("Consolas"));
        m_text->setFont(f);
    }
    root->addWidget(m_text, /*stretch=*/1);

    m_timer = new QTimer(this);
    m_timer->setInterval(kPollMs);
    connect(m_timer, &QTimer::timeout, this, &LogTailDock::onPoll);

    // Auto-restore last session's path.  Empty string is a no-op.
    QSettings settings;
    QString const saved = settings.value(QString::fromLatin1(kSettingsKey)).toString();
    if (!saved.isEmpty())
    {
        m_pathEdit->setText(saved);
        setLogPath(saved);
    }
}

QString LogTailDock::currentPath() const
{
    return m_path;
}

void LogTailDock::setLogPath(QString const& path)
{
    m_path = path;
    m_offset = 0;
    m_pending.clear();
    m_text->clear();

    // Mirror to QSettings even when empty (clears the saved binding).
    QSettings settings;
    settings.setValue(QString::fromLatin1(kSettingsKey), m_path);

    if (m_pathEdit->text() != path)
        m_pathEdit->setText(path);

    if (m_path.isEmpty())
    {
        m_timer->stop();
        return;
    }

    // Seed: render up to the last `kMaxLines` lines of the existing file
    // so the operator sees recent history instead of a blank pane until
    // the first new write lands.
    //
    // CRITICAL: only read the tail of the file.  Reading the whole file
    // (Server.log can be 300+ MB) decodes UTF-8 into a UTF-16 QString
    // (2x blow-up), then split() builds millions of QString objects.
    // The combined allocation easily exceeds the process address space
    // and throws qBadAlloc.  Cap the seed read to a few hundred KB.
    constexpr qint64 kTailSeedBytes = qint64(512) * 1024; // 512 KB
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly))
    {
        qint64 const size = f.size();
        qint64 const start = (size > kTailSeedBytes) ? (size - kTailSeedBytes) : 0;
        f.seek(start);
        QByteArray tail = f.readAll();
        // If we seeked into the middle of a line, drop the first
        // (partial) line so we don't render a half-truncated entry.
        if (start > 0)
        {
            int const nl = tail.indexOf('\n');
            if (nl >= 0 && nl + 1 < tail.size())
                tail = tail.mid(nl + 1);
        }
        QStringList lines = QString::fromUtf8(tail).split(QChar(u'\n'));
        // Drop trailing empty token from terminating newline so we don't
        // emit a blank row at the bottom.
        if (!lines.isEmpty() && lines.back().isEmpty())
            lines.pop_back();
        if (lines.size() > kMaxLines)
            lines = lines.mid(lines.size() - kMaxLines);
        for (QString const& line : lines)
            appendColoredLine(line);
        m_offset = size;
        f.close();
    }

    if (!m_paused)
        m_timer->start();
}

void LogTailDock::setPaused(bool paused)
{
    m_paused = paused;
    m_pauseButton->setChecked(paused);
    m_pauseButton->setText(paused ? tr("Resume") : tr("Pause"));
    if (paused)
        m_timer->stop();
    else if (!m_path.isEmpty())
        m_timer->start();
}

void LogTailDock::onSetClicked()
{
    QString const start = !m_path.isEmpty()
        ? QFileInfo(m_path).absolutePath()
        : QString{};
    QString const picked = QFileDialog::getOpenFileName(
        this,
        tr("Select worldserver log file"),
        start,
        tr("Log files (*.log *.txt);;All files (*)"));
    if (picked.isEmpty())
        return;
    setLogPath(picked);
}

void LogTailDock::onTogglePause()
{
    setPaused(!m_paused);
}

void LogTailDock::onPathEdited()
{
    QString const typed = m_pathEdit->text().trimmed();
    if (typed == m_path)
        return;
    setLogPath(typed);
}

void LogTailDock::onPoll()
{
    if (m_paused || m_path.isEmpty())
        return;
    pollOnce();
}

void LogTailDock::pollOnce()
{
    QFile f(m_path);
    if (!f.exists())
        return;

    qint64 const size = QFileInfo(f).size();
    if (size < m_offset)
    {
        // Rotation: file was truncated / replaced under us.  Reset and
        // re-read from the start so we don't skip content.
        m_offset = 0;
        m_pending.clear();
    }
    if (size == m_offset)
        return;

    // Cap the per-poll read.  Worldserver can spew hundreds of MB between
    // polls when it's misbehaving (seen 11+ GB Server.log in the wild).
    // A naive `size - m_offset` read would allocate that whole chunk as a
    // QByteArray, then as a QString, and the editor's GUI thread would
    // spend seconds (or run out of memory) processing it.  When the gap
    // exceeds the cap, skip ahead and read only the last kMaxBytesPerPoll
    // bytes -- the operator-visible buffer is line-capped anyway, so the
    // intermediate content would be discarded immediately.
    constexpr qint64 kMaxBytesPerPoll = qint64(256) * 1024; // 256 KB
    qint64 readStart = m_offset;
    qint64 readLen   = size - m_offset;
    bool const skippedAhead = (readLen > kMaxBytesPerPoll);
    if (skippedAhead)
    {
        readStart = size - kMaxBytesPerPoll;
        readLen   = kMaxBytesPerPoll;
        // Throw away any half-line in m_pending -- it belongs to content
        // we are about to skip over.
        m_pending.clear();
    }

    if (!f.open(QIODevice::ReadOnly))
        return;
    if (!f.seek(readStart))
    {
        f.close();
        return;
    }
    QByteArray chunk = f.read(readLen);
    m_offset = f.pos();
    f.close();

    // If we skipped ahead, the first line in `chunk` may be a partial
    // line.  Drop everything up to the first newline so we don't render
    // a half-truncated entry.
    if (skippedAhead)
    {
        int const nl = chunk.indexOf('\n');
        if (nl >= 0 && nl + 1 < chunk.size())
            chunk = chunk.mid(nl + 1);
    }

    if (chunk.isEmpty())
        return;

    // Stitch onto any half-line carried over from the previous poll,
    // split on '\n', and emit complete lines.  The trailing token (if
    // any) is the new partial-line carry.
    QString const text = m_pending + QString::fromUtf8(chunk);
    QStringList const lines = text.split(QChar(u'\n'));
    int const completeCount = lines.size() - 1;
    for (int i = 0; i < completeCount; ++i)
        appendColoredLine(lines.at(i));
    m_pending = lines.back();

    enforceLineCap();
}

void LogTailDock::appendColoredLine(QString const& line)
{
    QTextCharFormat fmt;
    QColor const c = pickColorForLine(line);
    if (c.isValid())
        fmt.setForeground(c);

    QTextCursor cur(m_text->document());
    cur.movePosition(QTextCursor::End);
    // Insert an explicit block per line so maximumBlockCount() can prune
    // from the top deterministically.
    if (!m_text->document()->isEmpty())
        cur.insertBlock();
    cur.setCharFormat(fmt);
    cur.insertText(line);

    // Auto-scroll to bottom so newest lines stay visible.
    QTextCursor end = m_text->textCursor();
    end.movePosition(QTextCursor::End);
    m_text->setTextCursor(end);
}

void LogTailDock::enforceLineCap()
{
    // QPlainTextEdit's setMaximumBlockCount() handles the prune for us;
    // this is a defensive no-op kept for clarity of intent.
    if (m_text->blockCount() <= kMaxLines)
        return;
    QTextCursor cur(m_text->document());
    cur.movePosition(QTextCursor::Start);
    int const excess = m_text->blockCount() - kMaxLines;
    for (int i = 0; i < excess; ++i)
    {
        cur.select(QTextCursor::BlockUnderCursor);
        cur.removeSelectedText();
        cur.deleteChar(); // newline
    }
}

} // namespace world_editor::app
