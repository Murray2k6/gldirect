/*
 * gld_diag.h - Early diagnostic logger
 *
 * Writes directly to a file, bypassing the normal GLD log system, so a hang or
 * crash before/during driver initialization still leaves evidence.
 *
 * Two levels:
 *
 *   gldDiagLog()   always written, flushed immediately.  For anything that
 *                  happens a bounded number of times - initialization, context
 *                  creation, failures, unsupported paths.
 *
 *   gldDiagLogV()  per-call tracing, written only when verbose diagnostics are
 *                  switched on.  For anything that can happen once per GL call
 *                  or once per frame.
 *
 * The split exists because tracing every GL call is not free.  A frame issues
 * thousands of calls, and flushing each one to disk turned a running game into
 * an apparent hang while it wrote a 174 MB log.  Tracing has to be something
 * you switch on to diagnose a problem, not something every user pays for.
 *
 * Enable with either:
 *      set GLDIRECT_VERBOSE=1              (environment variable)
 *      dwDiagVerbose=1 in gldirect.ini     ([GLDirect] section, next to the exe)
 *
 * This header is declarations only.  The state and the bodies live in
 * gld_diag.c because they MUST be single-instance: when they were `static`
 * here, every including translation unit got its own FILE* on the same file,
 * and the resulting use of a stream after its lock had been destroyed
 * presented as an access violation deep inside ntdll that looked nothing like
 * a logging bug.  The full mechanism is documented at the top of gld_diag.c.
 */

#ifndef GLD_DIAG_H
#define GLD_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Always recorded, flushed immediately.  Safe from any thread, and a no-op
 * once logging has been shut down. */
void gldDiagLog(const char *fmt, ...);

/* Verbose per-call tracing; a no-op unless explicitly enabled.  Same thread
 * and shutdown guarantees as gldDiagLog. */
void gldDiagLogV(const char *fmt, ...);

/* Whether verbose diagnostics are currently active.  Cheap to call.  Lets a
 * site hold a one-time resolution and gate a large dump on the same flag
 * gldDiagLogV uses, so a whole multi-kilobyte source listing only reaches the
 * log when the user actually asked for it. */
int gldDiagVerboseGet(void);

/* Crash-path logging.  Opens its own handle per call and shares no state with
 * the functions above - no lock, no cached stream, no shutdown latch - so it
 * still works when the fault happened inside the logger itself, which is the
 * one situation where the report actually matters.  Use only from an exception
 * handler; it is far too slow for anything else. */
void gldDiagLogFatal(const char *fmt, ...);

/* Stops logging permanently and releases the stream.  Called at
 * DLL_PROCESS_DETACH.  Logging never resumes afterwards - deliberately,
 * because anything still calling in at that point is racing process teardown
 * and must not be allowed to touch a stream that is being destroyed. */
void gldDiagLogClose(void);

/* One-time fault flag.  Writes "FAULT FLAG [category] key" exactly once per
 * process per (category, key) pair, no matter how often the faulting call
 * repeats.  A game that calls a broken path once per frame must leave one
 * line, not one line per frame - the wrapper's own log discipline states that
 * anything that can happen per call must not be logged per call, and a fault
 * that repeats is still the same fault.  `key` may be a formatted string; it
 * is hashed, not stored, so a caller-owned buffer may be reused safely. */
void gldFlagFault(const char *category, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* GLD_DIAG_H */
