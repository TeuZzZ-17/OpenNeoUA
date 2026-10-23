// Focused probes through the production icon parser, font renderer and plasma rules.
#include <bits/stdc++.h>
// The offscreen fixture supplies a surface to the production renderer singleton.
#define private public
#define protected public
#include "common/glyph_key.h"
#include "common/env.h"
#include "world/parsers.h"
#include "game/yw.h"
#include "game/ypabact.h"
#include "game/yw_game_ui_internal.h"
#include "gui/font.h"
#include "nucleus/bitmap.h"
#include "system/inivals.h"
#include <cstdio>
#include <fstream>
#undef main
#undef private
#undef protected

static int checks = 0, failures = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; std::printf("FAIL %s\n", #expr); } } while(false)

struct Actor : NC_STACK_ypabact
{
    void Bind(NC_STACK_ypaworld &world, int gid, double x)
    {
        _world = &world; _gid = gid; _owner = 1; _bact_type = BACT_TYPES_BACT;
        _status = BACT_STATUS_NORMAL; _status_flg = 0; _oflags = 0;
        _energy = 100; _energy_max = 1000; _position = vec3d(x, 0, -300);
        _wrldSize = vec2d(9600, -9600);
        _vp_normal = _vp_dead = _vp_fire = _vp_genesis = _vp_wait = _vp_megadeth = nullptr;
        _cellRef = world._cells.At(0).unitsList.push_back(this);
    }
    void Residue(int owner)
    {
        _owner = owner; _status = BACT_STATUS_DEAD; _status_flg = BACT_STFLAG_DEATH1;
        _position = vec3d(300, 0, -300); _vp_extra[0].pos = _position;
        _vp_extra[0].flags = EVPROTO_FLAG_ACTIVE; _scale_time = 10000;
    }
};


int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    uint32_t key = 0;
    for (int byte = 1; byte < 256; ++byte)
        CHECK(GlyphKey::Parse(std::string(1, (char)byte), key) && key == (uint32_t)byte);
    CHECK(GlyphKey::Parse(u8"∆", key) && key == 0x2206);
    CHECK(GlyphKey::Parse(u8"★", key) && key == 0x2605);
    CHECK(GlyphKey::Parse(u8"🗲", key) && key == 0x1f5f2);
    CHECK(GlyphKey::Parse("U+003B", key) && key == ';');
    CHECK(GlyphKey::Parse("#255", key, true) && key == 255);
    for (const auto &bad : {"U+110000", "U+D800", "U+FFFD", "#-1", "#99999999999999", "\xc0\xaf", "\xed\xa0\x80", "\xf0\x80\x80\x80", "\xef\xbf\xbd"})
        CHECK(!GlyphKey::Parse(bad, key, true));

    // Exercise the untouched legacy numeric syntax over every byte slot.
    const char *probePath = "Data/Fonts/HFonts/__ONUA_ICON_PROBE.FON";
    CHECK(!std::ifstream(probePath).good());
    {
        std::ofstream definitions(probePath, std::ios::binary);
        definitions << "H_FINDER.ILBM 16\n";
        for (int byte = 0; byte < 256; ++byte)
            definitions << "#" << byte << " 1 86 20\n";
        definitions << u8"★ 1 86 20\nU+1F5F2 1 86 20\n";
    }

    FSMgr::iDir::setBaseDir("");
    Common::Env.SetPrefix("data", "Data");
    Common::Env.SetPrefix("rsrc", "data:fonts");
    extern int init_classesLists_and_variables();
    init_classesLists_and_variables();
    GFX::Engine.StaticInit();

    NC_STACK_ypaworld world;
    world._vhclProtos.resize(256); world._weaponProtos.resize(256); world._buildProtos.resize(256);
    ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world),
        new World::Parsers::WeaponProtoParser(&world), new World::Parsers::BuildProtoParser(&world)};
    const Engine::StringList lines = {"\xef\xbb\xbfnew_vehicle 1", "model = tank", u8"type_icon = ∆", "end",
        "new_weapon 1", "type_icon = U+1F5F2", "end", "new_building 1", u8"type_icon = ★", "end"};
    CHECK(ScriptParser::Parser::ParseStringList(lines, handlers, ScriptParser::FLAG_NO_SCOPE_SKIP));
    CHECK(world._vhclProtos[1].type_icon == 0x2206);
    CHECK(world._weaponProtos[1].type_icon == 0x1f5f2);
    CHECK(world._buildProtos[1].TypeIcon == 0x2605);
    CHECK(ScriptParser::Parser::ParseStringList({"modify_vehicle 1", "type_icon = U+D800", "end"}, handlers, ScriptParser::FLAG_NO_SCOPE_SKIP));
    CHECK(world._vhclProtos[1].type_icon == 0x2206);

    auto *legacyFont = NC_STACK_ypaworld::yw_LoadFont("__ONUA_ICON_PROBE.FON");
    std::remove(probePath);
    CHECK(legacyFont != nullptr);
    if (legacyFont)
    {
        for (int byte = 0; byte < 256; ++byte)
            CHECK(legacyFont->map[byte].x == 1 && legacyFont->map[byte].w == 20);
        CHECK(legacyFont->Glyph(0x2605).w == 20 && legacyFont->Glyph(0x1f5f2).w == 20);
        delete legacyFont;
    }
    TileMap *font = NC_STACK_ypaworld::yw_LoadFont("TYPE_NS.FON");
    CHECK(font != nullptr);
    if (font)
    {
        CHECK(font->Glyph(0x2206).x == 1 && font->Glyph(0x2206).y == 86);
        CHECK(font->Glyph(0x2206).w == font->map[0xa3].w);
        CHECK(font->Glyph(0x10fffd).w == font->map[0].w);
        auto &renderer = GFX::Engine;
        renderer.ScreenSurface = SDL_CreateRGBSurfaceWithFormat(0, 64, 32, 32, SDL_PIXELFORMAT_ARGB8888);
        renderer.SetTileset(font, 28);
        auto render = [&](uint32_t glyph) {
            SDL_FillRect(renderer.Screen(), nullptr, 0);
            CmdStream commands;
            FontUA::select_tileset(&commands, 28);
            FontUA::set_xpos(&commands, 0); FontUA::set_ypos(&commands, 0);
            FontUA::store_glyph(&commands, glyph); FontUA::set_end(&commands);
            renderer.ProcessDrawSeq(commands);
            auto *pixels = (uint8_t *)renderer.Screen()->pixels;
            return std::string((char *)pixels, renderer.Screen()->pitch * renderer.Screen()->h);
        };
        const std::string legacyPixels = render(0xa3);
        CHECK(legacyPixels != std::string(legacyPixels.size(), 0));
        CHECK(render(0x2206) == legacyPixels);
        CmdStream cell;
        FontUA::glyph_cell(font, &cell, 0x2206, 12);
        CHECK(cell.size() > 6 && cell[0] == 0 && cell[1] == 13);
        renderer.SetTileset(nullptr, 28);
        delete font;
    }

    System::IniConf::GamePlasmaCurrencyEnable.Value = true;
    System::IniConf::GamePlasmaCurrencyEnable.WasSet = true;
    System::IniConf::GamePlasmaCurrencyGain.Value = std::string("100%");
    System::IniConf::GamePlasmaCurrencyGain.WasSet = true;
    System::IniConf::GamePlasmaDeathMagnetRadius.Value = std::string("100");
    System::IniConf::GamePlasmaDeathMagnetRadius.WasSet = true;
    System::IniConf::GamePlasmaDeathMagnetSpeed.Value = std::string("1000");
    System::IniConf::GamePlasmaDeathMagnetSpeed.WasSet = true;
    world._mapSize = Common::Point(8, 8); world._cells.Resize(world._mapSize);
    world._playerOwner = 1;
    Actor player, ai, enemy, source;
    player.Bind(world, 1, 350); player._oflags = BACT_OFLAG_USERINPT;
    ai.Bind(world, 2, 310); enemy.Bind(world, 3, 302); enemy._owner = 2;
    source.Bind(world, 4, 300); source.Residue(2); world._userUnit = &player;
    player._vehicleID = 1;
    CHECK(yw_GetSquadronManagerUnitIcon(&world, &player, 0) == 0x2206);
    CHECK(ai.CanCreditPlasmaCurrencyFrom(&source));
    CHECK(!ai.CanRecoverPlasmaEnergyFrom(&source));
    CHECK(!enemy.CanCollectPlasmaFrom(&source));
    CHECK(source.FindDeathPlasmaCollector(100) == &ai);
    CHECK(!player.CollectPlasmaFrom(&source));
    source.UpdateDeathPlasmaMagnet(20);
    CHECK(source._scale_time < 0 && ai._energy == 100 && player._energy == 100);
    CHECK(world.GetPlasmaCurrency() > 0);
    const uint64_t credited = world.GetPlasmaCurrency();
    CHECK(!ai.CollectPlasmaFrom(&source) && world.GetPlasmaCurrency() == credited);
    ai._energy = ai._energy_max; source.Residue(2);
    CHECK(ai.CollectPlasmaFrom(&source) && ai._energy == ai._energy_max);
    source.Residue(1);
    CHECK(!ai.CanCollectPlasmaFrom(&source));
    player._energy = player._energy_max;
    CHECK(!player.CanCollectPlasmaFrom(&source));
    player._energy = 100; const uint64_t beforeFriendly = world.GetPlasmaCurrency();
    CHECK(player.CollectPlasmaFrom(&source));
    CHECK(player._energy > 100 && world.GetPlasmaCurrency() == beforeFriendly);
    source.Residue(2); player._position.x = 301; player._energy = player._energy_max;
    CHECK(player.CollectPlasmaFrom(&source) && player._energy == player._energy_max);
    source.Residue(2); player._energy = 100;
    CHECK(player.CollectPlasmaFrom(&source) && player._energy > 100);
    ai._bact_type = BACT_TYPES_MISSLE; source.Residue(2);
    CHECK(!ai.CanCreditPlasmaCurrencyFrom(&source)); ai._bact_type = BACT_TYPES_BACT;
    System::IniConf::GamePlasmaCurrencyEnable.Value = false;
    ai._energy = 100; world._userRobo = &player;
    CHECK(!ai.CanCreditPlasmaCurrencyFrom(&source) && ai.CanRecoverPlasmaEnergyFrom(&source));
    world._isNetGame = true;
    CHECK(!ai.CanCollectPlasmaFrom(&source) && player.CanRecoverPlasmaEnergyFrom(&source));
    world._userRobo = world._userUnit = nullptr;
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
