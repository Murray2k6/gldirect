/*********************************************************************************
*  gl_dx9_compat.h — GL to D3D9 rendering pipeline
*
*  Routes ALL GL calls through D3D9. Mesa is only used for function resolution.
*  D3D9 is the actual renderer.
*********************************************************************************/

#ifndef GL_DX9_COMPAT_H
#define GL_DX9_COMPAT_H

#include <windows.h>
#include <d3d9.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Init / Shutdown */
void gldCompatInit(IDirect3DDevice9 *pDev);
void gldCompatShutdown(void);

/* Texture pipeline */
void gldCompatBindTexture(unsigned int target, unsigned int texture);
void gldCompatTexImage2D(unsigned int target, int level, int internalformat,
    int width, int height, int border, unsigned int format,
    unsigned int type, const void *pixels);
void gldCompatTexSubImage2D(unsigned int target, int level,
    int xoffset, int yoffset, int width, int height,
    unsigned int format, unsigned int type, const void *pixels);
void gldCompatCompressedTexImage2D(unsigned int target, int level,
    unsigned int internalformat, int width, int height, int border,
    int imageSize, const void *data);
void gldCompatDeleteTextures(int n, const unsigned int *textures);

/* Shader pipeline — GLSL to HLSL SM3.0 via D3D9 */
void gldCompatShaderSource(unsigned int shader, int count, const char *const*string, const int *length);
void gldCompatCompileShader(unsigned int shader);
unsigned int gldCompatCreateShader(unsigned int type);
void gldCompatDeleteShader(unsigned int shader);
unsigned int gldCompatCreateProgram(void);
void gldCompatDeleteProgram(unsigned int program);
void gldCompatAttachShader(unsigned int program, unsigned int shader);
void gldCompatLinkProgram(unsigned int program);
void gldCompatUseProgram(unsigned int program);

/* Uniforms — set D3D9 shader constants */
void gldCompatUniform1f(int location, float v0);
void gldCompatUniform2f(int location, float v0, float v1);
void gldCompatUniform3f(int location, float v0, float v1, float v2);
void gldCompatUniform4f(int location, float v0, float v1, float v2, float v3);
void gldCompatUniform1i(int location, int v0);
void gldCompatUniformMatrix4fv(int location, int count, unsigned char transpose, const float *value);
int  gldCompatGetUniformLocation(unsigned int program, const char *name);

/* Vertex attribute pipeline */
void gldCompatVertexAttribPointer(unsigned int index, int size, unsigned int type,
    unsigned char normalized, int stride, const void *pointer);
void gldCompatEnableVertexAttribArray(unsigned int index);
void gldCompatDisableVertexAttribArray(unsigned int index);

/* Draw calls — real D3D9 DrawPrimitiveUP */
void gldCompatDrawArrays(unsigned int mode, int first, int count);
void gldCompatDrawElements(unsigned int mode, int count, unsigned int type, const void *indices);

/* State */
void gldCompatViewport(int x, int y, int width, int height);
void gldCompatClear(unsigned int mask);
void gldCompatClearColor(float r, float g, float b, float a);
void gldCompatClearDepth(double depth);
void gldCompatEnable(unsigned int cap);
void gldCompatDisable(unsigned int cap);
void gldCompatBlendFunc(unsigned int sfactor, unsigned int dfactor);
void gldCompatDepthFunc(unsigned int func);
void gldCompatDepthMask(unsigned char flag);
void gldCompatCullFace(unsigned int mode);
void gldCompatFrontFace(unsigned int mode);
void gldCompatScissor(int x, int y, int width, int height);
void gldCompatColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

/* Matrix */
void gldCompatMatrixMode(unsigned int mode);
void gldCompatLoadMatrix(const float *m);
void gldCompatMultMatrix(const float *m);
void gldCompatLoadIdentity(void);
void gldCompatPushMatrix(void);
void gldCompatPopMatrix(void);
void gldCompatOrtho(double l, double r, double b, double t, double n, double f);
void gldCompatFrustum(double l, double r, double b, double t, double n, double f);

/* Frame boundary */
void gldCompatSwapBuffers(void);

/* Query: is the D3D9 compat layer active and has a device? */
BOOL gldCompatIsActive(void);

#ifdef __cplusplus
}
#endif

#endif
