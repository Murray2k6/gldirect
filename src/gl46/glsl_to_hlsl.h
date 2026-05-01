/*********************************************************************************
*
*  ===============================================================================
*  |                  GLDirect: Direct3D Device Driver for Mesa.                 |
*  |                                                                             |
*  |                Copyright (C) 1997-2007 SciTech Software, Inc.               |
*  |                                                                             |
*  |Permission is hereby granted, free of charge, to any person obtaining a copy |
*  |of this software and associated documentation files (the "Software"), to deal|
*  |in the Software without restriction, including without limitation the rights |
*  |to use, copy, modify, merge, publish, distribute, sublicense, and/or sell    |
*  |copies of the Software, and to permit persons to whom the Software is        |
*  |furnished to do so, subject to the following conditions:                     |
*  |                                                                             |
*  |The above copyright notice and this permission notice shall be included in   |
*  |all copies or substantial portions of the Software.                          |
*  |                                                                             |
*  |THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR   |
*  |IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,     |
*  |FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  |
*  |AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER       |
*  |LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,|
*  |OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN    |
*  |THE SOFTWARE.                                                                |
*  ===============================================================================
*
* Language:     ANSI C
* Environment:  Windows 9x/NT/2000/XP (Win32)
*
* Description:  GLSL to HLSL Shader Model 3.0 transpiler for D3D9.
*               Performs text-based GLSL to HLSL conversion, compiles
*               via d3dcompiler_47.dll (loaded dynamically), and creates
*               IDirect3DVertexShader9 / IDirect3DPixelShader9 objects.
*
*********************************************************************************/

#ifndef GLSL_TO_HLSL_H
#define GLSL_TO_HLSL_H

#include <windows.h>
#include <d3d9.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the transpiler — loads d3dcompiler_47.dll */
BOOL glslTranspilerInit(void);

/* Transpile GLSL to HLSL and compile to D3D9 shader bytecode.
 * shaderType: 0 = vertex shader, 1 = pixel/fragment shader
 * glslSource: null-terminated GLSL source code
 * ppBytecode: receives compiled shader bytecode (caller must free with glslFreeBytecode)
 * pBytecodeSize: receives size of bytecode
 * Returns TRUE on success */
BOOL glslTranspileAndCompile(int shaderType, const char *glslSource,
    void **ppBytecode, DWORD *pBytecodeSize);

/* Create D3D9 vertex/pixel shader from compiled bytecode */
BOOL glslCreateVertexShader(IDirect3DDevice9 *pDev, const void *bytecode, DWORD size,
    IDirect3DVertexShader9 **ppShader);
BOOL glslCreatePixelShader(IDirect3DDevice9 *pDev, const void *bytecode, DWORD size,
    IDirect3DPixelShader9 **ppShader);

/* Free bytecode allocated by glslTranspileAndCompile */
void glslFreeBytecode(void *pBytecode);

/* Shut down — unload d3dcompiler */
void glslTranspilerShutdown(void);

#ifdef __cplusplus
}
#endif

#endif
