// Run with run_native_probe.py after the engine may be built again.
#include "game/yw.h"
#include "common/env.h"
#include <cstdio>
#undef main

static int checks=0, failures=0;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; std::printf("FAIL %s\n", #expr); } } while (false)

int main()
{
    World::Location area;
    area.id=1; area.name="Dairopoli";
    area.points={{0,0},{4,0},{4,-1},{1,-1},{1,-4},{0,-4}};
    CHECK(World::ValidLocation(area));
    World::PrepareLocationBounds(area);
    CHECK(!World::LocationContains(area,10,10));
    CHECK(World::LocationContains(area,.5,-3));
    CHECK(!World::LocationContains(area,3,-3));
    CHECK(World::LocationContains(area,4,-.5));
    auto bad=area;
    bad.points={{0,0},{2,-2},{0,-2},{2,0}};
    CHECK(!World::ValidLocation(bad));
    bad=area; bad.points[1].x=INFINITY;
    CHECK(!World::ValidLocation(bad));
    bad=area; bad.fadeIn=-1;
    CHECK(!World::ValidLocation(bad));
    bad=area; bad.color="neon";
    CHECK(!World::ValidLocation(bad));
    CHECK(World::NormalizeLocationColor(" YELLOW ")=="yellow");
    CHECK(World::NormalizeLocationColor("neon")=="white");
    const World::LocationColorRgb yellow=World::LocationTextColor("yellow");
    CHECK(yellow.r==255 && yellow.g==221 && yellow.b==87);
    CHECK(World::LocationOpacity(area,225)==.5f);
    CHECK(World::LocationOpacity(area,3625)==.5f);
    CHECK(World::LocationOpacity(area,4000)==0);
    CHECK(World::FindLocation({area,area},.5,-3)==1);
    World::LocationEntryState state;
    CHECK(state.update(0,100)); // Initial entry, including a level starting inside.
    CHECK(!state.update(0,200)); // Switching units inside the same area is quiet.
    CHECK(!state.update(-1,300));
    CHECK(state.update(0,400)); // Leaving and re-entering repeats the notice.
    state=World::LocationEntryState(); state.suppressInitial=true;
    CHECK(!state.update(0,500)); // Loading a save does not invent an entry.
    CHECK(!state.update(-1,600));
    CHECK(state.update(0,700));
    CHECK(state.update(1,800));
    CHECK(World::LocationDisplayName("Dair\xf2poli")=="Dair\xc3\xb2poli");
    CHECK(World::LocationDisplayName("Dair\xc3\xb2poli")=="Dair\xc3\xb2poli");

    std::vector<World::Location> values;
    ScriptParser::HandlersList handlers{new World::Parsers::LevelLocationParser(&values)};
    Engine::StringList lines={"begin_location 7", "name = New Dairopoli City",
        "color = yellow", "sound = Data/Sounds/My City.wav",
        "point = 0_0", "point = 4_0", "point = 4_-4", "end"};
    CHECK(ScriptParser::Parser::ParseStringList(lines,handlers,ScriptParser::FLAG_NO_SCOPE_SKIP));
    CHECK(values.size()==1 && values[0].name=="New Dairopoli City");
    CHECK(values[0].color=="yellow");
    CHECK(values[0].sound=="Data/Sounds/My City.wav");
    CHECK(ScriptParser::Parser::ParseStringList(lines,handlers,ScriptParser::FLAG_NO_SCOPE_SKIP));
    CHECK(values.size()==1); // Duplicate IDs never create ambiguous areas.
    ScriptParser::HandlersList saved{new World::Parsers::LevelLocationParser(&values,true)};
    CHECK(ScriptParser::Parser::ParseStringList(lines,saved,ScriptParser::FLAG_NO_SCOPE_SKIP));
    CHECK(values.size()==1); // Save blocks replace the original LDF definitions.
    std::printf("Locations: %d checks, %d failures.\n",checks,failures);
    return failures ? 1 : 0;
}
