/*
 * CrashHandler - Windows unhandled-exception filter implementation.
 *
 * See CrashHandler.h for the module summary.  Implementation notes:
 *
 *  - Reentrancy guard: an InterlockedExchange on a static LONG ensures
 *    that if the handler itself faults (e.g. dbghelp blows up inside
 *    StackWalk64 on a corrupted stack) we don't recurse forever - the
 *    second entry returns EXCEPTION_EXECUTE_HANDLER immediately so the
 *    OS terminates the process.
 *
 *  - dbghelp is single-threaded.  We don't lock around Sym* calls
 *    because we're already inside the top-level filter, which by
 *    definition runs on the faulting thread with the rest of the
 *    process effectively frozen (no other thread is going to call
 *    dbghelp - we never do at runtime).
 *
 *  - We avoid the CRT heap as far as possible in the filter (a heap
 *    corruption is one of the more likely things to land us here).
 *    Strings are formatted into fixed-size stack buffers.
 *
 *  - MessageBoxW is used over Qt's QMessageBox because Qt's event loop
 *    is not safe in this context; MessageBoxW spins its own modal loop.
 */

#include "CrashHandler.h"

#ifdef _WIN32

// dbghelp.h must follow windows.h
#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

#pragma comment(lib, "dbghelp.lib")

namespace world_editor::app
{

namespace
{

// Reentrancy guard.  0 = idle, 1 = in handler.
volatile LONG g_inHandler = 0;

// Best-effort recursive mkdir for a wide path.  Walks the path
// segment-by-segment and CreateDirectoryW each prefix; ignores
// ERROR_ALREADY_EXISTS.  Returns true if the leaf directory exists
// at the end (regardless of who created it).
bool mkdirRecursive(wchar_t const* path)
{
    if (!path || !*path)
        return false;

    wchar_t buf[MAX_PATH];
    size_t const len = wcsnlen(path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return false;
    wmemcpy(buf, path, len + 1);

    // Skip drive letter / leading slashes ("C:\" or "\\server\share\").
    wchar_t* p = buf;
    if (len >= 2 && buf[1] == L':')
        p = buf + 2;
    if (*p == L'\\' || *p == L'/')
        ++p;

    for (; *p; ++p)
    {
        if (*p == L'\\' || *p == L'/')
        {
            wchar_t const saved = *p;
            *p = L'\0';
            if (!CreateDirectoryW(buf, nullptr))
            {
                DWORD const err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS && err != ERROR_ACCESS_DENIED)
                {
                    // Fall through; the next CreateDirectoryW may still
                    // succeed if this segment is a drive root etc.
                }
            }
            *p = saved;
        }
    }
    if (!CreateDirectoryW(buf, nullptr))
    {
        DWORD const err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
            return GetFileAttributesW(buf) != INVALID_FILE_ATTRIBUTES;
    }
    return true;
}

// Resolve %LOCALAPPDATA%\TrinityCore\world_editor\crashlogs into outDir.
// Returns false on failure.
bool resolveCrashDir(wchar_t* outDir, size_t outCap)
{
    wchar_t* localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)) || !localAppData)
        return false;
    int const written = _snwprintf_s(outDir, outCap, _TRUNCATE,
                                     L"%s\\TrinityCore\\world_editor\\crashlogs", localAppData);
    CoTaskMemFree(localAppData);
    if (written <= 0)
        return false;
    mkdirRecursive(outDir);
    return true;
}

// Build a "<crashdir>\crash-YYYY-MM-DD_HH-MM-SS" stem (no extension).
// Returns false on failure.
bool buildTimestampStem(wchar_t* outStem, size_t outCap)
{
    wchar_t crashDir[MAX_PATH];
    if (!resolveCrashDir(crashDir, MAX_PATH))
        return false;
    SYSTEMTIME st;
    GetLocalTime(&st);
    int const written = _snwprintf_s(outStem, outCap, _TRUNCATE,
                                     L"%s\\crash-%04u-%02u-%02u_%02u-%02u-%02u",
                                     crashDir,
                                     st.wYear, st.wMonth, st.wDay,
                                     st.wHour, st.wMinute, st.wSecond);
    return written > 0;
}

// Decode the most common exception codes into a short human label.
char const* decodeExceptionCode(DWORD code)
{
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return "FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:       return "FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:             return "FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:          return "FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:            return "FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP:              return "SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
        case 0xE06D7363:                         return "MS C++ EXCEPTION (throw)";
        default:                                 return "UNKNOWN";
    }
}

// Find module + offset for a code address.  Writes "module!+0xNNNN" or
// "<unknown>" into out.
void describeAddress(HANDLE process, DWORD64 addr, char* out, size_t outCap)
{
    out[0] = '\0';
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(addr)),
                           &mod) && mod)
    {
        char modPath[MAX_PATH] = {};
        DWORD const got = GetModuleFileNameA(mod, modPath, MAX_PATH);
        char const* base = modPath;
        if (got > 0)
        {
            for (char const* p = modPath; *p; ++p)
                if (*p == '\\' || *p == '/')
                    base = p + 1;
        }
        DWORD64 const offset = addr - reinterpret_cast<DWORD64>(mod);
        _snprintf_s(out, outCap, _TRUNCATE, "%s+0x%llX",
                    *base ? base : "<module>", static_cast<unsigned long long>(offset));
    }
    else
    {
        _snprintf_s(out, outCap, _TRUNCATE, "<unknown>");
    }

    // Append SymFromAddr resolution if available.
    char symBuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
    memset(symBuf, 0, sizeof(symBuf));
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen   = 511;
    DWORD64 displacement = 0;
    if (SymFromAddr(process, addr, &displacement, sym))
    {
        size_t const cur = strnlen(out, outCap);
        if (cur + 4 < outCap)
            _snprintf_s(out + cur, outCap - cur, _TRUNCATE, " (%s+0x%llX)",
                        sym->Name, static_cast<unsigned long long>(displacement));
    }
}

// Write the textual crash log.  Best-effort: any failure is silently
// ignored - the goal is to produce as much useful diagnostic as we
// can without ever throwing or crashing the handler.
void writeCrashLog(wchar_t const* path, EXCEPTION_POINTERS* ep)
{
    HANDLE const f = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ,
                                 nullptr, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;

    auto writeStr = [&](char const* s)
    {
        DWORD written = 0;
        WriteFile(f, s, static_cast<DWORD>(strlen(s)), &written, nullptr);
    };

    char line[1024];
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(line, _TRUNCATE,
                "TrinityCore world_editor crash log\r\n"
                "Timestamp: %04u-%02u-%02u %02u:%02u:%02u.%03u\r\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    writeStr(line);

    // Executable path.
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    _snprintf_s(line, _TRUNCATE, "Executable: %s\r\n", exePath);
    writeStr(line);

    // Executable build timestamp (PE TimeDateStamp from the NT header).
    HMODULE const exeMod = GetModuleHandleA(nullptr);
    if (exeMod)
    {
        auto const* dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(exeMod);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE)
        {
            auto const* nt = reinterpret_cast<IMAGE_NT_HEADERS const*>(
                reinterpret_cast<BYTE const*>(exeMod) + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE)
            {
                DWORD const ts = nt->FileHeader.TimeDateStamp;
                _snprintf_s(line, _TRUNCATE,
                            "Build timestamp (PE): 0x%08X (unix epoch %u)\r\n",
                            ts, ts);
                writeStr(line);
            }
        }
    }

    // OS version.
    OSVERSIONINFOEXW osv = {};
    osv.dwOSVersionInfoSize = sizeof(osv);
#pragma warning(push)
#pragma warning(disable: 4996)
    GetVersionExW(reinterpret_cast<LPOSVERSIONINFOW>(&osv));
#pragma warning(pop)
    _snprintf_s(line, _TRUNCATE,
                "OS version: %u.%u build %u\r\n",
                osv.dwMajorVersion, osv.dwMinorVersion, osv.dwBuildNumber);
    writeStr(line);

    writeStr("\r\n");

    if (!ep || !ep->ExceptionRecord)
    {
        writeStr("(no EXCEPTION_POINTERS available)\r\n");
        CloseHandle(f);
        return;
    }

    EXCEPTION_RECORD const* er = ep->ExceptionRecord;
    DWORD const code = er->ExceptionCode;
    DWORD64 const addr = reinterpret_cast<DWORD64>(er->ExceptionAddress);

    _snprintf_s(line, _TRUNCATE,
                "Exception code:    0x%08X (%s)\r\n"
                "Exception flags:   0x%08X\r\n"
                "Exception address: 0x%016llX\r\n",
                code, decodeExceptionCode(code),
                er->ExceptionFlags,
                static_cast<unsigned long long>(addr));
    writeStr(line);

    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR)
    {
        if (er->NumberParameters >= 2)
        {
            char const* op = "?";
            switch (er->ExceptionInformation[0])
            {
                case 0: op = "read";    break;
                case 1: op = "write";   break;
                case 8: op = "execute"; break;
                default: break;
            }
            _snprintf_s(line, _TRUNCATE,
                        "Access type:       %s at 0x%016llX\r\n",
                        op,
                        static_cast<unsigned long long>(er->ExceptionInformation[1]));
            writeStr(line);
        }
    }

    // Faulting module + offset.
    HANDLE const process = GetCurrentProcess();
    SymSetOptions(SymGetOptions() | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    BOOL const symInit = SymInitialize(process, nullptr, TRUE);

    char addrDesc[1024];
    describeAddress(process, addr, addrDesc, sizeof(addrDesc));
    _snprintf_s(line, _TRUNCATE, "Faulting module:   %s\r\n\r\n", addrDesc);
    writeStr(line);

    // Stack walk.
    writeStr("Call stack:\r\n");

    CONTEXT context = *ep->ContextRecord;
    STACKFRAME64 frame = {};
    DWORD machine = 0;
#if defined(_M_X64) || defined(__x86_64__)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_IX86)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#elif defined(_M_ARM64)
    machine = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset    = context.Pc;
    frame.AddrFrame.Offset = context.Fp;
    frame.AddrStack.Offset = context.Sp;
#else
    machine = 0;
#endif
    frame.AddrPC.Mode    = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    HANDLE const thread = GetCurrentThread();
    int const maxFrames = 64;
    for (int i = 0; i < maxFrames; ++i)
    {
        if (!StackWalk64(machine, process, thread, &frame, &context,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (frame.AddrPC.Offset == 0)
            break;

        char desc[1024];
        describeAddress(process, frame.AddrPC.Offset, desc, sizeof(desc));

        // Source line if available.
        IMAGEHLP_LINE64 ln = {};
        ln.SizeOfStruct = sizeof(ln);
        DWORD lineDispl = 0;
        char srcSuffix[512] = "";
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDispl, &ln))
        {
            _snprintf_s(srcSuffix, _TRUNCATE, "  [%s:%u]", ln.FileName, ln.LineNumber);
        }

        _snprintf_s(line, _TRUNCATE,
                    "  #%02d 0x%016llX %s%s\r\n",
                    i,
                    static_cast<unsigned long long>(frame.AddrPC.Offset),
                    desc,
                    srcSuffix);
        writeStr(line);
    }

    if (symInit)
        SymCleanup(process);

    FlushFileBuffers(f);
    CloseHandle(f);
}

// Write the minidump.  Best-effort.
void writeMiniDump(wchar_t const* path, EXCEPTION_POINTERS* ep)
{
    HANDLE const f = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION mei = {};
    mei.ThreadId          = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers    = FALSE;

    // Full-stack dump + thread info; small enough on a Qt-sized exe
    // (typically 5-30 MB) and big enough to give a usable post-mortem.
    MINIDUMP_TYPE const type = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithDataSegs
        | MiniDumpWithHandleData
        | MiniDumpWithThreadInfo
        | MiniDumpWithUnloadedModules
        | MiniDumpWithIndirectlyReferencedMemory);

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                      f, type,
                      ep ? &mei : nullptr,
                      nullptr, nullptr);

    FlushFileBuffers(f);
    CloseHandle(f);
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* ep)
{
    // Reentrancy guard - if we re-enter (handler itself faulted) let
    // the OS terminate the process so we don't loop forever.
    if (InterlockedExchange(&g_inHandler, 1) != 0)
        return EXCEPTION_EXECUTE_HANDLER;

    wchar_t stem[MAX_PATH];
    bool const haveStem = buildTimestampStem(stem, MAX_PATH);

    wchar_t dmpPath[MAX_PATH] = L"";
    wchar_t logPath[MAX_PATH] = L"";
    if (haveStem)
    {
        _snwprintf_s(dmpPath, _TRUNCATE, L"%s.dmp", stem);
        _snwprintf_s(logPath, _TRUNCATE, L"%s.log", stem);
        writeMiniDump(dmpPath, ep);
        writeCrashLog(logPath, ep);
    }

    // Pop the path to the operator.  MessageBoxW is safe in this
    // context; Qt's QMessageBox is NOT.
    wchar_t body[2 * MAX_PATH + 256];
    if (haveStem)
    {
        _snwprintf_s(body, _TRUNCATE,
                     L"world_editor has crashed.\n\n"
                     L"A minidump and log have been written:\n\n"
                     L"  %s\n  %s\n\n"
                     L"Please attach both files when reporting the crash.",
                     dmpPath, logPath);
    }
    else
    {
        _snwprintf_s(body, _TRUNCATE,
                     L"world_editor has crashed.\n\n"
                     L"Failed to resolve %%LOCALAPPDATA%% - no crashlog written.");
    }
    MessageBoxW(nullptr, body, L"TrinityCore world_editor - crash",
                MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);

    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

void installCrashHandler()
{
    SetUnhandledExceptionFilter(&unhandledExceptionFilter);
}

} // namespace world_editor::app

#else // !_WIN32

namespace world_editor::app
{
void installCrashHandler() {}
} // namespace world_editor::app

#endif // _WIN32
