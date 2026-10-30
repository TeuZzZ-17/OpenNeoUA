// Value helpers shared by the script parser files (parsers_*.cpp).
// Not part of the public parser API: include it only from those files.
#ifndef WORLD_PARSERS_INTERNAL_H_INCLUDED
#define WORLD_PARSERS_INTERNAL_H_INCLUDED

#include <string>
#include <vector>
#include "parsers.h"

namespace World
{
namespace Parsers
{

int ParsePositiveIntOrZero(const std::string &value);
void ParseSoundPitchRange(const std::string &value, TVhclSound &sound);
float ParseNonNegativeIniFloatOrZero(const std::string &value);
bool ParseAbsoluteOrPercent(const std::string &value,
                            TAbsoluteOrPercent &out,
                            float maxPercent = 100.0f);
float ParseMalusPercent(const std::string &value);
float ParseSignedPitchPercent(const std::string &value);
float ParseNormalizedOrPercent(const std::string &value);
bool ParseSmartVPList(const std::string &value, std::vector<int16_t> &out);
float Clamp01(float value);
float NonNegativeFiniteOrZero(float value);
float ClampPushIntensity(float value);
int NonNegativeFiniteMilliseconds(ScriptParser::Parser &parser,
                                  const std::string &value);
float ClampRecoilMultiplier(float value);
float ClampProjectileVisualMotionRadius(float value);
void InitStatusSoundFXDefaults(World::TVhclSound &snd, int defaultVolume);
int ClampSectorPower(int power);
bool IsUsableScriptText(const std::string &text);
void ResetVehicleScaleFX(TVhclProto *vhcl);
bool ReadTuple(const std::string &key,
               const std::string &value,
               size_t count,
               std::string *parts);
bool ParseVPScaleParam(ScriptParser::Parser &parser,
                       const std::string &prefix,
                       const std::string &p1,
                       const std::string &p2,
                       vec3d &scale);
bool ParseVPSpinParam(ScriptParser::Parser &parser,
                      const std::string &prefix,
                      const std::string &p1,
                      const std::string &p2,
                      vec3d &spin);
float ParseFiniteFloatOrFallback(ScriptParser::Parser &parser,
                                 const std::string &value,
                                 float fallback);
bool ParseVPRotationParam(ScriptParser::Parser &parser,
                          const std::string &prefix,
                          const std::string &p1,
                          const std::string &p2,
                          vec3d &rotation);
bool ParseExternalVisualParam(const std::string &p1,
                              const std::string &p2,
                              const std::string &prefix,
                              World::TExternalVisualSet &visuals);
bool ParseScriptIntRange(const std::string &value, int &minValue, int &maxValue);
bool ParseScriptFloatRange(const std::string &value, float &minValue, float &maxValue);
bool ParseNumWeaponsRange(const std::string &value,
                          int &minCount,
                          int &maxCount);
bool ParseMimicEnergyCostRange(const std::string &value,
                               int &minCost,
                               int &maxCost);
bool ParseDecorationFXParam(ScriptParser::Parser &parser,
                            const std::string &p1,
                            const std::string &p2,
                            World::TDecorationFXConfig &config);
bool ParseTintParam(ScriptParser::Parser &parser,
                    const std::string &paramName,
                    const std::string &p1,
                    const std::string &p2,
                    TVisualTint &tint,
                    bool invalidFallsBackToNeutral = false);
bool ParseBoundedIntegerParam(const std::string &paramName,
                              const std::string &p1,
                              const std::string &p2,
                              int minValue,
                              int maxValue,
                              int fallback,
                              int &value);
float ParseBoundedPositiveFiniteOrZero(ScriptParser::Parser &parser,
                                        const std::string &value,
                                        float maximum);
float ParseBoundedNonNegativeFiniteOrFallback(
    ScriptParser::Parser &parser, const std::string &value,
    float maximum, float fallback);
bool ParseMeshTracerParam(ScriptParser::Parser &parser,
                            const std::string &p1,
                            const std::string &p2,
                            World::TWeaponTracerConfig &config,
                            const std::string &prefix);
bool ParseLaserMeshParam(
    ScriptParser::Parser &parser, const std::string &p1,
    const std::string &p2, World::TWeapProto::TLaserMeshConfig &config);
void ParseDecalSizeOverride(const char *key, const std::string &value,
                            TDecalSizeOverride &out);
int ParseVehicleChainFXBlock(ScriptParser::Parser &parser, TVhclProto *vhcl);
int ParseWeaponChainFXBlock(ScriptParser::Parser &parser, TWeapProto *wpn);
int ParseSuperItemChainFXBlock(ScriptParser::Parser &parser, TSuperItemProfile *profile);
bool IsMimicVehicleShellParam(const std::string &p1);
float ParseFiniteCollisionFloat(ScriptParser::Parser &parser,
                                const std::string &value);
bool ParseCompoundCollisionParameter(
 ScriptParser::Parser &parser, const std::string &p1,
 const std::string &p2, rbcolls &coll, int32_t &collID);

}
}

#endif
