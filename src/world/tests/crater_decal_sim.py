#!/usr/bin/env python3
"""Decals inside terrain craters: the decal shader of decal_projection_sim.py
run on ground dug with the crater profile of world/terraincrater_core.h.

Usage: crater_decal_sim.py output.png Graphics/GroundDecals/Impact_01.png
The exit code is not zero when a decal has holes, is cut or stretched.
CRATER_SIM_BEFORE=1 runs the shader as it was before uMoved (shows the bug).
CRATER_SIM_FAST=1 skips the big Taer scene; CRATER_SIM_ONLY=<text> runs the scenes with it in their name.
CRATER_SIM_ALLOWANCE=1 runs the second fix (in/out allowance); with
CRATER_SIM_SYMMETRIC=1 also the first one (one allowance both ways).

Holes = share of the decal disc (seen from the surface, inside 40% of the decal
size from the impact) where nothing is drawn although the decal belongs there.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decal_projection_sim as sim  # noqa: E402


def crater_drop(x, z, craters):
    """Same bowl, rim and 1.75x cap as World::Crater::SampleAt (ground only)."""
    total = np.zeros_like(x)
    deepest, highest_rim = 0.0, 0.0
    for cx, cz, radius, depth, rim in craters:
        depth = min(depth, radius * 0.5)
        d = np.hypot(x - cx, z - cz) / radius
        inside = np.clip(1.0 - d * d, 0.0, None)
        drop = np.where(d < 1.0, depth * inside * inside, 0.0)
        ridge = (d - 1.0) / 0.22
        fade = 1.0 - sim.smoothstep(1.3, 1.6, d)
        drop = drop - np.where(d < 1.6, depth * rim * np.exp(-ridge * ridge) * fade, 0.0)
        total += drop
        deepest = max(deepest, depth)
        highest_rim = max(highest_rim, depth * rim)
    return np.clip(total, -highest_rim, deepest * 1.75)


class CraterScene(sim.Scene):
    def dug_ground(self, craters, half=420.0, step=18.75, s=2500.0):
        """Ground dug on the engine lattice (18.75), flat far away."""
        n = int(round(2 * half / step))
        xs = np.linspace(-half, half, n + 1)
        X, Z = np.meshgrid(xs, xs, indexing="ij")
        Y = -crater_drop(X, Z, craters)   # Simulator: y up, the crater goes down.
        for i in range(n):
            for k in range(n):
                p = [(X[i, k], Y[i, k], Z[i, k]), (X[i + 1, k], Y[i + 1, k], Z[i + 1, k]),
                     (X[i + 1, k + 1], Y[i + 1, k + 1], Z[i + 1, k + 1]), (X[i, k + 1], Y[i, k + 1], Z[i, k + 1])]
                pid = self._next_id
                self._next_id += 1
                self.tris += [(p[0], p[1], p[2]), (p[0], p[2], p[3])]
                self.ids += [pid, pid]
        # Flat ground around the dug square.
        self.quad((-s, 0, -s), (-s, 0, s), (-half, 0, s), (-half, 0, -s))
        self.quad((half, 0, -s), (half, 0, s), (s, 0, s), (s, 0, -s))
        self.quad((-half, 0, -s), (-half, 0, -half), (half, 0, -half), (half, 0, -s))
        self.quad((-half, 0, half), (-half, 0, s), (half, 0, s), (half, 0, half))


def map_allowance(scene, p, n):
    """map_final with the surface allowance of the decal: how far craters and
    dents moved the surface under it, into the surface (uMovedIn, the dug
    bottom, a pushed wall) and out of it (uMovedOut, a raised rim). Only that
    side of the hit surface counts as moved: a roof above a crater is not.
    The second fix, kept for comparison (CRATER_SIM_ALLOWANCE=1): the engine
    now takes each pixel back to its place before the hits (map_restored)."""
    allow_in = getattr(scene, "allow_in", 0.0)
    allow_out = getattr(scene, "allow_out", 0.0)
    t, b, nn = sim.frame(scene)
    reach = scene.size * 0.5
    base_tol = min(max(sim.SAME_SURFACE_MIN, reach * sim.SAME_SURFACE_RATIO), sim.SAME_SURFACE_MAX)
    d = p - scene.c
    dn = d @ nn
    allow = np.where(dn < 0.0, allow_in, allow_out)
    tol = np.maximum(base_tol, allow)
    pt = d - nn * dn[..., None]
    r = np.linalg.norm(pt, axis=-1)
    radial = pt / np.maximum(r, 1e-4)[..., None]
    c = np.clip(n @ nn, -0.95, 1.0)
    nt = n - nn * c[..., None]
    flat = pt - nt * (dn / (1 + c))[..., None]
    level = sim.smoothstep(0.906, 0.985, c)
    flat = flat + radial * (level * np.minimum(r, np.maximum(np.abs(dn) - tol, 0.0)))[..., None]
    # Ground a crater or dent moved: it was moved along the decal axis, so the
    # straight projection puts each point where it was before the hit, as if
    # the decal had been painted first and then dug with the ground.
    lean_len = np.linalg.norm(nt, axis=-1)
    moved = np.where(allow > 0.0,
                     (1.0 - sim.smoothstep(allow, allow * 1.25 + 4.0, np.abs(dn)))
                     * (1.0 - sim.smoothstep(0.88, 0.97, lean_len)), 0.0)
    flat = flat + (pt - flat) * moved[..., None]
    steep = sim.smoothstep(0.35, 0.60, lean_len) * (1.0 - moved)
    away = np.sum(sim.normalize(nt) * radial, axis=-1) * np.sign(dn)
    away = away * sim.smoothstep(0.0, tol, np.abs(dn))
    mask = sim.smoothstep(-0.3, -0.1, c) * (1 - steep * sim.smoothstep(0.02, 0.20, away))
    band = np.maximum(reach * 0.6, tol * 1.5)
    mask = mask * (1 - level * sim.smoothstep(band * 0.6, band, np.abs(dn)))
    return flat @ t, flat @ b, mask


def map_restored(scene, p, n):
    """The engine's decal on moved ground: each pixel is first taken back to
    where it was before the craters (the same displacement field, worked out
    in the shader), then mapped. A point that was on the hit surface gets the
    straight projection of its old place and full cover ("painted, then dug");
    anything else keeps the normal rules at its old place.
    The idea of the GLSL of decalshader.h, ground craters only. The engine also
    accepts a pixel whose lattice square can put it on the hit surface (the
    dug mesh is flat between lattice points) and restores its normal."""
    craters = getattr(scene, "craters", [])
    t, b, nn = sim.frame(scene)
    reach = scene.size * 0.5
    tol = min(max(sim.SAME_SURFACE_MIN, reach * sim.SAME_SURFACE_RATIO), sim.SAME_SURFACE_MAX)
    # Simulator: y up, a positive drop is lower ground. Old place = point lifted
    # by the drop at its x, z (craters move the ground straight down).
    drop = crater_drop(p[..., 0], p[..., 2], craters) if craters else np.zeros(p.shape[:-1])
    old = p.copy()
    old[..., 1] = p[..., 1] + drop
    moved = np.abs(drop) > 0.5
    u0, v0, mask0 = sim.map_final(scene, np.where(moved[..., None], old, p), n)
    d = old - scene.c
    dn = d @ nn
    pt = d - nn * dn[..., None]
    on_plane = moved & (np.abs(dn) <= tol + 2.0)
    u = np.where(on_plane, pt @ t, u0)
    v = np.where(on_plane, pt @ b, v0)
    mask = np.where(on_plane, 1.0, mask0)
    return u, v, mask


def spill(scene, pid, u, v, mask):
    """Share of the drawn decal that landed on a roof it does not belong to."""
    roofs = getattr(scene, "roof_ids", ())
    if not roofs:
        return 0.0
    uu, vv = np.nan_to_num(u / scene.size, nan=9), np.nan_to_num(v / scene.size, nan=9)
    drawn = (np.nan_to_num(mask) > 0.05) & (np.abs(uu) < 0.5) & (np.abs(vv) < 0.5)
    if not np.any(drawn):
        return 0.0
    return float(np.mean(np.isin(pid[drawn], list(roofs))))


def holes(scene, p, u, v, mask, pid=None):
    """Share of the decal disc left empty on the real ground (buildings excluded)."""
    d = p - scene.c
    t, b, nn = sim.frame(scene)
    planar = d - nn * (d @ nn)[..., None]
    inside = np.nan_to_num(np.linalg.norm(planar, axis=-1), nan=1e9) < scene.size * 0.4
    if pid is not None and getattr(scene, "building_ids", ()):
        inside &= ~np.isin(pid, list(scene.building_ids))
    uu, vv = np.nan_to_num(u / scene.size, nan=9), np.nan_to_num(v / scene.size, nan=9)
    drawn = (np.nan_to_num(mask) > 0.05) & (np.abs(uu) < 0.5) & (np.abs(vv) < 0.5)
    if not np.any(inside):
        return 0.0
    return float(np.mean(~drawn[inside]))


def build():
    out = []
    # Pike_Rocket (Weapon 24): decal 120~150, crater radius 120~150, depth 140~150
    # (limited to half the radius), rim 35%.
    pike = [(0.0, 0.0, 135.0, 140.0, 0.35)]
    for name, craters, eye in (
            ("pike: un colpo, dall'alto", pike, (-160, 260, -200)),
            ("pike: un colpo, radente", pike, (-330, 90, -260)),
            ("pike: tre colpi", pike + [(25.0, 10.0, 130.0, 145.0, 0.35), (-15.0, 20.0, 140.0, 140.0, 0.35)],
             (-170, 260, -210)),
            ("pike: colpo sul bordo", [(70.0, 0.0, 135.0, 140.0, 0.35)], (-170, 250, -210))):
        s = CraterScene(name, (0, 0, 0), (0, 1, 0), eye, (0, -30, 0), size=135.0)
        s.dug_ground(craters)
        s.craters = craters
        out.append(s)
    # Taer (Weapon 38): decal 1200~1400 over a crater radius 260~320, depth 60~80.
    s = CraterScene("taer", (0, 0, 0), (0, 1, 0), (-900, 900, -1000), (0, -30, 0), size=1300.0)
    s.dug_ground([(0.0, 0.0, 290.0, 70.0, 0.2)], half=900.0)
    s.craters = [(0.0, 0.0, 290.0, 70.0, 0.2)]
    out.append(s)
    # Crater next to a low building: the roof is above the hit surface, the
    # decal must not take it for moved ground (the square patches seen in game).
    s = CraterScene("pike: accanto a un tetto basso", (0, 0, 0), (0, 1, 0), (-190, 230, -230),
                    (20, 0, 0), size=135.0)
    stack = pike + [(25.0, 10.0, 130.0, 145.0, 0.35), (-15.0, 20.0, 140.0, 140.0, 0.35)]
    s.dug_ground(stack)
    first = s._next_id
    s.box((35, 0, -300), (330, 90, 300))
    s.roof_ids = (first + 4,)
    s.building_ids = tuple(range(first, first + 6))
    s.pit = True
    s.craters = stack
    out.insert(4, s)
    if os.environ.get("CRATER_SIM_FAST"):
        out = out[:5]
    if os.environ.get("CRATER_SIM_ONLY"):
        out = [scene for scene in out if os.environ["CRATER_SIM_ONLY"] in scene.name]
    # The allowance the engine gives each decal: the largest displacement of
    # the surface within the decal reach (sampled like the engine does).
    for s in out:
        reach = s.size * 0.5 * 2 ** 0.5
        g = np.linspace(-reach, reach, 17)
        X, Z = np.meshgrid(g, g, indexing="ij")
        keep = np.hypot(X, Z) <= reach
        drop = crater_drop(X[keep] + s.c[0], Z[keep] + s.c[2], s.craters)
        s.allow_in = float(max(0.0, np.max(drop)))      # Dug down: into the surface.
        s.allow_out = float(max(0.0, np.max(-drop)))    # Raised rim: out of it.
        if os.environ.get("CRATER_SIM_SYMMETRIC"):
            # The first fix: one allowance both ways (painted nearby roofs).
            s.allow_in = s.allow_out = float(np.max(np.abs(drop)))
    return out


def main(out_png, tex_path):
    tex = np.asarray(Image.open(tex_path).convert("RGBA"), float) / 255.0
    scenes = build()
    sheet = Image.new("RGB", (sim.W * 2 + 18, (sim.H + 22) * len(scenes) + 22), (20, 20, 24))
    draw = ImageDraw.Draw(sheet)
    draw.text((6, 4), "decal reale", fill=(255, 255, 255))
    draw.text((sim.W + 12, 4), "griglia di controllo", fill=(255, 255, 255))
    worst = 0.0
    failed = False
    print(f"{'scena':32s} buchi tetto  stretch comprime")
    for index, scene in enumerate(scenes):
        depth, pid, normal, rays, *_ = sim.trace(scene)
        p, n = sim.reconstruct(scene, depth, rays)
        base = sim.shade(depth, normal, pid)
        mapping = (sim.map_final if os.environ.get("CRATER_SIM_BEFORE")
                   else map_allowance if os.environ.get("CRATER_SIM_ALLOWANCE") else map_restored)
        u, v, mask = mapping(scene, p, n)
        hole = holes(scene, p, u, v, mask, pid)
        spilled = spill(scene, pid, u, v, mask)
        st, co, _se, _ed = sim.metrics(scene, p, pid, u, v, mask)
        # Three hits in one spot leave a pit with almost vertical walls: the
        # decal painted on the old ground runs down them like paint, so only
        # holes count there.
        pit = scene.name == "pike: tre colpi" or getattr(scene, "pit", False)
        bad = hole > 0.02 or spilled > 0.01 or (not pit and (st > 1.6 or co > 1.9))
        failed |= bad
        worst = max(worst, hole)
        print(f"{scene.name:32s} {hole * 100:5.1f}% {spilled * 100:5.1f}% {st:7.2f} {co:7.2f}{'  FAIL' if bad else ''}")
        y = index * (sim.H + 22) + 22
        draw.text((6, y), scene.name, fill=(255, 220, 120))
        for x, checker in enumerate((False, True)):
            rgb, a = sim.decal_alpha(scene, u, v, tex, checker)
            a = np.nan_to_num(a) * np.nan_to_num(mask)
            img = base * (1 - a[..., None]) + rgb * a[..., None]
            pic = Image.fromarray((np.clip(img[::-1], 0, 1) * 255).astype(np.uint8))
            sheet.paste(pic, (x * (sim.W + 12) + 6, y + 14))
    sheet.save(out_png)
    print("FAIL" if failed else "OK")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
