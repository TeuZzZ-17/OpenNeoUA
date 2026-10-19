// NC_STACK_ypaflyer: helicopter/aircraft unit.
#ifndef YFLYER_H_INCLUDED
#define YFLYER_H_INCLUDED

#include "nucleus/nucleas.h"
#include "game/ypabact.h"

class NC_STACK_ypaflyer: public NC_STACK_ypabact
{
public:
    virtual size_t Init(IDVList &stak) override;
    virtual size_t Deinit() override;
    virtual size_t SetParameters(IDVList &stak) override;
    virtual void AI_layer3(update_msg *arg) override;
    virtual void User_layer(update_msg *arg) override;
    virtual void Move(move_msg *arg) override;
    virtual size_t SetPosition(bact_arg80 *arg) override;
    virtual void Renew() override;
    virtual void HandBrake(update_msg *arg) override;

    NC_STACK_ypaflyer()
    {
        _flyerBoost = 0.0;
        _flyerType    = 0;
    };
    virtual ~NC_STACK_ypaflyer() {};

    virtual const std::string ClassName() const override {
        return __ClassName;
    };

    enum FLY_ATT
    {
        FLY_ATT_TYPE = 0x80002000
    };

    virtual void setFLY_type(int);

    virtual int getFLY_type();


    void sb_0x4b255c(float a2, vec3d v, int a4);
    void ypaflyer_func70__sub0(float angl);

    // Sblocco degli aerei con collision_shape incastrati contro un ostacolo (ypaflyer_unstuck.cpp)
    void ResetUnstuck();
    void UpdateUnstuck(bool blocked);
    void ApplyUnstuckSteering();
    void ApplyUnstuckThrust();

    //Data
    static constexpr const char * __ClassName = "ypaflyer.class";

    float _flyerBoost = 0.0;
    int _flyerType    = 0;

    enum UnstuckPhase
    {
        UNSTUCK_NONE = 0,
        UNSTUCK_REVERSE,   // retromarcia e salita per staccarsi
        UNSTUCK_TURN       // virata ampia con salita netta
    };

    UnstuckPhase _unstuckPhase = UNSTUCK_NONE;
    bool _unstuckTimerOn = false;     // timer "ostacolo senza progressi" attivo
    int _unstuckSince = 0;            // _clock di partenza del timer
    int _unstuckLastBlocked = 0;      // ultimo _clock con ostacolo davanti
    int _unstuckPhaseEnd = 0;         // _clock di fine della fase corrente
    int _unstuckEndTime = 0;          // _clock di fine dell'ultimo sblocco
    int _unstuckLevel = 0;            // 0 = mai incastrato; sale a ogni ripetizione ravvicinata
    float _unstuckSide = 1.0f;        // lato della virata: +1 sinistra, -1 destra
    vec3d _unstuckAnchor;             // posizione di partenza del timer
    vec3d _unstuckHeading;            // direzione orizzontale congelata all'inizio dello sblocco
};

#endif // YFLYER_H_INCLUDED
