#pragma once
//
// ScriptCompiler — run the script compiler as a child process WITHOUT flashing a
// console window.
//
// On Windows, ScriptManager used _popen("cmd /c ..."), which spawns a visible
// cmd.exe window every time scripts compile (jarring, and looks like malware to
// end users). The fix needs CreateProcess with CREATE_NO_WINDOW — but <windows.h>
// collides with raylib's Rectangle/CloseWindow/DrawText, so it must NOT be pulled
// into the engine headers. This declaration is implemented in ScriptCompiler.cpp,
// the one translation unit that includes <windows.h> and never includes raylib.
//
// On non-Windows there is no console-flash problem; ScriptManager keeps using
// popen() directly and this helper is Windows-only.
#if defined(_WIN32)

#include <string>

namespace Indium
{
    // Run `command` (a full shell command line) with no visible window, capturing
    // merged stdout+stderr into `outLog`. Returns the process exit code, or -1 if
    // the process could not be started.
    int RunHiddenCommand(const std::string& command, std::string& outLog);
}

#endif
