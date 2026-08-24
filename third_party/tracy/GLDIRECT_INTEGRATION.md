# Tracy dependency

GLDirect vendors the `public` client sources from Tracy v0.13.1 and builds
them through `src/tracy_client.cpp`. The upstream license is preserved in
`LICENSE`; `NEWS` is included for the vendored release history.

Integration feature switches live in `src/gld_tracy_config.h` and must remain
identical for the Tracy client and its C/C++ callers. Do not include
`TracyClient.cpp` from any other translation unit.

Upstream: https://github.com/wolfpld/tracy/tree/v0.13.1
