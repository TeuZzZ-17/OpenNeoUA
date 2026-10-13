// NC_STACK_ypacar: wheeled/tracked ground vehicle.
#ifndef YCAR_H_INCLUDED
#define YCAR_H_INCLUDED

#include "nucleus/nucleas.h"
#include "game/ypatank.h"

class NC_STACK_ypacar: public NC_STACK_ypatank
{
public:
    virtual size_t Init(IDVList &stak) override;
    virtual size_t Deinit() override;
    virtual size_t SetParameters(IDVList &stak) override;
    virtual void User_layer(update_msg *arg) override;
    virtual int AlignVehicleAI(float dtime, vec3d *pNormal) override;
    virtual int AlignVehicleUser(float dtime, const vec3d &oldDir) override;

    NC_STACK_ypacar();
    virtual ~NC_STACK_ypacar() {};

    virtual const std::string ClassName() const override {
        return __ClassName;
    };

    enum CAR_ATT
    {
        CAR_ATT_KAMIKAZE = 0x80003000,
        CAR_ATT_BLAST = 0x80003001
    };

    virtual void setCAR_kamikaze(bool kam);
    virtual void setCAR_blast(int blast);

    virtual bool getCAR_kamikaze();
    virtual int getCAR_blast();

    vec3d CarTip(float dtime, const vec3d &oldDir, vec3d rot);
    void DoKamikaze();

    //Data
public:
    static constexpr const char * __ClassName = "ypacar.class";

protected:
    bool _carKamikaze;
    int _carBlast;
};

#endif // YCAR_H_INCLUDED
