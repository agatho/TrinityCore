/*
 * DebugLog - Qt message handler routing qDebug/qInfo/qWarning/qCritical
 * output to a rolling file under %LOCALAPPDATA%\TrinityCore\world_editor\.
 *
 * Without this, qDebug on a GUI process on Windows vanishes into
 * OutputDebugString and is invisible unless DebugView is attached.
 * With it, the operator can paste log lines directly to a developer
 * without needing to launch the editor from a console.
 *
 * Output goes to:
 *   %LOCALAPPDATA%\TrinityCore\world_editor\debug.log
 *
 * Truncated on every editor launch (the previous run rolls to
 * debug.log.1) so the file doesn't grow unbounded.  Lines are
 * timestamped (ms precision) and prefixed with the message level.
 *
 * Install at the very top of main(), after the crash handler and
 * before QApplication.  Safe to call once per process.
 */

#pragma once

namespace world_editor::app
{

// Install the Qt message handler + open the debug.log file.
// Idempotent; subsequent calls are no-ops.
void installDebugLog();

} // namespace world_editor::app
