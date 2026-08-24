#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../gld_log.h"
#include "../gld_diag.h"
#include "glsl_to_hlsl.h"

void gldLogPrintf(GLDLOG_severityType severity, LPSTR message, ...)
{
    (void)severity;
    va_list ap;
    va_start(ap, message);
    vfprintf(stdout, message, ap);
    va_end(ap);
    fprintf(stdout, "\n");
}

void gldDiagLog(const char *fmt, ...)
{
    FILE *f = fopen("C:\\Users\\icetr\\AppData\\Local\\Temp\\opencode\\diag_px.txt", "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

#include "glsl_to_hlsl.c"

static int g_pass = 0;
static int g_fail = 0;

static void runOne(const char *path)
{
    FILE *f = fopen(path, "rb");
    static char buf[64 * 1024];
    size_t n;
    int type = 1; /* pixel only */

    if (!f) { printf("  ! cannot open %s\n", path); return; }
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    {
        void *bytecode = NULL;
        DWORD size = 0;
        if (glslTranspileAndCompile(type, buf, &bytecode, &size)) {
            if (bytecode) glslFreeBytecode(bytecode);
            g_pass++;
            printf("  PASS %s (as pixel)\n", path);
            return;
        }
    }
    g_fail++;
    printf("  FAIL %s (as pixel)\n", path);
}

int main(int argc, char **argv)
{
    int i;

    if (!glslTranspilerInit()) {
        printf("transpiler init failed (d3dcompiler_47.dll?)\n");
        return 1;
    }

    for (i = 1; i < argc; i++)
        runOne(argv[i]);

    printf("\n%d pass, %d fail\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}