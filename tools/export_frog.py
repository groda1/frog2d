#!/usr/bin/env python3
"""
.frog model exporter — writes the FROGMODL binary described in src/game/DESIGN.

Run headless:

    blender -b mymodel.blend --python-exit-code 1 -P tools/export_frog.py -- \
        resources/models/mymodel.frog [--summary]

Everything written to the file is echoed to stdout prefixed with its byte
offset, so the output can be checked against the DESIGN layout (and against
a hexdump) while writing the loader. --summary suppresses only the
per-triangle position dump; everything else always prints.

Scene conventions (see DESIGN, "Blender authoring model"):
  - every MESH object in the scene is exported, flattened, sorted by name
  - every EMPTY object in the scene is an anchor
  - timeline markers name animations: a marker starts an animation that
    runs to the frame before the next marker (or scene end); exported
    keyframes are the union of authored key frames in that range
"""

import math
import struct
import sys

import bpy
from mathutils import Matrix

MAGIC = b"FROGMODL"
VERSION = 1

# blender Z-up RH -> engine Y-up RH: (x, y, z) -> (x, z, -y)
AXES = Matrix(((1, 0, 0, 0),
               (0, 0, 1, 0),
               (0, -1, 0, 0),
               (0, 0, 0, 1)))
AXES_INV = AXES.transposed()

AREA_EPS = 1e-9        # |edge cross product| below this = degenerate triangle
RIGID_EPS = 1e-4       # orthonormality tolerance for anchor matrices
KEY_MERGE_EPS = 1e-4   # key frames closer than this collapse into one

DEFAULT_BASE = (0.8, 0.8, 0.8)
DEFAULT_SPEC = (0.0, 0.0, 0.0)


class ExportError(Exception):
    pass


warnings = []


def warn(msg):
    warnings.append(msg)
    print(f"WARNING: {msg}")


def section(title):
    print()
    print(f"---- {title} ".ljust(74, "-"))


def fvec3(v):
    return f"({v[0]:8.3f} {v[1]:8.3f} {v[2]:8.3f})"


def fquat(q):
    return f"({q[0]:+7.4f} {q[1]:+7.4f} {q[2]:+7.4f} {q[3]:+7.4f})"


class Writer:
    def __init__(self, fh):
        self.fh = fh
        self.off = 0

    def tell(self):
        return self.off

    def raw(self, data):
        self.fh.write(data)
        self.off += len(data)

    def u8(self, v):
        if not 0 <= v <= 0xFF:
            raise ExportError(f"u8 out of range: {v}")
        self.raw(struct.pack("<B", v))

    def u16(self, v):
        if not 0 <= v <= 0xFFFF:
            raise ExportError(f"u16 out of range: {v}")
        self.raw(struct.pack("<H", v))

    def u32(self, v):
        if not 0 <= v <= 0xFFFFFFFF:
            raise ExportError(f"u32 out of range: {v}")
        self.raw(struct.pack("<I", v))

    def f32(self, v):
        self.raw(struct.pack("<f", v))

    def vec3(self, v):
        self.raw(struct.pack("<3f", v[0], v[1], v[2]))

    def quat(self, q):
        self.raw(struct.pack("<4f", q[0], q[1], q[2], q[3]))

    def string(self, s):
        data = s.encode("utf-8")
        if len(data) > 0xFFFF:
            raise ExportError(f"string too long: {s!r}")
        self.u16(len(data))
        self.raw(data)


def log(off, text):
    print(f"@0x{off:08x}  {text}")


# ---------------------------------------------------------------- scene scan

def set_frame(scene, frame):
    base = int(math.floor(frame))
    scene.frame_set(base, subframe=frame - base)


def action_fcurves(anim_data):
    """All fcurves feeding one AnimData, across both the slotted-action API
    (Blender 4.4+, mandatory in 5.x) and the legacy flat Action.fcurves."""
    action = anim_data.action if anim_data else None
    if action is None:
        return
    if hasattr(action, "layers"):
        slot = getattr(anim_data, "action_slot", None)
        for layer in action.layers:
            for strip in layer.strips:
                bags = getattr(strip, "channelbags", ())
                if slot is not None:
                    try:
                        bag = strip.channelbag(slot)
                        bags = (bag,) if bag else ()
                    except (TypeError, AttributeError):
                        pass
                for bag in bags:
                    yield from bag.fcurves
    elif hasattr(action, "fcurves"):
        yield from action.fcurves


def gather_key_frames(scene):
    """Authored key frame times per object name, for EVERY object in the
    scene (deformation may be driven by objects we do not export)."""
    per_object = {}
    for obj in scene.objects:
        frames = set()
        for fc in action_fcurves(obj.animation_data):
            frames.update(kp.co.x for kp in fc.keyframe_points)
        data = getattr(obj, "data", None)
        shape_keys = getattr(data, "shape_keys", None) if data else None
        if shape_keys:
            for fc in action_fcurves(shape_keys.animation_data):
                frames.update(kp.co.x for kp in fc.keyframe_points)
        if frames:
            per_object[obj.name] = sorted(frames)
    return per_object


def dedupe_frames(frames):
    out = []
    for f in sorted(frames):
        if not out or f - out[-1] > KEY_MERGE_EPS:
            out.append(f)
    return out


class Animation:
    def __init__(self, name, start, end, frames, fps):
        self.name = name
        self.start = start
        self.end = end
        self.frames = frames
        self.times = [(f - start) / fps for f in frames]


def build_animations(scene, all_key_frames, fps):
    markers = sorted(scene.timeline_markers, key=lambda m: m.frame)
    keys = dedupe_frames(all_key_frames)

    if markers:
        names = [m.name for m in markers]
        if len(set(names)) != len(names):
            raise ExportError(f"duplicate timeline marker names: {names}")
        ranges = []
        for i, m in enumerate(markers):
            end = markers[i + 1].frame - 1 if i + 1 < len(markers) else scene.frame_end
            if end < m.frame:
                warn(f'marker "{m.name}" has an empty range, clamping to one frame')
                end = m.frame
            ranges.append((m.name, float(m.frame), float(end)))
    elif keys:
        ranges = [("default", float(scene.frame_start), float(scene.frame_end))]
    else:
        # static model: synthesize the single-animation / single-keyframe
        # wrapper (DESIGN: the loader recognizes this case)
        return [Animation("static", float(scene.frame_start), float(scene.frame_start),
                          [float(scene.frame_start)], fps)]

    animations = []
    for name, start, end in ranges:
        frames = [f for f in keys if start - KEY_MERGE_EPS <= f <= end + KEY_MERGE_EPS]
        if not frames or abs(frames[0] - start) > KEY_MERGE_EPS:
            frames.insert(0, start)
        if len(frames) > 0xFFFF:
            raise ExportError(f'animation "{name}": {len(frames)} keyframes exceeds u16')
        animations.append(Animation(name, start, end, frames, fps))
    return animations


# ------------------------------------------------------------------ geometry

def extract_triangles(obj, depsgraph):
    """Evaluated triangles of one object: world transform baked, axes
    converted to engine space, winding reversed CCW -> CW.
    Returns [(material_slot_index, v0, v1, v2), ...] in loop-triangle order."""
    ev = obj.evaluated_get(depsgraph)
    mesh = ev.to_mesh()
    try:
        if hasattr(mesh, "calc_loop_triangles"):
            mesh.calc_loop_triangles()
        to_engine = AXES @ ev.matrix_world
        verts = [to_engine @ v.co for v in mesh.vertices]
        tris = []
        for lt in mesh.loop_triangles:
            i0, i1, i2 = lt.vertices
            tris.append((lt.material_index, verts[i0], verts[i2], verts[i1]))
    finally:
        ev.to_mesh_clear()
    return tris


def triangle_area2(tri):
    _, v0, v1, v2 = tri
    return (v1 - v0).cross(v2 - v0).length


def material_colors(mat):
    if mat is None:
        return DEFAULT_BASE, DEFAULT_SPEC, "no material -> synthesized default"
    base = tuple(getattr(mat, "diffuse_color", (*DEFAULT_BASE, 1.0))[:3])
    base_src = "viewport diffuse"
    principled = None
    tree = getattr(mat, "node_tree", None)
    if tree:
        for node in tree.nodes:
            if node.type == "BSDF_PRINCIPLED":
                principled = node
                break
    if principled:
        base = tuple(principled.inputs["Base Color"].default_value[:3])
        base_src = "Principled Base Color"
    spec = getattr(mat, "specular_color", None)
    if spec is not None:
        spec, spec_src = tuple(spec[:3]), "viewport specular"
    elif principled and "Specular Tint" in principled.inputs:
        spec = tuple(principled.inputs["Specular Tint"].default_value[:3])
        spec_src = "Principled Specular Tint"
    else:
        spec, spec_src = DEFAULT_SPEC, "default"
    return base, spec, f"base: {base_src}, spec: {spec_src}"


def resolve_material(obj, slot_index):
    if 0 <= slot_index < len(obj.material_slots):
        return obj.material_slots[slot_index].material
    return None


def anchor_transform(empty, depsgraph):
    """Engine-space rigid transform of one anchor empty at the current
    frame. Fails the export if the matrix is not rigid."""
    ev = empty.evaluated_get(depsgraph)
    m = AXES @ ev.matrix_world @ AXES_INV
    r = m.to_3x3()
    cols = [r.col[0], r.col[1], r.col[2]]
    for i, c in enumerate(cols):
        if abs(c.length - 1.0) > RIGID_EPS:
            raise ExportError(
                f'anchor "{empty.name}": matrix not rigid (axis {i} length '
                f"{c.length:.6f}) — non-uniform scale or shear in its parent chain")
    for a, b in ((0, 1), (0, 2), (1, 2)):
        if abs(cols[a].dot(cols[b])) > RIGID_EPS:
            raise ExportError(
                f'anchor "{empty.name}": matrix not rigid (axes {a},{b} not '
                f"orthogonal) — non-uniform scale or shear in its parent chain")
    pos = m.to_translation()
    q = m.to_quaternion().normalized()
    return pos, (q.x, q.y, q.z, q.w)


# ---------------------------------------------------------------------- main

class MeshEntry:
    def __init__(self, obj):
        self.obj = obj
        self.ref_tri_count = 0   # loop-triangle count, pre-drop (the invariant)
        self.keep = []           # kept loop-triangle indices, fixed for all frames
        self.palette_indices = []  # per kept triangle
        self.global_start = 0


def parse_args():
    argv = sys.argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else []
    out_path = None
    summary = False
    for arg in argv:
        if arg == "--summary":
            summary = True
        elif out_path is None:
            out_path = arg
        else:
            raise ExportError(f"unexpected argument: {arg}")
    if out_path is None:
        blend = bpy.data.filepath
        out_path = (blend.rsplit(".", 1)[0] + ".frog") if blend else "model.frog"
    return out_path, summary


def export():
    out_path, summary = parse_args()
    scene = bpy.context.scene
    fps = scene.render.fps / scene.render.fps_base

    print("=" * 74)
    print(f" .frog export  (FROGMODL v{VERSION})")
    print(f" blend : {bpy.data.filepath or '(unsaved)'}")
    print(f" out   : {out_path}")
    print(f" scene : frames {scene.frame_start}..{scene.frame_end} @ {fps:.2f} fps")
    print(" axes  : engine = (x, z, -y) of blender; winding reversed CCW -> CW")
    print("=" * 74)

    meshes = sorted((o for o in scene.objects if o.type == "MESH"), key=lambda o: o.name)
    empties = sorted((o for o in scene.objects if o.type == "EMPTY"), key=lambda o: o.name)
    if not meshes:
        raise ExportError("no mesh objects in the scene")

    section("scene scan")
    print(f"mesh objects ({len(meshes)}, sorted by name — this is the file order):")
    for obj in meshes:
        parent = f'"{obj.parent.name}"' if obj.parent else "-"
        print(f'  "{obj.name}"  parent {parent}')
    print(f"anchor empties ({len(empties)}, sorted by name):")
    for empty in empties:
        parent = f'"{empty.parent.name}"' if empty.parent else "-"
        print(f'  "{empty.name}"  parent {parent}')
    if not empties:
        print("  (none)")

    key_frames = gather_key_frames(scene)
    print("authored key frames (all scene objects):")
    for name, frames in sorted(key_frames.items()):
        shown = ", ".join(f"{f:g}" for f in frames)
        print(f'  "{name}": [{shown}]')
    if not key_frames:
        print("  (none — static model)")

    all_keys = [f for frames in key_frames.values() for f in frames]
    animations = build_animations(scene, all_keys, fps)
    if len(animations) > 0xFFFF:
        raise ExportError("animation count exceeds u16")

    print(f"animations ({len(animations)}):")
    for anim in animations:
        shown = ", ".join(f"{f:g}" for f in anim.frames)
        print(f'  "{anim.name}"  frames {anim.start:g}..{anim.end:g}  '
              f"{len(anim.frames)} keyframes at [{shown}]")
    if len(animations) == 1 and len(animations[0].frames) == 1:
        print("  -> single animation, single keyframe: loader treats this as a static model")

    # -- reference pass: topology, materials, degenerate triangles ----------
    ref_frame = animations[0].frames[0]
    section(f"reference pass (frame {ref_frame:g}): topology + materials")
    set_frame(scene, ref_frame)
    depsgraph = bpy.context.evaluated_depsgraph_get()

    palette = {}       # name -> [index, base, spec, source]
    palette_order = []
    entries = []
    global_tri = 0
    for obj in meshes:
        entry = MeshEntry(obj)
        tris = extract_triangles(obj, depsgraph)
        entry.ref_tri_count = len(tris)
        dropped = []
        for i, tri in enumerate(tris):
            if triangle_area2(tri) < AREA_EPS:
                dropped.append(i)
                continue
            entry.keep.append(i)
            mat = resolve_material(obj, tri[0])
            name = mat.name if mat else "default"
            if name not in palette:
                base, spec, source = material_colors(mat)
                palette[name] = [len(palette_order), base, spec, source]
                palette_order.append(name)
            entry.palette_indices.append(palette[name][0])
        if dropped:
            warn(f'"{obj.name}": dropped {len(dropped)} degenerate triangle(s) '
                 f"(loop-triangle indices {dropped})")
        if not entry.keep:
            warn(f'"{obj.name}": no triangles, object skipped entirely')
            continue
        entry.global_start = global_tri
        global_tri += len(entry.keep)
        entries.append(entry)
        print(f'  "{obj.name}": {len(entry.keep)} tris '
              f"-> global tris {entry.global_start}..{global_tri - 1}")

    triangle_count = global_tri
    if triangle_count == 0:
        raise ExportError("no non-degenerate triangles to export")
    if len(palette_order) > 256:
        raise ExportError(f"{len(palette_order)} materials exceeds the u8 palette index")
    print(f"  total: {triangle_count} triangles, {triangle_count * 3} vertices, "
          f"{len(palette_order)} materials")

    # -- write ---------------------------------------------------------------
    with open(out_path, "wb") as fh:
        w = Writer(fh)

        section("header")
        log(w.tell(), f'magic            u8[8]  "{MAGIC.decode()}"')
        w.raw(MAGIC)
        log(w.tell(), f"version          u16    {VERSION}")
        w.u16(VERSION)
        log(w.tell(), f"triangle_count   u32    {triangle_count}")
        w.u32(triangle_count)
        log(w.tell(), f"material_count   u16    {len(palette_order)}")
        w.u16(len(palette_order))
        log(w.tell(), f"anchor_count     u16    {len(empties)}")
        w.u16(len(empties))
        log(w.tell(), f"animation_count  u16    {len(animations)}")
        w.u16(len(animations))

        section("materials")
        for name in palette_order:
            index, base, spec, source = palette[name]
            log(w.tell(), f'[{index}] "{name}"  base {fvec3(base)}  spec {fvec3(spec)}'
                          f"  ({source})")
            w.string(name)
            w.vec3(base)
            w.vec3(spec)

        section("triangle materials")
        flat = [pi for entry in entries for pi in entry.palette_indices]
        run_start = 0
        run_off = w.tell()
        for i, pi in enumerate(flat):
            w.u8(pi)
            last = i + 1 == len(flat)
            if last or flat[i + 1] != pi:
                log(run_off, f"tri {run_start:5d}..{i:5d}  -> "
                             f'[{pi}] "{palette_order[pi]}"')
                run_start = i + 1
                run_off = w.tell()

        section("anchor names")
        if not empties:
            print("  (none)")
        for empty in empties:
            log(w.tell(), f'"{empty.name}"')
            w.string(empty.name)

        for anim in animations:
            keyframe_bytes = 4 + triangle_count * 36 + len(empties) * 28
            section(f'animation "{anim.name}"  ({len(anim.frames)} keyframes, '
                    f"{keyframe_bytes} bytes each)")
            log(w.tell(), f'name             "{anim.name}"')
            w.string(anim.name)
            log(w.tell(), f"keyframe_count   u16    {len(anim.frames)}")
            w.u16(len(anim.frames))

            prev_quats = [None] * len(empties)
            for ki, frame in enumerate(anim.frames):
                time = anim.times[ki]
                set_frame(scene, frame)
                depsgraph = bpy.context.evaluated_depsgraph_get()

                print(f"  -- keyframe {ki}  time {time:.4f}s  (scene frame {frame:g})")
                log(w.tell(), f"time             f32    {time:.4f}")
                w.f32(time)

                degenerate_here = 0
                for entry in entries:
                    tris = extract_triangles(entry.obj, depsgraph)
                    if len(tris) != entry.ref_tri_count:
                        raise ExportError(
                            f'"{entry.obj.name}" at frame {frame:g}: {len(tris)} '
                            f"triangles vs {entry.ref_tri_count} at the reference "
                            f"frame — topology must not change between keyframes")
                    for local, tri_index in enumerate(entry.keep):
                        tri = tris[tri_index]
                        if triangle_area2(tri) < AREA_EPS:
                            degenerate_here += 1
                        if not summary:
                            log(w.tell(), f"tri {entry.global_start + local:5d}  "
                                          f"{fvec3(tri[1])} {fvec3(tri[2])} {fvec3(tri[3])}")
                        w.vec3(tri[1])
                        w.vec3(tri[2])
                        w.vec3(tri[3])
                if summary:
                    print(f"             positions of {triangle_count} tris "
                          "(suppressed by --summary)")
                if degenerate_here:
                    warn(f'"{anim.name}" keyframe {ki}: {degenerate_here} kept '
                         "triangle(s) are degenerate at this pose — the loader's "
                         "normal derivation must guard against zero area here")

                for ai, empty in enumerate(empties):
                    pos, quat = anchor_transform(empty, depsgraph)
                    if prev_quats[ai] is not None:
                        dot = sum(a * b for a, b in zip(quat, prev_quats[ai]))
                        if dot < 0.0:
                            quat = tuple(-c for c in quat)
                    prev_quats[ai] = quat
                    log(w.tell(), f'anchor "{empty.name}"  pos {fvec3(pos)}  '
                                  f"quat {fquat(quat)}")
                    w.vec3(pos)
                    w.quat(quat)

        total = w.tell()

    section("summary")
    print(f"wrote {out_path}: {total} bytes")
    print(f"  {triangle_count} tris / {triangle_count * 3} verts, "
          f"{len(palette_order)} materials, {len(empties)} anchors, "
          f"{len(animations)} animations, "
          f"{sum(len(a.frames) for a in animations)} keyframes total")
    if warnings:
        print(f"  {len(warnings)} warning(s):")
        for msg in warnings:
            print(f"    - {msg}")
    else:
        print("  no warnings")


def main():
    try:
        export()
    except ExportError as err:
        print(f"\nEXPORT FAILED: {err}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
