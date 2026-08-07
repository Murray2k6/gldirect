#!/usr/bin/env python
"""Generate src/gl46/gl_generated_stubs.h from glmap.json + the classification tables.

    python tools/gen_gl_stubs.py

Inputs (all committed, all in this directory):
    glmap.json                  Khronos registry snapshot - see glmap.README
    gl_stub_classification.py   family + forward for each of the 487 core names
    gl_dsa_mapping.py           bind target + forward call for the 101 DSA names

Output:
    src/gl46/gl_generated_stubs.h   822 exactly-typed stubs plus g_generatedGL[]

Why this is generated rather than hand-written
----------------------------------------------
gldGetProcAddress_GL46 used to answer an unknown name with a no-op whose
argument count came from _glsGuessArgCount, a pile of name-prefix heuristics.
On x86 __stdcall the callee pops the arguments, so a wrong count silently
corrupts the caller's stack.  822 signatures transcribed by hand is exactly the
failure mode that produced that bug; a script driven by glmap.json's params[]
cannot transpose or drop a parameter the way a human copying from documentation
can, and a wrong params[] transcription becomes a compile error rather than a
runtime stack smash.

The generated header is committed - there is no build-time codegen step, matching
how the rest of this tree treats generated source.  Re-running the script must
produce byte-identical output; nothing here depends on dict iteration order that
is not sorted, on a timestamp, or on the host platform.
"""

import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from gl_stub_classification import CLASSIFICATION       # noqa: E402
from gl_dsa_mapping import DSA_MAPPING                   # noqa: E402

OUT_PATH = os.path.join(ROOT, 'src', 'gl46', 'gl_generated_stubs.h')

# ---------------------------------------------------------------------------
# glmap.json
# ---------------------------------------------------------------------------

with io.open(os.path.join(HERE, 'glmap.json'), encoding='utf8') as fh:
    GLMAP = json.load(fh)

CORE = GLMAP['core']
ALIAS_TO_CORE = GLMAP['alias_to_core']
MISSING_CORE = sorted(GLMAP['missing_core'])
MISSING_ALIAS = sorted(GLMAP['missing_alias'])
RESOLVABLE = set(GLMAP['resolvable'])

IDENT = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)$')


def param_name(decl):
    """'const GLchar *const*string' -> 'string'."""
    m = IDENT.search(decl.strip())
    if not m:
        raise SystemExit('cannot find a parameter name in %r' % decl)
    return m.group(1)


def signature(name):
    e = CORE[name]
    params = e['params']
    decl = ', '.join(params) if params else 'void'
    return e['ret'], decl, [param_name(p) for p in params], params


def sentinel(ret):
    """A spec-legal value to hand back from a stub that does nothing."""
    if ret == 'void':
        return None
    if ret == 'GLboolean':
        return 'GL_FALSE'
    if ret.endswith('*'):
        return 'NULL'
    return '0'


# ---------------------------------------------------------------------------
# Existing symbols the generated bodies may call.  Every forward is checked
# against these, so a typo in a classification table is caught here rather than
# by the compiler (or, worse, not at all if the name is never exercised).
# ---------------------------------------------------------------------------

def scan_symbols():
    gls = set()
    with io.open(os.path.join(ROOT, 'src', 'gl46', 'gl_impl.h'),
                 encoding='utf8', errors='replace') as fh:
        gls |= set(re.findall(r'\b(_gls[A-Za-z0-9_]+)\s*\(', fh.read()))
    stubs = {}
    with io.open(os.path.join(ROOT, 'src', 'gl46', 'gl_modern_stubs.h'),
                 encoding='utf8', errors='replace') as fh:
        text = fh.read()
    stubs.update(dict(re.findall(
        r'\{\s*"(gl\w+)"\s*,\s*\(PROC\)\s*(_stub_\w+)', text)))
    stub_defs = set(re.findall(r'\b(_stub_gl\w+)\s*\(', text))
    return gls, stubs, stub_defs


GLS_SYMBOLS, MODERN_TABLE, MODERN_STUB_DEFS = scan_symbols()

# Helpers this generator emits into the header prologue.  Forwards naming one of
# these are as real as forwards naming a _gls* function.
GEN_HELPERS = {
    '_genDsaTexTarget', '_genDsaTexBinding', '_genBindTextureUnit',
    '_genBlitNamedFramebuffer', '_genCreateTextures', '_genCopyNamedBufferSubData',
    '_genVaoAttribQuery', '_genSamplerParam', '_genGetUniformValues',
    '_genUnpackP', '_genD2F',
}


def check_forward(where, name, text):
    if not text:
        return
    for sym in re.findall(r'\b(_gls[A-Za-z0-9_]+|_stub_gl[A-Za-z0-9_]+|_gen[A-Za-z0-9_]+)\s*\(', text):
        if sym in GLS_SYMBOLS or sym in MODERN_STUB_DEFS or sym in GEN_HELPERS:
            continue
        raise SystemExit('%s: %s forwards to unknown symbol %s' % (where, name, sym))


# ---------------------------------------------------------------------------
# Per-family body emitters.  Each returns a list of C statement lines (already
# indented by four spaces) for the body of _gen_<name>.
# ---------------------------------------------------------------------------

def body_silent(ret):
    s = sentinel(ret)
    return ['    return %s;' % s] if s else []


def body_call(ret, call):
    """`call` is an expression, a raw { ... } block, or '' for a silent no-op."""
    if call == '':
        return body_silent(ret)
    if call.startswith('{'):
        lines = ['    ' + call]
        s = sentinel(ret)
        if s:
            lines.append('    return %s;' % s)
        return lines
    if ret == 'void':
        return ['    %s;' % call]
    return ['    return (%s)%s;' % (ret, call)]


DSA_BIND = {
    # kind: (prologue lines, restore lines).  {obj} is the first parameter.
    'buf': (
        ['    GLS_State *_genS = glsGetState();',
         '    GLuint_t _genPrev = _genS->boundArrayBuffer;',
         '    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int){obj});'],
        ['    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);']),
    'fbo': (
        ['    GLS_State *_genS = glsGetState();',
         '    GLuint_t _genPrev = _genS->boundDrawFBO;',
         '    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int){obj});'],
        ['    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);']),
    'fbo-read': (
        ['    GLS_State *_genS = glsGetState();',
         '    GLuint_t _genPrev = _genS->boundReadFBO;',
         '    _glsBindFramebuffer((unsigned int)GL_READ_FRAMEBUFFER, (unsigned int){obj});'],
        ['    _glsBindFramebuffer((unsigned int)GL_READ_FRAMEBUFFER, (unsigned int)_genPrev);']),
    'rbo': (
        ['    GLS_State *_genS = glsGetState();',
         '    GLuint_t _genPrev = _genS->boundRBO;',
         '    _glsBindRenderbuffer((unsigned int)GL_RENDERBUFFER, (unsigned int){obj});'],
        ['    _glsBindRenderbuffer((unsigned int)GL_RENDERBUFFER, (unsigned int)_genPrev);']),
    'vao': (
        ['    GLS_State *_genS = glsGetState();',
         '    GLuint_t _genPrev = _genS->boundVAO;',
         '    _glsBindVertexArray((unsigned int){obj});'],
        ['    _glsBindVertexArray((unsigned int)_genPrev);']),
    'tex': (
        ['    GLenum _genTgt = _genDsaTexTarget((GLuint){obj});',
         '    GLuint _genPrev = _genDsaTexBinding(_genTgt);',
         '    _glsBindTexture((unsigned int)_genTgt, (unsigned int){obj});'],
        ['    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);']),
}


def body_dsa(name, ret, args):
    kind, call = DSA_MAPPING[name]
    check_forward('gl_dsa_mapping.py', name, call)
    if call is None:
        raise SystemExit('%s has no DSA implementation' % name)
    if kind == 'none':
        return body_call(ret, call)

    obj = args[0]
    pro, restore = DSA_BIND[kind]
    pro = [ln.replace('{obj}', obj) for ln in pro]
    lines = list(pro)
    if ret == 'void':
        lines += body_call(ret, call)
        lines += restore
    else:
        lines.insert(len(pro), '    %s _genRet;' % ret)
        # Declarations must precede statements (C89): move the temp up with the
        # other locals, then reorder so all declarations come first.
        decls = [ln for ln in lines if _is_decl(ln)]
        stmts = [ln for ln in lines if not _is_decl(ln)]
        lines = decls + stmts
        inner = body_call('void', call)
        if call.startswith('{') or call == '':
            lines += inner
            lines += restore
            lines.append('    return %s;' % (sentinel(ret) or '0'))
            return lines
        lines.append('    _genRet = (%s)%s;' % (ret, call))
        lines += restore
        lines.append('    return _genRet;')
    return lines


def _is_decl(line):
    t = line.strip()
    return (t.startswith('GLS_State ') or t.startswith('GLuint_t ') or
            t.startswith('GLenum ') or t.startswith('GLuint ') or
            t.startswith('GLboolean ') or t.startswith('void *') or
            t.startswith('GLint ') or t.startswith('GLfloat '))


# --- vertex attribute / colour type variants ------------------------------

# suffix -> (component count, is_vector, normalise divisor or None, )
SCALAR_NORM = {
    'b': '127.0f', 'i': '2147483647.0f', 's': '32767.0f',
    'ub': '255.0f', 'ui': '4294967295.0f', 'us': '65535.0f',
}
# Signed types only: GL clamps the most negative representable value up to
# exactly -1.0 (e.g. GLbyte -128 / 127 would otherwise be -1.008).
SCALAR_SIGNED = {'b', 'i', 's'}


def _components(args, count, vector, base, defaults):
    """C expressions for the four float components handed to the forward.

    `base` is the type suffix ('b', 'ub', ...) when the values must be
    normalised into [-1,1] / [0,1], or None for a plain cast."""
    src = args[-1]
    norm = SCALAR_NORM.get(base) if base else None
    out = []
    for i in range(4):
        if i >= count:
            out.append(defaults[i])
            continue
        raw = '%s[%d]' % (src, i) if vector else args[len(args) - count + i]
        if norm:
            if base in SCALAR_SIGNED:
                expr = '((float)%s / %s < -1.0f ? -1.0f : (float)%s / %s)' % (
                    raw, norm, raw, norm)
            else:
                expr = '(float)%s / %s' % (raw, norm)
        else:
            expr = '(float)%s' % raw
        out.append(expr)
    return out


ATTRIB_SUFFIX = re.compile(r'^glVertexAttrib(L?)([1-4])(N?)([a-zA-Z]*)$')


def body_attrib_variant(name, ret, args):
    m = ATTRIB_SUFFIX.match(name)
    _, count, npart, suffix = m.group(1), int(m.group(2)), m.group(3), m.group(4)
    vector = suffix.endswith('v')
    base = suffix[:-1] if vector else suffix
    comps = _components(args, count, vector, base if npart == 'N' else None,
                        ['0.0f', '0.0f', '0.0f', '1.0f'])
    guard = '    if (!%s) return;\n' % args[-1] if vector else ''
    return ([guard.rstrip('\n')] if guard else []) + [
        '    _glsVertexAttrib4f((unsigned int)%s, %s, %s, %s, %s);'
        % (args[0], comps[0], comps[1], comps[2], comps[3])]


SECCOLOR_SUFFIX = re.compile(r'^glSecondaryColor3([a-zA-Z]*)$')
WINDOWPOS_SUFFIX = re.compile(r'^glWindowPos([23])([a-zA-Z]*)$')


def body_gl14_variant(name, ret, args):
    m = SECCOLOR_SUFFIX.match(name)
    if m:
        suffix = m.group(1)
        vector = suffix.endswith('v')
        base = suffix[:-1] if vector else suffix
        # GL converts integer colour components to float by normalising them.
        comps = _components(args, 3, vector, base, ['0.0f'] * 4)
        guard = ['    if (!%s) return;' % args[-1]] if vector else []
        return guard + ['    _glsSecondaryColor3f(%s, %s, %s);'
                        % (comps[0], comps[1], comps[2])]
    m = WINDOWPOS_SUFFIX.match(name)
    count = int(m.group(1))
    suffix = m.group(2)
    vector = suffix.endswith('v')
    comps = _components(args, count, vector, None, ['0.0f'] * 4)
    guard = ['    if (!%s) return;' % args[-1]] if vector else []
    return guard + ['    _glsWindowPos3f(%s, %s, %s);' % (comps[0], comps[1], comps[2])]


# --- packed 2_10_10_10_REV types ------------------------------------------

PACKED_RE = re.compile(
    r'^gl(Vertex|Color|Normal|TexCoord|MultiTexCoord|SecondaryColor|VertexAttrib)P([1-4])uiv?$')

PACKED_FORWARD = {
    'Vertex': ('_glsVertex4f', 4, []),
    'Color': ('_glsColor4f', 4, []),
    'Normal': ('_glsNormal3f', 3, []),
    'TexCoord': ('_glsTexCoord4f', 4, []),
    'MultiTexCoord': ('_glsMultiTexCoord4fARB', 4, ['(unsigned int)texture']),
    'SecondaryColor': ('_glsSecondaryColor3f', 3, []),
    'VertexAttrib': ('_glsVertexAttrib4f', 4, ['(unsigned int)index']),
}


def body_packed(name, ret, args):
    m = PACKED_RE.match(name)
    prefix, count = m.group(1), int(m.group(2))
    fwd, fwd_arity, lead = PACKED_FORWARD[prefix]
    vector = name.endswith('uiv')
    src = args[-1]
    value = '%s[0]' % src if vector else src
    normalized = 'normalized' if prefix == 'VertexAttrib' else 'GL_TRUE'
    lines = ['    float _genV[4];']
    if vector:
        lines.append('    if (!%s) return;' % src)
    lines.append('    _genUnpackP((GLenum)type, (GLuint)%s, (GLboolean)%s, _genV);'
                 % (value, normalized))
    defaults = ['0.0f', '0.0f', '0.0f', '1.0f']
    comps = ['_genV[%d]' % i if i < count else defaults[i] for i in range(fwd_arity)]
    lines.append('    %s(%s);' % (fwd, ', '.join(lead + comps)))
    return lines


# --- integer variants ------------------------------------------------------

ATTRIB_I_RE = re.compile(r'^glVertexAttribI([1-4])([a-zA-Z]*)$')


def body_integer_variant(name, ret, args):
    m = ATTRIB_I_RE.match(name)
    if m:
        count = int(m.group(1))
        vector = m.group(2).endswith('v')
        comps = _components(args, count, vector, None, ['0.0f', '0.0f', '0.0f', '1.0f'])
        guard = ['    if (!%s) return;' % args[-1]] if vector else []
        return guard + [
            '    _glsVertexAttrib4f((unsigned int)%s, %s, %s, %s, %s);'
            % (args[0], comps[0], comps[1], comps[2], comps[3])]
    if name in ('glTexParameterIiv', 'glTexParameterIuiv'):
        return ['    _glsTexParameteri((unsigned int)target, (unsigned int)pname,'
                ' params ? (int)params[0] : 0);']
    if name in ('glSamplerParameterIiv', 'glSamplerParameterIuiv'):
        return ['    _glsSamplerParameteri((unsigned int)sampler, (unsigned int)pname,'
                ' param ? (int)param[0] : 0);']
    if name == 'glGetTexParameterIiv':
        return ['    _glsGetTexParameteriv((unsigned int)target, (unsigned int)pname,'
                ' (int *)params);']
    if name == 'glGetTexParameterIuiv':
        return ['    GLint _genV[4];',
                '    memset(_genV, 0, sizeof(_genV));',
                '    _glsGetTexParameteriv((unsigned int)target, (unsigned int)pname, _genV);',
                '    if (params) params[0] = (GLuint)_genV[0];']
    if name in ('glGetSamplerParameterIiv', 'glGetSamplerParameterIuiv'):
        cast = 'GLuint' if name.endswith('Iuiv') else 'GLint'
        return ['    GLS_Sampler *_genSmp = glsFindSampler((GLuint_t)sampler);',
                '    if (!params) return;',
                '    params[0] = (%s)_genSamplerParam(_genSmp, (GLenum)pname);' % cast]
    # glGetVertexAttribIiv / glGetVertexAttribIuiv
    cast = 'GLuint' if name.endswith('Iuiv') else 'GLint'
    return ['    GLS_VAO *_genVao = glsFindVAO(glsGetState()->boundVAO);',
            '    if (!params) return;',
            '    params[0] = (%s)((_genVao && index < GLS_MAX_VERTEX_ATTRIBS)' % cast,
            '        ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0);']


# --- uniform families ------------------------------------------------------

UNIFORM_SCALAR = re.compile(r'^([1-4])(dv|uiv|fv|iv|d|ui|f|i)$')
UNIFORM_MATRIX = re.compile(r'^Matrix([234])(x([234]))?(fv|dv)$')


def uniform_stmts(tail, args, forward):
    """Statements for a glUniform<tail>-shaped call over `args`
    (location first).  Shared by uniform-widen, fp64-demote and
    program-uniform, which differ only in what wraps them."""
    loc = args[0]
    m = UNIFORM_SCALAR.match(tail)
    if m:
        n, kind = int(m.group(1)), m.group(2)
        if kind in ('d', 'ui', 'f', 'i'):
            cast = 'int' if kind in ('ui', 'i') else 'float'
            vals = ', '.join('(%s)%s' % (cast, a) for a in args[1:1 + n])
            return ['    %s((int)%s, %s);' % (forward, loc, vals)]
        # vector forms: <count> elements of n components
        cap = 256
        per = n
        value = args[-1]
        count = args[-2]
        if kind in ('fv', 'iv'):
            # Already the sibling's own element type (these only reach here via
            # glProgramUniform*, whose plain glUniform* form is already mapped).
            return ['    %s((int)%s, (int)%s, %s);' % (forward, loc, count, value)]
        if kind == 'dv':
            return ['    float _genB[%d];' % cap,
                    '    int _genN = (int)%s;' % count,
                    '    if (!%s || _genN <= 0) return;' % value,
                    '    if (_genN > %d) _genN = %d;' % (cap // per, cap // per),
                    '    _genD2F(_genB, %d, %s, _genN * %d);' % (cap, value, per),
                    '    %s((int)%s, _genN, _genB);' % (forward, loc)]
        # uiv
        return ['    int _genB[%d];' % cap,
                '    int _genI;',
                '    int _genN = (int)%s;' % count,
                '    if (!%s || _genN <= 0) return;' % value,
                '    if (_genN > %d) _genN = %d;' % (cap // per, cap // per),
                '    for (_genI = 0; _genI < _genN * %d; _genI++)' % per,
                '        _genB[_genI] = (int)%s[_genI];' % value,
                '    %s((int)%s, _genN, _genB);' % (forward, loc)]

    m = UNIFORM_MATRIX.match(tail)
    cols = int(m.group(1))
    rows = int(m.group(3)) if m.group(3) else cols
    dbl = m.group(4) == 'dv'
    count, transpose, value = args[-3], args[-2], args[-1]
    if cols == rows and not dbl:
        return ['    %s((int)%s, (int)%s, (unsigned char)%s, %s);'
                % (forward, loc, count, transpose, value)]
    if cols == rows and dbl:
        cap = 64
        return ['    float _genB[%d];' % cap,
                '    int _genN = (int)%s;' % count,
                '    if (!%s || _genN <= 0) return;' % value,
                '    if (_genN > %d) _genN = %d;' % (cap // (cols * cols), cap // (cols * cols)),
                '    _genD2F(_genB, %d, %s, _genN * %d);' % (cap, value, cols * cols),
                '    %s((int)%s, _genN, (unsigned char)%s, _genB);'
                % (forward, loc, transpose)]
    # Non-square matCxR: one float4 constant register per column, zero padded,
    # uploaded as a plain float4 array.  This is the same packing
    # _glsUploadMatrices performs for the square case.
    maxmat = 64 // (cols * 4)
    return ['    float _genB[64];',
            '    int _genM, _genC, _genR;',
            '    int _genN = (int)%s;' % count,
            '    if (!%s || _genN <= 0) return;' % value,
            '    if (_genN > %d) _genN = %d;' % (maxmat, maxmat),
            '    memset(_genB, 0, sizeof(_genB));',
            '    for (_genM = 0; _genM < _genN; _genM++)',
            '        for (_genC = 0; _genC < %d; _genC++)' % cols,
            '            for (_genR = 0; _genR < %d; _genR++)' % rows,
            '                _genB[(_genM * %d + _genC) * 4 + _genR] = (float)(%s ?'
            % (cols, transpose),
            '                    %s[_genM * %d + _genR * %d + _genC]'
            % (value, cols * rows, cols),
            '                  : %s[_genM * %d + _genC * %d + _genR]);'
            % (value, cols * rows, rows),
            '    %s((int)%s, _genN * %d, _genB);' % (forward, loc, cols)]


def body_uniform(name, ret, args, forward):
    tail = name[len('glUniform'):]
    return uniform_stmts(tail, args, forward)


def body_program_uniform(name, ret, args, forward):
    tail = name[len('glProgramUniform'):]
    inner = uniform_stmts(tail, args[1:], forward)
    decls = [ln for ln in inner if _is_uniform_decl(ln)]
    rest = [ln for ln in inner if not _is_uniform_decl(ln)]
    # Argument-validation early-outs must happen before the program is switched,
    # or they would leave the previous binding unrestored.
    guards = [ln for ln in rest if _is_guard(ln)]
    stmts = [ln for ln in rest if not _is_guard(ln)]
    return (['    GLS_State *_genS = glsGetState();',
             '    GLuint_t _genPrevProg = _genS->boundProgram;'] + decls + guards +
            ['    _glsUseProgram((unsigned int)%s);' % args[0]] + stmts +
            ['    _glsUseProgram((unsigned int)_genPrevProg);'])


def _is_guard(line):
    t = line.strip()
    return t.startswith('if (') and t.endswith('return;')


def _is_uniform_decl(line):
    t = line.strip()
    return (t.startswith('float _gen') or t.startswith('int _gen'))


# --- robustness ------------------------------------------------------------

ROBUST_BODY = {
    'glReadnPixels': ['    _glsReadPixels((int)x, (int)y, (int)width, (int)height,'
                      ' (unsigned int)format, (unsigned int)type, data);'],
    'glGetnTexImage': ['    _glsGetTexImage((unsigned int)target, (int)level,'
                       ' (unsigned int)format, (unsigned int)type, pixels);'],
    'glGetnCompressedTexImage': ['    _glsGetCompressedTexImage((unsigned int)target,'
                                 ' (int)lod, pixels);'],
    'glGetnPolygonStipple': ['    if (bufSize < 128) return;',
                             '    _glsGetPolygonStipple((unsigned char *)pattern);'],
    'glGetnPixelMapfv': ['    if (bufSize < 32) return;',
                         '    _glsGetPixelMapfv((unsigned int)map, (float *)values);'],
    'glGetnPixelMapuiv': ['    if (bufSize < 32) return;',
                          '    _glsGetPixelMapuiv((unsigned int)map, (unsigned int *)values);'],
    'glGetnPixelMapusv': ['    if (bufSize < 32) return;',
                          '    _glsGetPixelMapusv((unsigned int)map, (unsigned short *)values);'],
    'glGetnMapfv': ['    if (bufSize < 1) return;',
                    '    _glsGetMapfv((unsigned int)target, (unsigned int)query, (float *)v);'],
    'glGetnMapiv': ['    if (bufSize < 1) return;',
                    '    _glsGetMapiv((unsigned int)target, (unsigned int)query, (int *)v);'],
    'glGetnMapdv': ['    float _genB[16];',
                    '    int _genI;',
                    '    int _genN = (int)bufSize;',
                    '    if (!v || _genN <= 0) return;',
                    '    if (_genN > 16) _genN = 16;',
                    '    memset(_genB, 0, sizeof(_genB));',
                    '    _glsGetMapfv((unsigned int)target, (unsigned int)query, _genB);',
                    '    for (_genI = 0; _genI < _genN; _genI++) v[_genI] = (GLdouble)_genB[_genI];'],
}

# The imaging subset (histogram, minmax, convolution, colour table) is not
# implemented anywhere in this wrapper.  The *entry point* is real, the
# *feature* simply has nothing to report, so these write no results and return
# rather than warning on every call.
ROBUST_IMAGING = {
    'glGetnHistogram', 'glGetnMinmax', 'glGetnConvolutionFilter',
    'glGetnSeparableFilter', 'glGetnColorTable',
}


def body_robustness(name, ret, args):
    if name in ROBUST_BODY:
        return ROBUST_BODY[name]
    if name in ROBUST_IMAGING:
        return []
    # glGetnUniform{fv,iv,uiv,dv}
    kind = name[len('glGetnUniform'):]
    return _uniform_readback(kind, 'program', 'location', 'params', 'bufSize')


def _uniform_readback(kind, program, location, out, bufsize=None):
    ctype = {'fv': 'GLfloat', 'iv': 'GLint', 'uiv': 'GLuint', 'dv': 'GLdouble'}[kind]
    lines = ['    float _genB[16];',
             '    int _genI, _genN;',
             '    if (!%s) return;' % out,
             '    _genN = _genGetUniformValues((GLuint)%s, (GLint)%s, _genB, 16);'
             % (program, location)]
    if bufsize:
        lines.append('    if (_genN > (int)(%s / (GLsizei)sizeof(%s)))'
                     % (bufsize, ctype))
        lines.append('        _genN = (int)(%s / (GLsizei)sizeof(%s));' % (bufsize, ctype))
    lines += ['    for (_genI = 0; _genI < _genN; _genI++)',
              '        %s[_genI] = (%s)_genB[_genI];' % (out, ctype)]
    return lines


# --- get-gap ---------------------------------------------------------------

INDEXED_GET = {
    'glGetBooleani_v': ('_glsGetBooleanv', 'GLboolean', 'unsigned char'),
    'glGetIntegeri_v': ('_glsGetIntegerv', 'GLint', 'int'),
    'glGetInteger64i_v': ('_glsGetIntegerv', 'GLint64', 'int'),
    'glGetFloati_v': ('_glsGetFloatv', 'GLfloat', 'float'),
    'glGetDoublei_v': ('_glsGetFloatv', 'GLdouble', 'float'),
}


def body_get_gap(name, ret, args):
    if name.startswith('glGetUniform'):
        return _uniform_readback(name[len('glGetUniform'):], 'program', 'location', 'params')
    if name in INDEXED_GET:
        fwd, gltype, ctype = INDEXED_GET[name]
        # D3D9 has exactly one of each indexed state, so index > 0 reads as zero.
        lines = ['    %s _genB[16];' % ctype,
                 '    int _genI;',
                 '    if (!data) return;',
                 '    memset(_genB, 0, sizeof(_genB));',
                 '    if (index == 0)',
                 '        %s((unsigned int)target, _genB);' % fwd,
                 '    for (_genI = 0; _genI < 4; _genI++)',
                 '        data[_genI] = (%s)_genB[_genI];' % gltype]
        return lines
    if name == 'glGetVertexAttribPointerv':
        return ['    GLS_VAO *_genVao = glsFindVAO(glsGetState()->boundVAO);',
                '    if (!pointer) return;',
                '    (void)pname;',
                '    pointer[0] = (_genVao && index < GLS_MAX_VERTEX_ATTRIBS)',
                '        ? (void *)_genVao->attribs[index].pointer : NULL;']
    if name in ('glGetVertexAttribfv', 'glGetVertexAttribiv',
                'glGetVertexAttribdv', 'glGetVertexAttribLdv'):
        ctype = {'glGetVertexAttribfv': 'GLfloat', 'glGetVertexAttribiv': 'GLint',
                 'glGetVertexAttribdv': 'GLdouble', 'glGetVertexAttribLdv': 'GLdouble'}[name]
        return ['    GLS_State *_genS = glsGetState();',
                '    GLS_VAO *_genVao = glsFindVAO(_genS->boundVAO);',
                '    int _genI;',
                '    if (!params || index >= GLS_MAX_VERTEX_ATTRIBS) return;',
                '    if (pname == GL_CURRENT_VERTEX_ATTRIB) {',
                '        for (_genI = 0; _genI < 4; _genI++)',
                '            params[_genI] = (%s)(_genVao' % ctype,
                '                ? _genVao->attribs[index].defaultValue[_genI] : 0.0f);',
                '        return;',
                '    }',
                '    params[0] = (%s)(_genVao' % ctype,
                '        ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0);']
    if name == 'glGetShaderSource':
        return ['    GLS_Shader *_genSh = glsFindShader((GLuint_t)shader);',
                '    GLsizei _genLen = 0;',
                '    if (_genSh && _genSh->source && source && bufSize > 0) {',
                '        strncpy(source, _genSh->source, (size_t)bufSize - 1);',
                '        source[bufSize - 1] = \'\\0\';',
                '        _genLen = (GLsizei)strlen(source);',
                '    } else if (source && bufSize > 0) {',
                '        source[0] = \'\\0\';',
                '    }',
                '    if (length) *length = _genLen;']
    if name == 'glGetAttachedShaders':
        return ['    GLS_Program *_genP = glsFindProgram((GLuint_t)program);',
                '    GLsizei _genN = 0;',
                '    if (_genP && shaders) {',
                '        if (_genP->vertShader && _genN < maxCount)',
                '            shaders[_genN++] = (GLuint)_genP->vertShader;',
                '        if (_genP->fragShader && _genN < maxCount)',
                '            shaders[_genN++] = (GLuint)_genP->fragShader;',
                '    }',
                '    if (count) *count = _genN;']
    if name in ('glGetSamplerParameterfv', 'glGetSamplerParameteriv'):
        ctype = 'GLfloat' if name.endswith('fv') else 'GLint'
        return ['    GLS_Sampler *_genSmp = glsFindSampler((GLuint_t)sampler);',
                '    if (!params) return;',
                '    params[0] = (%s)_genSamplerParam(_genSmp, (GLenum)pname);' % ctype]
    if name == 'glGetFramebufferParameteriv':
        return ['    (void)target;',
                '    _glsGetIntegerv((unsigned int)pname, (int *)params);']
    # glGetShaderPrecisionFormat: IEEE 754 binary32, which is what SM3 uses.
    return ['    (void)shadertype; (void)precisiontype;',
            '    if (range) { range[0] = 127; range[1] = 127; }',
            '    if (precision) precision[0] = 23;']


# --- is-misc ---------------------------------------------------------------

IS_MISC_BODY = {
    'glIsQuery': ['    return glsFindQuery((GLuint_t)id) ? GL_TRUE : GL_FALSE;'],
    'glIsSampler': ['    return glsFindSampler((GLuint_t)sampler) ? GL_TRUE : GL_FALSE;'],
    # A sync object is a raw pointer handed out by _glsFenceSync, not a name in
    # a table; non-NULL is the most this wrapper can honestly say about it.
    'glIsSync': ['    return sync ? GL_TRUE : GL_FALSE;'],
    'glIsEnabledi': ['    if (index != 0) return GL_FALSE;',
                     '    return (GLboolean)_glsIsEnabled((unsigned int)target);'],
    # D3D9 always clamps fixed-function colour output; the query is honoured,
    # the request to disable clamping cannot be.
    'glClampColor': ['    (void)target; (void)clamp;'],
    'glClearDepthf': ['    _glsClearDepth((double)d);'],
    'glDepthRangef': ['    _glsDepthRange((double)n, (double)f);'],
}


# ---------------------------------------------------------------------------
# Header prologue: the shared helpers the bodies above call.
# ---------------------------------------------------------------------------

PROLOGUE = r'''
/* ===== Shared helpers for the generated bodies ===== */

/* A DSA texture command names the texture but not its target; the target is
 * whatever the texture was created with.  GLS_Texture records it. */
static GLenum _genDsaTexTarget(GLuint texture)
{
    GLS_Texture *t = glsFindTexture((GLuint_t)texture);
    return (t && t->target) ? (GLenum)t->target : (GLenum)GL_TEXTURE_2D;
}

static GLuint _genDsaTexBinding(GLenum target)
{
    GLS_State *s = glsGetState();
    int unit = (int)(s->activeTexUnit - GL_TEXTURE0);
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS)
        unit = 0;
    if (target == GL_TEXTURE_CUBE_MAP)
        return (GLuint)s->boundTextureCube[unit];
    if (target == GL_TEXTURE_3D)
        return (GLuint)s->boundTexture3D[unit];
    if (target == GL_TEXTURE_BUFFER)
        return (GLuint)s->boundTextureBuffer[unit];
    return (GLuint)s->boundTexture2D[unit];
}

/* glBindTextureUnit binds to an explicit unit rather than the active one. */
static void _genBindTextureUnit(GLuint unit, GLuint texture)
{
    GLS_State *s = glsGetState();
    GLenum prevUnit = (GLenum)s->activeTexUnit;
    GLenum tgt = _genDsaTexTarget(texture);
    _glsActiveTexture((unsigned int)(GL_TEXTURE0 + unit));
    _glsBindTexture((unsigned int)tgt, (unsigned int)texture);
    _glsActiveTexture((unsigned int)prevUnit);
}

static void _genBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer,
                                     int srcX0, int srcY0, int srcX1, int srcY1,
                                     int dstX0, int dstY0, int dstX1, int dstY1,
                                     unsigned int mask, unsigned int filter)
{
    GLS_State *s = glsGetState();
    GLuint_t prevRead = s->boundReadFBO;
    GLuint_t prevDraw = s->boundDrawFBO;
    _glsBindFramebuffer((unsigned int)GL_READ_FRAMEBUFFER, (unsigned int)readFramebuffer);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)drawFramebuffer);
    _glsBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                        mask, filter);
    _glsBindFramebuffer((unsigned int)GL_READ_FRAMEBUFFER, (unsigned int)prevRead);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)prevDraw);
}

/* glCreateTextures differs from glGenTextures in that the target is decided at
 * creation rather than at first bind, so record it now. */
static void _genCreateTextures(GLenum target, int n, unsigned int *textures)
{
    int i;
    _glsGenTextures(n, textures);
    if (!textures)
        return;
    for (i = 0; i < n; i++) {
        GLS_Texture *t = glsFindTexture((GLuint_t)textures[i]);
        if (t)
            t->target = (GLenum_t)target;
    }
}

static void _genCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer,
                                       ptrdiff_t readOffset, ptrdiff_t writeOffset,
                                       ptrdiff_t size)
{
    GLS_State *s = glsGetState();
    GLuint_t prevR = s->boundCopyReadBuffer;
    GLuint_t prevW = s->boundCopyWriteBuffer;
    s->boundCopyReadBuffer  = (GLuint_t)readBuffer;
    s->boundCopyWriteBuffer = (GLuint_t)writeBuffer;
    _glsCopyBufferSubData((unsigned int)GL_COPY_READ_BUFFER,
                          (unsigned int)GL_COPY_WRITE_BUFFER,
                          readOffset, writeOffset, size);
    s->boundCopyReadBuffer  = prevR;
    s->boundCopyWriteBuffer = prevW;
}

/* The scalar half of glGetVertexAttrib*: everything except
 * GL_CURRENT_VERTEX_ATTRIB, which is a vec4 and is handled by the caller. */
static GLint _genVaoAttribQuery(const GLS_VertexAttrib *a, GLenum pname)
{
    if (!a)
        return 0;
    switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:        return a->enabled ? 1 : 0;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:           return (GLint)a->size;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:         return (GLint)a->stride;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:           return (GLint)a->type;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:     return a->normalized ? 1 : 0;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:        return a->integer ? 1 : 0;
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:        return (GLint)a->divisor;
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: return (GLint)a->bufferBinding;
    default:                                    return 0;
    }
}

static GLfloat _genSamplerParam(const GLS_Sampler *s, GLenum pname)
{
    if (!s)
        return 0.0f;
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER:   return (GLfloat)s->minFilter;
    case GL_TEXTURE_MAG_FILTER:   return (GLfloat)s->magFilter;
    case GL_TEXTURE_WRAP_S:       return (GLfloat)s->wrapS;
    case GL_TEXTURE_WRAP_T:       return (GLfloat)s->wrapT;
    case GL_TEXTURE_WRAP_R:       return (GLfloat)s->wrapR;
    case GL_TEXTURE_MIN_LOD:      return s->minLod;
    case GL_TEXTURE_MAX_LOD:      return s->maxLod;
    case GL_TEXTURE_LOD_BIAS:     return s->lodBias;
    case GL_TEXTURE_COMPARE_MODE: return (GLfloat)s->compareMode;
    case GL_TEXTURE_COMPARE_FUNC: return (GLfloat)s->compareFunc;
    case GL_TEXTURE_MAX_ANISOTROPY: return s->maxAnisotropy;
    default:                      return 0.0f;
    }
}

/* Uniform readback.  The wrapper keeps the last value written per location in
 * GLS_Program::uniforms, which is the only source glGetUniform* has. */
static int _genGetUniformValues(GLuint program, GLint location, float *out, int maxOut)
{
    GLS_Program *p = glsFindProgram((GLuint_t)program);
    int i, n;
    if (!p || !out || maxOut <= 0)
        return 0;
    for (i = 0; i < p->uniformCount && i < GLS_MAX_UNIFORMS; i++) {
        if (p->uniforms[i].location != location || !p->uniforms[i].set)
            continue;
        switch (p->uniforms[i].type) {
        case 0: case 1: n = 1;  break;
        case 2:         n = 2;  break;
        case 3:         n = 3;  break;
        case 4:         n = 4;  break;
        case 5:         n = 4;  break;
        case 6:         n = 9;  break;
        default:        n = 16; break;
        }
        if (n > maxOut)
            n = n > 16 ? 16 : maxOut;
        memcpy(out, p->uniforms[i].data, (size_t)n * sizeof(float));
        return n;
    }
    return 0;
}

static void _genD2F(float *dst, int cap, const GLdouble *src, int n)
{
    int i;
    if (!dst || !src)
        return;
    if (n > cap)
        n = cap;
    for (i = 0; i < n; i++)
        dst[i] = (float)src[i];
}

/*
 * GL_[UNSIGNED_]INT_2_10_10_10_REV unpack.
 *
 * Bit layout, least significant first: x[0..9] y[10..19] z[20..29] w[30..31].
 * The signed form is two's complement in 10 (and 2) bits, and GL clamps the
 * most negative representable value up to exactly -1.0 when normalising.
 */
static void _genUnpackP(GLenum type, GLuint packed, GLboolean normalized, float *out)
{
    int i;
    int raw[4];
    float scale[4];

    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;

    if (type == GL_INT_2_10_10_10_REV) {
        raw[0] = (int)((packed      ) & 0x3FFu);
        raw[1] = (int)((packed >> 10) & 0x3FFu);
        raw[2] = (int)((packed >> 20) & 0x3FFu);
        raw[3] = (int)((packed >> 30) & 0x3u);
        for (i = 0; i < 3; i++)
            if (raw[i] & 0x200) raw[i] -= 0x400;
        if (raw[3] & 0x2) raw[3] -= 0x4;
        scale[0] = scale[1] = scale[2] = 511.0f;
        scale[3] = 1.0f;
    } else if (type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        raw[0] = (int)((packed      ) & 0x3FFu);
        raw[1] = (int)((packed >> 10) & 0x3FFu);
        raw[2] = (int)((packed >> 20) & 0x3FFu);
        raw[3] = (int)((packed >> 30) & 0x3u);
        scale[0] = scale[1] = scale[2] = 1023.0f;
        scale[3] = 3.0f;
    } else {
        /* Not a packed type this entry point accepts. */
        return;
    }

    for (i = 0; i < 4; i++) {
        if (normalized) {
            out[i] = (float)raw[i] / scale[i];
            if (out[i] < -1.0f)
                out[i] = -1.0f;
        } else {
            out[i] = (float)raw[i];
        }
    }
}
'''


# ---------------------------------------------------------------------------
# Emit
# ---------------------------------------------------------------------------

# GL 4.x calls implemented by src/gl46/advanced_emulation.c.  Keeping this
# list next to the emitter makes it impossible for a regenerated dispatcher to
# silently replace a completed emulation path with the old warn-and-return
# body merely because its historical classification was "ceiling-noop".
ADVANCED_EMULATION = {
    'glActiveShaderProgram', 'glBindFragDataLocation',
    'glBindFragDataLocationIndexed', 'glBindProgramPipeline',
    'glBindVertexBuffer', 'glBindVertexBuffers', 'glBufferStorage',
    'glClearBufferData', 'glClearBufferSubData', 'glClearTexImage',
    'glClearTexSubImage', 'glCopyImageSubData', 'glCreateShaderProgramv',
    'glDeleteProgramPipelines', 'glDispatchComputeIndirect',
    'glDrawArraysIndirect', 'glDrawArraysInstancedBaseInstance',
    'glDrawElementsBaseVertex', 'glDrawElementsIndirect',
    'glDrawElementsInstancedBaseInstance',
    'glDrawElementsInstancedBaseVertex',
    'glDrawElementsInstancedBaseVertexBaseInstance',
    'glDrawRangeElementsBaseVertex', 'glDrawTransformFeedback',
    'glDrawTransformFeedbackInstanced', 'glDrawTransformFeedbackStream',
    'glDrawTransformFeedbackStreamInstanced', 'glFramebufferParameteri',
    'glGenProgramPipelines', 'glGetActiveUniformName',
    'glGetActiveUniformsiv', 'glGetFragDataIndex', 'glGetFragDataLocation',
    'glGetGraphicsResetStatus', 'glGetInternalformati64v',
    'glGetInternalformativ', 'glGetObjectLabel', 'glGetObjectPtrLabel',
    'glGetProgramBinary', 'glGetProgramInterfaceiv',
    'glGetProgramPipelineInfoLog', 'glGetProgramPipelineiv',
    'glGetProgramResourceIndex', 'glGetProgramResourceLocation',
    'glGetProgramResourceLocationIndex', 'glGetProgramResourceName',
    'glGetProgramResourceiv', 'glGetSynciv', 'glGetUniformIndices',
    'glObjectPtrLabel',
    'glGetActiveAtomicCounterBufferiv', 'glGetActiveSubroutineName',
    'glGetActiveSubroutineUniformName', 'glGetActiveSubroutineUniformiv',
    'glGetActiveUniformBlockName', 'glGetActiveUniformBlockiv',
    'glGetDebugMessageLog', 'glGetProgramStageiv', 'glGetSubroutineIndex',
    'glGetSubroutineUniformLocation', 'glGetUniformSubroutineuiv',
    'glShaderStorageBlockBinding', 'glUniformSubroutinesuiv',
    'glDebugMessageInsert', 'glPushDebugGroup', 'glPopDebugGroup',
    'glIsProgramPipeline', 'glMultiDrawArraysIndirect',
    'glMultiDrawArraysIndirectCount', 'glMultiDrawElementsBaseVertex',
    'glMultiDrawElementsIndirect', 'glMultiDrawElementsIndirectCount',
    'glPatchParameterfv', 'glProgramBinary', 'glProgramParameteri',
    'glShaderBinary', 'glSpecializeShader', 'glUseProgramStages',
    'glValidateProgramPipeline', 'glVertexAttribBinding',
    'glVertexAttribFormat', 'glVertexAttribIFormat', 'glVertexAttribLFormat',
    'glVertexAttribLPointer', 'glVertexBindingDivisor',
    'glBindTransformFeedback', 'glDeleteTransformFeedbacks',
    'glGenTransformFeedbacks', 'glIsTransformFeedback',
    'glPauseTransformFeedback', 'glResumeTransformFeedback',
}

def emit_core(name):
    ret, decl, args, _ = signature(name)
    family, forward = CLASSIFICATION[name]
    check_forward('gl_stub_classification.py', name, forward)

    if family == 'dsa':
        body = body_dsa(name, ret, args)
    elif name in ADVANCED_EMULATION:
        body = body_call(ret, 'gldAdv%s(%s)' % (name[2:], ', '.join(args)))
    elif family in ('ceiling-noop', 'tf-bookkeeping'):
        raise SystemExit('%s is still classified as %s without an implementation'
                         % (name, family))
    elif family == 'noop-legal':
        body = body_silent(ret)
    elif family in ('direct-forward', 'indexed-state', 'multi-bind'):
        body = body_call(ret, forward)
    elif family == 'attrib-type-variant':
        body = body_attrib_variant(name, ret, args)
    elif family == 'gl14-variant':
        body = body_gl14_variant(name, ret, args)
    elif family == 'packed-type':
        body = body_packed(name, ret, args)
    elif family == 'integer-variant':
        body = body_integer_variant(name, ret, args)
    elif family in ('fp64-demote', 'uniform-widen'):
        body = body_uniform(name, ret, args, forward)
    elif family == 'program-uniform':
        body = body_program_uniform(name, ret, args, forward)
    elif family == 'robustness':
        body = body_robustness(name, ret, args)
    elif family == 'get-gap':
        body = body_get_gap(name, ret, args)
    elif family == 'is-misc':
        body = IS_MISC_BODY[name]
    else:
        raise SystemExit('unhandled family %r for %s' % (family, name))

    # Silence unused-parameter diagnostics uniformly; harmless where the
    # parameter is used as well.
    voids = ''.join(' (void)%s;' % a for a in args)
    lines = ['static %s APIENTRY _gen_%s(%s)' % (ret, name, decl), '{']
    # Declarations must lead the block (C89), so the (void) casts go after them.
    lead = []
    rest = list(body)
    while rest and _looks_like_decl(rest[0]):
        lead.append(rest.pop(0))
    lines += lead
    if voids:
        lines.append('   ' + voids)
    lines += rest
    if not rest and not lead and sentinel(ret):
        lines.append('    return %s;' % sentinel(ret))
    lines.append('}')
    return '\n'.join(lines)


DECL_START = re.compile(
    r'^\s*(static\s+)?(const\s+)?'
    r'(GLS_\w+|GL\w+|BOOL|int|float|double|unsigned\s+\w+|void)\s*\**\s*'
    r'[A-Za-z_]\w*\s*(\[|=|,|;)')


def _looks_like_decl(line):
    return bool(DECL_START.match(line)) and 'return' not in line.split('=')[0]


def resolve_alias(alias, generated):
    """PROC symbol an alias row should point at."""
    target = ALIAS_TO_CORE[alias]
    if target in generated:
        return '_gen_%s' % target
    if target in MODERN_TABLE:
        return MODERN_TABLE[target]
    return None       # .def-only: needs a generated shim, see DEF_ONLY_SHIMS


# The seven alias targets that exist only as opengl32.def exports.  Referring to
# the exported `glXxx` name directly is not an option: glad/gl.h macro-defines
# every gl* name onto an uninitialised glad_gl* function pointer.  Forward to
# the underlying _gls* implementation instead, the same way gl_modern_stubs.h
# does everywhere else.
DEF_ONLY_SHIMS = {
    'glCopyTexImage1D': '_glsCopyTexImage2D((unsigned int)target, (int)level,'
                        ' (unsigned int)internalformat, (int)x, (int)y, (int)width, 1,'
                        ' (int)border)',
    'glCopyTexImage2D': '_glsCopyTexImage2D((unsigned int)target, (int)level,'
                        ' (unsigned int)internalformat, (int)x, (int)y, (int)width,'
                        ' (int)height, (int)border)',
    'glCopyTexSubImage1D': '_glsCopyTexSubImage2D((unsigned int)target, (int)level,'
                           ' (int)xoffset, 0, (int)x, (int)y, (int)width, 1)',
    'glCopyTexSubImage2D': '_glsCopyTexSubImage2D((unsigned int)target, (int)level,'
                           ' (int)xoffset, (int)yoffset, (int)x, (int)y, (int)width,'
                           ' (int)height)',
    'glGetPointerv': '_glsGetPointerv((unsigned int)pname, (void **)params)',
    'glTexSubImage1D': '_glsTexSubImage1D((unsigned int)target, (int)level, (int)xoffset,'
                       ' (int)width, (unsigned int)format, (unsigned int)type, pixels)',
    'glTexSubImage2D': '_glsTexSubImage2D((unsigned int)target, (int)level, (int)xoffset,'
                       ' (int)yoffset, (int)width, (int)height, (unsigned int)format,'
                       ' (unsigned int)type, pixels)',
}


def emit_shim(target):
    ret, decl, args, _ = signature(target)
    call = DEF_ONLY_SHIMS[target]
    check_forward('DEF_ONLY_SHIMS', target, call)
    voids = ''.join(' (void)%s;' % a for a in args)
    return '\n'.join(['static %s APIENTRY _gen_%s(%s)' % (ret, target, decl), '{',
                      '   ' + voids] + body_call(ret, call) + ['}'])


def main():
    generated = set(MISSING_CORE)

    chunks = []
    for name in MISSING_CORE:
        chunks.append(emit_core(name))

    # Shims for the alias targets that live only in opengl32.def.
    shim_targets = sorted({ALIAS_TO_CORE[a] for a in MISSING_ALIAS
                           if ALIAS_TO_CORE[a] not in generated
                           and ALIAS_TO_CORE[a] not in MODERN_TABLE})
    for t in shim_targets:
        if t not in DEF_ONLY_SHIMS:
            raise SystemExit('alias target %s resolves nowhere and has no shim' % t)
        chunks.append(emit_shim(t))
        generated.add(t)

    # Two passes over aliases so a chain that points at a newly generated name
    # resolves regardless of the order names appear in the source.
    rows = []
    for name in MISSING_CORE:
        rows.append((name, '_gen_%s' % name))
    unresolved = []
    for alias in MISSING_ALIAS:
        sym = resolve_alias(alias, generated)
        if sym:
            rows.append((alias, sym))
        else:
            unresolved.append(alias)
    for alias in unresolved:
        target = ALIAS_TO_CORE[alias]
        if target in generated:
            rows.append((alias, '_gen_%s' % target))
        else:
            raise SystemExit('alias %s -> %s resolves nowhere' % (alias, target))

    rows.sort()
    width = max(len(n) for n, _ in rows) + 3

    out = []
    out.append('/*********************************************************************************')
    out.append('*')
    out.append('*  gl_generated_stubs.h - exactly-typed entry points for every GL 1.0-4.6 name')
    out.append('*                         the wrapper did not already resolve.')
    out.append('*')
    out.append('*  GENERATED FILE - DO NOT EDIT BY HAND.')
    out.append('*      python tools/gen_gl_stubs.py')
    out.append('*  Inputs: tools/glmap.json, tools/gl_stub_classification.py,')
    out.append('*          tools/gl_dsa_mapping.py.  See tools/glmap.README.')
    out.append('*')
    out.append('*  %d entry points: %d core names plus %d extension aliases, every one with the'
               % (len(rows), len(MISSING_CORE), len(MISSING_ALIAS)))
    out.append('*  exact parameter list the Khronos registry gives it.  Before this file existed')
    out.append('*  gldGetProcAddress_GL46 answered these names with a no-op whose argument count')
    out.append('*  was guessed from the name, which corrupts the stack on x86 __stdcall whenever')
    out.append('*  the guess is wrong.')
    out.append('*')
    out.append('*  Every generated entry forwards to translator state, D3D9 work, or an')
    out.append('*  explicit emulation layer. Spec-defined discard hints are the only silent')
    out.append('*  operations; an unimplemented classification makes generation fail.')
    out.append('*')
    out.append('*********************************************************************************/')
    out.append('')
    out.append('#ifndef GL_GENERATED_STUBS_H')
    out.append('#define GL_GENERATED_STUBS_H')
    out.append('')
    out.append('#include <windows.h>')
    out.append('#include <string.h>')
    out.append('#include <glad/gl.h>')
    out.append('#include "gl_impl.h"')
    out.append('#include "gl_state.h"')
    out.append('#include "advanced_emulation.h"')
    out.append('#include "gl_modern_stubs.h"')
    out.append('#include "gld_diag.h"')
    out.append('')
    out.append('#ifdef __cplusplus')
    out.append('extern "C" {')
    out.append('#endif')
    out.append(PROLOGUE.rstrip())
    out.append('')
    out.append('/* ===== Generated entry points ===== */')
    out.append('')
    out.append('\n\n'.join(chunks))
    out.append('')
    out.append('/* ===== Master table, scanned by gldGetProcAddress_GL46 ===== */')
    out.append('')
    out.append('static const GLD_modernProcEntry g_generatedGL[] = {')
    for n, sym in rows:
        out.append('    { %-*s (PROC)%s },' % (width, '"%s",' % n, sym))
    out.append('    /* Sentinel */')
    out.append('    { NULL, NULL }')
    out.append('};')
    out.append('')
    out.append('#ifdef __cplusplus')
    out.append('}')
    out.append('#endif')
    out.append('')
    out.append('#endif /* GL_GENERATED_STUBS_H */')
    out.append('')

    with io.open(OUT_PATH, 'w', encoding='utf8', newline='\n') as fh:
        fh.write('\n'.join(out))
    print('wrote %s (%d table rows)' % (OUT_PATH, len(rows)))


if __name__ == '__main__':
    main()
