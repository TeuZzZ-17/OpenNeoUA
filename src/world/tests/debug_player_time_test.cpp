#include "game/yw.h"
#include "game/ypamissile.h"
#include "game/yw_game_impl.h"
#include "system/gametime.h"
#include "system/inivals.h"
#include "nucleus/particle.h"
#include <cstdio>
#undef main
struct WorldProbe:NC_STACK_ypaworld {
    cellArea cell;
    using NC_STACK_ypaworld::HandleDebugTimeHotkeys;
    void ypaworld_func159(yw_arg159 *) override {}
    size_t GetSectorInfo(yw_130arg *arg) override {arg->pcell=&cell;arg->CellId=Common::Point();return 1;}
};
struct Actor:NC_STACK_ypabact {
    int observedDelta=0,observedPeriod=0;
    void Bind(NC_STACK_ypaworld &w){_world=&w;_energy=_energy_max=1000;_status=BACT_STATUS_NORMAL;}
    void AI_layer1(update_msg *arg) override {
        observedDelta=arg->frameTime;observedPeriod=arg->inpt?arg->inpt->Period:0;
        _position.x+=arg->frameTime;
    }
};
struct Missile:NC_STACK_ypamissile {void Bind(NC_STACK_ypaworld &w){_world=&w;}};
struct Particles:World::ParticleSystem {
    int Age(size_t index) const {return _particles[index].Age;}
    double X(size_t index) const {return _particles[index].Pos.x;}
};
struct CaptureEffect:NC_STACK_base {
    baseRender_msg captured;
    size_t Render(baseRender_msg *arg,Instance *,bool) override {captured=*arg;return 1;}
};
int main()
{
    _putenv_s("ALSOFT_DRIVERS","null");SFXEngine::SFXe.init();
    WorldProbe world;Actor player,ally,mount;Missile shot,enemyShot;
    for(auto *a:{&player,&ally,&mount})a->Bind(world);
    shot.Bind(world);enemyShot.Bind(world);
    world._userUnit=&player;mount._parent=&player;mount._isUnitGunChild=true;
    System::IniConf::GameNewDebug.Value=std::string("yes");
    TInputState input{};input.KbdLastHit=Input::KC_F2;world.HandleDebugTimeHotkeys(&input,true);
    System::GameClock.Reset();System::GameClock.BeginFrame(1000,50,0,.2f,false,true,true);
    int checks=0,failures=0;
    auto check=[&](bool ok,const char *name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    check(player.GetDebugFrameTime(10)==50,"controlled unit stays at real speed");
    check(mount.GetDebugFrameTime(10)==50,"mounted parts share the controlled carrier speed");
    check(ally.GetDebugFrameTime(10)==10,"other allies remain slowed");
    shot.SetLauncherBact(&player);enemyShot.SetLauncherBact(&ally);
    check(shot.GetDebugFrameTime(10)==50,"player projectile stays at real speed");
    check(enemyShot.GetDebugFrameTime(10)==10,"other projectiles stay slowed");
    input.Period=10;update_msg update;update.frameTime=10;update.inpt=&input;
    player.Update(&update);ally.Update(&update);
    check(player.observedDelta==50&&player.observedPeriod==50,"production update uses real movement and input delta");
    check(ally.observedDelta==10&&update.frameTime==10&&input.Period==10,"actor exception does not leak to the shared world update");
    check(player._clock==50&&ally._clock==10,"weapon and action clocks advance in their actor time domain");
    check(player._soundcarrier.DebugRealtime&&!ally._soundcarrier.DebugRealtime,"audio ownership follows the same exception");
    check(player._debugVisualTimeOffset==40&&ally._debugVisualTimeOffset==0,"animation offsets accumulate only the player's extra ticks");
    NC_STACK_particle emitter;emitter._lifeTime=1000;emitter._scaleStart=1;
    System::IniConf::GfxParticlesLimit.Value=(int32_t)100;
    Particles particles;
    particles.AddParticle(&emitter,vec3d(),vec3d(100,0,0),0,GFX::TGLColor(1,1,1,1),vec3d(1),vec3d(),1,false,false,true);
    particles.AddParticle(&emitter,vec3d(),vec3d(100,0,0));
    TF::TForm3D view;view.CalcSclRot=mat3x3::Ident();
    area_arg_65 render;render.ViewTForm=&view;
    particles.UpdateRender(&render,10);
    check(particles.Age(0)==50&&particles.Age(1)==10,"only player particles advance at real speed");
    check(std::abs(particles.X(0)-5)<1e-5&&std::abs(particles.X(1)-1)<1e-5,"particle motion follows the matching lifetime clock");
    CaptureEffect effect;
    world._normalVizLimit=5000;world._normalFadeLength=2000;
    world._vhclModels.resize(2);world._vhclModels[1]=&effect;
    player._gid=100;player._kidRef=world._unitsList.push_back(&player);
    world.SpawnAttachedTransientVP(1,&player,vec3d(),1000);
    baseRender_msg renderEffect;renderEffect.frameTime=10;
    yw_RenderTransientVPs(&world,&world._transientVPs,&renderEffect);
    check(world._transientVPs.front().age==50&&effect.captured.frameTime==50,
          "attached visuals use the controlled actor's real time");
    check(effect.captured.debugPlayerParticles&&renderEffect.frameTime==10,
          "attached emitters preserve ownership without changing the world render clock");
    world._transientVPs.clear();world._unitsList.clear();world._vhclModels.clear();
    world._userUnit=&ally;
    check(player.GetDebugFrameTime(10)==10&&ally.GetDebugFrameTime(10)==50,"switching control transfers the actor exception");
    check(shot.GetDebugFrameTime(10)==50&&enemyShot.GetDebugFrameTime(10)==10,"projectiles remember who fired them");
    shot.Renew();shot.SetLauncherBact(&player);
    check(shot.GetDebugFrameTime(10)==10,"recycled projectiles clear the previous player's time ownership");
    System::GameClock.BeginFrame(1050,50,10,.2f,true,true);
    check(ally.GetDebugFrameTime(0)==0,"F4 freezes even the controlled unit");
    System::GameClock.BeginFrame(1100,50,10,2.f,false,true);
    input.KbdLastHit=Input::KC_F3;world.HandleDebugTimeHotkeys(&input,true);
    check(ally.GetDebugFrameTime(100)==100,"F3 keeps global acceleration");
    world._isNetGame=true;
    check(ally.GetDebugFrameTime(10)==10,"netplay never enables the exception");
    world._isNetGame=false;System::IniConf::GameNewDebug.Value=std::string("no");
    check(ally.GetDebugFrameTime(10)==10,"debug disabled keeps normal timing");
    std::printf("DEBUG PLAYER TIME: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
