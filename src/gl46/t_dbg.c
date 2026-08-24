#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../gld_log.h"
#include "../gld_diag.h"
#include "glsl_to_hlsl.h"

void gldLogPrintf(GLDLOG_severityType severity, LPSTR message, ...) { (void)severity; (void)message; }
void gldDiagLog(const char *fmt, ...) { (void)fmt; }

#include "glsl_to_hlsl.c"

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    static char buf[128 * 1024];
    size_t n;
    char *line;
    if (!f) return 1;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    line = buf;
    while (line && *line) {
        char *next = strchr(line, '\n');
        char *trimmed = glslSkipDeclQualifiers(line, NULL);
        if (strncmp(trimmed, "in ", 3) == 0 || strncmp(trimmed, "uniform ", 8) == 0) {
            printf("hit: trimmed=%.20s | c0=%d c1=%d c2=%d | in-match=%d int=%d semi=%d\n",
                   trimmed,
                   (unsigned char)trimmed[0], (unsigned char)trimmed[1], (unsigned char)trimmed[2],
                   strncmp(trimmed, "in ", 3) == 0,
                   strstr(trimmed, "int ") != NULL,
                   strchr(trimmed, ';') != NULL);
        }
        line = next ? next + 1 : NULL;
    }
    return 0;
}