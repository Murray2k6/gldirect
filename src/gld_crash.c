/*********************************************************************************
*  gld_crash.c - Record the faulting instruction before the game can hide it.
*
*  A "hard crash" in an id Tech title leaves nothing behind: the engine installs
*  its own unhandled-exception filter, so by the time Windows would write a WER
*  record the process has already been torn down by the game's own handler. The
*  Application log ends up with no entry at all, which reads like a silent exit
*  and gives nothing to debug.
*
*  A vectored exception handler runs ahead of every SEH frame and ahead of any
*  filter the process installed, so it sees the fault first regardless of what
*  the engine does with it afterwards. This one records what faulted and where -
*  including which module owns the faulting address, which is the single fact
*  that decides whether the bug is ours - and then declines to handle it, so the
*  game's own error path still runs exactly as before.
*
*  Only genuinely fatal codes are reported. First-chance C++ and CLR exceptions,
*  debugger notifications and the like are normal traffic in a running game and
*  are passed straight through.
*********************************************************************************/

#include <windows.h>
#include "gld_diag.h"

static PVOID  g_hVeh = NULL;

/*
 * Faults already reported, keyed by address.
 *
 * This used to be a single one-shot latch, on the reasoning that a fault in a
 * loop would otherwise fill the log. That was wrong in a way that cost several
 * debugging rounds: this wrapper raises access violations *deliberately*, inside
 * the __try/__except blocks that guard calls into a d3d9.dll it does not own.
 * Those are benign and handled - and one of them consumed the latch long before
 * the fault that actually killed the process, so every log came back with no
 * fault recorded at all and the crash looked like a silent exit.
 *
 * Keyed by address instead: a repeat at the same instruction stays quiet, but a
 * fault somewhere new is always reported no matter how many preceded it.
 */
#define GLD_MAX_FAULT_SITES  32
static void  *g_faultSites[GLD_MAX_FAULT_SITES];
static LONG   g_faultSiteCount = 0;
static LONG   g_dumpsWritten   = 0;

/* Returns TRUE the first time this address faults. */
static BOOL _gldFaultSiteIsNew(void *addr)
{
    LONG n = InterlockedCompareExchange(&g_faultSiteCount, 0, 0);
    LONG i;

    for (i = 0; i < n && i < GLD_MAX_FAULT_SITES; i++)
        if (g_faultSites[i] == addr)
            return FALSE;

    if (n < GLD_MAX_FAULT_SITES) {
        g_faultSites[n] = addr;
        InterlockedIncrement(&g_faultSiteCount);
    }
    return TRUE;
}

/*
 * Write a full-memory minidump next to the executable.
 *
 * Quake 4's own handler writes a *mini* dump, which carries registers and stack
 * but not the heap - and the question this crash keeps raising is what
 * overwrote a pointer, which can only be answered by looking at the memory
 * around it. A full dump answers that. Written from inside the process, so it
 * needs no debugger and no elevation, which matters because the game requires
 * elevation and cannot be launched under cdb from an ordinary prompt.
 *
 * dbghelp is bound at runtime rather than linked, so the project's link
 * settings are untouched and a machine without dbghelp.dll simply gets no dump
 * instead of failing to load the wrapper at all.
 */
typedef BOOL (WINAPI *FN_MiniDumpWriteDump)(HANDLE, DWORD, HANDLE, DWORD,
                                            PVOID, PVOID, PVOID);

/* Declared here rather than including dbghelp.h, which would also add a link
 * dependency.  Field-for-field identical to MINIDUMP_EXCEPTION_INFORMATION, so
 * the natural alignment matches on both x86 and x64. */
typedef struct {
    DWORD               ThreadId;
    EXCEPTION_POINTERS *ExceptionPointers;
    BOOL                ClientPointers;
} GLD_MINIDUMP_EXCEPTION_INFO;

static void _gldWriteCrashDump(EXCEPTION_POINTERS *pEP)
{
    HMODULE hDbg;
    FN_MiniDumpWriteDump pWrite;
    HANDLE hFile;
    char path[MAX_PATH];
    DWORD len;

    /* Bound: a fault storm must not fill the disk. */
    if (InterlockedIncrement(&g_dumpsWritten) > 3)
        return;

    hDbg = LoadLibraryA("dbghelp.dll");
    if (!hDbg)
        return;
    pWrite = (FN_MiniDumpWriteDump)GetProcAddress(hDbg, "MiniDumpWriteDump");
    if (!pWrite)
        return;

    len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return;
    while (len > 0 && path[len - 1] != '\\')
        len--;
    path[len] = '\0';
    if (len + 32 >= MAX_PATH)
        return;
    wsprintfA(path + len, "gldirect_crash_%lu.dmp",
              (unsigned long)InterlockedCompareExchange(&g_dumpsWritten, 0, 0));

    hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    {
        GLD_MINIDUMP_EXCEPTION_INFO mei;
        /* MiniDumpWithFullMemory (0x2) | WithHandleData (0x4) |
         * WithFullMemoryInfo (0x800) | WithThreadInfo (0x1000) - spelled
         * numerically so this compiles without pulling in dbghelp.h. */
        const DWORD type = 0x2 | 0x4 | 0x800 | 0x1000;

        mei.ThreadId          = GetCurrentThreadId();
        mei.ExceptionPointers = pEP;
        mei.ClientPointers    = FALSE;

        if (pWrite(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                   type, &mei, NULL, NULL))
            gldDiagLogFatal("*** FAULT: full memory dump written to %s", path);
        else
            gldDiagLogFatal("*** FAULT: MiniDumpWriteDump failed (%lu)",
                            (unsigned long)GetLastError());
    }

    CloseHandle(hFile);
}

static const char *_gldExceptionName(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
    case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE";
    default:                              return NULL;
    }
}

/*
 * Report the call stack, not just the faulting instruction.
 *
 * Knowing a fault happened inside ntdll says nothing useful on its own - ntdll
 * is where the heap manager, the loader and the CRT's syscall wrappers all
 * live, and it faults on behalf of whoever called it. The frame that matters
 * is the first one belonging to this DLL, because that is the call that handed
 * ntdll the bad value. Without it a heap fault looks identical no matter which
 * of a hundred allocations corrupted the heap, which is exactly the position
 * this crash left us in.
 *
 * Frames are printed as module + offset rather than raw addresses so they stay
 * meaningful under ASLR and can be matched against a map file for this build.
 */
static void _gldLogStackTrace(void)
{
    void *frames[32];
    USHORT captured, i;

    captured = RtlCaptureStackBackTrace(0, (DWORD)(sizeof(frames) / sizeof(frames[0])),
                                        frames, NULL);
    if (!captured) {
        gldDiagLogFatal("*** FAULT: no stack frames captured");
        return;
    }

    gldDiagLogFatal("*** FAULT: call stack (%u frames, innermost first):", (unsigned)captured);

    for (i = 0; i < captured; i++) {
        HMODULE hMod = NULL;
        char szMod[MAX_PATH];
        const char *leaf = "?";

        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)frames[i], &hMod) && hMod &&
            GetModuleFileNameA(hMod, szMod, MAX_PATH)) {
            const char *slash = strrchr(szMod, '\\');
            leaf = slash ? slash + 1 : szMod;
            gldDiagLogFatal("***   [%2u] %-20s + 0x%IX", (unsigned)i, leaf,
                       (SIZE_T)((char *)frames[i] - (char *)hMod));
        } else {
            gldDiagLogFatal("***   [%2u] %p  (no owning module)", (unsigned)i, frames[i]);
        }
    }
}

static LONG CALLBACK _gldVectoredHandler(EXCEPTION_POINTERS *pEP)
{
    EXCEPTION_RECORD *rec;
    const char *name;
    HMODULE hMod = NULL;
    char szMod[MAX_PATH];
    void *addr;

    if (!pEP || !pEP->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    rec  = pEP->ExceptionRecord;
    name = _gldExceptionName(rec->ExceptionCode);
    if (!name)
        return EXCEPTION_CONTINUE_SEARCH;   /* not fatal - normal traffic */

    addr = rec->ExceptionAddress;

    /* A faulting instruction inside a loop would otherwise fill the log with
     * thousands of identical entries.  Keyed by address rather than latched
     * once for the whole process, so a benign fault the wrapper raises and
     * handles itself cannot hide the one that follows it - see the note on
     * g_faultSites above. */
    if (!_gldFaultSiteIsNew(addr))
        return EXCEPTION_CONTINUE_SEARCH;

    szMod[0] = '\0';
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &hMod) && hMod) {
        if (!GetModuleFileNameA(hMod, szMod, MAX_PATH))
            szMod[0] = '\0';
    }

    gldDiagLogFatal("*** FAULT: %s (0x%08X) at %p", name,
               (unsigned)rec->ExceptionCode, addr);

    if (szMod[0]) {
        /* The offset is what makes the address usable: it survives ASLR, so it
         * can be matched against a map/PDB for this build. */
        gldDiagLogFatal("*** FAULT: module %s base %p offset 0x%IX",
                   szMod, (void *)hMod, (SIZE_T)((char *)addr - (char *)hMod));
    } else {
        gldDiagLogFatal("*** FAULT: address belongs to no loaded module "
                   "(jumped through a bad pointer)");
    }

    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        rec->NumberParameters >= 2) {
        static const char *op[] = { "reading", "writing", "executing" };
        ULONG_PTR kind = rec->ExceptionInformation[0];
        gldDiagLogFatal("*** FAULT: %s address %p",
                   (kind <= 2) ? op[kind] : "accessing",
                   (void *)rec->ExceptionInformation[1]);
    }

    _gldLogStackTrace();
    _gldWriteCrashDump(pEP);

    /* Let the game's own handler run. This records, it does not intervene. */
    return EXCEPTION_CONTINUE_SEARCH;
}

void gldCrashHandlerInstall(void)
{
    if (g_hVeh)
        return;
    /* First in the chain, so an engine handler installed later cannot preempt it. */
    g_hVeh = AddVectoredExceptionHandler(1, _gldVectoredHandler);
}

void gldCrashHandlerRemove(void)
{
    if (g_hVeh) {
        RemoveVectoredExceptionHandler(g_hVeh);
        g_hVeh = NULL;
    }
}
