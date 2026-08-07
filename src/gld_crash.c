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

/* The unhandled-exception filter this DLL displaced at install time.  It has
 * to go back on the way out: leaving ours in place after the module is gone
 * points the process at freed memory. */
static LPTOP_LEVEL_EXCEPTION_FILTER g_prevFilter = NULL;

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

static volatile LONG g_inHandler = 0;

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
        /* MiniDumpWithDataSegs (0x1) | WithHandleData (0x4) |
         * WithIndirectlyReferencedMemory (0x40) | WithThreadInfo (0x1000) -
         * spelled numerically so this compiles without pulling in dbghelp.h.
         *
         * Deliberately not WithFullMemory: that walks every mapped region in
         * the process, and against a DXVK/RTX Remix d3d9.dll - which maps
         * large device-visible regions - dbgcore faulted inside the write and
         * took the fault report with it.  These flags keep the stacks, the
         * loaded module list and the memory the registers point at, which is
         * what the report is read for. */
        const DWORD type = 0x1 | 0x4 | 0x40 | 0x1000;

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

/*===========================================================================
 * Fault reporting
 *
 * Three things about the old reporter made a real fault unreadable, all of
 * them visible in a Wolfenstein log:
 *
 *   - Each line was emitted separately, so with id Tech's SMP threads the
 *     lines of one report arrived shuffled between another thread's normal
 *     logging.  It read as three faults beside unrelated calls when it was
 *     one fault and a busy second thread.  A report is now built whole and
 *     emitted in a single call.
 *
 *   - GetModuleHandleEx by address returned nothing for an address that was
 *     plainly inside d3d9.dll, and the report said "belongs to no loaded
 *     module".  VirtualQuery answers from the memory map instead, and can
 *     tell a genuine wild jump from a fault in real code.
 *
 *   - Everything was labelled FAULT whether or not the process survived it.
 *     A first-chance exception that something downstream handles now says
 *     so, and only the unhandled filter reports one that ended the process.
 *===========================================================================*/

/* Describe an address from the memory map: module + offset when it is inside
 * a mapped image, and what the region actually is when it is not. */
static void _gldDescribeAddress(const void *addr, char *out)
{
    MEMORY_BASIC_INFORMATION mbi;
    char  modPath[MAX_PATH];
    const char *leaf;

    out[0] = '\0';
    if (!addr) { wsprintfA(out, "NULL"); return; }

    ZeroMemory(&mbi, sizeof(mbi));
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) != sizeof(mbi)) {
        wsprintfA(out, "%p (unqueryable)", addr);
        return;
    }

    if (mbi.State != MEM_COMMIT) {
        wsprintfA(out, "%p (memory not committed - reserved or free)", addr);
        return;
    }

    if (mbi.Type == MEM_IMAGE &&
        GetModuleFileNameA((HMODULE)mbi.AllocationBase, modPath, MAX_PATH)) {
        leaf = strrchr(modPath, '\\');
        leaf = leaf ? leaf + 1 : modPath;
        wsprintfA(out, "%p = %s+0x%X", addr, leaf,
                  (unsigned)((const char *)addr - (const char *)mbi.AllocationBase));
        return;
    }

    /* Committed, but no module owns it: heap, stack or generated code.
     * Executing here is the signature of a call through a stale or corrupt
     * function pointer, which is worth saying outright. */
    wsprintfA(out, "%p (committed %s memory, protect 0x%X - no module owns it)",
              addr,
              mbi.Type == MEM_PRIVATE ? "private" :
              mbi.Type == MEM_MAPPED  ? "mapped"  : "unknown",
              (unsigned)mbi.Protect);
}

/* Append to a bounded buffer without ever running past its end. */
static void _gldAppend(char *buf, int cap, int *len, const char *text)
{
    int n = lstrlenA(text);
    if (*len + n >= cap - 1) n = cap - 1 - *len;
    if (n <= 0) return;
    CopyMemory(buf + *len, text, n);
    *len += n;
    buf[*len] = '\0';
}

/*
 * Build the whole report into one buffer.  fatal distinguishes an exception
 * that reached the unhandled filter - one that actually ended the process -
 * from a first-chance exception something downstream went on to handle.
 */
/*
 * Recover the caller when execution has jumped into unmapped memory.
 *
 * RtlCaptureStackBackTrace is worthless for that fault: the jump destroys the
 * frame chain, so the trace is the three ntdll dispatch frames and the bad
 * address itself, naming nobody.  But a call through a function pointer
 * pushes its return address first, so the instruction after the call is
 * still sitting at [rsp] — and the rest of the caller's frame is a little
 * further up.  Scanning the top of the stack for values that land inside a
 * loaded module recovers who made the call.
 */
static void _gldReportCallSite(EXCEPTION_POINTERS *pEP, char *buf, int cap, int *len)
{
    CONTEXT *ctx = pEP->ContextRecord;
    const ULONG_PTR *sp;
    char  line[600];
    char  desc[MAX_PATH + 128];
    int   found = 0;
    int   i;

    if (!ctx) return;

#if defined(_M_X64)
    sp = (const ULONG_PTR *)ctx->Rsp;
#else
    sp = (const ULONG_PTR *)(ULONG_PTR)ctx->Esp;
#endif
    if (!sp) return;

    _gldAppend(buf, cap, len,
        "\r\n***   the frame chain is gone, so the stack was scanned for the "
        "return address the call pushed:");

    /* 32 slots is well past any plausible argument spill area and still a
     * fixed, bounded read. */
    for (i = 0; i < 32; i++) {
        MEMORY_BASIC_INFORMATION mbi;
        ULONG_PTR value;

        /* The stack itself is the one thing that may already be damaged, so
         * every slot is probed before it is read. */
        if (VirtualQuery(&sp[i], &mbi, sizeof(mbi)) != sizeof(mbi) ||
            mbi.State != MEM_COMMIT)
            break;

        value = sp[i];
        if (value < 0x10000)
            continue;

        ZeroMemory(&mbi, sizeof(mbi));
        if (VirtualQuery((const void *)value, &mbi, sizeof(mbi)) != sizeof(mbi))
            continue;
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_IMAGE)
            continue;
        /* Only executable pages can be a return address. */
        if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                             PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            continue;

        _gldDescribeAddress((const void *)value, desc);
        wsprintfA(line, "\r\n***   [rsp+0x%02X] %s", (unsigned)(i * sizeof(ULONG_PTR)), desc);
        _gldAppend(buf, cap, len, line);

        if (++found >= 8)
            break;
    }

    if (!found)
        _gldAppend(buf, cap, len,
            "\r\n***   no return address found on the stack - the call may have "
            "been a jump, or the stack is damaged");
}

/*
 * An address that belongs to no module may still be where a module used to
 * be: a cached function pointer into a DLL that has since been unloaded
 * lands exactly here, and reads as free memory afterwards.  Walking down
 * from the fault reports the nearest image below it, which is what makes
 * that case recognisable.
 */
static void _gldReportNearestImage(const void *addr, char *buf, int cap, int *len)
{
    MEMORY_BASIC_INFORMATION mbi;
    char  line[600];
    char  modPath[MAX_PATH];
    const char *leaf;
    ULONG_PTR probe = ((ULONG_PTR)addr) & ~(ULONG_PTR)0xFFFF;
    int   steps;

    for (steps = 0; steps < 256 && probe >= 0x10000; steps++, probe -= 0x10000) {
        ZeroMemory(&mbi, sizeof(mbi));
        if (VirtualQuery((const void *)probe, &mbi, sizeof(mbi)) != sizeof(mbi))
            continue;
        if (mbi.Type != MEM_IMAGE || mbi.State != MEM_COMMIT)
            continue;
        if (!GetModuleFileNameA((HMODULE)mbi.AllocationBase, modPath, MAX_PATH))
            continue;

        leaf = strrchr(modPath, '\\');
        leaf = leaf ? leaf + 1 : modPath;
        wsprintfA(line,
            "\r\n***   nearest loaded image below the fault: %s at %p "
            "(fault is 0x%IX past its base)",
            leaf, mbi.AllocationBase,
            (SIZE_T)((const char *)addr - (const char *)mbi.AllocationBase));
        _gldAppend(buf, cap, len, line);
        return;
    }

    _gldAppend(buf, cap, len,
        "\r\n***   no loaded image within 16 MB below the fault address");
}

static void _gldBuildFaultReport(EXCEPTION_POINTERS *pEP, BOOL fatal,
                                 char *buf, int cap)
{
    EXCEPTION_RECORD *rec = pEP->ExceptionRecord;
    CONTEXT          *ctx = pEP->ContextRecord;
    void  *frames[32];
    USHORT captured, i;
    char   line[600];
    char   desc[MAX_PATH + 128];
    int    len = 0;
    const char *name = _gldExceptionName(rec->ExceptionCode);

    buf[0] = '\0';

    wsprintfA(line, "*** %s: %s (0x%08X) on thread %lu",
              fatal ? "FATAL" : "FIRST-CHANCE",
              name ? name : "exception",
              (unsigned)rec->ExceptionCode,
              (unsigned long)GetCurrentThreadId());
    _gldAppend(buf, cap, &len, line);

    _gldDescribeAddress(rec->ExceptionAddress, desc);
    wsprintfA(line, "\r\n***   faulting instruction at %s", desc);
    _gldAppend(buf, cap, &len, line);

    /* For an access violation the record carries what was attempted and
     * where, which is what separates a wild jump from a bad dereference. */
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        rec->NumberParameters >= 2) {
        const char *how = (rec->ExceptionInformation[0] == 0) ? "reading" :
                          (rec->ExceptionInformation[0] == 1) ? "writing" :
                          (rec->ExceptionInformation[0] == 8) ? "executing (DEP)" :
                                                                "accessing";
        _gldDescribeAddress((const void *)rec->ExceptionInformation[1], desc);
        wsprintfA(line, "\r\n***   while %s %s", how, desc);
        _gldAppend(buf, cap, &len, line);

        if (rec->ExceptionInformation[1] == (ULONG_PTR)rec->ExceptionAddress) {
            _gldAppend(buf, cap, &len,
                "\r\n***   fault address == instruction pointer: execution "
                "jumped somewhere it cannot execute, which is a call through "
                "a bad function pointer");
            _gldReportNearestImage(rec->ExceptionAddress, buf, cap, &len);
            _gldReportCallSite(pEP, buf, cap, &len);
        }
    }

    if (ctx) {
#if defined(_M_X64)
        wsprintfA(line, "\r\n***   rip=%p rsp=%p rbp=%p rax=%p rcx=%p rdx=%p",
                  (void *)ctx->Rip, (void *)ctx->Rsp, (void *)ctx->Rbp,
                  (void *)ctx->Rax, (void *)ctx->Rcx, (void *)ctx->Rdx);
#else
        wsprintfA(line, "\r\n***   eip=%p esp=%p ebp=%p eax=%p ecx=%p edx=%p",
                  (void *)(ULONG_PTR)ctx->Eip, (void *)(ULONG_PTR)ctx->Esp,
                  (void *)(ULONG_PTR)ctx->Ebp, (void *)(ULONG_PTR)ctx->Eax,
                  (void *)(ULONG_PTR)ctx->Ecx, (void *)(ULONG_PTR)ctx->Edx);
#endif
        _gldAppend(buf, cap, &len, line);
    }

    /* Skip this handler's own frames so the first line of the trace is the
     * code being investigated, not the code doing the reporting. */
    captured = RtlCaptureStackBackTrace(2, 32, frames, NULL);
    if (captured == 0) {
        _gldAppend(buf, cap, &len, "\r\n***   no stack frames captured");
        return;
    }

    wsprintfA(line, "\r\n***   stack (%u frames, innermost first):",
              (unsigned)captured);
    _gldAppend(buf, cap, &len, line);

    for (i = 0; i < captured; i++) {
        _gldDescribeAddress(frames[i], desc);
        wsprintfA(line, "\r\n***   [%2u] %s", (unsigned)i, desc);
        _gldAppend(buf, cap, &len, line);
    }
}

/* Emit a complete report in a single call, so no other thread's logging can
 * land in the middle of it. */
static void _gldReportFault(EXCEPTION_POINTERS *pEP, BOOL fatal)
{
    /* Static rather than stack: a fault is no time to touch the heap or to
     * put 8 KiB on a stack that may itself be the thing that overflowed.
     * g_inHandler serialises every caller. */
    static char buf[8192];

    _gldBuildFaultReport(pEP, fatal, buf, (int)sizeof(buf));
    gldDiagLogFatal("%s", buf);
}

/*
 * Unhandled exceptions - the ones that actually end the process.  The
 * vectored handler sees every exception first-chance, including the many a
 * runtime raises and handles internally; only this filter can say that
 * nothing downstream dealt with it.
 */
static LONG WINAPI _gldUnhandledFilter(EXCEPTION_POINTERS *pEP)
{
    if (pEP && pEP->ExceptionRecord) {
        _gldReportFault(pEP, TRUE);
        _gldWriteCrashDump(pEP);
    }
    /* Hand back to whatever the application installed before us. */
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG CALLBACK _gldVectoredHandler(EXCEPTION_POINTERS *pEP)
{
    EXCEPTION_RECORD *rec;
    const char *name;
    void *addr;

    if (!pEP || !pEP->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    rec  = pEP->ExceptionRecord;
    name = _gldExceptionName(rec->ExceptionCode);
    if (!name)
        return EXCEPTION_CONTINUE_SEARCH;   /* not fatal - normal traffic */

    /*
     * Re-entrancy guard.  Everything below runs inside an exception, and
     * anything it touches can raise one of its own - the dump writer did
     * exactly that, faulting inside dbgcore while reporting a fault in
     * d3d9.dll, so the log recorded the handler's own crash instead of the
     * one being investigated.  A nested fault now leaves immediately and the
     * original report survives.
     */
    if (InterlockedCompareExchange(&g_inHandler, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;

    addr = rec->ExceptionAddress;

    /* A faulting instruction inside a loop would otherwise fill the log with
     * thousands of identical entries.  Keyed by address rather than latched
     * once for the whole process, so a benign fault the wrapper raises and
     * handles itself cannot hide the one that follows it - see the note on
     * g_faultSites above. */
    if (!_gldFaultSiteIsNew(addr)) {
        InterlockedExchange(&g_inHandler, 0);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    /* The report itself is built and emitted by _gldReportFault, which
     * resolves addresses through the memory map and writes the whole
     * thing in one call.  The per-line emission that used to live here
     * could not attribute an address inside d3d9.dll and interleaved
     * with other threads. */
_gldReportFault(pEP, FALSE);

    InterlockedExchange(&g_inHandler, 0);

    /* Let the game's own handler run. This records, it does not intervene. */
    return EXCEPTION_CONTINUE_SEARCH;
}

void gldCrashHandlerInstall(void)
{
    if (g_hVeh)
        return;
    /* First in the chain, so an engine handler installed later cannot preempt it. */
    g_hVeh = AddVectoredExceptionHandler(1, _gldVectoredHandler);
    g_prevFilter = SetUnhandledExceptionFilter(_gldUnhandledFilter);
}

void gldCrashHandlerRemove(void)
{
    /*
     * Both registrations point at code inside this module.  Once it is
     * unloaded, a vectored handler left behind is called by ntdll on the very
     * next exception raised anywhere in the process - and jumps into freed
     * memory.  That is the access violation seen in Wolfenstein: execution at
     * an uncommitted address in the DLL range, reached through ntdll's
     * exception dispatch, on a nearly empty stack.
     */
    SetUnhandledExceptionFilter(g_prevFilter);
    g_prevFilter = NULL;

    if (g_hVeh) {
        RemoveVectoredExceptionHandler(g_hVeh);
        g_hVeh = NULL;
    }
}
