#include "main.h"
#include "AppDelegate.h"
#include "CCEGLView.h"
#include "PcBindings.h"
#include <signal.h>
#include <shellapi.h>
#include <dbghelp.h>
#include <string>
#pragma comment(lib, "dbghelp.lib")

USING_NS_CC;

// On abort/crash, write the call stack (with symbols where we have PDBs) to spike.log
static void logStack(const char* why)
{
    void* frames[62];
    USHORT n = CaptureStackBackTrace(0, 62, frames, NULL);
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, NULL, TRUE);
    char buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)buf;
    printf("[crash] %s, thread %lu\n", why, GetCurrentThreadId());
    for (USHORT i = 0; i < n; i++)
    {
        HMODULE mod = NULL;
        char path[MAX_PATH] = "?";
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)frames[i], &mod);
        if (mod) GetModuleFileNameA(mod, path, MAX_PATH);
        const char* base = strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        DWORD ldisp = 0;
        IMAGEHLP_LINE64 line = { sizeof(IMAGEHLP_LINE64) };
        BOOL hasSym = SymFromAddr(proc, (DWORD64)frames[i], &disp, sym);
        BOOL hasLine = SymGetLineFromAddr64(proc, (DWORD64)frames[i], &ldisp, &line);
        printf("  #%02u %s+0x%llx %s %s:%lu\n", i, base, (unsigned long long)((char*)frames[i] - (char*)mod),
               hasSym ? sym->Name : "", hasLine ? line.FileName : "", hasLine ? line.LineNumber : 0);
    }
    fflush(stdout);
}
static void __cdecl onAbort(int) { logStack("abort()"); }
static LONG WINAPI onUnhandled(EXCEPTION_POINTERS* e)
{
    char why[64];
    sprintf(why, "unhandled exception 0x%08lx", e->ExceptionRecord->ExceptionCode);
    logStack(why);
    return EXCEPTION_CONTINUE_SEARCH;
}
// first-chance exceptions on worker threads, logged where they are raised (before any unwinding)
static DWORD s_mainThread;
static LONG WINAPI onFirstChance(EXCEPTION_POINTERS* e)
{
    static volatile LONG logged = 0;
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (GetCurrentThreadId() == s_mainThread || code == 0x406D1388 /* thread naming */ || code < 0x80000000) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&logged) > 3) return EXCEPTION_CONTINUE_SEARCH;
    char why[96];
    sprintf(why, "first-chance 0x%08lx at %p", code, e->ExceptionRecord->ExceptionAddress);
    logStack(why);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void installCrashLogging()
{
    s_mainThread = GetCurrentThreadId();
    AddVectoredExceptionHandler(1, onFirstChance);
    signal(SIGABRT, onAbort);
    // the old system msvcrt.dll (used by pthreadVCE2) has its own abort/signal table
    typedef void (__cdecl *handler_t)(int);
    typedef handler_t (__cdecl *signal_t)(int, handler_t);
    signal_t oldSignal = (signal_t)GetProcAddress(LoadLibraryA("msvcrt.dll"), "signal");
    if (oldSignal) oldSignal(SIGABRT, onAbort);
    SetUnhandledExceptionFilter(onUnhandled);
}

// Command line (from the launcher): --width W --height H [--fullscreen] --log PATH [--debug]
struct Options
{
    int width = 1136;
    int height = 640;
    std::wstring log = L"game.log";
    bool debug = false;
};

static Options parseOptions()
{
    Options o;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; i++)
    {
        std::wstring a = argv[i];
        bool hasValue = i + 1 < argc;
        if (a == L"--width" && hasValue) o.width = _wtoi(argv[++i]);
        else if (a == L"--height" && hasValue) o.height = _wtoi(argv[++i]);
        else if (a == L"--log" && hasValue) o.log = argv[++i];
        else if (a == L"--debug") o.debug = true;
    }
    LocalFree(argv);
    if (o.width < 320) o.width = 1136;
    if (o.height < 240) o.height = 640;
    return o;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    Options opt = parseOptions();
    _wfreopen(opt.log.c_str(), L"w", stdout);
    _wfreopen(opt.log.c_str(), L"a", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    installCrashLogging();
    pc_set_debug(opt.debug);

    // Held while the game runs, so the launcher can tell (see launcher/src/launch.rs).
    HANDLE running = CreateMutexW(NULL, FALSE, L"Local\NinjaHeroesPC.Game");

    AppDelegate app;
    CCEGLView* eglView = CCEGLView::sharedOpenGLView();
    eglView->setViewName("Ninja Heroes");
    eglView->setFrameSize((float)opt.width, (float)opt.height);
    int ret = CCApplication::sharedApplication()->run();

    if (running) CloseHandle(running);
    return ret;
}
