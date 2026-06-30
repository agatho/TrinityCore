#include "DebugLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QtMessageHandler>

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace world_editor::app
{

namespace
{

QFile*     g_logFile        = nullptr;
QTextStream* g_logStream    = nullptr;
QMutex     g_logMutex;
QtMessageHandler g_previousHandler = nullptr;
bool       g_installed      = false;

char const* levelTag(QtMsgType t) noexcept
{
    switch (t)
    {
        case QtDebugMsg:    return "DBG";
        case QtInfoMsg:     return "INF";
        case QtWarningMsg:  return "WRN";
        case QtCriticalMsg: return "CRT";
        case QtFatalMsg:    return "FTL";
    }
    return "???";
}

void messageHandler(QtMsgType type, QMessageLogContext const& ctx, QString const& msg)
{
    QMutexLocker lock(&g_logMutex);
    if (g_logStream)
    {
        QString const ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        *g_logStream << ts << ' ' << levelTag(type) << ' ' << msg;
        if (ctx.category && ctx.category[0])
            *g_logStream << "  [" << ctx.category << ']';
        *g_logStream << '\n';
        g_logStream->flush();
    }
    // Also forward to stderr so launching from a console still shows
    // output; the default handler routes to OutputDebugString on
    // Windows GUI processes, which DebugView can pick up.
    std::fprintf(stderr, "%s %s\n", levelTag(type), msg.toLocal8Bit().constData());
}

} // namespace

void installDebugLog()
{
    if (g_installed)
        return;
    g_installed = true;

    // We are called BEFORE QApplication exists, so QStandardPaths cannot
    // see the org/app name and would route the log to the wrong place
    // (or return empty).  Mirror what CrashHandler does and read
    // %LOCALAPPDATA% directly so the destination matches the crashlog
    // directory: %LOCALAPPDATA%\TrinityCore\world_editor\debug.log.
    QString dir;
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD const n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
        dir = QString::fromWCharArray(buf, int(n)) + QStringLiteral("/TrinityCore/world_editor");
#endif
    if (dir.isEmpty())
    {
        // Non-Windows or no env var: best-effort fall back to QStandardPaths
        // (will still work if QApplication is initialised later — many
        // platforms don't gate the location on QCoreApplication state).
        dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);

    QString const path    = dir + "/debug.log";
    QString const rollPath = dir + "/debug.log.1";

    // Roll the previous run to .log.1 so we don't lose it on relaunch.
    if (QFile::exists(rollPath))
        QFile::remove(rollPath);
    if (QFile::exists(path))
        QFile::rename(path, rollPath);

    g_logFile = new QFile(path);
    if (!g_logFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete g_logFile;
        g_logFile = nullptr;
        return;
    }
    g_logStream = new QTextStream(g_logFile);

    QString const banner = QStringLiteral(
        "# world_editor debug log\n"
        "# path: %1\n"
        "# launched: %2\n"
        "# level prefixes: DBG=qDebug INF=qInfo WRN=qWarning CRT=qCritical FTL=qFatal\n")
        .arg(path, QDateTime::currentDateTime().toString(Qt::ISODate));
    *g_logStream << banner;
    g_logStream->flush();

    g_previousHandler = qInstallMessageHandler(&messageHandler);
}

} // namespace world_editor::app
