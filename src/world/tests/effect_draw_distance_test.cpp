// Exercise the production transient renderer without a graphics context.
#include "game/yw.h"
#include "game/yw_game_impl.h"
#include "common/global.h"
#include <cstdio>
#include <cmath>
#undef main
struct CaptureEffect : NC_STACK_base
{
    int calls=0;
    baseRender_msg captured;
    size_t Render(baseRender_msg *arg,Instance *,bool) override
    {++calls;captured=*arg;return 1;}
};
int main()
{
    NC_STACK_ypaworld world;
    CaptureEffect model;
    world._vhclModels.resize(2);world._vhclModels[1]=&model;
    world._normalVizLimit=5000;world._normalFadeLength=2000;
    TF::TForm3D view;view.CalcPos=vec3d();
    auto *saved=TF::Engine.GetViewPoint();TF::Engine.SetViewPoint(&view);
    int checks=0,failures=0;
    auto check=[&](bool ok,const char *name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    for(float distance:{1000.f,3000.f,4000.f,4999.f,5000.f,9000.f})
    {
        world._transientVPs.clear();model.calls=0;
        world.SpawnTransientVP(1,vec3d(0,0,distance),mat3x3::Ident(),1000);
        baseRender_msg arg;arg.frameTime=10;
        yw_RenderTransientVPs(&world,&world._transientVPs,&arg);
        check(model.calls==(distance<5000?1:0),"world draw distance controls submission");
        if(distance<5000)
        {
            const float expected=distance<=3000?1:(5000-distance)/2000;
            check(std::abs(model.captured.tint.a-expected)<1e-5,"ordinary alpha fades with distance");
            check(std::abs(model.captured.vpFadeFactor-expected)<1e-5,"additive intensity uses the same fade");
        }
        check(world._transientVPs.front().age==10,"hidden effects still advance their lifetime");
    }
    world._transientVPs.clear();model.calls=0;
    world.SpawnTransientVP(1,vec3d(0,0,4000),mat3x3::Ident(),1000,1,World::TVisualTint(),vec3d(1),vec3d(),NC_STACK_ypaworld::TTransientVPParticleControls(),200,200);
    world._transientVPs.front().age=100;
    baseRender_msg arg;arg.frameTime=10;
    yw_RenderTransientVPs(&world,&world._transientVPs,&arg);
    check(std::abs(model.captured.tint.a-.25)<1e-5,"lifetime and distance fades combine");
    check(arg.tint.a==1&&arg.vpFadeFactor==1,"shared render controls are restored");
    // With the sky dome active the visible world ends at the sky horizon,
    // which is much closer than the CPU-side _normalVizLimit budget.
    world._normalVizLimit=50000;world._normalFadeLength=100;
    world._skyObject=&model;
    const float horizon=(float)world.GetSkyHorizonDistance();
    for(float offset:{-200.f,-1.f,0.f,500.f})
    {
        world._transientVPs.clear();model.calls=0;
        world.SpawnTransientVP(1,vec3d(0,0,horizon+offset),mat3x3::Ident(),1000);
        arg.particleSystem=nullptr;
        yw_RenderTransientVPs(&world,&world._transientVPs,&arg);
        check(model.calls==(offset<0?1:0),"sky horizon distance hides effects beyond the visible world");
    }
    world._skyObject=nullptr;
    world._transientVPs.clear();model.calls=0;
    world.SpawnTransientVP(1,vec3d(0,0,horizon+500),mat3x3::Ident(),1000);
    yw_RenderTransientVPs(&world,&world._transientVPs,&arg);
    check(model.calls==1,"without the sky dome the normal draw limit still applies");

    world._transientVPs.clear();model.calls=0;
    world.SpawnTransientVP(1,vec3d(0,0,9000),mat3x3::Ident(),1000);
    arg.particleSystem=&world.ParticleSystem();TF::Engine.SetViewPoint(nullptr);
    yw_RenderTransientVPs(&world,&world._transientVPs,&arg);
    check(model.calls==1,"isolated menu previews ignore mission draw distance");
    TF::Engine.SetViewPoint(saved);
    std::printf("EFFECT DISTANCE: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
