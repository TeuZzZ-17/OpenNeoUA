// Checks the crater shape and the lattice cut on their own (no engine needed).
//   g++ -std=gnu++14 -I.. world/tests/terrain_crater_core_test.cpp
#include <cstdio>
#include <map>
#include <set>
#include <tuple>
#include <utility>

#include "../terraincrater_core.h"

using namespace World::Crater;

namespace
{

int checks = 0;
int failures = 0;

void Check(bool ok, const char *label)
{
    ++checks;
    if ( !ok )
    {
        ++failures;
        std::printf("FAIL %s\n", label);
    }
}

Point At(double x, double y, double z, float u = 0.0f, float v = 0.0f)
{
    Point p;
    p.x = x; p.y = y; p.z = z; p.u = u; p.v = v;
    return p;
}

// A flat 300x300 sector piece, as the game stores it: one polygon.
Polygon Piece(double x0, double z0, double y = 0.0)
{
    return {At(x0, y, z0, 0, 0), At(x0 + 300, y, z0, 1, 0),
            At(x0 + 300, y, z0 + 300, 1, 1), At(x0, y, z0 + 300, 0, 1)};
}

}

int main()
{
    // ------------------------------------------------------------ shape ---
    const Shape crater = Make(150, 0, 150, 100, 30, 0.2);
    Check(Valid(crater), "valid crater");
    Check(std::fabs(ShapeDrop(crater, 150, 150, 0) - 30.0) < 1e-6, "centre goes down by the depth");
    Check(ShapeDrop(crater, 150 + 160, 150, 0) == 0.0, "nothing changes past the reach");
    Check(ShapeDrop(crater, 150 + 100, 150, 0) < 0.0, "the rim is raised at the edge");
    Check(std::fabs(ShapeDrop(crater, 150 + 100, 150, 0) + 30 * 0.2) < 0.5, "rim height is rim * depth");
    Check(ShapeDrop(crater, 150, 150, -200) == 0.0, "a roof 200 above the impact stays as it is");

    const Shape tooDeep = Make(0, 0, 0, 100, 500, 0);
    Check(std::fabs(tooDeep.depth - 50.0) < 1e-9, "depth limited to half the radius");
    Check(!Valid(Make(0, 0, 0, 0, 30, 0)), "zero radius makes no crater");
    Check(!Valid(Make(0, 0, 0, 100, -5, 0)), "negative depth makes no crater");
    Check(!Valid(Make(0, 0, 0, NAN, 30, 0)), "not a number makes no crater");
    Check(Make(0, 0, 0, 100, 30, 7).rim == 1.0, "rim limited to the depth");

    std::vector<Shape> pile(5, Make(0, 0, 0, 100, 40, 0));
    Check(std::fabs(Drop(pile, 0, 0, 0) - 70.0) < 1e-6, "repeated hits stop at 1.75 times the deepest");

    // The exact slope matches the slope measured between two close points.
    std::vector<Shape> slopeCraters = {crater, Make(230, 0, 120, 90, 25, 0.3)};
    double worstSlope = 0.0;
    for (double x = 40; x < 330; x += 7.3)
        for (double z = 40; z < 300; z += 9.1)
        {
            const Sample s = SampleAt(slopeCraters, x, z, 0);
            const double h = 0.01;
            const double fx = (Drop(slopeCraters, x + h, z, 0) - Drop(slopeCraters, x - h, z, 0)) / (2 * h);
            const double fz = (Drop(slopeCraters, x, z + h, 0) - Drop(slopeCraters, x, z - h, 0)) / (2 * h);
            worstSlope = std::max(worstSlope, std::max(std::fabs(s.slopeX - fx), std::fabs(s.slopeZ - fz)));
        }
    Check(worstSlope < 1e-3, "exact slope matches the measured slope");

    std::vector<Shape> one(1, crater);
    Check(std::fabs(Shade(one, 2000, 2000, 0) - 1.0) < 1e-9, "flat ground keeps its colour");
    Check(Shade(one, 150, 150, 0) < 0.9, "bottom of the crater is darker");

    // ---------------------------------------------------------- cutting ---
    std::vector<Polygon> pieces;
    CutOnLattice(Piece(0, 0), VISUAL_STEP, &pieces);
    Check(pieces.size() == 256, "a 300 piece becomes 16 x 16 cells");

    double area = 0.0;
    for (const Polygon &p : pieces)
        area += Area(p) * 0.5;
    Check(std::fabs(area - 90000.0) < 1e-3, "cutting keeps the whole surface");

    // Texture positions follow the surface: the centre cell sits at uv 0.5.
    bool uvRight = true;
    for (const Polygon &p : pieces)
        for (const Point &q : p)
            uvRight = uvRight && std::fabs(q.u - q.x / 300.0) < 1e-5 && std::fabs(q.v - q.z / 300.0) < 1e-5;
    Check(uvRight, "texture positions are interpolated exactly");

    // Two neighbour pieces: the points on their common edge must be the same
    // after the crater, or the ground would show a crack.
    const std::vector<Shape> onEdge(1, Make(300, 0, 150, 120, 40, 0.25));
    std::vector<Polygon> left, right;
    CutOnLattice(Piece(0, 0), VISUAL_STEP, &left);
    CutOnLattice(Piece(300, 0), VISUAL_STEP, &right);

    std::set<std::pair<long, long>> leftEdge, rightEdge;
    for (const Polygon &p : left)
        for (const Point &q : p)
            if ( std::fabs(q.x - 300.0) < 1e-9 )
                leftEdge.insert({std::lround(q.z * 1000), std::lround(Drop(onEdge, q.x, q.z, q.y) * 1000)});
    for (const Polygon &p : right)
        for (const Point &q : p)
            if ( std::fabs(q.x - 300.0) < 1e-9 )
                rightEdge.insert({std::lround(q.z * 1000), std::lround(Drop(onEdge, q.x, q.z, q.y) * 1000)});
    Check(!leftEdge.empty() && leftEdge == rightEdge, "neighbour pieces share every point of their edge");

    // A wall standing exactly on a lattice line is kept once, not twice.
    const Polygon wall = {At(150, 0, 0), At(150, 0, 75), At(150, -100, 75), At(150, -100, 0)};
    CutOnLattice(wall, VISUAL_STEP, &pieces);
    double wallArea = 0.0;
    for (const Polygon &p : pieces)
        wallArea += Area(p) * 0.5;
    Check(std::fabs(wallArea - 7500.0) < 1e-3, "a wall on a lattice line is kept once");

    // Winding is kept, so collision polygons still face the same way.
    CutOnLattice(Piece(0, 0), COLLISION_STEP, &pieces);
    bool windingKept = !pieces.empty();
    for (const Polygon &p : pieces)
    {
        const double cross = (p[1].x - p[0].x) * (p[2].z - p[1].z) - (p[1].z - p[0].z) * (p[2].x - p[1].x);
        windingKept = windingKept && cross > 0.0;
    }
    Check(windingKept && pieces.size() == 64, "collision cut keeps the winding, 8 x 8 cells");

    // Only surfaces in reach are cut.
    const std::vector<Shape> small(1, Make(150, 0, 150, 40, 15, 0));
    Check(Touches(small, Piece(0, 0)), "the hit piece is touched");
    Check(!Touches(small, Piece(600, 600)), "a far piece is not touched");
    Check(!Touches(small, Piece(0, 0, -400)), "a roof far above is not touched");

    // Shared points: the 16 x 16 cut of a piece has 17 x 17 distinct points.
    CutOnLattice(Piece(0, 0), VISUAL_STEP, &pieces);
    PointIndex index;
    uint32_t next = 0;
    for (const Polygon &p : pieces)
        for (const Point &q : p)
            index.Find(q, [&](const Point &) { return next++; });
    Check(next == 17 * 17, "points on cell borders are shared");

    // ------------------------------------------------------------- grid ---
    // The grid must hand every polygon a query can meet, and only nearby ones.
    CutOnLattice(Piece(0, 0), COLLISION_STEP, &pieces);
    std::vector<PolygonGrid::Box> boxes;
    for (const Polygon &p : pieces)
    {
        PolygonGrid::Box box = {1e30, 1e30, -1e30, -1e30};
        for (const Point &q : p)
        {
            box.minX = std::min(box.minX, q.x); box.maxX = std::max(box.maxX, q.x);
            box.minZ = std::min(box.minZ, q.z); box.maxZ = std::max(box.maxZ, q.z);
        }
        boxes.push_back(box);
    }
    PolygonGrid grid;
    grid.Build(boxes, COLLISION_STEP * 2.0);

    bool allFound = true, fewVisited = true;
    for (double x = 0.5; x < 300.0; x += 13.7)
        for (double z = 0.5; z < 300.0; z += 11.3)
        {
            std::set<uint32_t> seen;
            int visits = 0;
            grid.Query(x, z, x, z, [&](uint32_t id) { seen.insert(id); ++visits; });
            for (size_t id = 0; id < boxes.size(); ++id)
                if ( x >= boxes[id].minX && x <= boxes[id].maxX && z >= boxes[id].minZ && z <= boxes[id].maxZ )
                    allFound = allFound && seen.count((uint32_t)id);
            fewVisited = fewVisited && visits <= 32 && visits == (int)seen.size();
        }
    Check(allFound, "the grid finds every polygon under a vertical ray");
    Check(fewVisited, "a vertical ray visits few polygons, each once");

    // A point exactly on a square border still finds both sides.
    std::set<uint32_t> border;
    grid.Query(75.0, 75.0, 75.0, 75.0, [&](uint32_t id) { border.insert(id); });
    bool bothSides = false;
    for (uint32_t id : border)
        bothSides = bothSides || boxes[id].maxX <= 75.0 + 1e-9;
    Check(bothSides, "a point on a square border sees the polygons on both sides");

    int visited = 0;
    std::set<uint32_t> every;
    grid.Query(-5000, -5000, 5000, 5000, [&](uint32_t id) { ++visited; every.insert(id); });
    Check(visited == (int)boxes.size() && every.size() == boxes.size(),
          "an area over the whole piece visits every polygon exactly once");

    // A long ray over a big grid asks for the full scan instead.
    std::vector<PolygonGrid::Box> wide;
    for (int i = 0; i < 40; ++i)
        wide.push_back({i * 75.0, 0.0, i * 75.0 + 75.0, 75.0 * 40});
    PolygonGrid big;
    big.Build(wide, COLLISION_STEP * 2.0);
    visited = 0;
    Check(!big.Query(0, 0, 3000, 3000, [&](uint32_t) { ++visited; }) && visited == 0,
          "a huge area asks for the full scan instead");

    // ------------------------------------------------------------ dents ---
    // A roof hit at y = -300 (300 above the ground), pushed down into the building.
    const Shape dent = MakeDent(150, -300, 150, 100, 30, 0, 5, 0);
    Check(Valid(dent) && dent.kind == Shape::DENT && std::fabs(dent.ny - 1.0) < 1e-12,
          "dent made, its direction made unit length");
    Check(!Valid(MakeDent(0, 0, 0, 100, 30, 0, 0, 0)), "a dent without a direction is refused");
    Check(MakeDent(0, 0, 0, 100, 500, 1, 0, 0).depth == 50.0, "dent depth limited to half the radius");

    const std::vector<Shape> dents(1, dent);
    const Sample centre = SampleAt(dents, 150, 150, -300);
    Check(centre.OffsetY() > 30 * (1.0 - CRUMPLE) - 1e-9 && centre.OffsetY() < 30 * (1.0 + CRUMPLE) + 1e-9,
          "the centre goes into the building by about the depth");
    Check(centre.drop == 0.0, "a dent is not a crater in the ground");
    const Sample outside = SampleAt(dents, 150, 150, -300 + 101);
    Check(outside.OffsetX() == 0.0 && outside.OffsetY() == 0.0 && outside.OffsetZ() == 0.0 &&
          outside.dentLight == 1.0, "nothing moves outside the dent");
    Check(Drop(dents, 150, 150, 0) == 0.0, "the ground 300 below the roof stays as it is");
    const Sample again = SampleAt(dents, 150, 150, -300);
    Check(again.OffsetX() == centre.OffsetX() && again.OffsetY() == centre.OffsetY() &&
          again.OffsetZ() == centre.OffsetZ(), "the folds are the same every time");
    Check(centre.dentLight < 1.0, "inside the dent is darker");

    // Folds: points of the dent do not all move the same way.
    double lowest = 1e9, highest = -1e9, sideways = 0.0;
    for (double x = 100; x <= 200; x += 6.25)
    {
        const Sample s = SampleAt(dents, x, 150, -300);
        const double d = std::fabs(x - 150) / 100.0;
        const double smooth = 30 * (1 - d * d) * (1 - d * d);
        if ( smooth > 1.0 )
        {
            lowest = std::min(lowest, s.OffsetY() / smooth);
            highest = std::max(highest, s.OffsetY() / smooth);
        }
        sideways = std::max(sideways, std::fabs(s.OffsetX()) + std::fabs(s.OffsetZ()));
    }
    Check(highest - lowest > 0.2 && sideways > 0.5, "the dent is crumpled, not smooth");

    const std::vector<Shape> manyDents(6, dent);
    const Sample pile6 = SampleAt(manyDents, 150, 150, -300);
    const double moved6 = std::sqrt(pile6.OffsetX() * pile6.OffsetX() + pile6.OffsetY() * pile6.OffsetY() +
                                    pile6.OffsetZ() * pile6.OffsetZ());
    Check(moved6 <= 1.75 * 30 * (1.0 + CRUMPLE) + 1e-6, "repeated hits stop at 1.75 times the strongest dent");

    // A wall 300 wide and 600 high, hit in its middle: it is cut in height only
    // near the dent, and the whole wall is still there.
    const Polygon tallWall = {At(0, 0, 50), At(300, 0, 50), At(300, -600, 50), At(0, -600, 50)};
    const std::vector<Shape> wallDent(1, MakeDent(150, -300, 50, 90, 25, 0, 0, 1));
    double from = 1.0, to = 0.0;
    Check(DentHeights(wallDent, tallWall, &from, &to) && from == -390 && to == -210,
          "the dent asks for height cuts around itself only");
    CutOnLattice(tallWall, VISUAL_STEP, &pieces, from, to);
    double tallArea = 0.0, tallestPiece = 0.0;
    for (const Polygon &p : pieces)
    {
        tallArea += Area(p) * 0.5;
        double top = p[0].y, bottom = p[0].y;
        for (const Point &q : p)
        {
            top = std::min(top, q.y);
            bottom = std::max(bottom, q.y);
        }
        if ( bottom <= to && top >= from )
            tallestPiece = std::max(tallestPiece, bottom - top);
    }
    Check(std::fabs(tallArea - 180000.0) < 1e-3, "the cut wall keeps its whole surface");
    Check(tallestPiece <= 2 * VISUAL_STEP + 1e-9, "near the dent the wall is in small pieces");
    Check(pieces.size() < 16 * 33, "far from the dent the wall is not cut in height");

    // Two walls meeting at a corner, both near the dent: they get the very same
    // points on their common edge, so the dented corner does not open.
    const Polygon wallA = {At(0, 0, 0), At(300, 0, 0), At(300, -300, 0), At(0, -300, 0)};
    const Polygon wallB = {At(300, 0, 0), At(300, 0, 300), At(300, -300, 300), At(300, -300, 0)};
    const std::vector<Shape> cornerDent(1, MakeDent(290, -150, 10, 120, 35, -0.7, 0, 0.7));
    std::set<std::tuple<long, long, long>> edgeA, edgeB;
    for (int side = 0; side < 2; ++side)
    {
        const Polygon &wall = side ? wallB : wallA;
        double lo = 1.0, hi = 0.0;
        DentHeights(cornerDent, wall, &lo, &hi);
        CutOnLattice(wall, VISUAL_STEP, &pieces, lo, hi);
        for (const Polygon &p : pieces)
            for (const Point &q : p)
                if ( std::fabs(q.x - 300) < 1e-9 && std::fabs(q.z) < 1e-9 )
                {
                    const Sample s = SampleAt(cornerDent, q.x, q.z, q.y);
                    (side ? edgeB : edgeA).insert(std::make_tuple(std::lround((q.x + s.OffsetX()) * 1000),
                                                                  std::lround((q.y + s.OffsetY()) * 1000),
                                                                  std::lround((q.z + s.OffsetZ()) * 1000)));
                }
    }
    Check(edgeA.size() > 2 && edgeA == edgeB, "a dented corner stays closed");

    Check(Touches(dents, Piece(100, 100, -300)), "a roof near the dent is touched");
    Check(!Touches(dents, Piece(100, 100, 0)), "the ground far below the dent is not touched");

    DeformationLimits limits;
    limits.ground = 1000; limits.building = 40;
    limits.buildings.push_back({-100, -100, 100, 100});
    Sample large; large.drop = 200; large.moveX = 300; large.moveY = 100;
    const Sample bounded = limits.Apply(large, 0, 0);
    const auto length = [](const Sample &v) { return std::sqrt(v.OffsetX()*v.OffsetX() + v.OffsetY()*v.OffsetY() + v.OffsetZ()*v.OffsetZ()); };
    Check(std::fabs(length(bounded) - 40) < 1e-8, "building caps combined displacement");
    Check(length(limits.Apply(large, 300, 0)) > 40, "open ground keeps its separate cap");
    double previous = 40;
    for (int x = 100; x <= 250; ++x) {
        const double cap = limits.At(x, 0, 1000);
        Check(cap >= previous - 1e-8 && cap <= 1000, "foundation transition is continuous and monotonic");
        previous = cap;
    }
    limits.building = 0;
    Check(length(limits.Apply(large, 0, 0)) == 0, "zero building limit freezes foundations and dents");
    limits.ground = 0; limits.building = 40;
    const Sample noGround = limits.Apply(large, 0, 0);
    Check(noGround.drop == 0 && length(noGround) > 0 && length(noGround) <= 40.000001, "zero ground keeps building dents enabled");
    DeformationLimits absent;
    Check(length(absent.Apply(large, 0, 0)) == length(large), "missing limits preserve the old shape");

    // Map border pieces: nothing moves on them, the ground next to them fades
    // back smoothly over the footprint band, with or without other limits.
    DeformationLimits mapEdge;
    mapEdge.frozen.push_back({0, -1200, 1200, 0});
    Check(length(mapEdge.Apply(large, 600, -600)) == 0, "nothing moves on a map border piece");
    Check(length(mapEdge.Apply(large, 1200 + FOOTPRINT_BLEND, -600)) == length(large),
          "past the band the border leaves the crater whole");
    double before = 0;
    bool smooth = true;
    for (int x = 1200; x <= 1200 + (int)FOOTPRINT_BLEND; ++x) {
        const double moved = length(mapEdge.Apply(large, x, -600));
        smooth = smooth && moved >= before - 1e-8 && moved - before < 15.0;
        before = moved;
    }
    Check(smooth, "the crater fades in continuously next to the border");
    DeformationLimits buildingOnly;
    buildingOnly.ground = 1000; buildingOnly.building = 40;
    buildingOnly.buildings.push_back({1300, -700, 1400, -500});
    DeformationLimits both = buildingOnly;
    both.frozen = mapEdge.frozen;
    const double together = length(both.Apply(large, 1250, -600));
    Check(together > 0.0 && together < length(buildingOnly.Apply(large, 1250, -600)) &&
          together < length(mapEdge.Apply(large, 1250, -600)), "border and building caps both hold");
    std::printf("CRATER CORE CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
