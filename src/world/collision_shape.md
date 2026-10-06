# Model-derived collision profiles

`collision_shape = Data/Models/Collision/Tiger_Set1.collision` is an optional
vehicle prototype field. Absence, empty input or `0` preserve the existing
radius/coll_* path. Unsupported actors, a missing or malformed file, or changed
visual scale/rotation retain that same fallback.

A valid profile replaces the physical vehicle body for movement, vehicle
contacts, projectile direct hits, MGUN and laser traces. The original spheres
remain available for fallback. Terrain support, forces, steering, damage and
weapon effects keep their existing gameplay paths. The new service queries and
corrects those actors; it does not integrate a second rigid-body simulation.
Movement queries respect the existing exact-world and unit-collision flags.
AoE and proximity triggers keep their separate existing logic.

Profiles contain a union of convex hulls generated offline by Studio from the
structural parts of the normal model. They preserve a concavity by splitting it
into multiple hulls. Turrets stay in their authored pose, rotors and effects are
excluded explicitly, and texture animation alone does not remove a solid part.
This is a controlled geometric approximation, not a collision test against every
render triangle. Regenerate the profile after changing the model.
Studio lists disconnected components inside a single SKLT, including planar
surfaces, so the author can explicitly exclude a rotor or effect that shares
its skeleton with the body. Source T-junctions are split on the existing surface;
simple planar boundary loops can be capped on their existing perimeter. All
repairs and any remaining open/non-manifold topology are reported before CoACD
automatic preprocessing. Generated hulls must still pass the closed/convex checks.
CoACD's adjacent-face extrusion uses a normalized margin of 0.001 on internal
cuts to close numerical seams; it does not apply an overall collider margin.

## Format, version 1

```
begin_collision_shape
version = 1
source = Tiger_Set1
source_hash = <SHA-256 of selected source geometry and component identifiers>
asset_set = 1
scale = 1_1_1
rotation = 0_0_0
begin_hull
vertex = x_y_z
face = i_j_k
...
end
...
end
```

Faces are zero-based triangles. A profile has at most 64 hulls and each hull has
4–128 vertices, closed faces, finite bounded coordinates, nonzero volume and
convexity. Invalid profiles are rejected as a whole. Studio does not silently
truncate hulls or vertices to fit these limits.

`asset_set` is optional and records the SET used to generate the profile;
zero/absence means a standalone BASE/3DS source. It does not restrict runtime
use to that SET: texture changes share the same model collision. Vertices
already include visual scale and rotation in actor-local coordinates. The
runtime checks the metadata and applies only the actor transform and the existing
body anchor. `source_hash` records provenance; the runtime does not compare it
with the current render mesh. Profiles are cached until level teardown.

The engine's virtual filesystem is reused. Its historic extension mapping keeps
an explicit exception for `.collision`, preserving the full modern extension.
No save/replay format is extended. Saved vehicle modifications are applied on
top of the prototypes from the normal data load.

## Runtime and validation

Bullet 3.25 Collision/LinearMath (vendored, zlib licence) provides the broadphase,
GJK/EPA convex contact and continuous convex casts. Hull-to-hull contacts verify
the full support depth and refine degenerate normals against cached hull faces.
This avoids the expensive full edge-pair SAT contact search without inflating or
simplifying the hull surface. Shared profiles cache these face directions once;
loading does not rebuild a second polyhedral triangulation.
The runtime indexes live cell occupants, uses swept actor bounds across sectors,
and compares old/current poses for fast motion and rotation. LEGO and prepared
filler skeletons are copied into cached triangle meshes; changes to vertices or
topology invalidate those entries.
Whole-sector LEGO geometry is cached and queried once per building, even though
the old 300-unit grid returns it from nine interior cells. Supported actors use
a separate cached mesh that omits walkable faces (`B >= 0.6`, the existing tank
support rule); their original ground/landing controller owns height and tip.
Walls, steep faces and airborne terrain queries keep the full physical geometry.

The response stops a crossing movement, projects the remaining displacement
along contact planes with up to four sweeps, removes entering velocity
and resolves penetration planes with up to eight passes. Real CCD contacts are
retained until the shared gameplay update so stopping at the contact skin does
not lose unit collision damage. Damage is invoked at most once per actor frame.
Once a blocking hit is known, later terrain-part sweeps only search up to that
time. Unit-part sweeps also reject disjoint temporal bounds, skip stationary
poses and stop their search at the earliest known contact. These bounds include
rotation. The mesh BVH can then discard parts that cannot hit earlier, which
reduces the cost of detailed profiles without changing or dropping their geometry.
At contact, translation and rotation advance to the accepted continuous-cast
fraction. A blocked turn derives outward clearance from the contacting part's
vertices over the entire shortest rotation arc, including interior extrema.
That outward move is swept against all obstacles before the turn resumes;
a second wall can leave the turn partial. Clearance is limited to the vanilla
10-unit wall nudge per actor frame, and only the arc that fits that clearance
is accepted. Triangle selection includes the full angular sweep, rather than
only the final orientation's box. Grounded vehicles keep this clearance
horizontal. A touching corner cannot rotate through a wall, while a rotation away
from the surface remains free. Each update continues from its last resolved pose
instead of replaying movement already corrected by `Move`; `_old_pos` remains
the controller's movement history. Grounded tanks/cars keep their drive axis and
signed forward/reverse speed. A true 3D contact between two such units is resolved
using the convex parts' horizontal footprints, preserving their support height.
Their continuous contacts also use horizontal normals; collision fractions
do not rewind the support height already computed by the ground controller.
Squad parent links do not suppress contacts; physical gun/dummy attachments do.
After a penetration correction, candidates are refreshed at the corrected pose.
Terrain sweeps retain the hit triangle and use its actual face normal oriented
toward the casting part: a pure angular simplex cast can otherwise return an
inward normal and prevent the outward clearance of an offset hull.
Clearance and penetration corrections reuse the existing playable-box clamp;
the earlier clamp in `Move` does not constrain a later physical correction.
Only its horizontal bounds are reused: the movement-only viewer ground snap
must not change an airborne actor's altitude during a collision correction.
Blocking unit contacts notify the grounded tank/car AI. The physical correction
stays in this service, while the original wait/pass decision is shared with the
legacy collision path. Without that notification, a stopped shape sweep would
leave the AI driving into the same vehicle indefinitely. Player controls retain
the signed drive response and do not enter the AI avoidance state.
Dead plasma residues keep the existing pickup path and are not solid bodies.
World contacts carry their pre-correction velocity into the original landing
and recoil handlers. A non-viewer tank therefore still enters LAND when CCD
stops it just above the floor. Airborne impacts keep the vanilla reflected
velocity and attenuation; shape recoil retains the safe CCD pose instead of
restoring an old centre that may overlap after steering. Ground impacts project
that recoil onto the signed drive axis and notify the existing wall-avoidance
controller. Collision sound keeps the existing sound slot.
Unit contacts similarly retain their incoming velocity and are consumed once
per actor frame, before the original landing controller handles the floor.
Airborne units reuse the vanilla centre-to-centre recoil and attenuation, while
retaining the accepted shape pose. This lets nearby aircraft rebound and settle
instead of stacking. Vehicle impacts use the original `crashvhcl` sound slot 6;
world impacts use `crashland` slot 5. Grounded player tanks retain the original
forward/reverse cone and mass/traction shove, with the pushed path checked against
units and buildings. A displaced ground unit retains its LAND support state.
An impossible placement inside
solid geometry still requires adequate spawn space; no solver creates free space.

Direct projectile tests use the projectile's actual radius/coll_* volumes.
Target-class weapon tolerances remain on the legacy path. New contacts respect
the nearest world obstruction and reuse direct damage, armor penetration and
Deflect. MGUN retains its established unit multi-hit rule, and lasers retain
their damage/secondary targeting rules. F10 displays active hull edges in cyan
and contact normals in yellow, using the existing debug projection.

`tests/collision_shape_test.cpp` links production objects to probe convex and
mixed contacts, concavities, rotation, continuous motion, LEGO changes, the
vehicle parser and the real projectile collision method. Synthetic timings
reported there measure this service only, not total game FPS. Full gameplay,
multiplayer and replay behaviour need a real session before broad conversion.
The grounded regression uses the real Tiger profile with production tank `Move`
and support alignment, including signed reverse, tilted pairs and mixed legacy
contacts. Building fixtures include both LEGO and prepared filler skeletons;
missing fillers would measure warning-log I/O instead of valid terrain queries.
The movement-order regression invokes production tank `AI_layer3` toward a CELL
target, comparing free movement with legacy and testing a complete bypass around
an idle Tiger without interpenetration. The fixture supplies target propagation
and suppresses combat only; it is not a complete mission or pathfinding test.
The landing regression drops five real-profile Tigers and follows their movement
orders through the production controller. Building bypass fixtures use the real
world ray query for rectangular and sloped LEGO geometry at 20/100 ms steps.
Air Prism probes cover every wall, vanilla bounce attenuation, retreat and
simultaneous rotation/translation without undoing the accepted CCD pose.
