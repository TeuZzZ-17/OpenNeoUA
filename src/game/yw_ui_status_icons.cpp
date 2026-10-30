// Unit status icons (regen, drain, damage, radar...) with their blink logic,
// and the procedural HP/shield bars drawn above units and in the cockpit.
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

namespace
{
// Icon bitmaps and resolved icon paths, cached by configured path.
std::map<std::string, StatusIconCacheEntry> g_statusIconCache;
std::map<std::string, std::string> g_statusIconConfiguredPathCache;
// Per-unit blink state of the icons, pruned against the current world.
std::map<const NC_STACK_ypabact *, StatusIconBlinkState> g_statusIconBlinkStates;
NC_STACK_ypaworld *g_statusIconBlinkWorld = nullptr;
int32_t g_statusIconBlinkLastGameTime = 0;
uint32_t g_statusIconBlinkLastPrune = 0;
}

uint8_t yw_GetConfiguredUiOpacity(Common::Ini::Key &key, int fallback)
{
    return (uint8_t)System::IniConf::GetUiOpacity(key, fallback);
}

uint8_t yw_GetMapOpacity()
{
    return yw_GetConfiguredUiOpacity(System::IniConf::UiMapOpacity,
                                     System::IniConf::UiMapDefaultOpacity);
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
        const std::string configured = StrTrim(key.Get<std::string>());
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
    const uint8_t energyState = yw ? yw->GetUnitEnergyVisualState(bact) : (uint8_t)UNIT_ENERGY_VISUAL_NONE;
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
