#!/usr/bin/env python
"""Assert that every supported GL name has an exact-signature dispatch entry.

    python tools/check_gl_coverage.py

Re-parses the *shipped* sources rather than the generator's own bookkeeping -
src/gl46/gl_generated_stubs.h, src/gl46/gl_modern_stubs.h and opengl32.def - and
checks that every name in glmap.json's missing_core + missing_alias (822 of them)
appears as a table row or a .def export, so gldGetProcAddress_GL46 finds it
and unknown names resolve to NULL.

Also checks failure modes the generator itself cannot:

  * that the obsolete guessed-arity no-op implementation is absent;
  * that every generated adapter's parameter list is character-for-character the
    one glmap.json gives, which is the whole point of generating them.

This is a manual repo-local check, not a build step.  Exit status is 0 when
everything is accounted for, 1 otherwise.
"""

import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

GENERATED = os.path.join(ROOT, 'src', 'gl46', 'gl_generated_stubs.h')
MODERN = os.path.join(ROOT, 'src', 'gl46', 'gl_modern_stubs.h')
DRIVER = os.path.join(ROOT, 'src', 'gl46', 'gl46_driver.c')
DEF = os.path.join(ROOT, 'opengl32.def')
LEGACY = os.path.join(ROOT, 'src', 'gl_legacy_stubs.c')
LEGACY_RENAME = os.path.join(ROOT, 'src', 'gl_legacy_rename.h')
MESA_FORWARD = os.path.join(ROOT, 'src', 'gl_mesa_forward.c')
PROJECT = os.path.join(ROOT, 'gld9.vcxproj')


def read(path):
    with io.open(path, encoding='utf8', errors='replace') as fh:
        return fh.read()


def table_rows(text):
    return set(re.findall(r'\{\s*"(gl\w+)"\s*,\s*\(PROC\)', text))


def main():
    glmap = json.loads(read(os.path.join(HERE, 'glmap.json')))
    wanted = sorted(set(glmap['missing_core']) | set(glmap['missing_alias']))
    core = glmap['core']

    gen_text = read(GENERATED)
    generated = table_rows(gen_text)
    modern = table_rows(read(MODERN))
    exports = set(re.findall(r'^\s*(gl\w+)', read(DEF), re.M))

    failures = []

    # 1. Every unresolved name now resolves through one of the three paths that
    #    gldGetProcAddress_GL46 consults before the no-op ladder.
    unaccounted = [n for n in wanted
                   if n not in generated and n not in modern and n not in exports]
    print('names to account for : %d  (%d core + %d alias)'
          % (len(wanted), len(glmap['missing_core']), len(glmap['missing_alias'])))
    print('  in gl_generated_stubs.h : %d'
          % len([n for n in wanted if n in generated]))
    print('  in gl_modern_stubs.h    : %d'
          % len([n for n in wanted if n in modern and n not in generated]))
    print('  in opengl32.def         : %d'
          % len([n for n in wanted if n in exports
                 and n not in generated and n not in modern]))
    print('  unaccounted for         : %d' % len(unaccounted))
    if unaccounted:
        failures.append('%d GL name(s) have no exact-signature implementation: %s'
                        % (len(unaccounted), ', '.join(unaccounted[:20])))

    # 2. The generated table is actually wired into the resolution path.
    driver = read(DRIVER)
    if 'g_generatedGL[i].name' not in driver:
        failures.append('gldGetProcAddress_GL46 does not scan g_generatedGL[]')
    if 'gl46/gl_generated_stubs.h' not in driver:
        failures.append('gl46_driver.c does not include gl_generated_stubs.h')
    fallback_markers = ('_glsGetTypedNoop', '_glsGuessArgCount', '_stub_noop_')
    present_fallbacks = [m for m in fallback_markers if m in driver]
    print('guessed-arity no-op fallback     : %s'
          % ('present' if present_fallbacks else 'absent'))
    if present_fallbacks:
        failures.append('obsolete guessed-arity no-op fallback remains: %s'
                        % ', '.join(present_fallbacks))

    # 3. Every generated entry's parameter list matches glmap.json verbatim.
    sigs = dict(re.findall(r'^static\s+[\w \*]+?APIENTRY\s+_gen_(gl\w+)\(([^)]*)\)',
                           gen_text, re.M))
    mismatched = []
    for name in sorted(sigs):
        if name not in core:
            continue
        want = ', '.join(core[name]['params']) if core[name]['params'] else 'void'
        got = ' '.join(sigs[name].split())
        if got != want:
            mismatched.append((name, want, got))
    print('generated adapter signatures checked against glmap.json : %d' % len(sigs))
    print('  mismatched                                         : %d' % len(mismatched))
    for name, want, got in mismatched[:10]:
        failures.append('%s signature drifted:\n      want %s\n      got  %s'
                        % (name, want, got))

    # 5. Every public Win32 GL export is owned by the dual dispatcher and has
    # both a Mesa forwarding body and a renamed direct-D3D9 implementation.
    public_gl = set(re.findall(r'^\s*(gl\w+)', read(DEF), re.M))
    mesa_text = read(MESA_FORWARD)
    mesa_dispatch = set(re.findall(
        r'MESA_FORWARD_(?:VOID_HOOK|VOID)\(\s*(gl\w+)', mesa_text))
    mesa_dispatch |= set(re.findall(
        r'MESA_FORWARD_RET\(\s*[^,]+,\s*(gl\w+)', mesa_text))
    mesa_dispatch |= set(re.findall(r'\bAPIENTRY\s+(gl\w+)\s*\(', mesa_text))
    direct_dispatch = set(re.findall(
        r'^#define\s+(gl\w+)\s+direct_gl\w+', read(LEGACY_RENAME), re.M))
    legacy_defs = set(re.findall(
        r'\b(?:void|GLboolean|GLenum|GLuint|GLint|const\s+GLubyte\s*\*)'
        r'\s+APIENTRY\s+(gl\w+)\s*\(', read(LEGACY)))

    missing_mesa = sorted(public_gl - mesa_dispatch)
    missing_direct = sorted(public_gl - direct_dispatch)
    stale_renames = sorted(direct_dispatch ^ legacy_defs)
    print('public Win32 GL exports through dual dispatcher : %d' % len(public_gl))
    print('  missing Mesa forwarding body                  : %d' % len(missing_mesa))
    print('  missing direct-D3D9 implementation             : %d' % len(missing_direct))
    print('  stale generated rename entries                 : %d' % len(stale_renames))
    if missing_mesa:
        failures.append('public GL exports missing Mesa dispatch: %s'
                        % ', '.join(missing_mesa[:20]))
    if missing_direct:
        failures.append('public GL exports missing direct dispatch: %s'
                        % ', '.join(missing_direct[:20]))
    if stale_renames:
        failures.append('gl_legacy_rename.h is stale: %s'
                        % ', '.join(stale_renames[:20]))
    project = read(PROJECT)
    if 'src\\gl_mesa_forward.c' not in project or 'src\\gl_legacy_stubs.c' not in project:
        failures.append('gld9.vcxproj does not compile both sides of the dual dispatcher')

    print()
    if failures:
        print('FAIL')
        for f in failures:
            print('  - %s' % f)
        return 1
    print('PASS: all %d names have exact-signature dispatch; unknown names return NULL'
          % len(wanted))
    return 0


if __name__ == '__main__':
    sys.exit(main())
