/*
 * CrashHandler - Windows top-level unhandled-exception filter.
 *
 * Wires SetUnhandledExceptionFilter so that any crash in the editor
 * (access violation, stack overflow, divide-by-zero, etc.) produces:
 *   1) a minidump (.dmp) suitable for opening in Visual Studio or
 *      WinDbg, written via dbghelp's MiniDumpWriteDump
 *   2) a sibling text log (.log) with timestamp, decoded exception
 *      code, faulting module + offset, OS version, executable path
 *      and a best-effort symbolic stack walk via StackWalk64
 *   3) a MessageBoxW pop-up surfacing the crashlog directory to the
 *      operator before the process exits.
 *
 * Output goes to:
 *   %LOCALAPPDATA%\TrinityCore\world_editor\crashlogs\crash-<ts>.{dmp,log}
 *
 * The handler is pure Win32 / dbghelp - it must not call into Qt
 * because the Qt event loop is not in a defined state inside an
 * unhandled-exception filter.  On non-Windows builds installCrashHandler
 * is a no-op.
 *
 * Install at the very top of main() (before QApplication) so even
 * early-startup crashes get a dump.
 */

#pragma once

namespace world_editor::app
{

// Install the top-level unhandled-exception filter.  Safe to call
// once at process start; subsequent calls are harmless but redundant.
// On non-Windows builds this is a no-op.
void installCrashHandler();

} // namespace world_editor::app
