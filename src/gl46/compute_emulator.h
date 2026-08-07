/*********************************************************************************
* compute_emulator.h - software execution of GL compute stages with resource
*                      results returned to the direct D3D9 translator.
*********************************************************************************/

#ifndef GLD_COMPUTE_EMULATOR_H
#define GLD_COMPUTE_EMULATOR_H

#include <windows.h>
#include "gl_state.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL gldComputeEmulatorLink(GLS_Program *program, char *log, int logSize);
BOOL gldComputeEmulatorDispatch(GLS_Program *program,
                                unsigned int groupsX,
                                unsigned int groupsY,
                                unsigned int groupsZ,
                                char *log, int logSize);

typedef struct {
    float position[4];
    float varying[GLS_MAX_STAGE_VARYINGS][4];
    unsigned int primitiveSerial;
} GLD_StageVertex;

typedef struct {
    GLD_StageVertex *vertices;
    unsigned int vertexCount;
    unsigned int primitiveMode;
} GLD_StageDraw;

BOOL gldStageEmulatorLinkGraphics(GLS_Program *program, char *log, int logSize);
BOOL gldStageEmulatorDraw(GLS_Program *program, unsigned int mode,
                          int first, int count, unsigned int indexType,
                          const void *indices, int baseVertex,
                          int instanceCount, unsigned int baseInstance,
                          GLD_StageDraw *result, char *log, int logSize);
BOOL gldFragmentEmulatorDraw(GLS_Program *program, unsigned int mode,
                             int first, int count, unsigned int indexType,
                             const void *indices, int baseVertex,
                             int instanceCount, unsigned int baseInstance,
                             int width, int height,
                             const unsigned char *initialBGRA,
                             unsigned char *resultBGRA,
                             char *log, int logSize);
void gldFragmentEmulatorClear(unsigned int mask, float depth, int stencil);
void gldStageEmulatorFreeDraw(GLD_StageDraw *result);
void gldComputeEmulatorShutdown(void);

#ifdef __cplusplus
}
#endif

#endif
