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
    char *workBuf;
    if (!f) { printf("cannot open %s\n", argv[1]); return 1; }
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    workBuf = (char *)malloc(HLSL_MAX_OUTPUT);
    strcpy(workBuf, buf);
    glslRemoveDeclarationLines(workBuf);
    printf("--- RemoveDeclarationLines ALONE on raw source ---\n");
    {
        char *hit = strstr(workBuf, "in vec4");
        if (hit) printf("'in vec4' SURVIVED alone\n");
        else printf("'in vec4' was blanked alone\n");
    }
    strcpy(workBuf, buf);
    glslRenameReservedWords(workBuf);
    glslStripVersionLines(workBuf);
    glslStripPrecisionQualifiers(workBuf);
    glslStripLayoutQualifiers(workBuf, NULL, NULL);
    glslExpandUniformBlocks(workBuf);
    {
        char *hit = strstr(workBuf, "in vec4 in_Position");
        printf("--- 'in vec4 in_Position' raw around hit ---\n");
        if (hit) {
            char *line = hit;
            while (line > workBuf && line[-1] != '\n') line--;
            {
                char *nl = strchr(line, '\n');
                printf("[");
                fwrite(line, 1, nl ? (int)(nl - line) : (int)strlen(line), stdout);
                printf("]\n");
            }
        } else printf("NOT FOUND\n");
    }
    glslRemoveDeclarationLines(workBuf);
    printf("--- workBuf after RemoveDeclarationLines (first 40 lines) ---\n");
    {
        char *line = workBuf;
        int i = 0;
        while (line && *line && i < 40) {
            char *nl = strchr(line, '\n');
            int len = nl ? (int)(nl - line) : (int)strlen(line);
            printf("%02d: [", i + 1);
            fwrite(line, 1, len, stdout);
            printf("]\n");
            line = nl ? nl + 1 : NULL;
            i++;
        }
    }
    free(workBuf);
    return 0;
}