// Gun recoil stays axial while yaw/elevation change, with no visual tilt.
#include <bits/stdc++.h>
#define protected public
#include "game/ypabact_internal.h"
#include "game/ypagun.h"
#include "game/yw.h"
#undef protected
#undef main
struct RecoilWorld:NC_STACK_ypaworld {
    void ypaworld_func136(ypaworld_arg136 *arg) override {arg->isect=false;}
};
struct RecoilCapture:NC_STACK_base {
    TF::TForm3D observed;int calls=0;
    size_t Render(baseRender_msg *,Instance *,bool) override {observed=TForm();++calls;return 1;}
};
int main() {
    int checks=0,failures=0;
    auto check=[&](bool ok,const char *why){++checks;if(!ok){++failures;if(failures<12)printf("FAIL %s\n",why);}};
    System::IniConf::GameRecoilKickTime.Value=std::string("60");
    System::IniConf::GameRecoilHoldTime.Value=std::string("30");
    System::IniConf::GameRecoilReturnTime.Value=std::string("240");
    RecoilWorld world;NC_STACK_ypabact parent;
    for(int mount:{0,1,2})for(int yaw:{0,30,90,150,240,315})for(int elevation:{-60,-20,0,45,89,90}) {
        RecoilCapture body;
        NC_STACK_ypagun gun;gun._world=&world;gun._bact_type=BACT_TYPES_GUN;
        gun._position=vec3d(200,100,300);gun._clock=1000;
        gun._rotation=mat3x3::RotateX(elevation*C_PI_180)*mat3x3::RotateY(yaw*C_PI_180);
        if(mount){gun._parent=&parent;gun._gunFlags|=NC_STACK_ypagun::GUN_FLAGS_ROBO;}
        if(mount==2)gun._isUnitGunChild=true;
        const auto position=gun._position;const auto rotation=gun._rotation;
        gun.ApplyRecoil(-gun._rotation.AxisZ(),.8f);
        gun._clock=1060;vec3d offset;float pitch=1;
        ypabact_EvaluateRecoilVisual(&gun,&offset,&pitch);
        const auto axis=gun._rotation.AxisZ();
        check((offset*axis).length()<1e-5&&offset.dot(axis)<-1,"gun kick goes straight backwards along its aimed axis");
        check(pitch==0&&ypabact_GetRecoilVisualPitch(&gun)==0,"gun recoil adds no pitch or roll");
        check(gun._position==position&&gun._recoilPushVel.length()<1e-8,"fixed and mounted guns retain their physical anchor");
        check((gun._rotation.AxisX()-rotation.AxisX()).length()<1e-8,"recoil does not change aiming orientation");
        gun._vp_normal=&body;gun._vp_genesis=nullptr;gun.SetVP(&body);
        gun._tForm.Pos=position;gun._tForm.SclRot=rotation.Transpose();baseRender_msg render{};
        gun.Render(&render);
        check(body.calls==1&&(body.observed.Pos-position-offset).length()<1e-6,"production render receives only the axial translation");
        check((body.observed.SclRot.Transform(vec3d(1,2,3))-rotation.Transpose().Transform(vec3d(1,2,3))).length()<1e-6,"production render preserves the authored orientation without recoil tilt");
        const double peak=offset.length();
        gun._clock=1180;
        gun._rotation=mat3x3::RotateX(elevation*C_PI_180)*mat3x3::RotateY((yaw+75)*C_PI_180);
        ypabact_EvaluateRecoilVisual(&gun,&offset,&pitch);
        check((offset*gun._rotation.AxisZ()).length()<1e-5&&offset.dot(gun._rotation.AxisZ())<=0,"return follows the current barrel axis while the turret turns");
        check(offset.length()<peak,"existing smooth envelope returns towards rest");
        gun.ApplyRecoil(-gun._rotation.AxisZ(),.8f);gun._clock+=60;
        ypabact_EvaluateRecoilVisual(&gun,&offset,&pitch);
        check((offset*gun._rotation.AxisZ()).length()<1e-5&&offset.dot(gun._rotation.AxisZ())<0&&pitch==0,"repeated shots never accumulate sideways drift or tilt");
        gun._clock=gun._recoilVisualReturnEndTime;
        ypabact_EvaluateRecoilVisual(&gun,&offset,&pitch);
        check(offset.length()==0&&pitch==0,"gun returns exactly to rest");
        gun._parent=nullptr;
    }
    NC_STACK_ypabact tank;tank._world=&world;tank._bact_type=BACT_TYPES_TANK;tank._status_flg=BACT_STFLAG_LAND;tank._clock=1000;
    tank.ApplyRecoil(vec3d(0,0,-1),.8f);tank._clock=1060;
    check(ypabact_GetRecoilVisualPitch(&tank)>0,"tank visual recoil pitch is preserved");
    NC_STACK_ypagun player;player._world=&world;player._bact_type=BACT_TYPES_GUN;
    player._oflags=BACT_OFLAG_VIEWER|BACT_OFLAG_USERINPT;world._userUnit=world._viewerBact=&player;
    check(!ypabact_ShouldRenderRecoilVisualOffset(&player),"first-person camera does not receive duplicate body recoil");
    player._alternativeViewActive=true;
    check(ypabact_ShouldRenderRecoilVisualOffset(&player),"external alternative view retains the axial gun recoil");
    world._userUnit=world._viewerBact=nullptr;
    printf("GUN_RECOIL CHECKS=%d FAILED=%d\n",checks,failures);return failures?1:0;
}
