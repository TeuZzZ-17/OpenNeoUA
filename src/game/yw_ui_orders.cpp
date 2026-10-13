// World selection by mouse and order feedback (move/attack markers).
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

void yw_ResetWorldSelectionDrag(NC_STACK_ypaworld *yw)
{
    yw->_worldSelectDragArmed = false;
    yw->_worldSelectDragActive = false;
    yw->_worldSelectDragStart = Common::Point();
    yw->_worldSelectDragCurrent = Common::Point();
}

bool yw_ProjectWorldSelectionPoint(NC_STACK_ypaworld *yw, const vec3d &worldPos, Common::Point *screenPos)
{
    vec3d delta = worldPos - yw->_viewerPosition;

    mat3x3 corrected = yw->_viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, false);

    vec3d projected = corrected.Transform(delta);
    if ( projected.z <= 30.0 )
        return false;

    float x = projected.x / projected.z;
    float y = projected.y / projected.z;
    GFX::Engine.viewZoomCorrection(x, y);
    if ( x < -1.0 || x > 1.0 || y < -1.0 || y > 1.0 )
        return false;

    screenPos->x = (int)((yw->_screenSize.x / 2) * (x + 1.0));
    screenPos->y = (int)((yw->_screenSize.y / 2) * (y + 1.0));
    return true;
}

bool yw_IsWorldSelectionUnitValid(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    if ( !bact ||
         bact->_owner != yw->_userRobo->_owner ||
         bact->_energy <= 0 ||
         bact->_status == BACT_STATUS_CREATE ||
         bact->_status == BACT_STATUS_BEAM ||
         bact->_status == BACT_STATUS_DEAD ||
         (bact->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN | BACT_STFLAG_NORENDER)) ||
         bact->ShouldHideFromStrategicUI() )
    {
        return false;
    }

    switch ( bact->_bact_type )
    {
    case BACT_TYPES_BACT:
    case BACT_TYPES_TANK:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
        return true;

    default:
        return false;
    }
}

bool yw_TryAddClickedUnitToActiveSquad(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( !Input::Engine.GetKeyState(Input::KC_CTRL) ||
         !(inpt->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN) ||
         bzda.field_1D0 != 1 ||
         !(yw->_guiActFlags & 0x20) ||
         !yw_IsWorldSelectionUnitValid(yw, yw->_bactOnMouse) ||
         yw->_activeCmdrRemapIndex < 0 ||
         (size_t)yw->_activeCmdrRemapIndex >= yw->_cmdrsRemap.size() )
    {
        return false;
    }

    NC_STACK_ypabact *activeCommander = yw->_cmdrsRemap[yw->_activeCmdrRemapIndex];
    NC_STACK_ypabact *clickedUnit = yw->_bactOnMouse;
    if ( !activeCommander ||
         activeCommander->_status == BACT_STATUS_CREATE ||
         activeCommander->_status == BACT_STATUS_BEAM ||
         activeCommander->_status == BACT_STATUS_DEAD )
    {
        return false;
    }

    bact_arg109 reorganize;
    uint32_t activeCommandID = activeCommander->_commandID;

    if ( clickedUnit == activeCommander )
    {
        // A squad cannot become empty. With members present, vanilla group
        // reorganization promotes one of them and makes the old leader solo.
        if ( activeCommander->_kidList.empty() )
            return true;

        reorganize.field_0 = 3;
        reorganize.field_4 = nullptr;
        clickedUnit->ReorganizeGroup(&reorganize);
    }
    else if ( clickedUnit->_parent == activeCommander )
    {
        reorganize.field_0 = 3;
        reorganize.field_4 = nullptr;
        clickedUnit->ReorganizeGroup(&reorganize);
    }
    else
    {
        reorganize.field_0 = 4;
        reorganize.field_4 = clickedUnit;
        activeCommander->ReorganizeGroup(&reorganize);
    }

    yw->_activeCmdrID = activeCommandID;
    yw->sub_4C40AC();
    sub_4C707C(yw);
    return true;
}

void yw_SelectWorldUnitsInDrag(NC_STACK_ypaworld *yw)
{
    int left = std::min(yw->_worldSelectDragStart.x, yw->_worldSelectDragCurrent.x);
    int right = std::max(yw->_worldSelectDragStart.x, yw->_worldSelectDragCurrent.x);
    int top = std::min(yw->_worldSelectDragStart.y, yw->_worldSelectDragCurrent.y);
    int bottom = std::max(yw->_worldSelectDragStart.y, yw->_worldSelectDragCurrent.y);

    std::vector<NC_STACK_ypabact *> selected;
    selected.reserve(32);

    auto collect = [&](NC_STACK_ypabact *bact)
    {
        if ( !yw_IsWorldSelectionUnitValid(yw, bact) ||
             std::find(selected.begin(), selected.end(), bact) != selected.end() )
        {
            return;
        }

        if ( yw_GetWorldUiOpacity(yw, bact->_position) == 0 )
            return;

        Common::Point point;
        if ( !yw_ProjectWorldSelectionPoint(yw, bact->_position, &point) ||
             point.x < left || point.x > right ||
             point.y < top || point.y > bottom )
        {
            return;
        }

        selected.push_back(bact);
    };

    for ( NC_STACK_ypabact *commander : yw->_cmdrsRemap )
    {
        collect(commander);

        for ( NC_STACK_ypabact *unit : commander->_kidList )
            collect(unit);
    }

    if ( selected.empty() )
        return;

    NC_STACK_ypabact *leader = selected.front();

    bact_arg109 arg109;
    arg109.field_4 = nullptr;
    arg109.field_0 = 3;
    leader->ReorganizeGroup(&arg109);

    for (size_t i = 1; i < selected.size(); i++)
    {
        arg109.field_4 = selected[i];
        arg109.field_0 = 4;
        leader->ReorganizeGroup(&arg109);
    }

    yw->_activeCmdrID = leader->_commandID;
    yw->sub_4C40AC();
}

bool yw_CanStartWorldSelectionDrag(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    int viewportBottom = yw->_screenSize.y - yw->_downScreenBorder;
    Common::Point mouse = inpt->ClickInf.move.ScreenPos;

    return yw->_userRobo &&
           yw->_viewerBact &&
           yw->_levelInfo.State == TLevelInfo::STATE_PLAYING &&
           !yw->IsSpectatorControlled() &&
           !yw->_mouseGrabbed &&
           !yw->_guiDragging &&
           !yw->_guiDragDefaultMouse &&
           !yw->_makingWaypointsMode &&
           exit_menu.IsClosed() &&
           gui_lstvw.IsClosed() &&
           bzda.field_1D0 == 1 &&
           inpt->ClickInf.selected_btn == nullptr &&
           mouse.x >= 0 && mouse.x < yw->_screenSize.x &&
           mouse.y >= yw->_upScreenBorder && mouse.y < viewportBottom;
}

Common::Point yw_ClampWorldSelectionPoint(NC_STACK_ypaworld *yw, Common::Point point)
{
    int viewportBottom = yw->_screenSize.y - yw->_downScreenBorder - 1;

    point.x = std::max(0, std::min(point.x, yw->_screenSize.x - 1));
    point.y = std::max(yw->_upScreenBorder, std::min(point.y, viewportBottom));
    return point;
}

void yw_WorldSelectionDragInput(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    constexpr int DRAG_THRESHOLD = 6;
    TClickBoxInf *mouse = &inpt->ClickInf;

    if ( !(mouse->flag & TClickBoxInf::FLAG_OK) )
    {
        yw_ResetWorldSelectionDrag(yw);
        return;
    }

    if ( !yw->_worldSelectDragArmed )
    {
        if ( (mouse->flag & TClickBoxInf::FLAG_LM_DOWN) &&
             yw_CanStartWorldSelectionDrag(yw, inpt) )
        {
            yw->yw_MouseSelect(inpt);

            if ( !(yw->_guiActFlags & (4 | 0x20)) )
            {
                yw->_worldSelectDragArmed = true;
                yw->_worldSelectDragStart = yw_ClampWorldSelectionPoint(yw, mouse->move.ScreenPos);
                yw->_worldSelectDragCurrent = yw->_worldSelectDragStart;
                mouse->flag &= ~TClickBoxInf::FLAG_LM_DOWN;
            }
        }
        return;
    }

    if ( !yw_CanStartWorldSelectionDrag(yw, inpt) )
    {
        yw_ResetWorldSelectionDrag(yw);
        return;
    }

    yw->_worldSelectDragCurrent = yw_ClampWorldSelectionPoint(yw, mouse->move.ScreenPos);

    if ( mouse->flag & TClickBoxInf::FLAG_LM_HOLD )
    {
        int dx = yw->_worldSelectDragCurrent.x - yw->_worldSelectDragStart.x;
        int dy = yw->_worldSelectDragCurrent.y - yw->_worldSelectDragStart.y;
        if ( dx * dx + dy * dy >= DRAG_THRESHOLD * DRAG_THRESHOLD )
            yw->_worldSelectDragActive = true;

        mouse->flag &= ~TClickBoxInf::FLAG_LM_DOWN;
        return;
    }

    bool released = (mouse->flag & TClickBoxInf::FLAG_LM_UP) != 0;
    bool wasDrag = yw->_worldSelectDragActive;

    if ( released && wasDrag )
    {
        yw_SelectWorldUnitsInDrag(yw);
        mouse->flag &= ~(TClickBoxInf::FLAG_LM_DOWN | TClickBoxInf::FLAG_LM_UP);
    }
    else if ( released )
    {
        mouse->flag |= TClickBoxInf::FLAG_LM_DOWN;
    }

    yw_ResetWorldSelectionDrag(yw);
}

void yw_RenderWorldSelectionDrag(NC_STACK_ypaworld *yw)
{
    if ( !yw->_worldSelectDragActive )
        return;

    int left = std::min(yw->_worldSelectDragStart.x, yw->_worldSelectDragCurrent.x);
    int right = std::max(yw->_worldSelectDragStart.x, yw->_worldSelectDragCurrent.x);
    int top = std::min(yw->_worldSelectDragStart.y, yw->_worldSelectDragCurrent.y);
    int bottom = std::max(yw->_worldSelectDragStart.y, yw->_worldSelectDragCurrent.y);
    int halfWidth = yw->_screenSize.x / 2;
    int halfHeight = yw->_screenSize.y / 2;

    left -= halfWidth;
    right -= halfWidth;
    top -= halfHeight;
    bottom -= halfHeight;

    GFX::Engine.raster_func217(yw_GetNeutralSelectionShadowColor());
    GFX::Engine.raster_func201(Common::Line(left + 1, top + 1, right + 1, top + 1));
    GFX::Engine.raster_func201(Common::Line(right + 1, top + 1, right + 1, bottom + 1));
    GFX::Engine.raster_func201(Common::Line(right + 1, bottom + 1, left + 1, bottom + 1));
    GFX::Engine.raster_func201(Common::Line(left + 1, bottom + 1, left + 1, top + 1));

    GFX::Engine.raster_func217(yw_GetFactionSelectionColor(yw));
    GFX::Engine.raster_func201(Common::Line(left, top, right, top));
    GFX::Engine.raster_func201(Common::Line(right, top, right, bottom));
    GFX::Engine.raster_func201(Common::Line(right, bottom, left, bottom));
    GFX::Engine.raster_func201(Common::Line(left, bottom, left, top));
}

int yw_GetOrderIconOwner(NC_STACK_ypaworld *yw)
{
    if ( yw && yw->_userRobo && yw->_userRobo->_owner >= 1 &&
         yw->_userRobo->_owner <= 6 )
        return yw->_userRobo->_owner;

    return 0;
}

std::string yw_OrderTemplateCanonicalPath(YwOrderTemplateId templateId)
{
    switch ( templateId )
    {
    case YW_ORDER_TEMPLATE_ATTACK:
        return "Interface/Actions/AttackOrder/attack_order.svg";

    case YW_ORDER_TEMPLATE_ROBO_MOVE:
        return "Interface/Actions/MoveOrder/robo_move_order.svg";

    case YW_ORDER_TEMPLATE_MAP_MARKER:
        return "Interface/Actions/MapMarker/map_marker.svg";

    case YW_ORDER_TEMPLATE_MOVE:
    default:
        return "Interface/Actions/MoveOrder/move_order.svg";
    }
}

YwLoadedUiBitmap yw_LoadOrderTemplateBitmap(NC_STACK_ypaworld *yw,
                                                   YwOrderTemplateId templateId)
{
    (void)yw;
    return yw_LoadNeutralOwnerTintSvg(yw_OrderTemplateCanonicalPath(templateId));
}

void yw_RenderOrderTemplateAt(NC_STACK_ypaworld *yw,
                                     const Common::Point &point,
                                     int radius, uint8_t worldUiOpacity,
                                     YwOrderTemplateId templateId)
{
    YwLoadedUiBitmap bitmap = yw_LoadOrderTemplateBitmap(yw, templateId);
    if ( !bitmap.bitmap || !bitmap.bitmap->GetBitmap() )
        return;

    const int owner = std::max(yw_GetOrderIconOwner(yw), static_cast<int>(World::OWNER_RESIST));
    const SDL_Color tint = yw_GetUiOwnerTintColor(yw, owner);
    const int size = std::max(1, radius * 2 + 8);

    if ( bitmap.ownerTint )
    {
        StatusIconRenderBitmapTinted(yw, bitmap.bitmap,
                                     point.x - size / 2 + 1,
                                     point.y - size / 2 + 1,
                                     size + 2, &tint,
                                     (uint8_t)(worldUiOpacity / 3));
        StatusIconRenderBitmapTinted(yw, bitmap.bitmap,
                                     point.x - size / 2,
                                     point.y - size / 2,
                                     size, &tint, worldUiOpacity);
    }
    else
    {
        StatusIconRenderBitmap(yw, bitmap.bitmap,
                               point.x - size / 2 + 1,
                               point.y - size / 2 + 1,
                               size + 2, (uint8_t)(worldUiOpacity / 3));
        StatusIconRenderBitmap(yw, bitmap.bitmap,
                               point.x - size / 2,
                               point.y - size / 2,
                               size, worldUiOpacity);
    }
}

void yw_RenderRoboRelocationMarker(NC_STACK_ypaworld *yw)
{
    if ( !yw || !yw->_userRobo || yw->IsSpectatorControlled() )
        return;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);
    if ( !robo )
        return;

    vec3d target;
    bool teleport = false;
    if ( !robo->GetPlayerRoboRelocationTarget(&target, &teleport) )
        return;

    const uint8_t worldUiOpacity = yw_GetWorldUiOpacity(yw, target);
    if ( worldUiOpacity == 0 )
        return;

    Common::Point point;
    if ( !yw_ProjectWorldSelectionPoint(yw, target, &point) )
        return;

    // Host Station relocation uses its dedicated canonical marker and remains
    // persistent while a relocation target exists.
    const int radius = 7 + (int)((yw->_timeStamp % 240) * 7 / 240);
    yw_RenderOrderTemplateAt(yw, point, radius, worldUiOpacity,
                             YW_ORDER_TEMPLATE_ROBO_MOVE);
}

void yw_RenderMoveOrderFeedback(NC_STACK_ypaworld *yw)
{
    constexpr uint32_t FEEDBACK_DURATION = 720;

    if ( !yw || !yw->_moveOrderFeedbackActive )
        return;

    uint32_t age = yw->_timeStamp - yw->_moveOrderFeedbackStartTime;
    if ( age >= FEEDBACK_DURATION )
    {
        yw->_moveOrderFeedbackActive = false;
        return;
    }

    const uint8_t worldUiOpacity =
        yw_GetWorldUiOpacity(yw, yw->_moveOrderFeedbackPos);
    if ( worldUiOpacity == 0 )
        return;

    Common::Point point;
    if ( !yw_ProjectWorldSelectionPoint(yw, yw->_moveOrderFeedbackPos, &point) )
        return;

    int radius = 7 + (int)((age % 240) * 7 / 240);
    yw_RenderOrderTemplateAt(yw, point, radius, worldUiOpacity,
                             YW_ORDER_TEMPLATE_MOVE);
}

uint64_t yw_AttackOrderSquadKey(uint32_t commandID, uint8_t owner)
{
    return ((uint64_t)owner << 32) | commandID;
}

TAttackOrderFeedbackEntry *yw_FindAttackOrderFeedback(
        NC_STACK_ypaworld *yw, uint32_t commandID, uint8_t owner)
{
    if ( !yw || commandID == 0 )
        return nullptr;

    const uint64_t key = yw_AttackOrderSquadKey(commandID, owner);
    for ( TAttackOrderFeedbackEntry &entry : yw->_attackOrderFeedbacks )
    {
        if ( yw_AttackOrderSquadKey(entry.SquadCommandID, entry.SquadOwner) == key )
            return &entry;
    }

    return nullptr;
}

void yw_SetAttackOrderFeedback(NC_STACK_ypaworld *yw,
                                      NC_STACK_ypabact *commander,
                                      NC_STACK_ypabact *target)
{
    if ( !yw || !commander || !target )
        return;

    TAttackOrderFeedbackEntry *entry = yw_FindAttackOrderFeedback(
        yw, commander->_commandID, commander->_owner);
    if ( !entry )
    {
        yw->_attackOrderFeedbacks.push_back(TAttackOrderFeedbackEntry());
        entry = &yw->_attackOrderFeedbacks.back();
        entry->SquadCommandID = commander->_commandID;
        entry->SquadOwner = commander->_owner;
    }

    entry->TargetGid = target->_gid;
    entry->StartTime = yw->_timeStamp;
}

void yw_ClearAttackOrderFeedback(NC_STACK_ypaworld *yw,
                                        uint32_t commandID, uint8_t owner)
{
    if ( !yw || commandID == 0 )
        return;

    const uint64_t key = yw_AttackOrderSquadKey(commandID, owner);
    yw->_attackOrderFeedbacks.erase(
        std::remove_if(yw->_attackOrderFeedbacks.begin(),
                       yw->_attackOrderFeedbacks.end(),
                       [key](const TAttackOrderFeedbackEntry &entry)
                       {
                           return yw_AttackOrderSquadKey(entry.SquadCommandID,
                                                         entry.SquadOwner) == key;
                       }),
        yw->_attackOrderFeedbacks.end());
}

bool yw_IsActiveAttackOrderSource(NC_STACK_ypabact *unit)
{
    // A level-preplaced unit can receive and execute an automatic target while
    // it is still leaving CREATE/BEAM during the opening seconds of a mission.
    // The marker follows the real target fields, so only terminal lifetime
    // states are rejected here; rendering state must not hide a valid attacker.
    return unit &&
           !(unit->_oflags & BACT_OFLAG_USERINPT) &&
           unit->_energy > 0 &&
           unit->_status != BACT_STATUS_DEAD &&
           !(unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 |
                                  BACT_STFLAG_CLEAN));
}

bool yw_IsEnemyAttackOrderTarget(NC_STACK_ypaworld *yw,
                                         NC_STACK_ypabact *target)
{
    if ( !yw || !yw->_userRobo || !target ||
         target->_energy <= 0 ||
         target->_status == BACT_STATUS_DEAD ||
         (target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 |
                                BACT_STFLAG_CLEAN)) )
    {
        return false;
    }

    const uint8_t playerOwner = yw->_userRobo->_owner;

    // The strategic map already exposes the real unit target of a player squad
    // while that target is still passing through an opening visibility/render
    // transition. Do not apply IsHiddenFor() or NORENDER here: those extra UI
    // filters can make the world marker disagree with the actual combat order
    // during level-preplaced opening engagements. Cloaked units
    // remain protected until their normal reveal path clears the invisible flag.
    return target->_owner != World::OWNER_0 &&
           target->_owner != playerOwner &&
           !target->ShouldHideFromStrategicUI() &&
           !target->IsInvisibleUnrevealed();
}

void yw_AddAttackOrderTarget(NC_STACK_ypaworld *yw,
                                    NC_STACK_ypabact *target,
                                    std::vector<NC_STACK_ypabact *> *targets)
{
    if ( !targets || !yw_IsEnemyAttackOrderTarget(yw, target) )
        return;

    if ( std::find(targets->begin(), targets->end(), target) == targets->end() )
        targets->push_back(target);
}

bool yw_IsUnitTargetingAttackOrderTarget(NC_STACK_ypabact *unit,
                                                 NC_STACK_ypabact *target)
{
    if ( !yw_IsActiveAttackOrderSource(unit) || !target )
        return false;

    if ( unit->_primTtype == BACT_TGT_TYPE_UNIT && unit->_primT.pbact == target )
        return true;

    if ( unit->_secndTtype == BACT_TGT_TYPE_UNIT && unit->_secndT.pbact == target )
        return true;

    // Ground units can temporarily follow a path toward a unit through the
    // legacy movement target fields while still executing the same attack
    // order. Command IDs are shared by all members of a squad.
    return unit->_m_cmdID == (int)target->_commandID &&
           unit->_m_owner == target->_owner;
}

bool yw_IsSquadTargetingAttackOrderTarget(NC_STACK_ypabact *commander,
                                                  NC_STACK_ypabact *target)
{
    if ( yw_IsUnitTargetingAttackOrderTarget(commander, target) )
        return true;

    if ( !yw_IsActiveAttackOrderSource(commander) )
        return false;

    for ( NC_STACK_ypabact *member : commander->_kidList )
    {
        if ( yw_IsUnitTargetingAttackOrderTarget(member, target) )
            return true;
    }

    return false;
}

void yw_CollectAlliedAttackTargetsFromUnit(
        NC_STACK_ypaworld *yw, NC_STACK_ypabact *unit, uint8_t playerOwner,
        bool trustedPlayerHierarchy,
        std::vector<NC_STACK_ypabact *> *visited,
        std::vector<NC_STACK_ypabact *> *targets)
{
    if ( !unit || !visited || !targets ||
         std::find(visited->begin(), visited->end(), unit) != visited->end() )
    {
        return;
    }

    visited->push_back(unit);

    const bool isPlayerAllied = trustedPlayerHierarchy ||
                                unit->_owner == playerOwner;

    if ( yw_IsActiveAttackOrderSource(unit) &&
         isPlayerAllied &&
         unit->_bact_type != BACT_TYPES_MISSLE )
    {
        if ( unit->_primTtype == BACT_TGT_TYPE_UNIT )
            yw_AddAttackOrderTarget(yw, unit->_primT.pbact, targets);

        if ( unit->_secndTtype == BACT_TGT_TYPE_UNIT )
            yw_AddAttackOrderTarget(yw, unit->_secndT.pbact, targets);

        // Preserve the legacy movement-target phase used while a ground unit
        // is still approaching its unit target.
        if ( unit->_m_cmdID > 0 && unit->_m_owner != playerOwner )
        {
            NC_STACK_ypabact *moveTarget =
                yw->FindBactByCmdOwn((uint32_t)unit->_m_cmdID,
                                     (char)unit->_m_owner);
            yw_AddAttackOrderTarget(yw, moveTarget, targets);
        }
    }

    for ( NC_STACK_ypabact *kid : unit->_kidList )
    {
        yw_CollectAlliedAttackTargetsFromUnit(yw, kid, playerOwner,
                                              trustedPlayerHierarchy,
                                              visited, targets);
    }
}

void yw_CollectAlliedAttackTargetsFromSectorUnits(
        NC_STACK_ypaworld *yw, uint8_t playerOwner,
        std::vector<NC_STACK_ypabact *> *visited,
        std::vector<NC_STACK_ypabact *> *targets)
{
    if ( !yw || !visited || !targets )
        return;

    // Sector lists are the complete live-unit source used elsewhere in the
    // engine for units that are not guaranteed to remain reachable through a
    // Host Station/commander hierarchy, including units placed directly by a
    // level. Scan them all so the marker rule is independent of squad origin;
    // the shared visited list prevents duplicate work and duplicate targets.
    for ( cellArea &cell : yw->Sectors() )
    {
        for ( NC_STACK_ypabact *unit : cell.unitsList )
        {
            yw_CollectAlliedAttackTargetsFromUnit(yw, unit, playerOwner,
                                                  false, visited, targets);
        }
    }
}

void yw_CollectAlliedAttackTargets(
        NC_STACK_ypaworld *yw, std::vector<NC_STACK_ypabact *> *targets)
{
    if ( !yw || !yw->_userRobo || !targets )
        return;

    std::vector<NC_STACK_ypabact *> visited;
    visited.reserve(128);
    targets->reserve(32);

    const uint8_t playerOwner = yw->_userRobo->_owner;

    // The player's Host Station hierarchy is the authoritative ownership tree
    // for its commanders and squad members. _unitsList is not sufficient here:
    // it mostly contains root/major BACTs and can omit regular active units from
    // the traversal used by the world UI. Start from the player Host Station,
    // then also visit the commander remap as a safe fallback for a temporarily
    // detached or reorganising squad. The visited list prevents duplicates.
    yw_CollectAlliedAttackTargetsFromUnit(yw, yw->_userRobo, playerOwner,
                                          true, &visited, targets);

    for ( NC_STACK_ypabact *commander : yw->_cmdrsRemap )
    {
        yw_CollectAlliedAttackTargetsFromUnit(yw, commander, playerOwner,
                                              true, &visited, targets);
    }

    // Some level-preplaced units can be present and fighting in sector lists
    // without being reachable from the current player command hierarchy.
    // Reuse the existing unit-target reader on those live sector units instead
    // of adding a second targeting state or changing AI behavior.
    yw_CollectAlliedAttackTargetsFromSectorUnits(yw, playerOwner,
                                                 &visited, targets);
}

void yw_AddManualAttackOrderGraceTargets(
        NC_STACK_ypaworld *yw, std::vector<NC_STACK_ypabact *> *targets)
{
    constexpr uint32_t TARGET_ASSIGN_GRACE_MS = 1000;

    if ( !yw || !yw->_userRobo || !targets )
        return;

    const uint8_t playerOwner = yw->_userRobo->_owner;
    for ( auto it = yw->_attackOrderFeedbacks.begin();
          it != yw->_attackOrderFeedbacks.end(); )
    {
        NC_STACK_ypabact *target = yw->FindLiveBactByGid(it->TargetGid);
        NC_STACK_ypabact *commander =
            yw->FindBactByCmdOwn(it->SquadCommandID, (char)it->SquadOwner);

        const bool valid = it->SquadOwner == playerOwner &&
                           yw_IsEnemyAttackOrderTarget(yw, target) &&
                           yw_IsActiveAttackOrderSource(commander) &&
                           commander->_owner == it->SquadOwner;
        if ( !valid )
        {
            it = yw->_attackOrderFeedbacks.erase(it);
            continue;
        }

        const uint32_t age = yw->_timeStamp - it->StartTime;
        const bool assigned = yw_IsSquadTargetingAttackOrderTarget(commander,
                                                                   target);
        if ( age <= TARGET_ASSIGN_GRACE_MS || assigned )
        {
            yw_AddAttackOrderTarget(yw, target, targets);
            ++it;
        }
        else
        {
            it = yw->_attackOrderFeedbacks.erase(it);
        }
    }
}

bool yw_ProjectAttackOrderTargetPoint(
        NC_STACK_ypaworld *yw, const vec3d &worldPos, Common::Point *screenPos)
{
    if ( !yw || !screenPos )
        return false;

    vec3d delta = worldPos - yw->_viewerPosition;

    mat3x3 corrected = yw->_viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, false);

    vec3d projected = corrected.Transform(delta);
    if ( projected.z <= 30.0 )
        return false;

    // The ordinary selection helper rejects a target as soon as its centre
    // leaves the viewport. Large/flying units can still be visibly fighting at
    // the screen edge in that state. Give only the Attack marker a small
    // overscan area, then clamp it inside the viewport. Fully off-screen targets
    // remain hidden, avoiding a new permanent edge-indicator system.
    float projectedX = projected.x / projected.z;
    float projectedY = projected.y / projected.z;
    GFX::Engine.viewZoomCorrection(projectedX, projectedY);
    constexpr float VISIBLE_MODEL_OVERSCAN = 1.20f;
    if ( projectedX < -VISIBLE_MODEL_OVERSCAN ||
         projectedX > VISIBLE_MODEL_OVERSCAN ||
         projectedY < -VISIBLE_MODEL_OVERSCAN ||
         projectedY > VISIBLE_MODEL_OVERSCAN )
    {
        return false;
    }

    constexpr float EDGE_MARGIN_X = 0.97f;
    constexpr float EDGE_MARGIN_Y = 0.94f;
    const float x = std::max(-EDGE_MARGIN_X,
                             std::min(EDGE_MARGIN_X, projectedX));
    const float y = std::max(-EDGE_MARGIN_Y,
                             std::min(EDGE_MARGIN_Y, projectedY));

    screenPos->x = (int)((yw->_screenSize.x / 2) * (x + 1.0f));
    screenPos->y = (int)((yw->_screenSize.y / 2) * (y + 1.0f));
    return true;
}

void yw_RenderAttackOrderTargetMarker(
        NC_STACK_ypaworld *yw, NC_STACK_ypabact *target,
        const YwLoadedUiBitmap &bitmap)
{
    if ( !yw || !target || !bitmap.bitmap || !bitmap.bitmap->GetBitmap() )
        return;

    Common::Point point;
    if ( !yw_ProjectAttackOrderTargetPoint(yw, target->_position, &point) )
        return;

    constexpr float ATTACK_MARKER_SCALE = 0.5f;
    const uint32_t pulseTime = yw->_timeStamp % 240;
    const uint32_t triangle = pulseTime <= 120 ? pulseTime : 240 - pulseTime;
    const int baseRadius = 9 + (int)(triangle * 6 / 120);
    const int baseSize = baseRadius * 2 + 8;
    const int size = std::max(8, dround(baseSize * ATTACK_MARKER_SCALE));
    const uint8_t worldUiOpacity = yw_GetWorldUiOpacity(yw, target->_position);
    if ( worldUiOpacity == 0 )
        return;
    const SDL_Color tint = yw_GetUiOwnerTintColor(
        yw, std::max(yw_GetOrderIconOwner(yw), static_cast<int>(World::OWNER_RESIST)));

    if ( bitmap.ownerTint )
    {
        StatusIconRenderBitmapTinted(yw, bitmap.bitmap,
                                     point.x - size / 2 + 1,
                                     point.y - size / 2 + 1,
                                     size + 2, &tint,
                                     (uint8_t)(worldUiOpacity / 3));
        StatusIconRenderBitmapTinted(yw, bitmap.bitmap,
                                     point.x - size / 2,
                                     point.y - size / 2,
                                     size, &tint, worldUiOpacity);
    }
    else
    {
        StatusIconRenderBitmap(yw, bitmap.bitmap,
                               point.x - size / 2 + 1,
                               point.y - size / 2 + 1,
                               size + 2, (uint8_t)(worldUiOpacity / 3));
        StatusIconRenderBitmap(yw, bitmap.bitmap,
                               point.x - size / 2,
                               point.y - size / 2,
                               size, worldUiOpacity);
    }
}

void yw_RenderAttackOrderFeedback(NC_STACK_ypaworld *yw)
{
    if ( !yw || !yw->_userRobo )
        return;

    YwLoadedUiBitmap bitmap = yw_LoadOrderTemplateBitmap(
        yw, YW_ORDER_TEMPLATE_ATTACK);
    if ( !bitmap.bitmap )
        return;

    // The marker reflects actual allied combat state, not only the currently
    // selected squad. Every live enemy targeted by any player-owned unit gets
    // one deduplicated marker. Manual orders keep a short grace entry so the
    // feedback appears immediately before the legacy AI target fields update.
    std::vector<NC_STACK_ypabact *> targets;
    yw_CollectAlliedAttackTargets(yw, &targets);
    yw_AddManualAttackOrderGraceTargets(yw, &targets);

    if ( targets.empty() )
        return;

    for ( NC_STACK_ypabact *target : targets )
        yw_RenderAttackOrderTargetMarker(yw, target, bitmap);
}

int NC_STACK_ypaworld::yw_MouseFindCreationPoint(TClickBoxInf *winp)
{
    const World::TVhclProto &vhcl = _vhclProtos[bzda.field_2DC[bzda.field_8EC]];

    float mouseX = (float)(winp->move.ScreenPos.x - (_screenSize.x / 2)) /
                   (float)(_screenSize.x / 2);
    float mouseY = (float)(winp->move.ScreenPos.y - (_screenSize.y / 2)) /
                   (float)(_screenSize.y / 2);
    GFX::Engine.viewZoomCorrection(mouseX, mouseY, true);

    vec3d v47(mouseX, mouseY, 1.0);
    v47.normalise();

    float v63 = vhcl.radius * 4.0 + 200.0;

    float v62 = 0.0;

    if ( v63 > 0.0 )
    {
        vec3d v51 = v47 * v63;

        while ( v62 < v63 )
        {
            float v60 = 0.0;

            while ( v60 < C_2PI )
            {

                vec3d v55 = v51 + vec3d(v62 * cos(v60),  v62 * sin(v60),  0.0);

                mat3x3 corrected = _viewerRotation;
                GFX::Engine.matrixAspectCorrection(corrected, true);

                vec3d v37 = corrected.Transpose().Transform(v55);

                ypaworld_arg136 arg149;
                arg149.stPos = _viewerPosition;
                arg149.vect = v37;
                arg149.flags = 0;

                ypaworld_func149(&arg149);

                if ( !arg149.isect )
                {
                    float v48 = v37.length();

                    if ( v48 > 0.0 )
                        v37 /= v48;

                    _mouseVector = v37;
                    _mouseSelDistance = v48;

                    return 1;
                }

                v60 += 0.349;
            }

            v62 += v63 * 0.1;
        }
    }

    return 0;
}

void NC_STACK_ypaworld::yw_3D_MouseSelect(TClickBoxInf *winp)
{
    ypaworld_func64__sub21__sub1__sub3__sub0(winp);

    yw_arg150 arg150;
    arg150.field_24 = nullptr;
    arg150.pos = _viewerPosition;
    arg150.field_18 = _mouseVector * _normalVizLimit;
    arg150.unit = _viewerBact;

    ypaworld_arg136 arg149;
    arg149.stPos = arg150.pos;
    arg149.vect = arg150.field_18;
    arg149.flags = 0;

    ypaworld_func149(&arg149);

    ypaworld_func150(&arg150);

    float v27, v28;

    if ( arg149.isect )
    {
        _guiActFlags |= 0x10;

        _mouseSelDistance = (arg149.isectPos - _viewerPosition).length();

        v27 = _mouseSelDistance;
    }

    if ( arg150.field_24 )
    {
        _guiActFlags |= 0x20;

        _mouseSelDistance = arg150.field_28;

        v28 = _mouseSelDistance;
    }

    if ( (_guiActFlags & 0x30) == 0x30 )
    {
        if ( v27 >= v28 )
        {
            _mouseSelDistance = v28;
            _guiActFlags &= ~0x10;
        }
        else
        {
            _mouseSelDistance = v27;
            _guiActFlags &= ~0x20;
        }
    }

    if ( _guiActFlags & 0x10 )
    {
        Common::Point tmp = World::PositionToSectorID(arg149.isectPos);

        _cellOnMouse = &_cells(tmp);
        _cellMouseIsectPos = arg149.isectPos;
    }

    if ( _guiActFlags & 0x20 )
    {
        int v20 = 0;

        if ( arg150.field_24->_status == BACT_STATUS_CREATE || arg150.field_24->_status == BACT_STATUS_DEAD || arg150.field_24->_status == BACT_STATUS_BEAM || arg150.field_24->_bact_type == BACT_TYPES_MISSLE )
            v20 = 1;

        if ( v20 )
            _guiActFlags &= ~0x20;
        else
            _bactOnMouse = arg150.field_24;
    }
}

void NC_STACK_ypaworld::yw_MouseSelect(TInputState *arg)
{
    ypaworld_func64__sub21__sub1__sub0(arg);

    if ( _guiActFlags & 4 )
    {
        if ( _guiActFlags & 8 )
        {
            yw_MAP_MouseSelect(&arg->ClickInf);
        }
        else if ( _guiActFlags & 0x40 )
        {
            yw_SMAN_MouseSelect(&arg->ClickInf);
        }
    }
    else
    {
        yw_3D_MouseSelect(&arg->ClickInf);
    }
}
