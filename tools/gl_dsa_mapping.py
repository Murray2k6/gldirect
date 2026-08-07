"""Direct State Access name -> (bind kind, forwarded call).

Generator input for tools/gen_gl_stubs.py.  The DSA family needs a field the
other families in gl_stub_classification.py do not: the D3D9-facing object the
generated stub has to bind before it can reuse the existing non-DSA _gls* entry
point, so it lives in its own table.

Shape:  "glName": (kind, call)

kind selects the save/bind/restore wrapper the generator emits around `call`:

  "tex"       bind the named texture to its own recorded target (GLS_Texture
              .target, defaulting to GL_TEXTURE_2D); _genTgt names that target
              inside `call`.  Restores the previous binding for that target on
              the active texture unit.
  "buf"       bind the named buffer to GL_ARRAY_BUFFER; restores
              GLS_State.boundArrayBuffer.
  "fbo"       bind the named framebuffer to GL_DRAW_FRAMEBUFFER; restores
              GLS_State.boundDrawFBO.
  "fbo-read"  same for GL_READ_FRAMEBUFFER / GLS_State.boundReadFBO.
  "rbo"       bind the named renderbuffer to GL_RENDERBUFFER; restores
              GLS_State.boundRBO.
  "vao"       bind the named vertex array; restores GLS_State.boundVAO.
  "none"      no bind at all - object-creation commands and the handful of DSA
              names whose non-DSA sibling takes the object name directly.

call is one of:

  None      no reachable equivalent: correctly-typed warn-once stub.
  ""        doing nothing is spec-legal (invalidation hints): silent stub.
  "expr"    a C expression evaluated between bind and restore.  For a
            non-void command its value is what the stub returns.
  "{ ... }" a raw C block, used where the forward needs a temporary (an
            int-to-float or int-to-int64 output conversion).

Every parameter name below comes from glmap.json's params[] for that command, so
`call` is checked against the real signature by the compiler, not by eye.

Coverage note: this is the one Tier-1 family confirmed observed live rather than
included for spec completeness, so wrong pairings here matter more than
elsewhere.  100 entries, matching every glmap.json missing_core name classified
"dsa" in gl_stub_classification.py.
"""

DSA_MAPPING = {
    # ----- texture object DSA -----
    "glBindTextureUnit":              ("none", "_genBindTextureUnit((GLuint)unit, (GLuint)texture)"),
    "glCompressedTextureSubImage1D":  ("tex",  "_glsCompressedTexSubImage1D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)width, (unsigned int)format, (int)imageSize, data)"),
    "glCompressedTextureSubImage2D":  ("tex",  "_glsCompressedTexSubImage2D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)width, (int)height, (unsigned int)format, (int)imageSize, data)"),
    "glCompressedTextureSubImage3D":  ("tex",  "_glsCompressedTexSubImage3D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)zoffset, (int)width, (int)height, (int)depth, (unsigned int)format, (int)imageSize, data)"),
    # GL has no 1D texture storage here: a 1D texture is a 2D texture one row high.
    "glCopyTextureSubImage1D":        ("tex",  "_glsCopyTexSubImage2D((unsigned int)_genTgt, (int)level, (int)xoffset, 0, (int)x, (int)y, (int)width, 1)"),
    "glCopyTextureSubImage2D":        ("tex",  "_glsCopyTexSubImage2D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)x, (int)y, (int)width, (int)height)"),
    "glCopyTextureSubImage3D":        ("tex",  "_glsCopyTexSubImage3D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)zoffset, (int)x, (int)y, (int)width, (int)height)"),
    "glGenerateTextureMipmap":        ("tex",  "_glsGenerateMipmap((unsigned int)_genTgt)"),
    "glGetCompressedTextureImage":    ("tex",  "_glsGetCompressedTexImage((unsigned int)_genTgt, (int)level, pixels)"),
    "glGetCompressedTextureSubImage": ("none", "gldAdvGetCompressedTextureSubImage((GLuint)texture, (GLint)level, (GLint)xoffset, (GLint)yoffset, (GLint)zoffset, (GLsizei)width, (GLsizei)height, (GLsizei)depth, (GLsizei)bufSize, pixels)"),
    "glGetTextureImage":              ("tex",  "_glsGetTexImage((unsigned int)_genTgt, (int)level, (unsigned int)format, (unsigned int)type, pixels)"),
    "glGetTextureLevelParameterfv":   ("tex",  "{ GLint _genV[16]; memset(_genV, 0, sizeof(_genV)); _glsGetTexLevelParameteriv((unsigned int)_genTgt, (int)level, (unsigned int)pname, _genV); if (params) params[0] = (GLfloat)_genV[0]; }"),
    "glGetTextureLevelParameteriv":   ("tex",  "_glsGetTexLevelParameteriv((unsigned int)_genTgt, (int)level, (unsigned int)pname, (int *)params)"),
    "glGetTextureParameterIiv":       ("tex",  "_glsGetTexParameteriv((unsigned int)_genTgt, (unsigned int)pname, (int *)params)"),
    "glGetTextureParameterIuiv":      ("tex",  "{ GLint _genV[4]; memset(_genV, 0, sizeof(_genV)); _glsGetTexParameteriv((unsigned int)_genTgt, (unsigned int)pname, _genV); if (params) params[0] = (GLuint)_genV[0]; }"),
    "glGetTextureParameterfv":        ("tex",  "{ GLint _genV[4]; memset(_genV, 0, sizeof(_genV)); _glsGetTexParameteriv((unsigned int)_genTgt, (unsigned int)pname, _genV); if (params) params[0] = (GLfloat)_genV[0]; }"),
    "glGetTextureParameteriv":        ("tex",  "_glsGetTexParameteriv((unsigned int)_genTgt, (unsigned int)pname, (int *)params)"),
    "glGetTextureSubImage":           ("none", "gldAdvGetTextureSubImage((GLuint)texture, (GLint)level, (GLint)xoffset, (GLint)yoffset, (GLint)zoffset, (GLsizei)width, (GLsizei)height, (GLsizei)depth, (GLenum)format, (GLenum)type, (GLsizei)bufSize, pixels)"),
    "glTextureBuffer":                ("tex",  "_glsTexBuffer((unsigned int)_genTgt, (unsigned int)internalformat, (unsigned int)buffer)"),
    # D3D9 buffer textures have no sub-range concept: offset/size are ignored.
    "glTextureBufferRange":           ("tex",  "_glsTexBuffer((unsigned int)_genTgt, (unsigned int)internalformat, (unsigned int)buffer)"),
    "glTextureParameterIiv":          ("tex",  "_glsTexParameteri((unsigned int)_genTgt, (unsigned int)pname, params ? (int)params[0] : 0)"),
    "glTextureParameterIuiv":         ("tex",  "_glsTexParameteri((unsigned int)_genTgt, (unsigned int)pname, params ? (int)params[0] : 0)"),
    "glTextureParameterf":            ("tex",  "_glsTexParameterf((unsigned int)_genTgt, (unsigned int)pname, (float)param)"),
    "glTextureParameterfv":           ("tex",  "_glsTexParameterf((unsigned int)_genTgt, (unsigned int)pname, param ? (float)param[0] : 0.0f)"),
    "glTextureParameteri":            ("tex",  "_glsTexParameteri((unsigned int)_genTgt, (unsigned int)pname, (int)param)"),
    "glTextureParameteriv":           ("tex",  "_glsTexParameteri((unsigned int)_genTgt, (unsigned int)pname, param ? (int)param[0] : 0)"),
    "glTextureStorage1D":             ("tex",  "_glsTexStorage2D((unsigned int)_genTgt, (int)levels, (unsigned int)internalformat, (int)width, 1)"),
    "glTextureStorage2D":             ("tex",  "_glsTexStorage2D((unsigned int)_genTgt, (int)levels, (unsigned int)internalformat, (int)width, (int)height)"),
    # SM3 has no multisampled textures; allocate the single-sample equivalent.
    "glTextureStorage2DMultisample":  ("tex",  "_glsTexStorage2D((unsigned int)_genTgt, 1, (unsigned int)internalformat, (int)width, (int)height)"),
    "glTextureStorage3D":             ("tex",  "_glsTexStorage3D((unsigned int)_genTgt, (int)levels, (unsigned int)internalformat, (int)width, (int)height, (int)depth)"),
    "glTextureStorage3DMultisample":  ("tex",  "_glsTexStorage3D((unsigned int)_genTgt, 1, (unsigned int)internalformat, (int)width, (int)height, (int)depth)"),
    "glTextureSubImage1D":            ("tex",  "_glsTexSubImage1D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)width, (unsigned int)format, (unsigned int)type, pixels)"),
    "glTextureSubImage2D":            ("tex",  "_glsTexSubImage2D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)width, (int)height, (unsigned int)format, (unsigned int)type, pixels)"),
    "glTextureSubImage3D":            ("tex",  "_glsTexSubImage3D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)zoffset, (int)width, (int)height, (int)depth, (unsigned int)format, (unsigned int)type, pixels)"),
    # A texture view aliases another texture's storage; D3D9 cannot alias.
    "glTextureView":                  ("none", "gldAdvTextureView((GLuint)texture, (GLenum)target, (GLuint)origtexture, (GLenum)internalformat, (GLuint)minlevel, (GLuint)numlevels, (GLuint)minlayer, (GLuint)numlayers)"),

    # ----- buffer object DSA -----
    "glClearNamedBufferData":         ("buf",  "gldAdvClearBufferData((GLenum)GL_ARRAY_BUFFER, (GLenum)internalformat, (GLenum)format, (GLenum)type, data)"),
    "glClearNamedBufferSubData":      ("buf",  "gldAdvClearBufferSubData((GLenum)GL_ARRAY_BUFFER, (GLenum)internalformat, (GLintptr)offset, (GLsizeiptr)size, (GLenum)format, (GLenum)type, data)"),
    "glCopyNamedBufferSubData":       ("none", "_genCopyNamedBufferSubData((GLuint)readBuffer, (GLuint)writeBuffer, (ptrdiff_t)readOffset, (ptrdiff_t)writeOffset, (ptrdiff_t)size)"),
    "glFlushMappedNamedBufferRange":  ("buf",  "_glsFlushMappedBufferRange((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)offset, (ptrdiff_t)length)"),
    "glGetNamedBufferParameteri64v":  ("buf",  "{ GLint _genV[4]; memset(_genV, 0, sizeof(_genV)); _glsGetBufferParameteriv((unsigned int)GL_ARRAY_BUFFER, (unsigned int)pname, _genV); if (params) params[0] = (GLint64)_genV[0]; }"),
    "glGetNamedBufferParameteriv":    ("buf",  "_glsGetBufferParameteriv((unsigned int)GL_ARRAY_BUFFER, (unsigned int)pname, (int *)params)"),
    "glGetNamedBufferPointerv":       ("buf",  "_glsGetBufferPointerv((unsigned int)GL_ARRAY_BUFFER, (unsigned int)pname, (void **)params)"),
    "glGetNamedBufferSubData":        ("buf",  "_glsGetBufferSubData((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)offset, (ptrdiff_t)size, data)"),
    "glMapNamedBuffer":               ("buf",  "_glsMapBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)access)"),
    "glMapNamedBufferRange":          ("buf",  "_glsMapBufferRange((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)offset, (ptrdiff_t)length, (unsigned int)access)"),
    "glNamedBufferData":              ("buf",  "_glsBufferData((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)size, data, (unsigned int)usage)"),
    # Immutable storage has no D3D9 analogue; the allocation itself is what matters.
    "glNamedBufferStorage":           ("buf",  "_glsBufferData((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)size, data, (unsigned int)GL_STATIC_DRAW)"),
    "glNamedBufferSubData":           ("buf",  "_glsBufferSubData((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)offset, (ptrdiff_t)size, data)"),
    "glUnmapNamedBuffer":             ("buf",  "_glsUnmapBuffer((unsigned int)GL_ARRAY_BUFFER)"),

    # ----- framebuffer / renderbuffer DSA -----
    "glBlitNamedFramebuffer":         ("none", "_genBlitNamedFramebuffer((GLuint)readFramebuffer, (GLuint)drawFramebuffer, (int)srcX0, (int)srcY0, (int)srcX1, (int)srcY1, (int)dstX0, (int)dstY0, (int)dstX1, (int)dstY1, (unsigned int)mask, (unsigned int)filter)"),
    "glCheckNamedFramebufferStatus":  ("fbo",  "_glsCheckFramebufferStatus((unsigned int)target)"),
    "glClearNamedFramebufferfi":      ("fbo",  "_glsClearBufferfi((unsigned int)buffer, (int)drawbuffer, (float)depth, (int)stencil)"),
    "glClearNamedFramebufferfv":      ("fbo",  "_glsClearBufferfv((unsigned int)buffer, (int)drawbuffer, (const float *)value)"),
    "glClearNamedFramebufferiv":      ("fbo",  "_glsClearBufferiv((unsigned int)buffer, (int)drawbuffer, (const int *)value)"),
    "glClearNamedFramebufferuiv":     ("fbo",  "_glsClearBufferuiv((unsigned int)buffer, (int)drawbuffer, (const unsigned int *)value)"),
    "glGetNamedFramebufferAttachmentParameteriv": ("fbo", "_stub_glGetFramebufferAttachmentParameteriv((GLenum)GL_DRAW_FRAMEBUFFER, (GLenum)attachment, (GLenum)pname, (GLint *)params)"),
    "glGetNamedFramebufferParameteriv": ("none", "gldAdvGetNamedFramebufferParameteriv((GLuint)framebuffer, (GLenum)pname, (GLint *)param)"),
    "glGetNamedRenderbufferParameteriv": ("rbo", "_stub_glGetRenderbufferParameteriv((GLenum)GL_RENDERBUFFER, (GLenum)pname, (GLint *)params)"),
    # Invalidation is a hint; discarding nothing is a legal implementation.
    "glInvalidateNamedFramebufferData":    ("none", ""),
    "glInvalidateNamedFramebufferSubData": ("none", ""),
    "glNamedFramebufferDrawBuffer":   ("fbo",  "_glsDrawBuffer((unsigned int)buf)"),
    "glNamedFramebufferDrawBuffers":  ("fbo",  "_glsDrawBuffers((int)n, (const unsigned int *)bufs)"),
    "glNamedFramebufferParameteri":   ("fbo",  "gldAdvFramebufferParameteri((GLenum)GL_DRAW_FRAMEBUFFER, (GLenum)pname, (GLint)param)"),
    "glNamedFramebufferReadBuffer":   ("fbo-read", "_glsReadBuffer((unsigned int)src)"),
    "glNamedFramebufferRenderbuffer": ("fbo",  "_glsFramebufferRenderbuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)attachment, (unsigned int)renderbuffertarget, (unsigned int)renderbuffer)"),
    "glNamedFramebufferTexture":      ("fbo",  "_glsFramebufferTexture((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)attachment, (unsigned int)texture, (int)level)"),
    # D3D9 attaches a whole surface; the layer selects nothing this wrapper models.
    "glNamedFramebufferTextureLayer": ("fbo",  "_glsFramebufferTexture((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)attachment, (unsigned int)texture, (int)level)"),
    "glNamedRenderbufferStorage":     ("rbo",  "_glsRenderbufferStorage((unsigned int)GL_RENDERBUFFER, (unsigned int)internalformat, (int)width, (int)height)"),
    "glNamedRenderbufferStorageMultisample": ("rbo", "_glsRenderbufferStorageMultisample((unsigned int)GL_RENDERBUFFER, (int)samples, (unsigned int)internalformat, (int)width, (int)height)"),

    # ----- vertex array object DSA -----
    "glDisableVertexArrayAttrib":     ("vao",  "_glsDisableVertexAttribArray((unsigned int)index)"),
    "glEnableVertexArrayAttrib":      ("vao",  "_glsEnableVertexAttribArray((unsigned int)index)"),
    "glGetVertexArrayIndexed64iv":    ("vao",  "{ GLS_VAO *_genVao = glsFindVAO((GLuint_t)vaobj); if (param) param[0] = (GLint64)((_genVao && index < GLS_MAX_VERTEX_ATTRIBS) ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0); }"),
    "glGetVertexArrayIndexediv":      ("vao",  "{ GLS_VAO *_genVao = glsFindVAO((GLuint_t)vaobj); if (param) param[0] = (GLint)((_genVao && index < GLS_MAX_VERTEX_ATTRIBS) ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0); }"),
    "glGetVertexArrayiv":             ("none", "{ GLS_VAO *_genVao = glsFindVAO((GLuint_t)vaobj); if (param) param[0] = (GLint)((_genVao && pname == GL_ELEMENT_ARRAY_BUFFER_BINDING) ? _genVao->elementBuffer : 0); }"),
    "glVertexArrayElementBuffer":     ("vao",  "_glsBindBuffer((unsigned int)GL_ELEMENT_ARRAY_BUFFER, (unsigned int)buffer)"),
    "glVertexArrayBindingDivisor":    ("vao",  "_glsVertexAttribDivisor((unsigned int)bindingindex, (unsigned int)divisor)"),
    # The separate attribute-format / vertex-buffer-binding split (GL 4.3) sets
    # exactly the GLS_VertexAttrib fields the vertex path is being rewritten
    # around; it is implemented with that rewrite, not stubbed for real here.
    "glVertexArrayAttribBinding":     ("vao",  "gldAdvVertexAttribBinding((GLuint)attribindex, (GLuint)bindingindex)"),
    "glVertexArrayAttribFormat":      ("vao",  "gldAdvVertexAttribFormat((GLuint)attribindex, (GLint)size, (GLenum)type, (GLboolean)normalized, (GLuint)relativeoffset)"),
    "glVertexArrayAttribIFormat":     ("vao",  "gldAdvVertexAttribIFormat((GLuint)attribindex, (GLint)size, (GLenum)type, (GLuint)relativeoffset)"),
    "glVertexArrayAttribLFormat":     ("vao",  "gldAdvVertexAttribLFormat((GLuint)attribindex, (GLint)size, (GLenum)type, (GLuint)relativeoffset)"),
    "glVertexArrayVertexBuffer":      ("vao",  "gldAdvBindVertexBuffer((GLuint)bindingindex, (GLuint)buffer, (GLintptr)offset, (GLsizei)stride)"),
    "glVertexArrayVertexBuffers":     ("vao",  "gldAdvBindVertexBuffers((GLuint)first, (GLsizei)count, buffers, offsets, strides)"),

    # ----- object creation (DSA "create" == gen + immediate initialisation) -----
    "glCreateBuffers":                ("none", "_glsGenBuffers((int)n, (unsigned int *)buffers)"),
    "glCreateFramebuffers":           ("none", "_glsGenFramebuffers((int)n, (unsigned int *)framebuffers)"),
    "glCreateQueries":                ("none", "_glsGenQueries((int)n, (unsigned int *)ids)"),
    "glCreateRenderbuffers":          ("none", "_glsGenRenderbuffers((int)n, (unsigned int *)renderbuffers)"),
    "glCreateSamplers":               ("none", "_glsGenSamplers((int)n, (unsigned int *)samplers)"),
    "glCreateTextures":               ("none", "_genCreateTextures((GLenum)target, (int)n, (unsigned int *)textures)"),
    "glCreateVertexArrays":           ("none", "_glsGenVertexArrays((int)n, (unsigned int *)arrays)"),
    "glCreateProgramPipelines":       ("none", "gldAdvGenProgramPipelines((GLsizei)n, pipelines)"),
    "glCreateTransformFeedbacks":     ("none", "gldAdvGenTransformFeedbacks((GLsizei)n, ids)"),

    # ----- query / transform-feedback DSA with no D3D9 analogue -----
    "glGetQueryBufferObjecti64v":     ("none", "gldAdvGetQueryBufferObjecti64v((GLuint)id, (GLuint)buffer, (GLenum)pname, (GLintptr)offset)"),
    "glGetQueryBufferObjectiv":       ("none", "gldAdvGetQueryBufferObjectiv((GLuint)id, (GLuint)buffer, (GLenum)pname, (GLintptr)offset)"),
    "glGetQueryBufferObjectui64v":    ("none", "gldAdvGetQueryBufferObjectui64v((GLuint)id, (GLuint)buffer, (GLenum)pname, (GLintptr)offset)"),
    "glGetQueryBufferObjectuiv":      ("none", "gldAdvGetQueryBufferObjectuiv((GLuint)id, (GLuint)buffer, (GLenum)pname, (GLintptr)offset)"),
    "glGetTransformFeedbackVarying":  ("none", "gldAdvGetTransformFeedbackVarying((GLuint)program, (GLuint)index, (GLsizei)bufSize, length, size, type, name)"),
    "glGetTransformFeedbacki64_v":    ("none", "gldAdvGetTransformFeedbacki64_v((GLuint)xfb, (GLenum)pname, (GLuint)index, param)"),
    "glGetTransformFeedbacki_v":      ("none", "gldAdvGetTransformFeedbacki_v((GLuint)xfb, (GLenum)pname, (GLuint)index, param)"),
    "glGetTransformFeedbackiv":       ("none", "gldAdvGetTransformFeedbackiv((GLuint)xfb, (GLenum)pname, param)"),
    "glTransformFeedbackBufferBase":  ("none", "gldAdvTransformFeedbackBufferBase((GLuint)xfb, (GLuint)index, (GLuint)buffer)"),
    "glTransformFeedbackBufferRange": ("none", "gldAdvTransformFeedbackBufferRange((GLuint)xfb, (GLuint)index, (GLuint)buffer, (GLintptr)offset, (GLsizeiptr)size)"),
}
