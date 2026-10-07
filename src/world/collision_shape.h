#ifndef OPENNEOUA_COLLISION_SHAPE_H
#define OPENNEOUA_COLLISION_SHAPE_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "../matrix.h"

class NC_STACK_ypabact;
class NC_STACK_ypaworld;

namespace Collision
{
constexpr size_t MaxParts = 64;
constexpr size_t MaxVertices = 128;

struct Part
{
    std::vector<vec3d> vertices;
    std::vector<std::array<int, 3>> faces;
    std::vector<std::array<int, 2>> debugEdges; // Unique authored edges, prepared once when the hull is validated.
};

// Vertices are baked in actor-local coordinates. Visual scale/rotation are
// metadata, never applied a second time by the runtime.
class Shape
{
public:
    Shape();
    ~Shape();
    std::vector<Part> parts;
    std::string source, sourceHash;
    int assetSet = 0; // Source SET provenance; texture changes do not restrict collision use.
    vec3d visualScale = vec3d(1, 1, 1);
    vec3d visualRotation = vec3d(0, 0, 0);
    double radius = 0;
    double tolerance = 0.1;
    struct Impl;
    std::unique_ptr<Impl> impl;
};

bool Build(Shape &shape, std::string *error = nullptr);
std::shared_ptr<Shape> Load(const std::string &path, std::string *error = nullptr);
double DownExtent(const Shape &shape, const mat3x3 &rotation);

struct Contact
{
    vec3d point, normal;
    vec3d incomingVelocity;
    double depth = 0;
    double fraction = 1;
    NC_STACK_ypabact *actor = nullptr;
};

// Scene is an index of existing actors and world collision geometry; it
// owns no gameplay bodies and does not integrate forces or rigid dynamics.
class Scene
{
public:
    explicit Scene(NC_STACK_ypaworld &world);
    ~Scene();
    std::shared_ptr<Shape> LoadShared(const std::string &path);
    void Forget(NC_STACK_ypabact *actor);
    void UpdateActor(NC_STACK_ypabact *actor);
    void ResetActorPose(NC_STACK_ypabact *actor);
    bool TakeWorldContact(NC_STACK_ypabact *actor, Contact *contact);
    bool TakeUnitContact(NC_STACK_ypabact *actor, Contact *contact);
    bool Resolve(NC_STACK_ypabact *actor, const vec3d &oldPosition,
                 const mat3x3 &oldRotation, int frameTime);
    bool PairContact(NC_STACK_ypabact *a, NC_STACK_ypabact *b, Contact *contact);
    bool Trace(NC_STACK_ypabact *target, const vec3d &from, const vec3d &to,
               double radius, Contact *contact);
    bool TraceProjectile(NC_STACK_ypabact *projectile, NC_STACK_ypabact *target,
                         const mat3x3 &oldRotation, Contact *contact);
    std::vector<NC_STACK_ypabact *> ShapeTargets(const vec3d &from, const vec3d &to,
                                               double radius = 0);
    std::vector<Contact> lastContacts;
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Standalone geometry probes also power focused native tests.
bool ContactShapes(const Shape &a, const vec3d &pa, const mat3x3 &ra,
                   const Shape &b, const vec3d &pb, const mat3x3 &rb, Contact *out);
bool SweepShapes(const Shape &a, const vec3d &a0, const mat3x3 &ar0,
                 const vec3d &a1, const mat3x3 &ar1,
                 const Shape &b, const vec3d &b0, const mat3x3 &br0,
                 const vec3d &b1, const mat3x3 &br1, Contact *out);
}
#endif
