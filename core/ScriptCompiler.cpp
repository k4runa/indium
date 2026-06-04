// ScriptCompiler.cpp — Windows-only child-process runner with no console window.
//
// This is the ONLY translation unit allowed to include <windows.h>: it never
// includes raylib, so the Rectangle/CloseWindow/DrawText name clashes that forced
// the rest of the engine to forward-declare Win32 functions don't apply here.
#if defined(_WIN32)

#include "ScriptCompiler.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>

namespace Indium
{
    int RunHiddenCommand(const std::string& command, std::string& outLog)
    {
        // Pipe for the child's merged stdout+stderr. bInheritHandle=TRUE so the
        // child inherits the write end.
        SECURITY_ATTRIBUTES sa{};
        sa.nLength        = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE readPipe = nullptr, writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
        {
            outLog = "RunHiddenCommand: CreatePipe failed.";
            return -1;
        }
        // The read end must NOT be inherited by the child.
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb         = sizeof(si);
        si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;                 // belt-and-suspenders with CREATE_NO_WINDOW
        si.hStdOutput = writePipe;
        si.hStdError  = writePipe;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

        // Run via cmd.exe /c so the existing quoted "g++ ... 2>&1" command line
        // parses exactly as it did under _popen.
        std::string fullCmd = "cmd.exe /c " + command;

        // CreateProcessA may modify the command buffer, so pass a writable copy.
        std::vector<char> cmdBuf(fullCmd.begin(), fullCmd.end());
        cmdBuf.push_back('\0');

        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessA(
            nullptr,            // application name (taken from command line)
            cmdBuf.data(),      // command line (writable)
            nullptr, nullptr,   // process/thread security
            TRUE,               // inherit handles (the write pipe)
            CREATE_NO_WINDOW,   // <-- the whole point: no console window flashes
            nullptr,            // inherit environment
            nullptr,            // inherit current directory
            &si, &pi);

        // Parent doesn't write to the child; close its copy of the write end so the
        // read loop sees EOF when the child exits.
        CloseHandle(writePipe);

        if (!ok)
        {
            CloseHandle(readPipe);
            outLog = "RunHiddenCommand: CreateProcess failed (error " +
                     std::to_string((unsigned long)GetLastError()) + ").";
            return -1;
        }

        // Drain the pipe until the child closes its write end.
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(readPipe, buf, sizeof(buf), &n, nullptr) && n > 0)
            outLog.append(buf, n);
        CloseHandle(readPipe);

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (int)exitCode;
    }
}

#endif // _WIN32
