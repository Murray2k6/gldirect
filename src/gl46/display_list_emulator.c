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
*********************************************************************************/

#include "display_list_emulator.h"
#include "error_handler.h"
#include "gld_log.h"
#include <stdlib.h>
#include <string.h>

/*---------------------- Static module state ----------------------*/

/*
 * Linear array hash map of display lists.
 * Indexed by (listId % GLD_DL_MAX_LISTS) with linear probing.
 */
static GLD_displayList s_lists[GLD_DL_MAX_LISTS];

/* Recording state */
static BOOL     s_recording = FALSE;
static GLenum   s_recordingMode = 0;
static GLuint   s_recordingListId = 0;
static int      s_recordingSlot = -1;

/* Nesting depth tracker for gldCallList46 */
static int      s_nestingDepth = 0;

/* Next available list name for gldGenLists46 */
static GLuint   s_nextListName = 1;

/*---------------------- Internal helper functions ----------------------*/

/*
 * Find the slot index for a given list ID.
 * Returns -1 if not found.
 */
static int sFindListSlot(GLuint listId)
{
    int start = (int)(listId % GLD_DL_MAX_LISTS);
    int i;

    for (i = 0; i < GLD_DL_MAX_LISTS; i++) {
        int idx = (start + i) % GLD_DL_MAX_LISTS;
        if (s_lists[idx].inUse && s_lists[idx].listId == listId)
            return idx;
        if (!s_lists[idx].inUse && s_lists[idx].listId == 0)
            return -1;  /* Empty slot — list doesn't exist */
    }
    return -1;
}

/*
 * Find or allocate a slot for a given list ID.
 * Returns the slot index, or -1 if the table is full.
 */
static int sAllocListSlot(GLuint listId)
{
    int start = (int)(listId % GLD_DL_MAX_LISTS);
    int i;
    int firstFree = -1;

    for (i = 0; i < GLD_DL_MAX_LISTS; i++) {
        int idx = (start + i) % GLD_DL_MAX_LISTS;
        if (s_lists[idx].inUse && s_lists[idx].listId == listId)
            return idx;  /* Existing list — will be replaced */
        if (!s_lists[idx].inUse && firstFree < 0)
            firstFree = idx;
    }
    return firstFree;
}

/*
 * Free the command buffer for a display list slot.
 */
static void sFreeListSlot(int slot)
{
    if (slot < 0 || slot >= GLD_DL_MAX_LISTS)
        return;

    if (s_lists[slot].commands) {
        free(s_lists[slot].commands);
        s_lists[slot].commands = NULL;
    }
    s_lists[slot].cmdCount = 0;
    s_lists[slot].cmdCapacity = 0;
    s_lists[slot].inUse = FALSE;
    s_lists[slot].listId = 0;
}

/*
 * Ensure the command buffer has room for one more command.
 * Grows the buffer by doubling capacity.
 */
static BOOL sEnsureCommandCapacity(int slot)
{
    GLD_displayList *dl = &s_lists[slot];
    int newCapacity;
    GLD_dlCommand *newBuf;

    if (dl->cmdCount < dl->cmdCapacity)
        return TRUE;

    newCapacity = dl->cmdCapacity == 0 ? 64 : dl->cmdCapacity * 2;
    if (newCapacity > GLD_DL_MAX_COMMANDS)
        newCapacity = GLD_DL_MAX_COMMANDS;

    if (dl->cmdCount >= GLD_DL_MAX_COMMANDS) {
        gldLogPrintf(GLDLOG_ERROR,
            "Display list %u: command buffer full (%d commands)",
            dl->listId, GLD_DL_MAX_COMMANDS);
        return FALSE;
    }

    newBuf = (GLD_dlCommand *)realloc(dl->commands,
                                       (size_t)newCapacity * sizeof(GLD_dlCommand));
    if (!newBuf) {
        gldLogPrintf(GLDLOG_ERROR,
            "Display list %u: failed to allocate command buffer", dl->listId);
        return FALSE;
    }

    dl->commands = newBuf;
    dl->cmdCapacity = newCapacity;
    return TRUE;
}

/* ===================================================================
 * Public API
 * =================================================================== */

void gldNewList46(GLuint listId, GLenum mode)
{
    int slot;

    if (listId == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldNewList46: list ID 0 is reserved");
        return;
    }

    if (s_recording) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldNewList46: already recording list %u", s_recordingListId);
        return;
    }

    if (mode != GL_COMPILE && mode != GL_COMPILE_AND_EXECUTE) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldNewList46: invalid mode 0x%04X", (unsigned)mode);
        return;
    }

    /* Find or allocate a slot */
    slot = sAllocListSlot(listId);
    if (slot < 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldNewList46: display list table full (%d lists)",
            GLD_DL_MAX_LISTS);
        return;
    }

    /* If replacing an existing list, free old commands */
    if (s_lists[slot].inUse && s_lists[slot].listId == listId) {
        if (s_lists[slot].commands) {
            free(s_lists[slot].commands);
            s_lists[slot].commands = NULL;
        }
        s_lists[slot].cmdCount = 0;
        s_lists[slot].cmdCapacity = 0;
    }

    /* Initialize the slot */
    s_lists[slot].listId = listId;
    s_lists[slot].inUse = TRUE;
    s_lists[slot].cmdCount = 0;

    s_recording = TRUE;
    s_recordingMode = mode;
    s_recordingListId = listId;
    s_recordingSlot = slot;

    gldLogPrintf(GLDLOG_DEBUG,
        "gldNewList46: recording list %u (mode=%s)",
        listId,
        mode == GL_COMPILE ? "GL_COMPILE" : "GL_COMPILE_AND_EXECUTE");
}

// ***********************************************************************

void gldEndList46(void)
{
    if (!s_recording) {
        gldLogPrintf(GLDLOG_WARN,
            "gldEndList46: not currently recording");
        return;
    }

    gldLogPrintf(GLDLOG_DEBUG,
        "gldEndList46: finalized list %u (%d commands)",
        s_recordingListId, s_lists[s_recordingSlot].cmdCount);

    s_recording = FALSE;
    s_recordingMode = 0;
    s_recordingListId = 0;
    s_recordingSlot = -1;
}

// ***********************************************************************

void gldCallList46(GLuint listId)
{
    int slot;
    GLD_displayList *dl;
    int i;

    if (listId == 0)
        return;

    /* Check nesting depth */
    if (s_nestingDepth >= GLD_DL_MAX_NESTING) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCallList46: nesting depth exceeded (%d)",
            GLD_DL_MAX_NESTING);
        return;
    }

    slot = sFindListSlot(listId);
    if (slot < 0) {
        gldLogPrintf(GLDLOG_WARN,
            "gldCallList46: list %u not found", listId);
        return;
    }

    dl = &s_lists[slot];
    if (!dl->commands || dl->cmdCount == 0)
        return;

    s_nestingDepth++;

    /* Replay all recorded commands */
    for (i = 0; i < dl->cmdCount; i++) {
        GLD_dlCommand *cmd = &dl->commands[i];
        if (cmd->func) {
            cmd->func(cmd->argData);
        }
    }

    s_nestingDepth--;
}

// ***********************************************************************

void gldDeleteLists46(GLuint base, GLsizei range)
{
    GLsizei i;

    if (range < 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldDeleteLists46: negative range %d", range);
        return;
    }

    for (i = 0; i < range; i++) {
        GLuint id = base + (GLuint)i;
        int slot = sFindListSlot(id);
        if (slot >= 0) {
            sFreeListSlot(slot);
            gldLogPrintf(GLDLOG_DEBUG,
                "gldDeleteLists46: deleted list %u", id);
        }
    }
}

// ***********************************************************************

GLuint gldGenLists46(GLsizei range)
{
    GLuint base;
    GLsizei i;
    BOOL found;

    if (range <= 0)
        return 0;

    /*
     * Simple linear search for a contiguous range of unused list names.
     * Start from s_nextListName and scan forward.
     */
    base = s_nextListName;

    while (base < 0xFFFFFFFF - (GLuint)range) {
        found = TRUE;
        for (i = 0; i < range; i++) {
            if (sFindListSlot(base + (GLuint)i) >= 0) {
                found = FALSE;
                base = base + (GLuint)i + 1;
                break;
            }
        }
        if (found) {
            s_nextListName = base + (GLuint)range;
            return base;
        }
    }

    gldLogPrintf(GLDLOG_ERROR,
        "gldGenLists46: unable to allocate %d contiguous list names", range);
    return 0;
}

// ***********************************************************************

BOOL gldIsList46(GLuint listId)
{
    return (sFindListSlot(listId) >= 0) ? TRUE : FALSE;
}

// ***********************************************************************

BOOL gldDLRecordCommand(GLD_dlCommandFunc func,
                        const void *argData, int argSize)
{
    GLD_dlCommand *cmd;

    if (!s_recording)
        return FALSE;

    if (!func)
        return FALSE;

    if (argSize < 0 || argSize > GLD_DL_MAX_ARG_SIZE) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldDLRecordCommand: argSize %d exceeds max %d",
            argSize, GLD_DL_MAX_ARG_SIZE);
        return FALSE;
    }

    if (!sEnsureCommandCapacity(s_recordingSlot))
        return FALSE;

    cmd = &s_lists[s_recordingSlot].commands[s_lists[s_recordingSlot].cmdCount];
    cmd->func = func;
    cmd->argSize = argSize;

    if (argData && argSize > 0)
        memcpy(cmd->argData, argData, (size_t)argSize);
    else
        cmd->argSize = 0;

    s_lists[s_recordingSlot].cmdCount++;
    return TRUE;
}

// ***********************************************************************

BOOL gldDLIsRecording(void)
{
    return s_recording;
}

// ***********************************************************************

GLenum gldDLGetRecordingMode(void)
{
    return s_recording ? s_recordingMode : 0;
}

// ***********************************************************************
