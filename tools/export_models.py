#!/usr/bin/env python3
"""
Batch driver for export_frog.py — exports every .blend in a directory.

    tools/export_models.py <blender> <blend-dir> <out-dir>

Each <blend-dir>/foo.blend becomes <out-dir>/foo.frog. Every file is
re-exported unconditionally; blender's own output is passed straight
through so warnings from the exporter stay visible. Exits 1 if any
model failed, after having tried them all.

Driven by the "export-models" meson run_target, which is never part of
the default build:

    meson compile -C <builddir> export-models
"""

import shutil
import subprocess
import sys
from pathlib import Path

EXPORT_SCRIPT = Path(__file__).resolve().parent / "export_frog.py"


def main():
    if len(sys.argv) != 4:
        print(__doc__.strip(), file=sys.stderr)
        return 1

    blender, blend_dir, out_dir = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])

    if shutil.which(blender) is None and not Path(blender).is_file():
        print(f"export-models: blender not found: {blender}", file=sys.stderr)
        return 1
    if not blend_dir.is_dir():
        print(f"export-models: no such directory: {blend_dir}", file=sys.stderr)
        return 1

    blends = sorted(blend_dir.glob("*.blend"))
    if not blends:
        print(f"export-models: nothing to do, no .blend in {blend_dir}")
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)

    failed = []
    for blend in blends:
        out = out_dir / (blend.stem + ".frog")
        print(f"\n>>> {blend} -> {out}", flush=True)
        result = subprocess.run([
            blender, "-b", str(blend),
            "--python-exit-code", "1",
            "-P", str(EXPORT_SCRIPT),
            "--", str(out), "--summary",
        ])
        if result.returncode != 0:
            failed.append(blend.name)

    print()
    if failed:
        print(f"export-models: {len(failed)} of {len(blends)} failed: "
              f"{', '.join(failed)}", file=sys.stderr)
        return 1
    print(f"export-models: {len(blends)} model(s) exported to {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
