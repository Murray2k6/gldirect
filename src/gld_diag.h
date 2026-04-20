/*
 * gld_diag.h - Early diagnostic logger
 * Writes directly to a file, bypassing the normal GLD log system.
 * Used to diagnose hangs that occur before/during driver initialization.
 */

#ifndef GLD_DIAG_H
#define GLD_DIAG_H

#include <windows.h>
#include <stdio.h>

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
        g_diagFile = fopen("gldirect_diag.log", "a");
        if (!g_diagFile) return;
    }

    va_start(args, fmt);
    vfprintf(g_diagFile, fmt, args);
    va_end(args);
    fprintf(g_diagFile, "\n");
    fflush(g_diagFile);
}

#endif /* GLD_DIAG_H */
