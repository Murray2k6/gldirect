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

## Tracy profiling

The Release wrapper embeds the [Tracy 0.13.1 client](https://github.com/wolfpld/tracy/releases/tag/v0.13.1).
It starts on the first WGL call, outside the Windows loader lock, and runs in
on-demand mode: when no Tracy viewer is connected, frame history is not
accumulated. The listener is restricted to localhost, sampling and source-code
transfer are disabled, and the wrapper is process-pinned after profiling starts
so a late `FreeLibrary` cannot unmap Tracy code beneath its worker thread.

Run the game, open the matching Tracy 0.13.1 viewer on the same machine, and
connect to the target named:

```text
GLDirect/<game-folder>/<executable>/<x86|x64>
```

The session metadata includes the full game and wrapper paths, process ID,
architecture, and profiling mode. Captures contain GLDirect frame marks and
zones for WGL context creation/binding/presentation, D3D9 device creation or
reset, shader linking, and indexed/non-indexed draws. One-time wrapper fault
flags also appear as colored Tracy messages.

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

The ordinary smoke run validates GLDirect against the system D3D9 runtime. A
real Remix check must run the matching Win32 harness from a 32-bit game's Remix
directory so its local `d3d9.dll` bridge is loaded:

```text
wgl_smoke86.exe C:\absolute\path\to\bin\x86\Release\opengl32.dll --remix
```

That mode compiles FF4-style compatibility GLSL, checks the projection and
model-view stacks independently, submits native DX9 shaders and geometry, and
requires a fresh Remix server log to confirm a usable camera/ray-tracing pass.
After teardown it also requires both bridge logs to report clean shutdown and
the wrapper device reference count to reach zero.

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
