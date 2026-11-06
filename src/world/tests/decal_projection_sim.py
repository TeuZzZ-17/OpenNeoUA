#!/usr/bin/env python3
"""Pixel-accurate simulation of the depth-projected decal shader (system/decalshader.h).

Usage: decal_projection_sim.py output.png Graphics/GroundDecals/Impact_01.png
The exit code is not zero when the final mapping stretches or shrinks the decal.
map_final() must stay a line by line copy of the GLSL mapping.

Every scene is ray traced into a 24 bit depth buffer. The decal step then only
uses that depth buffer, exactly like the GLSL shader: nothing below reads the
scene's triangles except the metrics, which compare against the ground truth.

Stretch  = worst magnification of the texture on the surface (1 = perfect,
           huge = the streaks of the old decals).
Compress = worst shrinking of the texture on the surface.
Seam     = biggest jump of the decal between touching pixels of two different
           polygons, as a fraction of the decal size.
"""
import math
import sys

import numpy as np
from PIL import Image, ImageDraw

W, H = 420, 315
NEAR, FAR = 1.0, 16000.0
A = (FAR + NEAR) / (FAR - NEAR)
B = -2.0 * FAR * NEAR / (FAR - NEAR)
FOCAL = 1.0
SAME_SURFACE_MIN = 8.0
SAME_SURFACE_MAX = 40.0
SAME_SURFACE_RATIO = 0.25


def normalize(v):
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-12)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


class Scene:
    def __init__(self, name, c, n, eye, target, size=240.0):
        self.name, self.size = name, size
        self.c = np.array(c, float)
        self.n = normalize(np.array(n, float))
        self.eye, self.target = np.array(eye, float), np.array(target, float)
        self.tris = []
        self.ids = []
        self._next_id = 0

    def quad(self, a, b, c, d):
        """One planar polygon: its two triangles share the same id."""
        pid = self._next_id
        self._next_id += 1
        self.tris += [(a, b, c), (a, c, d)]
        self.ids += [pid, pid]

    def box(self, lo, hi):
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        self.quad((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))
        self.quad((x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0))
        self.quad((x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1))
        self.quad((x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0))
        self.quad((x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0))
        self.quad((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1))

    def ground(self, y=0.0, s=2500.0):
        self.quad((-s, y, -s), (-s, y, s), (s, y, s), (s, y, -s))


def trace(scene):
    """Rasterize into a depth buffer. Returns depth, polygon id, true normal."""
    forward = normalize(scene.target - scene.eye)
    right = normalize(np.cross(forward, [0.0, 1.0, 0.0]))
    up = np.cross(right, forward)
    aspect = W / H
    sx = (np.arange(W) + 0.5) / W * 2 - 1
    sy = (np.arange(H) + 0.5) / H * 2 - 1
    sx, sy = np.meshgrid(sx, sy)
    # The engine's projection uses x/z and -y/z with no aspect correction; the
    # sim keeps square pixels because only the view space positions matter.
    rays = (sx * aspect)[..., None] * right + (sy)[..., None] * up + forward * FOCAL
    tris = np.array(scene.tris, float)
    best = np.full((H, W), np.inf)
    pid = np.full((H, W), -1)
    normal = np.zeros((H, W, 3))
    for tri, poly in zip(tris, scene.ids):
        e1, e2 = tri[1] - tri[0], tri[2] - tri[0]
        pv = np.cross(rays, e2)
        det = pv @ e1
        ok = np.abs(det) > 1e-9
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        tv = scene.eye - tri[0]
        u = (pv @ tv) * inv
        qv = np.cross(tv, e1)
        v = (rays @ qv) * inv
        t = (e2 @ qv) * inv
        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > NEAR) & (t < best)
        best[hit] = t[hit]
        pid[hit] = poly
        fn = normalize(np.cross(e1, e2))
        normal[hit] = fn
    ndc = np.where(np.isfinite(best), A + B / np.where(np.isfinite(best), best, 1.0), 1.0)
    depth = np.round((ndc * 0.5 + 0.5) * (2 ** 24 - 1)) / (2 ** 24 - 1)   # 24 bit Z
    return depth, pid, normal, rays, forward, right, up


def reconstruct(scene, depth, rays):
    """View position from depth only, then normals with the 4 neighbour trick."""
    ndc = depth * 2.0 - 1.0
    z = B / np.minimum(ndc - A, -1e-9)
    p = scene.eye + rays * z[..., None]
    p[depth >= 1.0] = np.nan
    pad = np.pad(p, ((1, 1), (1, 1), (0, 0)), mode="edge")
    dl, dr = pad[1:-1, 1:-1] - pad[1:-1, :-2], pad[1:-1, 2:] - pad[1:-1, 1:-1]
    dd, du = pad[1:-1, 1:-1] - pad[:-2, 1:-1], pad[2:, 1:-1] - pad[1:-1, 1:-1]
    # Pick on each axis the neighbour lying on the same surface as the pixel:
    # second difference of depth tells which side is planar.
    zpad = np.pad(1.0 / z, ((2, 2), (2, 2)), mode="edge")
    ex_l = np.abs(2 * zpad[2:-2, 1:-3] - zpad[2:-2, :-4] - zpad[2:-2, 2:-2])
    ex_r = np.abs(2 * zpad[2:-2, 3:-1] - zpad[2:-2, 4:] - zpad[2:-2, 2:-2])
    ex_d = np.abs(2 * zpad[1:-3, 2:-2] - zpad[:-4, 2:-2] - zpad[2:-2, 2:-2])
    ex_u = np.abs(2 * zpad[3:-1, 2:-2] - zpad[4:, 2:-2] - zpad[2:-2, 2:-2])
    dx = np.where((ex_l < ex_r)[..., None], dl, dr)
    dy = np.where((ex_d < ex_u)[..., None], dd, du)
    raw = np.cross(dx, dy)
    n = normalize(raw)
    view = normalize(scene.eye - p)
    flip = np.sum(n * view, axis=-1, keepdims=True) < 0
    n = np.where(flip, -n, n)
    degenerate = np.linalg.norm(raw, axis=-1, keepdims=True) < 1e-9
    n = np.where(degenerate, scene.n, n)
    return p, n


def frame(scene):
    n = scene.n
    t = np.array([1.0, 0.0, 0.0])
    if abs(n @ t) > 0.9:
        t = np.array([0.0, 0.0, 1.0])
    t = normalize(t - n * (t @ n))
    return t, np.cross(n, t), n


def map_old(scene, p, n):
    """Current engine: world X/Z projection, whatever the surface."""
    d = p - scene.c
    return d[..., 0], d[..., 2], np.ones(p.shape[:2])


def map_box(scene, p, n):
    """Plain box projection along the decal normal."""
    t, b, nn = frame(scene)
    d = p - scene.c
    return d @ t, d @ b, np.ones(p.shape[:2])


def map_fold(scene, p, n):
    """Rotate each surface flat around the line where it meets the decal plane."""
    t, b, nn = frame(scene)
    d = p - scene.c
    dn = d @ nn
    c = np.clip(n @ nn, -0.95, 1.0)
    nt = n - nn * c[..., None]
    flat = d - nn * dn[..., None] - nt * (dn / (1 + c))[..., None]
    return flat @ t, flat @ b, smoothstep(-0.3, -0.1, c)


def map_final(scene, p, n):
    """Fold, plus the two cases a fold alone cannot know about.

    - Flat surface at another height (top of a step): continue the distance
      already walked on the step's side, so the decal does not restart.
    - Steep surface reached only from behind (far side of a wall): hidden.
    """
    t, b, nn = frame(scene)
    reach = scene.size * 0.5
    tol = min(max(SAME_SURFACE_MIN, reach * SAME_SURFACE_RATIO), SAME_SURFACE_MAX)
    d = p - scene.c
    dn = d @ nn
    pt = d - nn * dn[..., None]
    r = np.linalg.norm(pt, axis=-1)
    radial = pt / np.maximum(r, 1e-4)[..., None]
    c = np.clip(n @ nn, -0.95, 1.0)
    nt = n - nn * c[..., None]
    flat = pt - nt * (dn / (1 + c))[..., None]
    level = smoothstep(0.906, 0.985, c)
    flat = flat + radial * (level * np.minimum(r, np.maximum(np.abs(dn) - tol, 0.0)))[..., None]
    steep = smoothstep(0.35, 0.60, np.linalg.norm(nt, axis=-1))
    away = np.sum(normalize(nt) * radial, axis=-1) * np.sign(dn)
    away = away * smoothstep(0.0, tol, np.abs(dn))
    mask = smoothstep(-0.3, -0.1, c) * (1 - steep * smoothstep(0.02, 0.20, away))
    band = max(reach * 0.6, tol * 1.5)
    mask = mask * (1 - level * smoothstep(band * 0.6, band, np.abs(dn)))
    return flat @ t, flat @ b, mask


def decal_alpha(scene, u, v, texture=None, checker=False):
    """Decal coverage and colour for the mapped coordinates."""
    size = scene.size
    uu, vv = u / size + 0.5, v / size + 0.5
    inside = (uu >= 0) & (uu <= 1) & (vv >= 0) & (vv <= 1)
    rn = np.sqrt((uu - 0.5) ** 2 + (vv - 0.5) ** 2) * 2
    fade = 1 - smoothstep(0.45, 1.0, rn)
    if checker:
        cells = (np.floor(uu * 8) + np.floor(vv * 8)) % 2
        rgb = np.where(cells[..., None] > 0, [0.93, 0.15, 0.1], [0.98, 0.9, 0.2])
        a = np.where(rn <= 1, 0.92, 0.0)
    else:
        h, w = texture.shape[:2]
        ix = np.clip((np.nan_to_num(uu) * w).astype(int), 0, w - 1)
        iy = np.clip((np.nan_to_num(vv) * h).astype(int), 0, h - 1)
        px = texture[iy, ix]
        rgb, a = px[..., :3], px[..., 3] * fade
    return rgb, np.where(inside, a, 0.0)


def ghost_decal(scene, rays, base, tex):
    """Old behaviour: the decal mesh lies on the collision plane, drawn in air."""
    den = rays @ scene.n
    t = ((scene.c - scene.eye) @ scene.n) / np.where(np.abs(den) > 1e-9, den, 1e-9)
    hit = scene.eye + rays * t[..., None]
    d = hit - scene.c
    rgb, a = decal_alpha(scene, d[..., 0], d[..., 2], tex)
    return base * (1 - a[..., None]) + rgb * a[..., None]


def shade(depth, true_n, pid):
    light = normalize(np.array([0.35, 0.8, 0.5]))
    tone = 0.42 + 0.36 * np.clip(true_n @ light, 0, 1)
    tone = tone * (0.9 + 0.1 * ((pid % 3) / 2.0))
    img = np.repeat(tone[..., None], 3, axis=2)
    img[depth >= 1.0] = [0.55, 0.65, 0.78]
    return img


def metrics(scene, p, pid, u, v, mask):
    """Stretch/compression/seams measured only where the decal is really drawn."""
    size = scene.size
    uu, vv = u / size, v / size
    live = np.nan_to_num(mask) > 0.05
    live &= (np.nan_to_num(np.abs(uu)) < 0.5) & (np.nan_to_num(np.abs(vv)) < 0.5)
    live &= np.nan_to_num(np.sqrt(uu ** 2 + vv ** 2)) < 0.5
    same_x = (pid[:, :-1] == pid[:, 1:])[:-1, :]
    same_y = (pid[:-1, :] == pid[1:, :])[:, :-1]
    good = same_x & same_y & live[:-1, :-1]
    px = p[:-1, 1:] - p[:-1, :-1]
    py = p[1:, :-1] - p[:-1, :-1]
    ux = np.stack([(u[:-1, 1:] - u[:-1, :-1]), (v[:-1, 1:] - v[:-1, :-1])], -1)
    uy = np.stack([(u[1:, :-1] - u[:-1, :-1]), (v[1:, :-1] - v[:-1, :-1])], -1)
    px, py, ux, uy = px[:, :, :], py, ux, uy
    gx, gy = px[good], py[good]
    dx, dy = ux[good], uy[good]
    stretch, compress = 1.0, 1.0
    if len(gx):
        lx = np.linalg.norm(gx, axis=1)
        e1 = gx / lx[:, None]
        e2 = normalize(gy - e1 * np.sum(gy * e1, axis=1, keepdims=True))
        ax = np.sum(gy * e1, axis=1)
        ay = np.sum(gy * e2, axis=1)
        keep = np.abs(ay) > 1e-6
        J1 = dx[keep] / lx[keep][:, None]
        J2 = (dy[keep] - J1 * ax[keep][:, None]) / ay[keep][:, None]
        J = np.stack([J1, J2], axis=2)
        s = np.linalg.svd(J, compute_uv=False)
        s = s[np.all(np.isfinite(s), axis=1)]
        if len(s):
            stretch = float(np.percentile(1.0 / np.maximum(s[:, 1], 1e-3), 99))
            compress = float(np.percentile(s[:, 0], 99))
    # Seams: neighbouring pixels on two polygons that physically touch.
    seam = 0.0
    rho = np.sqrt(uu ** 2 + vv ** 2)
    edge = 0.0
    for axis in (0, 1):
        a, b = [slice(None)] * 2, [slice(None)] * 2
        a[axis], b[axis] = slice(None, -1), slice(1, None)
        a, b = tuple(a), tuple(b)
        cross = (pid[a] != pid[b]) & (pid[a] >= 0) & (pid[b] >= 0) & (live[a] | live[b])
        gap = np.linalg.norm(p[a] - p[b], axis=-1)
        pixel = np.linalg.norm(p[a] - scene.eye, axis=-1) * 2.0 / H
        touching = cross & (gap < 4 * pixel)
        if np.any(touching):
            jump = np.hypot(uu[a] - uu[b], vv[a] - vv[b])[touching]
            seam = max(seam, float(np.percentile(jump, 99)))
            edge = max(edge, float(np.percentile(np.abs(rho[a] - rho[b])[touching], 99)))
    return stretch, compress, seam, edge


def build_scenes():
    sc = []

    s = Scene("piatto", (0, 0, 0), (0, 1, 0), (-170, 190, -220), (0, 0, 0)); s.ground(); sc.append(s)

    s = Scene("muro a 50", (0, 0, 0), (0, 1, 0), (-230, 180, -230), (30, 45, 0))
    s.ground(); s.box((50, 0, -450), (160, 320, 450)); sc.append(s)

    s = Scene("muro attaccato", (0, 0, 0), (0, 1, 0), (-260, 190, -210), (0, 55, 0))
    s.ground(); s.box((3, 0, -450), (160, 320, 450)); sc.append(s)

    s = Scene("colpo sul muro", (50, 80, 0), (-1, 0, 0), (-260, 170, -210), (35, 60, 0))
    s.ground(); s.box((50, 0, -450), (160, 320, 450)); sc.append(s)

    s = Scene("angolo interno", (0, 0, 0), (0, 1, 0), (-230, 210, -230), (25, 40, 25))
    s.ground(); s.box((50, 0, -80), (170, 320, 450)); s.box((-450, 0, 60), (170, 320, 170)); sc.append(s)

    s = Scene("muro obliquo", (0, 0, 0), (0, 1, 0), (-240, 190, -160), (30, 40, 0))
    s.ground()
    co, si = math.cos(math.radians(38)), math.sin(math.radians(38))
    pts = [(55 + co * a - si * b, 55 * 0 + si * a + co * b) for a, b in ((0, -500), (0, 500), (200, 500), (200, -500))]
    for i in range(4):
        (ax, az), (bx, bz) = pts[i], pts[(i + 1) % 4]
        s.quad((ax, 0, az), (bx, 0, bz), (bx, 320, bz), (ax, 320, az))
    sc.append(s)

    s = Scene("rampa 30", (0, 0, 0), (0, 1, 0), (-200, 190, -240), (40, 20, 0))
    s.quad((-2500, 0, -2500), (-2500, 0, 2500), (30, 0, 2500), (30, 0, -2500))
    x1, y1 = 30 + 400 * math.cos(math.radians(30)), 400 * math.sin(math.radians(30))
    s.quad((30, 0, -2500), (30, 0, 2500), (x1, y1, 2500), (x1, y1, -2500)); sc.append(s)

    s = Scene("colpo su pendio", (0, 0, 0), normalize(np.array([-math.sin(math.radians(35)), math.cos(math.radians(35)), 0])),
              (-170, 260, -230), (20, 25, 0))
    x0, y0 = -130 * math.cos(math.radians(35)), -130 * math.sin(math.radians(35))
    x1, y1 = 400 * math.cos(math.radians(35)), 400 * math.sin(math.radians(35))
    s.quad((-2500, y0, -2500), (-2500, y0, 2500), (x0, y0, 2500), (x0, y0, -2500))
    s.quad((x0, y0, -2500), (x0, y0, 2500), (x1, y1, 2500), (x1, y1, -2500)); sc.append(s)

    s = Scene("gradino", (0, 0, 0), (0, 1, 0), (-190, 200, -220), (30, 8, 0))
    s.ground(); s.box((42, 0, -900), (900, 28, 900)); sc.append(s)

    s = Scene("scala", (0, 0, 0), (0, 1, 0), (-190, 210, -220), (45, 20, 0))
    s.ground()
    for i in range(7):
        s.box((36 + i * 26, 0, -900), (900, 13 * (i + 1), 900))
    sc.append(s)

    s = Scene("bordo tetto", (0, 150, 0), (0, 1, 0), (240, 330, -240), (25, 120, 0))
    s.ground(); s.box((-450, 0, -450), (40, 150, 450)); sc.append(s)

    s = Scene("cassa vicina", (0, 0, 0), (0, 1, 0), (-200, 190, -210), (25, 20, 0))
    s.ground(); s.box((36, 0, -40), (116, 62, 40)); sc.append(s)

    s = Scene("colonna", (0, 0, 0), (0, 1, 0), (-230, 170, -190), (30, 40, 0))
    s.ground()
    ring = [(92 + 44 * math.cos(i * math.pi / 8), 44 * math.sin(i * math.pi / 8)) for i in range(16)]
    for i in range(16):
        (ax, az), (bx, bz) = ring[i], ring[(i + 1) % 16]
        s.quad((ax, 0, az), (bx, 0, bz), (bx, 320, bz), (ax, 320, az))
    sc.append(s)

    s = Scene("muro sottile: retro", (0, 0, 0), (0, 1, 0), (320, 170, -190), (70, 35, 0))
    s.ground(); s.box((40, 0, -450), (52, 120, 450)); sc.append(s)
    s = Scene("spigolo esterno", (0, 0, 0), (0, 1, 0), (-250, 200, 230), (20, 30, 0))
    s.ground(); s.box((30, 0, 25), (400, 320, 500)); sc.append(s)

    s = Scene("colpo 10 sopra terra", (0, 10, 0), (0, 1, 0), (-170, 190, -220), (0, 0, 0)); s.ground(); sc.append(s)

    s = Scene("colpo 15 dentro muro", (65, 90, 0), (-1, 0, 0), (-200, 150, -170), (40, 70, 0))
    s.ground(); s.box((50, 0, -450), (160, 320, 450)); sc.append(s)

    s = Scene("proiettile su muro", (50, 80, 0), (-1, 0, 0), (-12, 92, -40), (50, 80, 0), size=6.0)
    s.ground(); s.box((50, 0, -450), (160, 320, 450)); sc.append(s)

    s = Scene("grande presso palazzo", (0, 0, 0), (0, 1, 0), (-420, 320, -420), (70, 70, 0), size=500.0)
    s.ground(); s.box((80, 0, -120), (320, 420, 120)); sc.append(s)
    tilt = normalize(np.array([-0.42, 0.9, 0.2]))
    s = Scene("collisione fantasma", (20, 95, 0), tilt, (-300, 240, -260), (30, 50, 0))
    s.ground(); s.box((60, 0, -70), (190, 70, 70)); sc.append(s)
    s.ghost = True
    return sc


METHODS = (("vecchio X/Z", map_old), ("proiezione", map_box), ("piega", map_fold), ("finale", map_final))


def main(out_png, tex_path):
    tex = np.asarray(Image.open(tex_path).convert("RGBA"), float) / 255.0
    scenes = build_scenes()
    rows = []
    header = f"{'scena':22s}" + "".join(f"| {m[0]:^26s}" for m in METHODS)
    print(header)
    print(f"{'':22s}" + "| stretch comprime  bordo    " * len(METHODS))
    columns = 1 + len(METHODS)
    sheet = Image.new("RGB", (W * 3 + 24, (H + 22) * len(scenes) + 22), (20, 20, 24))
    draw = ImageDraw.Draw(sheet)
    for x, label in enumerate(("vecchio sistema", "nuovo sistema", "nuovo: griglia di controllo")):
        draw.text((x * (W + 12) + 6, 4), label, fill=(255, 255, 255))
    totals = {m[0]: [0.0, 0.0, 0.0] for m in METHODS}
    for index, scene in enumerate(scenes):
        depth, pid, normal, rays, *_ = trace(scene)
        p, n = reconstruct(scene, depth, rays)
        base = shade(depth, normal, pid)
        line = f"{scene.name:22s}"
        pictures = {}
        for label, fn in METHODS:
            u, v, mask = fn(scene, p, n)
            st, co, se, ed = metrics(scene, p, pid, u, v, mask)
            totals[label] = [max(totals[label][0], st), max(totals[label][1], co), max(totals[label][2], ed)]
            line += f"| {st:7.2f} {co:7.2f} {ed:6.3f}   "
            pictures[label] = (u, v, mask)
        print(line)
        cols = []
        for label, checker in (("vecchio X/Z", False), ("finale", False), ("finale", True)):
            u, v, mask = pictures[label]
            rgb, a = decal_alpha(scene, u, v, tex, checker)
            a = np.nan_to_num(a) * np.nan_to_num(mask)
            img = base * (1 - a[..., None]) + rgb * a[..., None]
            if label == "vecchio X/Z" and getattr(scene, "ghost", False):
                # The old decal is a mesh built on the collision polygon itself.
                img = ghost_decal(scene, rays, base, tex)
            cols.append(img)
        y = index * (H + 22) + 22
        draw.text((6, y), scene.name, fill=(255, 220, 120))
        for x, img in enumerate(cols):
            pic = Image.fromarray((np.clip(img[::-1], 0, 1) * 255).astype(np.uint8))
            sheet.paste(pic, (x * (W + 12) + 6, y + 14))
    print(f"{'PEGGIORE':22s}" + "".join(f"| {t[0]:7.2f} {t[1]:7.2f} {t[2]:6.3f}   " for t in totals.values()))
    sheet.save(out_png)
    stretch, compress, _ = totals["finale"]
    if stretch > 1.3 or compress > 1.7:
        print("FAIL: stretch %.2f compress %.2f" % (stretch, compress))
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
