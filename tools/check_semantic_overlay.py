#!/usr/bin/env python3
"""
check_semantic_overlay.py - Verify the semantic-overlay harness evidence.

The harness (wgl_smoke.exe, OpenMW-style section) runs with
GLDIRECT_SEMANTIC_DIAG=1 and GLDIRECT_VERBOSE=1 and narrates every draw in
gldirect_diag.log.  This checker asserts the properties the feature exists
for:

  - the overlay armed and ran ("semantic overlay armed"),
  - a shader-carried mat4 camera was published into D3D9 transform state,
  - the active GL viewport was published alongside each narrated draw,
  - uniform-synthesized lights engaged for the shader-lit draws ("synth=1"),
  - the glLightfv variant engaged the mirrored-light path ("lights=1"),
  - the last two submitted draws produced byte-identical geometry
    (equal trailing geoHash values) - stable geometry for Remix.

Usage: python check_semantic_overlay.py [path-to-gldirect_diag.log]
Exit code 0 = pass, 1 = fail.
"""

import os
import re
import sys

GEOHASH_RE = re.compile(r"geoHash=([0-9A-Fa-f]{8})")


def find_log():
    candidates = ["gldirect_diag.log"]
    temp = os.environ.get("TEMP")
    if temp:
        candidates.append(os.path.join(temp, "gldirect_diag.log"))
    for path in candidates:
        if os.path.isfile(path):
            return path
    return None


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else find_log()
    if not path:
        print("FAIL: gldirect_diag.log not found (run wgl_smoke.exe first)")
        return 1

    with open(path, "r", errors="replace") as f:
        lines = f.read().splitlines()

    overlay_lines = [l for l in lines if "semantic overlay" in l]
    if not overlay_lines:
        print("FAIL: no semantic-overlay narration in %s" % path)
        return 1

    checks = {
        "armed": any("semantic overlay armed" in l for l in overlay_lines),
        "shader camera": any("camera=1" in l for l in overlay_lines),
        "viewport": any(re.search(r"viewport=-?\d+,-?\d+ \d+x\d+", l)
                        for l in overlay_lines),
        "synth": any("synth=1" in l for l in overlay_lines),
        "mirrored": any("lights=1" in l for l in overlay_lines),
    }

    hashes = [m.group(1) for l in lines for m in [GEOHASH_RE.search(l)]
              if m]
    checks["stable geometry"] = (
        len(hashes) >= 2 and hashes[-1].upper() == hashes[-2].upper())

    ok = True
    for name, passed in checks.items():
        print("%s %s" % ("PASS" if passed else "FAIL", name))
        ok = ok and passed

    if len(hashes) >= 2:
        print("last two geoHash: %s / %s"
              % (hashes[-2].upper(), hashes[-1].upper()))
    else:
        print("geoHash lines found: %d (need >= 2)" % len(hashes))

    print("overlay draw lines: %d" % len(overlay_lines))
    print("PASS: semantic overlay harness evidence verified" if ok
          else "FAIL: semantic overlay harness evidence missing")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
