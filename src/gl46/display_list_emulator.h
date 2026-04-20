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
* Description:  Display list emulation for OpenGL 4.6 core profile.
*
*               OpenGL core profile removes display list support
*               (glNewList, glEndList, glCallList, glDeleteLists, etc.).
*               This module provides a simple command recording and
*               playback mechanism that stores function pointer + argument
*               data for each recorded GL command, allowing legacy
*               applications that rely on display lists to function.
*
*               Uses a linear array hash map with a maximum of 1024 lists.
*
*********************************************************************************/

#ifndef __GL46_DISPLAY_LIST_EMULATOR_H
#define __GL46_DISPLAY_LIST_EMULATOR_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/*
 * Maximum number of display lists that can be active simultaneously.
 */
#define GLD_DL_MAX_LISTS        1024

/*
 * Maximum number of commands per display list.
 */
#define GLD_DL_MAX_COMMANDS     4096

/*
 * Maximum nesting depth for display list calls (glCallList within a list).
 */
#define GLD_DL_MAX_NESTING      64

/*
 * Maximum size of argument data per command (bytes).
 * Covers up to 16 floats (a 4x4 matrix) plus some extra.
 */
#define GLD_DL_MAX_ARG_SIZE     128

/*
 * Legacy display list mode constants (removed from core profile headers).
 */
#ifndef GL_COMPILE
#define GL_COMPILE              0x1300
#endif
#ifndef GL_COMPILE_AND_EXECUTE
#define GL_COMPILE_AND_EXECUTE  0x1301
#endif

/*
 * GLD_dlCommandFunc — function pointer type for recorded commands.
 * Each command is replayed by calling this function with a pointer
 * to the serialized argument data.
 */
typedef void (*GLD_dlCommandFunc)(const void *argData);

/*
 * GLD_dlCommand — a single recorded display list command.
 *
 * Fields:
 *   func    — function to call during playback.
 *   argSize — number of bytes of argument data.
 *   argData — serialized argument data (copied at record time).
 */
typedef struct {
    GLD_dlCommandFunc   func;
    int                 argSize;
    unsigned char       argData[GLD_DL_MAX_ARG_SIZE];
} GLD_dlCommand;

/*
 * GLD_displayList — a complete recorded display list.
 *
 * Fields:
 *   listId   — the GL list name (1-based).
 *   inUse    — TRUE if this slot contains a valid list.
 *   commands — array of recorded commands.
 *   cmdCount — number of commands recorded.
 */
typedef struct {
    GLuint          listId;
    BOOL            inUse;
    GLD_dlCommand  *commands;
    int             cmdCount;
    int             cmdCapacity;
} GLD_displayList;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Begin recording a new display list.
 *
 * Allocates a command buffer for the given list ID and sets the
 * recording mode.  If a list with the same ID already exists, it
 * is replaced.
 *
 * Parameters:
 *   listId — the display list name (must be > 0).
 *   mode   — GL_COMPILE or GL_COMPILE_AND_EXECUTE.
 */
void gldNewList46(GLuint listId, GLenum mode);

/*
 * Finalize the current display list recording.
 *
 * Stores the completed command buffer in the list hash map.
 * Must be called after gldNewList46.
 */
void gldEndList46(void);

/*
 * Replay a previously recorded display list.
 *
 * Looks up the list by ID and executes each recorded command in order.
 * Supports nested calls up to GLD_DL_MAX_NESTING depth.
 *
 * Parameters:
 *   listId — the display list name to replay.
 */
void gldCallList46(GLuint listId);

/*
 * Delete a range of display lists.
 *
 * Frees command buffers for list IDs in [base, base+range).
 *
 * Parameters:
 *   base  — first list ID to delete.
 *   range — number of consecutive lists to delete.
 */
void gldDeleteLists46(GLuint base, GLsizei range);

/*
 * Generate a contiguous range of unused display list names.
 *
 * Parameters:
 *   range — number of consecutive list names to allocate.
 *
 * Returns:
 *   The first list name in the allocated range, or 0 on failure.
 */
GLuint gldGenLists46(GLsizei range);

/*
 * Check if a display list name is in use.
 *
 * Parameters:
 *   listId — the list name to check.
 *
 * Returns:
 *   TRUE if the list exists, FALSE otherwise.
 */
BOOL gldIsList46(GLuint listId);

/*
 * Record a command into the currently active display list.
 *
 * This is called by other modules (e.g., immediate_mode_emulator)
 * when a display list is being recorded.  The command function and
 * argument data are serialized into the command buffer.
 *
 * Parameters:
 *   func    — the function to call during playback.
 *   argData — pointer to the argument data to serialize.
 *   argSize — size of the argument data in bytes.
 *
 * Returns:
 *   TRUE if the command was recorded, FALSE if not recording or
 *   the command buffer is full.
 */
BOOL gldDLRecordCommand(GLD_dlCommandFunc func,
                        const void *argData, int argSize);

/*
 * Query whether a display list is currently being recorded.
 *
 * Returns:
 *   TRUE if between gldNewList46 and gldEndList46, FALSE otherwise.
 */
BOOL gldDLIsRecording(void);

/*
 * Query the current recording mode.
 *
 * Returns:
 *   GL_COMPILE or GL_COMPILE_AND_EXECUTE if recording, 0 otherwise.
 */
GLenum gldDLGetRecordingMode(void);

#ifdef  __cplusplus
}
#endif

#endif
