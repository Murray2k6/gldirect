/*********************************************************************************
*
*  semantic_overlay.h - Per-draw D3D9 fixed-function state narration for Remix
*
*  RTX Remix reconstructs scene meaning from the D3D9 fixed-function stream a
*  game emits: lights, materials, textures, transforms, fog and geometry are
*  read by the bridge's classic pipeline path and become scene entities.  A
*  programmable GL draw translates to a shader-bound D3D9 draw whose meaning
*  Remix must infer from the translated shader instead - a far less reliable
*  classification.
*
*  This module publishes the GL fixed-function state the game really configured
*  as D3D9 fixed-function state around every draw, whether the draw runs as a
*  translated shader or as fixed function.  When the program is (conservatively)
*  FFP-equivalent the shaders are dropped entirely and the draw runs as pure
*  D3D9 fixed function - the classic path Remix classifies best.  Narration is
*  always safe: it publishes state alongside the shaders and cannot change what
*  the translated shader renders.
*
*********************************************************************************/

#ifndef GL46_SEMANTIC_OVERLAY_H
#define GL46_SEMANTIC_OVERLAY_H

#include <windows.h>
#include <d3d9.h>
#include "gl_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TRUE when the overlay should run: RTX Remix is detected, or the
 * GLDIRECT_SEMANTIC_DIAG environment variable is set to a non-zero value
 * (used by the harness so the behaviour is testable without Remix). */
BOOL gldSemanticOverlayEnabled(void);
void gldSemanticOverlayReleaseResources(void);

/* Conservative structural scan.  Returns TRUE only when both shader sources
 * look like what the D3D9 fixed-function pipeline can reproduce: no control
 * flow, no discards, no texel-level access, no shadow/cube/3D samplers, no
 * instance/vertex-ID or clip-distance inputs, and at most two samplers (the
 * GLS_D3DVertex format carries two texcoord sets).  Either source may be
 * NULL; NULL always answers FALSE.  Over-matching is safe (the draw stays
 * programmable); under-matching is what this scan is designed to avoid. */
BOOL gldDetectFFPEquivalent(const char *vsSource, const char *fsSource);

/* Publish the active viewport/camera and GL fixed-function state as D3D9 state
 * for the draw about to happen, then decide whether the program can run as
 * pure fixed function.  Viewport/camera publication is always active so any
 * downstream D3D9 wrapper can observe it; material/light narration is gated
 * by gldSemanticOverlayEnabled().
 *
 * Returns TRUE when the shaders were dropped (FFP degrade): the caller must
 * not rely on its vertex/pixel shader being bound afterwards.  Returns FALSE
 * when the draw remains programmable - the shaders are left exactly as the
 * application's last bind left them.
 *
 * When the previous draw degraded and this one does not, the translated
 * shaders are re-bound before returning.  Software-executed stages never
 * degrade: the caller only submits their assembled geometry, and the degrade
 * guard keeps the pure-narration path intact.
 */
BOOL gldApplySemanticOverlay(GLS_State *s, GLS_Program *prog);

/* FNV-1a hash over the exact bytes submitted for a draw - vertex payload,
 * index payload, primitive type, FVF, and the index format that was chosen.
 * Written as a verbose diagnostic line ("GL: draw submit ... geoHash=%08X")
 * so the harness can prove two identical draws produce byte-identical
 * submissions.  A no-op unless gldSemanticOverlayEnabled(). */
void gldLogDrawGeometry(D3DPRIMITIVETYPE primType, int primCount,
                        const void *verts, int vertCount,
                        const unsigned int *indices, int indexCount,
                        int indexFmt, const char *tag);

#ifdef __cplusplus
}
#endif

#endif /* GL46_SEMANTIC_OVERLAY_H */
