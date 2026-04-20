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
*  ===============================================================================
*  |        Original Author: Keith Harrison <sio2@users.sourceforge.net>         |
*  ===============================================================================
*
* Language:     ANSI C
* Environment:  Windows 9x/2000/XP/XBox (Win32)
*
* Description:  GL extensions
*
*********************************************************************************/

#include <windows.h>
// TODO: Mesa GL headers removed - replaced by GLAD
// #define GL_GLEXT_PROTOTYPES
// #include <GL/gl.h>
// #include <GL/glext.h>
#include <glad/gl.h>

// TODO: Mesa includes removed - replaced by mesa_compat.h shim
// #include "glheader.h"
// #include "context.h"
// #include "colormac.h"
// #include "depth.h"
// #include "extensions.h"
// #include "macros.h"
// #include "matrix.h"
// #include "mtypes.h"
// #include "texformat.h"
// #include "texstore.h"
#include "gld_context.h"
#include "mesa_compat.h"
// #include "extensions.h"

// For some reason this is not defined in an above header...
extern void _mesa_enable_imaging_extensions(GLcontext *ctx);

//---------------------------------------------------------------------------
// Hack for the SGIS_multitexture extension that was removed from Mesa
// NOTE: SGIS_multitexture enums also clash with GL_SGIX_async_pixel

	// NOTE: Quake2 ran *slower* with this enabled, so I've
	// disabled it for now.
	// To enable, uncomment:
	//  _mesa_add_extension(ctx, GL_TRUE, szGL_SGIS_multitexture, 0);

//---------------------------------------------------------------------------

enum {
	/* Quake2 GL_SGIS_multitexture */
	GL_SELECTED_TEXTURE_SGIS			= 0x835B,
	GL_SELECTED_TEXTURE_COORD_SET_SGIS	= 0x835C,
	GL_MAX_TEXTURES_SGIS				= 0x835D,
	GL_TEXTURE0_SGIS					= 0x835E,
	GL_TEXTURE1_SGIS					= 0x835F,
	GL_TEXTURE2_SGIS					= 0x8360,
	GL_TEXTURE3_SGIS					= 0x8361,
	GL_TEXTURE_COORD_SET_SOURCE_SGIS	= 0x8363,
};

//---------------------------------------------------------------------------

void APIENTRY gldSelectTextureSGIS(
	GLenum target)
{
	GLenum ARB_target = GL_TEXTURE0_ARB + (target - GL_TEXTURE0_SGIS);
	glActiveTexture(ARB_target);
}

//---------------------------------------------------------------------------

void APIENTRY gldMTexCoord2fSGIS(
	GLenum target,
	GLfloat s,
	GLfloat t)
{
	GLenum ARB_target = GL_TEXTURE0_ARB + (target - GL_TEXTURE0_SGIS);
	glMultiTexCoord2f(ARB_target, s, t);
}

//---------------------------------------------------------------------------

void APIENTRY gldMTexCoord2fvSGIS(
	GLenum target,
	const GLfloat *v)
{
	GLenum ARB_target = GL_TEXTURE0_ARB + (target - GL_TEXTURE0_SGIS);
	glMultiTexCoord2f(ARB_target, v[0], v[1]);
}

//---------------------------------------------------------------------------
// Extensions
//---------------------------------------------------------------------------

typedef struct {
	PROC proc;
	char *name;
}  GLD_extension;

GLD_extension GLD_extList[] = {
    {	NULL,							"glPolygonOffsetEXT"		},
    {	NULL,							"glBlendEquationEXT"		},
    {	NULL,							"glBlendColorExt"			},
    {	NULL,							"glVertexPointerEXT"		},
    {	NULL,							"glNormalPointerEXT"		},
    {	NULL,							"glColorPointerEXT"			},
    {	NULL,							"glIndexPointerEXT"			},
    {	NULL,							"glTexCoordPointer"			},
    {	NULL,							"glEdgeFlagPointerEXT"		},
    {	NULL,							"glGetPointervEXT"			},
    {	NULL,							"glArrayElementEXT"			},
    {	NULL,							"glDrawArrayEXT"			},
    {	NULL,							"glAreTexturesResidentEXT"	},
    {	NULL,							"glBindTextureEXT"			},
    {	NULL,							"glDeleteTexturesEXT"		},
    {	NULL,							"glGenTexturesEXT"			},
    {	NULL,							"glIsTextureEXT"			},
    {	NULL,							"glPrioritizeTexturesEXT"	},
    {	NULL,							"glCopyTexSubImage3DEXT"	},
    {	NULL,							"glTexImage3DEXT"			},
    {	NULL,							"glTexSubImage3DEXT"		},
    {	NULL,							"glPointParameterfEXT"		},
    {	NULL,							"glPointParameterfvEXT"		},

    {	NULL,							"glLockArraysEXT"			},
    {	NULL,							"glUnlockArraysEXT"			},
	{	NULL,							"\0"						}
};

GLD_extension GLD_multitexList[] = {
    {	NULL,							"glActiveTextureARB"		},
    {	NULL,							"glClientActiveTextureARB"	},
    {	NULL,							"glMultiTexCoord1dARB"		},
    {	NULL,							"glMultiTexCoord1dvARB"		},
    {	NULL,							"glMultiTexCoord1fARB"		},
    {	NULL,							"glMultiTexCoord1fvARB"		},
    {	NULL,							"glMultiTexCoord1iARB"		},
    {	NULL,							"glMultiTexCoord1ivARB"		},
    {	NULL,							"glMultiTexCoord1sARB"		},
    {	NULL,							"glMultiTexCoord1svARB"		},
    {	NULL,							"glMultiTexCoord2dARB"		},
    {	NULL,							"glMultiTexCoord2dvARB"		},
    {	NULL,							"glMultiTexCoord2fARB"		},
    {	NULL,							"glMultiTexCoord2fvARB"		},
    {	NULL,							"glMultiTexCoord2iARB"		},
    {	NULL,							"glMultiTexCoord2ivARB"		},
    {	NULL,							"glMultiTexCoord2sARB"		},
    {	NULL,							"glMultiTexCoord2svARB"		},
    {	NULL,							"glMultiTexCoord3dARB"		},
    {	NULL,							"glMultiTexCoord3dvARB"		},
    {	NULL,							"glMultiTexCoord3fARB"		},
    {	NULL,							"glMultiTexCoord3fvARB"		},
    {	NULL,							"glMultiTexCoord3iARB"		},
    {	NULL,							"glMultiTexCoord3ivARB"		},
    {	NULL,							"glMultiTexCoord3sARB"		},
    {	NULL,							"glMultiTexCoord3svARB"		},
    {	NULL,							"glMultiTexCoord4dARB"		},
    {	NULL,							"glMultiTexCoord4dvARB"		},
    {	NULL,							"glMultiTexCoord4fARB"		},
    {	NULL,							"glMultiTexCoord4fvARB"		},
    {	NULL,							"glMultiTexCoord4iARB"		},
    {	NULL,							"glMultiTexCoord4ivARB"		},
    {	NULL,							"glMultiTexCoord4sARB"		},
    {	NULL,							"glMultiTexCoord4svARB"		},

	// Descent3 doesn't use correct string, hence this hack
    {	NULL,							"glMultiTexCoord4f"			},

	// Quake2 SGIS multitexture
    {	(PROC)gldSelectTextureSGIS,		"glSelectTextureSGIS"		},
    {	(PROC)gldMTexCoord2fSGIS,		"glMTexCoord2fSGIS"			},
    {	(PROC)gldMTexCoord2fvSGIS,		"glMTexCoord2fvSGIS"		},

	{	NULL,							"\0"						}
};

//---------------------------------------------------------------------------

PROC gldGetProcAddress_DX(
	LPCSTR a)
{
	int		i;
	PROC	proc = NULL;

	for (i=0; GLD_extList[i].proc; i++) {
		if (!strcmp(a, GLD_extList[i].name)) {
			proc = GLD_extList[i].proc;
			break;
		}
	}

	if (glb.bMultitexture) {
		for (i=0; GLD_multitexList[i].proc; i++) {
			if (!strcmp(a, GLD_multitexList[i].name)) {
				proc = GLD_multitexList[i].proc;
				break;
			}
		}
	}

	gldLogPrintf(GLDLOG_INFO, "GetProcAddress: %s (%s)", a, proc ? "OK" : "Failed");

	return proc;
}

//---------------------------------------------------------------------------

void gldEnableExtensions_DX9(
	GLcontext *ctx)
{
	GLuint i;

	// Mesa enables some extensions by default.
	// This table decides which ones we want to switch off again.

	// NOTE: GL_EXT_compiled_vertex_array appears broken.

	const char *gld_disable_extensions[] = {
//		"GL_ARB_transpose_matrix",
//		"GL_EXT_compiled_vertex_array",
//		"GL_EXT_polygon_offset",
//		"GL_EXT_rescale_normal",
		"GL_EXT_texture3D",
//		"GL_NV_texgen_reflection",
		NULL
	};

	const char *gld_multitex_extensions[] = {
		"GL_ARB_multitexture",		// Quake 3
		NULL
	};

	// Quake 2 engines
	const char *szGL_SGIS_multitexture = "GL_SGIS_multitexture";

	const char *gld_enable_extensions[] = {
		"GL_EXT_texture_env_add",	// Quake 3
		"GL_ARB_texture_env_add",	// Quake 3
		NULL
	};
	
	for (i=0; gld_disable_extensions[i]; i++) {
		_mesa_disable_extension(ctx, gld_disable_extensions[i]);
	}
	
	for (i=0; gld_enable_extensions[i]; i++) {
		_mesa_enable_extension(ctx, gld_enable_extensions[i]);
	}

	if (glb.bMultitexture) {	
		for (i=0; gld_multitex_extensions[i]; i++) {
			_mesa_enable_extension(ctx, gld_multitex_extensions[i]);
		}

		// GL_SGIS_multitexture
		// NOTE: Quake2 ran *slower* with this enabled, so I've
		// disabled it for now.
		// Fair bit slower on GeForce256,
		// Much slower on 3dfx Voodoo5 5500.
//		_mesa_add_extension(ctx, GL_TRUE, szGL_SGIS_multitexture, 0);

	}

	//Needed for Bugdom 2 and Otto Matic
    if (glb.bGL13Needed)
    	_mesa_enable_1_3_extensions(ctx);

// Not required for OpenGL 1.1 support.
#if 0
    _mesa_enable_imaging_extensions(ctx);
	_mesa_enable_1_4_extensions(ctx);
	_mesa_enable_1_5_extensions(ctx);
#endif

}

//---------------------------------------------------------------------------
