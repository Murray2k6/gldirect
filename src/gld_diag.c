/*
 * gld_diag.c - Diagnostic logger state and implementation.
 *
 * This lives in a .c file rather than the header for a reason that cost six
 * rounds of misdiagnosis to find.
 *
 * The state used to be `static` in gld_diag.h, so each of the fifteen
 * translation units that include it got its OWN FILE* opened against the same
 * path. gldDiagLogClose() at DLL_PROCESS_DETACH closed exactly one of them;
 * the C runtime tore down the other fourteen as part of its own shutdown, and
 * tearing down a stream deletes the critical section that guards it.
 *
 * A game thread still inside an exported entry point at that moment - and a
 * heavily threaded engine always has one - would reach vfprintf, which takes
 * the stream lock, and enter a critical section that had already been deleted.
 * RtlDeleteCriticalSection leaves DebugInfo set to (PVOID)-1, so the entry
 * path dereferences -1 and the process dies with
 *
 *     ACCESS_VIOLATION reading address FFFFFFFFFFFFFFFF   inside ntdll
 *
 * which looks exactly like heap corruption from somewhere else entirely. It is
 * not. It is the logger being used after it was destroyed.
 *
 * Three things make that impossible now:
 *   - one FILE*, in one translation unit, instead of fifteen
 *   - an SRWLOCK, which is statically initialised and never deleted, so there
 *     is no window in which the guard itself is invalid
 *   - a one-way shutdown latch: once logging has stopped it never restarts, so
 *     a late call is a no-op rather than a resurrection of a dead stream
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "gld_diag.h"

/* SRWLOCK_INIT needs no initialisation call and has no destroy call, so unlike
 * a CRITICAL_SECTION it cannot be used after deletion - there is nothing to
 * delete. That property is the entire point here. */
static SRWLOCK  g_diagLock     = SRWLOCK_INIT;
static FILE    *g_diagFile     = NULL;
static LONG     g_diagShutdown = 0;   /* one-way: set at teardown, never cleared */
static LONG     g_diagOwner    = 0;   /* thread id currently inside the lock, 0 = none */
static int      g_diagVerbose  = -1;  /* -1 = not resolved yet */
static unsigned g_diagPending  = 0;   /* verbose lines written since last flush */

/*
 * Enter the log, distinguishing the two ways the lock can be unavailable.
 *
 * An SRWLOCK is not recursive, and these functions are reachable from the
 * vectored exception handler: a fault raised while this same thread was
 * already inside a write would re-enter and deadlock the process instead of
 * recording the crash. That case - and only that case - must give up.
 *
 * Contention from a *different* thread is ordinary and must NOT give up:
 * dropping those lines makes the log lossy, so a short log stops meaning "it
 * failed early" and starts meaning "maybe it just lost the evidence", which
 * removes the only thing this log is for. Those callers wait.
 */
static BOOL gldDiagEnter(void)
{
    LONG me = (LONG)GetCurrentThreadId();

    if (InterlockedCompareExchange(&g_diagOwner, 0, 0) == me)
        return FALSE;                  /* re-entrant on this thread - would deadlock */

    AcquireSRWLockExclusive(&g_diagLock);
    InterlockedExchange(&g_diagOwner, me);
    return TRUE;
}

static void gldDiagLeave(void)
{
    InterlockedExchange(&g_diagOwner, 0);
    ReleaseSRWLockExclusive(&g_diagLock);
}

/* Resolved once, from the environment or the ini beside the executable.
 * Callers hold the lock. */
static int gldDiagVerboseResolved(void)
{
    if (g_diagVerbose < 0) {
        char buf[16];
        DWORD n = GetEnvironmentVariableA("GLDIRECT_VERBOSE", buf, sizeof(buf));

        g_diagVerbose = (n > 0 && n < sizeof(buf) && buf[0] && buf[0] != '0') ? 1 : 0;

        if (!g_diagVerbose) {
            char iniPath[MAX_PATH];
            DWORD len = GetModuleFileNameA(NULL, iniPath, MAX_PATH);
            if (len > 0 && len < MAX_PATH) {
                char *slash = strrchr(iniPath, '\\');
                if (slash && (size_t)(slash - iniPath) < MAX_PATH - 16) {
                    strcpy(slash + 1, "gldirect.ini");
                    if (GetPrivateProfileIntA("GLDirect", "dwDiagVerbose", 0, iniPath))
                        g_diagVerbose = 1;
                }
            }
        }
    }
    return g_diagVerbose;
}

/* Callers hold the lock. */
static void gldDiagOpenLocked(void)
{
    if (g_diagFile)
        return;

    /* The bare name resolves against the working directory, which a game may
     * set anywhere and which is often not writable.  Fall back to the temp
     * directory so the very first PROCESS_ATTACH lines are always recorded -
     * without them there is no way to tell a DLL that failed to load from one
     * that loaded but could not write its log. */
    g_diagFile = fopen("gldirect_diag.log", "a");
    if (!g_diagFile) {
        char szTempLog[MAX_PATH];
        DWORD dwLen = GetTempPathA(sizeof(szTempLog), szTempLog);
        if (dwLen > 0 && dwLen < sizeof(szTempLog) - 32) {
            strcat(szTempLog, "gldirect_diag.log");
            g_diagFile = fopen(szTempLog, "a");
        }
    }
}

/*
 * Crash-path logging: shares no state with the normal logger, on purpose.
 *
 * The ordinary path cannot be used from an exception handler. If the fault
 * happened inside the logger itself - which is exactly what a corrupted or
 * torn-down stream produces - then this thread already owns the lock, the
 * re-entrancy guard refuses, and the crash report is silently discarded. That
 * is how two real access violations reached the Windows event log while our
 * own diagnostic recorded nothing at all.
 *
 * So this opens its own handle, writes, flushes and closes, touching neither
 * the shared FILE* nor the lock nor the shutdown latch. It is slower per line,
 * which does not matter when the process is already dying, and it cannot be
 * blocked or suppressed by whatever state the failure left behind. A crash
 * report that only works when nothing is wrong is worthless.
 */
void gldDiagLogFatal(const char *fmt, ...)
{
    va_list args;
    FILE *f;

    f = fopen("gldirect_diag.log", "a");
    if (!f) {
        char szTempLog[MAX_PATH];
        DWORD dwLen = GetTempPathA(sizeof(szTempLog), szTempLog);
        if (dwLen > 0 && dwLen < sizeof(szTempLog) - 32) {
            strcat(szTempLog, "gldirect_diag.log");
            f = fopen(szTempLog, "a");
        }
    }
    if (!f)
        return;

    __try {
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fputc('\n', f);
        fflush(f);
    } __finally {
        fclose(f);
    }
}

void gldDiagLogClose(void)
{
    FILE *doomed = NULL;
    int spins;

    /* Latch first, so a thread about to log sees the shutdown before the
     * stream goes away rather than after.  From here on every new writer
     * returns immediately, so the only threads that matter are the ones
     * already inside a write. */
    InterlockedExchange(&g_diagShutdown, 1);

    /* Wait briefly for those in-flight writers, but never block indefinitely.
     * This runs from DLL_PROCESS_DETACH, under the loader lock: blocking here
     * on a thread that itself needs the loader lock would hang the process on
     * exit, which is a worse failure than the one being fixed. */
    for (spins = 0; spins < 100; spins++) {
        if (TryAcquireSRWLockExclusive(&g_diagLock)) {
            doomed = g_diagFile;
            g_diagFile = NULL;
            ReleaseSRWLockExclusive(&g_diagLock);
            break;
        }
        Sleep(1);
    }

    /* Only closed once it is unreachable, so nothing can still be writing to
     * it.  If the lock never came free the handle is deliberately leaked: the
     * process is exiting, and leaking a FILE* costs nothing, whereas closing
     * one that another thread is still inside is exactly the use-after-free
     * this whole file exists to prevent. */
    if (doomed) {
        fflush(doomed);
        fclose(doomed);
    }
}

/* Always recorded, always flushed.  See gldDiagEnter for the locking rule. */
void gldDiagLog(const char *fmt, ...)
{
    va_list args;

    if (InterlockedCompareExchange(&g_diagShutdown, 0, 0) != 0)
        return;

    if (!gldDiagEnter())
        return;

    /* __finally, not a plain call: if the write faults, structured unwinding
     * would otherwise skip the release and leave the lock held forever, so
     * every later log call on every thread would block and the process would
     * hang. A logger must not be able to deadlock the program it is recording,
     * least of all while recording the fault that is killing it. */
    __try {
        gldDiagOpenLocked();
        if (g_diagFile) {
            va_start(args, fmt);
            vfprintf(g_diagFile, fmt, args);
            va_end(args);
            fputc('\n', g_diagFile);
            fflush(g_diagFile);
            g_diagPending = 0;
        }
    } __finally {
        gldDiagLeave();
    }
}

/*
 * Per-call tracing.  Off unless explicitly enabled, and when enabled it
 * batches flushes: losing at most the last few hundred lines to a hard crash
 * is a fair trade for a log that does not dominate frame time.  Anything that
 * must survive a crash should use gldDiagLog instead.
 */
void gldDiagLogV(const char *fmt, ...)
{
    va_list args;

    if (InterlockedCompareExchange(&g_diagShutdown, 0, 0) != 0)
        return;

    if (!gldDiagEnter())
        return;

    /* __finally for the same reason as gldDiagLog; see the note there. */
    __try {
        if (gldDiagVerboseResolved()) {
            gldDiagOpenLocked();
            if (g_diagFile) {
                va_start(args, fmt);
                vfprintf(g_diagFile, fmt, args);
                va_end(args);
                fputc('\n', g_diagFile);

                if (++g_diagPending >= 256) {
                    fflush(g_diagFile);
                    g_diagPending = 0;
                }
            }
        }
    } __finally {
        gldDiagLeave();
    }
}
