#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../gld_log.h"
#include "../gld_diag.h"
#include "glsl_to_hlsl.h"

void gldLogPrintf(GLDLOG_severityType severity, LPSTR message, ...) { (void)severity; (void)message; }
void gldDiagLog(const char *fmt, ...) { (void)fmt; }

#include "glsl_to_hlsl.c"

int main(void)
{
    char src[] = "in vec4 in_Position;\n"
                 "in vec4 in_Normal;\n"
                 "uniform vec4 _va_[7];\n"
                 "vec3 globalNormal;\n"
                 "void main() {\n"
                 "\tvec4 localPosition;\n"
                 "\tlocalPosition = in_Position;\n"
                 "}\n";
    glslRenameReservedWords(src);
    printf("AFTER RENAME:\n%s\n", src);
    glslRemoveDeclarationLines(src);
    printf("BLANKED:\n%s\n", src);
    return 0;
}