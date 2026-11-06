// Building and mounted radar modules scan continuously, without aiming at units.
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#define protected public
#include "game/ypagun.h"
#undef protected
#undef main

static int checks = 0;
static int failures = 0;

static void Check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::printf("FAIL %s\n", message);
    }
}

static void Prepare(NC_STACK_ypagun &gun, int type, int radar, float speed)
{
    gun._gunType = type;
    gun._radar = radar;
    gun._maxrot = speed;
    gun._status = BACT_STATUS_NORMAL;
    gun._gunFlags = NC_STACK_ypagun::GUN_FLAGS_ROBO;
    gun._secndTtype = BACT_TGT_TYPE_NONE;
    gun.ypagun_func128(vec3d(0, 0, 1), false);
}

static bool Near(const vec3d &a, const vec3d &b, double tolerance = 1e-5)
{
    return (a - b).length() < tolerance;
}

int main()
{
    update_msg second{};
    second.frameTime = 1000;

    NC_STACK_ypagun building;
    Prepare(building, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.35f);
    const mat3x3 initial = building._rotation;
    building.AI_layer3(&second);
    const mat3x3 expected = initial * mat3x3(building._gunRott, 0.35);
    Check(Near(building._rotation.AxisZ(), expected.AxisZ()),
          "building radar rotates by maxrot radians per second without a target");
    Check(std::fabs(building._rotation.AxisZ().y) < 1e-6,
          "building radar has no vertical tilt");

    NC_STACK_ypagun radar;
    Prepare(radar, NC_STACK_ypagun::GUN_TYPE_RADAR, 0, 0.55f);
    radar.AI_layer3(&second);
    Check(!Near(radar._rotation.AxisZ(), vec3d(0, 0, 1)),
          "explicit radar module scans even when its reveal radius is zero");

    NC_STACK_ypagun tracked;
    Prepare(tracked, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.35f);
    tracked._secndTtype = BACT_TGT_TYPE_CELL;
    tracked.AI_layer2(&second);
    Check(!Near(tracked._rotation.AxisZ(), vec3d(0, 0, 1)) &&
          tracked._secndTtype == BACT_TGT_TYPE_NONE,
          "radar drops any old secondary target and keeps scanning");

    NC_STACK_ypagun attached;
    Prepare(attached, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.2f);
    attached._isUnitGunChild = true;
    attached.AI_layer3(&second);
    Check(!Near(attached._rotation.AxisZ(), vec3d(0, 0, 1)),
          "a mounted radar scans rather than remaining inert");

    NC_STACK_ypagun tiltedMount;
    Prepare(tiltedMount, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.2f);
    tiltedMount._isUnitGunChild = true;
    const mat3x3 parentDelta = mat3x3::RotateX(0.6);
    tiltedMount._rotation.SetX(parentDelta.Transform(tiltedMount._rotation.AxisX()));
    tiltedMount._rotation.SetY(parentDelta.Transform(tiltedMount._rotation.AxisY()));
    tiltedMount._rotation.SetZ(parentDelta.Transform(tiltedMount._rotation.AxisZ()));
    tiltedMount._gunRott = parentDelta.Transform(tiltedMount._gunRott);
    tiltedMount._gunBasis = parentDelta.Transform(tiltedMount._gunBasis);
    const mat3x3 mountedExpected = tiltedMount._rotation * mat3x3(tiltedMount._gunRott, 0.2);
    tiltedMount.AI_layer3(&second);
    Check(Near(tiltedMount._rotation.AxisZ(), mountedExpected.AxisZ()) &&
          std::fabs(tiltedMount._rotation.AxisZ().dot(tiltedMount._gunRott)) < 1e-6,
          "a tilted carrier retains a radar scan in its mounting plane");

    NC_STACK_ypagun player;
    Prepare(player, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.2f);
    player.User_layer(&second);
    Check(!Near(player._rotation.AxisZ(), vec3d(0, 0, 1)),
          "the radar keeps scanning in the user-controlled path");

    NC_STACK_ypagun tilted;
    Prepare(tilted, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.35f);
    tilted._rotation = mat3x3::RotateX(0.4) * tilted._rotation;
    tilted.AI_layer3(&second);
    Check(std::fabs(tilted._rotation.AxisZ().dot(tilted._gunRott)) < 1e-6,
          "a previously tilted radar returns to its mounting plane");

    NC_STACK_ypagun stoppedTilted;
    Prepare(stoppedTilted, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.0f);
    stoppedTilted._rotation = mat3x3::RotateX(0.4) * stoppedTilted._rotation;
    stoppedTilted.AI_layer3(&second);
    Check(std::fabs(stoppedTilted._rotation.AxisZ().dot(stoppedTilted._gunRott)) < 1e-6,
          "a stopped radar also clears old vertical tilt without beginning to scan");

    NC_STACK_ypagun whole;
    NC_STACK_ypagun split;
    Prepare(whole, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.35f);
    Prepare(split, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.35f);
    whole.AI_layer3(&second);
    update_msg small{};
    small.frameTime = 10;
    for (int i = 0; i < 100; ++i) split.AI_layer3(&small);
    Check(Near(whole._rotation.AxisZ(), split._rotation.AxisZ()),
          "scan speed is independent of frame subdivision");

    NC_STACK_ypagun fullCircle;
    Prepare(fullCircle, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.55f);
    for (int i = 0; i < 20; ++i) fullCircle.AI_layer3(&second);
    const mat3x3 afterTurns = mat3x3::Ident() * mat3x3(fullCircle._gunRott, 11.0);
    Check(Near(fullCircle._rotation.AxisZ(), afterTurns.AxisZ(), 1e-4),
          "radar continues through 360 degrees without reversing or stopping");

    for (float speed : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity()})
    {
        NC_STACK_ypagun stopped;
        Prepare(stopped, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, speed);
        stopped.AI_layer3(&second);
        Check(Near(stopped._rotation.AxisZ(), vec3d(0, 0, 1)),
              "zero and invalid maxrot values do not rotate or corrupt the model");
    }

    NC_STACK_ypagun paused;
    Prepare(paused, NC_STACK_ypagun::GUN_TYPE_RADAR, 15, 0.5f);
    update_msg noTime{};
    paused.AI_layer3(&noTime);
    Check(Near(paused._rotation.AxisZ(), vec3d(0, 0, 1)),
          "a zero-duration frame does not move the radar");

    for (int type : {NC_STACK_ypagun::GUN_TYPE_REAL, NC_STACK_ypagun::GUN_TYPE_PROTO,
                     NC_STACK_ypagun::GUN_TYPE_POWER, NC_STACK_ypagun::GUN_TYPE_DUMMY})
    {
        NC_STACK_ypagun other;
        Prepare(other, type, 15, 0.5f);
        other.AI_layer3(&second);
        Check(Near(other._rotation.AxisZ(), vec3d(0, 0, 1)),
              "non-radar gun types do not scan even when radar radius is positive");
    }

    NC_STACK_ypabact target;
    target._position = vec3d(100, 0, 100);
    NC_STACK_ypagun vanillaDummy;
    Prepare(vanillaDummy, NC_STACK_ypagun::GUN_TYPE_DUMMY, 15, 0.35f);
    vanillaDummy._gunMaxSide = 3.2f;
    vanillaDummy._gunMaxUp = vanillaDummy._gunMaxDown = 1.6f;
    vanillaDummy._secndTtype = BACT_TGT_TYPE_UNIT;
    vanillaDummy._secndT.pbact = &target;
    vanillaDummy.AI_layer3(&second);
    Check(!Near(vanillaDummy._rotation.AxisZ(), vec3d(0, 0, 1)),
          "a dummy gun with radar range still tracks its target through the vanilla gun path");
    vanillaDummy._secndTtype = BACT_TGT_TYPE_NONE;
    vanillaDummy._secndT.pbact = nullptr;

    std::printf("RADAR ROTATION CHECKS=%d FAILURES=%d\n", checks, failures);
    return failures ? 1 : 0;
}
