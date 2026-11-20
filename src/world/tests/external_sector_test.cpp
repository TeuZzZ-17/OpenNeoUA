// External .sector files: sec_type parsing, installation in the active SET, SET fallbacks,
// reload stability. Run from the game root ("UA Rising"); it writes and removes
// Data/Models/Sector/_ExtSectorTest using one of our own models.
#include <bits/stdc++.h>
#include <windows.h>
#define protected public
#define private public
#include "game/yw.h"
#include "game/yw_internal.h"
#include "common/env.h"
#include "common/utils.h"
#include "system/system.h"
#include "system/inivals.h"
#include "nucleus/loaders.h"
#include "world/parsers.h"
#undef protected
#undef private
#undef main

extern int RegisterNucleusClasses();

static const char *DIR = "Data/Models/Sector/_ExtSectorTest";

static bool CopyFile2(const std::string &from, const std::string &to)
{
    std::ifstream in(from, std::ios::binary);
    std::ofstream out(to, std::ios::binary);
    out << in.rdbuf();
    return in.good() && out.good();
}

static void Write(const std::string &name, const std::string &text)
{
    std::ofstream(std::string(DIR) + "/" + name, std::ios::binary) << text;
}

static int FindLego(NC_STACK_ypaworld *w, const char *name)
{
    for (size_t i = 0; i < w->_setLegoCount; ++i)
        if (!StriCmp(w->_legoArray[i].Name, name)) return (int)i;
    return -1;
}

int main(int, char **)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // Fixture files first: the virtual file system is scanned at start-up.
    // (Our own Kyt flak model; SET models are referenced by name only).
    CreateDirectoryA("Data/Models/Sector", nullptr);
    CreateDirectoryA(DIR, nullptr);
    const std::string model = "Data/Models/Base/New/Ghorkov/Kyt_Flak/";
    bool fixture = CopyFile2(model + "Kyt_Flak_Normal.BASE", std::string(DIR) + "/Kyt_Flak_Normal.BASE") &&
                   CopyFile2(model + "KYTFLKN1.sklt", std::string(DIR) + "/KYTFLKN1.sklt") &&
                   CopyFile2("Data/Fonts/Sec4.PNG", std::string(DIR) + "/icon.png");
    Write("test.sector",
        "; test sector\n"
        "begin_sector\n    cells = G|G|G|G|CORE|G|G|G|G\n    ground = 0\n    map_symbol = 3\n    map_icon = icon.png|icon.png\nend\n"
        "begin_piece G\n    states = ground\nend\n"
        "begin_piece CORE\n    states = core|core|core|ground\n    destructible = 1\nend\n"
        "begin_model ground\n    visual = ST_EMPTY.base\nend\n"
        "begin_model core\n    visual = Kyt_Flak_Normal.BASE\n    collision = KYTFLKN1.sklt\n"
        "    shield = 40\n    explosion = 49|0|0|100\n    map_icon = icon.png\nend\n");
    Write("single.sector",
        "begin_sector\n    cells = C\n    ground = 2\nend\n"
        "begin_piece C\n    states = city\n    destructible = 1\nend\n"
        "begin_model city\n    visual = ST_CITY1.BASE ; not beside the file: SET model by name\n"
        "    map_icon = missing.png\nend\n");
    Write("broken.sector",
        "begin_sector\n    cells = C\n    ground = 0\nend\n"
        "begin_piece C\n    states = m\nend\n"
        "begin_model m\n    visual = Not_Existing_Model.BASE\nend\n");
    Write("badkey.sector", "begin_sector\n    cells = C\n    ground = 0\n    colour = 3\nend\n");

    std::ifstream videoInput("Data/Env/vid.def", std::ios::binary);
    const bool hadVideo = videoInput.good();
    const std::string videoMode((std::istreambuf_iterator<char>(videoInput)), std::istreambuf_iterator<char>());
    videoInput.close();
    System::Init();
    auto *window = SDL_GL_GetCurrentWindow();
    if (!window) { std::printf("No GL context\n"); return 2; }
    SDL_HideWindow(window);
    FSMgr::iDir::setBaseDir("");
    Common::Env.SetPrefix("data", "Data"); Common::Env.SetPrefix("env", "Data/Env");
    Common::Env.SetPrefix("levels", "data:levels"); Common::Env.SetPrefix("locale", "data:locale");
    Common::Env.SetPrefix("rsrc", "data:mc2res");
    RegisterNucleusClasses(); System::RescanFonts(); TF::Engine.Init(); GFX::Engine.Init();
    if (hadVideo) std::ofstream("Data/Env/vid.def", std::ios::binary | std::ios::trunc) << videoMode;
    else std::remove("Data/Env/vid.def");
    _putenv_s("ALSOFT_DRIVERS", "null"); SFXEngine::SFXe.init(); SFXEngine::SFXe.setMasterVolume(0);

    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *label) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", label); } };
    check(fixture, "fixture files written");

    auto world = std::unique_ptr<NC_STACK_ypaworld>(new NC_STACK_ypaworld);
    UserData user; user.clear(); user.p_YW = world.get(); world->_GameShell = &user;
    world->_screenSize = GFX::Engine.GetVirtualUIResolution(); world->_initScriptFilePath = "data:scripts/Startup.cfg";
    if (!world->ypaworld_func166("language")) return 7;
    if (!world->ProtosInit() || !world->yw_LoadSet(3)) { std::puts("load failed"); return 3; }

    const std::string base = std::string(DIR) + "/";
    ScriptParser::HandlersList parser{new World::Parsers::BuildProtoParser(world.get())};
    check(ScriptParser::ParseStringList({
        "modify_building 1", "sec_type = " + base + "test.sector|5", "end",
        "modify_building 2", "sec_type = " + base + "broken.sector|7", "end",
        "modify_building 3", "sec_type = " + base + "single.sector", "end",
        "modify_building 4", "sec_type = " + base + "badkey.sector|9", "end",
        "modify_building 5", "sec_type = 11", "end",
        "modify_building 5", "sec_type = Data/not_a_sector.txt", "end"}, parser, 0), "sec_type values parse");

    auto &p = world->_buildProtos;
    check(p[1].sector_file == base + "test.sector" && p[1].sector_fallback == 5 && p[1].SecType == 5, "path|fallback stored");
    check(p[5].sector_file.empty() && p[5].SecType == 11, "invalid sec_type keeps the previous number");

    // Same SET reload: tables are re-read and external sectors installed.
    check(world->yw_LoadSet(3), "SET reload with external sectors");
    const int slot = p[1].SecType;
    check(slot != 5 && slot >= 1 && slot <= 247, "test.sector got a free slot");
    TSectorDesc &desc = world->_secTypeArray[slot];
    check(desc.SectorType == 0 && desc.SurfaceType == 0 && desc.GUIElementID == 3, "3x3 sector fields");
    TSubSectorDesc *core = desc.SubSectors.At(1, 1);
    TSubSectorDesc *corner = desc.SubSectors.At(0, 0);
    check(core && corner && core != corner && core->StartHealth == 255 && corner->StartHealth == 0, "pieces placed");
    if (core && corner)
    {
        const TLego &coreLego = world->_legoArray[core->HPModels[0]];
        const TLego &groundLego = world->_legoArray[corner->HPModels[0]];
        check(core->HPModels[3] == corner->HPModels[0], "destroyed state uses the ground model");
        check(coreLego.Base && coreLego.CollisionSkelet && coreLego.UseCollisionSkelet == coreLego.CollisionSkelet,
              "local BASE and local skeleton loaded");
        check(coreLego.Shield == 40 && coreLego.Explosions.size() == 1 && coreLego.Explosions[0].ObjectID == 49 &&
              std::fabs(coreLego.Explosions[0].Position.y + 100.0) < 0.01, "shield and explosion");
        const int empty = FindLego(world.get(), "ST_EMPTY.base");
        check(empty >= 0 && groundLego.Base == world->_legoArray[empty].Base, "SET model reused by name");
        check(groundLego.CollisionSkelet && groundLego.CollisionSkelet != world->_legoArray[empty].CollisionSkelet,
              "inherited collision is an own skeleton object");
    }
    check(p[2].SecType == 7, "missing model falls back to the SET sector");
    if (core && corner)
    {
        const std::string icon = "Models/Sector/_ExtSectorTest/icon.png";
        const TLego &coreLego = world->_legoArray[core->HPModels[0]];
        check(coreLego.MapIcon[0] == icon && coreLego.MapIcon[2] == icon, "model map_icon for every lego zoom");
        check(world->_legoArray[corner->HPModels[0]].MapIcon[0].empty(), "SET model keeps its map symbol");
        check(desc.MapIcon[0] == icon && desc.MapIcon[1] == icon && world->_externalSectors.mapIcons,
              "sector map_icon for zoom 0 and 1");
    }
    check(p[4].SecType == 9, "unknown key falls back to the SET sector");
    TSectorDesc &single = world->_secTypeArray[p[3].SecType];
    const int city = FindLego(world.get(), "ST_CITY1.base");
    check(p[3].SecType != 0 && single.SectorType == 1 && single.SurfaceType == 2 && city >= 0 &&
          single.SubSectors.At(0, 0) &&
          world->_legoArray[single.SubSectors.At(0, 0)->HPModels[0]].Base == world->_legoArray[city].Base &&
          world->_legoArray[single.SubSectors.At(0, 0)->HPModels[0]].Explosions.size() ==
          world->_legoArray[city].Explosions.size(), "one large cell inherits a SET model");
    check(single.SubSectors.At(0, 0) &&
          world->_legoArray[single.SubSectors.At(0, 0)->HPModels[0]].MapIcon[0].empty(),
          "a missing map_icon keeps the map symbol and the sector");

    const size_t legos = world->_externalSectors.legos.size();
    check(world->yw_LoadSet(3) && p[1].SecType == slot && world->_externalSectors.legos.size() == legos,
          "reload is stable and does not leak slots");
    check(world->_secTypeArray[5].SubSectors.At(0, 0) != nullptr, "SET sectors untouched");

    world->FreeBriefDataSet();
    check(world->_externalSectors.legos.empty(), "external sectors released");

    for (const char *f : {"test.sector", "single.sector", "broken.sector", "badkey.sector",
                          "Kyt_Flak_Normal.BASE", "KYTFLKN1.sklt", "icon.png"})
        std::remove((base + f).c_str());
    RemoveDirectoryA(DIR);
    RemoveDirectoryA("Data/Models/Sector");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
