#include "protos.h"
#include "../env.h"
#include "../utils.h"
#include "../log.h"
#include "../wav.h"
#include "../ypabact.h"
#include "../yw.h"

#include <algorithm>
#include <cstdlib>

namespace World
{
int TWeapProto::RollLifeTime() const
{
    int minLifeTime = life_time_min;
    int maxLifeTime = life_time_max;

    // Keep prototypes created by older callers/source snapshots compatible:
    // before the range fields existed, life_time was the only source value.
    if ( minLifeTime == 0 && maxLifeTime == 0 && life_time != 0 )
    {
        minLifeTime = life_time;
        maxLifeTime = life_time;
    }

    if ( maxLifeTime < minLifeTime )
        std::swap(minLifeTime, maxLifeTime);

    if ( minLifeTime == maxLifeTime )
        return minLifeTime;

    const int64_t span = (int64_t)maxLifeTime -
                         (int64_t)minLifeTime + 1LL;
    const double randomPart = (double)rand() / ((double)RAND_MAX + 1.0);
    const int64_t offset = (int64_t)(randomPart * (double)span);
    return (int)((int64_t)minLifeTime + offset);
}

int TVhclProto::RollMimicProductionCost()
{
    if ( mimic_energy_cost_min <= 0 || mimic_energy_cost_max <= 0 )
    {
        mimic_energy_cost = 0;
        return energy;
    }

    if ( mimic_energy_cost_min == mimic_energy_cost_max )
    {
        mimic_energy_cost = mimic_energy_cost_min;
        return mimic_energy_cost;
    }

    const long long span = (long long)mimic_energy_cost_max -
                           (long long)mimic_energy_cost_min + 1LL;
    const double randomPart = (double)rand() / ((double)RAND_MAX + 1.0);
    mimic_energy_cost = mimic_energy_cost_min +
                        (int)(randomPart * (double)span);
    return mimic_energy_cost;
}

void TVhclProto::GetWeaponProjectileCountRange(int sourceSlot,
                                               int &minCount,
                                               int &maxCount) const
{
    int rawMin = num_weapons_min;
    int rawMax = num_weapons_max;

    // Backward compatibility for prototypes populated by older code that only
    // knows the scalar field. Authored zero still means the vanilla one-shot
    // behavior after normalization below.
    if ( rawMin == 0 && rawMax == 0 && num_weapons != 0 )
        rawMin = rawMax = num_weapons;

    if ( sourceSlot > 0 && sourceSlot <= (int)extra_num_weapons.size() )
    {
        const size_t index = (size_t)(sourceSlot - 1);
        const int extraScalar = extra_num_weapons[index];
        const int extraMin = extra_num_weapons_min[index];
        const int extraMax = extra_num_weapons_max[index];

        // A completely empty/zero extra count inherits the primary range.
        if ( extraScalar != 0 || extraMin != 0 || extraMax != 0 )
        {
            rawMin = extraMin;
            rawMax = extraMax;
            if ( rawMin == 0 && rawMax == 0 && extraScalar > 0 )
                rawMin = rawMax = extraScalar;
        }
    }

    rawMin = std::max(1, std::min(rawMin, 255));
    rawMax = std::max(1, std::min(rawMax, 255));
    if ( rawMax < rawMin )
        std::swap(rawMin, rawMax);

    minCount = rawMin;
    maxCount = rawMax;
}

VehicleCombatClass ResolveVehicleCombatClass(const NC_STACK_ypabact *unit)
{
    if ( !unit )
        return VEHICLE_COMBAT_CLASS_UNKNOWN;

    NC_STACK_ypabact *mutableUnit = const_cast<NC_STACK_ypabact *>(unit);
    NC_STACK_ypaworld *world = mutableUnit->getBACT_pWorld();
    if ( world )
    {
        const std::vector<TVhclProto> &protos = world->GetVhclProtos();
        const uint8_t protoId = unit->_mimic_disguise_vehicleID
                              ? unit->_mimic_disguise_vehicleID
                              : unit->_vehicleID;
        if ( protoId < protos.size() )
        {
            const VehicleCombatClass authored = protos.at(protoId).combat_class;
            if ( authored != VEHICLE_COMBAT_CLASS_UNKNOWN )
                return authored;
        }
    }

    switch ( unit->_bact_type )
    {
    case BACT_TYPES_BACT: return VEHICLE_COMBAT_CLASS_HELI;
    case BACT_TYPES_TANK: return VEHICLE_COMBAT_CLASS_TANK;
    case BACT_TYPES_UFO:  return VEHICLE_COMBAT_CLASS_UFO;
    case BACT_TYPES_CAR:  return VEHICLE_COMBAT_CLASS_CAR;
    case BACT_TYPES_ROBO: return VEHICLE_COMBAT_CLASS_ROBO;
    case BACT_TYPES_GUN:  return VEHICLE_COMBAT_CLASS_GUN;

    // BACT_TYPES_FLYER cannot distinguish plane/cruiser/glider/zeppelin, while the
    // the legacy ZEPP value is explicitly marked as having no real class.
    // Real scripted vehicles are resolved from their authored prototype above;
    // untyped/helper actors stay UNKNOWN instead of receiving a false matchup.
    default:              return VEHICLE_COMBAT_CLASS_UNKNOWN;
    }
}

bool TryGetSpecificFightJob(const TVhclProto &proto,
                            VehicleCombatClass targetClass,
                            int *outValue)
{
    int value = 0;
    bool defined = false;

    switch ( targetClass )
    {
    case VEHICLE_COMBAT_CLASS_PLANE:
        value = proto.job_fightplane;
        defined = proto.job_fightplane_defined;
        break;
    case VEHICLE_COMBAT_CLASS_CRUISER:
        value = proto.job_fightcruiser;
        defined = proto.job_fightcruiser_defined;
        break;
    case VEHICLE_COMBAT_CLASS_GLIDER:
        value = proto.job_fightglider;
        defined = proto.job_fightglider_defined;
        break;
    case VEHICLE_COMBAT_CLASS_ZEPPELIN:
        value = proto.job_fightzeppelin;
        defined = proto.job_fightzeppelin_defined;
        break;
    case VEHICLE_COMBAT_CLASS_UFO:
        value = proto.job_fightufo;
        defined = proto.job_fightufo_defined;
        break;
    case VEHICLE_COMBAT_CLASS_CAR:
        value = proto.job_fightcar;
        defined = proto.job_fightcar_defined;
        break;
    case VEHICLE_COMBAT_CLASS_GUN:
        value = proto.job_fightgun;
        defined = proto.job_fightgun_defined;
        break;
    default:
        break;
    }

    if ( !defined )
        return false;

    if ( outValue )
        *outValue = value;
    return true;
}

bool TryGetSpecificWeaponEnergy(const TWeapProto &proto,
                                VehicleCombatClass targetClass,
                                float *outValue)
{
    float value = 1.0f;
    bool defined = false;

    switch ( targetClass )
    {
    case VEHICLE_COMBAT_CLASS_PLANE:
        value = proto.energy_plane;
        defined = proto.energy_plane_defined;
        break;
    case VEHICLE_COMBAT_CLASS_CRUISER:
        value = proto.energy_cruiser;
        defined = proto.energy_cruiser_defined;
        break;
    case VEHICLE_COMBAT_CLASS_GLIDER:
        value = proto.energy_glider;
        defined = proto.energy_glider_defined;
        break;
    case VEHICLE_COMBAT_CLASS_ZEPPELIN:
        value = proto.energy_zeppelin;
        defined = proto.energy_zeppelin_defined;
        break;
    case VEHICLE_COMBAT_CLASS_UFO:
        value = proto.energy_ufo;
        defined = proto.energy_ufo_defined;
        break;
    case VEHICLE_COMBAT_CLASS_CAR:
        value = proto.energy_car;
        defined = proto.energy_car_defined;
        break;
    case VEHICLE_COMBAT_CLASS_GUN:
        value = proto.energy_gun;
        defined = proto.energy_gun_defined;
        break;
    default:
        break;
    }

    if ( !defined )
        return false;

    if ( outValue )
        *outValue = value;
    return true;
}

static int SampleFrameSize(ALenum format)
{
    switch (format)
    {
        case AL_FORMAT_MONO8:
            return 1;

        case AL_FORMAT_STEREO8:
        case AL_FORMAT_MONO16:
            return 2;

        case AL_FORMAT_STEREO16:
            return 4;

        default:
            return 1;
    }
}

uint8_t DestFX::ParseTypeName(const std::string &in)
{
    if ( !StriCmp(in, "death") )
        return FX_DEATH;

    if ( !StriCmp(in, "megadeth") )
        return FX_MEGADETH;

    if ( !StriCmp(in, "create") )
        return FX_CREATE;

    if ( !StriCmp(in, "beam") )
        return FX_BEAM;

    return FX_NONE;
}


void TVhclSound::SetPitchRange(int minPitch, int maxPitch)
{
    pitch_min = std::min(minPitch, maxPitch);
    pitch_max = std::max(minPitch, maxPitch);
}

void TVhclSound::ConfigureSoundSourcePitch(TSoundSource &sound) const
{
    sound.ConfigurePitchRange(pitch_min, pitch_max);
}

void TVhclSound::LoadSamples()
{
    if ( !MainSample.Sample && (ExtSamples.empty() || !ExtSamples.at(0).Sample) )
    {
        std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");

        if ( !extS.empty() )
        {
            for (size_t i = 0; i < extS.size(); i++)
            {
                TSampleParams &pprm = extS.at(i);

                ExtSamples.at(i).Sample = Nucleus::CInit<NC_STACK_wav>( {{NC_STACK_rsrc::RSRC_ATT_NAME, ExtSamples.at(i).Name}} );

                if ( ExtSamples.at(i).Sample )
                {
                    TSampleData *sample = ExtSamples.at(i).Sample->GetSampleData();
                    int frameSize = SampleFrameSize(sample->Format);

                    pprm.Sample = sample;
                    pprm.rlOffset = (sample->SampleRate * pprm.Offset / 11000) * frameSize;
                    pprm.rlSmplCnt = (sample->SampleRate * pprm.SampleCnt / 11000) * frameSize;

                    if ( pprm.rlOffset > sample->bufsz )
                        pprm.rlOffset = sample->bufsz;

                    if ( !pprm.rlSmplCnt )
                        pprm.rlSmplCnt = sample->bufsz;

                    if ( pprm.rlSmplCnt + pprm.rlOffset > sample->bufsz )
                        pprm.rlSmplCnt = sample->bufsz - pprm.rlOffset;
                }
                else
                {
                    ypa_log_out("Warning: Could not load sample %s.\n", ExtSamples.at(i).Name.c_str());
                }
            }
        }
        else if ( !MainSample.Name.empty() )
        {
            MainSample.Sample = Nucleus::CInit<NC_STACK_wav>( {{NC_STACK_rsrc::RSRC_ATT_NAME, MainSample.Name}} );

            if ( !MainSample.Sample )
                ypa_log_out("Warning: Could not load sample %s.\n", MainSample.Name.c_str());
        }

        Common::Env.SetPrefix("rsrc", oldRsrc);
    }
}

void TVhclSound::ClearSounds()
{
    MainSample.ClearLoaded();

    for (TSndSample &sample : ExtSamples)
        sample.ClearLoaded();

    for (TSampleParams &fragment : extS)
    {
        fragment.Sample = NULL;
        fragment.rlOffset = 0;
        fragment.rlSmplCnt = 0;
    }
}


TVhclProto::~TVhclProto()
{
    if ( wireframe )
    {
        wireframe->Delete();
        wireframe = NULL;
    }

    if ( hud_wireframe )
    {
        hud_wireframe->Delete();
        hud_wireframe = NULL;
    }

    if ( mg_wireframe )
    {
        mg_wireframe->Delete();
        mg_wireframe = NULL;
    }

    if ( mgun_wireframe )
    {
        mgun_wireframe->Delete();
        mgun_wireframe = NULL;
    }

    if ( wpn_wireframe_1 )
    {
        wpn_wireframe_1->Delete();
        wpn_wireframe_1 = NULL;
    }

    if ( wpn_wireframe_2 )
    {
        wpn_wireframe_2->Delete();
        wpn_wireframe_2 = NULL;
    }

    Common::DeleteAndNull(&RoboProto);
}

TWeapProto::~TWeapProto()
{
    if ( wireframe )
    {
        wireframe->Delete();
        wireframe = NULL;
    }
}

}
