// Mindcontrol and squad control hand-over.
#include "game/ypabact_internal.h"

bool ypabact_IsUsableControlFallback(NC_STACK_ypabact *bact, NC_STACK_ypabact *dying)
{
    return bact &&
           bact != dying &&
           bact->_status != BACT_STATUS_DEAD &&
           !(bact->_status_flg & BACT_STFLAG_DEATH1) &&
           !bact->IsArtilleryShellPlatform(); // OpenNeoUA: artillery shells are never a control fallback
}

bool ypabact_IsMindcontrolUnitType(NC_STACK_ypabact *bact)
{
    if ( !bact )
        return false;

    if ( bact->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(bact);
        if ( !gun || gun->IsRoboGun() )
            return false;
    }

    switch ( bact->_bact_type )
    {
    case BACT_TYPES_BACT:
    case BACT_TYPES_TANK:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
    case BACT_TYPES_GUN:
        return true;

    default:
        return false;
    }
}

bool ypabact_CanBeMindcontrolled(NC_STACK_ypabact *target, NC_STACK_ypabact *source)
{
    return target &&
           source &&
           target != source &&
           target->getBACT_pWorld() &&
           target->_bact_type != BACT_TYPES_ROBO &&
           target->_bact_type != BACT_TYPES_MISSLE &&
           target->_owner != source->_owner &&
           source->_owner != World::OWNER_0 &&
           target->_energy > 0 &&
           target->_energy_max > 0 &&
           ypabact_IsMindcontrolUnitType(target) &&
           target->_status != BACT_STATUS_DEAD &&
           target->_status != BACT_STATUS_CREATE &&
           target->_status != BACT_STATUS_BEAM &&
           !(target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

NC_STACK_yparobo *ypabact_GetMindcontrolHost(NC_STACK_ypabact *source)
{
    if ( !source )
        return nullptr;

    if ( source->_host_station &&
         source->_host_station->_status != BACT_STATUS_DEAD &&
         !(source->_host_station->_status_flg & BACT_STFLAG_DEATH1) )
        return source->_host_station;

    if ( source->_bact_type == BACT_TYPES_ROBO &&
         source->_status != BACT_STATUS_DEAD &&
         !(source->_status_flg & BACT_STFLAG_DEATH1) )
        return dynamic_cast<NC_STACK_yparobo *>(source);

    return nullptr;
}

void ypabact_ApplyMindcontrol(NC_STACK_ypabact *target, NC_STACK_ypabact *source)
{
    if ( !ypabact_CanBeMindcontrolled(target, source) )
        return;

    NC_STACK_ypaworld *world = target->getBACT_pWorld();
    NC_STACK_yparobo *newHost = ypabact_GetMindcontrolHost(source);
    uint8_t oldOwner = target->_owner;
    uint8_t newOwner = source->_owner;

    if ( world->_userUnit == target || world->_viewerBact == target )
        ypabact_SafeDetachControlFrom(target, world->_userRobo);

    target->_owner = newOwner;
    target->_m_owner = 0;
    target->_killer = nullptr;
    target->_killer_owner = 0;

    if ( oldOwner < world->_countUnitsPerOwner.size() && world->_countUnitsPerOwner[oldOwner] > 0 )
        world->_countUnitsPerOwner[oldOwner]--;

    if ( newOwner < world->_countUnitsPerOwner.size() )
        world->_countUnitsPerOwner[newOwner]++;

    if ( newHost )
    {
        target->_host_station = newHost;
        int commandId = newHost->getROBO_commCount();
        target->_commandID = commandId;
        if ( world->_isNetGame )
            target->_commandID |= newOwner << 24;
        newHost->setROBO_commCount(commandId + 1);

        if ( target->_parent != newHost )
            newHost->AddSubject(target);
    }

    target->_primTtype = BACT_TGT_TYPE_NONE;
    target->_secndTtype = BACT_TGT_TYPE_NONE;
    target->_assess_time = 0;
}

void ypabact_SafeDetachControlFrom(NC_STACK_ypabact *dying, NC_STACK_ypabact *preferredFallback)
{
    NC_STACK_ypaworld *world = dying->getBACT_pWorld();
    if ( !world )
        return;

    bool controlled = world->_userUnit == dying || world->_viewerBact == dying;
    bool hovered = world->_bactOnMouse == dying;

    if ( world->_guiVisor.field_18 == dying )
        world->_guiVisor.field_18 = nullptr;

    world->ClearUserDamageHoverTarget(dying);

    if ( hovered )
    {
        world->_bactOnMouse = nullptr;
        world->_guiActFlags &= ~0x20;
    }

    if ( !controlled )
        return;

    NC_STACK_ypabact *fallback = nullptr;
    if ( ypabact_IsUsableControlFallback(preferredFallback, dying) )
        fallback = preferredFallback;
    else if ( ypabact_IsUsableControlFallback(dying->_parent, dying) && !dying->_parent->ShouldHideFromStrategicUI() )
        fallback = dying->_parent;
    else if ( ypabact_IsUsableControlFallback(world->_userRobo, dying) )
        fallback = world->_userRobo;

    dying->setBACT_inputting(false);
    dying->setBACT_viewer(false);

    if ( fallback )
    {
        fallback->setBACT_inputting(true);
        fallback->setBACT_viewer(true);
        world->_viewerBact = fallback;
        world->setYW_userVehicle(fallback);
    }

    world->_playerInHSGun = false;
}

bool ypabact_IsUsableSquadControlCandidate(NC_STACK_ypabact *candidate,
                                                   NC_STACK_ypabact *dying)
{
    return ypabact_IsUsableControlFallback(candidate, dying) &&
           candidate->_status != BACT_STATUS_CREATE &&
           candidate->_status != BACT_STATUS_BEAM &&
           !candidate->ShouldHideFromStrategicUI();
}

std::vector<NC_STACK_ypabact *> ypabact_GetSquadControlFallbacks(NC_STACK_ypabact *dying)
{
    std::vector<NC_STACK_ypabact *> result;
    if ( !dying )
        return result;

    // Mirror the existing next-unit cycle order. Keep every valid candidate so
    // a long-lived missile camera can skip a member that dies before it ends.
    if ( dying->IsParentMyRobo() )
    {
        for ( NC_STACK_ypabact *candidate : dying->_kidList )
        {
            if ( ypabact_IsUsableSquadControlCandidate(candidate, dying) )
                result.push_back(candidate);
        }
    }
    else
    {
        World::RefBactList *siblings = dying->_kidRef.PList();
        if ( siblings )
        {
            World::RefBactList::iterator it = dying->_kidRef.iter();
            if ( it != siblings->end() )
                ++it;

            for ( ; it != siblings->end(); ++it )
            {
                if ( ypabact_IsUsableSquadControlCandidate(*it, dying) )
                    result.push_back(*it);
            }
        }
    }

    if ( dying->_parent != dying->_host_station &&
         ypabact_IsUsableSquadControlCandidate(dying->_parent, dying) )
    {
        result.push_back(dying->_parent);
    }

    return result;
}

NC_STACK_ypabact *ypabact_GetNextSquadControlFallback(NC_STACK_ypabact *dying)
{
    const std::vector<NC_STACK_ypabact *> candidates =
        ypabact_GetSquadControlFallbacks(dying);
    return candidates.empty() ? nullptr : candidates.front();
}

void NC_STACK_ypabact::PrepareSuicideControlHandoff()
{
    if ( !_world ||
         (_world->_userUnit != this && _world->_viewerBact != this) )
    {
        return;
    }

    // Suicide-only handoff: prefer the next unit in the same squad before the
    // existing safe-detach fallback can return control to the Host Station.
    // Normal combat deaths continue to use their untouched vanilla/OpenNeoUA path.
    ypabact_SafeDetachControlFrom(this, ypabact_GetNextSquadControlFallback(this));

    // setYW_userVehicle() takes effect immediately, while the world's unit loop
    // can still update the newly controlled squad member later in this same frame.
    // Without a release latch that unit consumes the very same held FIRE input,
    // fires, dies, hands off again and can cascade through the entire squad.
    // Arm the actual fallback selected by SafeDetach; normal input resumes as soon
    // as FIRE has been observed released once.
    if ( _world->_userUnit && _world->_userUnit != this )
        _world->_userUnit->_suicide_handoff_wait_fire_release = true;
}
