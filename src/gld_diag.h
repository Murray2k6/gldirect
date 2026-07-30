/*
 * gld_diag.h - Early diagnostic logger
 * Writes directly to a file, bypassing the normal GLD log system.
 * Used to diagnose hangs that occur before/during driver initialization.
 */

#ifndef GLD_DIAG_H
#define GLD_DIAG_H

#include <windows.h>
#include <stdio.h>
#include <string.h>

static FILE *g_diagFile = NULL;

static void gldDiagLogClose(void)
{
    if (g_diagFile) {
        fclose(g_diagFile);
        g_diagFile = NULL;
    }
}

static void gldDiagLog(const char *fmt, ...)
{
    va_list args;

    if (!g_diagFile) {
        /* The bare name resolves against the working directory, which a game
         * may set anywhere and which is often not writable.  Fall back to the
         * temp directory so the very first PROCESS_ATTACH lines are always
         * recorded — without them there is no way to tell a DLL that failed
         * to load from one that loaded but could not write its log. */
        g_diagFile = fopen("gldirect_diag.log", "a");
        if (!g_diagFile) {
            char szTempLog[MAX_PATH];
            DWORD dwLen = GetTempPathA(sizeof(szTempLog), szTempLog);
            if (dwLen > 0 && dwLen < sizeof(szTempLog) - 32) {
                strcat(szTempLog, "gldirect_diag.log");
                g_diagFile = fopen(szTempLog, "a");
            }
        }
        if (!g_diagFile) return;
    }

    va_start(args, fmt);
    vfprintf(g_diagFile, fmt, args);
    va_end(args);
    fprintf(g_diagFile, "\n");
    fflush(g_diagFile);
}

#endif /* GLD_DIAG_H */
