# GLDirect

GLDirect is a drop-in Windows `opengl32.dll` wrapper with 32-bit and 64-bit
builds. The default backend owns the OpenGL state machine and emits a real
Direct3D 9 command stream so D3D9 consumers such as RTX Remix can observe the
game's resources, state, shaders, geometry, and draws.

## Translation architecture

- OpenGL 1.x fixed-function transforms, lighting, materials, fog, texture
  stages, immediate-mode input, vertex arrays, display lists, and raster state
  are translated into D3D9 state and draws.
- GLSL vertex and fragment programs that fit Shader Model 3 are translated to
  HLSL and compiled to D3D9 vertex/pixel shaders. OpenGL clip depth, viewport
  clipping, and D3D9 half-pixel positioning are applied consistently to fixed,
  programmable, and emulated-stage draws.
- Geometry, tessellation-control, tessellation-evaluation, and other
  post-vertex work that D3D9 cannot execute run in a private, non-presenting
  Mesa worker. The resulting primitive stream is submitted as ordinary D3D9
  geometry; transform-feedback bytes are copied into the application's bound
  GL buffers.
- Compute programs run in the same private worker. UBO, SSBO, atomic-counter,
  image, sampler-object, texture, and ranged texture-buffer bindings are
  mirrored into it, and writable resources are synchronized back before later
  D3D9 work.
- Instanced/base-instance/base-vertex draws preserve `gl_VertexID` and
  `gl_InstanceID`. Named fragment outputs and multiple render targets are
  reflected into the available D3D9 render-target slots.

The bundled Mesa `mesa_gl.dll` is an execution engine only for stages D3D9
cannot represent. It never owns the game window or presents frames in the
normal backend. The old whole-renderer Mesa path remains an explicit diagnostic
mode selected with `GLDIRECT_USE_MESA=1` or `dwUseMesa=1`.

## RTX Remix

The wrapper searches the game directory first for `d3d9.dll`, so Remix remains
the D3D9 implementation that receives GLDirect's translated stream. It retains
programmable D3D9 shaders, reapplies draw state for interception, uses stable
user-memory geometry submissions, and avoids creating a second device during
teardown.

Use `Release|Win32` for a 32-bit game and `Release|x64` for a 64-bit game. RTX
Remix's runtime is 64-bit; its documented bridge layout is what allows a 32-bit
game—and therefore the Win32 GLDirect DLL—to communicate with that runtime.
Keep Remix's bridge/runtime files in their prescribed layout and place the
matching `opengl32.dll`, `gldirect.ini`, and `mesa_gl.dll` beside the game.

The integration follows the architecture described by the pinned
[dxvk-remix AGENTS.md](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/1b5b0fb786a2bef2bed8332e92280ef576e46c81/AGENTS.md):
Remix intercepts a D3D9 fixed-function/program-state stream, uses a 64-bit
runtime, and supports 32-bit games through its bridge.

## Build and verification

Build `gld9.vcxproj` with Visual Studio 2022 in `Release|Win32` and
`Release|x64`. Artifacts are written to `bin/x86/Release` and
`bin/x64/Release`.

`tools/check_gl_coverage.py` verifies that all 822 registry-derived OpenGL
1.0–4.6 core/alias procedure names have exact ABI signatures and that unknown
names cannot fall through to a guessed-arity no-op. `tools/wgl_smoke.c` covers
context creation, OpenGL 4.6 negotiation, `WGL_ARB_pixel_format` format
selection and attribute queries, fixed and programmable rendering, display
lists, oversized viewports and OpenGL depth conversion, named fragment outputs,
geometry and tessellation stages, transform feedback, instancing, compute
SSBO/image execution, ranged texture buffers, error handling, and buffer
swapping without linking to the system OpenGL library.

## Emulation contract

D3D9 limits select an implementation path; they are not exposed as OpenGL
limits. Work that fits fixed-function D3D9 or Shader Model 3 is translated
directly. Everything else must execute in the private GL 4.6 worker or a
software resource/raster path, with its resulting resources and primitive or
pixel stream submitted through D3D9. A missing fallback is a wrapper bug, not a
reason to advertise an OpenGL entry point and ignore it.

RTX Remix still needs semantic D3D9 geometry and state, so an emulated graphics
stage emits proxy geometry/state as well as any software-computed result. The
compatibility target is complete OpenGL behavior plus an interceptable D3D9
scene stream; performance may fall back to CPU execution where D3D9 cannot
express the operation.
