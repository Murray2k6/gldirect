/*
 * Mesa 3-D graphics library
 * Version:  5.1
 *
 * Copyright (C) 1999-2003  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * NOTE: This module previously depended on Mesa GLcontext internals.
 * With the Mesa removal (task 3.1), these functions are stubbed out.
 * They will be fully replaced by the GL46 Buffer_Manager module (task 9).
 */

#include "gld_arrayelt.h"

GLboolean _ae_create_context( void *ctx )
{
   (void)ctx;
   return GL_TRUE;
}

void _ae_destroy_context( void *ctx )
{
   (void)ctx;
}

void _ae_invalidate_state( void *ctx, GLuint new_state )
{
   (void)ctx;
   (void)new_state;
}

void GLAPIENTRY _ae_loopback_array_elt( GLint elt )
{
   (void)elt;
}
