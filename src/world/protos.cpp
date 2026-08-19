#include "protos.h"
#include "../env.h"
#include "../utils.h"
#include "../log.h"
#include "../wav.h"
#include "../ypabact.h"
#include "../yw.h"

namespace World
{
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

    // BACT_TYPES_FLYER cannot distinguish plane/glider/zeppelin, while the
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


void TVhclSound::LoadSamples()
{
    bool hasLoadedVariant = false;
    for (const TSndSample &sample : MainSampleVariants)
    {
        if (sample.Sample)
        {
            hasLoadedVariant = true;
            break;
        }
    }

    if ( !MainSample.Sample && !hasLoadedVariant && (ExtSamples.empty() || !ExtSamples.at(0).Sample) )
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

        if ( extS.empty() )
        {
            for (TSndSample &sample : MainSampleVariants)
            {
                if ( !sample.Name.empty() )
                {
                    sample.Sample = Nucleus::CInit<NC_STACK_wav>( {{NC_STACK_rsrc::RSRC_ATT_NAME, sample.Name}} );

                    if ( !sample.Sample )
                        ypa_log_out("Warning: Could not load sample %s.\n", sample.Name.c_str());
                }
            }
        }

        Common::Env.SetPrefix("rsrc", oldRsrc);
    }
}

void TVhclSound::SetMainSampleVariant(size_t variant, const std::string &name)
{
    if ( variant == 0 )
    {
        MainSample.Name = name;
        return;
    }

    if ( MainSampleVariants.size() < variant )
        MainSampleVariants.resize(variant);

    MainSampleVariants.at(variant - 1).Name = name;
}

void TVhclSound::ClearSounds()
{
    MainSample.ClearLoaded();

    for (TSndSample &sample : MainSampleVariants)
        sample.ClearLoaded();

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
