// External sectors: "sec_type = Data/....sector" in a new_building block.
//
// A .sector file describes one complete sector (cells, destruction states, models,
// collision, map symbols, explosions). On every SET load it is installed in free LEGO,
// piece and sector slots of the active SET; SET.BAS and SET.SDF are never changed.
//
// Resource rule: every file is searched beside the .sector file first. A missing BASE/3DS
// falls back to the active SET model with the same name; textures, skeletons and animations
// of a local BASE fall back to the active SET (Loose, then SET.BAS), so a Set3 map uses the
// Set3 look.
#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <memory>
#include <vector>

#include "game/yw.h"
#include "common/def_parser.h"
#include "common/log.h"
#include "common/utils.h"
#include "nucleus/IFFile.h"
#include "nucleus/loaders.h"
#include "nucleus/nucleas.h"
#include "nucleus/sklt.h"

namespace
{

struct SectorModelDef
{
    std::string name;
    std::string visual;      // VP number, .BASE/.3ds file, or SET model name
    std::string collision;   // .sklt file or SET resource name
    int collisionMode = -1;  // -1 = inherited / default
    int mapSymbol = -1;
    int shield = -1;
    bool explosionsSet = false;
    std::vector<std::array<long, 4>> explosions; // VP|x|y|z as in SET.SDF
};

struct SectorPieceDef
{
    std::string name;
    std::vector<std::string> states; // 1 value (all states) or 4 values
    bool destructible = false;
};

struct SectorDef
{
    bool sectorSeen = false;
    std::vector<std::string> cells; // 1 (one large cell) or 9 (3x3, SET.SDF order)
    int ground = -1;
    int mapSymbol = 0;
    std::vector<SectorModelDef> models;
    std::vector<SectorPieceDef> pieces;
};

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return text;
}

// "dir/Name.ext" -> "name": SET.SDF labels, override names and authored values
// are compared by stem so ".bas"/".base" spellings and folders do not matter.
std::string Stem(std::string text)
{
    std::replace(text.begin(), text.end(), '\\', '/');
    const size_t slash = text.rfind('/');
    if ( slash != std::string::npos )
        text.erase(0, slash + 1);
    const size_t dot = text.rfind('.');
    if ( dot != std::string::npos && dot > 0 )
        text.erase(dot);
    return Lower(text);
}

bool EndsWith(const std::string &text, const char *suffix)
{
    const std::string tail(suffix);
    return text.size() > tail.size() && !StriCmp(text.substr(text.size() - tail.size()), tail);
}

bool ParseInt(const std::string &text, long min, long max, long *out)
{
    char *end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 0);
    if ( text.empty() || !end || *end || value < min || value > max )
        return false;
    *out = value;
    return true;
}

// Models and pieces have separate name lists; names are unique inside their own list.
template<class T> int FindByName(const std::vector<T> &list, const std::string &name)
{
    for (size_t i = 0; i < list.size(); ++i)
        if ( !StriCmp(list[i].name, name) )
            return (int)i;
    return -1;
}

class SectorFileParser : public ScriptParser::DataHandler
{
public:
    explicit SectorFileParser(SectorDef &def) : _def(def) {}

    bool IsScope(ScriptParser::Parser &, const std::string &word, const std::string &opt) override
    {
        _model = nullptr;
        _piece = nullptr;
        _inSector = false;
        if ( !StriCmp(word, "begin_sector") )
        {
            if ( _def.sectorSeen )
                return false;
            _def.sectorSeen = _inSector = true;
            return true;
        }
        if ( opt.empty() )
            return false;
        if ( !StriCmp(word, "begin_model") && FindByName(_def.models, opt) < 0 )
        {
            _def.models.emplace_back();
            _model = &_def.models.back();
            _model->name = opt;
            return true;
        }
        if ( !StriCmp(word, "begin_piece") && FindByName(_def.pieces, opt) < 0 )
        {
            _def.pieces.emplace_back();
            _piece = &_def.pieces.back();
            _piece->name = opt;
            return true;
        }
        return false;
    }

    int Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2) override
    {
        if ( !StriCmp(p1, "end") )
            return ScriptParser::RESULT_SCOPE_END;
        if ( _inSector )
            return HandleSector(p1, p2);
        if ( _model )
            return HandleModel(parser, p1, p2);
        if ( _piece )
            return HandlePiece(p1, p2);
        return ScriptParser::RESULT_UNKNOWN;
    }

private:
    int HandleSector(const std::string &p1, const std::string &p2)
    {
        long value = 0;
        if ( !StriCmp(p1, "cells") )
        {
            _def.cells = Stok::Split(p2, "|");
            return _def.cells.size() == 1 || _def.cells.size() == 9
                   ? ScriptParser::RESULT_OK : ScriptParser::RESULT_BAD_DATA;
        }
        if ( !StriCmp(p1, "ground") )
        {
            if ( !ParseInt(p2, 0, 5, &value) ) return ScriptParser::RESULT_BAD_DATA;
            _def.ground = (int)value;
            return ScriptParser::RESULT_OK;
        }
        if ( !StriCmp(p1, "map_symbol") )
        {
            if ( !ParseInt(p2, 0, 255, &value) ) return ScriptParser::RESULT_BAD_DATA;
            _def.mapSymbol = (int)value;
            return ScriptParser::RESULT_OK;
        }
        return ScriptParser::RESULT_UNKNOWN;
    }

    int HandleModel(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
    {
        long value = 0;
        if ( !StriCmp(p1, "visual") )
        {
            _model->visual = parser.FullLineValue();
            return _model->visual.empty() ? ScriptParser::RESULT_BAD_DATA : ScriptParser::RESULT_OK;
        }
        if ( !StriCmp(p1, "collision") )
        {
            _model->collision = parser.FullLineValue();
            return _model->collision.empty() ? ScriptParser::RESULT_BAD_DATA : ScriptParser::RESULT_OK;
        }
        if ( !StriCmp(p1, "collision_mode") )
        {
            if ( !ParseInt(p2, 0, 2, &value) ) return ScriptParser::RESULT_BAD_DATA;
            _model->collisionMode = (int)value;
            return ScriptParser::RESULT_OK;
        }
        if ( !StriCmp(p1, "map_symbol") )
        {
            if ( !ParseInt(p2, 0, 255, &value) ) return ScriptParser::RESULT_BAD_DATA;
            _model->mapSymbol = (int)value;
            return ScriptParser::RESULT_OK;
        }
        if ( !StriCmp(p1, "shield") )
        {
            if ( !ParseInt(p2, 0, 100, &value) ) return ScriptParser::RESULT_BAD_DATA;
            _model->shield = (int)value;
            return ScriptParser::RESULT_OK;
        }
        if ( !StriCmp(p1, "explosion") )
        {
            // Repeatable. "explosion = none" clears explosions inherited from a SET model.
            if ( !_model->explosionsSet )
                _model->explosions.clear();
            _model->explosionsSet = true;
            if ( !StriCmp(p2, "none") )
                return ScriptParser::RESULT_OK;
            const std::vector<std::string> parts = Stok::Split(p2, "|");
            std::array<long, 4> fx {};
            if ( parts.size() != 4 || !ParseInt(parts[0], 0, 255, &fx[0]) ||
                 !ParseInt(parts[1], -100000, 100000, &fx[1]) ||
                 !ParseInt(parts[2], -100000, 100000, &fx[2]) ||
                 !ParseInt(parts[3], -100000, 100000, &fx[3]) )
                return ScriptParser::RESULT_BAD_DATA;
            _model->explosions.push_back(fx);
            return ScriptParser::RESULT_OK;
        }
        return ScriptParser::RESULT_UNKNOWN;
    }

    int HandlePiece(const std::string &p1, const std::string &p2)
    {
        long value = 0;
        if ( !StriCmp(p1, "states") )
        {
            _piece->states = Stok::Split(p2, "|");
            return _piece->states.size() == 1 || _piece->states.size() == 4
                   ? ScriptParser::RESULT_OK : ScriptParser::RESULT_BAD_DATA;
        }
        if ( !StriCmp(p1, "destructible") )
        {
            if ( !ParseInt(p2, 0, 1, &value) ) return ScriptParser::RESULT_BAD_DATA;
            _piece->destructible = value == 1;
            return ScriptParser::RESULT_OK;
        }
        return ScriptParser::RESULT_UNKNOWN;
    }

    SectorDef &_def;
    bool _inSector = false;
    SectorModelDef *_model = nullptr;
    SectorPieceDef *_piece = nullptr;
};

// Reads and validates a .sector file. Returns an empty string on success.
std::string ReadSectorFile(const std::string &path, SectorDef *def)
{
    // Read directly (exact name): the legacy resolver shortens long extensions.
    FSMgr::FileHandle file = FSMgr::iDir::openFile(path, "r");
    if ( !file.OK() )
        return "file not found";

    Engine::StringList lines;
    std::string line;
    while ( file.ReadLine(&line) )
        lines.push_back(line);

    ScriptParser::HandlersList handlers { new SectorFileParser(*def) };
    if ( !ScriptParser::ParseStringList(lines, handlers,
                                        ScriptParser::FLAG_NO_INCLUDE | ScriptParser::FLAG_NO_SCOPE_SKIP) )
        return "syntax error (see the PARSE ERROR line above)";

    if ( !def->sectorSeen || def->cells.empty() )
        return "begin_sector with cells is missing";
    if ( def->ground < 0 )
        return "begin_sector needs ground = 0-5";
    for (const std::string &cell : def->cells)
        if ( FindByName(def->pieces, cell) < 0 )
            return "cells uses an unknown piece '" + cell + "'";
    for (const SectorPieceDef &piece : def->pieces)
    {
        if ( piece.states.empty() )
            return "piece '" + piece.name + "' needs states";
        for (const std::string &state : piece.states)
            if ( FindByName(def->models, state) < 0 )
                return "piece '" + piece.name + "' uses an unknown model '" + state + "'";
    }
    for (const SectorModelDef &model : def->models)
        if ( model.visual.empty() )
            return "model '" + model.name + "' needs visual";
    return std::string();
}

} // namespace

void NC_STACK_ypaworld::ClearExternalSectors()
{
    for (size_t id : _externalSectors.legos)
    {
        if ( id < _legoArray.size() )
        {
            if ( _legoArray[id].CollisionSkelet )
                _legoArray[id].CollisionSkelet->Delete();
            _legoArray[id] = TLego();
        }
    }
    for (size_t id : _externalSectors.pieces)
        if ( id < _subSectorArray.size() )
            _subSectorArray[id] = TSubSectorDesc();
    for (size_t id : _externalSectors.sectors)
        if ( id < _secTypeArray.size() )
            _secTypeArray[id] = TSectorDesc();
    for (auto &entry : _externalSectors.bases)
        if ( entry.second )
            entry.second->Delete();
    _externalSectors = TExternalSectors();
}

void NC_STACK_ypaworld::LoadExternalSectors()
{
    for (World::TBuildingProto &proto : _buildProtos)
    {
        if ( proto.sector_file.empty() )
            continue;
        const int slot = ResolveExternalSector(proto.sector_file);
        proto.SecType = slot >= 0 ? (uint8_t)slot : proto.sector_fallback;
    }
}

int NC_STACK_ypaworld::ResolveExternalSector(const std::string &path)
{
    const auto cached = _externalSectors.slots.find(path);
    if ( cached != _externalSectors.slots.end() )
        return cached->second;

    int &result = _externalSectors.slots[path];
    result = -1;

    auto fail = [&](const std::string &reason) -> int
    {
        ypa_log_out("WARNING: external sector %s not used: %s; fallback sector used.\n",
                    path.c_str(), reason.c_str());
        return -1;
    };

    SectorDef def;
    const std::string error = ReadSectorFile(path, &def);
    if ( !error.empty() )
        return fail(error);

    const std::string folder = path.substr(0, path.rfind('/'));

    // Free sector slot: highest unused ID below the SET border range (248-255).
    int sectorSlot = -1;
    for (int id = 247; id >= 1 && sectorSlot < 0; --id)
    {
        const TSectorDesc &desc = _secTypeArray[id];
        if ( std::none_of(desc.SubSectors.begin(), desc.SubSectors.end(),
                          [](const TSubSectorDesc *p) { return p != nullptr; }) )
            sectorSlot = id;
    }
    if ( sectorSlot < 0 )
        return fail("no free sector slot in this SET");

    // SET model by name (SET.SDF label or override.lst file), compared by stem.
    auto findSetLego = [&](const std::string &name) -> int
    {
        const std::string key = Stem(name);
        for (size_t i = 0; i < _legoArray.size(); ++i)
        {
            const TLego &lego = _legoArray[i];
            if ( !lego.Base || std::find(_externalSectors.legos.begin(), _externalSectors.legos.end(), i)
                               != _externalSectors.legos.end() )
                continue;
            if ( (!lego.Name.empty() && Stem(lego.Name) == key) ||
                 (!lego.OverrideName.empty() && Stem(lego.OverrideName) == key) )
                return (int)i;
        }
        return -1;
    };

    // Owned BASE/3DS copies: sector models are static and must not share a
    // vehicle's cached instance.
    auto loadFile = [&](const std::string &dataPath) -> NC_STACK_base *
    {
        auto it = _externalSectors.bases.find(dataPath);
        if ( it != _externalSectors.bases.end() )
            return it->second;
        NC_STACK_base *base = Utils::ProxyLoadBase(dataPath);
        if ( base )
        {
            base->SetVizLimit(_normalVizLimit);
            base->SetFadeLength(_normalFadeLength);
            base->SetStatic(true);
            base->MakeVBO();
        }
        _externalSectors.bases[dataPath] = base;
        return base;
    };

    // Collision skeletons resolve beside the .sector first, then in the active SET.
    auto loadSkeleton = [&](const std::string &name) -> NC_STACK_sklt *
    {
        IFFile::BeginExternalBaseResourceScope(path);
        NC_STACK_sklt *skeleton = Nucleus::CInit<NC_STACK_sklt>({{NC_STACK_rsrc::RSRC_ATT_NAME, name}});
        IFFile::EndExternalBaseResourceScope();
        return skeleton;
    };

    std::vector<TLego> legos(def.models.size());
    auto releaseLegos = [&]()
    {
        for (TLego &lego : legos)
            if ( lego.CollisionSkelet )
                lego.CollisionSkelet->Delete();
    };

    for (size_t m = 0; m < def.models.size(); ++m)
    {
        const SectorModelDef &model = def.models[m];
        TLego &lego = legos[m];
        lego.Name = model.name;

        int inherit = -1;
        long vp = 0;
        if ( ParseInt(model.visual, 0, 0x7fff, &vp) )
        {
            if ( vp >= (long)_vhclModels.size() || !_vhclModels[vp] )
            {
                releaseLegos();
                return fail("model '" + model.name + "': VP " + model.visual + " does not exist in this SET");
            }
            lego.Base = _vhclModels[vp];
        }
        else if ( EndsWith(model.visual, ".BASE") || EndsWith(model.visual, ".3ds") )
        {
            // "Data/..." is used as written; anything else is relative to the .sector folder.
            std::string dataPath;
            const bool dataRooted = model.visual.size() > 5 && !StriCmp(model.visual.substr(0, 5), "Data/");
            if ( !uaNormalizeDataAssetPath(dataRooted ? model.visual : folder + "/" + model.visual,
                                           &dataPath, false) )
            {
                releaseLegos();
                return fail("model '" + model.name + "': invalid visual path " + model.visual);
            }
            if ( FSMgr::iDir::fileExist(dataPath) )
            {
                lego.Base = loadFile(dataPath);
                if ( !lego.Base )
                {
                    releaseLegos();
                    return fail("model '" + model.name + "': " + dataPath + " could not be loaded");
                }
            }
            else
                inherit = findSetLego(model.visual);
        }
        else
            inherit = findSetLego(model.visual);

        if ( !lego.Base )
        {
            if ( inherit < 0 )
            {
                releaseLegos();
                return fail("model '" + model.name + "': " + model.visual + " found neither beside the file nor in this SET");
            }
            const TLego &source = _legoArray[inherit];
            lego.Base = source.Base;
            lego.GUIElementID = source.GUIElementID;
            lego.Shield = source.Shield;
            lego.CollisionMode = source.CollisionMode;
            lego.CollisionName = source.CollisionName;
            lego.Explosions = source.Explosions;
        }

        if ( !model.collision.empty() )
            lego.CollisionName = model.collision;
        if ( lego.CollisionName.empty() )
        {
            releaseLegos();
            return fail("model '" + model.name + "' needs collision");
        }
        lego.CollisionSkelet = loadSkeleton(lego.CollisionName);
        if ( !lego.CollisionSkelet )
        {
            releaseLegos();
            return fail("model '" + model.name + "': collision " + lego.CollisionName + " could not be loaded");
        }

        if ( model.collisionMode >= 0 ) lego.CollisionMode = (uint8_t)model.collisionMode;
        if ( model.mapSymbol >= 0 ) lego.GUIElementID = (uint8_t)model.mapSymbol;
        if ( model.shield >= 0 ) lego.Shield = (uint8_t)model.shield;
        if ( model.explosionsSet )
        {
            lego.Explosions.clear();
            for (size_t i = 0; i < model.explosions.size(); ++i)
            {
                TLego::ExFX fx;
                fx.Index = (uint8_t)i;
                fx.ObjectID = (uint8_t)model.explosions[i][0];
                // Same axes as SET.SDF effect groups (yw_parse_lego).
                fx.Position = vec3d(model.explosions[i][1], -model.explosions[i][3], -model.explosions[i][2]);
                lego.Explosions.push_back(fx);
            }
        }
        lego.UseCollisionSkelet = lego.CollisionMode == 1 ? _colsubSkeleton
                                : lego.CollisionMode == 2 ? _colcompSkeleton
                                : lego.CollisionSkelet;
    }

    // Free LEGO and piece rows after the SET's own tables (override.lst may use some).
    std::vector<size_t> legoIds;
    for (size_t id = _setLegoCount; id < _legoArray.size() && legoIds.size() < legos.size(); ++id)
        if ( !_legoArray[id].Base && !_legoArray[id].CollisionSkelet )
            legoIds.push_back(id);
    const size_t firstPiece = _setPieceCount + _externalSectors.pieces.size();
    if ( legoIds.size() < legos.size() || firstPiece + def.pieces.size() > _subSectorArray.size() )
    {
        releaseLegos();
        return fail("not enough free model or piece slots");
    }

    for (size_t m = 0; m < legos.size(); ++m)
    {
        _legoArray[legoIds[m]] = legos[m];
        _externalSectors.legos.push_back(legoIds[m]);
    }

    for (size_t p = 0; p < def.pieces.size(); ++p)
    {
        const SectorPieceDef &piece = def.pieces[p];
        TSubSectorDesc &desc = _subSectorArray[firstPiece + p];
        for (size_t s = 0; s < desc.HPModels.size(); ++s)
        {
            const std::string &state = piece.states[piece.states.size() == 1 ? 0 : s];
            desc.HPModels[s] = (uint16_t)legoIds[FindByName(def.models, state)];
        }
        desc.StartHealth = piece.destructible ? 255 : 0;
        _externalSectors.pieces.push_back(firstPiece + p);
    }

    TSectorDesc &sector = _secTypeArray[sectorSlot];
    sector = TSectorDesc();
    sector.SectorType = def.cells.size() == 1 ? 1 : 0;
    sector.SurfaceType = (uint8_t)def.ground;
    sector.GUIElementID = (uint8_t)def.mapSymbol;
    for (size_t i = 0; i < def.cells.size(); ++i)
    {
        TSubSectorDesc *piece = &_subSectorArray[firstPiece + FindByName(def.pieces, def.cells[i])];
        if ( def.cells.size() == 1 )
            sector.SubSectors.At(0, 0) = piece;
        else
            sector.SubSectors.At(i % 3, 2 - i / 3) = piece; // same order as SET.SDF
    }
    _externalSectors.sectors.push_back(sectorSlot);

    ypa_log_out("External sector %s installed in sector slot %d.\n", path.c_str(), sectorSlot);
    result = sectorSlot;
    return result;
}
