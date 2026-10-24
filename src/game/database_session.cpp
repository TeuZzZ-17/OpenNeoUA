#include "game/database_session.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "common/env.h"
#include "platform/wav.h"
#include <algorithm>
#include <set>
#include "world/parsers.h"
#include <functional>
#include <cctype>
#include <cstdlib>
#include <new>
#include "system/system.h"
#include "locale/locale.h"

namespace {
template<class Prototype> void ResetCatalogPrototype(const Prototype &prototype)
{
    // Another script may redefine this ID. Release its owned models first.
    auto &value=const_cast<Prototype &>(prototype);
    value.~Prototype();new(&value) Prototype();
}
bool CatalogID(const std::string &text,size_t count,long &id)
{
    char *end=nullptr;id=std::strtol(text.c_str(),&end,0);
    return !text.empty()&&end!=text.c_str()&&!*end&&id>=0&&(size_t)id<count;
}
}

extern int vertMenuSpace;
extern int yw_MBLoadSet(NC_STACK_ypaworld *, int);

DatabaseSession::DatabaseSession() { carrier.Resize(1); }

DatabaseSession::~DatabaseSession()
{
    StopAudio();
    for(auto &entry:captions)SDL_FreeSurface(entry.second);
    if(detailFont)TTF_CloseFont(detailFont);
    preview.Clear();
    if (resources)
    {
        resources->ClearProceduralEnergyFX();
        resources->ClearSharedExternalBases();
        resources->ClearSharedExternalMeshes();
        resources->ClearOverrideModels();
        resources->_vhclModels.clear();
        resources->FreeLegos();
        resources->FreeFillers();
        if (resources->_setData) resources->_setData->Delete();
        resources->_setData = nullptr;
    }
}

bool DatabaseSession::LoadResources()
{
    if (resources) return loaded;
    resources.reset(new NC_STACK_ypaworld());
    resources->_normalVizLimit = 16000;
    resources->_normalFadeLength = 100;
    const std::string previous = Common::Env.GetPrefix("rsrc");
    loaded = yw_MBLoadSet(resources.get(),1) != 0;
    // Gameplay loads type_ns.font from Data/Fonts, not the shell's Set46 fonts.
    Common::Env.SetPrefix("rsrc","data:fonts");
    icons.reset(resources->yw_LoadFont("type_ns.font"));
    Common::Env.SetPrefix("rsrc",previous);
    return loaded;
}

void DatabaseSession::LoadDetailFont(int size)
{
    if(detailFont&&detailFontSize==size)return;
    for(auto &entry:captions)SDL_FreeSurface(entry.second);
    captions.clear();
    if(detailFont)TTF_CloseFont(detailFont);
    const auto parts=Stok::Split(System::ResolveMenuFontDescr(Locale::Text::Font()),",");
    detailFont=parts.empty()?nullptr:System::LoadFont(parts[0],size);
    detailFontSize=size;
}

void DatabaseSession::StopAudio()
{
    SFXEngine::SFXe.StopCarrier(&carrier);
    carrier.Resize(1);
    sound.ClearSounds();
}

void DatabaseSession::SelectAudio(const World::TVhclSound *definition, bool loop)
{
    StopAudio();
    if (!definition) return;
    // Copy only authored metadata. Loaded sample pointers remain owned by their
    // original carrier; the preview owns and frees its own sample references.
    sound.MainSample.Name = definition->MainSample.Name;
    sound.ExtSamples.resize(definition->ExtSamples.size());
    for (size_t i=0;i<sound.ExtSamples.size();++i)
        sound.ExtSamples[i].Name = definition->ExtSamples[i].Name;
    sound.extS = definition->extS;
    for (auto &fragment : sound.extS)
    {
        fragment.Sample = nullptr;
        fragment.rlOffset = 0;
        fragment.rlSmplCnt = 0;
    }
    sound.volume = definition->volume;
    sound.SetPitchRange(definition->pitch_min,definition->pitch_max);
    sound.LoadSamples();
    auto &output = carrier.Sounds[0];
    output.Volume = sound.volume;
    output.PSample = sound.MainSample.Sample ? sound.MainSample.Sample->GetSampleData() : nullptr;
    output.PFragments = sound.extS.empty() ? nullptr : &sound.extS;
    output.SetFragmented(output.PFragments != nullptr);
    output.SetLoop(loop);
    sound.ConfigureSoundSourcePitch(output);
    // Menu previews have no world-space attenuation, shakes or palette effects.
    if (output.PSample || output.PFragments) SFXEngine::SFXe.startSound(&carrier,0);
}

bool DatabaseSession::LoadDefinitions(const NC_STACK_ypaworld &source)
{
    definitions.reset(new NC_STACK_ypaworld());
    definitions->_vhclProtos.resize(source.GetVhclProtos().size());
    definitions->_weaponProtos.resize(source.GetWeaponsProtos().size());
    definitions->_buildProtos.resize(source.GetBuildProtos().size());
    definitions->_roboProtos.reserve(NC_STACK_ypaworld::NUM_ROBO_PROTO);
    definitions->_roboProtos.resize(1);
    Engine::StringList lines;
    std::set<std::string> visited;
    // Expand the manifest in authored order, then add loose scripts once.
    std::function<void(const std::string &)> read = [&](const std::string &path) {
        const auto *node=FSMgr::iDir::findNode(Common::Env.ApplyPrefix(path));
        std::string key=node?node->getPath():Common::Env.ApplyPrefix(path);
        std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return std::tolower(c);});
        if(!visited.insert(key).second)return;
        auto file=uaOpenFile(path,"r");if(!file.OK())return;
        std::string line;
        while(file.ReadLine(&line))
        {
            Stok tokens(line," \t\r\n=");std::string command,value;
            tokens.GetNext(&command);tokens.GetNext(&value);
            if(!StriCmp(command,"include"))read(value);
            else lines.push_back(line);
        }
    };
    read(source._initScriptFilePath);
    Engine::StringList manifestLines;manifestLines.swap(lines);
    std::function<void(const std::string &)> scan = [&](const std::string &dir) {
        std::vector<std::string> dirs,files;
        auto it=uaOpenDir(dir);FSMgr::iNode *node=nullptr;
        while(it&&it.getNext(&node))
        {
            if(!node)continue;
            const auto path=dir+node->getName();
            if(node->getType()==FSMgr::iNode::NTYPE_DIR)
            {
                const auto name=node->getName();
                if(!name.empty()&&name[0]!='.'&&StriCmp(name,"Save")&&StriCmp(name,"Build")&&StriCmp(name,"src"))
                    dirs.push_back(path+"/");
            }
            else
            {
                const auto dot=path.find_last_of('.');
                if(dot!=std::string::npos&&(!StriCmp(path.substr(dot),".scr")||!StriCmp(path.substr(dot),".cfg")))files.push_back(path);
            }
        }
        std::sort(files.begin(),files.end());std::sort(dirs.begin(),dirs.end());
        for(const auto &path:files)read(path);
        for(const auto &path:dirs)scan(path);
    };
    scan("");
    // Active-manifest definitions win when other game modes reuse the same IDs.
    lines.splice(lines.end(),manifestLines);
    const auto previous=Common::Env.SetPrefix("rsrc","data:");
    // Named special prototypes use a forced ID in their gameplay loader.
    // The catalog accepts the numeric IDs of ordinary script definitions.
    struct CatalogVehicleParser : World::Parsers::VhclProtoParser {
        const NC_STACK_ypaworld &source;
        size_t count;
        CatalogVehicleParser(NC_STACK_ypaworld *world,size_t size):World::Parsers::VhclProtoParser(world),source(*world),count(size){}
        bool IsScope(ScriptParser::Parser &parser,const std::string &word,const std::string &opt) override
        {
            if(!StriCmp(word,"new_vehicle")||!StriCmp(word,"modify_vehicle"))
            {
                long id=0;
                if(!CatalogID(opt,count,id))return false;
                if(!StriCmp(word,"new_vehicle"))ResetCatalogPrototype(source.GetVhclProtos()[id]);
            }
            return World::Parsers::VhclProtoParser::IsScope(parser,word,opt);
        }
    };
    struct CatalogWeaponParser : World::Parsers::WeaponProtoParser {
        const NC_STACK_ypaworld &source;
        CatalogWeaponParser(NC_STACK_ypaworld *w):World::Parsers::WeaponProtoParser(w),source(*w){}
        bool IsScope(ScriptParser::Parser &parser,const std::string &word,const std::string &opt) override
        {
            if(!StriCmp(word,"new_weapon")||!StriCmp(word,"modify_weapon"))
            {
                long id=0;if(!CatalogID(opt,source.GetWeaponsProtos().size(),id))return false;
                if(!StriCmp(word,"new_weapon"))ResetCatalogPrototype(source.GetWeaponsProtos()[id]);
            }
            return World::Parsers::WeaponProtoParser::IsScope(parser,word,opt);
        }
    };
    ScriptParser::HandlersList handlers{
        new CatalogVehicleParser(definitions.get(),definitions->GetVhclProtos().size()),
        new CatalogWeaponParser(definitions.get()),
        new World::Parsers::BuildProtoParser(definitions.get())};
    const bool ok=ScriptParser::ParseStringList(lines,handlers,ScriptParser::FLAG_NO_INCLUDE);
    Common::Env.SetPrefix("rsrc",previous);
    catalog.Build(*definitions);
    return ok;
}

void DatabaseSession::LoadOffers(const NC_STACK_ypaworld &source)
{
    offers.clear();
    for(int id:catalog.Entries(World::Database::Vehicle))
    {
        const auto &p=source.GetVhclProtos()[id];
        if(p.purchase_cost<0)continue;
        WorkshopOffer offer;offer.key=std::to_string(id);offer.vehicle=id;offer.cost=p.purchase_cost;
        offers.push_back(offer);
    }
}

DatabaseLayout DatabaseLayout::For(const NC_STACK_ypaworld &world, bool workshop)
{
    DatabaseLayout r;
    const int w=world._screenSize.x,h=world._screenSize.y;
    r.line=std::max(12,world._fontH+vertMenuSpace);
    r.detailLine=std::max(12,world._fontH*3/4+2);
    r.bottom=h-world._fontH;
    r.listTop=(workshop?3:2)*r.line;
    const int listBottom=workshop?r.bottom-6*r.line:r.bottom;
    r.rows=std::max(1,std::min((int)MaxRows,(listBottom-r.listTop)/r.line));
    r.listWidth=w*32/100;r.right=w*34/100;r.width=w-r.right-8;
    r.previewTop=3*r.line;
    r.previewWidth=r.width*54/100;
    r.previewHeight=std::max(5*r.detailLine,h*28/100);
    r.statsLeft=r.right+r.previewWidth+8;r.statsWidth=w-r.statsLeft-8;
    r.textTop=r.previewTop+r.previewHeight+8;
    r.textLines=std::max(1,(r.bottom-r.textTop-4)/r.detailLine);
    return r;
}
