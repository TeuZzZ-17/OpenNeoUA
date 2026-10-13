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

    //Data
    static constexpr const char * __ClassName = "ypaflyer.class";

    float _flyerBoost = 0.0;
    int _flyerType    = 0;
};

#endif // YFLYER_H_INCLUDED
