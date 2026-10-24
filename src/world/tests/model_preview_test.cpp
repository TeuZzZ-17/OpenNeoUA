// Render-only previews must be safe before a mission has supplied any models.
#include <bits/stdc++.h>
#define protected public
#define private public
#include "game/model_preview.h"
#include "game/yw.h"
#include "nucleus/particle.h"
#include "system/inivals.h"
#include "common/global.h"
#undef protected
#undef private
#include <cstdio>
#undef main

struct PreviewCaptureBase:NC_STACK_base
{
    baseRender_msg captured;
    TF::TForm3D observed;
    size_t Render(baseRender_msg *arg,Instance *,bool) override
    {captured=*arg;observed=TForm();return 1;}
};

int main()
{
    std::setvbuf(stdout,nullptr,_IONBF,0);
    NC_STACK_ypaworld world;
    ModelPreview preview;
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
    };
    check(!preview.Select(world, world, 0, 4), "undefined vehicle is unavailable");
    check(!preview.Select(world, world, 1, -1), "negative weapon is unavailable");
    check(!preview.SelectSector(world, 256), "invalid sector is unavailable");
    world._vhclProtos.resize(2);
    world._vhclProtos[1].Index = 1;
    world._vhclProtos[1].vp_normal = 300;
    check(!preview.Select(world, world, 0, 1), "missing numeric visual does not throw");
    check(world._unitsList.empty(), "selection never creates live actors");
    preview.Clear();
    check(!preview.Available(), "clear releases preview instances");
    PreviewCaptureBase body;
    world._vhclModels.push_back(&body);
    world._vhclProtos[1].vp_normal = 0;
    check(preview.Select(world,world,0,1), "valid model is assembled without a mission");
    check(preview.PartCount()==1, "one instance is owned per body");
    world._vhclProtos[1].model_id=BACT_TYPES_ROBO;
    World::TRoboProto robo;
    World::TRoboGun mount; mount.robo_gun_type=1;
    robo.guns.push_back(mount);world._vhclProtos[1].RoboProto=&robo;
    check(preview.Select(world,world,0,1)&&preview.PartCount()==2,
          "mounted visual can safely share the parent's BASE");
    preview.Clear();world._vhclProtos[1].RoboProto=nullptr;
    world._vhclProtos[1].model_id=BACT_TYPES_TANK;
    body.Meshes.emplace_back();
    GFX::TVertex first,last;
    first.Pos=vec3f(100,200,300);last.Pos=vec3f(120,240,360);
    body.Meshes.back().Vertexes={first,last};
    check(preview.Select(world,world,0,1)&&preview.center_==vec3d(110,220,330),
          "off-origin geometry is centered without changing the source model");
    check(std::abs(preview.Radius()-std::sqrt(1400.0))<0.01,"framing follows visible bounds");
    world._weaponProtos.resize(2);
    auto &weapon=world._weaponProtos[1];
    weapon.vp_normal=0;weapon.visual_scale=vec3d(2,3,4);
    weapon.visual_tint.r=0.2f;weapon.vp_trail_tint.g=0.3f;
    weapon.vp_trail_scale=vec3d(5,6,0.4);weapon.vp_trail_spin=vec3d(1,2,3);
    check(preview.Select(world,world,1,1),"weapon appearance is assembled from its prototype");
    const auto saved=body.TForm();baseRender_msg arg;arg.frameTime=20;
    preview.Render(arg,vec3d(0,0,500),0,0);
    check(body.captured.tint.r==0.2f&&body.captured.particleTint.g==0.3f,
          "main and trail keep their independent authored tints");
    check(body.captured.particleScale==vec3d(5,6,1)&&std::abs(body.captured.particleLifetimeScale-0.4f)<0.001&&
          body.captured.particleSpin==vec3d(1,2,3),"trail width height lifetime and spin reach the real renderer");
    check(body.TForm().Pos==saved.Pos,"render restores the shared model position");
    check(body.observed.SclRot.Transform(vec3d(1,1,1))==vec3d(2,3,4),"per-axis model scale reaches the renderer");
    for(const auto position:{vec3d(0,-500,1000),vec3d(500,-500,1000),vec3d(-500,500,1000)})
    {
        auto rotation=ModelPreview::ViewOrientation(position,0,-18,true);
        check(rotation.Transform(vec3d(0,-1,0)).dot(position)<0,
              "menu viewing ray exposes the top at every screen position");
    }
    PreviewCaptureBase decoration;
    world._vhclModels.push_back(&decoration);
    weapon.decoration_fx.mode=World::DECORATION_FX_PERSISTENT;
    weapon.decoration_fx.vp=1;weapon.decoration_fx.tint.a=0.0f;
    weapon.decoration_fx.vp_trail_tint.r=0.4f;
    weapon.decoration_fx.vp_trail_scale=vec3d(4,5,0.6);
    weapon.decoration_fx.scale=vec3d(2,2,2);
    weapon.decoration_fx.offset=vec3d(0,0,-20);
    preview.Select(world,world,1,1);preview.Render(arg,vec3d(0,0,500),0,0);
    check(decoration.captured.tint.a==0.0f&&decoration.captured.particleTint.r==0.4f&&
          decoration.captured.particleTintAlphaAffectsAdditive,"decoration body and additive particles use independent controls");
    check(decoration.captured.particleScale==vec3d(4,5,1)&&std::abs(decoration.captured.particleLifetimeScale-0.6f)<0.001,
          "decoration trail keeps its own scale and lifetime");
    check(world._transientVPs.empty()&&world._unitsList.empty(),"preview effects never enter mission actor or effect lists");
    weapon.spiral_speed=1;weapon.spiral_radius=30;
    preview.Select(world,world,1,1);preview.Render(arg,vec3d(0,0,500),0,0);
    check(body.observed.Pos.x+preview.center_.x>20,"prototype spiral uses the existing visual-motion evaluator");
    auto *menuParticles=body.captured.particleSystem;
    preview.Render(arg,vec3d(0,0,500),0,0,false,false);
    check(body.observed.SclRot.Transform(vec3d(1,1,1))==vec3d(1,1,1)&&body.captured.tint.r==1.0f&&
          body.captured.particleScale==vec3d(1.0),"legacy briefing render keeps its unmodified transform and particle controls");
    NC_STACK_particle emitter;
    emitter._genLifeTime=emitter._genEnd=1000;emitter._genStart=0;emitter._genPause=1000;
    emitter._pointID=0;emitter._startSpeed=1;
    emitter._magnifyStart=vec3d(1,0,0);emitter._magnifyDelta=vec3d();
    UAskeleton::Data skeleton;skeleton.POO.push_back(vec3d());
    TF::TForm3D transform;transform.SclRot=mat3x3::Ident();transform.Scale=vec3d(1.0);transform.CalcGlobal();
    World::ParticleSystem isolated;
    area_arg_65 emission;emission.OwnerTForm=&transform;emission.sklt=&skeleton;
    emission.frameTime=1;emission.particleSystem=&isolated;
    System::IniConf::GfxParticlesLimit.Value=(int32_t)100;
    System::IniConf::GfxParticlesLimit.WasSet=true;
    std::unique_ptr<NC_STACK_ade::InstanceOpts> options(emitter.GenRenderInstance());
    emitter.Emit(&emission,options.get());
    check(isolated._particles.size()==1&&world.ParticleSystem()._particles.empty(),
          "real emitters route menu particles to the provided system without a global world");
    auto *previousWorld=ypaworld;ypaworld=&world;
    emission.particleSystem=nullptr;options.reset(emitter.GenRenderInstance());emitter.Emit(&emission,options.get());
    check(world.ParticleSystem()._particles.size()==1,"missing particle context retains the vanilla mission destination");
    ypaworld=previousWorld;world.ParticleSystem().Clear();
    emission.particleSystem=menuParticles;options.reset(emitter.GenRenderInstance());emitter.Emit(&emission,options.get());
    preview.Clear();
    check(menuParticles->_particles.empty(),"selection cleanup releases all preview particles");
    std::printf("PREVIEW: 27 checks, %d failures\n", failures);
    return failures != 0;
}
