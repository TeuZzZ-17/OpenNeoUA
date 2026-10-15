// In-game GUI core: GUI window creation and update, buy/creation menus, player panel and the legacy
// input handlers. The other GUI areas live next to it:
//   yw_ui_map.cpp ........... map toolbar, strategic/tactical map and map markers
//   yw_ui_squadron.cpp ...... squadron manager
//   yw_ui_hud.cpp ........... HUD (radar, compass, lock markers, life bars, info panel, cursors)
//   yw_ui_orders.cpp ........ world selection, order feedback and unit selection by mouse
//   yw_ui_artillery.cpp ..... artillery shell map/HUD elements
//   yw_ui_windows.cpp ....... priority GUI windows, GUI list helpers and window management
//   yw_ui_voice.cpp ......... voice messages
// Shared helpers are declared in yw_game_ui_internal.h.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <iterator>
#include <array>
#include <algorithm>
#include <map>
#include <vector>
#include "common/env.h"
#include "common/includes.h"
#include "game/yw_internal.h"
#include "game/yw.h"
#include "game/yw_net.h"
#include "nucleus/loaders.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "game/ypagun.h"
#include "platform/windp.h"
#include "world/analyzer.h"
#include "system/inivals.h"
#include "game/yw_game_ui_internal.h"

GuiBase *dword_5BAFAC = nullptr;
std::string dword_5BAF98;
int dword_5BAF9C;
std::map<std::string, StatusIconCacheEntry> g_statusIconCache;
std::map<std::string, std::string> g_statusIconConfiguredPathCache;
std::map<const NC_STACK_ypabact *, StatusIconBlinkState> g_statusIconBlinkStates;
NC_STACK_ypaworld *g_statusIconBlinkWorld = nullptr;
int32_t g_statusIconBlinkLastGameTime = 0;
uint32_t g_statusIconBlinkLastPrune = 0;
std::map<const NC_STACK_ypabact *, EnemyIndicatorVisibilityState> g_enemyIndicatorVisibilityStates;
NC_STACK_ypaworld *g_enemyIndicatorVisibilityWorld = nullptr;
int32_t g_enemyIndicatorVisibilityLastGameTime = 0;
uint32_t g_enemyIndicatorVisibilityLastPrune = 0;
std::map<std::string, bool> g_speechEventLoggedUsed;
std::map<std::string, bool> g_speechEventLoggedFallback;
GuiList lstvw2;
int dword_5BAFA0;
int dword_5BAFA4;
int dword_5BAFA8;
GuiList exit_menu;
int dword_5C8B78;
int dword_5C8B7C;
int dword_5C8B80;
int dword_5C8B84;
int dword_5C8B88;
squadMan squadron_manager;
yw_infolog info_log;
int dword_516510;
int dword_516514;
int dword_516518;
int dword_51651C;
float flt_5164F4 = 0.0;
float flt_516504 = 0.0;
float flt_51650C = 0.0;
float flt_516524;
float flt_51652C;
float flt_516528;
float flt_516530;
tehMap robo_map;
NC_STACK_ypaworld *dword_5BAA60; // For sort func
energPanel up_panel;
GuiList gui_lstvw;
bzd bzda;

uint8_t yw_GetConfiguredUiOpacity(Common::Ini::Key &key, int fallback)
{
    return (uint8_t)System::IniConf::GetUiOpacity(key, fallback);
}

uint8_t yw_GetMapOpacity()
{
    return yw_GetConfiguredUiOpacity(System::IniConf::UiMapOpacity,
                                     System::IniConf::UiMapDefaultOpacity);
}

std::string StatusIconTrimPath(std::string path)
{
    const size_t begin = path.find_first_not_of(" \t\r\n");
    if ( begin == std::string::npos )
        return "";

    const size_t end = path.find_last_not_of(" \t\r\n");
    return path.substr(begin, end - begin + 1);
}

bool StatusIconResourceExists(const std::string &path)
{
    if ( path.empty() )
        return false;

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");
    const bool exists = uaFileExist("rsrc:" + path);
    Common::Env.SetPrefix("rsrc", oldRsrc);
    return exists;
}

const std::string &StatusIconConfiguredPath(Common::Ini::Key &key)
{
    auto cached = g_statusIconConfiguredPathCache.find(key.Name);
    if ( cached != g_statusIconConfiguredPathCache.end() )
        return cached->second;

    std::string resolved;

    // Status icons are opt-in through Nucleus.ini. Missing, empty or invalid
    // paths disable only this icon category without affecting gameplay.
    if ( key.WasSet )
    {
        const std::string configured = StatusIconTrimPath(key.Get<std::string>());
        if ( !configured.empty() && StatusIconResourceExists(configured) )
            resolved = configured;
    }

    return g_statusIconConfiguredPathCache.emplace(key.Name, resolved).first->second;
}

const std::string &StatusIconRegenPath()
{
    return StatusIconConfiguredPath(System::IniConf::UiStatusIconRegen);
}

const std::string &StatusIconDrainPath()
{
    return StatusIconConfiguredPath(System::IniConf::UiStatusIconDrain);
}

const std::string &StatusIconDamagedPath()
{
    return StatusIconConfiguredPath(System::IniConf::UiStatusIconDamaged);
}

const std::string &StatusIconSpawnPath()
{
    return StatusIconConfiguredPath(System::IniConf::UiStatusIconSpawn);
}

const std::string &StatusIconRadarPath()
{
    return StatusIconConfiguredPath(System::IniConf::UiStatusIconRadar);
}

const std::string &StatusIconPowerPath()
{
    return StatusIconConfiguredPath(System::IniConf::UiStatusIconPower);
}

const std::string &StatusIconSprintPath()
{
    return StatusIconConfiguredPath(System::IniConf::UiStatusIconSprint);
}

const std::string &StatusIconHandbrakePath()
{
    return StatusIconConfiguredPath(System::IniConf::UiStatusIconHandbrake);
}

int yw_GetUiOwner(const NC_STACK_ypaworld *yw)
{
    if ( yw && yw->_userRobo && yw->_userRobo->_owner >= 1 &&
         yw->_userRobo->_owner <= 6 )
        return yw->_userRobo->_owner;

    return World::OWNER_RESIST;
}

const std::string &StatusIconPlasmaPath(const NC_STACK_ypaworld *yw)
{
    const int owner = yw_GetUiOwner(yw);
    const std::string cacheKey = "plasma:" + std::to_string(owner);
    auto cached = g_statusIconConfiguredPathCache.find(cacheKey);
    if ( cached != g_statusIconConfiguredPathCache.end() )
        return cached->second;

    std::string path = "Interface/Plasma/owner_" + std::to_string(owner) + "/plasma.png";
    std::string resolved;
    if ( StatusIconResourceExists(path) )
        resolved = path;

    return g_statusIconConfiguredPathCache.emplace(cacheKey, resolved).first->second;
}

void SpeechEventLogOnce(std::map<std::string, bool> &cache, const std::string &key, const char *fmt, const std::string &path)
{
    if ( cache[key] )
        return;

    cache[key] = true;
    ypa_log_out(fmt, path.c_str());
}

std::string SpeechEventFindFile(std::map<std::string, World::Speech::Variants> &cache,
                                const std::string &path)
{
    std::string authored = path;
    if ( authored.compare(0, 5, "rsrc:") == 0 ) authored.erase(0, 5);
    std::string normalized;
    if ( !uaNormalizeDataAssetPath(authored, &normalized, true) ) return "";
    const std::string relative = normalized.substr(5);
    auto cached = cache.find(normalized);
    if ( cached == cache.end() )
    {
        World::Speech::Variants variants;
        const std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");
        if ( relative.size() >= 4 && !StriCmp(relative.substr(relative.size() - 4), ".wav") )
        {
            if ( uaFileExist("rsrc:" + relative) ) variants.Files.push_back(relative);
        }
        else
        {
            const size_t slash = relative.find_last_of('/');
            const std::string directory = slash == std::string::npos ? "" : relative.substr(0, slash + 1);
            const std::string stem = slash == std::string::npos ? relative : relative.substr(slash + 1);
            FSMgr::DirIter files = uaOpenDir("rsrc:" + directory);
            for (FSMgr::iNode *file = files.getNext(); file; file = files.getNext())
            {
                if ( file->getType() == FSMgr::iNode::NTYPE_FILE &&
                     World::Speech::IsVariantFilename(file->getName(), stem) )
                    variants.Files.push_back(directory + file->getName());
            }
            std::sort(variants.Files.begin(), variants.Files.end());
            if ( variants.Files.empty() && uaFileExist("rsrc:" + relative + ".wav") )
                variants.Files.push_back(relative + ".wav");
        }
        Common::Env.SetPrefix("rsrc", oldRsrc);
        cached = cache.emplace(normalized, std::move(variants)).first;
    }
    return World::Speech::ChooseVariant(cached->second, static_cast<unsigned int>(rand()));
}

bool StatusIconIsNonRoboGun(NC_STACK_ypabact *bact)
{
    if ( !bact || bact->_bact_type != BACT_TYPES_GUN )
        return false;

    NC_STACK_ypagun *gun = (NC_STACK_ypagun *)bact;
    return !gun->IsRoboGun();
}

NC_STACK_ypabact *StatusIconGetRoboGunHostStation(NC_STACK_ypabact *bact)
{
    if ( !bact || bact->_bact_type != BACT_TYPES_GUN || StatusIconIsNonRoboGun(bact) )
        return nullptr;

    NC_STACK_ypabact *host = bact->_host_station;
    if ( !host || host == bact || host->_bact_type != BACT_TYPES_ROBO ||
         host->getBACT_pWorld() != bact->getBACT_pWorld() )
        return nullptr;

    return host;
}

bool StatusIconCanUseVehicleCapabilityUnit(NC_STACK_ypabact *bact)
{
    return bact &&
           bact->_owner != World::OWNER_0 &&
           bact->_bact_type != BACT_TYPES_MISSLE &&
           bact->_bact_type != BACT_TYPES_ROBO &&
           bact->_bact_type != BACT_TYPES_GUN &&
           bact->_status != BACT_STATUS_DEAD &&
           bact->_status != BACT_STATUS_CREATE &&
           bact->_status != BACT_STATUS_BEAM &&
           !(bact->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER));
}

bool StatusIconAdd(StatusIconList &icons, int &count, const std::string &path)
{
    if ( path.empty() )
        return false;

    for (int i = 0; i < count; i++)
    {
        if ( !StriCmp(icons[i], path) )
            return false;
    }

    if ( count >= STATUS_ICON_MAX_COUNT )
        return false;

    icons[count++] = path;
    return true;
}

bool StatusIconListContains(const StatusIconList &icons, int count, const std::string &path)
{
    for (int i = 0; i < count; i++)
    {
        if ( !StriCmp(icons[i], path) )
            return true;
    }

    return false;
}

int StatusIconFindBlinkEntry(const std::vector<StatusIconBlinkEntry> &entries, const std::string &path)
{
    for (size_t i = 0; i < entries.size(); i++)
    {
        if ( !StriCmp(entries[i].path, path) )
            return (int)i;
    }

    return -1;
}

int StatusIconGetBlinkCount()
{
    int count = System::IniConf::UiStatusIconBlinkCount.Get<int32_t>();

    if ( count < 0 )
        return 0;
    if ( count > STATUS_ICON_BLINK_MAX_COUNT )
        return STATUS_ICON_BLINK_MAX_COUNT;

    return count;
}

void StatusIconPrepareBlinkStates(NC_STACK_ypaworld *yw, uint32_t now)
{
    if ( g_statusIconBlinkWorld != yw || yw->_timeStamp < g_statusIconBlinkLastGameTime )
    {
        g_statusIconBlinkStates.clear();
        g_statusIconBlinkWorld = yw;
        g_statusIconBlinkLastGameTime = yw->_timeStamp;
        g_statusIconBlinkLastPrune = now;
        return;
    }

    g_statusIconBlinkLastGameTime = yw->_timeStamp;

    if ( (uint32_t)(now - g_statusIconBlinkLastPrune) < STATUS_ICON_BLINK_PRUNE_INTERVAL_MS )
        return;

    for (auto it = g_statusIconBlinkStates.begin(); it != g_statusIconBlinkStates.end(); )
    {
        const StatusIconBlinkState &state = it->second;
        if ( state.world != yw ||
             (uint32_t)(now - state.lastTouched) > STATUS_ICON_BLINK_STATE_TTL_MS )
            it = g_statusIconBlinkStates.erase(it);
        else
            ++it;
    }

    g_statusIconBlinkLastPrune = now;
}

bool StatusIconBlinkTransitionComplete(const StatusIconBlinkEntry &entry, int blinkCount, uint32_t now)
{
    if ( entry.mode == STATUS_ICON_BLINK_STEADY )
        return false;

    const uint32_t totalDuration = (uint32_t)blinkCount * STATUS_ICON_BLINK_PHASE_MS * 2U;
    return (uint32_t)(now - entry.transitionStart) >= totalDuration;
}

bool StatusIconBlinkPhaseVisible(const StatusIconBlinkEntry &entry, uint32_t now)
{
    if ( entry.mode == STATUS_ICON_BLINK_STEADY )
        return true;

    const uint32_t elapsed = (uint32_t)(now - entry.transitionStart);
    return ((elapsed / STATUS_ICON_BLINK_PHASE_MS) & 1U) == 0U;
}

int StatusIconBuildBlinkRenderList(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact,
                                   const StatusIconList &desiredIcons, int desiredCount,
                                   StatusIconBlinkRenderList &renderIcons)
{
    if ( !yw || !bact )
        return 0;

    const int blinkCount = StatusIconGetBlinkCount();
    if ( blinkCount <= 0 )
    {
        g_statusIconBlinkStates.erase(bact);

        for (int i = 0; i < desiredCount; i++)
        {
            renderIcons[i].path = desiredIcons[i];
            renderIcons[i].visible = true;
        }
        return desiredCount;
    }

    // Status-icon transitions and GEM notification lifetime both belong to the
    // master gameplay clock, so global F5/F6 can never leave UI animation
    // advancing on a separate real-time domain.
    const uint32_t now = yw->_timeStamp;
    StatusIconPrepareBlinkStates(yw, now);

    auto stateIt = g_statusIconBlinkStates.find(bact);
    if ( stateIt == g_statusIconBlinkStates.end() )
    {
        if ( desiredCount <= 0 )
            return 0;

        StatusIconBlinkState state;
        state.world = yw;
        state.gid = bact->_gid;
        state.lastTouched = now;
        stateIt = g_statusIconBlinkStates.emplace(bact, state).first;
    }

    StatusIconBlinkState &state = stateIt->second;
    if ( state.world != yw || state.gid != bact->_gid )
    {
        state = StatusIconBlinkState();
        state.world = yw;
        state.gid = bact->_gid;
    }

    state.lastTouched = now;

    for (StatusIconBlinkEntry &entry : state.entries)
        entry.desired = false;

    for (int i = 0; i < desiredCount; i++)
    {
        int entryIndex = StatusIconFindBlinkEntry(state.entries, desiredIcons[i]);
        if ( entryIndex < 0 )
        {
            if ( state.entries.size() >= (size_t)STATUS_ICON_MAX_COUNT )
            {
                for (auto it = state.entries.begin(); it != state.entries.end(); ++it)
                {
                    if ( !StatusIconListContains(desiredIcons, desiredCount, it->path) )
                    {
                        state.entries.erase(it);
                        break;
                    }
                }
            }

            if ( state.entries.size() < (size_t)STATUS_ICON_MAX_COUNT )
            {
                StatusIconBlinkEntry entry;
                entry.path = desiredIcons[i];
                entry.mode = STATUS_ICON_BLINK_IN;
                entry.transitionStart = now;
                entry.desired = true;
                state.entries.push_back(entry);
                entryIndex = (int)state.entries.size() - 1;
            }
        }

        if ( entryIndex >= 0 )
        {
            StatusIconBlinkEntry &entry = state.entries[entryIndex];
            entry.desired = true;

            if ( entry.mode == STATUS_ICON_BLINK_OUT )
            {
                entry.mode = STATUS_ICON_BLINK_IN;
                entry.transitionStart = now;
            }
        }
    }

    for (StatusIconBlinkEntry &entry : state.entries)
    {
        if ( !entry.desired && entry.mode != STATUS_ICON_BLINK_OUT )
        {
            entry.mode = STATUS_ICON_BLINK_OUT;
            entry.transitionStart = now;
        }

        if ( entry.mode == STATUS_ICON_BLINK_IN &&
             StatusIconBlinkTransitionComplete(entry, blinkCount, now) )
            entry.mode = STATUS_ICON_BLINK_STEADY;
    }

    std::vector<StatusIconBlinkEntry> ordered;
    ordered.reserve(STATUS_ICON_MAX_COUNT);

    for (int i = 0; i < desiredCount; i++)
    {
        int entryIndex = StatusIconFindBlinkEntry(state.entries, desiredIcons[i]);
        if ( entryIndex >= 0 )
            ordered.push_back(state.entries[entryIndex]);
    }

    for (const StatusIconBlinkEntry &entry : state.entries)
    {
        if ( ordered.size() >= (size_t)STATUS_ICON_MAX_COUNT )
            break;

        if ( entry.desired )
            continue;

        if ( entry.mode == STATUS_ICON_BLINK_OUT &&
             !StatusIconBlinkTransitionComplete(entry, blinkCount, now) )
            ordered.push_back(entry);
    }

    state.entries.swap(ordered);

    int renderCount = 0;
    for (const StatusIconBlinkEntry &entry : state.entries)
    {
        renderIcons[renderCount].path = entry.path;
        renderIcons[renderCount].visible = StatusIconBlinkPhaseVisible(entry, now);
        renderCount++;
    }

    if ( state.entries.empty() )
        g_statusIconBlinkStates.erase(stateIt);

    return renderCount;
}

uint8_t ApplyWireframeTintComponent(uint8_t value, float tint)
{
    float scaled = (float)value * tint;

    if ( scaled < 0.0f )
        scaled = 0.0f;
    else if ( scaled > 255.0f )
        scaled = 255.0f;

    return (uint8_t)(scaled + 0.5f);
}

SDL_Color ApplyWireframeTint(SDL_Color color, const World::TVisualTint *tint)
{
    if ( !tint || tint->IsNeutral() )
        return color;

    if ( tint->ColorizesRGB() )
    {
        const uint8_t intensity = std::max(color.r, std::max(color.g, color.b));
        color.r = ApplyWireframeTintComponent(intensity, tint->r);
        color.g = ApplyWireframeTintComponent(intensity, tint->g);
        color.b = ApplyWireframeTintComponent(intensity, tint->b);
    }

    color.a = ApplyWireframeTintComponent(color.a, tint->a);
    return color;
}

int ResolveUnitDisplayVehicleProtoId(NC_STACK_ypaworld *yw, const NC_STACK_ypabact *bact)
{
    if ( !yw || !bact )
        return -1;

    int protoId = bact->_mimic_disguise_vehicleID ? bact->_mimic_disguise_vehicleID : bact->_vehicleID;

    if ( protoId < 0 || (size_t)protoId >= yw->_vhclProtos.size() )
        return -1;

    return protoId;
}

bool StatusIconIsSvgPath(const std::string &path)
{
    const size_t extPos = path.find_last_of('.');
    if ( extPos == std::string::npos || extPos + 1 >= path.size() )
        return false;

    return !StriCmp(path.substr(extPos + 1), "svg");
}

NC_STACK_bitmap *StatusIconLoad(const std::string &path, int width, int height)
{
    const bool sizedSvg = StatusIconIsSvgPath(path) &&
                          width >= 0 && height >= 0 &&
                          (width > 0 || height > 0);

    std::string cacheKey = path;
    if ( sizedSvg )
    {
        cacheKey += "|svg@";
        cacheKey += std::to_string(width);
        cacheKey += "x";
        cacheKey += std::to_string(height);
    }

    auto it = g_statusIconCache.find(cacheKey);
    if ( it != g_statusIconCache.end() )
        return it->second.bitmap;

    // OpenNeoUA UI assets live below Data and must not inherit the temporary
    // rsrc prefix of the currently loaded SET. Use the same shared resolver
    // already used by StatusIconResourceExists(), then restore the caller's
    // resource context immediately after loading.
    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");

    IDVList attrs;
    attrs.Add(NC_STACK_rsrc::RSRC_ATT_NAME, path);
    attrs.Add(NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1);

    if ( sizedSvg )
    {
        attrs.Add(NC_STACK_bitmap::BMD_ATT_WIDTH, (int32_t)width);
        attrs.Add(NC_STACK_bitmap::BMD_ATT_HEIGHT, (int32_t)height);

        // NC_STACK_rsrc shares resources by path only. Sized SVG variants must
        // remain private here; the UI cache above provides path+size reuse.
        attrs.Add(NC_STACK_rsrc::RSRC_ATT_TRYSHARED, (int32_t)0);
    }

    StatusIconCacheEntry entry;
    entry.bitmap = Utils::ProxyLoadImage(attrs);

    Common::Env.SetPrefix("rsrc", oldRsrc);

    auto inserted = g_statusIconCache.emplace(cacheKey, entry);
    return inserted.first->second.bitmap;
}

void StatusIconRenderBitmap(NC_STACK_ypaworld *yw, NC_STACK_bitmap *bitmap, int left, int top, int size, uint8_t opacity)
{
    if ( !bitmap || !bitmap->GetBitmap() )
        return;

    const float halfW = (float)yw->_screenSize.x * 0.5;
    const float halfH = (float)yw->_screenSize.y * 0.5;

    GFX::rstr_arg204 arg;
    arg.pbitm = bitmap->GetBitmap();
    arg.float4 = Common::FRect(-1.0, -1.0, 1.0, 1.0);
    arg.float14 = Common::FRect(
        ((float)left / halfW) - 1.0,
        ((float)top / halfH) - 1.0,
        ((float)(left + size) / halfW) - 1.0,
        ((float)(top + size) / halfH) - 1.0);
    arg.opacity = opacity;

    GFX::Engine.raster_func204(&arg);
}

SDL_Color yw_GetUiOwnerTintColor(NC_STACK_ypaworld *yw, int owner)
{
    SDL_Color tint = {255, 255, 255, 255};
    if ( yw && owner > World::OWNER_0 )
        tint = yw->GetColor(owner);

    tint.a = 255;
    return tint;
}

void StatusIconRenderBitmapTinted(NC_STACK_ypaworld *yw, NC_STACK_bitmap *bitmap,
                                  int left, int top, int size,
                                  const SDL_Color *tint,
                                  uint8_t opacity)
{
    if ( !bitmap || !bitmap->GetBitmap() )
        return;

    if ( !tint || !bitmap->GetBitmap()->swTex )
    {
        StatusIconRenderBitmap(yw, bitmap, left, top, size, opacity);
        return;
    }

    SDL_Surface *surface = bitmap->GetBitmap()->swTex;
    Uint8 oldR = 255, oldG = 255, oldB = 255;
    SDL_BlendMode oldBlendMode = SDL_BLENDMODE_NONE;
    SDL_GetSurfaceColorMod(surface, &oldR, &oldG, &oldB);
    SDL_GetSurfaceBlendMode(surface, &oldBlendMode);
    SDL_SetSurfaceColorMod(surface, tint->r, tint->g, tint->b);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    StatusIconRenderBitmap(yw, bitmap, left, top, size, opacity);

    SDL_SetSurfaceColorMod(surface, oldR, oldG, oldB);
    SDL_SetSurfaceBlendMode(surface, oldBlendMode);
}

YwLoadedUiBitmap yw_LoadNeutralOwnerTintSvg(const std::string &path,
                                                    int width,
                                                    int height)
{
    YwLoadedUiBitmap loaded;
    if ( path.empty() || !StatusIconResourceExists(path) )
        return loaded;

    loaded.bitmap = StatusIconLoad(path, width, height);
    if ( loaded.bitmap && loaded.bitmap->GetBitmap() )
        loaded.ownerTint = true;
    else
        loaded.bitmap = nullptr;

    return loaded;
}

bool StatusIconCanUseMobilePowerUnit(NC_STACK_ypabact *bact)
{
    return StatusIconCanUseVehicleCapabilityUnit(bact) &&
           bact->_energy > 0 &&
           bact->_energy_max > 0;
}

void StatusIconCollectMountedUnitGunIcons(const std::vector<World::TRoboGun> &guns, const std::string &fallbackIcon, StatusIconList &icons, int &iconCount)
{
    for (const World::TRoboGun &gun : guns)
    {
        if ( gun.robo_gun_type <= 0 )
            continue;

        if ( !gun.icon.empty() )
            StatusIconAdd(icons, iconCount, gun.icon);
        else
            StatusIconAdd(icons, iconCount, fallbackIcon);
    }
}

int StatusIconCollect(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, StatusIconList &icons)
{
    if ( !bact || !vhcl )
        return 0;

    int iconCount = 0;

    if ( bact->_active_debuff.active )
        StatusIconAdd(icons, iconCount, bact->_active_debuff.icon);

    if ( bact->_damaged_fx_active )
        StatusIconAdd(icons, iconCount, StatusIconDamagedPath());

    if ( bact->HasActiveBuff() )
        StatusIconAdd(icons, iconCount, bact->_buff.icon);

    StatusIconAdd(icons, iconCount, bact->GetProgressiveWeaponOverheatIcon());

    // Tank Sprint may still be finishing its force/pitch ramp-down after the
    // vehicle has already come to a complete stop. At that point the gameplay
    // state remains untouched, but the Sprint icon is no longer desired, so
    // the shared Status Icon system starts its normal blink-out transition.
    const bool stoppedTankSprint = bact->_bact_type == BACT_TYPES_TANK &&
                                   fabs(bact->_fly_dir_length) <= 0.001f;
    if ( yw && yw->IsPlayerSprintActiveFor(bact) && !stoppedTankSprint )
        StatusIconAdd(icons, iconCount, StatusIconSprintPath());

    // The handbrake is a local player input state, not a permanent vehicle
    // capability. Show its icon only while the current player-controlled unit
    // is in a live control state and the explicit handbrake input is held.
    const bool localPlayerHandbrakeActive = yw &&
                                            bact == yw->_userUnit &&
                                            (bact->_oflags & BACT_OFLAG_USERINPT) &&
                                            (bact->_status == BACT_STATUS_NORMAL ||
                                             bact->_status == BACT_STATUS_IDLE) &&
                                            bact->_handbrakeHeld;
    if ( localPlayerHandbrakeActive )
        StatusIconAdd(icons, iconCount, StatusIconHandbrakePath());

    if ( StatusIconCanUseVehicleCapabilityUnit(bact) )
    {
        if ( bact->CanUseCarrierSpawn() )
            StatusIconAdd(icons, iconCount, StatusIconSpawnPath());

        if ( bact->_radar >= 2 )
            StatusIconAdd(icons, iconCount, StatusIconRadarPath());

        if ( StatusIconCanUseMobilePowerUnit(bact) )
            StatusIconCollectMountedUnitGunIcons(bact->_unitGuns, vhcl->unit_gun_icon, icons, iconCount);

        if ( yw && yw->IsValidMobilePowerGenerator(bact) )
            StatusIconAdd(icons, iconCount, StatusIconPowerPath());
    }

    // Regen/Drain icons and global unit FX consume the same world-side state.
    // Icon assets, blink phase and HUD visibility do not participate in the
    // decision, so disabling a PNG never disables the gameplay-linked FX.
    const uint8_t energyState = yw ? yw->GetUnitEnergyVisualState(bact) : UNIT_ENERGY_VISUAL_NONE;
    if ( energyState & UNIT_ENERGY_VISUAL_REGEN )
        StatusIconAdd(icons, iconCount, StatusIconRegenPath());
    if ( energyState & UNIT_ENERGY_VISUAL_DRAIN )
        StatusIconAdd(icons, iconCount, StatusIconDrainPath());

    return iconCount;
}

void StatusIconRenderBlinkList(NC_STACK_ypaworld *yw, const StatusIconBlinkRenderList &icons,
                               int iconCount, int left, int top, int size,
                               uint8_t opacity)
{
    for (int i = 0; i < iconCount; i++)
    {
        if ( icons[i].visible )
        {
            NC_STACK_bitmap *bitmap = StatusIconLoad(icons[i].path);
            if ( bitmap )
                StatusIconRenderBitmap(yw, bitmap, left, top, size, opacity);
        }

        left += size + STATUS_ICON_SPACING;
    }
}

void StatusIconRenderWorld(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, int barLeft, int barTop, int barWidth, int barHeight, uint8_t opacity, int bottomReserve, int *markerCenterX)
{
    if ( markerCenterX )
        *markerCenterX = barLeft + barWidth / 2 - yw->_screenSize.x / 2;

    // A Host Station and its integrated Guns are one gameplay entity for
    // status presentation. Keep the Gun's normal world HP UI, but do not draw
    // a second Buff/Debuff/status row above it.
    if ( bact && bact->_bact_type == BACT_TYPES_GUN &&
         !StatusIconIsNonRoboGun(bact) )
    {
        g_statusIconBlinkStates.erase(bact);
        return;
    }

    StatusIconList desiredIcons;
    int desiredCount = StatusIconCollect(yw, bact, vhcl, desiredIcons);

    StatusIconBlinkRenderList icons;
    int iconCount = StatusIconBuildBlinkRenderList(yw, bact, desiredIcons, desiredCount, icons);

    if ( iconCount <= 0 )
        return;

    int iconBlockWidth = iconCount * STATUS_ICON_SIZE + (iconCount - 1) * STATUS_ICON_SPACING;
    int left = barLeft + (barWidth - iconBlockWidth) / 2;
    int top = barTop - bottomReserve - STATUS_ICON_SIZE - STATUS_ICON_SPACING;

    if ( left < 4 )
        left = 4;
    if ( left > yw->_screenSize.x - iconBlockWidth - 4 )
        left = yw->_screenSize.x - iconBlockWidth - 4;
    if ( top < 4 )
        top = 4;

    if ( markerCenterX )
        *markerCenterX = left + iconBlockWidth / 2 - yw->_screenSize.x / 2;

    StatusIconRenderBlinkList(yw, icons, iconCount, left, top, STATUS_ICON_SIZE, opacity);
}

GFX::TGLColor yw_ReadStatusBarTint(Common::Ini::Key &key, const GFX::TGLColor &fallback)
{
    const std::vector<std::string> parts = Stok::Split(key.Get<std::string>(), "_, \t");
    if ( parts.size() != 4 )
        return fallback;

    int component[4] = {0, 0, 0, 0};
    try
    {
        for (int i = 0; i < 4; i++)
        {
            size_t used = 0;
            component[i] = std::stoi(parts[i], &used, 10);
            if ( used != parts[i].size() || component[i] < 0 || component[i] > 255 )
                return fallback;
        }
    }
    catch (...)
    {
        return fallback;
    }

    return GFX::TGLColor(component[0] / 255.0f, component[1] / 255.0f,
                         component[2] / 255.0f, component[3] / 255.0f);
}

const YWProceduralStatusBarConfig &yw_GetProceduralStatusBarConfig()
{
    static const YWProceduralStatusBarConfig config = []()
    {
        YWProceduralStatusBarConfig out;
        out.hpEnabled = System::IniConf::GfxWorldNewHpBarEnable.Get<bool>();
        out.hpFullTint = yw_ReadStatusBarTint(System::IniConf::GfxWorldNewHpBarFullTint, out.hpFullTint);
        out.hpLowTint = yw_ReadStatusBarTint(System::IniConf::GfxWorldNewHpBarLowTint, out.hpLowTint);
        out.hpEmptyTint = yw_ReadStatusBarTint(System::IniConf::GfxWorldNewHpBarEmptyTint, out.hpEmptyTint);
        out.shieldEnabled = System::IniConf::GfxWorldNewShieldBarEnable.Get<bool>();
        out.shieldTint = yw_ReadStatusBarTint(System::IniConf::GfxWorldNewShieldBarTint, out.shieldTint);
        return out;
    }();
    return config;
}

GFX::TGLColor yw_StatusBarTintLerp(const GFX::TGLColor &lowTint,
                                           const GFX::TGLColor &fullTint,
                                           float ratio, uint8_t opacity)
{
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    const float opacityMul = opacity / 255.0f;
    return GFX::TGLColor(lowTint.r + (fullTint.r - lowTint.r) * ratio,
                         lowTint.g + (fullTint.g - lowTint.g) * ratio,
                         lowTint.b + (fullTint.b - lowTint.b) * ratio,
                         (lowTint.a + (fullTint.a - lowTint.a) * ratio) * opacityMul);
}

// Vertical strip painted by the procedural HP bar inside its tile cell: half
// the cell height, vertically centered. Shared by layout code so the bars that
// stack under the HP row always use the same geometry.
void yw_GetProceduralHpStripRect(int top, int cellHeight, int *outTop, int *outHeight)
{
    const int renderHeight = std::max(1, cellHeight / 2);
    if ( outTop )
        *outTop = top + (cellHeight - renderHeight) / 2;
    if ( outHeight )
        *outHeight = renderHeight;
}

bool yw_RenderProceduralHpBar(int left, int top, int squareCount, int value,
                                      int maxValue, int cellWidth, int cellHeight,
                                      uint8_t opacity,
                                      bool hideFilled, bool hideEmpty)
{
    const YWProceduralStatusBarConfig &config = yw_GetProceduralStatusBarConfig();
    if ( !config.hpEnabled || squareCount <= 0 || maxValue <= 0 ||
         cellWidth <= 0 || cellHeight <= 0 )
        return false;

    value = std::max(0, std::min(value, maxValue));
    const float ratio = (float)value / (float)maxValue;

    const GFX::TGLColor &fullTint = config.hpFullTint;
    const GFX::TGLColor &lowTint = config.hpLowTint;
    const GFX::TGLColor &emptyTint = config.hpEmptyTint;
    const GFX::TGLColor activeTint = yw_StatusBarTintLerp(lowTint, fullTint, ratio, opacity);
    GFX::TGLColor inactiveTint = emptyTint;
    inactiveTint.a *= opacity / 255.0f;

    // Keep the V2 50% vertical thickness, but render the bar as one continuous
    // filled strip instead of reproducing the old MAPMISC segmented glyphs.
    int renderTop = 0;
    int renderHeight = 0;
    yw_GetProceduralHpStripRect(top, cellHeight, &renderTop, &renderHeight);
    const float barLeft = (float)left;
    const float barRight = (float)(left + squareCount * cellWidth);
    const float barBottom = (float)(renderTop + renderHeight);
    const float activeRight = barLeft + (barRight - barLeft) * ratio;

    if ( !hideEmpty )
        GFX::Engine.DrawVirtualUISolidRect(barLeft, (float)renderTop,
                                           barRight, barBottom, inactiveTint);

    if ( !hideFilled && activeRight > barLeft )
        GFX::Engine.DrawVirtualUISolidRect(barLeft, (float)renderTop,
                                           activeRight, barBottom, activeTint);

    return true;
}

// OpenNeoUA custom: world Shield bar (gfx.world_new_shield_bar_enable).
// Deliberately thinner and shorter than the HP bar so it reads as a secondary
// strip, and centered under it. It replaces the classic world Shield row only
// while it is enabled; the cockpit Shield is not affected.
void yw_GetWorldShieldBarRect(int hpLeft, int hpTop, int hpWidth,
                                     int cellHeight, bool proceduralHp,
                                     int *outLeft, int *outTop,
                                     int *outWidth, int *outHeight)
{
    int hpBottom = hpTop + cellHeight;
    if ( proceduralHp )
    {
        int stripTop = 0;
        int stripHeight = 0;
        yw_GetProceduralHpStripRect(hpTop, cellHeight, &stripTop, &stripHeight);
        hpBottom = stripTop + stripHeight;
    }

    const int barWidth = std::max(1, (hpWidth * 3) / 4);
    const int barHeight = std::max(1, cellHeight / 4);

    if ( outLeft )
        *outLeft = hpLeft + (hpWidth - barWidth) / 2;
    if ( outTop )
        *outTop = hpBottom + 1;
    if ( outWidth )
        *outWidth = barWidth;
    if ( outHeight )
        *outHeight = barHeight;
}

// Single tint fill plus a translucent track of the same color, so the missing
// part stays readable without adding a second color parameter.
void yw_RenderProceduralShieldBar(int left, int top, int width, int height,
                                         int value, int maxValue, uint8_t opacity)
{
    const YWProceduralStatusBarConfig &config = yw_GetProceduralStatusBarConfig();
    if ( !config.shieldEnabled || width <= 0 || height <= 0 || maxValue <= 0 )
        return;

    value = std::max(0, std::min(value, maxValue));
    const float ratio = (float)value / (float)maxValue;
    const float opacityMul = opacity / 255.0f;

    // Keep the empty track clearly readable without competing with the fill.
    constexpr float trackAlpha = 0.4f;

    GFX::TGLColor trackTint = config.shieldTint;
    trackTint.a *= trackAlpha * opacityMul;
    GFX::TGLColor fillTint = config.shieldTint;
    fillTint.a *= opacityMul;

    const float barLeft = (float)left;
    const float barRight = (float)(left + width);
    const float barBottom = (float)(top + height);
    const float fillRight = barLeft + (barRight - barLeft) * ratio;

    GFX::Engine.DrawVirtualUISolidRect(barLeft, (float)top, barRight, barBottom, trackTint);

    if ( fillRight > barLeft )
        GFX::Engine.DrawVirtualUISolidRect(barLeft, (float)top, fillRight, barBottom, fillTint);
}

void yw_RenderUnitSquareBar(NC_STACK_ypaworld *yw, CmdStream *cur, int left, int top, int squareCount, int value, int maxValue, uint8_t filledTile, uint8_t emptyTile, uint8_t opacity)
{
    if ( !yw || !cur || squareCount <= 0 || maxValue <= 0 )
        return;

    if ( value < 0 )
        value = 0;
    else if ( value > maxValue )
        value = maxValue;

    const bool isHpBar = filledTile == 2 && emptyTile == 6;
    // The procedural path covers HP only. The world Shield row is left out when
    // the new world HP bar or the new world Shield bar is active (see
    // yw_RenderUnitLifeBar); the personal cockpit Shield remains on the classic
    // MAPMISC squares through sub_4E4F80().
    if ( isHpBar &&
         yw_RenderProceduralHpBar(left, top, squareCount, value, maxValue,
                                   yw->_guiTiles[50]->map[0].w, yw->_guiTiles[50]->h,
                                   opacity) )
    {
        return;
    }

    FontUA::select_tileset(cur, 50);
    FontUA::set_center_xpos(cur, left - (yw->_screenSize.x / 2) );
    FontUA::set_center_ypos(cur, top - (yw->_screenSize.y / 2) );

    int step = maxValue / squareCount;
    if ( step <= 0 )
        step = 1;

    int threshold = step / 2;

    for (int i = 1; i <= squareCount; i++)
    {
        if ( threshold > value )
            FontUA::store_u8(cur, emptyTile);
        else
            FontUA::store_u8(cur, filledTile);

        threshold += step;
    }
}

bool StatusIconCanRenderCockpitUnit(NC_STACK_ypabact *bact)
{
    if ( !bact ||
         bact->_owner == World::OWNER_0 ||
         bact->_vehicleID < 0 ||
         bact->_bact_type == BACT_TYPES_MISSLE ||
         bact->_bact_type == BACT_TYPES_ROBO ||
         bact->_status == BACT_STATUS_DEAD ||
         bact->_status == BACT_STATUS_CREATE ||
         bact->_status == BACT_STATUS_BEAM ||
         (bact->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER)) )
        return false;

    if ( bact->_bact_type == BACT_TYPES_GUN )
    {
        if ( StatusIconIsNonRoboGun(bact) )
            return bact->_energy_max > 0;

        NC_STACK_ypabact *host = StatusIconGetRoboGunHostStation(bact);
        return host && host->_owner != World::OWNER_0 && host->_energy_max > 0 &&
               host->_status != BACT_STATUS_DEAD &&
               host->_status != BACT_STATUS_CREATE &&
               host->_status != BACT_STATUS_BEAM &&
               !(host->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER));
    }

    return bact->_energy_max > 0;
}

void StatusIconRenderCockpit(NC_STACK_ypaworld *yw, sklt_wis *wis, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, float hudX, float hudY)
{
    if ( !yw || !wis )
        return;

    float nameY = hudY + wis->field_92 * 12.0;
    int nameCenterX = (yw->_screenSize.x / 2) + (yw->_screenSize.x / 2) * hudX;
    int nameCenterY = (yw->_screenSize.y / 2) + (yw->_screenSize.y / 2) * nameY;
    int belowNameTop = nameCenterY + yw->_fontH + 4;

    if ( !StatusIconCanRenderCockpitUnit(bact) || !vhcl )
        return;

    NC_STACK_ypabact *statusBact = bact;
    World::TVhclProto *statusVhcl = vhcl;

    // First-person control of an integrated Host Station Gun shows the Host
    // Station's status state instead of creating a separate Gun status state.
    NC_STACK_ypabact *host = StatusIconGetRoboGunHostStation(bact);
    if ( host )
    {
        const int hostProtoId = ResolveUnitDisplayVehicleProtoId(yw, host);
        if ( hostProtoId < 0 || (size_t)hostProtoId >= yw->_vhclProtos.size() )
            return;

        statusBact = host;
        statusVhcl = &yw->_vhclProtos[hostProtoId];
    }

    StatusIconList desiredIcons;
    int desiredCount = StatusIconCollect(yw, statusBact, statusVhcl, desiredIcons);

    StatusIconBlinkRenderList icons;
    int iconCount = StatusIconBuildBlinkRenderList(yw, statusBact, desiredIcons, desiredCount, icons);
    if ( iconCount <= 0 )
        return;

    int iconBlockWidth = iconCount * STATUS_ICON_SIZE + (iconCount - 1) * STATUS_ICON_SPACING;

    int left = nameCenterX - (iconBlockWidth / 2);
    int top = belowNameTop;

    int maxLeft = yw->_screenSize.x - iconBlockWidth - 4;
    if ( left > maxLeft )
        left = maxLeft;
    if ( left < 4 )
        left = 4;
    if ( top < 4 )
        top = 4;
    if ( top > yw->_screenSize.y - STATUS_ICON_SIZE - 4 )
        top = yw->_screenSize.y - STATUS_ICON_SIZE - 4;

    StatusIconRenderBlinkList(yw, icons, iconCount, left, top, STATUS_ICON_SIZE);
}

////////////////////////////////////////
////////////////////////////////////////





void NC_STACK_ypaworld::sb_0x4c87fc(const std::string &a2, GuiBase *lstvw)
{
    GuiWinOpen(&lstvw2);
    GuiWinToFront(&lstvw2);

    dword_5BAF98 = a2;
    dword_5BAFAC = lstvw;
    dword_5BAF9C = 3;
}

void NC_STACK_ypaworld::sub_449DE8(const std::string &a2)
{
    // OpenNeoUA: legacy online help pages are obsolete/dead.
    // Keep this as a harmless compatibility stub for old callers/hotkeys.
    (void)a2;
    _helpURL.clear();
    _helpSavedURL.clear();
}

void sub_4C3A54(NC_STACK_ypaworld *yw)
{
    if ( yw->_userUnit )
    {
        if ( yw->_userUnit == yw->_userRobo || yw->_playerInHSGun )
        {
            bzda.field_1CC = 0x29;

            if ( bzda.field_8E0 )
            {
                bzda.field_1CC = 0x2B;

                if ( yw->_activeCmdrRemapIndex != -1 )
                    bzda.field_1CC = 0x2F;
            }

            if ( bzda.field_8E4 )
                bzda.field_1CC |= 0x10;

            bzda.field_1D4 = 1;
        }
        else
        {
            bzda.field_1D4 = 1;
            bzda.field_1CC = 41;
        }

        if ( yw->_userRobo->_status == BACT_STATUS_DEAD )
            bzda.field_1CC = 0;
    }
    else
    {
        bzda.field_1D4 = 0;
        bzda.field_1CC = 0;
    }

    bzda.field_1D0 &= bzda.field_1CC;
    bzda.field_1D8 &= bzda.field_1D4;

    if ( !bzda.field_1D0 )
        bzda.field_1D0 = bzda.field_1CC & 1;
}

void sb_0x451034__sub8(NC_STACK_ypaworld *yw)
{
    up_panel = energPanel();

    TileMap *f30 = yw->_guiTiles[30];

    up_panel.field_1CC = f30->h;
    up_panel.field_1DC = f30->map[65].w;
    up_panel.flags = 0;
    up_panel.field_1D0 = up_panel.field_1DC + f30->map[84].w;
    up_panel.buttons.resize(4);


    int v8 = yw->_screenSize.x - 4 * (up_panel.field_1DC + f30->map[84].w);

    up_panel.field_1D8 = v8 / 4;
    up_panel.field_1D4 = v8 / 8;
    up_panel.field_1E0 = 2;
    up_panel.field_1E4 = up_panel.field_1D0 - up_panel.field_1DC - 4;

    up_panel.x = 0;
    up_panel.y = 0;
    up_panel.w = yw->_screenSize.x;
    up_panel.h = up_panel.field_1CC;

    for (int i = 0; i < 4; i++)
        up_panel.buttons[i] = ButtonBox(up_panel.field_1D4 + (up_panel.field_1D0 + up_panel.field_1D8) * i,    0,
                                        up_panel.field_1D0,   up_panel.field_1CC);

    up_panel.cmdCommands.reserve(512);

    Input::Engine.AddClickBoxFront(&up_panel);
}

Common::Point sub_4F681C(vec2d in)
{
    if ( robo_map.field_1E8 & 0x100 )
    {
        float a1a = in.x - robo_map.field_1D8;
        float a2a = in.y - robo_map.field_1DC;

        in.x = robo_map.field_1D8 + robo_map.field_264 * a1a + flt_5164F4 * a2a;
        in.y = robo_map.field_1DC + flt_516504 * a1a + flt_51650C * a2a;
    }

    return Common::Point( robo_map.field_200 + dround(in.x / robo_map.field_1E0) - robo_map.field_1F0
                        , robo_map.field_204 + dround(-in.y / robo_map.field_1E4) - robo_map.field_1F4 );
}

void sub_4F68FC(float a3, float a4, float a5, float a6, SDL_Color a7)
{
    GFX::Engine.raster_func217(a7);
    GFX::Engine.raster_func201( Common::Line( sub_4F681C({a3, a4}), sub_4F681C({a5, a6}) )  );
}

bool GetPlayerRoboAIBehaviorMapTarget(NC_STACK_ypaworld *yw, vec3d *target)
{
    if ( !yw || !yw->_userRobo || !target )
        return false;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);
    if ( !robo )
        return false;

    return robo->GetPlayerRoboAIBehaviorMoveTarget(target);
}

void sub_4F6980(CmdStream *cur, float a1, float a2, char a3, int a4, int a5, int screenYOffset)
{
    Common::Point tmp = sub_4F681C( {a1, a2} );

    int v7 = tmp.x - a4 / 2 - robo_map.field_200;
    int v8 = tmp.y - a5 / 2 - robo_map.field_204 + screenYOffset;
    int v9 = v7 + a4;
    int v10 = v8 + a5;

    if ( v7 < robo_map.field_1F8 && v8 < robo_map.field_1FC && v9 > 0 && v10 > 0 )
    {
        int v11;

        if ( v7 >= 0 )
        {
            v11 = 0;
        }
        else
        {
            v11 = -v7;
            v7 = 0;
        }

        int v33;

        if ( v8 >= 0 )
        {
            v33 = 0;
        }
        else
        {
            v33 = -v8;
            v8 = 0;
        }

        int v34;

        if ( v9 <= robo_map.field_1F8 )
            v34 = 0;
        else
            v34 = robo_map.field_1F8 - v7;

        int v35;

        if ( v10 <= robo_map.field_1FC )
            v35 = 0;
        else
            v35 = robo_map.field_1FC - v8;

        FontUA::set_center_xpos(cur, robo_map.field_200 + v7);
        FontUA::set_center_ypos(cur, robo_map.field_204 + v8);

        if ( v33 )
        {
            FontUA::set_yoff(cur, v33);
        }
        else if ( v35 )
        {
            FontUA::set_yheight(cur, v35);
        }

        if ( v11 )
        {
            FontUA::set_xoff(cur, v11);
        }
        else if ( v34 )
        {
            FontUA::set_xwidth(cur, v34);
        }

        FontUA::store_u8(cur, a3);
    }
}

bool yw_IsDirectPlayerControlledUnit(NC_STACK_ypaworld *yw,
                                             NC_STACK_ypabact *bact)
{
    return yw && bact &&
           yw->_userUnit == bact &&
           (bact->_oflags & BACT_OFLAG_USERINPT);
}

void sub_4F72E8(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    if ( !bact || bact->ShouldHideFromStrategicUI() )
        return;

    // OpenNeoUA invisible: do not draw strategic target/command lines for a cloaked unit.
    if ( bact->IsInvisibleUnrevealed() )
        return;

    // Keep this renderer close to vanilla. Spectator target filtering belongs in
    // gameplay/target selection, not in strategic-line drawing; filtering lines
    // here proved unsafe during in-game testing. Only null-check target pointers.
    const bool directPlayerControl = yw_IsDirectPlayerControlledUnit(yw, bact);

    if ( bact != yw->_userRobo && bact->_host_station != bact->_parent && bact->_parent )
    {
        SDL_Color clr;
        NC_STACK_ypabact *bct;

        if ( !directPlayerControl && bact->_secndTtype == BACT_TGT_TYPE_UNIT )
        {
            clr = yw->GetColor(10);
            bct = bact->_secndT.pbact;
        }
        else
        {
            if ( !bact->_parent )
                return;

            clr = yw->GetColor(11);
            bct = bact->_parent;
        }

        if ( !bct )
            return;

        sub_4F68FC(bact->_position.x, bact->_position.z, bct->_position.x, bct->_position.z, clr);
        return;
    }

    // A directly controlled unit may still have a transient player lock target,
    // but that is not an AI order and must not be represented by strategic
    // attack/waypoint lines. Squad relationship lines above remain available.
    if ( directPlayerControl )
        return;

    if ( bact->_secndTtype == BACT_TGT_TYPE_UNIT )
    {
        NC_STACK_ypabact *bct = bact->_secndT.pbact;
        if ( !bct )
            return;

        sub_4F68FC(bact->_position.x, bact->_position.z, bct->_position.x, bct->_position.z,  yw->GetColor(10));
        return;
    }

    if ( bact->_secndTtype == BACT_TGT_TYPE_CELL )
    {
        sub_4F68FC(bact->_position.x, bact->_position.z, bact->_sencdTpos.x, bact->_sencdTpos.z, yw->GetColor(10));
    }
    else
    {
        if ( bact->_primTtype == BACT_TGT_TYPE_CELL )
        {
            sub_4F68FC(bact->_position.x, bact->_position.z, bact->_primTpos.x, bact->_primTpos.z, yw->GetColor(9));
        }
        else if ( bact->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            if ( !bact->_primT.pbact )
                return;

            sub_4F68FC(bact->_position.x, bact->_position.z, bact->_primT.pbact->_position.x, bact->_primT.pbact->_position.z, yw->GetColor(9));
        }

        if ( bact->_status_flg & BACT_STFLAG_WAYPOINT )
        {
            if ( bact->_status_flg & BACT_STFLAG_WAYPOINTCCL )
            {
                SDL_Color clr = yw->GetColor(9);

                for (int i = 1; i < bact->_waypoints_count; i++)
                    sub_4F68FC(bact->_waypoints[i - 1].x, bact->_waypoints[i - 1].z, bact->_waypoints[i].x, bact->_waypoints[i].z, clr);

                sub_4F68FC(bact->_waypoints[bact->_waypoints_count - 1].x, bact->_waypoints[bact->_waypoints_count - 1].z, bact->_waypoints[0].x, bact->_waypoints[0].z, clr);
            }
            else if ( bact->_waypoints_count > 0 )
            {
                SDL_Color clr = yw->GetColor(9);

                for (int i = bact->_current_waypoint; i < bact->_waypoints_count - 1; i++)
                    sub_4F68FC(bact->_waypoints[i].x, bact->_waypoints[i].z, bact->_waypoints[i + 1].x, bact->_waypoints[i + 1].z, clr);
            }
        }
    }
}

void  sb_0x4f8f64__sub1__sub0(NC_STACK_ypaworld *yw)
{
    for( NC_STACK_ypabact* &bact : yw->_unitsList )
    {
        if ( bact->_bact_type == BACT_TYPES_ROBO && bact != yw->_userRobo )
        {
            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(bact);

            for (int i = 0; i < 8; i++)
            {
                int v10 = 0;

                switch ( i )
                {
                case 0:
                    if ( robo->_roboRadarValue > 0 )
                        v10 = robo->_roboRadarCellID;
                    break;

                case 1:
                    if ( robo->_roboSafetyValue > 0 )
                        v10 = robo->_roboSafetyCellID;
                    break;

                case 2:
                    if ( robo->_roboPowerValue > 0 )
                        v10 = robo->_roboPowerCellID;
                    break;

                case 3:
                    if ( robo->_roboEnemyValue > 0 )
                        v10 = robo->_roboEnemyCellID;
                    break;

                case 4:
                    if ( robo->_roboConqValue > 0 )
                        v10 = robo->_roboConqCellID;
                    break;

                case 5:
                    if ( robo->_roboPositionValue > 0 )
                        v10 = robo->_roboPositionCellID;
                    break;

                case 6:
                    if ( robo->_roboExploreValue > 0 )
                        v10 = robo->_roboExploreCellID;
                    break;

                case 7:
                    if ( robo->_roboDangerValue > 0 )
                        v10 = robo->_roboDangerCellID;
                    break;

                default:
                    break;
                }

                if ( v10 )
                {
                    vec2d secC = World::SectorIDToCenterPos2( {v10 % yw->_mapSize.x, v10 / yw->_mapSize.x} );

                    sub_4F68FC(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(i));
                }
            }


            if ( robo->_roboBuildingDuty )
            {
                vec2d secC = World::SectorIDToCenterPos2( {robo->_roboBuildingCellID % yw->_mapSize.x, robo->_roboBuildingCellID / yw->_mapSize.x} );

                if ( (yw->_timeStamp / 300) & 1 )
                    sub_4F68FC(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(0));
            }

            if ( robo->_roboVehicleDuty )
            {
                vec2d secC = World::SectorIDToCenterPos2( {robo->_roboVehicleCellID % yw->_mapSize.x, robo->_roboVehicleCellID / yw->_mapSize.x} );

                if ( (yw->_timeStamp / 300) & 1 )
                {
                    sub_4F68FC(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(7));
                }
            }
        }
    }
}

void sb_0x4f8f64__sub1(NC_STACK_ypaworld *yw)
{
    if ( robo_map.field_1E8 & 0x100 )
    {
        if ( yw->_userRobo != yw->_userUnit && yw->_userUnit->_parent )
        {
            sub_4F68FC(yw->_userUnit->_position.x, yw->_userUnit->_position.z, yw->_userUnit->_parent->_position.x, yw->_userUnit->_parent->_position.z, yw->GetColor(11));

            if ( yw->_userUnit->IsParentMyRobo() &&
                 !yw_IsDirectPlayerControlledUnit(yw, yw->_userUnit) )
            {
                int v7 = 0;
                float a5, a6;

                if ( yw->_userUnit->_primTtype == BACT_TGT_TYPE_CELL )
                {
                    a5 = yw->_userUnit->_primTpos.x;
                    a6 = yw->_userUnit->_primTpos.z;
                    v7 = 1;
                }
                else if ( yw->_userUnit->_primTtype == BACT_TGT_TYPE_UNIT )
                {
                    a5 = yw->_userUnit->_primT.pbact->_position.x;
                    a6 = yw->_userUnit->_primT.pbact->_position.z;
                    v7 = 1;
                }

                if ( v7 )
                    sub_4F68FC(yw->_userUnit->_position.x, yw->_userUnit->_position.z, a5, a6, yw->GetColor(9));
            }
        }
    }
    else
    {
        if ( robo_map.field_1EC & 4 )
        {
            for (NC_STACK_ypabact* const comm: yw->_cmdrsRemap)
            {
                if (comm->_status != BACT_STATUS_DEAD)
                    sub_4F72E8(yw, comm);

                for( NC_STACK_ypabact* &kid : comm->_kidList )
                {
                    if ( kid->_status != BACT_STATUS_DEAD && !kid->ShouldHideFromStrategicUI() )
                        sub_4F72E8(yw, kid);
                }
            }
        }
        else
        {
            if ( yw->_activeCmdrRemapIndex != -1 )
            {
                NC_STACK_ypabact *tmp = yw->_cmdrsRemap[yw->_activeCmdrRemapIndex];

                if ( tmp->_status != BACT_STATUS_DEAD )
                    sub_4F72E8(yw, tmp);
            }
        }

        if ( yw->_userUnit )
        {
            sub_4F68FC(yw->_userUnit->_position.x, 0.0, yw->_userUnit->_position.x, -yw->_mapLength.y, yw->GetColor(13));
            sub_4F68FC(0.0, yw->_userUnit->_position.z, yw->_mapLength.x, yw->_userUnit->_position.z, yw->GetColor(13));
        }

        if ( !yw->IsSpectatorControlled() )
        {
            vec3d playerRoboTarget;
            if ( GetPlayerRoboAIBehaviorMapTarget(yw, &playerRoboTarget) )
            {
                sub_4F68FC(yw->_userRobo->_position.x,
                           yw->_userRobo->_position.z,
                           playerRoboTarget.x,
                           playerRoboTarget.z,
                           yw->GetColor(9));
            }
        }

        if ( robo_map.field_1E8 & 0x200 )
        {
            int wh = yw->_screenSize.x / 2;
            int hh = yw->_screenSize.y / 2;

            SDL_Color selectionColor = yw->GetColor(12);
            yw_GetFactionUiAccent(yw, &selectionColor);
            GFX::Engine.raster_func217(selectionColor);

            GFX::Engine.raster_func201( {dword_516510 - wh,
                                         dword_516514 - hh,
                                         dword_516518 - wh,
                                         dword_516514 - hh} );
            GFX::Engine.raster_func201( {dword_516518 - wh,
                                         dword_516514 - hh,
                                         dword_516518 - wh,
                                         dword_51651C - hh} );
            GFX::Engine.raster_func201( {dword_516518 - wh,
                                         dword_51651C - hh,
                                         dword_516510 - wh,
                                         dword_51651C - hh} );
            GFX::Engine.raster_func201( {dword_516510 - wh,
                                         dword_51651C - hh,
                                         dword_516510 - wh,
                                         dword_516514 - hh} );
        }
    }

    if ( yw->_showDebugMode )
        sb_0x4f8f64__sub1__sub0(yw);
}

void sb_0x4f8f64__sub2__sub0(CmdStream *cur, float a1, float a2, char a3, int a4, int a5)
{
    Common::Point tmp = sub_4F681C( {a1, a2} );

    int v8 = tmp.x - a4 / 2 - robo_map.field_200;
    int v9 = tmp.y - a4 / 2 - robo_map.field_204;
    int v10 = v8 + a4;
    int v11 = v9 + a5;

    if ( v8 < robo_map.field_1F8 && v9 < robo_map.field_1FC && v10 > 0 && v11 > 0 )
    {
        int v31;
        if ( v8 >= 0 )
        {
            v31 = 0;
        }
        else
        {
            v31 = -v8;
            v8 = 0;
        }

        int v12;
        if ( v9 >= 0 )
        {
            v12 = 0;
        }
        else
        {
            v12 = -v9;
            v9 = 0;
        }

        int v30;
        if ( v10 <= robo_map.field_1F8 )
            v30 = 0;
        else
            v30 = robo_map.field_1F8 - v8;

        int v32;
        if ( v11 <= robo_map.field_1FC )
            v32 = a5;
        else
            v32 = robo_map.field_1FC - v9;

        FontUA::set_center_xpos(cur, robo_map.field_200 + v8);
        FontUA::set_center_ypos(cur, robo_map.field_204 + v9);

        if ( v12 )
            FontUA::set_yoff(cur, v12);

        if ( v32 )
            FontUA::set_yheight(cur, v32);

        if ( v31 )
        {
            FontUA::set_xoff(cur, v31);
        }
        else if ( v30 )
        {
            FontUA::set_xwidth(cur, v30);
        }

        FontUA::store_u8(cur, a3);
    }
}

void sb_0x4f8f64__sub2(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int a4, v4;

    switch ( robo_map.field_1EE )
    {
    case 0:
        v4 = 16;
        a4 = 4;
        break;

    case 1:
        v4 = 17;
        a4 = 8;
        break;

    case 2:
        v4 = 18;
        a4 = 16;
        break;

    case 3:
        v4 = 19;
        a4 = 32;
        break;

    case 4:
        v4 = 20;
        a4 = 64;
        break;

    default:
        break;
    }

    if ( a4 >= 8 )
    {
        if ( robo_map.field_1EC & 1 )
        {
            FontUA::select_tileset(cur, v4);

            for ( const auto &ps : yw->_powerStations )
            {
                if ( ps.second.pCell )
                {
                    if ( yw_IsStrategicMapCellVisible(yw, ps.second.pCell) )
                    {
                        if ( ps.second.EffectivePower > 0 )
                        {
                            int v9 = 0;

                            vec2d tmp = World::SectorIDToCenterPos2(ps.second.CellId);

                            if ( ps.second.EffectivePower <= 32)
                                v9 = 0x80;
                            else if ( ps.second.EffectivePower <= 64)
                                v9 = 0x81;
                            else if ( ps.second.EffectivePower <= 96)
                                v9 = 0x82;
                            else if ( ps.second.EffectivePower <= 128)
                                v9 = 0x83;
                            else if ( ps.second.EffectivePower <= 160)
                                v9 = 0x84;
                            else if ( ps.second.EffectivePower <= 192)
                                v9 = 0x85;
                            else if ( ps.second.EffectivePower <= 224)
                                v9 = 0x86;
                            else if ( ps.second.EffectivePower <= 256)
                                v9 = 0x87;

                            sub_4F6980(cur, tmp.x, tmp.y, 0x89, a4, a4);
                            sb_0x4f8f64__sub2__sub0(cur, tmp.x, tmp.y, v9, a4, a4 / 8);
                        }
                    }
                }
            }

            for ( const TMapGem &gem : yw->_techUpgrades )
            {
                cellArea &cell = yw->SectorAt(gem.CellId);
                if ( yw_IsStrategicMapCellVisible(yw, &cell) )
                {
                    int v13 = 1;

                    if ( cell.PurposeType == cellArea::PT_TECHUPGRADE )
                    {
                        if ( yw->_timeStamp / 300 & 1 )
                            v13 = 0;
                    }

                    if ( v13 )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( gem.CellId );
                        sub_4F6980(cur, tmp.x, tmp.y, 0x88, a4, a4);
                    }
                }
            }

            for ( const TMapGate &gate : yw->_levelInfo.Gates )
            {
                if ( gate.PCell->PurposeType == cellArea::PT_GATECLOSED )
                {
                    if ( yw_IsStrategicMapCellVisible(yw, gate.PCell) )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( gate.CellId );
                        sub_4F6980(cur, tmp.x, tmp.y, 0x93, a4, a4);
                    }

                    for ( const TMapKeySector &ks : gate.KeySectors )
                    {
                        if ( ks.PCell )
                        {
                            if ( yw_IsStrategicMapCellVisible(yw, ks.PCell) )
                            {
                                if ( ks.PCell->owner == yw->_userRobo->_owner || yw->_timeStamp / 300 & 1 )
                                {
                                    vec2d tmp = World::SectorIDToCenterPos2( ks.CellId );
                                    sub_4F6980(cur, tmp.x, tmp.y, 0x8A, a4, a4);
                                }
                            }
                        }
                    }
                }
                else if ( gate.PCell->PurposeType == cellArea::PT_GATEOPENED )
                {
                    if ( yw_IsStrategicMapCellVisible(yw, gate.PCell) )
                    {
                        if ( yw->_timeStamp / 300 & 1 )
                        {
                            vec2d tmp = World::SectorIDToCenterPos2( gate.CellId );
                            sub_4F6980(cur, tmp.x, tmp.y, 0x94, a4, a4);
                        }
                    }
                }
            }

            for (const TMapSuperItem &sitem : yw->_levelInfo.SuperItems)
            {
                int v29 = 0;
                int v61 = 0;

                if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
                {
                    v29 = 0x8E;

                    if ( sitem.State == TMapSuperItem::STATE_INACTIVE )
                        v61 = 0x8B;
                    else if ( sitem.State == TMapSuperItem::STATE_ACTIVE ||
                              sitem.State == TMapSuperItem::STATE_STOPPED )
                        v61 = 0x8C;
                    else if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
                        v61 = 0x8D;
                }
                else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
                {
                    v29 = 0x92;

                    if ( sitem.State == TMapSuperItem::STATE_INACTIVE )
                        v61 = 0x8F;
                    else if ( sitem.State == TMapSuperItem::STATE_ACTIVE ||
                              sitem.State == TMapSuperItem::STATE_STOPPED )
                        v61 = 0x90;
                    else if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
                        v61 = 0x91;
                }

                if ( v61 )
                {
                    int v34 = 0;

                    if ( yw_IsStrategicMapCellVisible(yw, sitem.PCell) )
                    {
                        if ( sitem.PCell->owner == yw->_userRobo->_owner || yw->_timeStamp / 300 & 1 )
                            v34 = 1;
                    }

                    if ( v34 )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( sitem.CellId );
                        sub_4F6980(cur, tmp.x, tmp.y, v61, a4, a4);
                    }

                    if ( v29 )
                    {
                        for ( const TMapKeySector &ks : sitem.KeySectors )
                        {
                            if ( ks.PCell )
                            {
                                if ( yw_IsStrategicMapCellVisible(yw, ks.PCell) )
                                {
                                    if ( ks.PCell->owner == yw->_userRobo->_owner || yw->_timeStamp / 500 & 1 )
                                    {
                                        vec2d tmp = World::SectorIDToCenterPos2( ks.CellId );
                                        sub_4F6980(cur, tmp.x, tmp.y, v61, a4, a4);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void sub_4F6DFC(NC_STACK_ypaworld *yw, CmdStream *cur, int height, int width, NC_STACK_ypabact *bact, int a6)
{
    // OpenNeoUA invisible: cloaked stealth units never appear on the strategic map / radar
    // (icon, view triangle and health bar are all drawn from here).
    if ( bact->_status != BACT_STATUS_DEAD && !bact->IsInvisibleUnrevealed() && !bact->IsHiddenFor( yw->GetPlayerOwner() ) )
    {
        int v8;

        if ( a6 )
        {
            if ( bact->_bact_type == BACT_TYPES_MISSLE )
                v8 = bact->_owner + 1;
            else
            {
                int protoId = ResolveUnitDisplayVehicleProtoId(yw, bact);
                v8 = protoId >= 0 ? yw->_vhclProtos[protoId].type_icon : 0;
            }
        }
        else
        {
            switch ( bact->_bact_type )
            {
            case BACT_TYPES_BACT:
                v8 = bact->_owner + 9;
                break;

            case BACT_TYPES_ROBO:
                switch ( robo_map.field_1EE )
                {
                case 0:
                case 1:
                    v8 = bact->_owner + 0x30;
                    break;

                case 2:
                    v8 = bact->_owner + 0x28;
                    break;

                case 3:
                    v8 = bact->_owner + 0x20;
                    break;

                case 4:
                    v8 = bact->_owner + 0x18;
                    break;
                }
                break;

            case BACT_TYPES_MISSLE:
                v8 = bact->_owner + 1;
                break;

            case BACT_TYPES_TANK:
            case BACT_TYPES_CAR:
                v8 = bact->_owner + 0x19;
                break;

            case BACT_TYPES_FLYER:
                v8 = bact->_owner + 0x11;
                break;

            case BACT_TYPES_UFO:
                v8 = bact->_owner + 0x21;
                break;

            case BACT_TYPES_GUN:
                v8 = bact->_owner + 0x29;
                break;

            default:
                v8 = 0x41;
                break;
            }
        }

        sub_4F6980(cur, bact->_position.x, bact->_position.z, v8, height, width);

        if ( bact->_bact_type != BACT_TYPES_MISSLE && robo_map.field_1EE > 2 )
        {
            SDL_Color clr = yw->GetColor(bact->_owner);

            int v31 = 0;
            float v33, v35;

            float v21;
            float v22;
            float v23;
            float v24;
            float v25;

            if ( bact == yw->_userRobo )
            {
                v33 = bact->_viewer_rotation.m02;
                v35 = bact->_viewer_rotation.m22;

                if ( bact == yw->_userUnit )
                {
                    v21 = -380.0;
                    v22 = 380.0;

                    v23 = -300.0;
                    v24 = 600.0;
                    v25 = -300.0;

                    v31 = 1;

                    clr = yw->GetColor(13);
                }
                else
                {
                    v21 = -85.0;
                    v22 = 85.0;

                    v23 = 0.0;
                    v24 = 700.0;
                    v25 = 0.0;
                }
            }
            else if ( bact == yw->_userUnit )
            {
                v33 = yw->_viewerRotation.m02;
                v35 = yw->_viewerRotation.m22;

                v21 = -190.0;
                v22 = 190.0;

                v23 = -200.0;
                v24 = 400.0;
                v25 = -200.0;

                v31 = 1;

                clr = yw->GetColor(13);
            }
            else
            {
                v33 = bact->_rotation.m02;
                v35 = bact->_rotation.m22;

                v21 = -40.0;
                v22 = 40.0;

                v23 = 0.0;
                v24 = 350.0;
                v25 = 0.0;
            }

            float v30 = sqrt(POW2(v33) + POW2(v35));
            float v34, v36;

            if ( v30 <= 0.0 )
            {
                v36 = 1.0;
                v34 = 0.0;
            }
            else
            {
                v34 = v33 / v30;
                v36 = v35 / v30;
            }

            float v14 = -v34;

            float a3 = v36 * v21 + v14 * v23 + bact->_position.x;
            float v28 = v34 * v21 + v36 * v23 + bact->_position.z;
            float v16 = v36 * 0.0 + v14 * v24 + bact->_position.x;
            float v15 = v34 * 0.0 + v36 * v24 + bact->_position.z;

            sub_4F68FC(a3, v28, v16, v15, clr);

            float v27 = v36 * v22 + v14 * v25 + bact->_position.x;
            float v29 = v34 * v22 + v36 * v25 + bact->_position.z;

            sub_4F68FC(v27, v29, v16, v15, clr);

            if ( v31 )
                sub_4F68FC(a3, v28, v27, v29, clr); // Also draw base of view triangle
        }

        // This function is reached only for units in cells visible through
        // MapViewMask. When the Energy layer is enabled, show HP for every
        // currently visible non-neutral unit, including enemies and remote
        // Host Stations.
        const bool drawHealthBar = bact->_owner != World::OWNER_0
            && (!yw->IsSpectatorControlled() || !yw->IsSpectatorBact(bact));
        const bool isPlayerHostStation = bact->_bact_type == BACT_TYPES_ROBO
            && bact == yw->_userRobo;

        const bool drawHostStationAtSmallZoom = bact->_bact_type == BACT_TYPES_ROBO;

        if ( bact->_bact_type != BACT_TYPES_MISSLE && !isPlayerHostStation && drawHealthBar )
        {
            if ( robo_map.field_1EC & 4 && bact->_energy_max > 0
                    && (robo_map.field_1EE > 2 || drawHostStationAtSmallZoom) )
            {
                float v32 = (float)bact->_energy / (float)bact->_energy_max;
                int v19;

                if ( v32 > 0.25 )
                {
                    if ( v32 > 0.5 )
                    {
                        if ( v32 > 0.75 )
                            v19 = 0x83;
                        else
                            v19 = 0x82;
                    }
                    else
                    {
                        v19 = 0x81;
                    }
                }
                else
                {
                    v19 = 0x80;
                }

                if ( bact->_bact_type == BACT_TYPES_ROBO )
                {
                    // Host Station icons use MAPROBO, which has no lifemeter
                    // glyphs. Use the same zoom-dependent lifemeter tileset
                    // and dimensions as ordinary units, then restore MAPROBO
                    // for the caller's next Host Station icon.
                    const int healthTileset = yw_RoboMapVehicleTileset();
                    const int healthWidth = yw->_guiTiles[healthTileset]->map[128].w;
                    const int healthHeight = yw->_guiTiles[healthTileset]->h;

                    FontUA::select_tileset(cur, healthTileset);

                    const int hostIconHeight = yw->_guiTiles[1]->h;
                    const int healthBarYOffset = -(hostIconHeight + healthHeight) / 2 + 14;

                    sub_4F6980(cur, bact->_position.x, bact->_position.z, v19,
                                healthWidth, healthHeight, healthBarYOffset);
                    FontUA::select_tileset(cur, 1);
                }
                else
                {
                    // Keep the vanilla vehicle lifemeter dimensions and
                    // tileset. Only Host Stations need the separate MAPROBO
                    // lifemeter path above.
                    sub_4F6980(cur, bact->_position.x, bact->_position.z, v19,
                                height, width);
                }
            }
        }
    }
}

NC_STACK_ypabact * sub_4D3C3C(NC_STACK_ypabact *bact)
{
    if ( !bact )
        return nullptr;

    if ( !bact->_parent )
        return nullptr;

    if ( bact->_parent != bact->_host_station )
        return bact->_parent;

    return bact;
}

void sub_4F7BE8(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact, int a2, int a4, int a5, int a6)
{
    if ( bact )
    {
        if ( bact->ShouldHideFromStrategicUI() )
            return;

        // OpenNeoUA invisible: no strategic-map marker dots for a cloaked unit/squad member.
        if ( bact->IsInvisibleUnrevealed() )
            return;

        FontUA::select_tileset(cur, a2);

        sub_4F6980(cur, bact->_position.x, bact->_position.z, a4, a5, a6);

        for( NC_STACK_ypabact* &kid : bact->_kidList )
        {
            if ( kid->_status != BACT_STATUS_DEAD && !kid->ShouldHideFromStrategicUI() )
                sub_4F6980(cur, kid->_position.x, kid->_position.z, a4, a5, a6);
        }
    }
}

int NC_STACK_ypaworld::sb_0x4f8f64__sub3__sub0(const Common::Point &sector)
{
    int enrg = 0;

    for( NC_STACK_ypabact* &v2 : _cells(sector).unitsList )
    {
        if ( v2->_status != BACT_STATUS_DEAD && v2->_status != BACT_STATUS_BEAM )
        {
            if ( v2->_bact_type != BACT_TYPES_ROBO && v2->_bact_type != BACT_TYPES_MISSLE && v2->_bact_type != BACT_TYPES_GUN )
                enrg += (v2->_energy_max + 99) / 100;
        }
    }

    return enrg;
}

void sb_0x4f8f64__sub3__sub1(NC_STACK_ypaworld *yw, const std::string &labl, int tileset_id, CmdStream *cur, int a4, int a5)
{
    int v8 = yw->_guiTiles[tileset_id]->GetWidth(labl);

    Common::Point tmp = sub_4F681C( World::SectorIDToPos2( {a4, a5} ) );

    int v9 = tmp.x - robo_map.field_200;
    int v10 = tmp.y - robo_map.field_204;

    if ( tmp.x - robo_map.field_200 > 0 && v10 > 0 && tmp.x - robo_map.field_200 + v8 < robo_map.field_1F8 && tmp.y - robo_map.field_204 + yw->_guiTiles[tileset_id]->h < robo_map.field_1FC )
    {
        FontUA::select_tileset(cur, tileset_id);
        FontUA::set_center_xpos(cur, robo_map.field_200 + v9);
        FontUA::set_center_ypos(cur, robo_map.field_204 + v10);

        for ( uint8_t c : labl )
            FontUA::store_s8(cur, c);
    }
}

void sb_0x4f8f64__sub3(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int v111;
    int a5;
    int a4;
    int v114;
    int v110;
    int v7;
    int v99;

    switch ( robo_map.field_1EE )
    {
    case 0:
        v111 = 4;
        v114 = 59;
        v110 = 16;
        a5 = yw->_guiTiles[59]->h;
        a4 = yw->_guiTiles[59]->map[1].w;
        v7 = 23;
        v99 = 19;
        break;

    case 1:
        v114 = 59;
        v111 = 8;
        v110 = 17;
        a5 = yw->_guiTiles[59]->h;
        a4 = yw->_guiTiles[59]->map[1].w;
        v7 = 23;
        v99 = 19;
        break;

    case 2:
        v111 = 16;
        v114 = 60;
        v110 = 18;
        a5 = yw->_guiTiles[60]->h;
        a4 = yw->_guiTiles[60]->map[1].w;
        v7 = 22;
        v99 = 18;
        break;

    case 3:
        v110 = 19;
        v111 = 32;
        v114 = 61;
        a5 = yw->_guiTiles[61]->h;
        a4 = yw->_guiTiles[61]->map[1].w;
        v7 = 21;
        v99 = 17;
        break;

    case 4:
        v110 = 20;
        v111 = 64;
        v114 = 28;
        a5 = yw->_guiTiles[28]->h;
        a4 = yw->_guiTiles[28]->map[1].w;
        v7 = 20;
        v99 = 16;
        break;
    }


    switch ( yw->_doAction )
    {
    case World::DOACTION_2:
        if ( !(robo_map.field_1E8 & 0x200) )
        {
            if ( yw->_guiActFlags & 0x20 )
            {
                FontUA::select_tileset(cur, v114);

                sub_4F6980(cur, yw->_bactOnMouse->_position.x, yw->_bactOnMouse->_position.z, 0x86, a4, a5);
            }
            else if ( yw->_guiActFlags & 0x10 && yw->_mouseGrabbed == false)
            {
                if ( yw->_cellOnMouse->owner == yw->_userRobo->_owner && yw->_cellOnMouse->IsCanSee(yw->_userRobo->_owner) )
                {
                    FontUA::select_tileset(cur, v114);

                    sub_4F6980(cur, yw->_cellMouseIsectPos.x, yw->_cellMouseIsectPos.z, 0x88, a4, a5);
                }
                else
                {
                    FontUA::select_tileset(cur, v110);

                    sub_4F6980(cur, yw->_cellOnMouse->CenterPos.x, yw->_cellOnMouse->CenterPos.z, 66, v111, v111);
                }
            }
        }
        break;

    case World::DOACTION_5:
        FontUA::select_tileset(cur, v114);

        sub_4F6980(cur, yw->_bactOnMouse->_position.x, yw->_bactOnMouse->_position.z, 0x85, a4, a5);
        break;

    case World::DOACTION_6:
        FontUA::select_tileset(cur, v110);

        sub_4F6980(cur, yw->_cellOnMouse->CenterPos.x, yw->_cellOnMouse->CenterPos.z, 65, v111, v111);
        break;

    case World::DOACTION_8:
        if ( !(robo_map.field_1E8 & 0x200) )
        {
            if ( yw->_bactOnMouse )
            {
                if ( yw->_bactOnMouse == yw->_userRobo )
                {
                    int v22 = yw->_guiTiles[1]->h;

                    FontUA::select_tileset(cur, 1);

                    sub_4F6980(cur, yw->_bactOnMouse->_position.x, yw->_bactOnMouse->_position.z, v7, v22, v22);
                }
                else
                {
                    NC_STACK_ypabact *v25 = sub_4D3C3C(yw->_bactOnMouse);

                    if ( v25 )
                    {
                        sub_4F7BE8(yw, cur, v25, v114, 0x85, a4, a5);
                    }

                }
            }
        }
        break;

    case World::DOACTION_10:
        FontUA::select_tileset(cur, v114);

        sub_4F6980(cur, yw->_cellMouseIsectPos.x, yw->_cellMouseIsectPos.z, 0x88, a4, a5);
        break;

    default:
        break;
    }

    {
        vec3d playerRoboTarget;
        if ( GetPlayerRoboAIBehaviorMapTarget(yw, &playerRoboTarget) )
        {
            FontUA::select_tileset(cur, v114);
            sub_4F6980(cur, playerRoboTarget.x, playerRoboTarget.z, 0x88, a4, a5);
        }
    }

    if ( bzda.field_1D0 & 0x20 )
    {
        FontUA::select_tileset(cur, 1);

        int v44 = yw->_guiTiles[1]->h;

        sub_4F6980(cur, yw->_userRobo->_position.x, yw->_userRobo->_position.z, v99, v44, v44);
    }
    else
    {
        if ( yw->_activeCmdrRemapIndex != -1 )
        {
            NC_STACK_ypabact *v47 = yw->_cmdrsRemap[yw->_activeCmdrRemapIndex];

            if ( v47 )
            {
                sub_4F7BE8(yw, cur, v47, v114, 0x84, a4, a5);

                if ( v47->_primTtype == BACT_TGT_TYPE_CELL )
                {
                    if ( v47->_primT.pcell->owner != yw->_userRobo->_owner )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( World::PositionToSectorID(v47->_primTpos) );

                        FontUA::select_tileset(cur, v110);

                        sub_4F6980(cur, tmp.x, tmp.y, 66, v111, v111);

                        FontUA::select_tileset(cur, v114);

                        sub_4F6980(cur, v47->_primTpos.x, v47->_primTpos.z, 0x88, a4, a5);
                    }
                    else
                    {
                        FontUA::select_tileset(cur, v114);

                        sub_4F6980(cur, v47->_primTpos.x, v47->_primTpos.z, 0x88, a4, a5);
                    }
                }
                else if ( v47->_primTtype == BACT_TGT_TYPE_UNIT )
                {
                    FontUA::select_tileset(cur, v114);

                    sub_4F6980(cur, v47->_primT.pbact->_position.x, v47->_primT.pbact->_position.z, 0x86, a4, a5);
                }

                if ( v47->_status_flg & BACT_STFLAG_WAYPOINT )
                {
                    if ( v47->_waypoints_count > 0 )
                    {
                        int v68;

                        if ( v47->_status_flg & BACT_STFLAG_WAYPOINTCCL )
                            v68 = 0;
                        else
                            v68 = v47->_current_waypoint;

                        for (int i = v68; i < v47->_waypoints_count; i++)
                        {
                            FontUA::select_tileset(cur, v114);

                            sub_4F6980(cur, v47->_waypoints[i].x, v47->_waypoints[i].z, 0x88, a4, a5);
                        }
                    }
                }
            }
        }
    }

    if ( yw->_userUnit )
    {
        if ( yw->_timeStamp / 300 & 1 )
        {
            FontUA::select_tileset(cur, v114);

            sub_4F6980(cur, yw->_userUnit->_position.x, yw->_userUnit->_position.z, 0x89, a4, a5);
        }
    }

    if ( yw->_lastMsgSender )
    {
        if ( yw->_timeStamp / 300 & 1 )
        {
            FontUA::select_tileset(cur, v114);

            sub_4F6980(cur, yw->_lastMsgSender->_position.x, yw->_lastMsgSender->_position.z, 0x8A, a4, a5);
        }
    }

    for ( const TMapGate &gate : yw->_levelInfo.Gates )
    {
        if ( gate.PCell->PurposeType == cellArea::PT_GATEOPENED )
        {
            int v81 = yw->sb_0x4f8f64__sub3__sub0(gate.CellId);

            int v85 = 1;

            if ( 2 * yw->_beamEnergyCapacity < 2 * v81 )
            {
                v85 = (yw->_timeStamp / 300) & 1; // FLASHING GATES TITLE
            }

            if ( v85 )
                sb_0x4f8f64__sub3__sub1(yw,  fmt::sprintf("%d / %d", 2 * v81, 2 * yw->_beamEnergyCapacity), 0x1F, cur, gate.CellId.x, gate.CellId.y);
        }
    }
}

void sb_0x4f8f64__sub0(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    // OpenNeoUA: the strategic-map controls are now drawn in the title bar.
    // Keep this legacy hook as a no-op so the map render order stays intact.
    (void)yw;
    (void)cur;
}

void sb_0x4f8f64(NC_STACK_ypaworld *yw)
{
    yw_RenderLegacyStrategicMap(yw);
    yw_RenderCustomMapMarkers(yw);
    yw_RenderMapTitleToolbar(yw);
}

void sub_4C157C(NC_STACK_ypaworld *yw)
{
    float v1 = (robo_map.h - robo_map.field_250) * robo_map.field_1E4;
    float v3 = (robo_map.w - robo_map.field_24C) * robo_map.field_1E0;

    float v6 = (yw->_mapSize.x * World::CVSectorLength);
    int v20 = robo_map.w - robo_map.field_248;

    int v17 = ((robo_map.field_1D8 - v3 * 0.5) * v20) / v6;
    int v19 = v20 * v3 / v6;



    if ( v19 >= robo_map.field_228 )
    {
        if ( v19 > v20 )
            v19 = v20;
    }
    else
    {
        v17 -= (robo_map.field_228 - v19) >> 1;
        v19 = robo_map.field_228;
    }

    if ( v17 < 0 )
        v17 = 0;

    if ( v17 + v19 > v20 )
        v17 = v20 - v19;

    robo_map.field_1CC = v20;
    robo_map.field_1CE = v17;
    robo_map.field_1D0 = v19;




    float v15 = -(robo_map.field_1DC + v1 * 0.5);
    float v14 = (yw->_mapSize.y * World::CVSectorLength);

    v20 = robo_map.h - robo_map.field_250;
    v19 = (v20 * v1 / v14);

    int v18 = ((v20 * v15) / v14);


    if ( v19 >= robo_map.field_228 )
    {
        if ( v19 > v20 )
            v19 = v20;
    }
    else
    {
        v18 -= (robo_map.field_228 - v19) >> 1;
        v19 = robo_map.field_228;
    }

    if ( v18 < 0 )
        v18 = 0;

    if ( v19 + v18 > v20 )
        v18 = v20 - v19;

    robo_map.field_1D2 = v20;
    robo_map.field_1D4 = v18;
    robo_map.field_1D6 = v19;
}

void sub_4C0C00(NC_STACK_ypaworld *yw)
{
    int v20 = robo_map.x - (yw->_screenSize.x / 2);
    int v21 = robo_map.y - (yw->_screenSize.y / 2);
    int v22 = robo_map.w;
    int v23 = robo_map.h;
    const int horizontalTrim = std::max(0, robo_map.field_240 - kRoboMapScrollbarVisualSize);

    const int closeW = std::max(18, yw->_fontDefCloseW);
    const int toolW = std::max(18, std::min(24, yw->_fontH));
    const int toolbarIds[] = {3, 4, 5, 6, 9, 18, 19};
    const int toolCount = (int)(sizeof(toolbarIds) / sizeof(toolbarIds[0]));
    const int toolStart = std::max(0, robo_map.w - closeW - toolCount * toolW);
    const int toolbarOffsetX = 1;
    const int toolbarOffsetY = 1;

    robo_map.buttons[0] = ButtonBox(robo_map.w - closeW + toolbarOffsetX, toolbarOffsetY,
                                    closeW, yw->_fontH);
    robo_map.buttons[1] = ButtonBox(0, 0, toolStart, yw->_fontH);
    robo_map.buttons[2] = ButtonBox();
    robo_map.buttons[7] = ButtonBox();
    robo_map.buttons[8] = ButtonBox();

    for ( int i = 0; i < toolCount; i++ )
        robo_map.buttons[toolbarIds[i]] = ButtonBox(toolStart + i * toolW + toolbarOffsetX, toolbarOffsetY,
                                                    toolW, yw->_fontH);

    robo_map.cmdCommands.clear();

    // Keep FLAG_WITH_CLOSE on the window so the existing input/close lifecycle
    // remains untouched, but suppress the legacy atlas X. A matching vector X
    // is drawn by yw_RenderMapTitleToolbar() in the exact ButtonBox[0].
    GuiBase::FormateTitle(yw, v20, v21, v22,
                          Locale::Text::WinName(Locale::WINNAME_MAP),
                          &robo_map.cmdCommands, 0,
                          robo_map.flags & ~GuiBase::FLAG_WITH_CLOSE);

    FontUA::next_line(&robo_map.cmdCommands);
    // next_line() returns to the line origin set by FormateTitle. Keep this
    // frame glyph on the map's left edge; the right scrollbar has its own
    // command buffer and remains visually trimmed there.
    FontUA::set_center_xpos(&robo_map.cmdCommands, v20);
    FontUA::reset_tileset(&robo_map.cmdCommands, 13);

    FontUA::store_u8(&robo_map.cmdCommands, 65);

    FontUA::next_line(&robo_map.cmdCommands);
    FontUA::reset_tileset(&robo_map.cmdCommands, 12);

    // The visible horizontal scrollbar starts horizontalTrim pixels below
    // its logical track. Extend only the left frame's final vertical segment
    // by the same amount so the lower-left corner remains connected.
    int v13 = v23 - robo_map.field_250 - 1 + horizontalTrim;
    while (v13 > yw->_fontVScrollH)
    {
        FontUA::store_u8(&robo_map.cmdCommands, 65);
        FontUA::next_line(&robo_map.cmdCommands);

        v13 -= yw->_fontVScrollH;
    }

    if ( v13 > 1 )
    {
        FontUA::set_yheight(&robo_map.cmdCommands, v13 - 1);
        FontUA::store_u8(&robo_map.cmdCommands, 65);
        FontUA::next_line(&robo_map.cmdCommands);
    }

    FontUA::reset_tileset(&robo_map.cmdCommands, 13);
    FontUA::store_u8(&robo_map.cmdCommands, 66);
    FontUA::next_line(&robo_map.cmdCommands);

    FontUA::include(&robo_map.cmdCommands, 0);
    FontUA::include(&robo_map.cmdCommands, 1);
    FontUA::include(&robo_map.cmdCommands, 2);
    FontUA::set_end(&robo_map.cmdCommands);
}

void sub_4C0FEC(NC_STACK_ypaworld *yw)
{
    int v1 = robo_map.x - (yw->_screenSize.x / 2);
    const int horizontalTrim = std::max(0, robo_map.field_240 - kRoboMapScrollbarVisualSize);
    const int verticalTrim = std::max(0, robo_map.field_248 - kRoboMapScrollbarVisualSize);
    int v3 = robo_map.y + robo_map.h - robo_map.field_240 + horizontalTrim
             - (yw->_screenSize.y / 2);

    robo_map.buttons[13] = ButtonBox(0, robo_map.h - robo_map.field_240,
                                     robo_map.field_1CE, robo_map.field_240);

    robo_map.buttons[14] = ButtonBox(robo_map.field_1CE, robo_map.h - robo_map.field_240,
                                     robo_map.field_1D0, robo_map.field_240);

    robo_map.buttons[16] = ButtonBox(robo_map.w - robo_map.field_248,  robo_map.h - robo_map.field_240,
                                     robo_map.field_248,  robo_map.field_240);

    robo_map.buttons[15] = ButtonBox(robo_map.field_1D0 + robo_map.field_1CE,   robo_map.h - robo_map.field_240,
                                     robo_map.field_1CC - robo_map.field_1CE - robo_map.field_1D0,   robo_map.field_240);

    robo_map.t1_cmdbuf_2.clear();

    FontUA::select_tileset(&robo_map.t1_cmdbuf_2, 11);
    FontUA::set_center_xpos(&robo_map.t1_cmdbuf_2, v1);
    FontUA::set_center_ypos(&robo_map.t1_cmdbuf_2, v3);

    int v4 = robo_map.field_1CE;

    if ( v4 > 0 )
    {
        yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 65, horizontalTrim);
        if ( v4 > 1 )
        {
            FontUA::op17(&robo_map.t1_cmdbuf_2, v4);
            yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 66, horizontalTrim);
        }
    }

    yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 68, horizontalTrim);

    FontUA::op17(&robo_map.t1_cmdbuf_2, robo_map.field_1D0 + v4 - 1);

    yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 69, horizontalTrim);
    yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 70, horizontalTrim);


    int v11 = v4 + robo_map.field_1D0;
    if ( v11 < robo_map.field_1CC )
    {
        if ( v11 < robo_map.field_1CC - 1 )
        {
            FontUA::op17(&robo_map.t1_cmdbuf_2, robo_map.field_1CC - 1);

            yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 66, horizontalTrim);
        }
        yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 67, horizontalTrim);
    }

    if ( verticalTrim > 0 )
    {
        // Keep the resize cube trimmed, but compensate half of the removed
        // scrollbar width so it joins the two bars without overshooting.
        FontUA::op17(&robo_map.t1_cmdbuf_2,
                     robo_map.field_1CC + verticalTrim - 1);
        yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 67, horizontalTrim);

        FontUA::add_xpos(&robo_map.t1_cmdbuf_2,
                         std::max(0, verticalTrim / 2 - 1));
        FontUA::set_xoff(&robo_map.t1_cmdbuf_2, (uint8_t)verticalTrim);
    }
    yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 71, horizontalTrim);

    FontUA::set_end(&robo_map.t1_cmdbuf_2);
}

void sub_4C1214(NC_STACK_ypaworld *yw)
{
    const int horizontalTrim = std::max(0, robo_map.field_240 - kRoboMapScrollbarVisualSize);
    const int verticalTrim = std::max(0, robo_map.field_248 - kRoboMapScrollbarVisualSize);
    int v2 = robo_map.x + robo_map.w - robo_map.field_248 + verticalTrim
             - (yw->_screenSize.x / 2);
    int v3 = robo_map.y + robo_map.field_23C - (yw->_screenSize.y / 2);

    robo_map.buttons[10] = ButtonBox(robo_map.w - robo_map.field_248,    robo_map.field_23C,
                                     robo_map.field_248,   robo_map.field_1D4);

    robo_map.buttons[11] = ButtonBox(robo_map.w - robo_map.field_248,    robo_map.field_1D4 + robo_map.field_23C,
                                     robo_map.field_248,   robo_map.field_1D6);

    robo_map.buttons[12] = ButtonBox(robo_map.w - robo_map.field_248,    robo_map.field_1D6 + robo_map.field_1D4 + robo_map.field_23C,
                                     robo_map.field_248,   robo_map.field_1D2 - robo_map.field_1D4 - robo_map.field_1D6);

    robo_map.t1_cmdbuf_1.clear();

    FontUA::set_center_xpos(&robo_map.t1_cmdbuf_1, v2);
    FontUA::set_center_ypos(&robo_map.t1_cmdbuf_1, v3);


    if ( robo_map.field_1D4 > 0 )
    {
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 67, verticalTrim);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);

        int v9 = robo_map.field_1D4 - 1;
        if ( robo_map.field_1D6 <= 0
                && robo_map.field_1D2 - robo_map.field_1D4 <= 0 )
        {
            // If the scrollbar has no knob or lower track, this is the
            // final vertical segment. Extend it to the trimmed bottom bar.
            v9 += horizontalTrim;
        }

        while (v9 >= yw->_fontVScrollH )
        {
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 66, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            v9 -= yw->_fontVScrollH;
        }

        if ( v9 > 0 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, v9);
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 66, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }
    }

    if ( robo_map.field_1D6 > 0 )
    {
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);
        yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 69, verticalTrim);

        FontUA::next_line(&robo_map.t1_cmdbuf_1);

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);


        int v14 = robo_map.field_1D6 - 1;
        if ( robo_map.field_1D2 - robo_map.field_1D4 - robo_map.field_1D6 <= 0 )
            v14 += horizontalTrim;

        while ( v14 > yw->_fontVScrollH )
        {
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 67, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            v14 -= yw->_fontVScrollH;
        }

        if ( v14 > 1 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, v14 - 1);
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 67, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 70, verticalTrim);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);
    }

    int v21 = robo_map.field_1D2 - robo_map.field_1D4 - robo_map.field_1D6;

    if ( v21 > 0 )
    {
        // When the lower scrollbar track is visible, continue it to the
        // shifted horizontal scrollbar instead of leaving a lower-right gap.
        v21 += horizontalTrim;
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);

        while ( v21 > yw->_fontVScrollH )
        {
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 66, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            v21 -= yw->_fontVScrollH;
        }

        if ( v21 > 1 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, v21 - 1);

            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 66, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 68, verticalTrim);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);
    }
    FontUA::set_end(&robo_map.t1_cmdbuf_1);
}

void sub_4F6114(NC_STACK_ypaworld *yw, int a2, CmdStream *cmdbuf, int a3, int a4, int a5, int a6, int a7, mapFunc ffunc, int xpos, int ypos)
{
    int v33;

    if ( a2 == 4)
        v33 = 2;
    else if ( a2 == 8)
        v33 = 3;
    else if ( a2 == 16 )
        v33 = 4;
    else if ( a2 == 32 )
        v33 = 5;
    else if ( a2 == 64 )
        v33 = 6;
    else
    {
        ypa_log_out("yw_maprnd.c/yw_RMapLego(): wrong size\n");

        FontUA::set_end(cmdbuf);
        return ;
    }

    FontUA::set_center_xpos(cmdbuf, xpos);
    FontUA::set_center_ypos(cmdbuf, ypos);

    int msk = a2 - 1;

    FontUA::reset_tileset(cmdbuf, a7);

    int row = a4;

    while (row < a6)
    {
        int v37 = 0;
        int col = a3;
        int v17 = row >> v33;

        if ( msk & row )
        {
            FontUA::set_yoff(cmdbuf, row & msk);

            row += a2 - (msk & row);
        }
        else
        {
            if ( a6 - row >= a2 )
            {
                row += a2;
            }
            else
            {
                FontUA::set_yheight(cmdbuf, a6 - row);

                row = a6;
            }
        }

        if ( col & msk )
        {
            int v37_4 = msk & col;

            int v21 = ffunc(yw, col >> v33, v17) & 0xFF;

            if ( v21 )
            {
                FontUA::set_xoff(cmdbuf, v37_4);
                FontUA::store_u8(cmdbuf, v21);

                v37 = 0;
            }
            else
            {
                v37 = a2 - v37_4;

                FontUA::add_xpos(cmdbuf, v37);
            }

            col += a2 - v37_4;
        }

        while ( col < a5 - a2 )
        {
            int v24 = ffunc(yw, col >> v33, v17) & 0xFF;

            if ( v24 )
            {
                FontUA::store_u8(cmdbuf, v24);

                v37 = 0;
            }
            else
            {
                if ( v37 <= 0 )
                {
                    v37 = a2;

                    FontUA::add_xpos(cmdbuf, v37);
                }
                else
                {
                    v37 += a2;

                    cmdbuf->pop_back();
                    cmdbuf->pop_back();
                    FontUA::store_s16(cmdbuf, v37);
                }
            }

            col += a2;
        }

        if ( a5 - col > 0 )
        {
            int v28 = ffunc(yw, col >> v33, v17) & 0xFF;
            if ( v28 )
            {
                FontUA::set_xwidth(cmdbuf, a5 - col);
                FontUA::store_u8(cmdbuf, v28);
            }
        }
        FontUA::next_line(cmdbuf);

    }
}

int sub_4F6048(NC_STACK_ypaworld *yw, int x, int y)
{
    if ( yw->IsSector( {x, y} ) )
    {
        cellArea &v4 = yw->_cells(x, y);

        if ( robo_map.MapViewMask & v4.view_mask )
        {
            if ( !v4.owner )
                return 8;
            else
                return v4.owner;
        }
    }

    return 0;
}

int sub_4F5FE0(NC_STACK_ypaworld *yw, int x, int y)
{
    if ( yw->IsSector( {x, y} ) )
    {
        cellArea &v6 = yw->_cells(x, y);

        if ( robo_map.MapViewMask & v6.view_mask )
            return yw->_secTypeArray[v6.type_id].GUIElementID;
    }

    return 0;
}

int sub_4F5CEC(NC_STACK_ypaworld *yw, int x, int y)
{
    int v9 = x / 4;
    int v8 = x & 3;
    int v7 = y / 4;
    int v27 = y & 3;

    if ( !yw->IsSector( {v9, v7} ) )
        return 0;

    cellArea &v12 = yw->_cells(v9, v7);

    if ( !(robo_map.MapViewMask & v12.view_mask) )
        return 0;

    if ( v8 && v27 )
    {
        if ( v12.SectorType == 1 )
        {
            TLego *v26 = &yw->_legoArray[ yw->GetLegoBld(&v12, 0, 0) ];
            int v25 = (16 * (v27 - 1) + v8 - 1 + v26->GUIElementID) & 0xFF;
            return v25;
        }
        else
        {
            TLego *v26 = &yw->_legoArray[ yw->GetLegoBld(&v12, v8 - 1, 2 - (v27 - 1))  ];
            return v26->GUIElementID;
        }
    }
    else
    {
        // OpenNeoUA: hide only the strategic-map grid/elevation-boundary layer.
        // LEGO terrain and building glyphs are returned by the branch above.
        return 0;
    }
}

void sb_0x4f6650__sub0(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int xpos, int ypos, int w, int h)
{
    float v7 = robo_map.field_1D8 / robo_map.field_1E0;
    float v8 = -robo_map.field_1DC / robo_map.field_1E4;
    int v9 = v7 - (w / 2);
    int v23 = w + v9;
    int v10 = v8 - (h / 2);
    int v22 = h + v10;

    if ( robo_map.field_1EC & 2 )
        sub_4F6114(yw, 4 << robo_map.field_1EE, cmdbuf, v9, v10, v23, v22, robo_map.field_1EE + 50, sub_4F6048, xpos, ypos);

    if ( robo_map.field_1EC & 1 )
    {
        switch ( robo_map.field_1EE )
        {
        case 0:
            sub_4F6114(yw, 4, cmdbuf, v9, v10, v23, v22, 43, sub_4F5FE0, xpos, ypos);
            break;

        case 1:
            sub_4F6114(yw, 8, cmdbuf, v9, v10, v23, v22, 44, sub_4F5FE0, xpos, ypos);
            break;

        case 2:
            sub_4F6114(yw, 4, cmdbuf, v9 + 2, v10 + 2, v23 + 2, v22 + 2, 42, sub_4F5CEC, xpos, ypos);
            break;

        case 3:
            sub_4F6114(yw, 8, cmdbuf, v9 + 4, v10 + 4, v23 + 4, v22 + 4, 41, sub_4F5CEC, xpos, ypos);
            break;

        case 4:
            sub_4F6114(yw, 16, cmdbuf, v9 + 8, v10 + 8, v23 + 8, v22 + 8, 40, sub_4F5CEC, xpos, ypos);
            break;
        }
    }
}

void sb_0x4f6650(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int x, int y)
{
    sb_0x4f6650__sub0(yw, cmdbuf, x, y, robo_map.w - robo_map.field_24C, robo_map.h - robo_map.field_250);
    FontUA::set_end(cmdbuf);
}

void sub_4C1814(NC_STACK_ypaworld *yw, int a2, int a3)
{
    float v15 = yw->_mapSize.x * World::CVSectorLength;
    float v12 = yw->_mapSize.y * World::CVSectorLength;

    float v4 = a2 * robo_map.field_1E0;
    float v13 = v4 * 0.5;

    if ( v4 <= v15 )
    {
        if ( robo_map.field_1D8 - v13 >= 0.0 )
        {
            if ( robo_map.field_1D8 + v13 > v15 )
                robo_map.field_1D8 = v15 - v13;
        }
        else
        {
            robo_map.field_1D8 = v13;
        }
    }
    else
    {
        robo_map.field_1D8 = v15 * 0.5;
    }

    float v8 = a3 * robo_map.field_1E4;
    float v14 = v8 * 0.5;

    if ( v8 > v12 )
    {
        robo_map.field_1DC = -v12 * 0.5;
    }
    else
    {
        if ( robo_map.field_1DC + v14 > 0.0 )
        {
            robo_map.field_1DC = -v8 * 0.5;
        }
        else
        {
            if ( robo_map.field_1DC - v14 < -v12 )
                robo_map.field_1DC = -v12 + v14;
        }
    }
}

void  sb_0x451034__sub2(NC_STACK_ypaworld *yw)
{
    robo_map.t1_cmdbuf_1.reserve(256);
    robo_map.t1_cmdbuf_2.reserve(256);
    robo_map.t1_cmdbuf_3.reserve(32768);

    robo_map.flags = (GuiBase::FLAG_CLOSED | GuiBase::FLAG_WITH_CLOSE | GuiBase::FLAG_WITH_DRAGBAR);
    robo_map.field_228 = 8;
    robo_map.field_22C = yw->_guiTiles[10]->map[65].w;
    robo_map.field_230 = yw->_guiTiles[10]->h;
    robo_map.field_234 = 4;
    robo_map.field_238 = yw->_guiTiles[0]->map[113].w;
    robo_map.field_23C = yw->_fontH;
    robo_map.field_240 = yw->_fontHScrollH;
    robo_map.field_244 = yw->_guiTiles[12]->map[65].w;
    robo_map.field_248 = yw->_fontVBScrollW;
    robo_map.field_24C = robo_map.field_248 + robo_map.field_244;
    robo_map.field_250 = robo_map.field_240 + robo_map.field_23C;
    robo_map.field_210 = robo_map.field_248 + 7 * robo_map.field_22C + robo_map.field_244 + 8;
    robo_map.field_212 = robo_map.field_240 + robo_map.field_23C + 96;
    robo_map.field_214 = 32000;
    robo_map.field_216 = 32000;
    robo_map.x = 0;
    robo_map.y = yw->_upScreenBorder;
    robo_map.w = 2 * yw->_screenSize.x / 3;
    robo_map.h = 2 * yw->_screenSize.y / 3;
    robo_map.field_1EE = 4;
    robo_map.field_1EC = 7;
    robo_map.buttons.resize(20);

    robo_map.cmdCommands.reserve(512);
    robo_map.cmdInclude = {&robo_map.t1_cmdbuf_1, &robo_map.t1_cmdbuf_2, &robo_map.t1_cmdbuf_3};

    robo_map.preDraw = yw_RenderMapBackground;
    robo_map.postDraw = sb_0x4f8f64;

    if ( yw->_userRobo )
    {
        robo_map.field_1D8 = yw->_userRobo->_position.x;
        robo_map.field_1DC = yw->_userRobo->_position.z;
    }
    else
    {
        robo_map.field_1DC = -39000.0;
        robo_map.field_1D8 = 39000.0;
    }

    robo_map.field_1E4 = 18.75f;
    robo_map.field_1E0 = 18.75f;

    robo_map.field_1ED = 0;
    robo_map.field_1E8 = 0;
    robo_map.markerMode = true;
    robo_map.markerDragIndex = -1;
    robo_map.zoomInFlashUntil = 0;
    robo_map.zoomOutFlashUntil = 0;
    robo_map.maximized = false;
    robo_map.restoreRectValid = false;
    robo_map.customMarkers.clear();
    robo_map.MapViewMask = cellArea::ViewMask(1);

    robo_map.field_254 = 0.45;
    robo_map.field_258 = 0;
    robo_map.field_25C = 0.95;
    robo_map.field_260 = 0.6;

    robo_map.field_208 = yw->_screenSize.x - robo_map.field_210;
    robo_map.field_20A = yw->_screenSize.y - yw->_downScreenBorder - robo_map.field_212;
    robo_map.field_20C = robo_map.field_210;
    robo_map.field_20E = robo_map.field_212;

    sub_4C157C(yw);
    sub_4C0C00(yw);
    sub_4C0FEC(yw);
    sub_4C1214(yw);
    MapRoboUpdateMapInt(yw);
    sub_4C1814(yw, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
}

void sub_4C32A8(NC_STACK_ypaworld *yw, TMFWinStatus *status)
{
    if ( status->Valid )
    {
        if ( status->Rect.y < yw->_upScreenBorder
                || status->Rect.x + status->Rect.w > yw->_screenSize.x
                || status->Rect.y + status->Rect.h > yw->_screenSize.y - yw->_downScreenBorder )
        {
            status->Valid = false;
        }
    }

    if ( status->Valid )
    {
        if ( yw->_roboMapStatus.Data[4] < yw->_upScreenBorder
                || yw->_roboMapStatus.Data[3] + yw->_roboMapStatus.Data[5] > yw->_screenSize.x
                || yw->_roboMapStatus.Data[6] + yw->_roboMapStatus.Data[4] > yw->_screenSize.y - yw->_downScreenBorder )
            status->Valid = false;
    }
}

void sub_4C1970(NC_STACK_ypaworld *yw, int a2)
{
    if ( a2 == 1 )
        robo_map.field_1EE++;
    else if ( a2 == 2 )
        robo_map.field_1EE--;

    if ( robo_map.field_1EE > 4 )
        robo_map.field_1EE = 4;
    else if ( robo_map.field_1EE < 1 )
        robo_map.field_1EE = 1;

    // The tactical map uses one live zoom across Host Station/vehicle contexts.
    // Keep both saved UI states aligned so save/load never revives a stale zoom.
    if ( yw )
    {
        yw->_roboMapStatus.Data[2] = robo_map.field_1EE;
        yw->_vhclMapStatus.Data[2] = robo_map.field_1EE;
    }

    switch ( robo_map.field_1EE )
    {
    case 0:
        robo_map.field_1E4 = 300.0f;
        robo_map.field_1E0 = 300.0f;
        break;

    case 1:
        robo_map.field_1E4 = 150.0f;
        robo_map.field_1E0 = 150.0f;
        break;

    case 2:
        robo_map.field_1E4 = 75.0f;
        robo_map.field_1E0 = 75.0f;
        break;

    case 3:
        robo_map.field_1E4 = 37.5f;
        robo_map.field_1E0 = 37.5f;
        break;

    case 4:
        robo_map.field_1E4 = 18.75f;
        robo_map.field_1E0 = 18.75f;
        break;
    }
}

int sb_0x451034__sub3(NC_STACK_ypaworld *yw)
{
    bzda = bzd();

    int v3 = yw->_screenSize.x - 11 * yw->_iconOrderW;
    bzda.field_8FC = v3 / 4;

    // OpenNeoUA default: keep the Genesis/Creation panel compact and anchored to
    // the left edge. nucleus.ini can still override all three geometry values.
    int configuredGenesisWidth = System::IniConf::UiGenesisListWidth.Get<int32_t>();
    if ( configuredGenesisWidth <= 0 )
        configuredGenesisWidth = System::IniConf::UiGenesisListDefaultWidth;
    bzda.field_900 = std::min(yw->_screenSize.x, std::max(60, configuredGenesisWidth));

    int configuredGenesisX = System::IniConf::UiGenesisListX.Get<int32_t>();
    if ( configuredGenesisX < 0 )
        configuredGenesisX = System::IniConf::UiGenesisListDefaultX;
    bzda.field_904 = std::min(configuredGenesisX, std::max(0, yw->_screenSize.x - bzda.field_900));

    // Preserve the legacy spacing inside the non-Genesis toolbar block, then
    // shift that whole block as one unit so its final Analyzer/Menu pair is
    // flush with the right edge. Render positions and click boxes share these
    // same absolute anchors below, avoiding the historical desync caused by
    // moving only the Genesis/create origin.
    bzda.field_908 = yw->_iconOrderW + bzda.field_8FC + bzda.field_8FC / 2;
    bzda.field_90C = 2 * yw->_iconOrderW + bzda.field_908 + bzda.field_8FC;
    bzda.field_910 = 5 * yw->_iconOrderW + bzda.field_90C + bzda.field_8FC;

    const int toolbarRight = bzda.field_910 + 2 * yw->_iconOrderW;
    const int toolbarShiftRight = std::max(0, yw->_screenSize.x - toolbarRight);
    bzda.field_908 += toolbarShiftRight;
    bzda.field_90C += toolbarShiftRight;
    bzda.field_910 += toolbarShiftRight;
    bzda.field_914 = 1;
    bzda.field_918 = 0;
    bzda.flags = 0;
    bzda.buttons.resize(11);

    bzda.x = 0;
    bzda.y = yw->_screenSize.y - yw->_iconOrderH;
    bzda.w = yw->_screenSize.x;
    bzda.h = yw->_iconOrderH;

    for (int i = 0; i < 11; i++)
        bzda.buttons[i] = ButtonBox();

    bzda.cmdCommands.reserve(1088);
    bzda.field_1D8 = 0;

    sub_4C3A54(yw);

    bzda.field_8EC = 0;
    bzda.field_8F0 = 0;
    bzda.field_8F4 = 0;
    bzda.field_8F8 = 0;

    if ( bzda.field_1D4 & 1 )
    {
        if ( !(bzda.field_1D8 & 1) )
        {
            bzda.field_1D8 |= 1;
            Input::Engine.AddClickBoxFront(&bzda);
        }
    }

    GuiList::tInit args;
    args.resizeable = false;
    args.numEntries = 8;
    args.shownEntries = 8;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 16;

    int genesisListHeight = System::IniConf::UiGenesisListHeight.Get<int32_t>();
    if ( genesisListHeight <= 0 )
        genesisListHeight = System::IniConf::UiGenesisListDefaultHeight;

    const int usableHeight = std::max(1, genesisListHeight - 2 * yw->_fontBorderH);
    const int configuredRows = std::max(1, usableHeight / std::max(1, yw->_fontH));
    args.maxShownEntries = std::min(configuredRows, 64);
    args.shownEntries = std::min(args.shownEntries, args.maxShownEntries);

    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = bzda.field_900;
    args.maxEntryWidth = std::max(1024, yw->_screenSize.x);
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.instantInput = true;
    args.keyboardInput = true;
    args.thinScrollbar = true;
    args.fillThinScrollbarGap = true;
    args.wheelScroll = true;
    // Genesis background opacity is data-driven. The OpenNeoUA runtime default is 220
    // when the nucleus.ini key is absent; explicit values still override it.
    int genesisListOpacity = System::IniConf::UiGenesisListOpacity.Get<int32_t>();
    genesisListOpacity = std::max(0, std::min(255, genesisListOpacity));
    args.backgroundOpacity = static_cast<uint8_t>(genesisListOpacity);

    if ( !gui_lstvw.Init(yw, args) )
        return 0;


    sub_4C32A8(yw, &yw->_roboMapStatus);
    if ( yw->_roboFinderStatus.Valid )
    {
        if ( yw->_roboFinderStatus.Rect.y < yw->_upScreenBorder
                || yw->_roboFinderStatus.Rect.x + yw->_roboFinderStatus.Rect.w > yw->_screenSize.x
                || yw->_roboFinderStatus.Rect.h + yw->_roboFinderStatus.Rect.y > yw->_screenSize.y - yw->_downScreenBorder )
        {
            yw->_roboFinderStatus.Valid = false;
        }
    }

    sub_4C32A8(yw, &yw->_vhclMapStatus);
    if ( yw->_vhclFinderStatus.Valid )
    {
        if ( yw->_vhclFinderStatus.Rect.y < yw->_upScreenBorder
                || yw->_vhclFinderStatus.Rect.w + yw->_vhclFinderStatus.Rect.x > yw->_screenSize.x
                || yw->_vhclFinderStatus.Rect.h + yw->_vhclFinderStatus.Rect.y > yw->_screenSize.y - yw->_downScreenBorder )
        {
            yw->_vhclFinderStatus.Valid = false;
        }
    }

    if ( !yw->_roboMapStatus.Data[5] || !yw->_roboMapStatus.Data[6] )
        yw->_roboMapStatus.Valid = false;

    if ( !yw->_vhclMapStatus.Data[5] || !yw->_vhclMapStatus.Data[6] )
        yw->_vhclMapStatus.Valid = false;

    if ( yw->_roboMapStatus.Valid )
    {
        if ( yw->_roboMapStatus.IsOpen )
        {
            yw->GuiWinOpen(&robo_map);
            yw->GuiWinToFront(&robo_map);
        }
        else
            yw->GuiWinClose(&robo_map);

        robo_map.x = yw->_roboMapStatus.Rect.x;
        robo_map.y = yw->_roboMapStatus.Rect.y;
        robo_map.w = yw->_roboMapStatus.Rect.w;
        robo_map.h = yw->_roboMapStatus.Rect.h;
        robo_map.field_1EC = yw->_roboMapStatus.Data[0];
        robo_map.field_1ED = yw->_roboMapStatus.Data[1];
        robo_map.field_1EE = yw->_roboMapStatus.Data[2];
        robo_map.field_208 = yw->_roboMapStatus.Data[3];
        robo_map.field_20A = yw->_roboMapStatus.Data[4];
        robo_map.field_20C = yw->_roboMapStatus.Data[5];
        robo_map.field_20E = yw->_roboMapStatus.Data[6];

        sub_4C1970(yw, 0);
    }


    if ( yw->_roboFinderStatus.Valid )
    {
        if ( yw->_roboFinderStatus.IsOpen && !yw->IsSpectatorControlled() )
        {
            yw->GuiWinOpen(&squadron_manager);
            yw->GuiWinToFront(&squadron_manager);
        }
        else
            yw->GuiWinClose(&squadron_manager);

        yw_ApplySquadronManagerSavedRect(yw, yw->_roboFinderStatus.Rect);
    }

    return 1;
}

void NC_STACK_ypaworld::FFeedback_Init()
{
    _ffTimeStamp = 0;
    _ffEffectType = -1;
    _ffPeriod = 0;
    _ffMagnitude = 0;
}

void create_info_log(NC_STACK_ypaworld *yw)
{
    info_log = yw_infolog();

    GuiList::tInit args;
    args.title = Locale::Text::WinName(Locale::WINNAME_LOG);
    args.resizeable = true;
    args.numEntries = 1;
    args.shownEntries = 6;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 24;
    args.minShownEntries = 3;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = 200;
    args.maxEntryWidth = 32000;
    args.minEntryWidth = 2 * (5 * yw->_guiTiles[0]->map[48].w);
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.closeChar = 74;

    if ( info_log.Init(yw, args) )
    {
        if ( yw->_timeStamp )
        {
            std::string tmp = fmt::sprintf("<%02d:%02d:%02d>", (yw->_timeStamp >> 10) / 60 / 60 % 24, (yw->_timeStamp >> 10) / 60 % 60, (yw->_timeStamp >> 10) % 60);
            std::string tmp2 = Locale::Text::Common(Locale::CMN_MISNCONT);
            sprintf(info_log.msgs[0].txt, tmp2.c_str(), tmp.c_str());
        }
        else
        {
            std::string tmp = Locale::Text::Common(Locale::CMN_WELCOME);
            strcpy(info_log.msgs[0].txt, tmp.c_str());
        }

        info_log.msgs[0].field_4 = yw->_timeStamp + 1;
        info_log.field_255C = 0;
        info_log.field_2560 = yw->_timeStamp;
        info_log.field_2564 = 0;
        info_log.field_250 = 127;
        info_log.field_248 = 6 * yw->_guiTiles[0]->map[48].w + 2 * yw->_guiTiles[0]->map[58].w + 12;
    }
}

void create_exit_menu(NC_STACK_ypaworld *yw)
{
    dword_5C8B7C = (yw->_fontH / 2) + yw->_fontH;
    dword_5C8B84 = yw->_fontH / 4;

    int tmp = yw->_guiTiles[0]->GetWidth("WWWWWWW");

    GuiList::tInit args;
    args.title = Locale::Text::WinName(Locale::WINNAME_EXIT);
    args.resizeable = false;
    args.numEntries = 5;
    args.shownEntries = 5;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 5;
    args.minShownEntries = 5;
    args.withIcon = false;
    args.entryHeight = dword_5C8B7C;
    args.entryWidth = (2 * tmp + 16);
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.staticItems = true;
    args.closeChar = 85;
    args.factionAccentTitle = false;

    if ( exit_menu.Init(yw, args) )
    {
        int v5 = yw->_fontH + exit_menu.upperVborder + dword_5C8B84;

        dword_5C8B80 = 2 * yw->_guiTiles[0]->map[32].w + yw->_fontBorderW;
        dword_5C8B88 = exit_menu.entryWidth - 2 * dword_5C8B80;

        for (int i = 8; i < 13; i++)
        {
            exit_menu.buttons[i] = ButtonBox(dword_5C8B80, (i - 8) * dword_5C8B7C + v5,
                                             dword_5C8B88, yw->_fontH);
        }
    }
}

void sb_0x451034__sub5(NC_STACK_ypaworld *yw)
{
    int v2 = yw->_guiTiles[0]->GetWidth("WWWWWWW") * 3.5 + 16;

    GuiList::tInit args;
    args.title = " ";
    args.resizeable = false;
    args.numEntries = 3;
    args.shownEntries = 3;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 3;
    args.minShownEntries = 3;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = v2;
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.staticItems = true;
    args.closeChar = 85;
    args.withHelp = false;

    if ( lstvw2.Init(yw, args) )
    {
        int v1 = yw->_fontH + lstvw2.upperVborder + 2 * lstvw2.entryHeight;

        dword_5BAFA8 = 2 * yw->_guiTiles[0]->map[32].w + yw->_fontBorderW;
        dword_5BAFA0 = (lstvw2.entryWidth - 2 * dword_5BAFA8) / 3;

        lstvw2.buttons.resize(10);

        lstvw2.buttons[8] = ButtonBox(dword_5BAFA8, v1, dword_5BAFA0, lstvw2.entryHeight);

        lstvw2.buttons[9] = ButtonBox(  lstvw2.entryWidth - dword_5BAFA8 - dword_5BAFA0,
                                        v1,
                                        dword_5BAFA0,
                                        lstvw2.entryHeight);

        dword_5BAFA4 = lstvw2.buttons[9].x - (dword_5BAFA0 + lstvw2.buttons[8].x);

        dword_5BAF9C = 0;
    }
}

void sb_0x451034__sub9(NC_STACK_ypaworld *yw)
{
    const std::array<std::string, 14> off_51825C
    {
        "wireless/bar.sklt",
        "wireless/compass.sklt",
        "wireless/frame.sklt",
        "wireless/arrow.sklt",
        "wireless/mgvis.sklt",
        "wireless/grenvis1.sklt",
        "wireless/rockvis1.sklt",
        "wireless/missvis1.sklt",
        "wireless/bombvis1.sklt",
        "wireless/grenvis2.sklt",
        "wireless/rockvis2.sklt",
        "wireless/missvis2.sklt",
        "wireless/bombvis2.sklt",
        "wireless/triangle.sklt"
    };

    sklt_wis *wis = &yw->_hud;

    *wis = sklt_wis();

    wis->field_0 = 1;

    Common::Env.SetPrefix("rsrc", "data:");

    for (int i = 0; i < 14; i++)
    {
        wis->sklts[i] = Nucleus::CInit<NC_STACK_sklt>({{NC_STACK_rsrc::RSRC_ATT_NAME, off_51825C.at(i)}});

        if ( wis->sklts[i] )
            wis->sklts_intern[i] = wis->sklts[i]->GetSkelet();
    }

    float CW, CH;
    GFX::Engine.getAspectCorrection(CW, CH, false);

    if ( yw->_screenSize.x >= 512 )
    {
        wis->field_9E = 8;
        wis->field_96 = 28;
        wis->field_92 = (float)yw->_guiTiles[0]->h / ((float)yw->_screenSize.y * CH);
        wis->field_8E = wis->field_92 * 14.0;
        wis->field_9A = 28;
    }
    else
    {
        wis->field_9E = 6;
        wis->field_96 = 18;
        wis->field_92 = (float)yw->_guiTiles[0]->h * 1.5 / ((float)yw->_screenSize.y * CH);
        wis->field_8E = wis->field_92 * 18.0;
        wis->field_9A = 18;
    }

    wis->field_72 = 0;
    wis->field_8A = (float)(yw->_guiTiles[51]->map[1].w * wis->field_9E + wis->field_9A + wis->field_96) / ((float)yw->_screenSize.x * CW);

    wis->field_72 = 1;
}

void sub_4DA874(NC_STACK_ypaworld *yw)
{
    exit_menu.Free();
}

void sub_46E16C(NC_STACK_ypaworld *yw)
{
    info_log.Free();
}

void sub_4C706C(NC_STACK_ypaworld *yw)
{
    squadron_manager.Free();
}

void sub_4C39A4(NC_STACK_ypaworld *yw)
{
    yw->GuiWinClose( &gui_lstvw );

    if ( bzda.field_1D8 & 1 )
    {
        bzda.field_1D8 &= 0xFFFFFFFE;
        Input::Engine.RemClickBox(&bzda);
    }

    gui_lstvw.Free();
}

void sub_4E1D24()
{
    Input::Engine.RemClickBox(&up_panel);
}

void sub_4E2B24(NC_STACK_ypaworld *yw)
{
    for (int i = 0; i < 14; i++)
    {
        if (yw->_hud.sklts[i])
        {
            yw->_hud.sklts[i]->Delete();
            yw->_hud.sklts[i] = nullptr;
            yw->_hud.sklts_intern[i] = nullptr;
        }
    }
}

void sub_4C8524(NC_STACK_ypaworld *yw)
{
    lstvw2.Free();
}

void NC_STACK_ypaworld::FFeedback_StopAll()
{
    Input::Engine.ForceFeedback(Input::FF_STATE_STOP, Input::FF_TYPE_ALL);
}

int sb_0x451034(NC_STACK_ypaworld *yw)
{
    if ( yw->_guiLoaded )
        return yw->_guiLoaded;

    yw->_guiDragging = false;
    yw->_guiDragElement = nullptr;
    yw->_guiDragDefaultMouse = false;
    yw->_mouseCursorHidden = false;
    yw->_prevMousePos = Common::Point();
    yw->_bactPrevClicked = nullptr;
    yw_ResetWorldSelectionDrag(yw);
    yw->_moveOrderFeedbackActive = false;
    yw->_attackOrderFeedbacks.clear();

    sb_0x451034__sub9(yw);
    sb_0x451034__sub8(yw);
    sb_0x451034__sub2(yw);
    create_squad_man(yw);
    create_info_log(yw);
    create_exit_menu(yw);
    sb_0x451034__sub5(yw);
    sb_0x451034__sub6(yw);

    yw->ypaworld_func139(&robo_map);
    yw->ypaworld_func139(&squadron_manager);
    yw->ypaworld_func139(&info_log);
    yw->ypaworld_func139(&exit_menu);
    yw->ypaworld_func139(&lstvw2);
    yw->ypaworld_func139(&netgame_wnd);

    if ( !sb_0x451034__sub3(yw) )
    {
        yw->ypaworld_func140(&lstvw2);
        yw->ypaworld_func140(&exit_menu);
        yw->ypaworld_func140(&info_log);
        yw->ypaworld_func140(&squadron_manager);
        yw->ypaworld_func140(&robo_map);
        yw->ypaworld_func140(&netgame_wnd);

        sub_4DA874(yw);
        sub_46E16C(yw);
        sub_4C706C(yw);
        return 0;
    }

    yw->_guiLoaded = true;

    if ( yw->_preferences & World::PREF_CDMUSICDISABLE )
    {
        SFXEngine::SFXe.SetMusicTrack(yw->_levelInfo.MusicTrack, yw->_levelInfo.MusicTrackMinDelay, yw->_levelInfo.MusicTrackMaxDelay);
        SFXEngine::SFXe.PlayMusicTrack();
    }

    return 1;
}

void sb_0x4d7c08__sub0__sub1(const SDL_Color *uiAccent)
{
    if ( bzda.IsOpen() )
    {
        GFX::Engine.ProcessDrawSeq(bzda.cmdCommands, &bzda.cmdInclude, uiAccent);

        if ( gui_lstvw.IsOpen() )
            GFX::Engine.ProcessDrawSeq(gui_lstvw.cmdCommands, &gui_lstvw.cmdInclude, uiAccent);
    }
}

void sb_0x4d7c08__sub0(NC_STACK_ypaworld *yw)
{
    SDL_Color uiAccentColor;
    const SDL_Color *uiAccent = yw_GetFactionUiAccent(yw, &uiAccentColor);

    if ( yw->_hideHudForScreenshots )
        return;

    // Keep the GEM upgrade popup visually isolated. Repeated calls are safe and
    // immediately close any gameplay window reopened while the notification is active.
    if ( yw->HasActiveNewGemNotification() )
        yw_CloseGameplayWindowsForGemNotification(yw);

    if ( yw->_userUnit->_bact_type != BACT_TYPES_MISSLE )
    {
        const bool priorityWindowOpen = yw_IsPriorityGameplayUiOcclusionOpen();

        // Render the complete ordinary gameplay UI first. When Map/Squadron or
        // the Genesis list is open, only the pixels under the actual window
        // rectangle are removed later; nothing outside the overlap is globally
        // suppressed.
        yw_RenderWorldSelectionDrag(yw);
        yw_RenderMoveOrderFeedback(yw);
        yw_RenderAttackOrderFeedback(yw);
        yw_RenderRoboRelocationMarker(yw);
        yw_RenderCustomWorldMapMarkers(yw);
        sb_0x4d7c08__sub0__sub4(yw);

        if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            sb_0x4d7c08__sub0__sub0(yw);
            sb_0x4d7c08__sub0__sub2(yw);
        }

        // Ordinary GUI windows are foreground UI. Render them after world/HUD
        // overlays so status bars and other gameplay indicators cannot bleed
        // through dialogs or panels. Tooltips still render above these windows.
        if ( !priorityWindowOpen )
        {
            for ( GuiBase *window : yw->_guiActive )
            {
                if ( !yw_IsTopLevelExitMenuFlowWindow(window) )
                    yw_RenderGameplayGuiWindow(yw, window, uiAccent);
            }
        }

        draw_tooltip(yw, uiAccent);

        if ( !priorityWindowOpen )
        {
            // Fixed gameplay bars and creation UI remain above ordinary windows.
            GFX::Engine.ProcessDrawSeq(up_panel.cmdCommands, &up_panel.cmdInclude, uiAccent);
            yw_RenderPlasmaCurrencyHudIcon(yw);
            sb_0x4d7c08__sub0__sub1(uiAccent);

            // Unlike ordinary windows, the complete Exit Menu flow is
            // finalized after every gameplay overlay so nothing from the
            // world UI can cover either the menu or its confirmations.
            yw_RenderTopLevelExitMenuFlow(yw, uiAccent);
        }
    }
}

void buy_list_update_sub(NC_STACK_ypaworld *yw, int a2, GuiList *lstvw, CmdStream *cur,
                         char a5, const std::string &a6, int a7, int remaining)
{
    const int scrollbarGap = yw_GetGuiListScrollbarGapFillWidth(lstvw, yw);
    int v33 = lstvw->entryWidth - 2 * yw->_fontBorderW + scrollbarGap;

    FontUA::ColumnItem v24[3];

    int v14;
    int v15;
    int v16;
    int v17;

    if ( a2 )
    {
        v14 = 9;
        v15 = 98;
        v16 = 99;
        v17 = 100;
    }
    else
    {
        v14 = 0;
        v16 = 102;
        v15 = 102;
        v17 = 102;
    }

    // A capped Genesis entry at zero remains selectable, but its text uses a
    // fixed neutral disabled gray for every faction. Keep it bright enough to
    // remain readable over both the normal and selected row backgrounds.
    if ( remaining == 0 )
        FontUA::set_txtColor(cur, 160, 160, 160);
    else if ( a2 )
        FontUA::set_txtColor(cur, yw->_iniColors[62].r, yw->_iniColors[62].g, yw->_iniColors[62].b);
    else
        FontUA::set_txtColor(cur, yw->_iniColors[61].r, yw->_iniColors[61].g, yw->_iniColors[61].b);

    const std::string priceText = fmt::sprintf("%d", a7);

    // Keep the price block state-independent. Vanilla used five digit cells from
    // the active row tileset, so simply moving the selection changed the width
    // left for the name. Reserve only what the current price really needs, using
    // the larger requirement of the normal and selected row assets.
    const int renderedPriceWidth = yw_GetGenesisScreenTextWidth(yw, priceText);
    const int normalPricePadding = std::max(2, yw_GetGenesisScreenTextWidth(yw, "0") / 4);
    const int selectedPricePadding = normalPricePadding;
    const int normalPriceWidth = renderedPriceWidth
                               + yw->_guiTiles[0]->map[102].w
                               + normalPricePadding;
    const int selectedPriceWidth = renderedPriceWidth
                                 + yw->_guiTiles[9]->map[100].w
                                 + selectedPricePadding;
    const int effectivePriceColumnWidth = std::max(normalPriceWidth, selectedPriceWidth);

    v24[0].txt = a5;
    v24[0].fontID = 28;
    v24[0].width = squadron_manager.field_2CC;
    v24[0].prefixChar = 0;
    v24[0].postfixChar = 0;
    v24[0].spaceChar = 64;
    v24[0].flags = 4;

    v24[1].fontID = v14;
    v24[1].spaceChar = v16;
    v24[1].prefixChar = v15;
    v24[1].flags = 37;
    v24[1].width = v33 - squadron_manager.field_2CC - effectivePriceColumnWidth;
    v24[1].postfixChar = 0;

    const int normalNameTextWidth = v24[1].width - yw->_guiTiles[0]->map[102].w;
    const int selectedNameTextWidth = v24[1].width - yw->_guiTiles[9]->map[98].w;
    v24[1].txt = yw_FormatGenesisVehicleName(yw, normalNameTextWidth,
                                             selectedNameTextWidth, a6, remaining);

    v24[2].txt = priceText;
    v24[2].fontID = v14;
    v24[2].spaceChar = v16;
    v24[2].postfixChar = v17;
    v24[2].width = effectivePriceColumnWidth;
    v24[2].flags = 42;
    v24[2].prefixChar = 0;

    const uint8_t backgroundOpacity = lstvw->backgroundOpacity;
    yw_SetGuiListBackgroundOpacity(cur, backgroundOpacity);

    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 123);

    FontUA::select_tileset(cur, v14);
    FontUA::op10(cur, squadron_manager.field_2CC);
    FontUA::store_u8(cur, v16);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);

    FontUA::add_xpos(cur, -squadron_manager.field_2CC);

    // The Genesis opacity controls the row background only. Vehicle/building
    // icons, names and prices remain at their original opacity.
    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);
    FormateColumnItem(yw, cur, 1, v24);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, backgroundOpacity);

    FormateColumnItem(yw, cur, 2, &v24[1]);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);

    yw_SetGuiListBackgroundOpacity(cur, backgroundOpacity);
    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 125);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);

    FontUA::next_line(cur);
}

float NC_STACK_ypaworld::sub_4498F4()
{
    float v3 = 1.0;

    if ( _isNetGame )
    {
        if ( _levelUnitLimitType == 2 )
        {
            int v1 = _countUnitsPerOwner[_userRobo->_owner] - _levelUnitLimit;
            if ( v1 > 0 )
                v3 = (_levelUnitLimitArg * v1) * 0.01 + 1.0;
        }
    }

    return v3;
}

void gui_update_create_btn__sub0(NC_STACK_ypaworld *yw)
{
    gui_lstvw.itemBlock.clear();
    Input::Engine.RemClickBox(&gui_lstvw);
    Input::Engine.AddClickBoxFront(&gui_lstvw);

    gui_lstvw.numEntries = bzda.field_8E8;
    gui_lstvw.selectedEntry = bzda.field_8F8;

    if ( gui_lstvw.maxShownEntries >= bzda.field_8E8 )
        gui_lstvw.shownEntries = bzda.field_8E8;
    else
        gui_lstvw.shownEntries = gui_lstvw.maxShownEntries;

    if ( gui_lstvw.firstShownEntries + gui_lstvw.shownEntries > gui_lstvw.numEntries )
        gui_lstvw.firstShownEntries = gui_lstvw.numEntries - gui_lstvw.shownEntries;

    gui_lstvw.SetRect(yw, -2, -2);

    const int desiredGenesisX = bzda.field_904 + bzda.x;
    gui_lstvw.x = std::max(0, std::min(desiredGenesisX,
                                      std::max(0, yw->_screenSize.x - gui_lstvw.w)));
    gui_lstvw.y = bzda.field_918 + bzda.y - gui_lstvw.h;

    gui_lstvw.ItemsPreLayout(yw, &gui_lstvw.itemBlock, 0, "uvw");

    for (int i = 0; i < gui_lstvw.shownEntries; i++ )
    {
        int v21 = 0;
        int v3 = i + gui_lstvw.firstShownEntries;

        if ( bzda.field_4DC[ v3 ].i == 1 )
        {
            int v5 = bzda.field_4DC[ v3 ].d;

            const World::TVhclProto &proto = yw->_vhclProtos[v5];

            int v17 = dround(yw->sub_4498F4() * 2 * proto.GetProductionCost() / 100.0);

            const std::string v8 = yw->ResolveGameplayVehicleName(proto);
            const int remaining = yw->_userRobo
                ? yw->GetVehicleProductionLimitRemaining(yw->_userRobo->_owner, v5)
                : -1;

            if ( v3 == gui_lstvw.selectedEntry )
                v21 = 1;

            buy_list_update_sub(yw, v21, &gui_lstvw, &gui_lstvw.itemBlock,
                                proto.type_icon, v8, v17, remaining);
        }
        else if ( bzda.field_4DC[ v3 ].i == 2 )
        {
            int v9 = bzda.field_4DC[ v3 ].d;
            const World::TBuildingProto &v10 = yw->_buildProtos[v9];

            int v18 = dround(yw->sub_4498F4() * v10.GetProductionCost() / 100.0);

            std::string v13 = yw->ResolveGameplayBuildingName(v10, yw->_isNetGame);

            if ( v3 == gui_lstvw.selectedEntry )
                v21 = 1;

            buy_list_update_sub(yw, v21, &gui_lstvw, &gui_lstvw.itemBlock,
                                v10.TypeIcon, v13, v18, -1);
        }
    }

    gui_lstvw.ItemsPostLayout(yw, &gui_lstvw.itemBlock, 0, "xyz");

    FontUA::set_end(&gui_lstvw.itemBlock);
}

void gui_update_create_btn(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( yw->_userUnit->_status == BACT_STATUS_DEAD || yw->IsSpectatorControlled() )
    {
        bzda.buttons[0] = ButtonBox(); //create_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 67);

        yw->GuiWinClose( &gui_lstvw );
    }
    else
    {
        bzda.buttons[0] = ButtonBox(bzda.field_904, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH);

        if ( !(bzda.field_1CC & 0x16) )
        {
            bzda.buttons[0] = ButtonBox(); //create_btn

            FontUA::select_tileset(cur, 23);
            FontUA::store_u8(cur, 67);
        }
        else
        {
            const bool createPressed = (bzda.field_1D0 & 0x16) != 0
                                    || (bzda.field_91C & BZDA_GEM_PRESS_CREATE) != 0;
            if ( createPressed )
            {
                if ( gui_lstvw.IsOpen() )
                    gui_update_create_btn__sub0(yw);

                FontUA::select_tileset(cur, 22);
            }
            else
            {
                FontUA::select_tileset(cur, 21);
            }

            int v9 = bzda.field_1D0 & 0x16;

            if (v9 == 0x10)
                FontUA::store_u8(cur, 70);
            else if (v9 == 4)
                FontUA::store_u8(cur, 68);
            else if (v9 == 2)
                FontUA::store_u8(cur, 67);
            else
                FontUA::store_u8(cur, 67);
        }
    }
}

void gui_update_map_squad_btn(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( yw->IsSpectatorControlled() )
    {
        int v8 = bzda.field_908;

        bzda.buttons[2] = ButtonBox(v8, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH);
        v8 += yw->_iconOrderW;
        bzda.buttons[3] = ButtonBox();

        if ( robo_map.IsClosed() )
            FontUA::select_tileset(cur, 21);
        else
            FontUA::select_tileset(cur, 22);

        FontUA::store_u8(cur, 72);

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 73);

        yw->GuiWinClose( &squadron_manager );
        return;
    }

    if ( yw->_userUnit->_status == BACT_STATUS_DEAD )
    {
        FontUA::select_tileset(cur, 23);

        FontUA::store_u8(cur, 72);
        FontUA::store_u8(cur, 73);

        bzda.buttons[2] = ButtonBox(); //map_btn
        bzda.buttons[3] = ButtonBox(); //squad_btn

        yw->GuiWinClose( &robo_map );
        yw->GuiWinClose( &squadron_manager );
        yw->GuiWinClose( &info_log );
    }
    else
    {
        int v8 = bzda.field_908;

        for (int i = 2; i < 4; i++)
        {
            bzda.buttons[i] = ButtonBox(v8, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH);

            v8 += yw->_iconOrderW;
        }

        const bool mapPressed = robo_map.IsOpen();
        FontUA::select_tileset(cur, mapPressed ? 22 : 21);
        FontUA::store_u8(cur, 72);

        const bool squadPressed = squadron_manager.IsOpen();
        FontUA::select_tileset(cur, squadPressed ? 22 : 21);
        FontUA::store_u8(cur, 73);
    }
}

void gui_update_player_panel(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int v35 = 0;
    bool spectatorControlled = yw->IsSpectatorControlled();

    if ( yw->_userRobo == yw->_userUnit )
    {
        if ( !yw->_cmdrsRemap.empty() )
            v35 = 0x80;

        if ( bzda.field_8DC )
            v35 |= 0x40;
    }
    else if ( yw->_userUnit->_status == BACT_STATUS_DEAD )
    {
        v35 = 0x90;
    }
    else if ( yw->_playerInHSGun )
    {
        v35 = 0x90;

        if ( bzda.field_8DC > 1 )
            v35 = 0xD0;
    }
    else if ( yw->_userUnit->IsParentMyRobo() )
    {
        v35 = 0x10;

        if ( yw->_userUnit->_bact_type == BACT_TYPES_GUN )
        {
            if ( !yw->_cmdrsRemap.empty() )
                v35 = 0x90;
        }
        else if ( yw->_cmdrsRemap.size() > 1 )
            v35 = 0x90;

        // Show the "next unit" button only if at least one child can be selected.
        // Unit Guns with unit_gun_no_switch = 1 do not count, so the button is
        // hidden (and its hotkey disabled) when they are the only children.
        for ( NC_STACK_ypabact *kid : yw->_userUnit->_kidList )
        {
            if ( !kid->_unitGunNoSwitch )
            {
                v35 |= 0x40;
                break;
            }
        }
    }
    else
    {
        v35 = 0x70;

        if ( yw->_userUnit->_bact_type == BACT_TYPES_GUN )
        {
            if ( !yw->_cmdrsRemap.empty() )
                v35 = 0xF0;
        }
        else if ( yw->_cmdrsRemap.size() > 1 )
            v35 = 0xF0;
    }

    if ( yw->_userRobo->_status == BACT_STATUS_DEAD )
        v35 = 0;

    if ( spectatorControlled )
    {
        // Keep normal command buttons disabled in Spectator Mode. While the
        // follow camera is active, expose only the vanilla back/up button and
        // reuse it as Return to Spectator.
        v35 = yw->IsSpectatorFollowActive() ? 0x10 : 0;
        yw->GuiWinClose( &gui_lstvw );
    }

    // Reuse the vanilla Jump into Vehicle button and state machine. Spectator
    // mode exposes the same button because its command mask already includes
    // the control-mode bit; only the final action differs and starts external
    // Spectator Follow instead of taking direct control of the unit.
    if ( bzda.field_1CC & 8 )
    {
        bzda.buttons[1] = ButtonBox(bzda.field_90C, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //into_vhcl_btn

        if ( bzda.field_1D0 & 8 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 69);
    }
    else
    {
        bzda.buttons[1] = ButtonBox();

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 69);
    }


    if ( v35 & 0x10 )
    {
        bzda.buttons[4] = ButtonBox(yw->_iconOrderW + bzda.field_90C, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //to_host_btn

        if ( bzda.field_91C & 0x10 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 81);
    }
    else
    {
        bzda.buttons[4] = ButtonBox(); //to_host_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 81);
    }

    if ( v35 & 0x20 )
    {
        bzda.buttons[5] = ButtonBox(bzda.field_90C + 2 * yw->_iconOrderW, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //to_leader_btn

        if ( bzda.field_91C & 0x20 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 82);
    }
    else
    {
        bzda.buttons[5] = ButtonBox(); // to_leader_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 82);
    }

    if ( v35 & 0x40 )
    {
        if ( !bzda.field_8DC || (yw->_userRobo != yw->_userUnit && !yw->_playerInHSGun) )
        {
            bzda.buttons[6] = ButtonBox(bzda.field_90C + 3 * yw->_iconOrderW, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //turrets_btn

            if ( bzda.field_91C & 0x40 )
                FontUA::select_tileset(cur, 22);
            else
                FontUA::select_tileset(cur, 21);

            FontUA::store_u8(cur, 83);
        }
        else
        {
            bzda.buttons[6] = ButtonBox(bzda.field_90C + 3 * yw->_iconOrderW, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //turrets_btn

            if ( bzda.field_91C & 0x40 )
                FontUA::select_tileset(cur, 22);
            else
                FontUA::select_tileset(cur, 21);

            FontUA::store_u8(cur, 66);
        }
    }
    else
    {
        bzda.buttons[6] = ButtonBox(); // turrets_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 83);
    }

    if ( v35 & 0x80 )
    {
        bzda.buttons[7] = ButtonBox(4 * yw->_iconOrderW + bzda.field_90C, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //next_squad_btn

        if ( bzda.field_91C & 0x80 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 84);
    }
    else
    {
        bzda.buttons[7] = ButtonBox(); //next_squad_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 84);
    }
}

void gui_update_tools(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( yw->IsSpectatorControlled() )
    {
        // Observer mode keeps only the exit/menu button. Situation Analyzer and
        // Help are tied to owner-1 player context and should not be available.
        bzda.buttons[8] = ButtonBox(); // analyzer_btn
        bzda.buttons[9] = ButtonBox(); // help_btn
        bzda.buttons[10] = ButtonBox(bzda.field_910, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); // menu_btn

        if ( bzda.field_91C & 0x400 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 85);
        return;
    }

    if ( yw->_userUnit->_status == BACT_STATUS_DEAD )
    {
        bzda.buttons[8] = ButtonBox(); // analyzer_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 63);
    }
    else
    {
        bzda.buttons[8] = ButtonBox(bzda.field_910, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); // analyzer_btn

        if ( bzda.field_91C & 0x100 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 63);
    }

    // OpenNeoUA: remove the deprecated online-help button from the gameplay toolbar.
    bzda.buttons[9] = ButtonBox(); // help_btn removed

    bzda.buttons[10] = ButtonBox(yw->_iconOrderW + bzda.field_910,  bzda.field_918,  yw->_iconOrderW,  yw->_iconOrderH); // menu_btn

    if ( bzda.field_91C & 0x400 )
        FontUA::select_tileset(cur, 22);
    else
        FontUA::select_tileset(cur, 21);

    FontUA::store_u8(cur, 85);
}

void sub_449970(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3, const std::string &a5, int a6, int a7)
{
    int v10, v22;
    const int markerGap = yw->_screenSize.x >= 512 ? 8 : 6;

    if ( yw->_screenSize.x >= 512 )
    {
        v22 = a7 - 72;
        v10 = a4 + a7 - 72 + markerGap;
    }
    else
    {
        v22 = a7 - 56;
        v10 = a4 + a7 - 56 + markerGap;
    }

    FontUA::select_tileset(cur, 15);

    FontUA::set_xpos(cur, a4);
    FontUA::set_ypos(cur, a3);

    int v14 = yw->_fontH - yw->_guiTiles[51]->h;

    FontUA::ColumnItem arg;
    arg.txt = a5;
    arg.width = v22;
    arg.fontID = 15;
    arg.prefixChar = 0;
    arg.spaceChar = 32;
    arg.postfixChar = 0;
    arg.flags = 40;

    FormateTextAlignedClippedString(yw, cur, &arg);

    FontUA::select_tileset(cur, 51);

    FontUA::set_xpos(cur, v10);
    FontUA::set_ypos(cur, (v14 / 2) + a3);

    for (int i = 0; i < a6; i++)
        FontUA::store_u8(cur, yw->_userRobo->_owner);
}

void ypaworld_func64__sub7__sub2__sub1__sub0(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( bzda.field_1D0 & 6 )
    {
        if ( bzda.field_8EC != -1 && !yw->_playerInHSGun )
        {
            World::TVhclProto *v4 = &yw->_vhclProtos[ bzda.field_2DC[ bzda.field_8EC ] ];

            auto resolvedJob = [v4](World::VehicleCombatClass targetClass, int legacyFallback)
            {
                int value = 0;
                if ( World::TryGetSpecificFightJob(*v4, targetClass, &value) )
                    return value;
                return legacyFallback;
            };

            auto jobMarkers = [](int value)
            {
                int markers = value / 2;
                // Match the original UI exactly: only a literal zero becomes
                // one visible marker; negative authored values remain non-drawing.
                if ( !markers )
                    markers = 1;
                return markers;
            };

            const int vsRobo = jobMarkers(v4->job_fightrobo);
            const int vsTank = jobMarkers(v4->job_fighttank);
            const int vsPlane = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_PLANE, v4->job_fightflyer));
            const int vsHeli = jobMarkers(v4->job_fighthelicopter);
            const int vsCruiser = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_CRUISER, v4->job_fightflyer));
            const int vsGlider = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_GLIDER, v4->job_fightflyer));
            const int vsZeppelin = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_ZEPPELIN, v4->job_fightflyer));
            const int vsUfo = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_UFO, v4->job_fightflyer));
            const int vsCar = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_CAR, v4->job_fighttank));
            // Legacy Host Station deployment treated unrecognized Gun targets as
            // the helicopter combat job; preserve that fallback until job_fightgun
            // is explicitly authored.
            const int vsGun = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_GUN, v4->job_fighthelicopter));
            const int capture = jobMarkers(v4->job_conquer);
            const int recon = jobMarkers(v4->job_reconnoitre);
            const bool hasFineJobs =
                v4->job_fightplane_defined || v4->job_fightcruiser_defined ||
                v4->job_fightglider_defined ||
                v4->job_fightzeppelin_defined || v4->job_fightufo_defined ||
                v4->job_fightcar_defined || v4->job_fightgun_defined;

            int v29_4 = 4 * yw->_screenSize.x / 7;
            int v6 = yw->_screenSize.x - v29_4;

            // Legacy-only Vehicle data keeps the original six-row layout
            // exactly. As soon as any fine job is authored, expose the extended
            // twelve-row class view (six additional specific combat classes).
            int v30 = -(yw->_downScreenBorder + (hasFineJobs ? 13 : 7) * yw->_fontH);

            const SDL_Color factionTextColor = yw_GetFactionUiTextColor(yw);
            FontUA::set_txtColor(cur, factionTextColor.r,
                                 factionTextColor.g, factionTextColor.b);

            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_VSROBO), vsRobo, v6);
            v30 += yw->_fontH;
            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_VSTANK), vsTank, v6);
            v30 += yw->_fontH;
            sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_PLANE), vsPlane, v6);
            v30 += yw->_fontH;
            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_VSHELI), vsHeli, v6);
            v30 += yw->_fontH;

            if ( hasFineJobs )
            {
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_CRUISER), vsCruiser, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_GLIDER), vsGlider, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_ZEPPELIN), vsZeppelin, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_UFO), vsUfo, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_CAR), vsCar, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_GUN), vsGun, v6);
                v30 += yw->_fontH;
            }

            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_CAPTURE), capture, v6);
            v30 += yw->_fontH;
            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_RECON), recon, v6);
        }
    }
}

void ypaworld_func64__sub7__sub2__sub1(NC_STACK_ypaworld *yw)
{
    sub_4C3A54(yw);

    bzda.cmdCommands.clear();

    if ( bzda.field_1D4 & 1 )
    {
        int v20 = bzda.x - (yw->_screenSize.x / 2);
        int v21 = bzda.y - (yw->_screenSize.y / 2);

        if ( !(bzda.field_1D8 & 1) )
        {
            bzda.field_1D8 |= 1;
            Input::Engine.AddClickBoxFront(&bzda);
        }

        Input::Engine.RemClickBox(&bzda);
        Input::Engine.AddClickBoxFront(&bzda);

        FontUA::select_tileset(&bzda.cmdCommands, 21);
        FontUA::set_center_xpos(&bzda.cmdCommands, v20);
        FontUA::set_center_ypos(&bzda.cmdCommands, v21);


        if ( bzda.field_904 > 0 )
            FontUA::add_xpos(&bzda.cmdCommands, bzda.field_904);

        gui_update_create_btn(yw, &bzda.cmdCommands);

        // The create glyph advances the command cursor by one icon. From here
        // onward use the same absolute anchors as the click boxes instead of
        // accumulating legacy gaps from the Genesis origin. This keeps visual
        // buttons and input hitboxes perfectly aligned for every Genesis X.
        const int afterCreateX = bzda.field_904 + yw->_iconOrderW;
        const int toMapSquad = bzda.field_908 - afterCreateX;
        if ( toMapSquad != 0 )
            FontUA::add_xpos(&bzda.cmdCommands, toMapSquad);

        gui_update_map_squad_btn(yw, &bzda.cmdCommands);

        const int afterMapSquadX = bzda.field_908 + 2 * yw->_iconOrderW;
        const int toPlayerPanel = bzda.field_90C - afterMapSquadX;
        if ( toPlayerPanel != 0 )
            FontUA::add_xpos(&bzda.cmdCommands, toPlayerPanel);

        gui_update_player_panel(yw, &bzda.cmdCommands);

        const int afterPlayerPanelX = bzda.field_90C + 5 * yw->_iconOrderW;
        const int toTools = bzda.field_910 - afterPlayerPanelX;
        if ( toTools != 0 )
            FontUA::add_xpos(&bzda.cmdCommands, toTools);

        gui_update_tools(yw, &bzda.cmdCommands);

        if ( gui_lstvw.IsOpen() )
            ypaworld_func64__sub7__sub2__sub1__sub0(yw, &bzda.cmdCommands);
    }

    FontUA::set_opacity(&bzda.cmdCommands, 255);
    FontUA::set_end(&bzda.cmdCommands);

    if ( gui_lstvw.IsOpen() )
        gui_lstvw.Formate(yw);
}

NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub6(NC_STACK_ypaworld *yw)
{
    if ( !yw->_prevUnitId )
        return nullptr;

    if ( yw->_prevUnitId == yw->_userRobo->_gid )
        return yw->_userRobo;

    for( NC_STACK_ypabact* &comm : yw->_userRobo->_kidList )
    {
        if ( comm->_status != BACT_STATUS_DEAD && comm->_status != BACT_STATUS_CREATE && comm->_status != BACT_STATUS_BEAM )
        {
            if ( yw->_prevUnitId == comm->_gid )
                return comm;

            for( NC_STACK_ypabact* &unit : comm->_kidList )
            {
                if ( unit->_status != BACT_STATUS_DEAD && unit->_status != BACT_STATUS_CREATE && unit->_status != BACT_STATUS_BEAM && yw->_prevUnitId == unit->_gid )
                    return unit;
            }
        }
    }

    return nullptr;
}

void ypaworld_func64__sub7__sub2__sub7(NC_STACK_ypaworld *yw)
{
    if ( !yw->_userUnit->_parent || yw->_userUnit->_parent != yw->_userRobo )
    {
        if ( yw->_userUnit->_bact_type != BACT_TYPES_ROBO && yw->_userUnit->_bact_type != BACT_TYPES_MISSLE && yw->_userUnit->_bact_type != BACT_TYPES_GUN )
        {
            bact_arg109 v5;
            v5.field_0 = 2;
            v5.field_4 = yw->_userUnit->_parent;

            yw->_userUnit->ReorganizeGroup(&v5);
        }
    }
}

int ypaworld_func64__sub7__sub2__sub3(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    if ( yw->IsSpectatorControlled() )
    {
        const int mapHotKey = yw->_GameShell
            ? yw->_GameShell->InputConfig[World::INPUT_BIND_MAP].KeyID
            : 8;
        const int quitHotKey = yw->_GameShell
            ? yw->_GameShell->InputConfig[World::INPUT_BIND_QUIT].KeyID
            : 24;

        if ( inpt->HotKeyID == mapHotKey && yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 2;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        else if ( inpt->HotKeyID == quitHotKey )
        {
            // Reuse the ordinary in-game Exit Menu button path. This supports
            // both Escape and any remapped Quit binding without a spectator-only
            // menu implementation.
            winpt->selected_btnID = 10;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
            inpt->HotKeyID = -1;
        }

        // Spectator gameplay commands remain disabled here. Class-level input
        // owned by the controlled UFO is left intact for its normal User_layer.
        return 0;
    }

    // During the GEM popup, Creation remains blocked. Map and Squadron Manager
    // intentionally keep their normal hotkey path because they now have higher
    // visual priority than every gameplay overlay except the Exit Menu.
    if ( yw->HasActiveNewGemNotification() )
    {
        switch ( inpt->HotKeyID )
        {
        case 2:
        case 3:
            bzda.field_91C |= BZDA_GEM_PRESS_CREATE;
            yw->GuiWinClose(&gui_lstvw);
            return 0;

        default:
            break;
        }
    }

    switch ( inpt->HotKeyID )
    {
    case 0:
        if ( bzda.field_1CC & 1 )
            bzda.field_1D0 = 1;
        break;

    case 1:
        if ( yw->_userUnit == yw->_userRobo )
        {
            if ( bzda.buttons[6] ) //turrets_btn
            {
                winpt->selected_btnID = 6;
                winpt->selected_btn = &bzda;
                winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
            }
        }
        break;

    case 2:
        if ( bzda.field_1CC & 2 )
        {
            if ( bzda.field_1D0 & 0x16 )
            {
                bzda.field_1D0 = bzda.field_1CC & 1;
            }
            else
            {
                if ( !(bzda.field_1D0 & 2) || gui_lstvw.IsClosed() )
                {
                    yw->GuiWinOpen( &gui_lstvw );
                    gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                }
                else
                {
                    yw->GuiWinClose( &gui_lstvw );
                }

                if ( bzda.field_8E0 <= bzda.field_8F8 )
                    bzda.field_1D0 = 16;
                else
                    bzda.field_1D0 = 2;

                return 0;
            }
        }
        break;

    case 3:
        if ( bzda.field_1CC & 4 )
        {
            if ( bzda.field_1D0 & 0x16 )
            {
                bzda.field_1D0 = bzda.field_1CC & 1;
            }
            else
            {
                if ( !(bzda.field_1D0 & 4) || gui_lstvw.IsClosed() )
                {
                    yw->GuiWinOpen( &gui_lstvw );
                    gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                }
                else
                {
                    yw->GuiWinClose( &gui_lstvw );
                }

                bzda.field_1D0 = 4;
                return 0;
            }
        }
        break;

    case 4:
        if ( bzda.field_1CC & 8 )
        {
            winpt->selected_btn = &bzda;
            winpt->selected_btnID = 1;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 7:
        if ( bzda.field_1CC & 0x20 )
        {
            if ( bzda.field_1D0 & 0x20 )
                bzda.field_1D0 = bzda.field_1CC & 1;
            else
                bzda.field_1D0 = 32;
        }
        break;

    case 8:
        if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 2;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 9:
        if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 3;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 20:
        if ( bzda.buttons[7] ) //next_squad_btn
        {
            winpt->selected_btnID = 7;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 21:
        if ( bzda.buttons[4] ) //to_host_btn
        {
            winpt->selected_btnID = 4;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 22:
        if ( bzda.buttons[6] ) //turrets_btn
        {
            winpt->selected_btnID = 6;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 23:
        if ( bzda.buttons[5] ) //to_leader_btn
        {
            winpt->selected_btnID = 5;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 24:
        if ( yw->_userRobo->_status == BACT_STATUS_DEAD || bzda.field_1D0 & 1 )
        {
            if ( yw->_userRobo == yw->_userUnit || yw->_userRobo->_status == BACT_STATUS_DEAD )
            {
                if ( bzda.buttons[10] ) //menu_btn
                {
                    winpt->selected_btnID = 10;
                    winpt->selected_btn = &bzda;
                    winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
                }
            }
            else
            {
                winpt->selected_btnID = 4;
                winpt->selected_btn = &bzda;
                winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
                inpt->HotKeyID = -1;
            }
        }
        else
        {
            bzda.field_1D0 = 1;
        }
        break;

    case 25:
        if ( yw->_userUnit != yw->_userRobo )
        {
            if ( yw->_hud.field_0 )
                yw->_hud.field_0 = 0;
            else
                yw->_hud.field_0 = 1;
        }
        break;

    case 27:
        if ( info_log.IsClosed() )
        {
            yw->GuiWinOpen(&info_log);
            yw->GuiWinToFront(&info_log);
        }
        else
            yw->GuiWinClose(&info_log);
        break;

    case 28:
        if ( bzda.field_1D0 == 2 || bzda.field_1D0 == 4 || bzda.field_1D0 == 16 )
        {
            bzda.field_8F8++;
            if ( bzda.field_8F8 >= bzda.field_8E8 )
                bzda.field_8F8 = 0;

            if ( bzda.field_8F8 >= bzda.field_8E0 )
            {
                bzda.field_8F4 = bzda.field_8F8 - bzda.field_8E0;
                bzda.field_1D0 = 16;
                bzda.field_8EC = -1;
            }
            else
            {
                bzda.field_8EC = bzda.field_8F8;
                bzda.field_8F4 = -1;

                if ( !(bzda.field_1D0 & 4) )
                    bzda.field_1D0 = 2;
            }
        }
        return 0;

    case 29:
        if ( bzda.field_1D0 == 2 || bzda.field_1D0 == 4 || bzda.field_1D0 == 16 )
        {
            bzda.field_8F8--;

            if ( bzda.field_8F8 < 0 )
                bzda.field_8F8 = bzda.field_8E8 - 1;

            if ( bzda.field_8F8 >= bzda.field_8E0 )
            {
                bzda.field_8F4 = bzda.field_8F8 - bzda.field_8E0;
                bzda.field_8EC = -1;
                bzda.field_1D0 = 16;
            }
            else
            {
                bzda.field_8EC = bzda.field_8F8;
                bzda.field_8F4 = -1;

                if ( !(bzda.field_1D0 & 4) )
                    bzda.field_1D0 = 2;
            }
        }
        return 0;

    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
        if ( yw->_activeCmdrRemapIndex != -1 )
        {
            int v16;

            switch ( inpt->HotKeyID )
            {
            case 38:
                v16 = 0;
                break;

            case 39:
                v16 = 25;
                break;

            case 40:
                v16 = 50;
                break;

            case 41:
                v16 = 75;
                break;

            case 42:
                v16 = 100;
                break;
            }

            yw->_cmdrsRemap[ yw->_activeCmdrRemapIndex ]->setBACT_aggression(v16);

            if ( yw->_GameShell )
                SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
        }
        break;

    case 43:
        yw->sub_449DE8(Locale::Text::Help(Locale::HELP_GAMEPLAY));
        break;

    case 46:
        if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 8;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    default:
        break;
    }

    return 1;
}

void ypaworld_func64__sub7__sub2__sub2(NC_STACK_ypaworld *yw)
{
    if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
    {
        if ( yw->_userUnit == yw->_userRobo )
        {
            yw->_roboMapStatus.Valid = true;
            yw->_roboMapStatus.IsOpen = robo_map.IsOpen();
            yw->_roboMapStatus.Rect.x = robo_map.x;
            yw->_roboMapStatus.Rect.y = robo_map.y;
            yw->_roboMapStatus.Rect.w = robo_map.w;
            yw->_roboMapStatus.Rect.h = robo_map.h;

            yw->_roboMapStatus.Data[0] = robo_map.field_1EC;
            yw->_roboMapStatus.Data[1] = robo_map.field_1ED;
            yw->_roboMapStatus.Data[2] = robo_map.field_1EE;
            yw->_roboMapStatus.Data[3] = robo_map.field_208;
            yw->_roboMapStatus.Data[4] = robo_map.field_20A;
            yw->_roboMapStatus.Data[5] = robo_map.field_20C;
            yw->_roboMapStatus.Data[6] = robo_map.field_20E;
            yw->_roboMapStatus.Data[7] = 0;

            yw->_roboFinderStatus.Valid = true;
            yw->_roboFinderStatus.IsOpen = squadron_manager.IsOpen();
            yw->_roboFinderStatus.Rect.x = squadron_manager.x;
            yw->_roboFinderStatus.Rect.y = squadron_manager.y;
            yw->_roboFinderStatus.Rect.w = squadron_manager.w;
            yw->_roboFinderStatus.Rect.h = squadron_manager.h;
        }
        else
        {
            yw->_vhclMapStatus.Valid = true;
            yw->_vhclMapStatus.IsOpen = robo_map.IsOpen();
            yw->_vhclMapStatus.Rect.x = robo_map.x;
            yw->_vhclMapStatus.Rect.y = robo_map.y;
            yw->_vhclMapStatus.Rect.w = robo_map.w;
            yw->_vhclMapStatus.Rect.h = robo_map.h;

            yw->_vhclMapStatus.Data[0] = robo_map.field_1EC;
            yw->_vhclMapStatus.Data[1] = robo_map.field_1ED;
            yw->_vhclMapStatus.Data[2] = robo_map.field_1EE;
            yw->_vhclMapStatus.Data[3] = robo_map.field_208;
            yw->_vhclMapStatus.Data[4] = robo_map.field_20A;
            yw->_vhclMapStatus.Data[5] = robo_map.field_20C;
            yw->_vhclMapStatus.Data[6] = robo_map.field_20E;
            yw->_vhclMapStatus.Data[7] = 0;

            yw->_vhclFinderStatus.Valid = true;
            yw->_vhclFinderStatus.IsOpen = squadron_manager.IsOpen();
            yw->_vhclFinderStatus.Rect.x = squadron_manager.x;
            yw->_vhclFinderStatus.Rect.y = squadron_manager.y;
            yw->_vhclFinderStatus.Rect.w = squadron_manager.w;
            yw->_vhclFinderStatus.Rect.h = squadron_manager.h;
        }
    }
}

NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub5(NC_STACK_ypaworld *yw)
{
    if ( bzda.field_8F0 != -1 )
    {
        if ( yw->_playerInHSGun )
        {
            bzda.field_8F0++;

            if ( bzda.field_8F0 >= bzda.field_8DC )
            {
                bzda.field_8F0 = 0;
                yw->_playerInHSGun = false;
                return yw->_userRobo;
            }

            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

            yw->_playerInHSGun = true;

            return robo->GetGuns().at(  bzda.field_1DC[ bzda.field_8F0 ]  ).gun_obj;
        }
        else
        {
            bzda.field_8F0 = 0;

            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

            yw->_playerInHSGun = true;

            return robo->GetGuns().at( bzda.field_1DC[ bzda.field_8F0 ] ).gun_obj;
        }
    }
    return nullptr;
}

NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub4(NC_STACK_ypaworld *yw)
{
    if ( yw->_userUnit->IsParentMyRobo() )
    {
        for ( NC_STACK_ypabact* &unit : yw->_userUnit->_kidList )
        {
            if ( unit->_status != BACT_STATUS_CREATE && unit->_status != BACT_STATUS_DEAD && unit->_status != BACT_STATUS_BEAM &&
                 !unit->_unitGunNoSwitch )
            return unit;
        }
    }
    else
    {
        World::RefBactList *lst = yw->_userUnit->_kidRef.PList();
        if (lst)
        {
            for( World::RefBactList::iterator it = std::next(yw->_userUnit->_kidRef.iter(), 1); it != lst->end(); it++) // Start from current unit in this list
            {
               if ( (*it)->_status != BACT_STATUS_CREATE && (*it)->_status != BACT_STATUS_DEAD && (*it)->_status != BACT_STATUS_BEAM &&
                    !(*it)->_unitGunNoSwitch )
                    return *it;
            }
        }
    }

    if ( yw->_userUnit->_parent != yw->_userUnit->_host_station && yw->_userUnit->_parent )
        return yw->_userUnit->_parent;

    return nullptr;
}

int sub_4C885C()
{
    if ( dword_5BAF9C == 3 )
    {
        if ( lstvw2.IsClosed() )
            dword_5BAF9C = 2;
    }
    return dword_5BAF9C;
}

void ypaworld_func64__sub7__sub2__sub8(NC_STACK_ypaworld *yw)
{
    if ( sub_4C885C() != 3 )
    {
        yw->GuiWinOpen( &exit_menu );
        exit_menu.Attach(yw->_guiActive);
    }
}

void ypaworld_func64__sub7__sub2__sub9(NC_STACK_ypaworld *yw)
{
    if ( sub_4C885C() != 3 )
        yw->GuiWinClose( &exit_menu );
}

void NC_STACK_ypaworld::SituationAnalyzer()
{
    yw_arg159 v38;
    v38.txt = World::GameAnalyzer::Analyze(this);
    v38.unit = 0;
    v38.Priority = 100;
    v38.MsgID = 0;

    ypaworld_func159(&v38);
}

void ypaworld_func64__sub7__sub2__sub0()
{
    bzda.field_8E8 = 0;

    for (int i = 0; i < bzda.field_8E0; i++)
    {
        bzda.field_4DC[i].i = 1;
        bzda.field_4DC[i].d = bzda.field_2DC[i];
    }

    bzda.field_8E8 += bzda.field_8E0;

    if ( !(bzda.field_1D0 & 4) )
    {
        for (int i = 0; i < bzda.field_8E4; i++)
        {
            bzda.field_4DC[i + bzda.field_8E0].i = 2;
            bzda.field_4DC[i + bzda.field_8E0].d = bzda.field_3DC[i];
        }

        bzda.field_8E8 += bzda.field_8E4;
    }

    if ( bzda.field_8E8 )
    {
        if ( bzda.field_8E8 == 1 )
        {
            bzda.field_8F8 = 0;
        }
        else if ( bzda.field_8E8 <= bzda.field_8F8 )
        {
            bzda.field_8F8 = bzda.field_8E8 - 1;
        }
    }
    else
    {
        bzda.field_8F8 = -1;
    }
}

void sb_0x4c66f8__sub0(NC_STACK_ypaworld *yw)
{
    if ( yw->_timeStamp - yw->_msgTimestampHSReturn > 35000 )
    {
        yw_arg159 arg159;
        arg159.unit = yw->_userRobo;
        arg159.Priority = 10;
        arg159.MsgID = 39;

        yw->ypaworld_func159(&arg159);
    }

    yw->_msgTimestampHSReturn = yw->_timeStamp;
}

void sb_0x4c66f8(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact1, NC_STACK_ypabact *bact2)
{
    if ( !yw->CanControlUnitInSpectatorMode(bact1) )
        return;

    // A latched player Kamikaze sequence is intentionally irreversible. Keep
    // control on the armed carrier until its shared detonation path completes;
    // this guard runs before viewer/input flags are dropped from the current unit.
    if ( yw->_kamikazeFireTimeScaleDrainGid > 0 &&
         yw->_userUnit &&
         yw->_userUnit->_gid == yw->_kamikazeFireTimeScaleDrainGid &&
         bact1 != bact2 )
    {
        return;
    }

    // OpenNeoUA custom: never take control of an artillery shell platform. Bail out before the
    // current unit's viewer/input is dropped, so the player is not left viewer-less.
    if ( bact1 && bact1->IsArtilleryShellPlatform() )
        return;

    if ( bact1 != bact2 )
    {
        if ( bact1->_status != BACT_STATUS_CREATE && bact1->_status != BACT_STATUS_DEAD && bact1->_status != BACT_STATUS_BEAM )
        {
            yw->_hud.field_76 = yw->_timeStamp;

            bact2->setBACT_viewer(false);
            bact2->setBACT_inputting(false);

            bact1->setBACT_viewer(true);
            bact1->setBACT_inputting(true);

            yw->GuiWinClose( &gui_lstvw );

            if ( bzda.field_1CC & 1 )
            {
                if ( bzda.field_1D0 == 8 )
                    bzda.field_1D0 = 1;
            }

            if ( bact1 == yw->_userRobo )
            {
                yw->_mouseGrabbed = false;
                sb_0x4c66f8__sub0(yw);
            }
            else
            {
                yw_arg159 arg159;
                arg159.unit = yw->_userUnit;
                arg159.Priority = 33;
                arg159.MsgID = 17;

                yw->ypaworld_func159(&arg159);
            }

            yw->_guiDragDefaultMouse = false;
        }
    }
}

void  ypaworld_func64__sub7__sub2(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    NC_STACK_ypabact *bact1 = nullptr;
    NC_STACK_ypabact *bact2 = yw->_viewerBact;

    bzda.field_91C = 0;

    int a2a = 1;

    if ( bzda.field_1D4 & 1 )
    {
        if ( yw->IsSpectatorControlled() )
        {
            // Spectator UI accepts only the observer-safe actions handled by
            // sub3 (Map and Exit Menu). Unknown hotkeys are deliberately left
            // untouched so the controlled UFO's normal User_layer can consume
            // present and future class-level controls.
            if ( inpt->HotKeyID >= 0 )
                a2a = ypaworld_func64__sub7__sub2__sub3(yw, inpt);
        }
        else
        {
            switch ( inpt->HotKeyID )
            {
                case 31:
                {
                    NC_STACK_ypabact *v9 = yw->GetLastMsgSender();

                    if ( v9 )
                        bact1 = v9;
                }
                break;

                case 44:
                    bact1 = ypaworld_func64__sub7__sub2__sub6(yw);
                    break;

                case 45:
                    ypaworld_func64__sub7__sub2__sub7(yw);
                    break;

                default:
                    if ( inpt->HotKeyID >= 0 )
                        a2a = ypaworld_func64__sub7__sub2__sub3(yw, inpt);

                    break;
            }
        }

        if ( yw->_userUnit->_status == BACT_STATUS_DEAD && !yw->IsSpectatorControlled() )
        {
            if ( yw->_fireBtnDownHappen )
            {
                bact1 = yw->GetNextSquad();
                inpt->Buttons.UnSet(0);
            }
        }

        if ( gui_lstvw.IsOpen() )
        {
            if ( yw->_kbdLastKeyHit == Input::KC_ESCAPE || yw->_kbdLastKeyHit == Input::KC_RETURN )
            {
                yw->GuiWinClose(&gui_lstvw);

                inpt->KbdLastDown = Input::KC_NONE;
                inpt->HotKeyID = -1;
                inpt->KbdLastHit = Input::KC_NONE;
            }
        }

        if ( yw->_userUnit == yw->_userRobo )
        {
            if ( robo_map.field_1E8 & 8 )
                robo_map.field_1E8 &= 0xFFFFFFF7;
        }
        else if ( !(robo_map.field_1E8 & 8) )
        {
            robo_map.field_1E8 |= 8;
        }

        ypaworld_func64__sub7__sub2__sub2(yw);

        if ( winpt->selected_btn == &bzda )
        {
            if ( winpt->selected_btnID != -1 )
            {
                if ( yw->_GameShell )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
                }
            }

            switch ( winpt->selected_btnID )
            {
            case 0:
                if ( yw->HasActiveNewGemNotification() )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN
                                      | TClickBoxInf::FLAG_BTN_HOLD
                                      | TClickBoxInf::FLAG_RM_DOWN
                                      | TClickBoxInf::FLAG_RM_HOLD) )
                        bzda.field_91C |= BZDA_GEM_PRESS_CREATE;

                    yw->GuiWinClose(&gui_lstvw);
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                    break;
                }

                if ( !(winpt->flag & TClickBoxInf::FLAG_BTN_DOWN) )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_RM_DOWN )
                    {
                        if ( bzda.field_1D0 & 0x16 )
                        {
                            bzda.field_1D0 = bzda.field_1CC & 1;
                            yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                        }
                        else
                        {

                            if ( !(bzda.field_1D0 & 4) || gui_lstvw.IsClosed() )
                            {
                                yw->GuiWinOpen( &gui_lstvw );
                                gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                            }
                            else
                            {
                                yw->GuiWinClose( &gui_lstvw );
                            }

                            a2a = 0;
                            bzda.field_1D0 = 4;
                        }
                    }
                    else
                        yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);

                }
                else if ( bzda.field_1D0 & 0x16 )
                {
                    bzda.field_1D0 = bzda.field_1CC & 1;

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                }
                else
                {
                    if ( !(bzda.field_1D0 & 2) || gui_lstvw.IsClosed() )
                    {
                        yw->GuiWinOpen( &gui_lstvw );
                        gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                    }
                    else
                    {
                        yw->GuiWinClose( &gui_lstvw );
                    }

                    if ( bzda.field_8E0 <= bzda.field_8F8 )
                        bzda.field_1D0 = 16;
                    else
                        bzda.field_1D0 = 2;

                    a2a = 0;
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                }
                break;

            case 1: // Jump into vehicle
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    if ( bzda.field_1D0 & 8 )
                        bzda.field_1D0 = bzda.field_1CC & 1;
                    else
                        bzda.field_1D0 = 8;
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_TAKECONTROL, 4);
                break;

            case 2: //MAP
            case 3: //SQUAD
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    if ( winpt->selected_btnID == 2 )
                    {
                        if (robo_map.IsClosed())
                        {
                            yw->GuiWinOpen(&robo_map);
                            yw->GuiWinToFront(&robo_map);
                        }
                        else
                            yw->GuiWinClose(&robo_map);
                    }
                    else if ( winpt->selected_btnID == 3 && !yw->IsSpectatorControlled() )
                    {
                        if (squadron_manager.IsClosed())
                        {
                            yw->GuiWinOpen(&squadron_manager);
                            yw->GuiWinToFront(&squadron_manager);
                        }
                        else
                            yw->GuiWinClose(&squadron_manager);
                    }
                }

                if ( winpt->selected_btnID == 2 )
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_MAPWND, 8);
                else if ( winpt->selected_btnID == 3 && !yw->IsSpectatorControlled() )
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_FINDERWND, 9);

                break;

            case 4: //Back to host station
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x10;

                if ( yw->IsSpectatorFollowActive() )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        yw->ReturnToSpectatorVehicle();
                        yw->_guiDragDefaultMouse = false;
                    }

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_RETURNSPECTATOR, 21);
                    break;
                }

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    bact1 = yw->_userRobo;

                    if ( yw->_playerInHSGun )
                        bzda.field_1D0 = 1;
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOTOHS, 21);
                break;

            case 5:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x20;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    bact1 = yw->_userUnit->_parent;

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOTOCMDR, 23);
                break;

            case 6: // Cycle through units in squad
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x40;

                if ( yw->_userRobo == yw->_userUnit || yw->_playerInHSGun )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        bact1 = ypaworld_func64__sub7__sub2__sub5(yw);

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOINTOGUNS, 22);
                }
                else
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        bact1 = ypaworld_func64__sub7__sub2__sub4(yw);

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEXTUNIT, 22);
                }
                break;

            case 7: //Cycle through squad leader
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x80;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    bact1 = yw->GetNextSquad();

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEXTCMDR, 20);
                break;

            case 8:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    yw->SituationAnalyzer();

                yw->SetShowingTooltipWithHotkey(Locale::TIP_ANALYZER, 46);
                break;

            case 9:
                // Deprecated online-help button removed from the toolbar.
                break;

            case 10:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x400;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    if ( exit_menu.IsClosed() )
                        ypaworld_func64__sub7__sub2__sub8(yw);
                    else
                        ypaworld_func64__sub7__sub2__sub9(yw);
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_PAUSECANCEL, 24);
                break;

            default:
                break;
            }
        }

        if ( bzda.field_1D0 & 0x29 )
            yw->GuiWinClose(&gui_lstvw);

        if ( a2a )
        {
            gui_lstvw.InputHandle(yw, inpt);
            if ( gui_lstvw.IsOpen() )
            {
                if ( bzda.field_1D0 == 2 || bzda.field_1D0 == 4 || bzda.field_1D0 == 16)
                {
                    bzda.field_8F8 = gui_lstvw.selectedEntry;

                    if ( gui_lstvw.selectedEntry >= bzda.field_8E0 )
                    {
                        bzda.field_8F4 = gui_lstvw.selectedEntry - bzda.field_8E0;
                        bzda.field_8EC = -1;
                        bzda.field_1D0 = 16;
                    }
                    else
                    {
                        bzda.field_8F4 = -1;
                        bzda.field_8EC = gui_lstvw.selectedEntry;

                        if ( !(bzda.field_1D0 & 4) )
                            bzda.field_1D0 = 2;
                    }
                }
            }
        }
    }

    if ( bact1 )
    {
        if ( bact1 != bact2 )
            sb_0x4c66f8(yw, bact1, bact2);
    }

    ypaworld_func64__sub7__sub2__sub0();
    ypaworld_func64__sub7__sub2__sub1(yw);
}

void sub_4E1D6C(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, uint8_t icon, uint8_t icon2, uint8_t icon3, float a6, float a7, const std::string &a8)
{
    int v26 = (a6 * up_panel.field_1E4);
    int v25 = (a7 * up_panel.field_1E4);

    if ( !v26 && a6 > 0.0 )
        v26 = 1;

    if ( !v25 && a7 > 0.0 )
        v25 = 1;

    FontUA::select_tileset(cur, 30);
    FontUA::set_center_xpos(cur, x);
    FontUA::set_center_ypos(cur, y);

    FontUA::store_u8(cur, icon);

    FontUA::store_u8(cur, 84);

    if ( v26 > 0 || v25 > 0 )
    {
        int v13 = up_panel.field_1E0 + up_panel.field_1DC + x;

        FontUA::set_center_xpos(cur, v13);

        if ( v26 > 0 )
        {
            if ( v26 > 1 )
                FontUA::op11(cur, v26);

            FontUA::store_u8(cur, icon2);
        }

        if ( v25 > v26 )
        {
            if ( v25 > 1 )
                FontUA::op11(cur, v25);

            FontUA::store_u8(cur, icon3);
        }
    }

    if ( !a8.empty() )
    {
        FontUA::select_tileset(cur, 31);

        FontUA::set_center_xpos(cur, x + up_panel.field_1DC + 4);
        FontUA::set_ypos(cur, 0);

        cur->insert(cur->end(), a8.begin(), a8.end());
    }
}

void ypaworld_func64__sub7__sub7__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, int a3, int a4, int a5, int a6, float a7)
{
    int icon;

    if ( a3 == -1 )
        icon = 82;
    else if ( a3 == 0 )
        icon = 83;
    else if ( a3 == 1 )
        icon = 81;

    int v11 = 0;
    switch ( yw->_userRobo->_pSector->owner )
    {
    case 1:
        v11 = 49;
        break;

    case 2:
        v11 = 50;
        break;

    case 3:
        v11 = 51;
        break;

    case 4:
        v11 = 52;
        break;

    case 5:
        v11 = 53;
        break;

    case 6:
        v11 = 54;
        break;

    case 7:
        v11 = 55;
        break;

    default:
        v11 = 0;
        break;
    }

    float a6a, a7a;

    if ( a4 <= 0 )
    {
        a7a = 0.0;
        a6a = 0.0;
    }
    else
    {
        a7a = a4 / 255.0;
        a6a = a7 * a7a;
    }

    std::string a1a;

    if ( a3 >= 0 )
    {
        if ( a5 )
        {
            a1a = fmt::sprintf("+%d   (%d%%)", a5, (int)(a7 * 100.0) );
        }
        else
        {
            a1a = fmt::sprintf("%d", 0);
        }
    }
    else
    {
        a1a = fmt::sprintf("%d", -a5);
    }

    sub_4E1D6C(yw, cur, x, y, icon, v11, 57, a6a, a7a, a1a);
}

void ypaworld_func64__sub7__sub7__sub0__sub1(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char a4, int a5, int a6, int a7)
{
    int v16;
    if ( a4 & 1 )
    {
        if ( a5 == -1 )
        {
            v16 = 66;
        }
        else if ( a5 == 0 )
        {
            v16 = 67;
        }
        else if ( a5 == 1 )
        {
            v16 = 65;
        }
    }
    else
    {
        v16 = 68;
    }

    float a6a = (float)a6 / (float)a7;
    std::string a1a = fmt::sprintf("%d", a6 / 100);

    int v10;

    if ( a7 / 5 <= a6 )
    {
        v10 = 56;
    }
    else
    {
        v10 = 54;

        if ( yw->_timeStamp / 300 & 1 )
            a1a = "";
    }

    sub_4E1D6C(yw, cur, x, y, v16, v10, 0, a6a, 0.0, a1a);
}

void ypaworld_func64__sub7__sub7__sub0__sub2(NC_STACK_ypaworld *yw, CmdStream *cur, int a3, int a2, char a4, int a5, int a6, int a7)
{
    int v18;
    if ( a4 & 4 )
    {
        if ( a5 == -1 )
            v18 = 70;
        else if ( a5 == 0 )
            v18 = 71;
        else if ( a5 == 1 )
            v18 = 69;
    }
    else
    {
        v18 = 72;
    }

    float v15 = (float)a7;
    int v9 = 57;
    float a7a = 0.0;
    float a6a = (float)a6 / v15;

    if ( bzda.field_1D0 & 0x16 )
    {
        if ( bzda.field_8F8 != -1 )
            a7a = yw->_updateMessage.energy / v15;
    }

    if ( a7a <= a6a )
    {
        float v10 = a6a;
        a6a -= a7a;
        a7a = v10;
    }
    else
    {
        a7a = a6a;
        v9 = 54;
        a6a = 0.0;
    }

    sub_4E1D6C(yw, cur, a3, a2, v18, 56, v9, a6a, a7a,  fmt::sprintf("%d", a6 / 100) );
}

void ypaworld_func64__sub7__sub7__sub0__sub3(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char a4, int a5, int a6, int a7)
{
    int v18;
    if ( a4 & 8 )
    {
        if ( a5 == -1 )
            v18 = 78;
        else if ( a5 == 0 )
            v18 = 79;
        else if ( a5 == 1 )
            v18 = 77;
    }
    else
    {
        v18 = 80;
    }

    float v15 = (float)a7;
    int v9 = 57;
    float a7a = 0.0;
    float a6a = (float)a6 / v15;

    if ( bzda.field_1D0 & 0x20 )
        a7a = yw->_updateMessage.energy / v15;

    if ( a7a <= a6a )
    {
        float v10 = a6a;
        a6a -= a7a;
        a7a = v10;
    }
    else
    {
        a7a = a6a;
        v9 = 54;
        a6a = 0.0;
    }

    sub_4E1D6C(yw, cur, x, y, v18, 56, v9, a6a, a7a,  fmt::sprintf("%d", a6 / 100) );
}

void ypaworld_func64__sub7__sub7__sub0(NC_STACK_ypaworld *yw)
{
    up_panel.cmdCommands.clear();

    if ( yw->IsSpectatorControlled() )
    {
        Input::Engine.RemClickBox(&up_panel);
        FontUA::set_end(&up_panel.cmdCommands);
        return;
    }

    if ( yw->_userRobo->_status != BACT_STATUS_DEAD )
    {
        int v3 = up_panel.x - yw->_screenSize.x / 2;
        int y = up_panel.y - yw->_screenSize.y / 2;
        int v41 = up_panel.field_1D8 + up_panel.field_1D0;

        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

        int a4 = robo->getROBO_fillMode();
        int v29 = robo->getROBO_battVehicle();
        int v28 = robo->getROBO_battBeam();
        int v27 = robo->getROBO_loadFlags();
        int v26 = robo->getROBO_lossFlags();

        int v36 = yw->_userRobo->_energy_max;

        yw_arg176 arg176;
        arg176.owner = yw->_userRobo->_pSector->owner;

        yw->ypaworld_func176(&arg176);

        int v32 = yw->_userRobo->_pSector->energy_power;
        int v33 = yw->_userRobo->_pSector->energy_power * arg176.field_4;

        int idx = robo->getROBO_absReload();

        int v11 = 10 * idx / 100;
        int v38 = 10 * ( idx * arg176.field_4 ) / 100;

        int v13, v34, v37, v35;

        if ( yw->_userRobo->_owner == yw->_userRobo->_pSector->owner )
            v13 = v33 != 0;
        else if ( v33 )
            v13 = -1;
        else
            v13 = 0;

        if ( v27 & 1 )
            v34 = 1;
        else if ( v26 & 1 )
            v34 = -1;
        else
            v34 = 0;

        if ( v27 & 4 )
            v37 = 1;
        else if ( v26 & 4 )
            v37 = -1;
        else
            v37 = 0;

        if ( v27 & 8 )
            v35 = 1;
        else if ( v26 & 8 )
            v35 = -1;
        else
            v35 = 0;

        Input::Engine.RemClickBox(&up_panel);
        Input::Engine.AddClickBoxFront(&up_panel);

        int x = (up_panel.field_1D4 & 0xFFFF) + v3;

        ypaworld_func64__sub7__sub7__sub0__sub0(yw, &up_panel.cmdCommands, x, y, v13, v32, v38, v11, arg176.field_4);

        int v18 = v41 + x;
        ypaworld_func64__sub7__sub7__sub0__sub1(yw, &up_panel.cmdCommands, v18, y, a4, v34, yw->_userRobo->_energy, v36);

        yw_RenderPlasmaCurrencyHud(yw, &up_panel.cmdCommands);

        int v20 = v41 + v18;
        ypaworld_func64__sub7__sub7__sub0__sub2(yw, &up_panel.cmdCommands, v20, y, a4, v37, v29, v36);
        ypaworld_func64__sub7__sub7__sub0__sub3(yw, &up_panel.cmdCommands, v41 + v20, y, a4, v35, v28, v36);
    }

    FontUA::set_end(&up_panel.cmdCommands);
}

void ypaworld_func64__sub7__sub7(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    if ( yw->IsSpectatorControlled() )
    {
        ypaworld_func64__sub7__sub7__sub0(yw);
        return;
    }

    if ( winpt->selected_btn == &up_panel )
    {
        if ( yw->_GameShell )
        {
            if ( winpt->selected_btnID != -1 )
            {
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                    SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
            }
        }

        if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
        {
            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

            int a4 = robo->getROBO_fillMode();

            if ( winpt->selected_btnID == 1 )
                a4 ^= 1;
            else if ( winpt->selected_btnID == 2 )
                a4 ^= 4;
            else if ( winpt->selected_btnID == 3 )
                a4 ^= 8;

            robo->setROBO_fillMode(a4);
        }

        switch ( winpt->selected_btnID )
        {
        case 0:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_RELOAD);
            break;

        case 1:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_MAIN);
            break;

        case 2:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_CREATION);
            break;

        case 3:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_TELEPORT);
            break;

        default:
            break;
        }
    }

    ypaworld_func64__sub7__sub7__sub0(yw);
}

char sub_4C7134(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    int protoId = ResolveUnitDisplayVehicleProtoId(yw, bact);
    uint8_t icon = protoId >= 0 ? yw->_vhclProtos[protoId].type_icon : 0;

    if ( !icon )
        icon = 65;

    return icon;
}

void sub_4C7950(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3)
{
    if ( a4 > 0 && a3 > 0 )
    {
        if ( a4 + squadron_manager.field_2CC < yw->_screenSize.x )
        {
            if ( a3 + yw->_fontH < yw->_screenSize.y )
            {
                FontUA::select_tileset(cur, 28);

                FontUA::set_center_xpos(cur, a4 - (yw->_screenSize.x / 2));
                FontUA::set_center_ypos(cur, a3 - (yw->_screenSize.y / 2));

                FontUA::set_opacity(cur, kSquadronManagerDragOpacity);
                FontUA::store_u8(cur, sub_4C7134(yw, squadron_manager.field_2BC));
                FontUA::set_opacity(cur, 255);
            }
        }
    }
}

void ypaworld_func64__sub7__sub3__sub0__sub2(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    yw_StoreGuiListRowBackground(&squadron_manager, cur, 0,
                                 yw_GetSquadronManagerBackgroundFillWidth(yw));

    FontUA::next_line(cur);
}

void sub_4514F0(TileMap *tyle, CmdStream *cur, const std::string &st, int a3, char a5)
{
    int wdth = 0;
    int char_num = 0;
    int neg_wdth = 0;

    for (size_t i = 0; i < st.size() && neg_wdth == 0; i++ )
    {
        uint8_t chr = (uint8_t)st[i];

        wdth += tyle->map[chr].w;

        if ( wdth <= a3 )
            char_num++;
        else
            neg_wdth = tyle->map[chr].w - (wdth - a3);
    }

    for (int i = 0; i < char_num; i++)
        cur->push_back( st[i] );


    if ( neg_wdth <= 0 )
    {
        if ( wdth < a3 )
        {
            int v13 = a3 - wdth;

            while( v13 > 0 )
            {
                if ( v13 <= 255 )
                    FontUA::op10(cur, v13);
                else
                    FontUA::op10(cur, 255);

                v13 -= 255;

                FontUA::store_u8(cur, a5);
            }
        }
    }
    else
    {
        FontUA::set_xwidth(cur, neg_wdth);

        FontUA::store_u8(cur, st[char_num]);
    }
}

void ypaworld_func64__sub7__sub3__sub0__sub1(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, CmdStream *cur)
{
    const int blink = (yw->_timeStamp / 300) & 1;
    char icon = sub_4C7134(yw, bact);
    if ( bact == yw->_userUnit && blink )
        icon = 33;
    else if ( info_log.field_255C == bact->_gid
              && yw->_timeStamp - info_log.msgs[info_log.field_24C].field_4 < 10000
              && blink )
    {
        icon = 34;
    }
    const bool selected = yw_IsSquadronManagerRowSelected(yw, bact);
    const bool hovered = yw_IsSquadronManagerRowHovered(bact);
    const int rowTileset = hovered && !selected ? 9 : 0;
    const int rowOpacity = selected ? 0 : (hovered ? 208 : -1);

    yw_StoreGuiListRowBackground(&squadron_manager, cur, rowTileset,
                                 yw_GetSquadronManagerBackgroundFillWidth(yw),
                                 rowOpacity);
    FontUA::add_xpos(cur,
        -(squadron_manager.entryWidth - 2 * yw->_fontBorderW + 1
          + yw_GetSquadronManagerScrollbarGapFillWidth(yw)));

    FontUA::select_tileset(cur, 28);
    FontUA::store_u8(cur, icon);

    // Keep the classic lifemeter visible for the Host Station as well.  The
    // 128..131 glyphs carry their own vertical offset and render above the
    // unit icon, matching the original Squadron Manager health indicator.
    FontUA::add_xpos(cur, -yw->_guiTiles[28]->map[(uint8_t)icon].w);
    FontUA::store_u8(cur, yw_GetSquadronManagerHealthGlyph(bact));

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub3__sub0__sub0(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, CmdStream *cur)
{
    const bool selected = yw_IsSquadronManagerRowSelected(yw, bact);
    const bool hovered = yw_IsSquadronManagerRowHovered(bact);
    const int rowTileset = hovered && !selected ? 9 : 0;
    const int rowOpacity = selected ? 0 : (hovered ? 208 : -1);
    const int rowFillWidth = yw_GetSquadronManagerBackgroundFillWidth(yw);
    const int rowRewind = squadron_manager.entryWidth
                        - 2 * yw->_fontBorderW + 1
                        + yw_GetSquadronManagerScrollbarGapFillWidth(yw);

    yw_StoreGuiListRowBackground(&squadron_manager, cur, rowTileset,
                                 rowFillWidth, rowOpacity);

    FontUA::add_xpos(cur, -rowRewind);
    yw_RenderSquadronManagerSquadContent(yw, bact, cur, 255);

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub3__sub0__sub3(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    const uint8_t backgroundOpacity = squadron_manager.backgroundOpacity;

    // Fixed aggression footer.  It is rendered after all visible squad rows,
    // inside the extra lower border reserved by create_squad_man().
    yw_StoreGuiListRowBackground(&squadron_manager, cur, 0,
                                 yw_GetSquadronManagerBackgroundFillWidth(yw));
    FontUA::add_xpos(cur,
        -(squadron_manager.entryWidth - 2 * yw->_fontBorderW + 1
          + yw_GetSquadronManagerScrollbarGapFillWidth(yw)));

    const int aggressionOffset = squadron_manager.field_2DC - yw->_fontBorderW;
    if ( aggressionOffset > 0 )
        FontUA::add_xpos(cur, aggressionOffset);

    yw_RenderSquadronAggressionStrip(yw, cur);
    FontUA::next_line(cur);

    yw_SetGuiListBackgroundOpacity(cur, backgroundOpacity);

    FontUA::reset_tileset(cur, 28);

    FontUA::set_yoff(cur, yw->_guiTiles[28]->h - yw->_fontBorderH);

    FontUA::store_u8(cur, 38);

    FontUA::op17(cur, yw_GetSquadronManagerBackgroundFillWidth(yw));

    FontUA::store_u8(cur, 47);
    FontUA::store_u8(cur, 61);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);
}

void ypaworld_func64__sub7__sub3__sub0(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    squadron_manager.itemBlock.clear();

    squadron_manager.ItemsPreLayout(yw, &squadron_manager.itemBlock, 0, "{ }");

    FontUA::select_tileset(&squadron_manager.itemBlock, 28);

    for (int i = 0; i < squadron_manager.shownEntries; i++)
    {
        NC_STACK_ypabact *v8 = squadron_manager.squads[i];

        if ( v8 )
        {
            if ( v8 == yw->_userRobo )
            {
                ypaworld_func64__sub7__sub3__sub0__sub1(yw, v8, &squadron_manager.itemBlock);
            }
            else
            {
                if ( v8->IsParentMyRobo() )
                    ypaworld_func64__sub7__sub3__sub0__sub0(yw, v8, &squadron_manager.itemBlock);
                else
                    ypaworld_func64__sub7__sub3__sub0__sub2(yw, &squadron_manager.itemBlock);
            }
        }
        else
        {
            ypaworld_func64__sub7__sub3__sub0__sub2(yw, &squadron_manager.itemBlock);
        }
    }

    ypaworld_func64__sub7__sub3__sub0__sub3(yw, &squadron_manager.itemBlock);

    if ( squadron_manager.field_2A8 & 1 )
    {
        if ( squadron_manager.field_2C8 != winpt->move.ScreenPos.x || squadron_manager.field_2CA != winpt->move.ScreenPos.y )
        {
            if ( squadron_manager.draggingWholeSquad )
            {
                yw_RenderSquadronManagerWholeSquadDragGhost(
                    yw, &squadron_manager.itemBlock,
                    squadron_manager.field_2C4, squadron_manager.field_2C6);
            }
            else
            {
                sub_4C7950(yw, &squadron_manager.itemBlock,
                           squadron_manager.field_2C4,
                           squadron_manager.field_2C6);

                if ( squadron_manager.field_2A8 & 2 )
                {
                    sub_4C7950(yw, &squadron_manager.itemBlock,
                               squadron_manager.field_2C4
                                   + squadron_manager.field_2CC / 4,
                               squadron_manager.field_2C6
                                   + (yw->_fontH / 4));
                }
            }
        }
    }

    FontUA::set_end(&squadron_manager.itemBlock);
}

void sub_4C707C(NC_STACK_ypaworld *yw)
{
    squadron_manager.squads.fill(nullptr);
    squadron_manager.squadRemapIndices.fill(-2);

    const int entryCount = (int)yw->_cmdrsRemap.size() + 1;
    if ( (size_t)(squadron_manager.firstShownEntries
                   + squadron_manager.shownEntries) >= (size_t)entryCount )
    {
        squadron_manager.firstShownEntries = entryCount
                                             - squadron_manager.shownEntries;
        if ( squadron_manager.firstShownEntries < 0 )
            squadron_manager.firstShownEntries = 0;
    }

    const int first = squadron_manager.firstShownEntries;
    for ( int i = 0; i < entryCount; i++ )
    {
        const int visibleIndex = i - first;
        if ( visibleIndex >= 0 && visibleIndex < squadron_manager.shownEntries )
        {
            squadron_manager.squads[visibleIndex] =
                i == 0 ? yw->_userRobo : yw->_cmdrsRemap[i - 1];
            squadron_manager.squadRemapIndices[visibleIndex] = i - 1;
        }
    }

    squadron_manager.numEntries = entryCount;
}

bool NC_STACK_ypaworld::ypaworld_func64__sub7__sub3__sub2()
{
    NC_STACK_ypabact *bct = _cmdrsRemap[squadron_manager.field_2B4];
    if ( !bct )
        return false;

    if ( bct == squadron_manager.field_2BC )
    {
        if (bct->_status != BACT_STATUS_DEAD && bct->_status != BACT_STATUS_CREATE && bct->_status != BACT_STATUS_BEAM && !bct->ShouldHideFromStrategicUI() )
            return true;
    }


    for ( NC_STACK_ypabact* &kid : bct->_kidList )
    {
        if ( kid == squadron_manager.field_2BC )
        {
            if ( kid->_status != BACT_STATUS_DEAD && kid->_status != BACT_STATUS_CREATE && kid->_status != BACT_STATUS_BEAM && !kid->ShouldHideFromStrategicUI() )
                return true;
        }
    }
    return false;
}

void ypaworld_func64__sub7__sub3__sub3(NC_STACK_ypaworld *yw, TClickBoxInf *winpt)
{
    if ( winpt->selected_btn != &squadron_manager
            || winpt->selected_btnID < 8
            || squadron_manager.numEntries <= (int32_t)(winpt->selected_btnID - 8 + squadron_manager.firstShownEntries)
            || squadron_manager.squads[ winpt->selected_btnID - 8 ] == 0
            || squadron_manager.squads[ winpt->selected_btnID - 8 ] == yw->_userRobo )
    {
        if ( squadron_manager.field_2A8 & 2 )
        {
            if ( squadron_manager.field_2BC->_host_station != squadron_manager.field_2BC->_parent )
            {
                NC_STACK_ypabact *v3 = nullptr;

                World::RefBactList *kidlist = squadron_manager.field_2BC->_kidRef.PList();
                if (kidlist)
                {
                    World::RefBactList::iterator it = squadron_manager.field_2BC->_kidRef;

                    while(it != kidlist->end())
                    {
                        NC_STACK_ypabact *itbact = *it;
                        it++;

                        bact_arg109 arg109;

                        if ( v3 )
                        {
                            arg109.field_4 = itbact;
                            arg109.field_0 = 4;
                        }
                        else
                        {
                            arg109.field_4 = nullptr;
                            arg109.field_0 = 3;

                            v3 = itbact;
                        }
                        v3->ReorganizeGroup(&arg109);
                    }
                }
            }
        }
        else
        {
            bact_arg109 arg109;
            arg109.field_4 = nullptr;
            arg109.field_0 = 3;

            squadron_manager.field_2BC->ReorganizeGroup(&arg109);
        }
        yw->_activeCmdrID = squadron_manager.field_2BC->_commandID;
        yw->sub_4C40AC();
        sub_4C707C(yw);
    }
    else
    {
        NC_STACK_ypabact *v6 = squadron_manager.squads[ winpt->selected_btnID - 8 ];
        int v7;
        if ( squadron_manager.field_2BC->IsParentMyRobo() )
            v7 = v6 == squadron_manager.field_2BC;
        else
            v7 = v6 == squadron_manager.field_2BC->_parent;

        if ( !v7 )
        {
            if ( squadron_manager.field_2A8 & 2 )
            {
                if ( squadron_manager.field_2BC->_host_station != squadron_manager.field_2BC->_parent )
                {
                    World::RefBactList *kidlist = squadron_manager.field_2BC->_kidRef.PList();
                    if (kidlist)
                    {
                        World::RefBactList::iterator it = squadron_manager.field_2BC->_kidRef;
                        while ( it != kidlist->end() )
                        {
                            NC_STACK_ypabact *itbact = *it;
                            it++;

                            bact_arg109 arg109;
                            arg109.field_0 = 1;
                            arg109.field_4 = v6;
                            itbact->ReorganizeGroup(&arg109);
                        }
                    }
                }
                else
                {
                    bact_arg109 arg109;
                    arg109.field_0 = 1;
                    arg109.field_4 = v6;
                    squadron_manager.field_2BC->ReorganizeGroup(&arg109);
                }
            }
            else
            {
                bact_arg109 arg109;

                if ( squadron_manager.field_2BC == yw->_userUnit )
                {
                    // Moving the directly controlled vehicle into another squad
                    // makes it that squad's commander. Reuse the existing
                    // ORG_BECOMECHIEF path so command ID, targets and members are
                    // transferred atomically instead of appending the player at
                    // the end of the destination list.
                    arg109.field_0 = 2;
                    arg109.field_4 = v6;
                    yw->_userUnit->ReorganizeGroup(&arg109);
                    v6 = yw->_userUnit;
                }
                else
                {
                    arg109.field_0 = 4;
                    arg109.field_4 = squadron_manager.field_2BC;
                    v6->ReorganizeGroup(&arg109);
                }
            }
            yw->_activeCmdrID = v6->_commandID;
            yw->sub_4C40AC();
            sub_4C707C(yw);
        }
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub7__sub3__sub4(NC_STACK_ypabact *bact)
{
    if ( bact->_status == BACT_STATUS_NORMAL )
    {
        if ( bact->_status_flg & BACT_STFLAG_ESCAPE )
            SetShowingTooltip(Locale::TIP_FINDER_ESCAPE);
        else if ( bact->_secndTtype )
            SetShowingTooltip(Locale::TIP_FINDER_FIGHTING);
        else if ( bact->_primTtype )
            SetShowingTooltip(Locale::TIP_FINDER_MOVE);
    }
    else if (bact->_status == BACT_STATUS_IDLE)
        SetShowingTooltip(Locale::TIP_FINDER_WAITORDER);
}

NC_STACK_ypabact * NC_STACK_ypaworld::sub_4C7B0C(int sqid, int a3)
{
    NC_STACK_ypabact *v3 = squadron_manager.squads[sqid];

    if ( !v3 )
    {
        squadron_manager.field_2B0 = -1;
        squadron_manager.field_2AC = -1;
        return nullptr;
    }

    if ( v3 == _userRobo )
    {
        squadron_manager.field_2B0 = -1;
        squadron_manager.field_2AC = -1;
        return _userRobo;
    }

    squadron_manager.field_2B0 = -1;
    squadron_manager.field_2AC = squadron_manager.squadRemapIndices[sqid];
    if ( squadron_manager.field_2AC < 0
            || (size_t)squadron_manager.field_2AC >= _cmdrsRemap.size() )
    {
        squadron_manager.field_2AC = -1;
        return nullptr;
    }

    const std::vector<NC_STACK_ypabact *> displayUnits =
        yw_GetSquadronDisplayUnits(v3);
    const int unitSlotCount = yw_GetSquadronManagerUnitSlotCount(this);

    if ( a3 >= squadron_manager.field_2D0
            && a3 < squadron_manager.field_2D0 + squadron_manager.field_2CC )
        return v3;

    if ( a3 < squadron_manager.field_2D4 )
    {
        squadron_manager.field_2B0 = -1;
        squadron_manager.field_2AC = -1;
        return nullptr;
    }

    const int slot = 1 + (a3 - squadron_manager.field_2D4)
                         / std::max(1, squadron_manager.field_2CC);
    if ( slot < unitSlotCount && slot < (int)displayUnits.size() )
    {
        squadron_manager.field_2B0 = slot - 1;
        return displayUnits[slot];
    }

    squadron_manager.field_2B0 = -1;
    squadron_manager.field_2AC = -1;
    return nullptr;
}

int NC_STACK_ypaworld::ypaworld_func64__sub7__sub3__sub1(TClickBoxInf *winpt)
{
    squadron_manager.draggingWholeSquad = false;

    if ( winpt->selected_btnID < 8 )
        return 0;
    if ( bzda.field_1D0 & 8 )
        return 0;
    if ( (int32_t)(winpt->selected_btnID - 8 + squadron_manager.firstShownEntries) >= squadron_manager.numEntries )
        return 0;

    const int squadIndex = winpt->selected_btnID - 8;
    NC_STACK_ypabact *v5 = sub_4C7B0C(squadIndex, winpt->move.BtnPos.x);

    // The blank part after the visible unit icons is the drag handle for the
    // complete squad.
    // A normal LMB click still goes through the GuiList selection path; holding
    // LMB and moving starts the existing drag/drop flow.  The whole-group flag
    // makes the drop reuse ORG_NEWCHIEF so leader and members are absorbed by
    // the destination squad together.
    if ( !v5 && (winpt->flag & TClickBoxInf::FLAG_LM_DOWN)
            && yw_IsSquadronManagerWholeSquadDragArea(
                   this, squadIndex, winpt->move.BtnPos.x) )
    {
        v5 = squadron_manager.squads[squadIndex];
        squadron_manager.draggingWholeSquad = true;
        squadron_manager.field_2AC =
            squadron_manager.squadRemapIndices[squadIndex];
        squadron_manager.field_2B0 = -1;
    }

    if ( !v5 )
        return 0;

    if ( robo_map.IsOpen() )
    {
        if ( winpt->flag & TClickBoxInf::FLAG_RM_DOWN )
        {
            robo_map.field_1D8 = v5->_position.x;
            robo_map.field_1ED = 0;
            robo_map.field_1DC = v5->_position.z;

            sub_4C1814(this, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
        }
    }

    if ( v5 == _userRobo )
        return 0;

    if ( winpt->flag & TClickBoxInf::FLAG_LM_DOWN )
    {
        // The list rows are rebuilt before the generic DOACTION_8 handler runs.
        // Publish the new stable squad identity now so that buffer and overlay
        // agree in this same frame; the normal action path completes selection.
        NC_STACK_ypabact *squadCommander = sub_4D3C3C(v5);
        if ( squadCommander )
            _activeCmdrID = squadCommander->_commandID;
    }

    squadron_manager.field_2B4 = squadron_manager.field_2AC;
    squadron_manager.field_2B8 = squadron_manager.field_2B0;
    squadron_manager.field_2C8 = winpt->move.ScreenPos.x;
    squadron_manager.field_2BC = v5;

    if ( squadron_manager.draggingWholeSquad )
    {
        // The whole-squad ghost follows the pointer, not the row's original
        // icon column.  This keeps the dragged icons centered under the mouse
        // regardless of whether the Squadron Manager is compact or widened.
        const std::vector<NC_STACK_ypabact *> displayUnits =
            yw_GetSquadronDisplayUnits(v5);
        const int ghostWidth = std::max(1, (int)displayUnits.size())
                             * std::max(1, squadron_manager.field_2CC);
        squadron_manager.field_2C0 = -(ghostWidth / 2);
        squadron_manager.field_2C2 = -(std::max(1, _fontH) / 2);
    }
    else
    {
        if ( v5->IsParentMyRobo() )
        {
            squadron_manager.field_2C0 = -(winpt->move.BtnPos.x - squadron_manager.field_2D0);
        }
        else
        {
            const int relativeX = winpt->move.BtnPos.x - squadron_manager.field_2D4;
            squadron_manager.field_2C0 = -(relativeX
                                          % std::max(1, squadron_manager.field_2CC));
        }

        squadron_manager.field_2C2 = -winpt->move.BtnPos.y;
    }
    squadron_manager.field_2CA = winpt->move.ScreenPos.y;
    return 1;
}

void sub_4C1FBC()
{
    ButtonBox &pMapInter = robo_map.buttons[17];

    int v0 = -(pMapInter.x + robo_map.x + (pMapInter.w / 2));
    int v3 = -(pMapInter.y + robo_map.y + (pMapInter.h / 2));
    flt_516524 = (float)(v0 + dword_516510) * robo_map.field_1E0 + robo_map.field_1D8;
    flt_51652C = (float)(v0 + dword_516518) * robo_map.field_1E0 + robo_map.field_1D8;
    flt_516528 = robo_map.field_1DC - (float)(v3 + dword_516514) * robo_map.field_1E4;
    flt_516530 = robo_map.field_1DC - (float)(v3 + dword_51651C) * robo_map.field_1E4;

    if ( flt_516524 > flt_51652C )
    {
        float v4 = flt_516524;
        flt_516524 = flt_51652C;
        flt_51652C = v4;
    }

    if ( flt_516528 > flt_516530 )
    {
        float v5 = flt_516528;
        flt_516528 = flt_516530;
        flt_516530 = v5;
    }
}

void ypaworld_func64__sub7__sub1__sub0(NC_STACK_ypaworld *yw)
{
    if ( yw && yw->IsSpectatorControlled() )
    {
        yw->_activeCmdrID = 0;
        yw->_activeCmdrRemapIndex = -1;
        yw->_activeCmdrKidsCount = 0;
        yw->_cmdrIdToSelect = -1;
        return;
    }

    int v8 = 0;

    int v22 = flt_516524 / World::CVSectorLength;
    int v6 = -flt_516530 / World::CVSectorLength;
    int v24 = flt_51652C / World::CVSectorLength;
    int v23 = -flt_516528 / World::CVSectorLength;

    if ( v22 >= yw->_mapSize.x )
        v22 = yw->_mapSize.x - 1;
    else if ( v22 < 1 )
        v22 = 1;

    if ( v24 >= yw->_mapSize.x )
        v24 = yw->_mapSize.x - 1;
    else if ( v24 < 1 )
        v24 = 1;

    if ( v6 >= yw->_mapSize.y )
        v6 = yw->_mapSize.y - 1;
    else if ( v6 < 1 )
        v6 = 1;

    if ( v23 >= yw->_mapSize.y )
        v23 = yw->_mapSize.y - 1;
    else if ( v23 < 1 )
        v23 = 1;

    NC_STACK_ypabact *v13 = nullptr;

    for (int i = v6; i <= v23; i++)
    {
        for (int j = v22; j <= v24; j++)
        {
            cellArea &cll = yw->_cells(j, i);

            if ( robo_map.MapViewMask & cll.view_mask )
            {
                for ( NC_STACK_ypabact* &bct : cll.unitsList )
                {
                    if ( bct->_owner == yw->_userRobo->_owner )
                    {
                        if ( bct->_position.x > flt_516524 && bct->_position.z > flt_516528 && bct->_position.x < flt_51652C && bct->_position.z < flt_516530 )
                        {
                            if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_GUN )
                            {
                                if ( bct->_status != BACT_STATUS_CREATE && bct->_status != BACT_STATUS_BEAM && bct->_status != BACT_STATUS_DEAD )
                                {

                                    if ( v13 )
                                    {
                                        if ( bct->_bact_type != BACT_TYPES_ROBO )
                                        {
                                            bact_arg109 arg109;
                                            arg109.field_4 = bct;
                                            arg109.field_0 = 4;

                                            v13->ReorganizeGroup(&arg109);

                                            v8 = 0;
                                        }
                                    }
                                    else
                                    {
                                        if ( bct->_bact_type == BACT_TYPES_ROBO )
                                        {
                                            if ( bct == yw->_userRobo )
                                                v8 = 1;
                                        }
                                        else
                                        {
                                            v13 = bct;

                                            bact_arg109 arg109;
                                            arg109.field_4 = nullptr;
                                            arg109.field_0 = 3;

                                            v13->ReorganizeGroup(&arg109);

                                            v8 = 0;
                                        }
                                    }


                                }
                            }
                        }
                    }
                }
            }

        }
    }

    if ( v8 )
        bzda.field_1D0 = bzda.field_1CC & 0x20;

    if ( v13 )
        yw->_activeCmdrID = v13->_commandID;

    yw->sub_4C40AC();
}

SDL_Color yw_GetFactionSelectionColor(NC_STACK_ypaworld *yw)
{
    if (yw && yw->_userRobo && yw->_userRobo->_owner >= 1 &&
        yw->_userRobo->_owner <= 6)
    {
        return yw->GetColor(yw->_userRobo->_owner);
    }

    return GFX::Engine.Color(210, 210, 195);
}

void ypaworld_func64__sub7__sub0__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int id)
{
    int v5 = id + info_log.field_254 + info_log.firstShownEntries;

    if ( v5 >= 64 )
        v5 -= 64;
    else if ( v5 < 0 )
        v5 += 64;

    FontUA::store_u8(cur, 123);

    FontUA::FormateClippedText(yw->_guiTiles[0], cur, info_log.msgs[v5].txt, info_log.entryWidth - 2 * yw->_fontBorderW, 32);

    FontUA::store_u8(cur, 125);
    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub0__sub0(NC_STACK_ypaworld *yw)
{
    info_log.itemBlock.clear();

    info_log.ItemsPreLayout(yw, &info_log.itemBlock, 0, "{ }");

    for (int i = 0; i < info_log.shownEntries; i++ )
        ypaworld_func64__sub7__sub0__sub0__sub0(yw, &info_log.itemBlock, i);

    info_log.ItemsPostLayout(yw, &info_log.itemBlock, 0, "xyz");

    FontUA::set_end(&info_log.itemBlock);
}

void ypaworld_func64__sub7__sub0(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( info_log.flags & (GuiBase::FLAG_CLOSED | GuiBase::FLAG_ICONIFED) )
    {
        info_log.InputHandle(yw, inpt);
    }
    else
    {
        info_log.InputHandle(yw, inpt);
        info_log.Formate(yw);
        ypaworld_func64__sub7__sub0__sub0(yw);
    }
}

GuiBase * ypaworld_func64__sub7__sub6__sub1()
{
    return dword_5BAFAC;
}

void NC_STACK_ypaworld::NetSendExitMsg(uint8_t normExit)
{
    uamessage_exit exMsg;
    exMsg.msgID = UAMSG_EXIT;
    exMsg.owner = _GameShell->netPlayerOwner;
    exMsg.norm = normExit;

    NetBroadcastMessage(&exMsg, sizeof(exMsg), true);

    _GameShell->sentAQ = false;
}

void sub_451714_with_yoffset(TileMap *, CmdStream *cur, const std::string &txt, int a2, uint8_t a4, int textYOffset)
{
    if ( a2 > 0 )
    {
        if ( textYOffset )
            FontUA::add_ypos(cur, textYOffset);

        FontUA::copy_position(cur);

        if ( textYOffset )
            FontUA::add_ypos(cur, -textYOffset);

        int v6 = a2;
        while (v6 > 0)
        {
            if ( v6 <= 255 )
                FontUA::op10(cur, v6);
            else
                FontUA::op10(cur, 255);

            FontUA::store_u8(cur, a4);

            v6 -= 255;
        }

        FontUA::add_txt(cur, a2, 4, txt);
    }
}

void sub_451714(TileMap *tiles, CmdStream *cur, const std::string &txt, int a2, uint8_t a4)
{
    sub_451714_with_yoffset(tiles, cur, txt, a2, a4, 0);
}

void sub_4DA8DC(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3, const std::string &a5)
{
    std::string v5 = a5;

    FontUA::set_yheight(cur, dword_5C8B84);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::op17(cur, dword_5C8B80 + dword_5C8B88);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    int v11;

    if ( a4 )
        v11 = 4;
    else if ( a3 )
        v11 = 8;
    else
        v11 = 3;

    FontUA::select_tileset(cur, v11);
    FontUA::store_u8(cur, 98);

    if ( a4 )
        v5 = " ";

    if ( a3 )
    {
        FontUA::set_flag(cur, 0x10);
    }

    sub_451714(yw->_guiTiles[v11], cur, v5, dword_5C8B88 - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);

    FontUA::set_yheight(cur, dword_5C8B84);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::op17(cur, dword_5C8B80 + dword_5C8B88);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub6__sub3(NC_STACK_ypaworld *yw, int a2, int a4)
{
    exit_menu.itemBlock.clear();
    exit_menu.ItemsPreLayout(yw, &exit_menu.itemBlock, 0, "{ }");

    const SDL_Color boxTextColor = yw->GetFactionBoxTextColor();
    FontUA::set_txtColor(&exit_menu.itemBlock, boxTextColor.r,
                         boxTextColor.g, boxTextColor.b);

    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x100, a2 & 0x100, Locale::Text::Common(Locale::CMN_EXITMISN));
    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x200, a2 & 0x200, Locale::Text::Common(Locale::CMN_SAVE));
    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x400, a2 & 0x400, Locale::Text::Common(Locale::CMN_LOAD));
    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x800, a2 & 0x800, Locale::Text::Common(Locale::CMN_RESTART));
    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x1000, a2 & 0x1000, Locale::Text::Common(Locale::CMN_QUITGAME));

    exit_menu.ItemsPostLayout(yw, &exit_menu.itemBlock, 0, "xyz");

    FontUA::set_end(&exit_menu.itemBlock);
}

void NC_STACK_ypaworld::ypaworld_func64__sub7__sub6(TInputState *inpt)
{
    if ( exit_menu.IsClosed() )
    {
        if ( _levelInfo.State != TLevelInfo::STATE_PLAYING )
        {
            if ( _levelInfo.State == TLevelInfo::STATE_SAVE )
            {
                yw_arg159 v18;
                v18.unit = 0;
                v18.Priority = 10;
                v18.txt = Locale::Text::Feedback(Locale::FEEDBACK_GAMESAVED);
                v18.MsgID = 0;

                ypaworld_func159(&v18);
            }

            _levelInfo.State = TLevelInfo::STATE_PLAYING;
        }
    }
    else
    {
        if ( _levelInfo.State != TLevelInfo::STATE_PAUSED )
        {
            _levelInfo.State = TLevelInfo::STATE_PAUSED;

            if ( _GameShell )
            {
                _hasSGMSave = UserData::IsHasSGM(_GameShell->UserName, 0);

                if ( UserData::IsHasRestartForLevel(_GameShell->UserName, _levelInfo.LevelID) )
                    _hasRSTSave = true;
                else
                    _hasRSTSave = false;
            }
        }
    }

    if ( ypaworld_func64__sub7__sub6__sub1() == &exit_menu )
    {
        int v7 = sub_4C885C();

        if ( v7 == 1 )
        {
            switch ( dword_5C8B78 )
            {
            case 8:
                _levelInfo.State = TLevelInfo::STATE_ABORTED;

                if ( _isNetGame )
                    NetSendExitMsg(1);

                break;

            case 9:
                _levelInfo.State = TLevelInfo::STATE_SAVE;
                break;

            case 10:
                _levelInfo.State = TLevelInfo::STATE_LOAD;
                break;

            case 11:
                _levelInfo.State = TLevelInfo::STATE_RESTART;
                break;

            case 14:
                _levelInfo.State = TLevelInfo::STATE_QUIT_GAME;

                if ( _isNetGame )
                    NetSendExitMsg(1);

                break;

            case 13:
                _helpURL = _helpSavedURL;
                _helpSavedURL.clear();
                break;

            default:
                break;
            }

            dword_5BAF9C = 0;
        }
        else if ( v7 == 2 )
        {
            dword_5BAF9C = 0;
        }
    }

    if ( exit_menu.IsClosed() )
    {
        exit_menu.InputHandle(this, inpt);
    }
    else
    {
        TClickBoxInf *winpt = &inpt->ClickInf;

        int a2 = 0;
        int a4 = 0;

        if ( !_hasSGMSave )
            a4 = 1024;

        if ( !_hasRSTSave )
            a4 |= 0x800;

        if ( _userUnit->_status == BACT_STATUS_DEAD )
            a4 |= 0x200;

        if ( _isNetGame )
            a4 |= 0xE00;

        if ( inpt->ClickInf.selected_btn == &exit_menu )
        {
            if ( _GameShell )
            {
                if ( inpt->ClickInf.selected_btnID >= 8 )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                }
            }

            switch ( inpt->ClickInf.selected_btnID )
            {
            case 7:
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    sub_449DE8(Locale::Text::Help(Locale::HELP_GAMEMENU));

                SetShowingTooltip(Locale::TIP_ONLINEHELP);
                break;

            case 8:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    a2 |= 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    dword_5C8B78 = 8;

                    if ( sub_4C885C() != 3 )
                        GuiWinClose( &exit_menu );

                    sb_0x4c87fc( Locale::Text::Advanced(Locale::ADV_RLYEXIT) , &exit_menu);
                }
                break;

            case 9:
                if ( !(a4 & 0x200) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        a2 |= 0x200;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        dword_5C8B78 = 9;

                        if ( sub_4C885C() != 3 )
                            GuiWinClose( &exit_menu );

                        sb_0x4c87fc( Locale::Text::Advanced(Locale::ADV_RLYSAVE) , &exit_menu);
                    }
                }
                break;

            case 10:
                if ( !(a4 & 0x400) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        a2 |= 0x400;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        dword_5C8B78 = 10;

                        if ( sub_4C885C() != 3 )
                            GuiWinClose( &exit_menu );

                        sb_0x4c87fc( Locale::Text::Advanced(Locale::ADV_RLYLOAD) , &exit_menu);
                    }
                }
                break;

            case 11:
                if ( !(a4 & 0x800) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        a2 |= 0x800;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        dword_5C8B78 = 11;

                        if ( sub_4C885C() != 3 )
                            GuiWinClose( &exit_menu );

                        sb_0x4c87fc(Locale::Text::Advanced(Locale::ADV_RLYRESTART) , &exit_menu);
                    }
                }
                break;

            case 12:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    a2 |= 0x1000;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    dword_5C8B78 = 14;

                    if ( sub_4C885C() != 3 )
                        GuiWinClose( &exit_menu );

                    sb_0x4c87fc(Locale::Text::Advanced(Locale::ADV_RLYQUITGAME), &exit_menu);
                }
                break;

            default:
                break;
            }
        }
        exit_menu.InputHandle(this, inpt);
        exit_menu.Formate(this);

        ypaworld_func64__sub7__sub6__sub3(this, a2, a4);
    }
}

void sub_4C8534(NC_STACK_ypaworld *yw, CmdStream *cur, const std::string &a2)
{
    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    sub_451714(yw->_guiTiles[0], cur, a2, lstvw2.entryWidth - 2 * dword_5BAFA8, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub4__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int a3)
{
    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    int v8;

    if ( a3 & 0x100 )
    {
        FontUA::set_flag(cur, 0x10);
        v8 = 8;
    }
    else
    {
        v8 = 3;
    }

    FontUA::select_tileset(cur, v8);

    FontUA::store_u8(cur, 98);

    sub_451714(yw->_guiTiles[v8], cur, Locale::Text::Common(Locale::CMN_OK), dword_5BAFA0 - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::op10(cur, dword_5BAFA4);

    FontUA::store_u8(cur, 32);

    if ( a3 & 0x200 )
    {
        FontUA::set_flag(cur, 0x10);
        v8 = 8;
    }
    else
    {
        v8 = 3;
    }

    FontUA::select_tileset(cur, v8);

    FontUA::store_u8(cur, 98);

    sub_451714(yw->_guiTiles[v8], cur, Locale::Text::Common(Locale::CMN_CANCEL), dword_5BAFA0 - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

void NC_STACK_ypaworld::ypaworld_func64__sub7__sub4__sub0(int a2)
{
    lstvw2.itemBlock.clear();
    lstvw2.ItemsPreLayout(this, &lstvw2.itemBlock, 0, "{ }");

    const SDL_Color boxTextColor = GetFactionBoxTextColor();
    FontUA::set_txtColor(&lstvw2.itemBlock, boxTextColor.r,
                         boxTextColor.g, boxTextColor.b);

    sub_4C8534(this, &lstvw2.itemBlock, dword_5BAF98);
    sub_4C8534(this, &lstvw2.itemBlock, " ");

    FontUA::set_txtColor(&lstvw2.itemBlock, boxTextColor.r,
                         boxTextColor.g, boxTextColor.b);

    ypaworld_func64__sub7__sub4__sub0__sub0(this, &lstvw2.itemBlock, a2);
    lstvw2.ItemsPostLayout(this, &lstvw2.itemBlock, 0, "xyz");

    FontUA::set_end(&lstvw2.itemBlock);
}

void NC_STACK_ypaworld::ypaworld_func64__sub7__sub4(TInputState *inpt)
{
    if ( lstvw2.IsClosed() )
    {
        lstvw2.InputHandle(this, inpt);
    }
    else
    {
        TClickBoxInf *winpt = &inpt->ClickInf;

        int a1_1 = 0;

        if ( _kbdLastKeyHit == Input::KC_RETURN )
        {
            GuiWinClose( &lstvw2 );

            dword_5BAF9C = 1;

            inpt->KbdLastHit = Input::KC_NONE;
            inpt->KbdLastDown = Input::KC_NONE;
            inpt->HotKeyID = -1;
        }
        else if ( _kbdLastKeyHit == Input::KC_ESCAPE )
        {
            GuiWinClose( &lstvw2 );

            dword_5BAF9C = 2;

            inpt->KbdLastHit = Input::KC_NONE;
            inpt->KbdLastDown = Input::KC_NONE;
            inpt->HotKeyID = -1;
        }
        else if ( inpt->ClickInf.selected_btn == &lstvw2 )
        {
            if ( _GameShell )
            {
                if ( inpt->ClickInf.selected_btnID >= 8 )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                }
            }

            if ( winpt->selected_btnID == 8 )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    a1_1 = 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    GuiWinClose( &lstvw2 );
                    dword_5BAF9C = 1;
                }
            }
            else if ( winpt->selected_btnID == 9 )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    a1_1 = 0x200;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    GuiWinClose( &lstvw2 );
                    dword_5BAF9C = 2;
                }
            }
        }

        lstvw2.InputHandle(this, inpt);
        lstvw2.Formate(this);
        ypaworld_func64__sub7__sub4__sub0(a1_1);
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub7(TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    if ( _userUnit->_bact_type != BACT_TYPES_MISSLE )
    {
        if ( winpt->flag & TClickBoxInf::FLAG_OK )
        {
            for(GuiBaseList::iterator it = _guiActive.begin(); it != _guiActive.end(); it++)
            {
                GuiBase *llst = *it;

                if ( llst->flags & GuiBase::FLAG_WITH_ICON )
                {
                    if ( llst->flags & GuiBase::FLAG_ICONIFED )
                        llst->iconString[2] = 1;
                }

                if ( llst->flags & GuiBase::FLAG_WITH_CLOSE )
                {
                    if ( !( llst->flags & (GuiBase::FLAG_ICONIFED | GuiBase::FLAG_CLOSED) ) )
                        llst->flags &= ~GuiBase::FLAG_CLOSE_DOWN;
                }
            }

            if ( inpt->ClickInf.selected_btn )
            {
                GuiBase *v9 = (GuiBase *)inpt->ClickInf.selected_btn->pobject;
                if ( v9 )
                {
                    if ( v9->flags & 1 )
                    {
                        if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                            v9->iconString[2] = 2;

                        if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        {
                            v9->flags &= 0xFFFFFFFE;

                            Input::Engine.RemClickBox(&v9->iconBox);
                            Input::Engine.AddClickBoxFront(v9);

                            v9->Attach(_guiActive);
                        }
                    }
                    else
                    {
                        if ( winpt->flag & TClickBoxInf::FLAG_LM_DOWN )
                        {
                            Input::Engine.RemClickBox(v9);
                            Input::Engine.AddClickBoxFront(v9);

                            v9->Attach(_guiActive);
                        }

                        if ( v9->flags & 0x10 )
                        {
                            if ( winpt->selected_btnID == 0)
                            {
                                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                                    v9->flags |= 0x40;

                                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                                {
                                    if ( _GameShell )
                                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                                }

                                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                                {
                                    Input::Engine.RemClickBox(v9);

                                    if ( v9->flags & 4 )
                                    {
                                        Input::Engine.AddClickBoxFront(&v9->iconBox);

                                        v9->flags |= 1;
                                    }
                                    else
                                    {
                                        v9->flags |= 0x20;
                                    }

                                    v9->Attach(_guiActive);
                                }
                            }
                        }

                        if ( v9->flags & 8 )
                        {
                            if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                            {
                                if ( winpt->selected_btnID == 1 )
                                {
                                    _guiDragDefaultMouse = true;
                                    _guiDragElement = v9;
                                    _guiDragging = true;
                                    _guiDragPos.x = winpt->ldw_pos.BoxPos.x;
                                    _guiDragPos.y = winpt->ldw_pos.BoxPos.y;
                                }
                            }
                        }
                    }
                }
            }

            if ( _guiDragging )
            {
                if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
                {
                    const int rawX = winpt->move.ScreenPos.x - _guiDragPos.x;
                    const int rawY = winpt->move.ScreenPos.y - _guiDragPos.y;
                    int v13 = rawX;
                    int v15 = rawY;

                    if ( v13 >= 0 )
                    {
                        if ( _guiDragElement->w + v13 > _screenSize.x )
                            v13 = _screenSize.x - _guiDragElement->w;
                    }
                    else
                    {
                        v13 = 0;
                    }

                    if ( v15 >= _upScreenBorder )
                    {
                        if ( _guiDragElement->h + v15 > _screenSize.y - _downScreenBorder )
                            v15 = _screenSize.y - _downScreenBorder - _guiDragElement->h;
                    }
                    else
                    {
                        v15 = _upScreenBorder;
                    }

                    // Map/Squadron keep the mouse attached to the dragged title
                    // even after the window reaches a screen limit. Rebase the
                    // grab offset at the clamp so reversing direction moves the
                    // window immediately instead of crossing a dead zone first.
                    if ( _guiDragElement == &robo_map ||
                         _guiDragElement == &squadron_manager )
                    {
                        if ( v13 != rawX )
                            _guiDragPos.x = winpt->move.ScreenPos.x - v13;
                        if ( v15 != rawY )
                            _guiDragPos.y = winpt->move.ScreenPos.y - v15;
                    }

                    _guiDragElement->y = v15;
                    _guiDragElement->x = v13;

                    int16_t tmp = v13 - (_screenSize.x / 2);
                    _guiDragElement->cmdCommands[5] = (tmp >>  8) & 0xFF ;
                    _guiDragElement->cmdCommands[6] = tmp & 0xFF ;

                    tmp = v15 - (_screenSize.y / 2);
                    _guiDragElement->cmdCommands[9] = (tmp >>  8) & 0xFF ;
                    _guiDragElement->cmdCommands[10] = tmp & 0xFF ;
                }
            }

            if ( _guiDragging )
            {
                if ( !(winpt->flag & TClickBoxInf::FLAG_LM_HOLD) )
                {
                    _guiDragDefaultMouse = false;
                    _guiDragging = false;
                }
            }

            if ( _isNetGame )
                ypaworld_func64__sub7__sub5(this, inpt);

            ypaworld_func64__sub7__sub4(inpt);
            ypaworld_func64__sub7__sub6(inpt);
            RoboMap_InputHandle(this, inpt);
            SquadManager_InputHandle(inpt);
            ypaworld_func64__sub7__sub0(this, inpt);
            ypaworld_func64__sub7__sub7(this, inpt);
        }

        // The tactical map input handler runs first and consumes wheel/+/-
        // whenever the map is open. Spectator Follow then receives the same
        // contextual controls here. The controlled Spectator UFO keeps its
        // remaining class-level input for the normal User_layer below. Keeping
        // this call outside FLAG_OK also preserves distance interpolation on
        // frames without a click-box event.
        UpdateSpectatorFollowCamera(inpt);

        yw_WorldSelectionDragInput(this, inpt);
        ypaworld_func64__sub7__sub2(this, inpt);
    }
    else
    {
        yw_ResetWorldSelectionDrag(this);
    }
}

NC_STACK_ypabact * NC_STACK_ypaworld::GetLastMsgSender()
{
    if ( !info_log.field_255C )
        return nullptr;

    for (NC_STACK_ypabact *comm : _cmdrsRemap)
    {
        NC_STACK_ypabact *found = nullptr;

        if ( info_log.field_255C == comm->_gid )
        {
            found = comm;
        }
        else
        {
            for (NC_STACK_ypabact *bact : comm->_kidList)
            {
                if ( bact->_status != BACT_STATUS_DEAD &&
                     bact->_status != BACT_STATUS_CREATE &&
                     bact->_status != BACT_STATUS_BEAM &&
                     !bact->ShouldHideFromStrategicUI() &&
                     bact->_gid == info_log.field_255C )
                {
                    found = bact;
                    break;
                }
            }
        }

        if (found)
        {
            if ( (_timeStamp - info_log.field_2560) > 10000 )
                return nullptr;

            return found;
        }
    }

    return nullptr;
}

bool sub_4C3E34(NC_STACK_ypabact* const &a1, NC_STACK_ypabact* const &a2)
{
    return (a1->_commandID & 0xFFFFFF) < (a2->_commandID & 0xFFFFFF);
}

void NC_STACK_ypaworld::sub_4C40AC()
{
    _cmdrsRemap.clear();
    _cmdrsRemap.reserve(128);

    if ( IsSpectatorControlled() )
    {
        _activeCmdrID = 0;
        _activeCmdrRemapIndex = -1;
        _activeCmdrKidsCount = 0;
        _cmdrIdToSelect = -1;
        _lastMsgSender = GetLastMsgSender();
        return;
    }

    if ( _userRobo )
    {
        for ( NC_STACK_ypabact * &bact : _userRobo->GetKidList())
        {
            if (bact->_status != BACT_STATUS_DEAD && bact->_status != BACT_STATUS_BEAM && bact->_bact_type != BACT_TYPES_GUN)
                _cmdrsRemap.push_back(bact);
        }
    }

    std::sort(_cmdrsRemap.begin(), _cmdrsRemap.end(), sub_4C3E34);

    if ( !_cmdrsRemap.empty() )
    {
        bool selected = false;
        if (_cmdrIdToSelect != -1)
        {
            for (size_t i = 0; i < _cmdrsRemap.size(); i++)
            {
                if (_cmdrIdToSelect == (int32_t)_cmdrsRemap[i]->_commandID)
                {
                    _activeCmdrRemapIndex = i;
                    _activeCmdrID = _cmdrIdToSelect;
                    selected = true;
                    break;
                }
            }
        }

        if (!selected)
        {
            if ( _activeCmdrID == 0 )
            {
                if ( bzda.field_1D0 & 6 )
                {
                    _activeCmdrID = _cmdrsRemap.back()->_commandID;
                    _activeCmdrRemapIndex = _cmdrsRemap.size() - 1;
                }
            }
            else
            {
                _activeCmdrRemapIndex = -1;

                for (size_t i = 0; i < _cmdrsRemap.size(); i++)
                {
                    if (_activeCmdrID == _cmdrsRemap[i]->_commandID)
                    {
                        _activeCmdrRemapIndex = i;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        _activeCmdrRemapIndex = -1;
    }

    _cmdrIdToSelect = -1;

    if ( _activeCmdrRemapIndex == -1 )
    {
        _activeCmdrKidsCount = 0;
    }
    else
    {
        _activeCmdrKidsCount = 0;
        for ( NC_STACK_ypabact* &bact : _cmdrsRemap[ _activeCmdrRemapIndex ]->_kidList )
        {
            if (bact->_status != BACT_STATUS_DEAD && bact->_status != BACT_STATUS_BEAM && bact->_bact_type != BACT_TYPES_GUN)
                _activeCmdrKidsCount++;
        }
    }

    _lastMsgSender = GetLastMsgSender();
}

void ypaworld_func64__sub14(NC_STACK_ypaworld *yw)
{
    if ( robo_map.IsOpen() )
    {
        sub_4C157C(yw);
        sub_4C1814(yw, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
        sub_4C0C00(yw);
        sub_4C0FEC(yw);
        sub_4C1214(yw);
        MapRoboUpdateMapInt(yw);
    }
}

void ypaworld_func64__sub8(NC_STACK_ypaworld *yw)
{
    if ( robo_map.IsOpen() )
    {
        if ( !(robo_map.field_1E8 & 0x16) && robo_map.field_1ED == 1 )
        {
            if ( yw->_viewerBact )
            {
                robo_map.field_1D8 = yw->_viewerBact->_position.x;
                robo_map.field_1DC = yw->_viewerBact->_position.z;

                sub_4C1814(yw, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
            }
        }
        robo_map.field_1E8 &= 0xFFFFFF1F;
    }
}

// OpenNeoUA: Genesis list order switch. Only an explicit energy value
// selects the Energy order; missing or unknown values keep vanilla.
bool yw_IsGenesisEnergyOrder()
{
    return !StriCmp(System::IniConf::UiGenesisListOrder.Get<std::string>(), "energy");
}

int sub_4C3C88(const void *a1, const void *a2)
{
    uint8_t aa1 = *(const uint8_t *)a1;
    uint8_t aa2 = *(const uint8_t *)a2;

    const World::TVhclProto &v4 = dword_5BAA60->_vhclProtos[aa1];
    const World::TVhclProto &v5 = dword_5BAA60->_vhclProtos[aa2];

    // OpenNeoUA: Genesis list order follows ui.genesis_list_order.
    // Energy sorts by final production cost only; missing or unknown
    // values keep the vanilla type-grouped order.
    if ( yw_IsGenesisEnergyOrder() )
    {
        const int cost1 = v4.GetProductionCost();
        const int cost2 = v5.GetProductionCost();

        if ( cost1 < cost2 )
            return -1;
        else if ( cost1 > cost2 )
            return 1;

        return 0;
    }

    int v7, v8;

    switch ( v4.model_id )
    {
    case BACT_TYPES_BACT:
        v7 = 1;
        break;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        v7 = 0;
        break;

    case BACT_TYPES_ROBO:
        v7 = 5;
        break;

    case BACT_TYPES_FLYER:
        v7 = 2;
        break;

    case BACT_TYPES_UFO:
        v7 = 3;
        break;

    case BACT_TYPES_GUN:
        v7 = 4;
        break;

    case BACT_TYPES_MISSLE:
    case BACT_TYPES_ZEPP:
        v7 = 10;
        break;

    default:
        v7 = 0;
        break;
    }

    switch ( v5.model_id )
    {
    case BACT_TYPES_BACT:
        v8 = 1;
        break;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        v8 = 0;
        break;

    case BACT_TYPES_ROBO:
        v8 = 5;
        break;

    case BACT_TYPES_FLYER:
        v8 = 2;
        break;

    case BACT_TYPES_UFO:
        v8 = 3;
        break;

    case BACT_TYPES_GUN:
        v8 = 4;
        break;

    case BACT_TYPES_MISSLE:
    case BACT_TYPES_ZEPP:
        v8 = 10;
        break;

    default:
        v8 = 0;
        break;
    }

    if ( v7 < v8 )
        return -1;
    else if ( v7 > v8)
        return 1;
    else if ( v7 == v8 )
    {
        if ( v4.energy > v5.energy )
            return 1;
        else if (v4.energy < v5.energy)
            return -1;
    }

    return 0;
}

void ypaworld_func64__sub16(NC_STACK_ypaworld *yw)
{
    int v6 = 0;

    if ( yw->_userRobo )
        v6 = 1 << yw->_userRobo->_owner;

    int v3 = 0;
    int i = 0;
    for (World::TVhclProto &proto : yw->_vhclProtos)
    {
        if ( v6 & proto.disable_enable_bitmask )
        {
            bzda.field_2DC[v3] = i;
            v3++;
        }
        i++;
    }

    bzda.field_8E0 = v3;

    dword_5BAA60 = yw;

    qsort(bzda.field_2DC, v3, 1, sub_4C3C88);

    if ( !v3 )
    {
        bzda.field_8EC = -1;
    }
    else
    {
        if ( v3 == 1 )
        {
            bzda.field_8EC = 0;
        }
        else if ( v3 <= bzda.field_8EC )
        {
            bzda.field_8EC = v3 - 1;
        }
    }
}

int sub_4C3D6C(const void *a1, const void *a2)
{

    uint8_t aa1 = *(const uint8_t *)a1;
    uint8_t aa2 = *(const uint8_t *)a2;

    World::TBuildingProto *v3 = &dword_5BAA60->_buildProtos[aa1];
    World::TBuildingProto *v4 = &dword_5BAA60->_buildProtos[aa2];

    // OpenNeoUA: same ui.genesis_list_order switch as the vehicle list.
    if ( yw_IsGenesisEnergyOrder() )
    {
        const int cost1 = v3->GetProductionCost();
        const int cost2 = v4->GetProductionCost();

        if ( cost1 < cost2 )
            return -1;
        else if ( cost1 > cost2 )
            return 1;

        return 0;
    }

    int v6, v8;

    if ( v3->ModelID == 1 )
        v6 = 0;
    else if ( v3->ModelID == 2 )
        v6 = 2;
    else if ( v3->ModelID == 3 )
        v6 = 1;
    else
        v6 = 10;

    if ( v4->ModelID == 1 )
        v8 = 0;
    else if ( v4->ModelID == 2 )
        v8 = 2;
    else if ( v4->ModelID == 3 )
        v8 = 1;
    else
        v8 = 10;

    if ( v6 >= v8 )
    {
        if ( v6 <= v8 )
        {
            if ( v3->Energy >= v4->Energy )
                return v3->Energy > v4->Energy;
            else
                return -1;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return -1;
    }
    return 0;
}

void ypaworld_func64__sub17(NC_STACK_ypaworld *yw)
{
    int v6 = 0;

    if ( yw->_userRobo )
        v6 = 1 << yw->_userRobo->_owner;

    int v3 = 0;

    int i = 0;
    for (const World::TBuildingProto &proto : yw->_buildProtos)
    {
        if ( v6 & proto.EnableMask )
        {
            bzda.field_3DC[v3] = i;
            v3++;
        }
        i++;
    }

    bzda.field_8E4 = v3;

    dword_5BAA60 = yw;

    qsort(bzda.field_3DC, v3, 1, sub_4C3D6C);

    if ( !v3 )
    {
        bzda.field_8F4 = -1;
    }
    else
    {
        if ( v3 == 1 )
        {
            bzda.field_8F4 = 0;
        }
        else if ( v3 <= bzda.field_8F4 )
        {
            bzda.field_8F4 = v3 - 1;
        }
    }
}

void ypaworld_func64__sub15(NC_STACK_ypaworld *yw)
{
    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

    int v4 = 0;
    int i = 0;

    for(World::TRoboGun &gun : robo->GetGuns())
    {
        if ( gun.gun_obj )
        {
            bzda.field_1DC[v4] = i;

            if ( gun.gun_obj == yw->_userUnit )
                bzda.field_8F0 = v4;

            v4++;
        }
        i++;
    }

    bzda.field_8DC = v4;

    if ( !v4 )
    {
        bzda.field_8F0 = -1;
    }
    else
    {
        if ( v4 == 1 )
        {
            bzda.field_8F0 = 0;
        }
        else if ( v4 <= bzda.field_8F0 )
        {
            bzda.field_8F0 = v4 - 1;
        }
    }
}

void sb_0x4d7c08__sub0__sub0(NC_STACK_ypaworld *yw)
{

    CmdStream buf;
    buf.reserve(1024);

    if ( yw->_isNetGame )
    {
        if ( yw->_netEvent.EventType )
        {
            uint32_t timeDelay;
            std::string str;

            if ( yw->_netEvent.EventType == 1 )
            {
                str = Locale::Text::Advanced(Locale::ADV_VICTORY);
                timeDelay = 40000;
            }
            else if ( yw->_netEvent.EventType > 1 && yw->_netEvent.EventType <= 4 )
            {
                str = fmt::sprintf( Locale::Text::Advanced(Locale::ADV_DEFEATED) , yw->_netEvent.PlayerName);

                timeDelay = 20000;
            }
            else
            {
                timeDelay = 0;
            }

            if ( !str.empty() && yw->_timeStamp - yw->_netEvent.TimeStamp < timeDelay )
            {
                if ( yw->_timeStamp / 300 & 1 )
                {
                    FontUA::select_tileset(&buf, 15);
                    FontUA::set_xpos(&buf, 0);
                    FontUA::set_ypos(&buf, yw->_screenSize.y / 3);

                    FontUA::set_txtColor(&buf, 255, 255, 255);

                    FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf, str, yw->_screenSize.x);
                }
            }
            else
            {
                yw->_netEvent.EventType = 0;
            }
        }
    }

    if ( sb_0x4d7c08__sub0__sub0__sub0(yw) )
        return;

    int v10;

    if ( info_log.field_254 <= info_log.msg_count )
        v10 = info_log.msg_count - info_log.field_254;
    else
        v10 = info_log.msg_count + 64 - info_log.field_254;

    if ( v10 > 10 )
        v10 = 10;

    FontUA::select_tileset(&buf, 15);
    FontUA::set_ypos(&buf, up_panel.field_1CC + yw->_fontH / 2);


    int v11 = (info_log.msg_count - v10) % 64;

    if ( v11 < 0 )
        v11 += 64;

    int v28 = 0;
    int v27 = 0;

    while ( 1 )
    {
        if ( v11 >= 64 )
            v11 = 0;

        inflog_msg *v14 = &info_log.msgs[v11];

        if ( v14->field_8 > 0 )
        {
            if ( !v27 )
            {
                v27 = 1;

                int v15;

                if ( v11 > info_log.msg_count )
                    v15 = info_log.msg_count + 64 - v11;
                else
                    v15 = info_log.msg_count - v11;

                v15 = (v15 + 1) / 2;

                if ( !v15 )
                    v15 = 1;

                v14->field_8 -= yw->_frameTime * v15;
            }

            if ( v14->field_4 )
                v28 = v14->field_4;
            else
                v28 += 200;

            int v18 = yw->_timeStamp - v28;

            int v19;

            if ( v18 < 200 )
            {
                v19 = 100 * v18 / 200;

                if ( v19 < 0 )
                    v19 = 0;
                else if ( v19 > 100 )
                    v19 = 100;
            }
            else
            {
                v19 = 100;
            }

            if ( v19 > 0 )
            {
                FontUA::set_xpos(&buf, 16);
                FontUA::set_txtColor(&buf, yw->_iniColors[64].r, yw->_iniColors[64].g, yw->_iniColors[64].b);
                // Output ingame messages. From analyzer and other.
                FontUA::TextRelWidthItem(yw->_guiTiles[15], &buf, v14->txt, v19, 4);

                FontUA::next_line(&buf);
            }
        }

        if ( v11 == info_log.msg_count )
            break;

        v11++;
    }

    FontUA::set_end(&buf);

    SDL_Color uiAccentColor;
    GFX::Engine.ProcessDrawSeq(buf, nullptr,
                              yw_GetFactionUiAccent(yw, &uiAccentColor));
}

void sub_4E4F80(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, float x, float y, int value, int maxval, int valCH, int valBG, const std::string &txt1, const std::string &txt2, int flag)
{
    int wnd_xpos = 0;
    int wnd_xpos2 = 0;
    int wnd_ypos = 0;
    int wnd_ypos2 = 0;

    int wnd_vis = 0;

    if ( robo_map.IsOpen() )
    {
        wnd_xpos = robo_map.x - (yw->_screenSize.x / 2);
        wnd_xpos2 = robo_map.w + wnd_xpos;
        wnd_ypos = robo_map.y - (yw->_screenSize.y / 2);
        wnd_ypos2 = robo_map.h + wnd_ypos;

        wnd_vis = 1;
    }

    int v51 = (yw->_screenSize.x / 2) * x;
    int v50 = (yw->_screenSize.y / 2) * y;

    int v49 = yw->_guiTiles[51]->map[valCH].w * wis->field_9E;
    // Keep every classic HUD status row on the same shared layout, but leave
    // a small breathing space between the three-letter label and the bar.
    const int labelBarGap = txt1.empty() ? 0 : std::max(2, yw->_guiTiles[51]->map[1].w / 2);


    FontUA::set_txtColor(cur, yw->_iniColors[65].r, yw->_iniColors[65].g, yw->_iniColors[65].b);

    v51 -= (wis->field_9A + wis->field_96 + labelBarGap + v49) / 2;

    if ( !txt1.empty() )
    {
        int v19 = (wis->field_96 / 2) + v51;
        int v20 = v50;

        if ( !wnd_vis || v19 <= wnd_xpos || v19 >= wnd_xpos2 || v20 <= wnd_ypos || v20 >= wnd_ypos2 )
        {
            FontUA::select_tileset(cur, 15);

            FontUA::set_center_xpos(cur, v51);
            FontUA::set_center_ypos(cur, v50 - (yw->_fontH / 2));

            FontUA::TextRelWidthItem(yw->_guiTiles[15], cur, txt1, 100, 4);
        }
    }

    FontUA::select_tileset(cur, 51);

    v51 += wis->field_96 + labelBarGap;

    FontUA::set_center_xpos(cur, v51);
    FontUA::set_center_ypos(cur, v50 - (yw->_guiTiles[51]->h / 2));

    int v29 = v51 + (yw->_guiTiles[51]->map[1].w / 2);
    int v30 = v50 + (yw->_guiTiles[51]->h >> 1);

    const bool isHpBar = valCH == 2 && valBG == 6;
    bool useProceduralStatusBar = false;
    if ( isHpBar )
    {
        const int barLeft = (yw->_screenSize.x / 2) + v51;
        const int barTop = (yw->_screenSize.y / 2) + v50 - (yw->_guiTiles[51]->h / 2);
        useProceduralStatusBar = yw_RenderProceduralHpBar(
                                     barLeft, barTop, wis->field_9E, value, maxval,
                                     yw->_guiTiles[51]->map[1].w, yw->_guiTiles[51]->h,
                                     255,
                                     (flag & 1) != 0, (flag & 2) != 0);
    }

    for (int i = 1; i <= wis->field_9E; i++)
    {
        int v35 = i * (maxval / wis->field_9E) - (maxval / wis->field_9E) / 2;

        if ( (!wnd_vis || v29 <= wnd_xpos || v29 >= wnd_xpos2 || v30 <= wnd_ypos || v30 >= wnd_ypos2) &&
             !useProceduralStatusBar )
        {
            if ( v35 > value )
            {
                if ( (flag & 2) == 0 )
                    FontUA::store_u8(cur, valBG);
                else
                    FontUA::add_xpos(cur, yw->_guiTiles[51]->map[1].w);
            }
            else
            {
                if ( (flag & 1) == 0 )
                    FontUA::store_u8(cur, valCH);
                else
                    FontUA::add_xpos(cur, yw->_guiTiles[51]->map[1].w);
            }
        }

        v29 += yw->_guiTiles[51]->map[1].w;
    }

    v51 += wis->field_9A + v49;

    if ( !txt2.empty() )
    {
        int v37 = v51 - (wis->field_9A / 2);
        int v38 = v50;

        if ( !wnd_vis || v37 <= wnd_xpos || v37 >= wnd_xpos2 || v38 <= wnd_ypos || v38 >= wnd_ypos2 )
        {
            FontUA::select_tileset(cur, 15);

            FontUA::set_center_xpos(cur, v51);
            FontUA::set_center_ypos(cur, v50 - (yw->_fontH / 2));

            FontUA::TextRelWidthItem(yw->_guiTiles[15], cur, txt2, 100, 8);
        }
    }
}

bool yw_IsMgunWeaponInfoAvailable(NC_STACK_ypabact *bact,
                                         World::TVhclProto *vhcl)
{
    if ( !bact || !vhcl || bact->_bact_type != BACT_TYPES_GUN )
        return false;

    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(bact);
    if ( !gun || gun->getGUN_fireType() != NC_STACK_ypagun::GUN_TYPE_PROTO )
        return false;

    // Opt-in only: old data without the two new authoring fields keeps the
    // previous gun_type = mg HUD behavior.
    return !vhcl->mgun_name.empty() || vhcl->mgun_wireframe;
}

void sb_0x4d7c08__sub0__sub0__sub0__sub0(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, float a4, float a5, int bldId, int a7)
{

    if ( !(a7 & 0x10) || (yw->_timeStamp - wis->field_76 - 200) / 180.0 > 0.0 )
    {
        if ( yw->_timeStamp / 200 & 1 )
        {
            std::string bldName = yw->ResolveGameplayBuildingName(bldId, yw->_isNetGame);

            yw_RenderInfoVehicleName(yw, wis, cur, bldName, a4, a5);
        }
    }
}

std::string yw_GemTargetName(NC_STACK_ypaworld *yw, const TGemNotificationEntry &entry)
{
    if ( entry.TargetKind == TGemNotificationEntry::TARGET_VEHICLE )
        return yw->ResolveGameplayVehicleName(entry.TargetProtoId);

    if ( entry.TargetKind == TGemNotificationEntry::TARGET_BUILDING )
        return yw->ResolveGameplayBuildingName(entry.TargetProtoId, yw->_isNetGame);

    if ( entry.RelatedVehicleId > 0 )
    {
        std::string vehicleName = yw->ResolveGameplayVehicleName(entry.RelatedVehicleId);
        if ( !vehicleName.empty() )
            return vehicleName;
    }

    return yw->ResolveGameplayWeaponName(entry.TargetProtoId);
}

std::string yw_GemCategoryLabel(const TGemNotificationEntry &entry)
{
    if ( entry.ChangeKind == TGemNotificationEntry::CHANGE_ENERGY )
    {
        if ( entry.TargetKind == TGemNotificationEntry::TARGET_WEAPON )
            return Locale::Text::Gem(Locale::GEMSTR_ATTACK_UPGRADE);

        return Locale::Text::Gem(Locale::GEMSTR_ENERGY_UPGRADE);
    }

    switch ( entry.ChangeKind )
    {
    case TGemNotificationEntry::CHANGE_SHIELD:
        return Locale::Text::Gem(Locale::GEMSTR_ARMOR_UPGRADE);

    case TGemNotificationEntry::CHANGE_RADAR:
    case TGemNotificationEntry::CHANGE_UNHIDE_RADAR:
        return Locale::Text::Gem(Locale::GEMSTR_RADAR_UPGRADE);

    case TGemNotificationEntry::CHANGE_NUM_WEAPONS:
        return Locale::Text::Gem(Locale::GEMSTR_WEAPON_COUNT_UPGRADE);

    case TGemNotificationEntry::CHANGE_SHOT_TIME:
    case TGemNotificationEntry::CHANGE_SHOT_TIME_USER:
        return Locale::Text::Gem(Locale::GEMSTR_FIRE_RATE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_HELI:
        return Locale::Text::Gem(Locale::GEMSTR_HELI_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_TANK:
        return Locale::Text::Gem(Locale::GEMSTR_TANK_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_FLYER:
        return Locale::Text::Gem(Locale::GEMSTR_FLYER_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_ROBO:
        return Locale::Text::Gem(Locale::GEMSTR_ROBO_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_PLANE:
        return Locale::Text::OpenUA(Locale::OUA_GEM_PLANE_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_GLIDER:
        return Locale::Text::OpenUA(Locale::OUA_GEM_GLIDER_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_ZEPPELIN:
        return Locale::Text::OpenUA(Locale::OUA_GEM_ZEPPELIN_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_UFO:
        return Locale::Text::OpenUA(Locale::OUA_GEM_UFO_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_CAR:
        return Locale::Text::OpenUA(Locale::OUA_GEM_CAR_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_GUN:
        return Locale::Text::OpenUA(Locale::OUA_GEM_GUN_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_ENERGY_CRUISER:
        return Locale::Text::OpenUA(Locale::OUA_GEM_CRUISER_DAMAGE_UPGRADE);

    case TGemNotificationEntry::CHANGE_MAX_ACTIVE_AT_ONCE:
        return Locale::Text::OpenUA(Locale::OUA_GEM_MAX_UNITS_UPGRADE);

    default:
        return std::string();
    }
}

std::string yw_GemTargetKey(const TGemNotificationEntry &entry)
{
    if ( entry.TargetKind == TGemNotificationEntry::TARGET_WEAPON && entry.RelatedVehicleId > 0 )
        return fmt::sprintf("v:%d", entry.RelatedVehicleId);

    return fmt::sprintf("%d:%d", entry.TargetKind, entry.TargetProtoId);
}

void yw_GemAppendSpacer(std::vector<GemNotificationLine> *lines)
{
    lines->emplace_back(std::string());
}

void yw_GemAppendUpgradeLine(std::vector<GemNotificationLine> *lines,
                                    const TGemNotificationEntry &entry)
{
    std::string category = yw_GemCategoryLabel(entry);
    if ( !category.empty() )
        lines->emplace_back(category);
}

void yw_GemAddUniqueFallbackTarget(NC_STACK_ypaworld *yw,
                                          std::vector<GemNotificationLine> *targets,
                                          const TGemNotificationEntry &entry)
{
    const std::string name = yw_GemTargetName(yw, entry);
    if ( name.empty() )
        return;

    const auto found = std::find_if(targets->begin(), targets->end(), [&name](const GemNotificationLine &target)
    {
        return target.Text == name;
    });
    if ( found == targets->end() )
        targets->emplace_back(name, yw->_iniColors[65]);
}

int yw_GemFindPrimaryVehicleForWeapon(NC_STACK_ypaworld *yw, int weaponId)
{
    if ( weaponId <= 0 )
        return 0;

    for (size_t vehicleId = 0; vehicleId < yw->_vhclProtos.size(); ++vehicleId)
    {
        if ( yw->_vhclProtos[vehicleId].weapon == weaponId )
            return (int)vehicleId;
    }

    return 0;
}

void yw_GemBuildFallbackLines(NC_STACK_ypaworld *yw, std::vector<GemNotificationLine> *lines)
{
    std::vector<GemNotificationLine> targets;
    TGemNotificationEntry target;

    if ( yw->_upgradeVehicleId > 0 && (size_t)yw->_upgradeVehicleId < yw->_vhclProtos.size() )
    {
        target.TargetKind = TGemNotificationEntry::TARGET_VEHICLE;
        target.TargetProtoId = yw->_upgradeVehicleId;
        yw_GemAddUniqueFallbackTarget(yw, &targets, target);
    }

    if ( yw->_upgradeWeaponId > 0 && (size_t)yw->_upgradeWeaponId < yw->_weaponProtos.size() )
    {
        int relatedVehicleId = yw_GemFindPrimaryVehicleForWeapon(yw, yw->_upgradeWeaponId);
        target.TargetKind = TGemNotificationEntry::TARGET_WEAPON;
        target.TargetProtoId = yw->_upgradeWeaponId;
        target.RelatedVehicleId = relatedVehicleId;
        yw_GemAddUniqueFallbackTarget(yw, &targets, target);
    }

    if ( yw->_upgradeBuildId > 0 && (size_t)yw->_upgradeBuildId < yw->_buildProtos.size() )
    {
        target.TargetKind = TGemNotificationEntry::TARGET_BUILDING;
        target.TargetProtoId = yw->_upgradeBuildId;
        target.RelatedVehicleId = 0;
        yw_GemAddUniqueFallbackTarget(yw, &targets, target);
    }

    if ( targets.empty() &&
         yw->_upgradeId >= 0 && (size_t)yw->_upgradeId < yw->_techUpgrades.size() &&
         !yw->_techUpgrades[yw->_upgradeId].MsgDefault.empty() )
        targets.emplace_back(yw->_techUpgrades[yw->_upgradeId].MsgDefault);

    lines->emplace_back(Locale::Text::Gem(Locale::GEMSTR_UPGRADE_UNLOCKED));

    if ( targets.empty() )
    {
        lines->emplace_back(Locale::Text::Gem(Locale::GEMSTR_UNLOCKED));
        return;
    }

    yw_GemAppendSpacer(lines);

    for (size_t i = 0; i < targets.size(); ++i)
    {
        if ( i != 0 )
            yw_GemAppendSpacer(lines);

        lines->push_back(targets[i]);
        lines->emplace_back(Locale::Text::Gem(Locale::GEMSTR_UNLOCKED));
    }
}

void yw_DrawGemPanelOutline(NC_STACK_ypaworld *yw, int halfWidth, int top, int bottom)
{
    SDL_Color outer = yw->GetColor(34);
    SDL_Color inner = yw->GetColor(25);

    GFX::Engine.raster_func217(outer);
    GFX::Engine.raster_func201(Common::Line(-halfWidth, top, halfWidth, top));
    GFX::Engine.raster_func201(Common::Line(halfWidth, top, halfWidth, bottom));
    GFX::Engine.raster_func201(Common::Line(halfWidth, bottom, -halfWidth, bottom));
    GFX::Engine.raster_func201(Common::Line(-halfWidth, bottom, -halfWidth, top));

    if ( halfWidth > 2 && bottom - top > 4 )
    {
        GFX::Engine.raster_func217(inner);
        GFX::Engine.raster_func201(Common::Line(-halfWidth + 2, top + 2, halfWidth - 2, top + 2));
        GFX::Engine.raster_func201(Common::Line(halfWidth - 2, top + 2, halfWidth - 2, bottom - 2));
        GFX::Engine.raster_func201(Common::Line(halfWidth - 2, bottom - 2, -halfWidth + 2, bottom - 2));
        GFX::Engine.raster_func201(Common::Line(-halfWidth + 2, bottom - 2, -halfWidth + 2, top + 2));
    }
}

void yw_BuildNewGemNotificationLines(NC_STACK_ypaworld *yw,
                                             std::vector<GemNotificationLine> *lines)
{
    lines->clear();

    std::vector<GemNotificationView> views;
    bool hasRealChange = false;

    for (const TGemNotificationEntry &entry : yw->_gemNotificationEntries)
    {
        GemNotificationView view;
        view.Entry = &entry;

        if ( entry.ChangeKind == TGemNotificationEntry::CHANGE_ENABLE )
        {
            if ( !entry.NewlyEnabled && !entry.AlreadyUnlocked )
                continue;

            if ( entry.NewlyEnabled )
                hasRealChange = true;
        }
        else
        {
            if ( entry.PreviousRawValue == entry.NewRawValue &&
                 entry.ChangeKind != TGemNotificationEntry::CHANGE_NUM_WEAPONS )
                continue;

            if ( !entry.AlreadyUnlocked )
                hasRealChange = true;
        }

        view.TargetName = yw_GemTargetName(yw, entry);
        if ( view.TargetName.empty() )
            continue;

        views.push_back(view);
    }

    if ( hasRealChange )
    {
        views.erase(std::remove_if(views.begin(), views.end(), [](const GemNotificationView &view)
        {
            return view.Entry->AlreadyUnlocked;
        }), views.end());
    }

    if ( views.empty() )
    {
        yw_GemBuildFallbackLines(yw, lines);
    }
    else
    {
        const bool singleNewUnlock = hasRealChange && views.size() == 1 &&
                                     views.front().Entry->ChangeKind == TGemNotificationEntry::CHANGE_ENABLE;
        const bool showTitle = hasRealChange && !singleNewUnlock;
        if ( showTitle )
        {
            std::string title = views.size() == 1
                              ? Locale::Text::Gem(Locale::GEMSTR_UPGRADE_UNLOCKED)
                              : Locale::Text::Gem(Locale::GEMSTR_UPGRADES_UNLOCKED);
            lines->emplace_back(title);
            yw_GemAppendSpacer(lines);
        }

        std::string previousTargetKey;
        for (size_t i = 0; i < views.size(); ++i)
        {
            const GemNotificationView &view = views[i];
            const TGemNotificationEntry &entry = *view.Entry;
            std::string targetKey = yw_GemTargetKey(entry);

            if ( targetKey != previousTargetKey )
            {
                if ( !previousTargetKey.empty() || !showTitle )
                    yw_GemAppendSpacer(lines);

                lines->emplace_back(view.TargetName, yw->_iniColors[65]);
                yw_GemAppendSpacer(lines);
                previousTargetKey = targetKey;
            }

            if ( entry.AlreadyUnlocked )
            {
                lines->emplace_back(Locale::Text::Gem(Locale::GEMSTR_ALREADY_UNLOCKED));
                continue;
            }

            if ( entry.ChangeKind == TGemNotificationEntry::CHANGE_ENABLE )
            {
                lines->emplace_back(Locale::Text::Gem(Locale::GEMSTR_UNLOCKED));
                continue;
            }

            yw_GemAppendUpgradeLine(lines, entry);
        }
    }

}

bool yw_RenderNewGemNotification(NC_STACK_ypaworld *yw)
{
    std::vector<GemNotificationLine> lines;
    yw_BuildNewGemNotificationLines(yw, &lines);

    const int lineHeight = std::max(yw->_fontH + 2, 12);
    int contentHeight = (int)lines.size() * lineHeight;

    TileMap *fontTiles = yw->_guiTiles[15];
    auto measureText = [fontTiles](const std::string &text)
    {
        int width = GFX::Engine.MeasureScreenTextWidth(text);
        return width > 0 ? width : fontTiles->GetWidth(text);
    };

    int widestLine = 0;
    for (const GemNotificationLine &line : lines)
        widestLine = std::max(widestLine, measureText(line.Text));

    int panelWidth = std::max(180, widestLine + 32);
    panelWidth = std::min(panelWidth, yw->_screenSize.x - 40);
    int halfWidth = panelWidth / 2;

    const int halfScreenHeight = yw->_screenSize.y / 2;
    const int topSafeMargin = std::max(64, yw->_fontH * 3);
    const int bottomSafeMargin = 24;
    const int topLimit = -halfScreenHeight + topSafeMargin;
    const int bottomLimit = halfScreenHeight - bottomSafeMargin;
    const int safeHeight = bottomLimit - topLimit;
    const int centerY = (topLimit + bottomLimit) / 2;
    int contentTop = centerY - contentHeight / 2;
    int contentBottom = contentTop + contentHeight;

    if ( contentHeight <= safeHeight && contentTop < topLimit )
    {
        contentTop = topLimit;
        contentBottom = contentTop + contentHeight;
    }
    if ( contentHeight <= safeHeight && contentBottom > bottomLimit )
    {
        contentBottom = bottomLimit;
        contentTop = contentBottom - contentHeight;
    }

    // Match the vanilla GEM/HUD upgrade blink cadence: the complete notification
    // (frame and text) is visible only during the active 200 ms phase.
    if ( ((yw->GetNewGemNotificationElapsedTime() / 200) & 1) == 0 )
        return true;

    const int panelTopPadding = std::max(yw->_fontH + 4, 16);
    yw_DrawGemPanelOutline(yw, halfWidth, contentTop - panelTopPadding, contentBottom + 8);

    CmdStream buffer;
    buffer.reserve(256 + lines.size() * 96);
    FontUA::select_tileset(&buffer, 15);

    for (size_t i = 0; i < lines.size(); ++i)
    {
        const GemNotificationLine &line = lines[i];
        if ( line.Text.empty() )
            continue;

        int y = contentTop + (int)i * lineHeight;
        const SDL_Color color = line.HasCustomColor ? line.TextColor : yw->_iniColors[65];
        FontUA::set_txtColor(&buffer, color.r, color.g, color.b);
        FontUA::set_xpos(&buffer, 0);
        FontUA::set_center_ypos(&buffer, y - yw->_fontH / 2);
        FontUA::FormateCenteredSkipableItem(fontTiles, &buffer, line.Text, yw->_screenSize.x);
    }

    FontUA::set_end(&buffer);
    SDL_Color uiAccentColor;
    GFX::Engine.ProcessDrawSeq(buffer, nullptr,
                              yw_GetFactionUiAccent(yw, &uiAccentColor));
    return true;
}

int sb_0x4d7c08__sub0__sub0__sub0(NC_STACK_ypaworld *yw)
{
    //Tech update draw
    GFX::Engine.raster_func210( Common::FRect(-1.0, -1.0, 1.0, 1.0) );

    sklt_wis *wis = &yw->_hud;

    CmdStream buf;
    buf.reserve(1024);

    if ( yw->_upgradeId == -1 )
        return 0;

    if ( System::IniConf::GameGemUnlockNewUI.Get<bool>() )
    {
        if ( yw->HasActiveNewGemNotification() && yw_RenderNewGemNotification(yw) )
            return 1;

        return 0;
    }

    const uint32_t legacyGemDuration = System::IniConf::GameGemUnlockDuration.WasSet
                                     ? yw->GetGemUnlockDuration()
                                     : 10000;
    if ( yw->_timeStamp - yw->_upgradeTimeStamp >= legacyGemDuration )
        return 0;

    if ( !yw->_upgradeVehicleId && !yw->_upgradeWeaponId && !yw->_upgradeBuildId )
        return 0;

    int a8 = 0;
    int v6 = yw->_upgradeVehicleId;
    int v14 = yw->_upgradeBuildId;

    if ( !v6 && yw->_upgradeWeaponId)
    {
        bool notOk = true;
        int i = 0;
        for (const World::TVhclProto &proto : yw->_vhclProtos)
        {
            if ( proto.weapon == yw->_upgradeWeaponId)
            {
                v6 = i;
                notOk = false;
                break;
            }
            i++;
        }

        if (notOk)
            return 0;
    }

    wis->field_86 = 1.0;

    int v10 = yw->_techUpgrades[ yw->_upgradeId ].Type;

    if ( v10 == World::UPGRADE_WEAPON )
        a8 |= 1;
    else if ( v10 == World::UPGRADE_ARMOR )
        a8 |= 4;
    else if ( v10 == World::UPGRADE_VEHICLE )
        a8 |= 8;

    if ( v6 )
        yw_RenderHUDInfo(yw, wis, &buf, 0.0, -0.5, 0, v6, a8);
    else if ( v14 )
        sb_0x4d7c08__sub0__sub0__sub0__sub0(yw, wis, &buf, 0.0, -0.5, v14, a8);

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);

    return 1;
}

void sb_0x4d7c08__sub0__sub4__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact)
{
    if ( yw_ShouldHideControlledUnitWorldUi(yw, bact) )
        return;

    if ( yw->_GameShell )
    {
        if ( yw->_isNetGame )
        {
            if ( !yw->_GameShell->netPlayers[bact->_owner].Name.empty() )
            {
                const uint8_t worldUiOpacity = yw_GetWorldUiOpacity(yw, bact->_position);
                if ( worldUiOpacity == 0 )
                    return;

                float v5 = bact->_position.x - yw->_viewerPosition.x;
                float v6 = bact->_position.y - yw->_viewerPosition.y;
                float v10 = bact->_position.z - yw->_viewerPosition.z;

                mat3x3 corrected = yw->_viewerRotation;
                GFX::Engine.matrixAspectCorrection(corrected, false);

                float v32 = corrected.m00 * v5 + corrected.m01 * v6 + corrected.m02 * v10;
                float v31 = corrected.m10 * v5 + corrected.m11 * v6 + corrected.m12 * v10;
                float v26 = corrected.m20 * v5 + corrected.m21 * v6 + corrected.m22 * v10;

                if ( v26 > 30.0 && v32 < v26 )
                {
                    if ( v32 > -v26 && v31 < v26 && v31 > -v26 )
                    {
                        int v30 = yw->_guiTiles[15]->h;

                        float projectedX = v32 / v26;
                        float projectedY = v31 / v26;
                        GFX::Engine.viewZoomCorrection(projectedX, projectedY);
                        if ( projectedX < -1.0f || projectedX > 1.0f ||
                             projectedY < -1.0f || projectedY > 1.0f )
                        {
                            return;
                        }

                        int v27_4 = ((yw->_screenSize.x / 2) * (projectedX + 1.0f));
                        int v15 = (yw->_screenSize.y / 2) * (projectedY + 1.0f);

                        int v28 = 96;

                        if ( v27_4 + 96 >= yw->_screenSize.x )
                            v28 = yw->_screenSize.x - v27_4 - 1;

                        if ( v28 > 0 && v27_4 >= 0 )
                        {
                            if ( v28 + v27_4 <  yw->_screenSize.x && v15 >= 0 )
                            {
                                if ( v30 + v15 < yw->_screenSize.y )
                                {
                                    FontUA::select_tileset(cur, 15);
                                    FontUA::set_center_xpos(cur, v27_4 - (yw->_screenSize.x / 2) );
                                    FontUA::set_center_ypos(cur, v15 - (yw->_screenSize.y / 2) );

                                    FontUA::set_txtColor(cur, yw->_iniColors[ bact->_owner ].r, yw->_iniColors[ bact->_owner ].g, yw->_iniColors[ bact->_owner ].b);
                                    FontUA::set_opacity(cur, worldUiOpacity);

                                    FontUA::FormateClippedText(yw->_guiTiles[15], cur,  yw->_GameShell->netPlayers[bact->_owner].Name, v28, 32);
                                    FontUA::set_opacity(cur, 255);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void sb_0x4d7c08__sub0__sub4(NC_STACK_ypaworld *yw)
{
    CmdStream buf;
    buf.reserve(1024);

    if ( yw->_userUnit->_status == BACT_STATUS_DEAD )
    {
        std::string msg;

        if ( !(yw->_userRobo->_status_flg & BACT_STFLAG_CLEAN) )
        {
            if ( yw->_userRobo->_status == BACT_STATUS_DEAD )
                msg = Locale::Text::Common(Locale::CMN_HSDESTR);
            else
                msg = Locale::Text::Common(Locale::CMN_UNITDESTR);
        }

        if ( !msg.empty() && yw->_timeStamp / 500 & 1 )
        {
            FontUA::select_tileset(&buf, 15);
            FontUA::set_xpos(&buf, 0);
            FontUA::set_center_ypos(&buf, -(yw->_fontH / 2));

            FontUA::set_txtColor(&buf, 255, 255, 255);

            FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf, msg, yw->_screenSize.x);
        }
    }
    else if ( yw->_userUnit != yw->_userRobo && yw->_hud.field_0 && !yw->IsSpectatorControlled() )
    {
        yw_RenderHUDRadare(yw);

        if ( yw->_userUnit->_energy >= yw->_userUnit->_energy_max )
        {
            yw->_hud.field_82 = 3000;
        }
        else
        {
            // Keep the original health-driven cockpit colour cycle unchanged.
            // The procedural HP bar itself does not pulse or blink.
            yw->_hud.field_82 = 3000 * yw->_userUnit->_energy / yw->_userUnit->_energy_max;

            if ( yw->_hud.field_82 < 200 )
                yw->_hud.field_82 = 200;
        }

        yw->_hud.field_86 += (float)yw->_frameTime / (float)yw->_hud.field_82;

        if ( yw->_hud.field_86 >= 1.0 )
            yw->_hud.field_86 = 0;

        yw_RenderHUDVectorGFX(yw, &buf);
    }

    if ( yw->_guiActFlags & 0x20 )
    {
        if ( !yw->_bactOnMouse->ShouldHideFromStrategicUI() )
        {
            yw_RenderCursorOverUnit(yw, yw->_bactOnMouse);

            if ( !yw->IsSpectatorControlled() )
                yw_RenderUnitLifeBar(yw, &buf, yw->_bactOnMouse);
        }
    }

    std::vector<NC_STACK_ypabact *> damageHoverTargets = yw->GetUserDamageHoverTargets();
    if ( !yw->IsSpectatorControlled() )
    {
        for (NC_STACK_ypabact *damageHoverTarget : damageHoverTargets)
        {
            bool sameAsMouseHover = (yw->_guiActFlags & 0x20) && yw->_bactOnMouse == damageHoverTarget;
            if ( !sameAsMouseHover )
                yw_RenderCursorOverUnit(yw, damageHoverTarget);

            if ( !sameAsMouseHover && yw->_guiVisor.field_18 != damageHoverTarget )
                yw_RenderUnitLifeBar(yw, &buf, damageHoverTarget);
        }
    }

    if ( !yw->IsSpectatorControlled() && yw->_guiVisor.field_18 && !yw->_guiVisor.field_18->ShouldHideFromStrategicUI() )
        yw_RenderUnitLifeBar(yw, &buf, yw->_guiVisor.field_18);

    if ( yw->IsUfoSpyUiControlContext() )
    {
        // Arrows are the lightweight default UFO sensor layer and stay visible
        // independently from the optional detailed HP/status toggle.
        for ( NC_STACK_ypabact *unit : yw->_unitsList )
            yw_RenderUfoSpyWorldArrows(yw, unit, damageHoverTargets);
    }

    if ( yw_IsUfoSpyHudActive(yw) )
    {
        for ( NC_STACK_ypabact *unit : yw->_unitsList )
            yw_RenderUfoSpyWorldStatus(yw, &buf, unit, damageHoverTargets);
    }

    yw_RenderOverlayCursors(yw, &buf, damageHoverTargets);
    yw_RenderPlasmaCurrencyPopups(yw, &buf);

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);

    yw->_guiVisor.field_18 = nullptr;
}

void sub_4E3B80(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    float v15 = yw->_hud.field_86 * 2.0 - 1.0;

    float v17 = fabs( v15 - sqrt(POW2(x1) + POW2(y1)) );
    float v16 = fabs( v15 - sqrt(POW2(x2) + POW2(y2)) );

    if ( v17 > 1.0)
        v17 = 1.0;
    else if ( v17 < 0.0)
        v17 = 0.0;

    if ( v16 > 1.0)
        v16 = 1.0;
    else if ( v16 < 0.0)
        v16 = 0.0;

    int v9 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * v17 + yw->_hud.cl1_g;
    int v10 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * v17 + yw->_hud.cl1_r;
    int v11 =  (yw->_hud.cl2_b - yw->_hud.cl1_b) * v17 + yw->_hud.cl1_b;
    *out1 = GFX::Engine.Color(v10, v9, v11);

    int v12 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * v16 + yw->_hud.cl1_g;
    int v13 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * v16 + yw->_hud.cl1_r;
    int v14 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * v16 + yw->_hud.cl1_b;
    *out2 = GFX::Engine.Color(v13, v12, v14);
}

void sub_4E3D98(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    int v7 = yw->_timeStamp % 300;
    float v9 = v7 / 30000.0;

    sklt_wis *v8 = &yw->_hud;

    int v10 = (v8->cl2_g - v8->cl1_g) * v9 + v8->cl1_g;
    int v11 = (v8->cl2_r - v8->cl1_r) * v9 + v8->cl1_r;
    int v12 = (v8->cl2_b - v8->cl1_b) * v9 + v8->cl1_b;
    *out1 = GFX::Engine.Color(v11, v10, v12);

    int v13 = (v8->cl2_r - v8->cl1_r) * v9 + v8->cl1_r;
    int v16 = (v8->cl2_b - v8->cl1_b) * v9 + v8->cl1_b;
    int v15 = (v8->cl2_g - v8->cl1_g) * v9 + v8->cl1_g;
    *out2 = GFX::Engine.Color(v13, v15, v16);
}

void sb_0x4d7c08__sub0__sub4__sub2__sub0(NC_STACK_ypaworld *yw)
{
    // Render units on radare

    CmdStream buf;
    buf.reserve(8192);

    FontUA::select_tileset(&buf, 61);

    float v6 = yw->_screenSize.x / 2;
    float v7 = yw->_screenSize.y / 2;

    robo_map.field_1F8 = dround((robo_map.field_25C - robo_map.field_254) * v6);
    robo_map.field_1FC = dround((robo_map.field_260 - robo_map.field_258) * v7);

    robo_map.field_1F0 = dround(robo_map.field_1D8 / robo_map.field_1E0) - (robo_map.field_1F8 / 2);
    robo_map.field_1F4 = -dround(robo_map.field_1DC / robo_map.field_1E4) - (robo_map.field_1FC / 2);

    robo_map.field_200 = dround(v6 * robo_map.field_254);
    robo_map.field_204 = dround(v7 * robo_map.field_258);

    Common::Rect drect;
    drect.left = robo_map.field_200;
    drect.top = robo_map.field_204;
    drect.right = robo_map.field_1F8 + robo_map.field_200 - 1;
    drect.bottom = robo_map.field_1FC + robo_map.field_204 - 1;

    GFX::Engine.raster_func211(drect);
    yw->RenderArtilleryShellMapMarkers();
    yw->RenderLaserMapBeams(61);

    int v14 = dround(robo_map.field_1F0 * robo_map.field_1E0) / World::CVSectorLength;
    int v29 = dround(robo_map.field_1F4 * robo_map.field_1E4) / World::CVSectorLength;
    int v28 = dround( (robo_map.field_1F8 + robo_map.field_1F0) * robo_map.field_1E0 ) / World::CVSectorLength;
    int v30 = dround( (robo_map.field_1FC + robo_map.field_1F4) * robo_map.field_1E4 ) / World::CVSectorLength;

    if ( v14 < 1 )
        v14 = 1;

    if ( v29 < 1 )
        v29 = 1;

    if ( v28 >= yw->_mapSize.x )
        v28 = yw->_mapSize.x - 1;

    if ( v30 >= yw->_mapSize.y )
        v30 = yw->_mapSize.y - 1;

    int v38 = 0;

    for (int i = v29; i <= v30; i++ )
    {
        for (int j = v14; j <= v28; j++ )
        {
            if ( v38 < 512 ) // Units maximum?
            {
                cellArea &v17 = yw->_cells(j, i);

                if ( robo_map.MapViewMask & v17.view_mask )
                {
                    for( NC_STACK_ypabact* &bact : v17.unitsList )
                    {
                        if ( bact->ShouldHideFromStrategicUI() )
                            continue;

                        int v19 = 0;
                        if ( robo_map.IsOpen() )
                        {
                            Common::PointRect r { robo_map.x - (yw->_screenSize.x / 2)
                                                , robo_map.y - (yw->_screenSize.y / 2)
                                                , robo_map.w
                                                , robo_map.h };

                            if ( r.IsIn( sub_4F681C( bact->_position.XZ() ) ) )
                                v19 = 1;
                        }

                        if ( bact->_bact_type == BACT_TYPES_ROBO )
                        {
                            if ( !v19 )
                            {
                                FontUA::select_tileset(&buf, 1);

                                sub_4F6DFC(yw, &buf, yw->_guiTiles[1]->h, yw->_guiTiles[1]->map[24].w, bact, 0);

                                FontUA::select_tileset(&buf, 61);

                                v38++;
                            }
                        }
                        else if ( bact->_bact_type == BACT_TYPES_GUN )
                        {
                            NC_STACK_ypagun *gun = (NC_STACK_ypagun *)bact;

                            if ( !gun->IsRoboGun() && !v19 )
                            {
                                sub_4F6DFC(yw, &buf, 7, 7, bact, 0);

                                v38++;
                            }
                        }
                        else if ( !v19 )
                        {
                            sub_4F6DFC(yw, &buf, 7, 7, bact, 0);

                            v38++;
                        }
                    }
                }
            }
        }
    }

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);

    // OpenNeoUA custom: the 3D world already shows the artillery shell cooldown/readiness bar
    // above the HP bar. Mirror that feedback on the small gameplay radar too,
    // drawing it after unit icons so it stays visible.
    GFX::Engine.raster_func211(drect);
    yw_RenderArtilleryShellCooldownRadarBars(yw, v14, v29, v28, v30);

    // Draw custom annotations last so they remain readable above unit icons and
    // cooldown bars while the player is controlling a unit.
    yw_RenderCustomHudRadarMarkers(yw);
}

int sub_47F360(const void *a1, const void *a2)
{
    return (((const aab *)a2)->b) - (((const aab *)a1)->b);
}

void sb_0x4d7c08__sub0__sub2__sub1(NC_STACK_ypaworld *yw, CmdStream *cur, const std::array<World::TPlayerStatus, World::CVFractionsCount> &statuses, int a3)
{
    if ( yw->_isNetGame )
    {
        aab v25[8];

        int nums = 0;

        for (int i = 0; i < 8; i++)
        {
            if ( (1 << i) & yw->_levelInfo.OwnerMask )
            {
                v25[nums].a = i;
                v25[nums].b = statuses[i].Score;
                nums++;
            }
        }

        qsort(v25, nums, sizeof(aab), sub_47F360);

        for (int i = 0; i < nums; i++)
        {
            int clrid;

            FontUA::ColumnItem a4a[2];

            switch ( v25[i].a )
            {
            case 1:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_RESIST);
                clrid = 1;
                break;

            case 2:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_SULG);
                clrid = 2;
                break;

            case 3:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_MYKO);
                clrid = 3;
                break;

            case 4:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_TAER);
                clrid = 4;
                break;

            case 5:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_BLKSCT);
                clrid = 5;
                break;

            case 6:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_GHOR);
                clrid = 6;
                break;

            default:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_NEUTRAL);
                clrid = 7;
                break;
            }

            FontUA::set_txtColor(cur, yw->_iniColors[clrid].r, yw->_iniColors[clrid].g, yw->_iniColors[clrid].b);

            a4a[0].spaceChar = 32;
            a4a[0].prefixChar = 0;
            a4a[0].postfixChar = 0;
            a4a[0].fontID = 15;
            a4a[0].flags = 36;
            a4a[0].width = a3 * 0.5;

            a4a[1].txt = fmt::sprintf("%d", statuses[ v25[i].a ].Score);
            a4a[1].width = a3 * 0.5;
            a4a[1].fontID = 15;
            a4a[1].spaceChar = 32;
            a4a[1].flags = 36;
            a4a[1].prefixChar = 0;
            a4a[1].postfixChar = 0;

            FormateColumnItem(yw, cur, 2, a4a);

            FontUA::next_line(cur);
        }

        FontUA::next_line(cur);
    }
}

void sb_0x4d7c08__sub0__sub2__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int a3)
{
    if ( yw->_isNetGame )
    {
        if ( yw->_countUnitsPerOwner[ yw->_userRobo->_owner ] <= yw->_levelUnitLimit )
            FontUA::set_txtColor(cur, yw->_iniColors[63].r, yw->_iniColors[63].g, yw->_iniColors[63].b);
        else
            FontUA::set_txtColor(cur, yw->_iniColors[6].r, yw->_iniColors[6].g, yw->_iniColors[6].b);

        FontUA::ColumnItem a4[2];

        a4[0].txt = Locale::Text::Advanced(Locale::ADV_UNITS);
        a4[0].width = a3 * 0.5;
        a4[0].spaceChar = 32;
        a4[0].fontID = 15;
        a4[0].prefixChar = 0;
        a4[0].postfixChar = 0;
        a4[0].flags = 36;

        a4[1].txt = fmt::sprintf("%d / %d", yw->_countUnitsPerOwner[ yw->_userRobo->_owner ], yw->_levelUnitLimit);
        a4[1].width = a3 * 0.5;
        a4[1].fontID = 15;
        a4[1].postfixChar = 0;
        a4[1].spaceChar = 32;
        a4[1].flags = 36;
        a4[1].prefixChar = 0;

        FormateColumnItem(yw, cur, 2, a4);

        FontUA::next_line(cur);
    }
}

void sb_0x4d7c08__sub0__sub2(NC_STACK_ypaworld *yw)
{
    CmdStream buf;
    buf.reserve(1024);

    int v25 = 2 * yw->_screenSize.x / 3;
    int v2 = yw->_screenSize.x - v25;
    const int startY = up_panel.field_1CC + (yw->_fontH / 2);

    FontUA::select_tileset(&buf, 15);
    FontUA::set_xpos(&buf, v25);
    FontUA::set_ypos(&buf, startY);

    sb_0x4d7c08__sub0__sub2__sub0(yw, &buf, v2);
    sb_0x4d7c08__sub0__sub2__sub1(yw, &buf, yw->_gameplayStats, v2);

    FontUA::set_end(&buf);

    SDL_Color uiAccentColor;
    GFX::Engine.ProcessDrawSeq(buf, nullptr,
                              yw_GetFactionUiAccent(yw, &uiAccentColor));

    // Super-item text carries gameplay ownership information. Render it outside
    // the player's UI-accent remap so the displayed colour stays the real owner colour.
    CmdStream superItemBuf;
    superItemBuf.reserve(512);

    int superItemY = startY;
    if ( yw->_isNetGame )
    {
        int ownerRows = 0;
        for (int i = 0; i < World::CVFractionsCount; ++i)
        {
            if ( (1 << i) & yw->_levelInfo.OwnerMask )
                ++ownerRows;
        }

        // Unit count row + faction score rows + the existing blank separator.
        superItemY += (ownerRows + 2) * yw->_guiTiles[15]->h;
    }

    FontUA::select_tileset(&superItemBuf, 15);
    FontUA::set_xpos(&superItemBuf, v25);
    FontUA::set_ypos(&superItemBuf, superItemY);

    for (const TMapSuperItem &sitem : yw->_levelInfo.SuperItems)
    {
        if ( sitem.Type != 0 )
        {
            std::string timeStr;
            std::string typeStr = yw->GetSuperItemDisplayName(sitem);

            int v23 = 0;

            if ( sitem.State == TMapSuperItem::STATE_ACTIVE || sitem.State == TMapSuperItem::STATE_STOPPED )
            {
                int v10 = (sitem.CountDown + 1023) / 1024;

                if ( v10 < 0 )
                    v10 = 0;

                timeStr = fmt::sprintf("%s: %02d:%02d", typeStr, v10 / 60 % 60, v10 % 60);

                v23 = 1;
            }
            else if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
            {
                timeStr = fmt::sprintf("%s: %s", typeStr, Locale::Text::Advanced(Locale::ADV_TRIGGERED));

                v23 = 1;
            }

            if ( v23 )
            {
                const uint32_t owner = sitem.ActivateOwner <= World::OWNER_7
                                     ? sitem.ActivateOwner
                                     : World::OWNER_0;

                FontUA::set_xpos(&superItemBuf, v25);
                FontUA::set_txtColor(&superItemBuf,
                                     yw->_iniColors[owner].r,
                                     yw->_iniColors[owner].g,
                                     yw->_iniColors[owner].b);

                FontUA::FormateClippedText(yw->_guiTiles[15], &superItemBuf, timeStr.c_str(),
                                           yw->_screenSize.x - v25, 32);

                FontUA::next_line(&superItemBuf);
            }
        }
    }

    FontUA::set_end(&superItemBuf);
    GFX::Engine.ProcessDrawSeq(superItemBuf);
}

void ypaworld_func2__sub0__sub1(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact1, NC_STACK_ypabact *bact2)
{
    if ( bact2->_bact_type == BACT_TYPES_ROBO )
    {
        if ( yw->_roboMapStatus.Valid )
        {
            if ( yw->_roboMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
            else
                yw->GuiWinClose( &robo_map );

            robo_map.x = yw->_roboMapStatus.Rect.x;
            robo_map.y = yw->_roboMapStatus.Rect.y;
            robo_map.w = yw->_roboMapStatus.Rect.w;
            robo_map.h = yw->_roboMapStatus.Rect.h;
            robo_map.field_1EC = yw->_roboMapStatus.Data[0];
            robo_map.field_1ED = yw->_roboMapStatus.Data[1];
            robo_map.field_208 = yw->_roboMapStatus.Data[3];
            robo_map.field_20A = yw->_roboMapStatus.Data[4];
            robo_map.field_20C = yw->_roboMapStatus.Data[5];
            robo_map.field_20E = yw->_roboMapStatus.Data[6];

            sub_4C1970(yw, 0);
        }

        if ( yw->_roboFinderStatus.Valid )
        {
            if ( yw->_roboFinderStatus.IsOpen && !yw->IsSpectatorControlled() )
            {
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
            else
                yw->GuiWinClose( &squadron_manager );

            yw_ApplySquadronManagerSavedRect(yw, yw->_roboFinderStatus.Rect);
        }
    }
    else if ( bact1->_bact_type == BACT_TYPES_ROBO )
    {
        if ( yw->_vhclMapStatus.Valid )
        {
            if ( yw->_vhclMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
            else
                yw->GuiWinClose( &robo_map );

            robo_map.x = yw->_vhclMapStatus.Rect.x;
            robo_map.y = yw->_vhclMapStatus.Rect.y;
            robo_map.w = yw->_vhclMapStatus.Rect.w;
            robo_map.h = yw->_vhclMapStatus.Rect.h;
            robo_map.field_1EC = yw->_vhclMapStatus.Data[0];
            robo_map.field_1ED = yw->_vhclMapStatus.Data[1];
            robo_map.field_208 = yw->_vhclMapStatus.Data[3];
            robo_map.field_20A = yw->_vhclMapStatus.Data[4];
            robo_map.field_20C = yw->_vhclMapStatus.Data[5];
            robo_map.field_20E = yw->_vhclMapStatus.Data[6];

            sub_4C1970(yw, 0);
        }

        if ( yw->_vhclFinderStatus.Valid )
        {
            if ( yw->_vhclFinderStatus.IsOpen && !yw->IsSpectatorControlled() )
            {
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
            else
                yw->GuiWinClose( &squadron_manager );

            yw_ApplySquadronManagerSavedRect(yw, yw->_vhclFinderStatus.Rect);
        }
    }
    else if ( bact1->_status == BACT_STATUS_DEAD )
    {
        if ( yw->_vhclMapStatus.Valid )
        {
            if ( yw->_vhclMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
        }

        if ( yw->_vhclFinderStatus.Valid )
        {
            if ( yw->_vhclFinderStatus.IsOpen && !yw->IsSpectatorControlled() )
            {
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
        }

        if ( bact2->_bact_type != BACT_TYPES_ROBO )
            yw->_invulnerable = 1;
    }

    if ( bact2->_bact_type != BACT_TYPES_ROBO )
        robo_map.field_1ED = 1;
}

int NC_STACK_ypaworld::sb_0x4d3d44(TClickBoxInf *winp)
{
    if ( _guiActFlags & 8 || bzda.field_8EC == -1 )
        return 4;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);

    int a4 = robo->getROBO_battVehicle();

    if ( _updateMessage.energy > a4 )
        return 2;

    const int vehicleId = bzda.field_2DC[bzda.field_8EC];
    if ( _userRobo && IsVehicleProductionLimitReached(_userRobo->_owner, vehicleId) )
        return 3;

    if ( !yw_MouseFindCreationPoint(winp) )
        return 1;

    if ( _isNetGame && !_levelUnitLimitType && _levelUnitLimit <= _countUnitsPerOwner[_userRobo->_owner] )
        return 3;

    return 0;
}

int NC_STACK_ypaworld::UserActBuildCheck()
{
    if ( !(_guiActFlags & 0x10) || bzda.field_8F4 == -1 )
        return 1; // NO with no tip

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);

    int a4 = robo->getROBO_battVehicle();

    if ( _updateMessage.energy > a4 )
        return 6; // TIP_NOENERGY

    if ( _cellOnMouse->owner && _cellOnMouse->owner != _userRobo->_owner )
        return 2; // TIP_NOTOWN

    Common::Point cellDist = _cellOnMouse->CellId.AbsDistance( _userRobo->_cellId );

    if ( cellDist.IsNull() )
        return 3; // TIP_BUILD_TOOCLOSE

    if ( cellDist.x > 1 || cellDist.y > 1 )
        return 4; // TIP_BUILD_TOOFAR

    if ( _cellOnMouse->PurposeType == cellArea::PT_CONSTRUCTING ||
         (!_allowMultiBuildLevel && IsAnyBuildingProcess(_playerOwner)) )
        return 5; // TIP_BUILD_INPROC

    if ( _cellOnMouse->PurposeType == cellArea::PT_TECHUPGRADE ||
         _cellOnMouse->PurposeType == cellArea::PT_GATECLOSED ||
         _cellOnMouse->PurposeType == cellArea::PT_GATEOPENED ||
         _cellOnMouse->PurposeType == cellArea::PT_STOUDSON ||
         (_cellOnMouse->PurposeType == cellArea::PT_TECHDEACTIVE && _isNetGame) )
        return 1; // NO with no tip

    return 0; // Can build
}

int NC_STACK_ypaworld::ypaworld_func64__sub21__sub3()
{
    if ( !(_guiActFlags & 0x10) )
        return 6;

    if ( !_cellOnMouse->IsCanSee(_userRobo->_owner) )
        return 3;

    if ( _cellOnMouse->PurposeType == cellArea::PT_CONSTRUCTING )
        return 6;

    float v5 = _cellMouseIsectPos.x - _userRobo->_position.x;
    float v6 = _cellMouseIsectPos.y - _userRobo->_position.y;
    float v8 = _cellMouseIsectPos.z - _userRobo->_position.z;

    float v15 = sqrt(POW2(v5) + POW2(v6) + POW2(v8));

    if ( v15 < 100.0 )
        return 4;

    int v18 = 0;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);

    bool playerRoboAIBehavior = robo && robo->IsPlayerRoboAIBehavior();

    if ( playerRoboAIBehavior ||
         (_cellOnMouse->PurposeType != cellArea::PT_GATEOPENED && (_cellOnMouse->PurposeType != cellArea::PT_POWERSTATION || _cellOnMouse->owner != _userRobo->_owner)) )
    {
        v18 = (POW2(v15) / 230.4);
    }

    if ( IsDebugHostStationCheatEnabled() )
        v18 = 0;

    int a4 = robo->getROBO_battBeam();

    _updateMessage.energy = v18;

    int mobileEnergyCost = v18;
    if ( playerRoboAIBehavior )
        mobileEnergyCost = NC_STACK_yparobo::ScalePlayerMobileMoveEnergyCost(v18);

    if ( mobileEnergyCost > a4 )
        return 1;

    for ( NC_STACK_ypabact* &v11 : _cellOnMouse->unitsList )
    {
        if ( v11->_bact_type == BACT_TYPES_ROBO && v11->_status != BACT_STATUS_DEAD && v11->_owner != _userRobo->_owner )
            return 5;
    }

    float v20 = 100000.0;
    float v19 = -100000.0;

    ypaworld_arg136 arg136;
    arg136.vect.x = 0;
    arg136.vect.y = 50000.0;
    arg136.vect.z = 0;
    arg136.flags = 0;

    for (int j = -3; j <= 3; j++)
    {
        for (int i = -3; i <= 3; i++)
        {
            arg136.stPos.y = -25000.0;
            arg136.stPos.x = _cellMouseIsectPos.x - j * 57.14285714285715;
            arg136.stPos.z = _cellMouseIsectPos.z - i * 57.14285714285715;

            ypaworld_func136(&arg136);

            if ( arg136.isect )
            {
                if ( arg136.isectPos.y < v20 )
                    v20 = arg136.isectPos.y;

                if ( arg136.isectPos.y > v19 )
                    v19 = arg136.isectPos.y;
            }
        }
    }

    if ( abs(v19 - v20) > 800 )
        return 2;

    if ( _cellMouseIsectPos.x < 1500.0 || _cellMouseIsectPos.z > -1500.0 || _cellMouseIsectPos.x > _mapLength.x - 1500.0 || _cellMouseIsectPos.z < 1500.0 - _mapLength.y )
        return 2;

    _cellMouseIsectPos.y = v20 - _userRobo->_height;
    return 0;
}

int NC_STACK_ypaworld::ypaworld_func64__sub21__sub4(TInputState *arg, int a3)
{
    const bool waypointInputHeld =
        arg->Buttons.Is(4) && !IsPlayerSprintInputHeld();

    if ( !_makingWaypointsMode && waypointInputHeld )
    {
        _makingWaypointsMode = true;
        _waypointCount = 0;
        return a3;
    }

    if ( _makingWaypointsMode && !waypointInputHeld )
    {
        _makingWaypointsMode = false;
        return a3;
    }

    if ( _makingWaypointsMode   &&   a3 == World::DOACTION_2 &&   arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN )
    {
        if ( _waypointCount )
        {
            if ( (_firstWaypointPosition.XZ() - _cellMouseIsectPos.XZ()).length() < 100.0 )
            {
                _waypointCount++;
                return World::DOACTION_WP_CYCLE;
            }

            _waypointCount++;

            return World::DOACTION_WP_ADD;
        }
        else
        {
            _firstWaypointPosition = _cellMouseIsectPos;

            _waypointCount++;

            return World::DOACTION_WP_BEGIN;
        }
    }

    return a3;
}

bool NC_STACK_ypaworld::ypaworld_func64__sub21__sub6(TClickBoxInf *winp)
{
    if ( winp->flag & TClickBoxInf::FLAG_DBL_CLICK
            && _bactPrevClicked
            && Common::ABS(winp->move.ScreenPos.x - _prevMousePos.x) < 4
            && Common::ABS(winp->move.ScreenPos.y - _prevMousePos.y) < 4 )
    {
        _prevMousePos = Common::Point();

        _bactOnMouse = _bactPrevClicked;
        _bactPrevClicked = nullptr;

        _guiActFlags |= 0x20;

        return true;
    }
    else
    {
        if ( (winp->flag & TClickBoxInf::FLAG_LM_DOWN)   &&   _guiActFlags & 0x20   &&   _userRobo->_owner == _bactOnMouse->_owner )
        {
            _bactPrevClicked = _bactOnMouse;
            _prevMousePos = winp->move.ScreenPos;
        }
        else if ( winp->flag & TClickBoxInf::FLAG_LM_DOWN )
        {
            if ( !(_guiActFlags & 0x20) )
            {
                _bactPrevClicked = nullptr;
                _prevMousePos = Common::Point();
            }
        }
    }
    return false;
}

void NC_STACK_ypaworld::ypaworld_func64__sub21__sub1__sub0(TInputState *arg)
{
    _guiActFlags = 0;

    TClickBoxInf *winp = &arg->ClickInf;

    if ( arg->ClickInf.flag & TClickBoxInf::FLAG_OK )
    {
        if ( winp->selected_btn )
        {
            _guiActFlags |= 4;

            if ( winp->selected_btn == &robo_map )
            {
                if ( winp->selected_btnID == 17 )
                    _guiActFlags |= 8;
            }
            else if ( winp->selected_btn == &squadron_manager )
            {
                if ( winp->selected_btnID >= 8 && (int32_t)(squadron_manager.firstShownEntries + winp->selected_btnID - 8) < squadron_manager.numEntries )
                    _guiActFlags |= 0x40;
            }
        }

        if ( bzda.field_1D0 == 8 )
            _guiActFlags |= 2;
        else if ( bzda.field_1D0 == 1 || bzda.field_1D0 == 2 || bzda.field_1D0 == 4 || bzda.field_1D0 == 16 || bzda.field_1D0 == 32 )
            _guiActFlags |= 0x83;
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub21__sub1__sub3__sub0(TClickBoxInf *winp)
{
    float v3 = (float)(winp->move.ScreenPos.x - (_screenSize.x / 2)) / (float)(_screenSize.x / 2);
    float v4 = (float)(winp->move.ScreenPos.y - (_screenSize.y / 2)) / (float)(_screenSize.y / 2);

    GFX::Engine.viewZoomCorrection(v3, v4, true);

    mat3x3 corrected = _viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, true);

    float v16 = corrected.m00 * v3 + corrected.m10 * v4 + corrected.m20;
    float v17 = corrected.m01 * v3 + corrected.m11 * v4 + corrected.m21;
    float v18 = corrected.m02 * v3 + corrected.m12 * v4 + corrected.m22;

    float v14 = sqrt(POW2(v16) + POW2(v17) + POW2(v18));

    _mouseVector.x = v16 / v14;
    _mouseVector.y = v17 / v14;
    _mouseVector.z = v18 / v14;
}

void NC_STACK_ypaworld::ypaworld_func64__sub21__sub7()
{
    _updateMessage.energy = 0;

    if ( bzda.field_1D0 == 2 || bzda.field_1D0 == 4 )
    {
        if ( bzda.field_8EC != -1 )
        {
            _updateMessage.energy = dround(sub_4498F4() * (2 * _vhclProtos[bzda.field_2DC[bzda.field_8EC]].GetProductionCost()));
        }
    }
    else if ( bzda.field_1D0 == 16 && bzda.field_8F4 != -1 )
    {
        _updateMessage.energy = dround(sub_4498F4() * _buildProtos[bzda.field_3DC[bzda.field_8F4]].GetProductionCost());
    }

    // New Debug Shift+F11 keeps the normal authored prices visible in the Genesis
    // UI, but the actual player Host Station command is free. Zero the shared
    // action message here so affordability checks and the runtime use the same
    // authoritative cost instead of maintaining a parallel cheat path.
    if ( IsDebugHostStationCheatEnabled() )
        _updateMessage.energy = 0;
}

int sub_4D3C80(NC_STACK_ypaworld *yw)
{
    if ( !yw->_bactOnMouse )
        return 0;

    if ( yw->_bactOnMouse == yw->_userRobo )
        return 0;

    NC_STACK_ypabact *a4 = yw->_bactOnMouse;

    if ( !yw->_bactOnMouse || !yw->_bactOnMouse->_parent )
        return 0;

    if ( yw->_bactOnMouse->_parent != yw->_bactOnMouse->_host_station )
        a4 = yw->_bactOnMouse->_parent;

    if ( !a4 )
        return 0;

    yw->_bactOnMouse = a4;
    return 1;
}

void NC_STACK_ypaworld::ypaworld_func64__sub21__sub5(int arg)
{
    if ( IsSpectatorControlled() && arg != World::DOACTION_0 )
        return;

    switch ( arg )
    {
    case World::DOACTION_8:
        if ( _bactOnMouse->_bact_type != BACT_TYPES_GUN && _bactOnMouse != _userRobo )
        {
            // Resolve the clicked unit's squad commander without replacing the
            // actual world-space hover target. sub_4D3C80() mutates _bactOnMouse
            // to the commander, which makes the HP bar flash on the leader for
            // this frame before the next mouse raycast restores the clicked member.
            NC_STACK_ypabact *squadCommander = sub_4D3C3C(_bactOnMouse);
            if ( squadCommander )
            {
                _activeCmdrID = squadCommander->_commandID;
                sub_4C40AC();
            }
        }

        if ( _userRobo != _bactOnMouse )
            bzda.field_1D0 = bzda.field_1CC & 1;
        else
            bzda.field_1D0 = bzda.field_1CC & 0x20;
        break;

    case World::DOACTION_2:
        if (_activeCmdrRemapIndex >= 0)
        {
            NC_STACK_ypabact *orderedSquad = _cmdrsRemap[_activeCmdrRemapIndex];
            _updateMessage.user_action = World::DOACTION_2;
            _updateMessage.selectBact = orderedSquad;

            if ( _guiActFlags & 0x10 )
            {
                _updateMessage.target_Sect = _cellOnMouse;
                _updateMessage.target_point = _cellMouseIsectPos;
                _updateMessage.target_Bact = nullptr;

                if ( _cellOnMouse )
                {
                    // The move marker confirms the player's click, not sector
                    // ownership, fog state or distance from the ordered squad.
                    // Any valid world intersection accepted as a move order
                    // must therefore display the canonical move-order marker.
                    _moveOrderFeedbackActive = true;
                    _moveOrderFeedbackPos = _cellMouseIsectPos;
                    _moveOrderFeedbackStartTime = _timeStamp;
                }

                if ( orderedSquad )
                    yw_ClearAttackOrderFeedback(this, orderedSquad->_commandID,
                                                orderedSquad->_owner);
            }
            else
            {
                _updateMessage.target_Sect = 0;
                _updateMessage.target_Bact = _bactOnMouse;

                // OpenNeoUA custom: show a short animated feedback only for an
                // explicit attack order on an enemy unit. Sector attacks and
                // friendly-unit clicks do not use this marker.
                if ( _bactOnMouse &&
                     orderedSquad &&
                     _bactOnMouse != _userRobo &&
                     _bactOnMouse->_owner != _userRobo->_owner &&
                     _bactOnMouse->_status != BACT_STATUS_CREATE &&
                     _bactOnMouse->_status != BACT_STATUS_BEAM &&
                     _bactOnMouse->_status != BACT_STATUS_DEAD )
                {
                    yw_SetAttackOrderFeedback(this, orderedSquad, _bactOnMouse);
                }
                else if ( orderedSquad )
                {
                    yw_ClearAttackOrderFeedback(this, orderedSquad->_commandID,
                                                orderedSquad->_owner);
                }
            }

            bzda.field_1D0 = bzda.field_1CC & 1;
        }
        break;

    case World::DOACTION_WP_BEGIN:
    case World::DOACTION_WP_ADD:
    case World::DOACTION_WP_CYCLE:
        if (_activeCmdrRemapIndex >= 0)
        {
            _updateMessage.user_action = arg;
            _updateMessage.selectBact = _cmdrsRemap[_activeCmdrRemapIndex];

            if ( _guiActFlags & 0x30 )
            {
                _updateMessage.target_Sect = _cellOnMouse;
                _updateMessage.target_point = _cellMouseIsectPos;
                _updateMessage.target_Bact = nullptr;
            }
        }
        break;

    case World::DOACTION_ADD_UNIT1:
        _updateMessage.user_action = World::DOACTION_ADD_UNIT1;
        _updateMessage.protoID = bzda.field_2DC[bzda.field_8EC];
        _updateMessage.target_point = _mouseVector * _mouseSelDistance + _viewerPosition;

        bzda.field_1D0 = 4;

        _activeCmdrRemapIndex = -1;
        _activeCmdrID = 0;
        break;

    case World::DOACTION_ADD_UNIT2:
        if (_activeCmdrRemapIndex >= 0)
        {
            _updateMessage.user_action = World::DOACTION_ADD_UNIT2;
            _updateMessage.selectBact = _cmdrsRemap[_activeCmdrRemapIndex];

            _updateMessage.protoID = bzda.field_2DC[bzda.field_8EC];
            _updateMessage.target_point = _mouseVector * _mouseSelDistance + _viewerPosition;
        }
        break;

    case World::DOACTION_5:
        sb_0x4c66f8(this, _bactOnMouse, _viewerBact);

        if ( _bactOnMouse->_bact_type != BACT_TYPES_GUN && _bactOnMouse != _userRobo )
        {
            if ( sub_4D3C80(this) )
            {
                _activeCmdrID = _bactOnMouse->_commandID;
                sub_4C40AC();
            }
        }
        break;

    case World::DOACTION_6:
        _updateMessage.user_action = World::DOACTION_6;
        _updateMessage.protoID = bzda.field_3DC[bzda.field_8F4];
        _updateMessage.target_Sect = _cellOnMouse;
        _updateMessage.target_point = _cellMouseIsectPos;
        _updateMessage.target_Bact = nullptr;
        bzda.field_1D0 = bzda.field_1CC & 1;
        break;

    case World::DOACTION_10:
        _updateMessage.user_action = World::DOACTION_10;
        _updateMessage.target_Sect = _cellOnMouse;
        _updateMessage.target_point = _cellMouseIsectPos;
        _updateMessage.target_Bact = nullptr;
        bzda.field_1D0 = 1;
        break;

    case World::DOACTION_19:
        // OpenNeoUA custom: refuse to cycle the viewer into an artillery shell platform.
        if ( _bactOnMouse && _bactOnMouse->IsArtilleryShellPlatform() )
            break;
        _viewerBact->setBACT_viewer(false);
        _viewerBact->setBACT_inputting(false);
        _bactOnMouse->setBACT_viewer(true);
        break;

    default:
        return;
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub21(TInputState *arg)
{
    // OpenNeoUA custom: while the white artillery targeting preview is active,
    // RMB is first and foremost "cancel artillery aiming". Do this before the
    // vanilla FLAG_OK/viewer gate so the map-marker RMB binding cannot swallow it.
    if ( _userRobo && !IsSpectatorControlled() &&
         bzda.field_1D0 == 1 &&
         (_guiActFlags & 8) &&
         _artilleryShellManualGid &&
         (arg->ClickInf.flag & TClickBoxInf::FLAG_RM_DOWN) )
    {
        _artilleryShellManualGid = 0;
        _artilleryShellManualRadius = 0.0f;
        arg->ClickInf.flag &= ~TClickBoxInf::FLAG_RM_DOWN;
        return;
    }

    if ( _userRobo && (arg->ClickInf.flag & TClickBoxInf::FLAG_OK) && _viewerBact )
    {
        yw_MouseSelect(arg);
        ypaworld_func64__sub21__sub7();

        if ( !IsSpectatorControlled() && yw_TryAddClickedUnitToActiveSquad(this, arg) )
        {
            _bactPrevClicked = nullptr;
            _prevMousePos = Common::Point();
            arg->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
            return;
        }

        if ( IsSpectatorControlled() &&
             (arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN) &&
             (_guiActFlags & 0x20) &&
             _bactOnMouse &&
             IsValidSpectatorFollowTarget(_bactOnMouse) )
        {
            const bool jumpIntoVehicleMode = (bzda.field_1D0 & 8) != 0;

            SetSpectatorFollowTarget(_bactOnMouse);

            // Match the vanilla one-shot control mode: after a valid unit is
            // selected, release the Jump into Vehicle button. Direct one-click
            // Spectator Follow remains available when the mode was not armed.
            if ( jumpIntoVehicleMode )
                bzda.field_1D0 = bzda.field_1CC & 1;

            arg->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
            return;
        }

        if ( !IsSpectatorControlled() &&
             bzda.field_1D0 == 1 &&
             (_guiActFlags & 8) &&
             (arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN) &&
             HandleArtilleryShellMapClick() )
        {
            arg->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
            return;
        }

        int tooltip = 0;
        int mousePointer = 0;
        int doAction = World::DOACTION_0;

        if ( _mouseGrabbed )
        {
            mousePointer = -1;
        }
        else if ( _guiDragDefaultMouse || ((_guiActFlags & 4) && !(_guiActFlags & 0x48)) )
        {
            mousePointer = 0;
        }
        else if ( robo_map.field_1E8 & 0x200 )
        {
            mousePointer = 2;
        }
        else
        {

            // The tactical map always permits marker placement, so its neutral
            // cursor is Pointer rather than the blocking Cancel cursor.
            mousePointer = (_guiActFlags & 8) ? 0 : 1;
            doAction = World::DOACTION_0;

            if ( bzda.field_1D0 == 1 )
            {
                if ( !(_guiActFlags & 0x10) )
                {
                    if ( (_guiActFlags & 0x20) )
                    {
                        if ( _userRobo->_owner == _bactOnMouse->_owner )
                        {
                            doAction = World::DOACTION_8;
                            mousePointer = 2;
                            tooltip = Locale::TIP_DO_SELECT;
                        }
                        else if ( _activeCmdrRemapIndex != -1 )
                        {
                            doAction = World::DOACTION_2;
                            mousePointer = 3;
                            tooltip = Locale::TIP_DO_ATKVHCL;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {
                    if ( _userRobo->_owner == _cellOnMouse->owner && _cellOnMouse->IsCanSee(_userRobo->_owner) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                        mousePointer = 3;
                    }
                }
            }
            else if ( bzda.field_1D0 == 2 )
            {
                if ( !(_guiActFlags & 8) )
                {
                    if ( (_guiActFlags & 0x20) && _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        switch ( sb_0x4d3d44(&arg->ClickInf) )
                        {
                        case 0:
                            doAction = World::DOACTION_ADD_UNIT1;
                            mousePointer = 6;
                            tooltip = Locale::TIP_DO_NEWSQUAD;
                            break;

                        case 1:
                            tooltip = Locale::TIP_ERR_NOROOM;
                            break;

                        case 2:
                            tooltip = Locale::TIP_ERR_NOENERGY;
                            break;

                        case 3:
                            tooltip = Locale::TIP_UNITLIMIT;
                            break;

                        default:
                            break;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {
                    if ( !(_guiActFlags & 0x10) )
                    {
                        if ( _guiActFlags & 0x20 )
                        {
                            if ( _userRobo->_owner == _bactOnMouse->_owner )
                            {
                                doAction = World::DOACTION_8;
                                mousePointer = 2;
                                tooltip = Locale::TIP_DO_SELECT;
                            }
                            else
                            {
                                doAction = World::DOACTION_2;
                                mousePointer = 3;
                                tooltip = Locale::TIP_DO_ATKVHCL;
                            }
                        }
                    }
                    else if ( _userRobo->_owner == _cellOnMouse->owner && _cellOnMouse->IsCanSee(_userRobo->_owner) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                    }
                }
            }
            else if ( bzda.field_1D0 == 4 )
            {
                if ( !(_guiActFlags & 8) )
                {
                    if ( (_guiActFlags & 0x20) && _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else if ( _activeCmdrRemapIndex != -1 )
                    {
                        switch ( sb_0x4d3d44(&arg->ClickInf) )
                        {
                        case 0:
                            doAction = World::DOACTION_ADD_UNIT2;
                            mousePointer = 7;
                            tooltip = Locale::TIP_DO_ADDSQUAD;
                            break;

                        case 1:
                            tooltip = Locale::TIP_ERR_NOROOM;
                            break;

                        case 2:
                            tooltip = Locale::TIP_ERR_NOENERGY;
                            break;

                        case 3:
                            tooltip = Locale::TIP_UNITLIMIT;
                            break;

                        default:
                            break;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {

                    if ( !(_guiActFlags & 0x10) )
                    {
                        if ( _guiActFlags & 0x20 )
                        {
                            if ( _userRobo->_owner == _bactOnMouse->_owner )
                            {
                                doAction = World::DOACTION_8;
                                mousePointer = 2;
                                tooltip = Locale::TIP_DO_SELECT;
                            }
                            else
                            {
                                doAction = World::DOACTION_2;
                                mousePointer = 3;
                                tooltip = Locale::TIP_DO_ATKVHCL;
                            }
                        }
                    }
                    else if ( _userRobo->_owner == _cellOnMouse->owner && _cellOnMouse->IsCanSee(_userRobo->_owner) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                    }
                }
            }
            else if (bzda.field_1D0 == 8 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    // OpenNeoUA custom: artillery shell platforms cannot be possessed, so show no
                    // "take control" cursor/tooltip when hovering one (it would wrongly
                    // suggest the player can enter it).
                    if ( _bactOnMouse->IsArtilleryShellPlatform() )
                    {
                        // leave mousePointer at its neutral value: no control prompt
                    }
                    else if ( !System::IniConf::IsGameNewDebugEnabled() &&
                              arg->KbdLastDown == Input::KC_F7 &&
                              (arg->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD) )
                    {
                        doAction = World::DOACTION_19;
                        mousePointer = 8;
                    }
                    else if ( _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_5;
                        mousePointer = 8;
                        tooltip = Locale::TIP_DO_CONTROL;
                    }
                }
            }
            else if ( bzda.field_1D0 == 16 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    if ( _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKVHCL;
                    }
                }
                else
                {
                    switch ( UserActBuildCheck() )
                    {
                    case 0:
                        doAction = World::DOACTION_6;
                        mousePointer = 10;
                        tooltip = Locale::TIP_DO_BUILD;
                        break;

                    case 6:
                        tooltip = Locale::TIP_ERR_NOENERGY;
                        break;

                    case 2:
                        tooltip = Locale::TIP_ERR_NOTOWNED;
                        break;

                    case 3:
                        tooltip = Locale::TIP_ERR_BUILD_TOOCLOSE;
                        break;

                    case 4:
                        tooltip = Locale::TIP_ERR_BUILD_TOOFAR;
                        break;

                    case 5:
                        tooltip = Locale::TIP_ERR_BUILD_INPROC;
                        break;

                    default:
                        break;
                    }
                }
            }
            else if ( bzda.field_1D0 == 32 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    if ( _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKVHCL;
                    }
                }
                else
                {
                    switch ( ypaworld_func64__sub21__sub3() )
                    {
                    case 0:
                        doAction = World::DOACTION_10;
                        mousePointer = 9;
                        tooltip = Locale::TIP_DO_TELEPORT;
                        break;

                    case 5:
                        tooltip = Locale::TIP_ERR_UNITSINSECTOR;
                        break;

                    case 1:
                        tooltip = Locale::TIP_ERR_NOENERGY;
                        break;

                    case 2:
                        tooltip = Locale::TIP_ERR_SECTORJAGGY;
                        break;

                    case 3:
                        tooltip = Locale::TIP_ERR_LOCUNKNOWN;
                        break;

                    default:
                        break;
                    }
                }
            }

            if ( IsSpectatorControlled() )
            {
                doAction = World::DOACTION_0;

                if ( (_guiActFlags & 0x20) && IsValidSpectatorFollowTarget(_bactOnMouse) )
                {
                    if ( bzda.field_1D0 & 8 )
                    {
                        // The armed button uses the same cursor and tooltip as
                        // vanilla Jump into Vehicle, while the resulting view
                        // remains the external Spectator Follow camera.
                        mousePointer = 8;
                        tooltip = Locale::TIP_DO_CONTROL;
                    }
                    else
                    {
                        mousePointer = 2;
                        tooltip = 0;
                    }
                }
                else
                {
                    mousePointer = 1;
                    tooltip = 0;
                }
            }

            // OpenNeoUA custom: artillery shell cursor feedback.
            //  - In the 3D world, hovering one of our artillery shells shows the "can't enter"
            //    cursor (it can't be possessed).
            //  - On the strategic map it must instead read as USABLE: a "target here"
            //    cursor while aiming a selected artillery shell, and a "selectable" cursor when
            //    hovering one. (The blue "your unit" marker overlay is unaffected.)
            if ( !IsSpectatorControlled() )
            {
                bool onMap = (_guiActFlags & 8) != 0;
                bool hoverOwnArtilleryShell = (_guiActFlags & 0x20) && _bactOnMouse &&
                                      _bactOnMouse->_owner == _userRobo->_owner &&
                                      _bactOnMouse->IsArtilleryShellPlatform();

                if ( onMap && _artilleryShellManualGid )
                {
                    mousePointer = 3;   // aiming the bombardment zone: "target here"
                    tooltip = 0;
                }
                else if ( onMap && hoverOwnArtilleryShell )
                {
                    mousePointer = 2;   // hovering our artillery shell on the map: "selectable"
                    tooltip = 0;
                }
                else if ( !onMap && hoverOwnArtilleryShell )
                {
                    mousePointer = 1;   // 3D world: can't be entered
                    tooltip = 0;
                }
            }

            const bool mouseOverMapMarker =
                arg->ClickInf.selected_btn == &robo_map &&
                arg->ClickInf.selected_btnID == 17 &&
                yw_FindRoboMapMarkerAt(arg->ClickInf.move.BtnPos) >= 0;
            if ( mouseOverMapMarker )
            {
                // A marker is always an annotation, never a move/attack target.
                // Pointer feedback matches LMB drag and RMB deletion.
                mousePointer = 0;
                doAction = World::DOACTION_0;
                tooltip = 0;
            }

            _doAction = ypaworld_func64__sub21__sub4(arg, doAction);
            int v18 = _doAction;

            if ( !IsSpectatorControlled() && ypaworld_func64__sub21__sub6(&arg->ClickInf) )
            {
                // OpenNeoUA custom: a double-click must never possess an artillery shell platform.
                if ( !_bactOnMouse || !_bactOnMouse->IsArtilleryShellPlatform() )
                {
                    v18 = World::DOACTION_5;
                    mousePointer = 8;
                    arg->ClickInf.flag |= TClickBoxInf::FLAG_LM_DOWN;
                }
            }

            if ( arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN )
                ypaworld_func64__sub21__sub5(v18);

            if ( (arg->ClickInf.flag & TClickBoxInf::FLAG_LM_HOLD) && v18 != World::DOACTION_0 )
                mousePointer = 0;
        }

        if ( _mouseCursorHidden )
            mousePointer = -1;

        GFX::displ_arg263 v19;

        if ( mousePointer == -1 )
            v19.bitm = nullptr;
        else if (_mousePointers[mousePointer])
            v19.bitm = _mousePointers[mousePointer]->GetBitmap();

        switch ( mousePointer )
        {
        case 0:
            v19.pointer_id = 1;
            break;

        case 1:
            v19.pointer_id = 2;
            break;

        case 2:
            v19.pointer_id = 3;
            break;

        case 3:
            v19.pointer_id = 4;
            break;

        case 4:
            v19.pointer_id = 5;
            break;

        case 5:
            v19.pointer_id = 6;
            break;

        case 6:
            v19.pointer_id = 7;
            break;

        case 7:
            v19.pointer_id = 8;
            break;

        case 8:
            v19.pointer_id = 9;
            break;

        case 9:
            v19.pointer_id = 10;
            break;

        case 10:
            v19.pointer_id = 11;
            break;

        default:
            v19.pointer_id = 0;
            break;
        }

        if ( squadron_manager.field_2A8 & 1 )
            v19.pointer_id = 1;

        GFX::Engine.SetCursor(v19.pointer_id, 0);

        if ( tooltip )
            SetShowingTooltip(tooltip);
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub1(TInputState *inpt)
{
    //Precompute input
    TClickBoxInf *winp = &inpt->ClickInf;

    inpt->Buttons.UnSet(31);
    inpt->HandBrakePressed = inpt->Buttons.Is(3);

    // model = ufo never owns the generic Stop/Hand Brake action. Spy Mode has
    // its own remappable binding (Space by default), while other classes keep
    // the existing brake behavior unchanged.
    const bool controlledUfo = _userUnit && _userUnit->_bact_type == BACT_TYPES_UFO;
    if ( controlledUfo )
    {
        inpt->Buttons.UnSet(3);
        inpt->HandBrakePressed = false;
    }

    const bool cycleTargetPressed = inpt->Buttons.Is(6);
    if ( cycleTargetPressed && !_cycleTargetBtnIsDown && _userUnit )
        _userUnit->RequestHomingTargetCycle();
    _cycleTargetBtnIsDown = cycleTargetPressed;

    const bool alternativeViewPressed = inpt->Buttons.Is(7);
    if ( _userUnit )
        _userUnit->SetAlternativeViewActive(alternativeViewPressed);

    // Switch Weapon is a remappable player action, so the configured physical
    // key is authoritative here. Keep the legacy Button[1] channel only as a
    // boot-time fallback when the user binding is not available yet.
    bool weaponSwitchPressed = false;
    if ( _GameShell &&
         World::INPUT_BIND_SWITCH_WEAPON < (int)_GameShell->InputConfig.size() )
    {
        const UserData::TInputConf &switchBind =
            _GameShell->InputConfig[World::INPUT_BIND_SWITCH_WEAPON];

        if ( switchBind.Type == World::INPUT_BIND_TYPE_BUTTON &&
             switchBind.PKeyCode > Input::KC_NONE &&
             switchBind.PKeyCode < Input::KC_MAX )
            weaponSwitchPressed = Input::Engine.GetKeyState(switchBind.PKeyCode);
    }
    else
    {
        weaponSwitchPressed = inpt->Buttons.Is(1);
    }

    // The fixed secondary shortcut comes from the shared input definition.
    // On an eligible UFO the same MMB edge belongs to UFO Spy Mode, so do
    // not also route the held button into Switch Weapon. Everywhere else the
    // existing MMB weapon shortcut remains unchanged.
    const bool ufoSpyOwnsFixedShortcut =
        _mouseGrabbed && IsUfoSpyUiControlContext() &&
        World::IsFixedInputShortcutHeld(World::INPUT_BIND_TOGGLE_UFO_SPY_UI);
    if ( _mouseGrabbed && !ufoSpyOwnsFixedShortcut &&
         World::IsFixedInputShortcutHeld(World::INPUT_BIND_SWITCH_WEAPON) )
    {
        weaponSwitchPressed = true;
    }

    if ( !weaponSwitchPressed )
    {
        _weaponSwitchBtnIsDown = false;
    }
    else if ( !_weaponSwitchBtnIsDown && _userUnit && _userUnit->CycleControlledWeapon() )
    {
        // Consume the press only after a real switch. A temporarily unavailable
        // switch can therefore succeed later without requiring an unrelated key.
        _weaponSwitchBtnIsDown = true;
    }

    int v38 = 0;

    if ( _timeStamp - _vehicleTakenControlTimestamp < 500 )
        inpt->Buttons.UnSet(0);

    if ( _timeStamp - _vehicleTakenControlTimestamp > 5000 )
        _invulnerable = 0;

    if ( _shellConfIsParsed )
    {
        if ( _preferences & World::PREF_JOYDISABLE )
            v38 = 1;
    }

    _fireBtnDownHappen = false;

    if ( inpt->Buttons.Is(0) )
    {
        if ( !_fireBtnIsDown )
            _fireBtnDownHappen = true;

        _fireBtnIsDown = true;
    }
    else
    {
        _fireBtnIsDown = false;
    }

    if ( _userUnit->_status == BACT_STATUS_DEAD )
    {
        _mouseGrabbed = false;
    }
    else if ( winp->flag & TClickBoxInf::FLAG_RM_DOWN )
    {
        if ( _mouseGrabbed
                || winp->selected_btn == &robo_map
                || winp->selected_btn == &bzda
                || winp->selected_btn == &squadron_manager
                || (!(bzda.field_1D0 & 1) && _activeCmdrRemapIndex != -1) )
        {
            if ( _mouseGrabbed )
            {
                _mouseGrabbed = false;
            }
            else
            {
                if ( winp->selected_btn != &robo_map && winp->selected_btn != &bzda && !(bzda.field_1D0 & 1) )
                    bzda.field_1D0 = bzda.field_1CC & 1;
            }
        }
        else
        {
            _mouseGrabbed = true;

            Input::Engine.ResetSlider(10);
            Input::Engine.ResetSlider(11);
        }
    }

    if ( _mouseGrabbed ) // If grabbed mouse
    {
        // Piu-piu mazafaka
        winp->selected_btnID = -1;
        winp->flag &= ~(TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD | TClickBoxInf::FLAG_BTN_UP);

        inpt->Sliders[0] += inpt->Sliders[10];
        inpt->Sliders[1] += inpt->Sliders[11];
        inpt->Sliders[3] += inpt->Sliders[10];
        inpt->Sliders[5] -= inpt->Sliders[11] * 1.5;

        if ( World::IsFixedInputShortcutHeld(World::INPUT_BIND_FIRE) )
            inpt->Buttons.Set(0);

    }

    if ( !v38 )
    {
        int v13 = _userUnit->_bact_type == BACT_TYPES_TANK || _userUnit->_bact_type == BACT_TYPES_CAR;
        int v34 = _userUnit->_bact_type == BACT_TYPES_ROBO;

        if ( !_framesElapsed )
        {
            _oldJoyX = inpt->Sliders[12];
            _oldJoyY = inpt->Sliders[13];
            _oldJoyZ = inpt->Sliders[14];
            _oldJoyHatX = inpt->Sliders[15];
            _oldJoyHatY = inpt->Sliders[16];
        }

        int v45 = fabs(_oldJoyX - inpt->Sliders[12]) >= 0.05;
        int v44 = fabs(_oldJoyY - inpt->Sliders[13]) >= 0.05;
        int v46 = fabs(_oldJoyZ - inpt->Sliders[14]) >= 0.05;
        int v43 = fabs(_oldJoyHatX - inpt->Sliders[15]) >= 0.05;
        int v47 = fabs(_oldJoyHatY - inpt->Sliders[16]) >= 0.05;

        if ( v45 || v44 || v46 || v43 || v47)
        {
            _oldJoyX = inpt->Sliders[12];
            _oldJoyY = inpt->Sliders[13];
            _oldJoyZ = inpt->Sliders[14];
            _oldJoyHatX = inpt->Sliders[15];
            _oldJoyHatY = inpt->Sliders[16];
        }

        if ( _joyIgnoreX )
        {
            if ( v45 )
                _joyIgnoreX = 0;
        }
        else
        {
            if ( ( v13 && inpt->Sliders[3] != 0.0 ) || inpt->Sliders[0] != 0.0 )
                _joyIgnoreX = 1;
        }

        if ( _joyIgnoreY )
        {
            if ( v44 || v43 || v47 )
                _joyIgnoreY = 0;
        }
        else
        {
            if ( v13 )
            {
                if ( (_preferences & World::PREF_ALTJOYSTICK ) && inpt->Sliders[5] != 0.0 )
                    _joyIgnoreY = 1;
                else if ( inpt->Sliders[4] != 0.0 )
                    _joyIgnoreY = 1;
            }
            else if ( inpt->Sliders[1] != 0.0 )
                _joyIgnoreY = 1;
        }

        if ( _joyIgnoreZ )
        {
            if ( v46 || v43 || v47 )
                _joyIgnoreZ = 0;
        }
        else
        {
            if ( v13 )
            {
                if ( (_preferences & World::PREF_ALTJOYSTICK) && inpt->Sliders[4] != 0.0 )
                    _joyIgnoreZ = 1;
            }
            else if ( !v34 && inpt->Sliders[2] != 0.0 )
                _joyIgnoreZ = 1;
        }

        if ( inpt->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN )
        {
            if ( !(v43 || v44 || v45 || v46 || v47) )
            {
                _joyIgnoreY = 1;
                _joyIgnoreX = 1;
            }
        }

        if ( _joyIgnoreX )
            inpt->Sliders[12] = 0;

        if ( _joyIgnoreY )
            inpt->Sliders[13] = 0;

        if ( _joyIgnoreZ )
            inpt->Sliders[14] = 0;

        if ( !_joyIgnoreX )
        {
            if ( v13 )
                inpt->Sliders[3] += inpt->Sliders[12];
            else
                inpt->Sliders[0] += inpt->Sliders[12];
        }

        if ( !_joyIgnoreY )
        {
            if ( v13 )
            {
                if ( _preferences & World::PREF_ALTJOYSTICK )
                {
                    if ( inpt->Sliders[16] != 0.0 )
                        inpt->Sliders[5] += inpt->Sliders[16];
                    else
                        inpt->Sliders[5] += inpt->Sliders[13] * 2.0;
                }
                else
                {

                    if ( fabs(inpt->Sliders[13]) < 0.3 )
                        inpt->Buttons.Set(3);
                    else
                        inpt->Sliders[4] += -inpt->Sliders[13];

                    inpt->Buttons.Set(31);
                }
            }
            else
            {
                inpt->Sliders[1] += inpt->Sliders[13];
            }
        }

        if ( !_joyIgnoreZ )
        {
            if ( v13 )
            {
                if ( _preferences & World::PREF_ALTJOYSTICK )
                {
                    if ( inpt->Sliders[4] != 0.0 )
                    {
                    }
                    else if ( fabs(inpt->Sliders[14]) < 0.3 )
                    {
                        inpt->Buttons.Set(3);
                    }
                    else
                        inpt->Sliders[4] += inpt->Sliders[14];

                    inpt->Buttons.Set(31);
                }
                else
                {
                    if ( inpt->Sliders[5] == 0.0 )
                        inpt->Sliders[5] += inpt->Sliders[16];
                }
            }
            else
            {
                if ( inpt->Sliders[2] == 0.0 )
                    inpt->Sliders[2] += inpt->Sliders[14];
            }
        }
        if ( inpt->Buttons.Is(16) )
            inpt->Buttons.Set(0);

        if ( inpt->Buttons.Is(17) )
            inpt->Buttons.Set(2);

        if ( inpt->Buttons.Is(18) )
            inpt->Buttons.Set(1);

        if ( inpt->Buttons.Is(19) )
        {
            inpt->Buttons.Set(3);
            inpt->HandBrakePressed = true;
        }
    }

    if ( _userUnit && _userUnit->_bact_type == BACT_TYPES_UFO )
    {
        // Synthetic joystick auto-brake must not reintroduce the removed UFO
        // handbrake after the explicit input was consumed above.
        inpt->Buttons.UnSet(3);
        inpt->HandBrakePressed = false;
    }
}
