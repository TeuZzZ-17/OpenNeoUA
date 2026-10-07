#include "collision_shape.h"
#include "../def_parser.h"
#include "../utils.h"
#include "../log.h"
#include "../ypabact.h"
#include "../ypatank.h"
#include "../ypagun.h"
#include "../yw.h"
#include "../skeleton.h"
#include "../yw_internal.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/NarrowPhaseCollision/btContinuousConvexCollision.h>
#include <BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h>
#include <BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h>
#include <BulletCollision/NarrowPhaseCollision/btRaycastCallback.h>
#include <LinearMath/btTransformUtil.h>

namespace Collision
{
namespace
{
btVector3 V(const vec3d &v) { return btVector3(v.x, v.y, v.z); }
vec3d V(const btVector3 &v) { return vec3d(v.x(), v.y(), v.z()); }
btTransform Pose(const vec3d &p, const mat3x3 &r)
{
    const mat3x3 t = r.Transpose();
    return btTransform(btMatrix3x3(t.m00, t.m01, t.m02, t.m10, t.m11, t.m12,
                                   t.m20, t.m21, t.m22), V(p));
}
bool Finite(const vec3d &v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
mat3x3 Rotation(const btQuaternion &q)
{
    const btMatrix3x3 b(q);
    return mat3x3(b[0][0], b[1][0], b[2][0], b[0][1], b[1][1], b[2][1],
                  b[0][2], b[1][2], b[2][2]);
}
// Ground controllers support the body vertically. A horizontal separation must
// clear the actual convex footprints, instead of lifting one tank through another.
std::vector<btVector3> Footprint(const btCollisionObjectWrapper *object)
{
    std::vector<btVector3> points;
    if (object->getCollisionShape()->getShapeType() != CONVEX_HULL_SHAPE_PROXYTYPE) return points;
    const auto *hull = static_cast<const btConvexHullShape *>(object->getCollisionShape());
    for (int i = 0; i < hull->getNumPoints(); ++i)
    {
        btVector3 p = object->getWorldTransform() * hull->getScaledPoint(i);
        p.setY(0); points.push_back(p);
    }
    std::sort(points.begin(), points.end(), [](const btVector3 &a, const btVector3 &b) {
        return a.x() < b.x() || (a.x() == b.x() && a.z() < b.z());
    });
    points.erase(std::unique(points.begin(), points.end()), points.end());
    if (points.size() < 3) return points;
    auto cross = [](const btVector3 &a, const btVector3 &b, const btVector3 &c) {
        return (b.x() - a.x()) * (c.z() - a.z()) - (b.z() - a.z()) * (c.x() - a.x());
    };
    std::vector<btVector3> outline;
    for (const auto &p : points)
    {
        while (outline.size() > 1 && cross(outline[outline.size()-2], outline.back(), p) <= 0) outline.pop_back();
        outline.push_back(p);
    }
    const size_t lower = outline.size();
    for (auto it = points.rbegin() + 1; it != points.rend(); ++it)
    {
        while (outline.size() > lower && cross(outline[outline.size()-2], outline.back(), *it) <= 0) outline.pop_back();
        outline.push_back(*it);
    }
    outline.pop_back();
    return outline;
}
bool HorizontalContact(const btCollisionObjectWrapper *a, const btCollisionObjectWrapper *b,
                       const std::vector<btVector3> &ap, const std::vector<btVector3> &bp,
                       Contact *out, bool bodyDirection)
{
    if (!a->getCollisionShape()->isConvex() || !b->getCollisionShape()->isConvex()) return false;
    std::vector<btVector3> axes;
    for (const auto *points : {&ap, &bp})
        for (size_t i = 0; i < points->size(); ++i)
        {
            const auto edge = (*points)[(i + 1) % points->size()] - (*points)[i];
            axes.emplace_back(-edge.z(), 0, edge.x());
        }
    const btVector3 ac = a->getWorldTransform().getOrigin(), bc = b->getWorldTransform().getOrigin();
    axes.emplace_back(ac.x() - bc.x(), 0, ac.z() - bc.z());
    // Sphere/polygon contact also needs axes through the polygon's vertices.
    if (ap.empty()) for (const auto &p : bp) axes.emplace_back(ac.x()-p.x(), 0, ac.z()-p.z());
    if (bp.empty()) for (const auto &p : ap) axes.emplace_back(bc.x()-p.x(), 0, bc.z()-p.z());
    if (axes.empty()) return false;
    auto support = [](const btCollisionObjectWrapper *object,
                      const std::vector<btVector3> &outline, const btVector3 &axis) {
        if (!outline.empty())
        {
            btScalar maximum = -BT_LARGE_FLOAT;
            for (const auto &point : outline) maximum = std::max(maximum, point.dot(axis));
            return maximum;
        }
        const auto *shape = static_cast<const btConvexShape *>(object->getCollisionShape());
        const auto &t = object->getWorldTransform();
        return (t * shape->localGetSupportingVertex(t.getBasis().transpose() * axis)).dot(axis);
    };
    double depth = 1e30;
    for (auto axis : axes)
    {
        if (axis.length2() < 1e-12) continue;
        axis.normalize();
        const double amax = support(a, ap, axis), amin = -support(a, ap, -axis);
        const double bmax = support(b, bp, axis), bmin = -support(b, bp, -axis);
        const double positive = bmax - amin, negative = amax - bmin;
        if (positive <= 0 || negative <= 0) return false;
        if (std::min(positive, negative) < depth)
        {
            depth = std::min(positive, negative);
            out->normal = V(positive < negative ? axis : -axis);
        }
    }
    out->depth = depth;
    if (bodyDirection)
    {
        btVector3 axis = a->getCollisionObject()->getWorldTransform().getOrigin() -
                         b->getCollisionObject()->getWorldTransform().getOrigin();
        axis.setY(0);
        if (axis.length2()<1e-12) axis=btVector3(1,0,0);
        axis.normalize();
        // All touching parts of a Robo use one outward body direction.
        // Opposing leaf normals can otherwise keep an embedded unit trapped.
        out->normal=V(axis);
        out->depth=support(b,bp,axis)+support(a,ap,-axis);
    }
    return depth < 1e29;
}
bool Triple(const std::string &s, vec3d *v)
{
    std::string text = s;
    std::replace(text.begin(), text.end(), '_', ' ');
    std::istringstream input(text);
    std::string extra;
    return (input >> v->x >> v->y >> v->z) && !(input >> extra) && Finite(*v);
}
struct Detector
{
    btDefaultCollisionConfiguration config;
    btCollisionDispatcher dispatcher{&config};
    btDbvtBroadphase broadphase;
    btCollisionWorld world{&dispatcher, &broadphase, &config};
    Detector() { world.getDispatchInfo().m_enableSatConvex = false; }
};
struct Hull : btConvexHullShape
{
    // Keep validated face directions once per shared profile. Bullet needs
    // the vertices for GJK, not a second triangulation and contact clipper.
    std::vector<btVector3> planes;
};
bool ConvexContactPlane(const btCollisionObjectWrapper *a,
                        const btCollisionObjectWrapper *b, Contact *out)
{
    if (a->getCollisionShape()->getShapeType()!=CONVEX_HULL_SHAPE_PROXYTYPE ||
        b->getCollisionShape()->getShapeType()!=CONVEX_HULL_SHAPE_PROXYTYPE) return true;
    auto support=[](const btCollisionObjectWrapper *object,const btVector3 &direction) {
        const auto *shape=static_cast<const btConvexShape *>(object->getCollisionShape());
        const auto &transform=object->getWorldTransform();
        return (transform*shape->localGetSupportingVertex(
            transform.getBasis().transpose()*direction)).dot(direction);
    };
    btVector3 reported=V(out->normal);
    if (reported.length2()>1e-12)
    {
        reported.normalize();
        const double positive=support(b,reported)+support(a,-reported);
        const double negative=support(a,reported)+support(b,-reported);
        const double projected=std::min(positive,negative);
        if (projected<=0) return false;
        // An EPA normal whose complete support depth agrees with its contact
        // already clears the hulls. Only degenerate normals need face tests.
        if (projected<=out->depth+1e-3)
        {
            out->depth=projected;
            out->normal=V(positive<negative?reported:-reported);
            return true;
        }
    }
    std::vector<btVector3> axes{V(out->normal)};
    for (const auto *object:{a,b})
    {
        const auto *hull=static_cast<const Hull *>(object->getCollisionShape());
        for (const auto &local:hull->planes)
            axes.push_back(object->getWorldTransform().getBasis()*local);
    }
    double depth=1e30;
    for (auto axis:axes)
    {
        if (axis.length2()<1e-12) continue;
        axis.normalize();
        const double positive=support(b,axis)+support(a,-axis);
        const double negative=support(a,axis)+support(b,-axis);
        if (positive<=0 || negative<=0) return false;
        if (std::min(positive,negative)<depth)
        {
            depth=std::min(positive,negative);
            out->normal=V(positive<negative?axis:-axis);
        }
    }
    // GJK establishes the real overlap. Refine its sometimes diagonal
    // zero-margin normal against the hull faces and project the full hulls;
    // the resulting displacement clears their actual supporting planes.
    out->depth=depth;
    return true;
}
struct ContactCollector : btCollisionWorld::ContactResultCallback
{
    const btCollisionObject *self;
    bool horizontal;
    bool bodyDirection;
    std::set<std::pair<const btCollisionShape *, const btCollisionShape *>> leaves;
    std::map<std::pair<const btCollisionObject *, const btCollisionShape *>, std::vector<btVector3>> footprints;
    std::vector<Contact> contacts;
    explicit ContactCollector(const btCollisionObject *obj, bool ground = false, bool body = false)
        : self(obj), horizontal(ground), bodyDirection(body) {}
    const std::vector<btVector3> &FootprintFor(const btCollisionObjectWrapper *object)
    {
        const auto key = std::make_pair(object->getCollisionObject(), object->getCollisionShape());
        const auto found = footprints.find(key);
        if (found != footprints.end()) return found->second;
        // Leaf poses stay fixed throughout one Bullet contact query.
        return footprints.emplace(key, Footprint(object)).first->second;
    }
    btScalar addSingleResult(btManifoldPoint &p, const btCollisionObjectWrapper *a,
                            int, int, const btCollisionObjectWrapper *b, int, int) override
    {
        if ( p.getDistance() > 0 ) return 0;
        const bool selfIsA = a->getCollisionObject() == self;
        Contact c;
        c.depth = -p.getDistance();
        c.normal = V(selfIsA ? p.m_normalWorldOnB : -p.m_normalWorldOnB);
        c.point = V(selfIsA ? p.getPositionWorldOnB() : p.getPositionWorldOnA());
        if (horizontal)
        {
            if (!leaves.insert({a->getCollisionShape(), b->getCollisionShape()}).second) return 0;
            const auto *selfWrap = selfIsA ? a : b, *otherWrap = selfIsA ? b : a;
            if (!HorizontalContact(selfWrap, otherWrap, FootprintFor(selfWrap), FootprintFor(otherWrap), &c, bodyDirection)) return 0;
        }
        else if (a->getCollisionShape()->getShapeType()==CONVEX_HULL_SHAPE_PROXYTYPE &&
                 b->getCollisionShape()->getShapeType()==CONVEX_HULL_SHAPE_PROXYTYPE)
        {
            if (!leaves.insert({a->getCollisionShape(),b->getCollisionShape()}).second) return 0;
            if (!ConvexContactPlane(selfIsA?a:b,selfIsA?b:a,&c)) return 0;
        }
        contacts.push_back(c);
        return 0;
    }
};
void ContactPair(Detector &detector, btCollisionObject &a, btCollisionObject &b,
                 ContactCollector &result)
{
    btVector3 aMin, aMax, bMin, bMax;
    a.getCollisionShape()->getAabb(a.getWorldTransform(), aMin, aMax);
    b.getCollisionShape()->getAabb(b.getWorldTransform(), bMin, bMax);
    // Direct Bullet pair queries bypass the world's broad phase.
    if (!TestAabbAgainstAabb2(aMin, aMax, bMin, bMax)) return;
    detector.world.contactPairTest(&a, &b, result);
}
struct TerrainCastResult : btCollisionWorld::ClosestConvexResultCallback
{
    int triangle = -1;
    TerrainCastResult(const btVector3 &from, const btVector3 &to) : ClosestConvexResultCallback(from, to) {}
    btScalar addSingleResult(btCollisionWorld::LocalConvexResult &result, bool worldNormal) override
    {
        triangle = result.m_localShapeInfo ? result.m_localShapeInfo->m_triangleIndex : -1;
        return ClosestConvexResultCallback::addSingleResult(result, worldNormal);
    }
};
void CastTerrain(const btConvexShape *shape,const btTransform &from,const btTransform &to,
                 btCollisionObject &object,TerrainCastResult &result)
{
    if (from.getRotation().angleShortestPath(to.getRotation()) < 1e-5)
    {
        btCollisionWorld::objectQuerySingle(shape,from,to,&object,object.getCollisionShape(),
                                            object.getWorldTransform(),result,0);
        return;
    }
    // Bullet's mesh object query selects triangles using only the final
    // orientation's box. A long body can hit a wall midway through a turn
    // while both end poses miss it. Include the complete angular sweep.
    struct Query : btTriangleConvexcastCallback
    {
        const btCollisionObject &object;
        TerrainCastResult &result;
        Query(const btConvexShape *shape,const btTransform &from,const btTransform &to,
              const btCollisionObject &object,TerrainCastResult &result)
            : btTriangleConvexcastCallback(shape,from,to,object.getWorldTransform(),0),
              object(object),result(result) {}
        btScalar reportHit(const btVector3 &normal,const btVector3 &point,btScalar fraction,
                           int part,int triangle) override
        {
            if (fraction <= result.m_closestHitFraction)
            {
                btCollisionWorld::LocalShapeInfo info;
                info.m_shapePart=part; info.m_triangleIndex=triangle;
                btCollisionWorld::LocalConvexResult hit(&object,&info,normal,point,fraction);
                return result.addSingleResult(hit,true);
            }
            return fraction;
        }
    } query(shape,from,to,object,result);
    const btTransform inverse=object.getWorldTransform().inverse();
    const btTransform localFrom=inverse*from,localTo=inverse*to;
    btVector3 linear,angular,minimum,maximum;
    btTransformUtil::calculateVelocity(localFrom,localTo,1,linear,angular);
    shape->calculateTemporalAabb(localFrom,linear,angular,1,minimum,maximum);
    static_cast<const btBvhTriangleMeshShape *>(object.getCollisionShape())->processAllTriangles(
        &query,minimum,maximum);
}
struct SweepBounds
{
    btVector3 minimum, maximum;
};
SweepBounds SweptBounds(const btConvexShape *shape, const btTransform &from,
                       const btTransform &to)
{
    btVector3 linear, angular;
    btTransformUtil::calculateVelocity(from, to, 1, linear, angular);
    SweepBounds bounds;
    shape->calculateTemporalAabb(from, linear, angular, 1, bounds.minimum, bounds.maximum);
    return bounds;
}
bool Cast(const btConvexShape *a, const btTransform &a0, const btTransform &a1,
          const btConvexShape *b, const btTransform &b0, const btTransform &b1,
          Contact *out)
{
    btVector3 linear, angular, aMin, aMax, bMin, bMax;
    btTransformUtil::calculateVelocity(a0, a1, 1, linear, angular);
    a->calculateTemporalAabb(a0, linear, angular, 1, aMin, aMax);
    btTransformUtil::calculateVelocity(b0, b1, 1, linear, angular);
    b->calculateTemporalAabb(b0, linear, angular, 1, bMin, bMax);
    // A detailed model can have thousands of hull pairs near another model.
    // Only pairs whose complete swept bounds overlap need a convex cast.
    if (!TestAabbAgainstAabb2(aMin, aMax, bMin, bMax)) return false;
    btVoronoiSimplexSolver simplex;
    btGjkEpaPenetrationDepthSolver penetration;
    btContinuousConvexCollision cast(a, b, &simplex, &penetration);
    btConvexCast::CastResult hit;
    hit.m_fraction = 1;
    hit.m_allowedPenetration = 0;
    if ( !cast.calcTimeOfImpact(a0, a1, b0, b1, hit) ) return false;
    if ( out )
    {
        out->fraction = hit.m_fraction;
        out->normal = V(hit.m_normal);
        out->normal.normalise();
        out->point = V(hit.m_hitPoint);
    }
    return true;
}
bool RotationEntersPlane(const btConvexShape *shape, const btTransform &from,
                        const btTransform &to, const vec3d &relativeMove, const vec3d &normal)
{
    const auto q0 = from.getRotation(), q1 = to.getRotation();
    const double angle = q0.angleShortestPath(q1);
    if (angle < 1e-6) return false;
    const double fraction = std::min(1.0, 0.001 / angle);
    const btMatrix3x3 next(q0.slerp(q1, fraction));
    const btVector3 n = V(normal);
    const btVector3 first = from.getBasis() * shape->localGetSupportingVertex(from.getBasis().transpose() * -n);
    const btVector3 probe = next * shape->localGetSupportingVertex(next.transpose() * -n);
    return (probe - first + V(relativeMove * fraction)).dot(n) < -1e-7;
}
double RotationClearance(const btConvexShape *shape, const btTransform &local,
                         const btTransform &from, const btQuaternion &to, const vec3d &normal)
{
    // Keep every vertex outside the contact plane throughout the shortest
    // rotation, including an interior extremum that its end pose can hide.
    btQuaternion delta = to * from.getRotation().inverse();
    if (delta.w() < 0) delta = -delta;
    delta.normalize();
    const double angle = delta.getAngle();
    if (angle < 1e-6) return 0;
    const btVector3 axis = delta.getAxis(), n = V(normal);
    double first = 1e30, swept = 1e30;
    auto point = [&](const btVector3 &vertex) {
        const btVector3 r = from.getBasis() * (local * vertex);
        const double c = n.dot(axis) * axis.dot(r);
        const double a = n.dot(r) - c, b = n.dot(axis.cross(r));
        double minimum = std::min(a + c, a * cos(angle) + b * sin(angle) + c);
        double critical = atan2(b, a) + SIMD_PI;
        if (critical > SIMD_2_PI) critical -= SIMD_2_PI;
        if (critical >= 0 && critical <= angle)
            minimum = std::min(minimum, a * cos(critical) + b * sin(critical) + c);
        first = std::min(first, a + c); swept = std::min(swept, minimum);
    };
    if (shape->getShapeType() == CONVEX_HULL_SHAPE_PROXYTYPE)
    {
        const auto *hull = static_cast<const btConvexHullShape *>(shape);
        for (int i = 0; i < hull->getNumPoints(); ++i) point(hull->getScaledPoint(i));
    }
    else if (shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE)
        point(btVector3(0, 0, 0)); // The spherical radius is constant under rotation.
    return first < 1e29 ? std::max(0.0, first - swept) : 0;
}
class ShapeParser : public ScriptParser::DataHandler
{
public:
    Shape &shape;
    bool began = false, complete = false, inPart = false, version = false;
    explicit ShapeParser(Shape &s) : shape(s) {}
    bool IsScope(ScriptParser::Parser &, const std::string &word, const std::string &) override
    {
        if ( word != "begin_collision_shape" || began ) return false;
        began = true;
        return true;
    }
    int Handle(ScriptParser::Parser &, const std::string &key, const std::string &value) override
    {
        using namespace ScriptParser;
        if ( key == "end" )
        {
            if ( inPart ) { inPart = false; return RESULT_OK; }
            complete = true;
            return RESULT_SCOPE_END;
        }
        if ( key == "begin_hull" && !inPart && shape.parts.size() < MaxParts )
        {
            shape.parts.emplace_back(); inPart = true; return RESULT_OK;
        }
        if ( inPart )
        {
            Part &part = shape.parts.back();
            vec3d v;
            if ( key == "vertex" && part.vertices.size() < MaxVertices && Triple(value, &v) )
            { part.vertices.push_back(v); return RESULT_OK; }
            if ( key == "face" && part.faces.size() < MaxVertices * 4 && Triple(value, &v) &&
                 v.x == floor(v.x) && v.y == floor(v.y) && v.z == floor(v.z) &&
                 v.x >= 0 && v.y >= 0 && v.z >= 0 && v.x < MaxVertices && v.y < MaxVertices && v.z < MaxVertices )
            { part.faces.push_back({(int)v.x, (int)v.y, (int)v.z}); return RESULT_OK; }
            return RESULT_BAD_DATA;
        }
        if ( key == "version" && value == "1" && !version ) { version = true; return RESULT_OK; }
        if ( key == "source" ) { shape.source = value; return RESULT_OK; }
        if ( key == "source_hash" ) { shape.sourceHash = value; return RESULT_OK; }
        if (key == "asset_set")
        {
            std::istringstream input(value); int number; std::string extra;
            if (!(input >> number) || (input >> extra) || number < 0 || number > 255) return RESULT_BAD_DATA;
            shape.assetSet = number; return RESULT_OK;
        }
        if ( key == "scale" && Triple(value, &shape.visualScale) ) return RESULT_OK;
        if ( key == "rotation" && Triple(value, &shape.visualRotation) ) return RESULT_OK;
        return RESULT_BAD_DATA;
    }
};
}

struct Shape::Impl
{
    std::vector<std::unique_ptr<btConvexHullShape>> hulls;
    btCompoundShape compound;
};
Shape::Shape() = default;
Shape::~Shape() = default;

bool Build(Shape &shape, std::string *error)
{
    auto fail = [&](const char *why) { if (error) *error = why; shape.impl.reset(); return false; };
    if ( shape.parts.empty() || shape.parts.size() > MaxParts ) return fail("invalid hull count");
    auto data = std::unique_ptr<Shape::Impl>(new Shape::Impl);
    vec3d lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
    shape.radius = 0;
    for (Part &part : shape.parts)
    {
        if ( part.vertices.size() < 4 || part.vertices.size() > MaxVertices || part.faces.size() < 4 || part.faces.size() > MaxVertices * 4 )
            return fail("invalid hull vertices/faces");
        vec3d center(0, 0, 0);
        for (const vec3d &v : part.vertices)
        {
            if ( !Finite(v) || v.length() > 1e7 ) return fail("non-finite or oversized vertex");
            center += v;
            lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
            hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
            shape.radius = std::max(shape.radius, v.length());
        }
        center /= (double)part.vertices.size();
        std::unique_ptr<Hull> hull(new Hull);
        std::map<std::pair<int, int>, int> edges;
        double volume = 0;
        for (const auto &f : part.faces)
        {
            for (int index : f) if (index < 0 || (size_t)index >= part.vertices.size()) return fail("face index out of range");
            const vec3d &a = part.vertices[f[0]], &b = part.vertices[f[1]], &c = part.vertices[f[2]];
            vec3d normal = (b - a) * (c - a);
            const double length = normal.normalise();
            if (length < 1e-9) return fail("degenerate face");
            if ( normal.dot(a - center) < 0 ) normal = -normal;
            const double eps = std::max(1e-5, shape.radius * 1e-6);
            for (const vec3d &v : part.vertices)
                if (normal.dot(v - a) > eps) return fail("part is not convex");
            const btVector3 axis=V(normal);
            if (std::none_of(hull->planes.begin(),hull->planes.end(),[&](const btVector3 &other) {
                // Compare directions directly: float dot products lose the
                // precision needed to recognise identical oblique planes.
                return (axis-other).length2()<2e-12 || (axis+other).length2()<2e-12;
            })) hull->planes.push_back(axis);
            volume += fabs((a - center).dot((b - center) * (c - center))) / 6.0;
            for (int i = 0; i < 3; ++i) ++edges[std::minmax(f[i], f[(i + 1) % 3])];
        }
        for (const auto &edge : edges) if (edge.second != 2) return fail("hull is not closed");
        if (volume < 1e-9) return fail("hull has no volume");
        part.debugEdges.clear();
        part.debugEdges.reserve(edges.size());
        for (const auto &edge : edges)
            part.debugEdges.push_back({{edge.first.first, edge.first.second}});
        hull->setMargin(0);
        for (const vec3d &v : part.vertices) hull->addPoint(V(v), false);
        hull->recalcLocalAabb();
        data->compound.addChildShape(btTransform::getIdentity(), hull.get());
        data->hulls.push_back(std::move(hull));
    }
    shape.tolerance = std::max(0.1, std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) * 0.005);
    shape.impl = std::move(data);
    return true;
}
std::shared_ptr<Shape> Load(const std::string &path, std::string *error)
{
    std::shared_ptr<Shape> shape(new Shape);
    auto *reader = new ShapeParser(*shape);
    ScriptParser::HandlersList handlers{reader};
    if ( !ScriptParser::ParseFile(path, handlers, ScriptParser::FLAG_NO_INCLUDE | ScriptParser::FLAG_NO_SCOPE_SKIP) ||
         !reader->complete || !reader->version || !Build(*shape, error) )
    {
        if (error && error->empty()) *error = "invalid or incomplete collision profile";
        return nullptr;
    }
    return shape;
}
double DownExtent(const Shape &shape, const mat3x3 &rotation)
{
    if (!shape.impl) return 0;
    const btTransform pose = Pose(vec3d(0,0,0), rotation);
    const btVector3 direction = pose.getBasis().transpose() * btVector3(0,1,0);
    double extent = 0;
    for (const auto &hull : shape.impl->hulls)
        extent = std::max(extent, (double)(pose.getBasis() * hull->localGetSupportingVertex(direction)).y());
    return extent;
}
bool ContactShapes(const Shape &a, const vec3d &pa, const mat3x3 &ra,
                   const Shape &b, const vec3d &pb, const mat3x3 &rb, Contact *out)
{
    if (!a.impl || !b.impl) return false;
    Detector detector;
    btCollisionObject ca, cb;
    ca.setCollisionShape(&a.impl->compound); cb.setCollisionShape(&b.impl->compound);
    ca.setWorldTransform(Pose(pa, ra)); cb.setWorldTransform(Pose(pb, rb));
    ContactCollector result(&ca);
    ContactPair(detector, ca, cb, result);
    if ( result.contacts.empty() ) return false;
    if (out) *out = *std::max_element(result.contacts.begin(), result.contacts.end(),
                         [](const Contact &x, const Contact &y) { return x.depth < y.depth; });
    return true;
}
bool SweepShapes(const Shape &a, const vec3d &a0, const mat3x3 &ar0,
                 const vec3d &a1, const mat3x3 &ar1,
                 const Shape &b, const vec3d &b0, const mat3x3 &br0,
                 const vec3d &b1, const mat3x3 &br1, Contact *out)
{
    if (!a.impl || !b.impl) return false;
    Contact first; bool found = false;
    for (const auto &ha : a.impl->hulls) for (const auto &hb : b.impl->hulls)
    {
        Contact c;
        if ( Cast(ha.get(), Pose(a0, ar0), Pose(a1, ar1), hb.get(), Pose(b0, br0), Pose(b1, br1), &c) &&
             (!found || c.fraction < first.fraction) ) { first = c; found = true; }
    }
    if (found && out) *out = first;
    return found;
}

namespace
{
bool Vehicle(const NC_STACK_ypabact *a)
{
    return a && (a->_bact_type == BACT_TYPES_BACT || a->_bact_type == BACT_TYPES_TANK ||
                 a->_bact_type == BACT_TYPES_CAR || a->_bact_type == BACT_TYPES_FLYER ||
                 a->_bact_type == BACT_TYPES_UFO ||
                 (a->_bact_type == BACT_TYPES_ROBO && a->HasCollisionShape()));
}
bool Live(const NC_STACK_ypabact *a)
{
    return a && a->_status != BACT_STATUS_DEAD && !(a->_status_flg & BACT_STFLAG_DEATH1);
}
bool Residue(const NC_STACK_ypabact *a)
{
    return a && a->_status == BACT_STATUS_DEAD &&
           (a->_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE) && a->_scale_time > 0;
}
bool Related(const NC_STACK_ypabact *a, const NC_STACK_ypabact *b)
{
    const auto *gunA = dynamic_cast<const NC_STACK_ypagun *>(a);
    const auto *gunB = dynamic_cast<const NC_STACK_ypagun *>(b);
    const bool attachedA = a->_isUnitGunChild || a->_isDummy ||
        (gunA && (gunA->_gunFlags & NC_STACK_ypagun::GUN_FLAGS_ROBO));
    const bool attachedB = b->_isUnitGunChild || b->_isDummy ||
        (gunB && (gunB->_gunFlags & NC_STACK_ypagun::GUN_FLAGS_ROBO));
    return a == b || (attachedA && a->_parent == b) || (attachedB && b->_parent == a) ||
           (attachedA && attachedB && a->_parent && a->_parent == b->_parent);
}
bool GroundDrive(const NC_STACK_ypabact *a)
{
    return a && (a->_status_flg & BACT_STFLAG_LAND) &&
           (a->_bact_type == BACT_TYPES_TANK || a->_bact_type == BACT_TYPES_CAR);
}
bool HorizontalPair(const NC_STACK_ypabact *a, const NC_STACK_ypabact *b)
{
    // Robo altitude belongs to its hover controller, like grounded support.
    const auto controlled = [](const NC_STACK_ypabact *actor) {
        return actor && (GroundDrive(actor) || actor->_bact_type == BACT_TYPES_ROBO);
    };
    return controlled(a) && controlled(b);
}
void SetVelocity(NC_STACK_ypabact *a, const vec3d &velocity)
{
    if (GroundDrive(a))
    {
        a->_fly_dir = a->_rotation.AxisZ();
        a->_fly_dir_length = velocity.dot(a->_fly_dir);
    }
    else
    {
        a->_fly_dir_length = velocity.length();
        if (a->_fly_dir_length > 1e-8) a->_fly_dir = velocity / a->_fly_dir_length;
    }
}
void NotifyUnitContact(NC_STACK_ypabact *a, NC_STACK_ypabact *other, const vec3d &normal)
{
    if (other && GroundDrive(a))
        if (auto *tank = dynamic_cast<NC_STACK_ypatank *>(a)) tank->HandleShapeUnitCollision(other, normal);
}
vec3d ConstrainPosition(NC_STACK_ypabact *a, const vec3d &position)
{
    if (a->_wrldSize.x <= 0 || a->_wrldSize.y >= 0) return position;
    const vec3d saved = a->_position;
    a->_position = position;
    a->CorrectPositionInLevelBox(nullptr);
    // Reuse the horizontal border, without the viewer ground snap performed
    // by ordinary movement. Physical clearance keeps its requested altitude.
    const vec3d constrained(a->_position.x, position.y, a->_position.z);
    a->_position = saved;
    return constrained;
}
struct Body
{
    NC_STACK_ypabact *actor = nullptr;
    std::vector<std::unique_ptr<btSphereShape>> spheres;
    std::unique_ptr<btCompoundShape> compound;
    std::shared_ptr<Shape> profile;
    std::unique_ptr<btTriangleMesh> mesh;
    std::vector<btVector3> triangleNormals;
    std::unique_ptr<btBvhTriangleMeshShape> terrain;
    btCollisionObject object;
    std::vector<btConvexShape *> parts;
    std::vector<btTransform> local;
    std::vector<SweepBounds> sweptParts, currentParts;
    btTransform oldPose = btTransform::getIdentity();
    vec3d resolvedPosition;
    mat3x3 resolvedRotation;
    int resolvedStamp = -1;
    std::set<NC_STACK_ypabact *> pendingDamage;
    Contact worldContact;
    bool hasWorldContact = false;
    Contact unitContact;
    bool hasUnitContact = false;
    bool touchedUnit = false;
    double clearanceUsed = 0;
    bool residue = false;
    int stamp = -1;
    uint64_t signature = 0;
};
struct Candidates : btBroadphaseAabbCallback
{
    std::vector<Body *> bodies;
    bool process(const btBroadphaseProxy *proxy) override
    {
        auto *object = static_cast<btCollisionObject *>(proxy->m_clientObject);
        auto *body = static_cast<Body *>(object->getUserPointer());
        if (body) bodies.push_back(body);
        return true;
    }
};
}

struct Scene::Impl
{
    NC_STACK_ypaworld &world;
    Detector detector;
    std::map<NC_STACK_ypabact *, std::unique_ptr<Body>> actors;
    std::map<std::tuple<int, int, bool>, std::unique_ptr<Body>> tiles;
    std::map<std::string, std::shared_ptr<Shape>> profiles;
    int stamp = -1;
    explicit Impl(NC_STACK_ypaworld &w) : world(w) {}
    ~Impl()
    {
        for (auto &pair : actors) detector.world.removeCollisionObject(&pair.second->object);
    }
    void Update(NC_STACK_ypabact *a)
    {
        if ((!Live(a) && !Residue(a)) || a->_bact_type == BACT_TYPES_MISSLE) return;
        auto &slot = actors[a];
        const auto profile = Live(a) ? a->_collisionShape : std::shared_ptr<Shape>();
        if (slot && (slot->profile != profile || slot->residue != Residue(a)))
        {
            detector.world.removeCollisionObject(&slot->object);
            slot.reset();
        }
        if (!slot)
        {
            slot.reset(new Body);
            Body &body = *slot;
            body.actor = a;
            body.profile = profile;
            body.residue = Residue(a);
            if (a->HasCollisionShape() && Live(a))
            {
                body.object.setCollisionShape(&body.profile->impl->compound);
                for (auto &h : body.profile->impl->hulls)
                { body.parts.push_back(h.get()); body.local.push_back(btTransform::getIdentity()); }
            }
            else
            {
                std::vector<NC_STACK_ypabact::TCollisionSphereWorld> spheres;
                a->GetCollisionSpheres(spheres, vec3d(0, 0, 0), mat3x3::Ident(), false);
                if (spheres.empty()) { slot.reset(); actors.erase(a); return; }
                body.compound.reset(new btCompoundShape);
                for (const auto &s : spheres)
                {
                    std::unique_ptr<btSphereShape> sphere(new btSphereShape(s.radius));
                    btTransform local = btTransform::getIdentity(); local.setOrigin(V(s.center));
                    body.compound->addChildShape(local, sphere.get());
                    body.parts.push_back(sphere.get()); body.local.push_back(local);
                    body.spheres.push_back(std::move(sphere));
                }
                body.object.setCollisionShape(body.compound.get());
            }
            body.object.setUserPointer(&body);
            body.object.setWorldTransform(Pose(a->GetBodyPosition(), a->_rotation));
            body.oldPose = body.object.getWorldTransform();
            detector.world.addCollisionObject(&body.object);
        }
        Body &body = *slot;
        const bool newFrame = body.stamp != world._timeStamp;
        if (newFrame)
        {
            body.oldPose = body.object.getWorldTransform(); body.stamp = world._timeStamp;
            body.pendingDamage.clear();
            body.hasWorldContact = false;
            body.hasUnitContact = false;
            body.touchedUnit = false;
            body.clearanceUsed = 0;
        }
        const btTransform pose = Pose(a->GetBodyPosition(), a->_rotation);
        if (!newFrame && body.object.getWorldTransform() == pose) return;
        body.object.setWorldTransform(pose);
        body.sweptParts.resize(body.parts.size());
        body.currentParts.resize(body.parts.size());
        for (size_t i = 0; i < body.parts.size(); ++i)
        {
            const btTransform current = body.object.getWorldTransform() * body.local[i];
            body.sweptParts[i] = SweptBounds(body.parts[i], body.oldPose * body.local[i], current);
            body.parts[i]->getAabb(current, body.currentParts[i].minimum, body.currentParts[i].maximum);
        }
        btVector3 lo, hi, oldLo, oldHi;
        body.object.getCollisionShape()->getAabb(body.object.getWorldTransform(), lo, hi);
        body.object.getCollisionShape()->getAabb(body.oldPose, oldLo, oldHi);
        lo.setMin(oldLo); hi.setMax(oldHi);
        const auto &oldBasis = body.oldPose.getBasis(), &newBasis = body.object.getWorldTransform().getBasis();
        if ((oldBasis.getColumn(0) - newBasis.getColumn(0)).length2() +
            (oldBasis.getColumn(1) - newBasis.getColumn(1)).length2() > 1e-10)
        {
            const double radius = a->GetCollisionBroadRadius();
            const btVector3 pad(radius, radius, radius);
            lo.setMin(body.oldPose.getOrigin() - pad);
            hi.setMax(body.oldPose.getOrigin() + pad);
            lo.setMin(body.object.getWorldTransform().getOrigin() - pad);
            hi.setMax(body.object.getWorldTransform().getOrigin() + pad);
        }
        detector.broadphase.setAabb(body.object.getBroadphaseHandle(), lo, hi, &detector.dispatcher);
    }
    void Frame()
    {
        if (stamp == world._timeStamp) return;
        stamp = world._timeStamp;
        std::set<NC_STACK_ypabact *> live;
        // Cell occupants are authoritative, including unparented units and
        // actors whose hierarchy changed during creation or destruction.
        for (cellArea &cell : world._cells)
            for (NC_STACK_ypabact *a : world.SnapshotBacts(cell.unitsList))
                if (Live(a) || Residue(a)) live.insert(a);
        for (NC_STACK_ypabact *a : world.SnapshotBacts(world._unitsList))
            if (Live(a) || Residue(a)) live.insert(a);
        for (auto it = actors.begin(); it != actors.end();)
        {
            if (!live.count(it->first))
            { detector.world.removeCollisionObject(&it->second->object); it = actors.erase(it); }
            else ++it;
        }
        for (auto *a : live) Update(a);
    }
    NC_STACK_ypabact *Actor(Body *body)
    {
        return body->actor;
    }
    std::vector<Body *> Nearby(const btVector3 &lo, const btVector3 &hi)
    {
        Candidates result;
        detector.broadphase.aabbTest(lo, hi, result);
        return result.bodies;
    }
    void Order(std::vector<Body *> &bodies)
    {
        std::sort(bodies.begin(), bodies.end(), [](const Body *x, const Body *y) {
            if (x->actor && y->actor) return x->actor->_gid < y->actor->_gid;
            if (x->actor || y->actor) return x->actor != nullptr;
            const auto &a = x->object.getWorldTransform().getOrigin();
            const auto &b = y->object.getWorldTransform().getOrigin();
            if (a.x() != b.x()) return a.x() < b.x();
            if (a.z() != b.z()) return a.z() < b.z();
            return a.y() < b.y();
        });
    }
    std::vector<Body *> Terrain(const vec3d &from, const vec3d &to, double radius, bool supported)
    {
        std::vector<Body *> result;
        std::set<std::tuple<int, int, bool>> visited;
        int x0 = std::max(1, (int)floor((std::min(from.x, to.x) - radius) / 300.0) - 1);
        int x1 = std::min(world.GetMapSize().x * 4 - 2, (int)ceil((std::max(from.x, to.x) + radius) / 300.0) + 1);
        int z0 = std::max(1, (int)floor((-std::max(from.z, to.z) - radius) / 300.0) - 1);
        int z1 = std::min(world.GetMapSize().y * 4 - 2, (int)ceil((-std::min(from.z, to.z) + radius) / 300.0) + 1);
        const int cx = (int)(to.x + 150) / 300, cz = (int)(-to.z + 150) / 300;
        for (int x = x0; x <= x1; ++x) for (int z = z0; z <= z1; ++z)
        {
            TSectorCollision tile = world.sub_44DBF8(cx, cz, x, z, 0);
            if (!tile.CollisionType || !tile.sklt) continue;
            // A whole-sector LEGO is returned for all nine interior grid cells.
            // Cache and query that physical building only once.
            const bool whole = tile.CollisionType == 1 && world._cells(tile.Cell).SectorType == 1;
            const auto key = std::make_tuple(whole ? tile.Cell.x * 4 + 1 : x,
                                             whole ? tile.Cell.y * 4 + 1 : z, supported);
            if (!visited.insert(key).second) continue;
            if (tile.CollisionType != 1) world.sub_44E07C(tile);
            const auto *s = tile.sklt->GetSkelet();
            uint64_t signature = reinterpret_cast<uintptr_t>(tile.sklt);
            // Filler skeletons are shared mutable scratch geometry. Copy only
            // after preparing this tile and invalidate on actual vertex changes.
            for (const auto &v : s->POO)
                for (double n : {v.x, v.y, v.z})
                    signature = signature * 1099511628211ULL ^ std::hash<double>()(n);
            for (const auto &f : s->polygons)
            {
                signature = signature * 1099511628211ULL ^ (uint64_t)f.num_vertices;
                signature = signature * 1099511628211ULL ^ std::hash<double>()(f.B);
                for (int i = 0; i < f.num_vertices; ++i)
                    signature = signature * 1099511628211ULL ^ (uint64_t)f.v[i];
            }
            auto &slot = tiles[key];
            if (!slot || slot->signature != signature)
            {
                slot.reset(new Body);
                Body &body = *slot;
                body.signature = signature;
                body.mesh.reset(new btTriangleMesh);
                for (const auto &f : s->polygons)
                {
                    // Use the existing ground controller's walkable-face rule.
                    // It owns support height and tip; walls and steep faces stay solid.
                    if (supported && f.B >= 0.6) continue;
                    for (int i = 1; i + 1 < f.num_vertices; ++i)
                    {
                        const int ia = f.v[0], ib = f.v[i], ic = f.v[i + 1];
                        if (ia < 0 || ib < 0 || ic < 0 || (size_t)ia >= s->POO.size() ||
                            (size_t)ib >= s->POO.size() || (size_t)ic >= s->POO.size()) continue;
                        const vec3d a = s->POO[ia], b = s->POO[ib], c = s->POO[ic];
                        const vec3d normal = (b - a) * (c - a);
                        if (normal.square() > 1e-12)
                        {
                            body.mesh->addTriangle(V(a), V(b), V(c), true);
                            body.triangleNormals.push_back(V(normal).normalized());
                        }
                    }
                }
                if (body.mesh->getNumTriangles() > 0)
                {
                    body.terrain.reset(new btBvhTriangleMeshShape(body.mesh.get(), true));
                    body.terrain->setMargin(0);
                    body.object.setCollisionShape(body.terrain.get());
                }
            }
            if (!slot->terrain) continue;
            slot->object.setWorldTransform(Pose(tile.pos, mat3x3::Ident()));
            result.push_back(slot.get());
        }
        return result;
    }
};
Scene::Scene(NC_STACK_ypaworld &world) : impl(new Impl(world)) {}
Scene::~Scene() = default;
bool Scene::TakeWorldContact(NC_STACK_ypabact *a, Contact *contact)
{
    const auto it = impl->actors.find(a);
    if (it == impl->actors.end() || it->second->stamp != impl->world._timeStamp ||
        !it->second->hasWorldContact) return false;
    Body &body = *it->second;
    if (contact) *contact = body.worldContact;
    body.hasWorldContact = false;
    return true;
}
bool Scene::TakeUnitContact(NC_STACK_ypabact *a, Contact *contact)
{
    const auto it = impl->actors.find(a);
    if (it == impl->actors.end() || it->second->stamp != impl->world._timeStamp ||
        !it->second->hasUnitContact || a->_shapeCollisionResponseStamp == impl->world._timeStamp)
        return false;
    Body &body = *it->second;
    if (contact) *contact = body.unitContact;
    body.hasUnitContact = false;
    a->_shapeCollisionResponseStamp = impl->world._timeStamp;
    return true;
}
std::shared_ptr<Shape> Scene::LoadShared(const std::string &path)
{
    auto it = impl->profiles.find(path);
    if (it != impl->profiles.end()) return it->second;
    std::string error;
    auto shape = Load(path, &error);
    if (!shape) ypa_log_out("WARNING: collision_shape %s failed (%s); existing collision retained.\n", path.c_str(), error.c_str());
    impl->profiles[path] = shape;
    return shape;
}
void Scene::Forget(NC_STACK_ypabact *a)
{
    for (auto &pair : impl->actors)
    {
        pair.second->pendingDamage.erase(a);
        if (pair.second->hasUnitContact && pair.second->unitContact.actor == a)
            pair.second->hasUnitContact = false;
    }
    auto it = impl->actors.find(a);
    if (it == impl->actors.end()) return;
    impl->detector.world.removeCollisionObject(&it->second->object);
    impl->actors.erase(it);
}
void Scene::UpdateActor(NC_STACK_ypabact *a) { impl->Frame(); impl->Update(a); }
void Scene::ResetActorPose(NC_STACK_ypabact *a)
{
    Forget(a);
    UpdateActor(a);
    const auto it = impl->actors.find(a);
    if (it == impl->actors.end()) return;
    Body &body = *it->second;
    // Explicit placement is a new starting pose, not a swept flight path.
    body.resolvedPosition = a->_position;
    body.resolvedRotation = a->_rotation;
    body.resolvedStamp = impl->world._timeStamp;
}
bool Scene::PairContact(NC_STACK_ypabact *a, NC_STACK_ypabact *b, Contact *out)
{
    if (!Live(a) || !Live(b) || Related(a, b) || (!a->HasCollisionShape() && !b->HasCollisionShape())) return false;
    impl->Frame(); impl->Update(a); impl->Update(b);
    const auto ia = impl->actors.find(a), ib = impl->actors.find(b);
    if (ia == impl->actors.end() || ib == impl->actors.end()) return false;
    ContactCollector result(&ia->second->object, HorizontalPair(a, b),
                            a->_bact_type == BACT_TYPES_ROBO || b->_bact_type == BACT_TYPES_ROBO);
    ContactPair(impl->detector, ia->second->object, ib->second->object, result);
    if (result.contacts.empty()) return false;
    if (out)
    {
        *out = *std::max_element(result.contacts.begin(), result.contacts.end(),
                                [](const Contact &x, const Contact &y) { return x.depth < y.depth; });
        out->actor = b;
    }
    return true;
}
std::vector<NC_STACK_ypabact *> Scene::ShapeTargets(const vec3d &from, const vec3d &to, double radius)
{
    impl->Frame();
    const btVector3 a = V(from), b = V(to), pad(radius, radius, radius);
    btVector3 lo = a, hi = a; lo.setMin(b); hi.setMax(b);
    std::vector<NC_STACK_ypabact *> result;
    for (Body *body : impl->Nearby(lo - pad, hi + pad))
    {
        auto *actor = impl->Actor(body);
        if (actor && Live(actor) && actor->HasCollisionShape()) result.push_back(actor);
    }
    // Stable gameplay IDs make traversal independent of pointer allocation.
    std::sort(result.begin(), result.end(), [](const NC_STACK_ypabact *a, const NC_STACK_ypabact *b) { return a->_gid < b->_gid; });
    return result;
}
bool Scene::Trace(NC_STACK_ypabact *target, const vec3d &from, const vec3d &to,
                  double radius, Contact *out)
{
    if (!target || !target->HasCollisionShape() || !Finite(from) || !Finite(to)) return false;
    btCollisionObject object;
    object.setCollisionShape(&target->_collisionShape->impl->compound);
    object.setWorldTransform(Pose(target->GetBodyPosition(), target->_rotation));
    Contact first; bool found = false;
    btSphereShape initialSphere(std::max(0.00001, radius));
    btCollisionObject initial;
    initial.setCollisionShape(&initialSphere);
    initial.setWorldTransform(Pose(from, mat3x3::Ident()));
    ContactCollector overlap(&initial);
    ContactPair(impl->detector, initial, object, overlap);
    if (!overlap.contacts.empty())
    {
        if (out) { *out = overlap.contacts.front(); out->fraction = 0; out->actor = target; }
        return true;
    }
    if (radius <= 0)
    {
        btCollisionWorld::ClosestRayResultCallback ray(V(from), V(to));
        btCollisionWorld::rayTestSingle(Pose(from, mat3x3::Ident()), Pose(to, mat3x3::Ident()),
                                        &object, object.getCollisionShape(), object.getWorldTransform(), ray);
        if (ray.hasHit()) { first.point = V(ray.m_hitPointWorld); first.normal = V(ray.m_hitNormalWorld); first.fraction = ray.m_closestHitFraction; found = true; }
    }
    else
    {
        btSphereShape sphere(radius);
        for (const auto &part : target->_collisionShape->impl->hulls)
        {
            Contact c;
            if (Cast(&sphere, Pose(from, mat3x3::Ident()), Pose(to, mat3x3::Ident()),
                     part.get(), object.getWorldTransform(), object.getWorldTransform(), &c) &&
                (!found || c.fraction < first.fraction)) { first = c; found = true; }
        }
    }
    if (found && out) { *out = first; out->actor = target; }
    return found;
}
bool Scene::TraceProjectile(NC_STACK_ypabact *projectile, NC_STACK_ypabact *target,
                            const mat3x3 &oldRotation, Contact *out)
{
    if (!projectile || !Live(target) || !target->HasCollisionShape()) return false;
    impl->Frame(); impl->Update(target);
    const Body &body = *impl->actors.at(target);
    std::vector<NC_STACK_ypabact::TCollisionSphereWorld> oldSpheres, newSpheres;
    projectile->GetCollisionSpheres(oldSpheres, projectile->_old_pos, oldRotation, false);
    projectile->GetCollisionSpheres(newSpheres, projectile->_position, projectile->_rotation, false);
    Contact first; bool found = false;
    for (size_t i = 0; i < std::min(oldSpheres.size(), newSpheres.size()); ++i)
    {
        Contact c;
        if (newSpheres[i].radius <= 0) continue;
        btSphereShape sphere(newSpheres[i].radius);
        btCollisionObject initial, oldTarget;
        initial.setCollisionShape(&sphere);
        initial.setWorldTransform(Pose(oldSpheres[i].center, mat3x3::Ident()));
        oldTarget.setCollisionShape(&target->_collisionShape->impl->compound);
        oldTarget.setWorldTransform(body.oldPose);
        ContactCollector overlap(&initial);
        ContactPair(impl->detector, initial, oldTarget, overlap);
        if (!overlap.contacts.empty())
        {
            first = overlap.contacts.front(); first.fraction = 0; first.actor = target; found = true;
            break;
        }
        for (auto &part : target->_collisionShape->impl->hulls)
            if (Cast(&sphere, Pose(oldSpheres[i].center, mat3x3::Ident()),
                     Pose(newSpheres[i].center, mat3x3::Ident()), part.get(),
                     body.oldPose, body.object.getWorldTransform(), &c) &&
                (!found || c.fraction < first.fraction))
            { first = c; first.actor = target; found = true; }
    }
    if (found && out) *out = first;
    return found;
}

bool Scene::Resolve(NC_STACK_ypabact *a, const vec3d &oldPosition,
                    const mat3x3 &oldRotation, int frameTime)
{
    // Guns keep their mount and aim, but their query bounds must follow both.
    if (Live(a) && a->_bact_type == BACT_TYPES_GUN)
    {
        UpdateActor(a);
        return false;
    }
    if (!Live(a) || !Vehicle(a) || !Finite(oldPosition) || !Finite(a->_position) ||
        (!a->getBACT_bactCollisions() && !a->HasCollisionShape())) return false;
    if (impl->stamp != impl->world._timeStamp) lastContacts.clear();
    impl->Frame(); impl->Update(a);
    const auto selfIt = impl->actors.find(a);
    if (selfIt == impl->actors.end()) return false;
    Body &self = *selfIt->second;
    vec3d fromPosition = oldPosition;
    mat3x3 fromRotation = oldRotation;
    // Move already corrected part of this update. Continue from that pose;
    // replaying the whole frame can block steering or undo ground alignment.
    if (self.resolvedStamp == impl->world._timeStamp &&
        (frameTime > 0 || (oldPosition - self.resolvedPosition).square() < 1e-10))
    {
        fromPosition = self.resolvedPosition;
        fromRotation = self.resolvedRotation;
    }
    const vec3d bodyOffset = a->GetBodyPosition() - a->_position;
    const btTransform start = Pose(fromPosition + bodyOffset, fromRotation);
    const btTransform end = self.object.getWorldTransform();
    const double radius = a->HasCollisionShape() ? a->_collisionShape->radius : a->GetCollisionBroadRadius();
    const double tolerance = a->HasCollisionShape() ? a->_collisionShape->tolerance : 0.1;
    const btVector3 pad(radius, radius, radius);
    btVector3 lo = start.getOrigin(), hi = lo; lo.setMin(end.getOrigin()); hi.setMax(end.getOrigin());
    auto nearby = impl->Nearby(lo - pad, hi + pad);
    std::vector<Body *> terrain;
    if (a->HasCollisionShape() && a->getBACT_exactCollisions())
        terrain = impl->Terrain(fromPosition + bodyOffset, a->GetBodyPosition(), radius, a->_status_flg & BACT_STFLAG_LAND);
    nearby.insert(nearby.end(), terrain.begin(), terrain.end());
    if (a->HasCollisionShape() && a->getBACT_bactCollisions())
        for (Body *body : nearby)
        {
            auto *other = body->actor;
            if (!other || !a->CanCollectPlasmaFrom(other)) continue;
            ContactCollector contact(&self.object);
            ContactPair(impl->detector, self.object, body->object, contact);
            bool touched = !contact.contacts.empty();
            if (!touched)
                for (size_t i = 0; i < self.parts.size() && !touched; ++i)
                    for (size_t j = 0; j < body->parts.size() && !touched; ++j)
                        touched = Cast(self.parts[i], start * self.local[i], end * self.local[i],
                            body->parts[j], body->oldPose * body->local[j],
                            body->object.getWorldTransform() * body->local[j], nullptr);
            if (touched) a->CollectPlasmaFrom(other);
        }
    impl->Order(nearby);
    auto allowed = [&](Body *body) {
        auto *other = impl->Actor(body);
        if (body == &self) return false;
        if (!other) return a->HasCollisionShape();
        return a->getBACT_bactCollisions() && Live(other) && !Related(a, other) &&
               !a->CanCollectPlasmaFrom(other) && (a->HasCollisionShape() || other->HasCollisionShape());
    };
    bool changed = false;
    std::set<NC_STACK_ypabact *> damageContacts;
    vec3d safePosition = fromPosition, desiredPosition = a->_position;
    mat3x3 safeRotation = fromRotation, desiredRotation = a->_rotation;
    mat3x3 rotationGoal = desiredRotation;
    bool resumeRotation = false;
    // Consume the tangential part of a blocked move, testing it again against
    // other surfaces. A bounded sweep avoids both wall sticking and tunnelling
    // through the second wall of a corner.
    for (int slide = 0; slide < 4; ++slide)
    {
        const btTransform slideFrom = Pose(safePosition + bodyOffset, safeRotation);
        const btTransform slideTo = Pose(desiredPosition + bodyOffset, desiredRotation);
        if (slide > 0)
        {
            btVector3 low = slideFrom.getOrigin(), high = low;
            low.setMin(slideTo.getOrigin()); high.setMax(slideTo.getOrigin());
            nearby = impl->Nearby(low - pad, high + pad);
            if (a->HasCollisionShape() && a->getBACT_exactCollisions())
            {
                auto geometry = impl->Terrain(safePosition + bodyOffset, desiredPosition + bodyOffset, radius, a->_status_flg & BACT_STFLAG_LAND);
                nearby.insert(nearby.end(), geometry.begin(), geometry.end());
            }
            impl->Order(nearby);
        }
        Contact first; bool hit = false;
        size_t firstPart = 0;
        std::vector<SweepBounds> selfBounds;
        selfBounds.reserve(self.parts.size());
        for (size_t i = 0; i < self.parts.size(); ++i)
            selfBounds.push_back(SweptBounds(self.parts[i], slideFrom * self.local[i], slideTo * self.local[i]));
        for (Body *body : nearby)
        {
            if (!allowed(body)) continue;
            SweepBounds terrainBounds;
            if (body->terrain)
                body->object.getCollisionShape()->getAabb(body->object.getWorldTransform(),
                                                        terrainBounds.minimum, terrainBounds.maximum);
            for (size_t i = 0; i < self.parts.size(); ++i)
            {
                Contact c; bool castHit = false;
                if (body->terrain)
                {
                    if (!TestAabbAgainstAabb2(selfBounds[i].minimum, selfBounds[i].maximum,
                                             terrainBounds.minimum, terrainBounds.maximum)) continue;
                    TerrainCastResult result(slideFrom.getOrigin(), slideTo.getOrigin());
                    const btTransform partFrom=slideFrom*self.local[i],partTo=slideTo*self.local[i];
                    // Later parts only need to beat the earliest blocking hit.
                    // Shortening their sweep also lets the mesh BVH discard
                    // interior parts before running an expensive convex cast.
                    const double limit=hit ? std::min(1.0,first.fraction+1e-6) : 1.0;
                    const btTransform partEnd(partFrom.getRotation().slerp(partTo.getRotation(),limit),
                                              partFrom.getOrigin().lerp(partTo.getOrigin(),limit));
                    CastTerrain(self.parts[i],partFrom,partEnd,body->object,result);
                    if (result.hasHit())
                    {
                        c.fraction = result.m_closestHitFraction*limit; c.normal = V(result.m_hitNormalWorld);
                        c.point = V(result.m_hitPointWorld); castHit = true;
                        if (result.triangle >= 0 && (size_t)result.triangle < body->triangleNormals.size())
                        {
                            // Pure angular triangle casts can return a simplex
                            // normal pointing into the wall. Use the actual face
                            // plane and orient it toward the casting convex part.
                            btVector3 minimum, maximum;
                            self.parts[i]->getAabb(btTransform::getIdentity(), minimum, maximum);
                            const btVector3 centre = slideFrom * self.local[i] * ((minimum + maximum) * .5);
                            btVector3 normal = body->object.getWorldTransform().getBasis() * body->triangleNormals[result.triangle];
                            if (normal.dot(centre - result.m_hitPointWorld) < 0) normal = -normal;
                            c.normal = V(normal);
                        }
                    }
                }
                else
                {
                    for (size_t j = 0; j < body->parts.size(); ++j)
                    {
                        // Reuse each part's complete motion bounds across hull pairs.
                        const auto &bounds = slide == 0 ? body->sweptParts[j] : body->currentParts[j];
                        if (!TestAabbAgainstAabb2(selfBounds[i].minimum, selfBounds[i].maximum,
                                                 bounds.minimum, bounds.maximum)) continue;
                        Contact pair;
                        const btTransform aFrom=slideFrom*self.local[i], aTo=slideTo*self.local[i];
                        const btTransform bFrom=(slide == 0 ? body->oldPose : body->object.getWorldTransform())*body->local[j];
                        const btTransform bTo=body->object.getWorldTransform()*body->local[j];
                        if ((aTo.getOrigin()-aFrom.getOrigin()).length2()<1e-16 &&
                            (bTo.getOrigin()-bFrom.getOrigin()).length2()<1e-16 &&
                            aFrom.getRotation().angleShortestPath(aTo.getRotation())<1e-8 &&
                            bFrom.getRotation().angleShortestPath(bTo.getRotation())<1e-8) continue;
                        const double limit=std::min(hit?first.fraction+1e-6:1.0,
                                                    castHit?c.fraction+1e-6:1.0);
                        const btTransform aEnd(aFrom.getRotation().slerp(aTo.getRotation(),limit),aFrom.getOrigin().lerp(aTo.getOrigin(),limit));
                        const btTransform bEnd(bFrom.getRotation().slerp(bTo.getRotation(),limit),bFrom.getOrigin().lerp(bTo.getOrigin(),limit));
                        if (Cast(self.parts[i],aFrom,aEnd,body->parts[j],bFrom,bEnd,&pair))
                        {
                            pair.fraction*=limit;
                            if (!castHit || pair.fraction<c.fraction) { c=pair; castHit=true; }
                        }
                    }
                }
                if (castHit && HorizontalPair(a, body->actor))
                {
                    // A sloped hull face must not turn a grounded vehicle
                    // contact into a vertical impulse or a support correction.
                    c.normal.y = 0;
                    if (c.normal.normalise() < 1e-6) castHit = false;
                }
                // Resting contact must allow escape, but a sweep starting at
                // contact still has to stop a corner rotating into the surface.
                const vec3d relativeMove = desiredPosition - safePosition -
                    (slide == 0 && body->actor ? V(body->object.getWorldTransform().getOrigin() - body->oldPose.getOrigin()) : vec3d(0, 0, 0));
                const double rotationChange = (safeRotation.AxisX() - desiredRotation.AxisX()).length() +
                                              (safeRotation.AxisY() - desiredRotation.AxisY()).length();
                const bool angularEntry = castHit && rotationChange > 1e-5 &&
                    (c.fraction > 1e-5 || RotationEntersPlane(self.parts[i], slideFrom * self.local[i],
                                                             slideTo * self.local[i], relativeMove, c.normal));
                if (castHit && (relativeMove.dot(c.normal) < -1e-7 || angularEntry) &&
                    (!hit || c.fraction < first.fraction))
                { first = c; first.actor = impl->Actor(body); firstPart = i; hit = true; }
            }
        }
        if (!hit)
        {
            a->_position = desiredPosition; a->_rotation = desiredRotation;
            if (resumeRotation)
            {
                safePosition = desiredPosition; safeRotation = desiredRotation;
                desiredRotation = rotationGoal; resumeRotation = false;
                continue;
            }
            break;
        }
        const vec3d motion = desiredPosition - safePosition;
        const btQuaternion fromRotation = slideFrom.getRotation(), toRotation = slideTo.getRotation();
        const double travel = motion.length() + radius * fromRotation.angleShortestPath(toRotation);
        double advance = std::max(0.0, first.fraction - tolerance / std::max(1.0, travel));
        if (!first.actor && fromRotation.angleShortestPath(toRotation) > 1e-5)
        {
            const btVector3 normal=V(first.normal);
            const btTransform partFrom=slideFrom*self.local[firstPart];
            const btVector3 support=partFrom*self.parts[firstPart]->localGetSupportingVertex(
                partFrom.getBasis().transpose()*-normal);
            const double gap=std::max(0.0,(V(support)-first.point).dot(first.normal)-tolerance);
            double lower=0,upper=advance;
            // Check the arc against the actual hit face. This also keeps
            // a nearly touching angular cast from accepting a late impact.
            for (int iteration=0;iteration<18;++iteration)
            {
                const double middle=(lower+upper)*.5;
                const double entry=RotationClearance(self.parts[firstPart],self.local[firstPart],
                    slideFrom,fromRotation.slerp(toRotation,middle),first.normal)-
                    std::min(0.0,motion.dot(first.normal)*middle);
                if (entry<=gap) lower=middle; else upper=middle;
            }
            advance=lower;
        }
        a->_position = safePosition + motion * advance;
        if (GroundDrive(a) || HorizontalPair(a, first.actor)) a->_position.y = desiredPosition.y;
        a->_rotation = Rotation(fromRotation.slerp(toRotation, advance));
        vec3d velocity = a->_fly_dir * a->_fly_dir_length;
        const double entering = velocity.dot(first.normal);
        if (first.actor)
        {
            self.touchedUnit = true;
            if (entering < -1e-6 && (!self.hasUnitContact ||
                entering < self.unitContact.incomingVelocity.dot(self.unitContact.normal)))
            {
                first.incomingVelocity = velocity;
                self.unitContact = first;
                self.hasUnitContact = true;
            }
        }
        if (!first.actor && entering < -1e-6 &&
            (!self.hasWorldContact || entering < self.worldContact.incomingVelocity.dot(self.worldContact.normal)))
        {
            first.incomingVelocity = velocity;
            self.worldContact = first;
            self.hasWorldContact = true;
        }
        if (entering < 0) velocity -= first.normal * entering;
        SetVelocity(a, velocity);
        NotifyUnitContact(a, first.actor, first.normal);
        if (first.actor) damageContacts.insert(first.actor);
        if (lastContacts.size() < 128) lastContacts.push_back(first);
        changed = true;
        vec3d remaining = desiredPosition - a->_position;
        const double intoPlane = remaining.dot(first.normal);
        if (intoPlane < 0) remaining -= first.normal * intoPlane;
        const double clearance = RotationClearance(self.parts[firstPart], self.local[firstPart],
            Pose(a->GetBodyPosition(), a->_rotation), toRotation, first.normal);
        if (clearance > 1e-6)
        {
            vec3d outward = first.normal;
            if (GroundDrive(a)) outward.y = 0;
            const double projection = outward.dot(first.normal);
            if (projection > 1e-6)
            {
                // Sweep the outward move before retrying the remaining turn.
                // Other vehicles and the second wall of a corner still block it.
                // The vanilla wall response nudges by 10 units. Spend at most
                // that amount per actor frame, instead of jumping by the full
                // clearance of a large requested turn.
                const double step = std::min((clearance + tolerance) / projection,
                                             std::max(0.0, 10.0 - self.clearanceUsed));
                if (step > 1e-6)
                {
                    if (step*projection < clearance+tolerance)
                    {
                        const btTransform atContact=Pose(a->GetBodyPosition(),a->_rotation);
                        const auto rotation=atContact.getRotation();
                        double lower=0,upper=1;
                        for (int iteration=0;iteration<18;++iteration)
                        {
                            const double middle=(lower+upper)*.5;
                            if (RotationClearance(self.parts[firstPart],self.local[firstPart],
                                atContact,rotation.slerp(toRotation,middle),first.normal)<=
                                std::max(0.0,step*projection-tolerance)) lower=middle;
                            else upper=middle;
                        }
                        rotationGoal=Rotation(rotation.slerp(toRotation,lower));
                    }
                    remaining += outward * step;
                    self.clearanceUsed += step;
                    resumeRotation = true;
                }
            }
        }
        if (remaining.length() < 1e-6) break;
        safePosition = a->_position;
        safeRotation = a->_rotation;
        desiredPosition = safePosition + remaining;
        if (resumeRotation) desiredPosition = ConstrainPosition(a, desiredPosition);
        desiredRotation = safeRotation;
    }
    // Resolve all independent contact planes, rather than deriving a push
    // direction from one sphere pair or from a cancelling average of centres.
    for (int pass = 0; pass < 8; ++pass)
    {
        self.object.setWorldTransform(Pose(a->GetBodyPosition(), a->_rotation));
        if (pass > 0)
        {
            const btVector3 p = self.object.getWorldTransform().getOrigin();
            nearby = impl->Nearby(p - pad, p + pad);
            if (a->HasCollisionShape() && a->getBACT_exactCollisions())
            {
                auto geometry = impl->Terrain(a->GetBodyPosition(), a->GetBodyPosition(), radius, a->_status_flg & BACT_STFLAG_LAND);
                nearby.insert(nearby.end(), geometry.begin(), geometry.end());
            }
            impl->Order(nearby);
        }
        std::vector<Contact> contacts;
        for (Body *body : nearby)
        {
            if (!allowed(body)) continue;
            ContactCollector result(&self.object, HorizontalPair(a, body->actor),
                a->_bact_type == BACT_TYPES_ROBO || (body->actor && body->actor->_bact_type == BACT_TYPES_ROBO));
            ContactPair(impl->detector, self.object, body->object, result);
            for (auto c : result.contacts)
            {
                c.actor = impl->Actor(body);
                if (c.depth > tolerance) contacts.push_back(c);
                if (c.actor) { damageContacts.insert(c.actor); self.touchedUnit = true; }
            }
        }
        if (contacts.empty()) break;
        std::sort(contacts.begin(), contacts.end(), [](const Contact &x, const Contact &y) { return x.depth > y.depth; });
        vec3d correction(0, 0, 0);
        for (const Contact &c : contacts)
        {
            const double need = c.depth - tolerance - correction.dot(c.normal);
            if (need > 0) correction += c.normal * need;
        }
        if (correction.length() <= 1e-7) break;
        a->_position = ConstrainPosition(a, a->_position + correction);
        vec3d velocity = a->_fly_dir * a->_fly_dir_length;
        const Contact *blocked = nullptr;
        for (const Contact &c : contacts)
            if (velocity.dot(c.normal) < 0)
            {
                if (c.actor && velocity.dot(c.normal) < -1e-6 &&
                    (!self.hasUnitContact || velocity.dot(c.normal) < self.unitContact.incomingVelocity.dot(self.unitContact.normal)))
                {
                    self.unitContact = c;
                    self.unitContact.incomingVelocity = velocity;
                    self.hasUnitContact = true;
                }
                if (!c.actor && velocity.dot(c.normal) < -1e-6 &&
                    (!self.hasWorldContact || velocity.dot(c.normal) < self.worldContact.incomingVelocity.dot(self.worldContact.normal)))
                {
                    self.worldContact = c;
                    self.worldContact.incomingVelocity = velocity;
                    self.hasWorldContact = true;
                }
                velocity -= c.normal * velocity.dot(c.normal);
                if (!blocked && c.actor) blocked = &c;
            }
        SetVelocity(a, velocity);
        if (blocked) NotifyUnitContact(a, blocked->actor, blocked->normal);
        for (const auto &c : contacts)
            if (lastContacts.size() < 128) lastContacts.push_back(c);
        changed = true;
    }
    // Move can stop at the CCD skin before shared Update handles gameplay.
    // Carry that real contact to Update even if the corrected bodies no
    // longer overlap; the frame boundary and Forget clear stale actors.
    self.pendingDamage.insert(damageContacts.begin(), damageContacts.end());
    if (frameTime > 0)
    {
        impl->Update(a);
        Contact unit;
        if (TakeUnitContact(a, &unit)) a->HandleShapeUnitContact(unit, frameTime);
        if (!self.touchedUnit) a->_status_flg &= ~BACT_STFLAG_BCRASH;
        Contact impact;
        if (TakeWorldContact(a, &impact)) a->HandleShapeWorldCollision(impact);
    }
    self.resolvedPosition = a->_position;
    self.resolvedRotation = a->_rotation;
    self.resolvedStamp = impl->world._timeStamp;
    if (frameTime > 0 && a->_shapeCollisionDamageStamp != impl->world._timeStamp)
    {
        const std::vector<NC_STACK_ypabact *> contacts(self.pendingDamage.begin(), self.pendingDamage.end());
        if (!contacts.empty()) a->_shapeCollisionDamageStamp = impl->world._timeStamp;
        self.pendingDamage.clear();
        // Gameplay damage can retire actors and call Forget. Do not iterate
        // a cache-owned set across that callback.
        for (auto *other : contacts)
            if (Live(a) && Live(other)) a->HandleUnitCollisionContact(other, frameTime);
    }
    impl->Update(a);
    return changed;
}
}
