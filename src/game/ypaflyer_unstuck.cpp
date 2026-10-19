// NC_STACK_ypaflyer: sblocco degli aerei incastrati contro un ostacolo (solo con collision_shape).
#include <algorithm>
#include <math.h>
#include "game/ypaflyer.h"

namespace
{
// Tempo con ostacolo davanti e senza progressi, prima che parta lo sblocco.
constexpr int STUCK_DETECT_MS = 3000;
// Pausa senza ostacolo oltre la quale il timer riparte da zero.
constexpr int STUCK_CLEAR_MS = 1000;
// Un nuovo incastro entro questo tempo dopo uno sblocco rende la manovra piu' decisa.
constexpr int STUCK_ESCALATE_MS = 12000;
constexpr int STUCK_MAX_LEVEL = 3;

// "Nessun progresso" = meno del 15% della distanza che farebbe a piena velocita' nella finestra.
// Il limite tiene conto di aerei lenti (es. Zeppelin) e li protegge dai falsi positivi.
constexpr float STUCK_PROGRESS_FRACTION = 0.15f;
constexpr float STUCK_PROGRESS_MIN = 30.0f;
constexpr float STUCK_PROGRESS_MAX = 150.0f;

// Fase 1: retromarcia (e salita) per staccarsi dall'ostacolo.
constexpr int REVERSE_MS = 1000;
constexpr int REVERSE_MS_PER_LEVEL = 500;
constexpr float REVERSE_THRUST = 0.6f;     // frazione di _force, all'indietro
constexpr float REVERSE_CLIMB = 0.5f;      // pendenza verso l'alto del target

// Fase 2: virata ampia, tutta spinta e salita netta.
constexpr int TURN_MS = 2500;
constexpr int TURN_MS_PER_LEVEL = 1250;
constexpr float TURN_ANGLE_DEG = 90.0f;    // cresce di 30 gradi a ogni livello
constexpr float TURN_ANGLE_DEG_PER_LEVEL = 30.0f;
constexpr float TURN_CLIMB = 0.8f;

// Spinta verticale (multiplo del peso) durante tutto lo sblocco; come il flag UPWRD.
constexpr float CLIMB_BOOST = 2.5f;
}

void NC_STACK_ypaflyer::ResetUnstuck()
{
    _unstuckPhase = UNSTUCK_NONE;
    _unstuckTimerOn = false;
    _unstuckLevel = 0;
}

void NC_STACK_ypaflyer::UpdateUnstuck(bool blocked)
{
    // Senza hull il comportamento resta quello vanilla (rimbalzo e micro-virate).
    if ( !HasCollisionShape() )
        return;

    if ( _unstuckPhase != UNSTUCK_NONE )
    {
        if ( _clock < _unstuckPhaseEnd )
            return;

        if ( _unstuckPhase == UNSTUCK_REVERSE )
        {
            _unstuckPhase = UNSTUCK_TURN;
            _unstuckPhaseEnd = _clock + TURN_MS + TURN_MS_PER_LEVEL * (_unstuckLevel - 1);
            return;
        }

        // Fine manovra: si torna al comportamento normale con un timer nuovo.
        _unstuckPhase = UNSTUCK_NONE;
        _unstuckTimerOn = false;
        _unstuckEndTime = _clock;
        return;
    }

    if ( !blocked )
    {
        if ( _unstuckTimerOn && _clock - _unstuckLastBlocked > STUCK_CLEAR_MS )
            _unstuckTimerOn = false;
        return;
    }

    _unstuckLastBlocked = _clock;

    // Distanza minima da percorrere nella finestra per non essere considerato fermo.
    float topSpeed = _airconst_static > 0.0f ? _force / _airconst_static : 0.0f;
    float progress = topSpeed * 6.0f * (STUCK_DETECT_MS / 1000.0f) * STUCK_PROGRESS_FRACTION;
    progress = std::max(STUCK_PROGRESS_MIN, std::min(progress, STUCK_PROGRESS_MAX));

    if ( !_unstuckTimerOn || (_position - _unstuckAnchor).length() > progress )
    {
        _unstuckTimerOn = true;
        _unstuckSince = _clock;
        _unstuckAnchor = _position;
        return;
    }

    if ( _clock - _unstuckSince < STUCK_DETECT_MS )
        return;

    // Incastrato: parte la manovra. Un ripetersi ravvicinato cambia lato e la rende piu' lunga.
    const bool repeated = _unstuckLevel > 0 && _clock - _unstuckEndTime <= STUCK_ESCALATE_MS;
    if ( repeated )
    {
        _unstuckLevel = std::min(_unstuckLevel + 1, STUCK_MAX_LEVEL);
        _unstuckSide = -_unstuckSide;
    }
    else
    {
        _unstuckLevel = 1;
        // Si prova il lato opposto a quello che la schivata normale non e' riuscita a superare.
        _unstuckSide = (_status_flg & BACT_STFLAG_DODGE_LEFT) ? -1.0f : 1.0f;
    }

    _unstuckHeading = _rotation.AxisZ().X0Z();
    if ( _unstuckHeading.normalise() < 0.001f )
        _unstuckHeading = vec3d::OZ(1.0);

    _unstuckPhase = UNSTUCK_REVERSE;
    _unstuckPhaseEnd = _clock + REVERSE_MS + REVERSE_MS_PER_LEVEL * (_unstuckLevel - 1);
}

void NC_STACK_ypaflyer::ApplyUnstuckSteering()
{
    if ( _unstuckPhase == UNSTUCK_NONE )
        return;

    vec3d dir = _unstuckHeading;
    float climb = REVERSE_CLIMB;

    if ( _unstuckPhase == UNSTUCK_TURN )
    {
        // Rotazione positiva = sinistra, come la schivata DODGE_LEFT.
        const float angle = _unstuckSide * (TURN_ANGLE_DEG + TURN_ANGLE_DEG_PER_LEVEL * (_unstuckLevel - 1)) *
                            (float)(M_PI / 180.0);
        const float c = cosf(angle);
        const float s = sinf(angle);
        dir = vec3d(_unstuckHeading.x * c - _unstuckHeading.z * s, 0.0, _unstuckHeading.x * s + _unstuckHeading.z * c);
        climb = TURN_CLIMB;
    }

    // In UA Y cresce verso il basso: negativo = salita.
    _target_dir = vec3d(dir.x, -climb, dir.z);
    _target_dir.normalise();
}

void NC_STACK_ypaflyer::ApplyUnstuckThrust()
{
    if ( _unstuckPhase == UNSTUCK_NONE )
        return;

    _flyerBoost = _mass * CLIMB_BOOST * 9.80665;

    if ( _unstuckPhase == UNSTUCK_REVERSE )
        _thraction = -_force * REVERSE_THRUST;
}
