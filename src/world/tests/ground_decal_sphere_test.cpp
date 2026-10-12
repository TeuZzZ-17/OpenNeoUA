#include "../grounddecal_contact.h"
#include <cstdio>
#include <limits>

namespace
{
int checks = 0, failures = 0;
void Check(bool result, const char *label)
{
    ++checks;
    if ( !result ) { ++failures; std::printf("FAIL: %s\n", label); }
}
bool Touches(const vec3d &center, double radius,
             const vec3d &a, const vec3d &b, const vec3d &c)
{
    return World::GroundDecalSphereTouchesPoint(center, radius,
        World::GroundDecalClosestTrianglePoint(center, a, b, c));
}
}

int main()
{
    const vec3d a(-100, 0, -100), b(100, 0, -100), c(0, 0, 100);
    Check(Touches(vec3d(0, -10, 0), 10, a, b, c), "flat terrain exact tangency");
    Check(Touches(vec3d(0, -9.999, 0), 10, a, b, c), "flat terrain tiny overlap");
    Check(!Touches(vec3d(0, -10.001, 0), 10, a, b, c), "flat terrain tiny gap");
    Check(!Touches(vec3d(0, -11, 0), 10, a, b, c), "air explosion out of reach");
    Check(Touches(vec3d(0, 0, 0), 10, a, b, c), "center on terrain");
    Check(Touches(vec3d(0, 5, 0), 10, a, b, c), "sphere crossing terrain from below");
    Check(!Touches(vec3d(0, 0, 0), 0, a, b, c), "zero radius inactive");
    Check(!Touches(vec3d(0, 0, 0), -1, a, b, c), "negative radius inactive");
    Check(!Touches(vec3d(0, 0, 0), std::numeric_limits<double>::infinity(), a, b, c), "infinite radius inactive");
    Check(!Touches(vec3d(0, 0, 0), std::numeric_limits<double>::quiet_NaN(), a, b, c), "NaN radius inactive");

    const vec3d edgeA(10, 0, -20), edgeB(10, 0, 20), edgeC(50, 0, 0);
    Check(Touches(vec3d(0, 0, 0), 10, edgeA, edgeB, edgeC), "side of sphere touches triangle edge");
    Check(!Touches(vec3d(-0.001, 0, 0), 10, edgeA, edgeB, edgeC), "triangle edge tiny gap");
    Check(Touches(vec3d(9, -4, -20), std::sqrt(17.0), edgeA, edgeB, edgeC), "vertex contact");
    Check(!Touches(vec3d(0, -1, 0), 5, edgeA, edgeB, edgeC), "near plane but outside finite polygon");

    const vec3d slopeA(-100, -100, -100), slopeB(100, 100, -100), slopeC(0, 0, 100);
    Check(Touches(vec3d(0, -10, 0), 8, slopeA, slopeB, slopeC), "slope touched sideways with vertical gap greater than radius");
    Check(!Touches(vec3d(0, -10, 0), 7, slopeA, slopeB, slopeC), "slope out of sphere");
    Check(Touches(vec3d(0, -10, 0), 8, slopeC, slopeB, slopeA), "reversed polygon winding");
    const vec3d closest = World::GroundDecalClosestTrianglePoint(vec3d(0, -10, 0), slopeA, slopeB, slopeC);
    Check((closest - vec3d(-5, -5, 0)).length() < 1e-10, "nearest decal anchor lies on slope");
    Check(Touches(vec3d(0, -1, 0), 1, vec3d(-10, 0, 0), vec3d(10, 0, 0), vec3d(0, 0, 0)), "collapsed triangle handled");
    Check(Touches(vec3d(0, -1, 0), 1, vec3d(0, 0, 0), vec3d(0, 0, 0), vec3d(0, 0, 0)), "collapsed point handled");
    const vec3d translation(12000, -800, -12000);
    Check(Touches(translation + vec3d(0, -9.999, 0), 10, a + translation, b + translation, c + translation), "world-coordinate translation");
    Check(!Touches(vec3d(std::numeric_limits<double>::quiet_NaN(), 0, 0), 10, a, b, c), "invalid center rejected");
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
