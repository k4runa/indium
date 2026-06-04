// CrashHandler — Windows unhandled-exception filter that appends a crash report
// (exception code, faulting module + offset, and a module-relative backtrace) to
// crash.log next to the executable. This is the ONLY TU besides ScriptCompiler.cpp
// allowed to include <windows.h> — it never includes raylib, avoiding the
// Rectangle/CloseWindow/DrawText name clashes.
//
// Offsets are module-relative so they can be mapped back to source with addr2line
// even on an optimized build.

#if defined(_WIN32)
#include <windows.h>
#include <cstdio>
#include <ctime>

static void logModuleOffset(FILE* f, const char* tag, void* addr)
{
    HMODULE mod = nullptr;
    char    name[MAX_PATH] = {0};
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &mod) && mod)
    {
        GetModuleFileNameA(mod, name, MAX_PATH);
        const char* base = name;
        for (const char* p = name; *p; ++p) if (*p == '\\' || *p == '/') base = p + 1;
        unsigned long long off = (unsigned long long)((char*)addr - (char*)mod);
        fprintf(f, "%s %s +0x%llX\n", tag, base, off);
    }
    else
    {
        fprintf(f, "%s %p\n", tag, addr);
    }
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep)
{
    FILE* f = fopen("crash.log", "a");
    if (f)
    {
        time_t t = time(nullptr);
        fprintf(f, "\n=== CRASH %s", ctime(&t));
        fprintf(f, "exceptionCode = 0x%08lX\n", (unsigned long)ep->ExceptionRecord->ExceptionCode);
        logModuleOffset(f, "faultAddr     =", ep->ExceptionRecord->ExceptionAddress);

        fprintf(f, "backtrace:\n");
        void*  frames[40];
        USHORT n = CaptureStackBackTrace(0, 40, frames, nullptr);
        for (USHORT i = 0; i < n; i++)
        {
            char tag[16];
            snprintf(tag, sizeof(tag), "  [%02d]", i);
            logModuleOffset(f, tag, frames[i]);
        }
        fflush(f);
        fclose(f);
    }
    return EXCEPTION_EXECUTE_HANDLER; // let the process terminate
}

extern "C" void InstallCrashHandler()
{
    SetUnhandledExceptionFilter(CrashFilter);
}

#else
extern "C" void InstallCrashHandler() {}
#endif
