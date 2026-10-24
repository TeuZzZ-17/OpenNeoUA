// Briefing, database and workshop share the same render-only model assembly.
#include "game/model_preview.h"
#include "game/yw.h"
#include "game/yw_game_impl.h"
#include "world/gunrotation.h"
#include "world/spin.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <limits>

struct ModelPreview::Effects
{
    struct Source
    {
        World::TDecorationFXConfig config;
        NC_STACK_base *base;
        vec3d offset;
        mat3x3 rotation;
        int next = 0;
    };
    std::vector<Source> sources;
    std::list<NC_STACK_ypaworld::TTransientVP> active;
    World::ParticleSystem particles;
    // This unregistered carrier only evaluates the existing visual-motion math.
    // It has no world, collision, update tick, weapons or gameplay lifetime.
    std::unique_ptr<NC_STACK_ypabact> motion;
    std::mt19937 random{0x44425052u};
};

ModelPreview::ModelPreview():effects_(new Effects) {}
ModelPreview::~ModelPreview() = default;

namespace
{
NC_STACK_base *Resolve(NC_STACK_ypaworld &world, int vp,
                       const std::string &mesh, const std::string &base)
{
    // The gameplay resolver intentionally throws for invalid legacy IDs. A menu
    // must also support partially authored definitions, without changing that contract.
    try { return world.ResolveVisualModel(vp, mesh, base); }
    catch (const std::out_of_range &) { return nullptr; }
}

mat3x3 Orientation(const vec3d &degrees)
{
    constexpr double radians = 3.14159265358979323846 / 180.0;
    return mat3x3::RotateX(degrees.x * radians) *
           mat3x3::RotateY(degrees.y * radians) *
           mat3x3::RotateZ(degrees.z * radians);
}
}

void ModelPreview::Clear()
{
    parts_.clear();
    resources_ = nullptr;
    laser_ = false;
    laserConfig_ = World::TWeapProto::TLaserMeshConfig();
    radius_ = 25.0f;
    center_ = visualRotation_ = vec3d();
    effects_->active.clear();effects_->sources.clear();effects_->random.seed(0x44425052u);
    effects_->motion.reset();
    effects_->particles.Clear();
    tracer_=World::TWeaponTracerConfig();age_=0;
}

void ModelPreview::Add(NC_STACK_base *base, const vec3d &offset,
                       const mat3x3 &rotation, const vec3d &scale,
                       const World::TVisualTint &tint, bool root)
{
    if (!base) return;
    Part part;
    part.base = base;
    part.instance.reset(base->GenRenderInstance());
    part.offset = offset;
    part.rotation = rotation;
    part.scale = scale;
    part.tint = tint;
    part.root = root;
    parts_.push_back(std::move(part));
}

void ModelPreview::AddMounts(NC_STACK_ypaworld &resources, const NC_STACK_ypaworld &source,
                             const std::vector<World::TRoboGun> &guns)
{
    for (const auto &gun : guns)
    {
        if (!gun.robo_gun_type || gun.robo_gun_type >= source.GetVhclProtos().size()) continue;
        const auto &p = source.GetVhclProtos()[gun.robo_gun_type];
        const auto rotation=World::InitialGunRotation(gun.dir).Transpose();
        const size_t before=parts_.size();
        Add(Resolve(resources, p.vp_normal, p.visual_3ds.normal, p.visual_base.normal),
            gun.pos, rotation, p.visual_scale, p.visual_tint, false);
        if(parts_.size()>before)
        {parts_.back().spin=p.visual_spin;parts_.back().visualRotation=p.visual_rotation;}
        AddDecoration(p.decoration_fx,gun.pos,World::InitialGunRotation(gun.dir).Transpose());
    }
}

void ModelPreview::AddDecoration(const World::TDecorationFXConfig &config,const vec3d &offset,
                                  const mat3x3 &rotation)
{
    if(!resources_||(config.vp<=0&&config.mesh3ds.empty()&&config.basePath.empty()))return;
    if(config.mode!=World::DECORATION_FX_PERSISTENT&&
       (config.count_min<=0||config.count_max<=0||config.interval_min<=0||config.interval_max<=0))return;
    if(auto *base=Resolve(*resources_,config.vp,config.mesh3ds,config.basePath))
        effects_->sources.push_back({config,base,offset,rotation,0});
}

bool ModelPreview::AddSector(NC_STACK_ypaworld &resources, int sector)
{
    if (sector < 0 || sector >= (int)resources._secTypeArray.size()) return false;
    auto &p = resources._secTypeArray[sector];
    const int size = p.SectorType == 1 ? 1 : 3;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const auto *sub = p.SubSectors.At(x,y);
            if (!sub || sub->HPModels[0] >= resources._legoArray.size()) continue;
            Add(resources._legoArray[sub->HPModels[0]].Base,
                vec3d(size == 1 ? 0 : x - 1, 0, size == 1 ? 0 : y - 1) * 300.0,
                mat3x3::Ident(), vec3d(1.0), World::TVisualTint(), false);
        }
    return Available();
}

bool ModelPreview::SelectSector(NC_STACK_ypaworld &resources, int sector)
{
    Clear();
    resources_ = &resources;
    AddSector(resources, sector);
    FitBounds();
    return Available();
}

bool ModelPreview::Select(NC_STACK_ypaworld &resources, const NC_STACK_ypaworld &source,
                           int category, int id)
{
    Clear();
    resources_ = &resources;
    if (id < 0) return false;
    if (category == 0 && id < (int)source.GetVhclProtos().size())
    {
        const auto &p = source.GetVhclProtos()[id];
        if (p.Index < 0) return false;
        visualRotation_ = p.visual_rotation;
        Add(Resolve(resources,p.vp_normal,p.visual_3ds.normal,p.visual_base.normal),
            vec3d(),mat3x3::Ident(),p.visual_scale,p.visual_tint,true);
        if(!parts_.empty())parts_.back().spin=p.visual_spin;
        AddDecoration(p.decoration_fx,vec3d(),mat3x3::Ident());
        if (p.model_id == BACT_TYPES_ROBO && p.RoboProto) AddMounts(resources,source,p.RoboProto->guns);
        AddMounts(resources,source,p.unit_guns);
    }
    else if (category == 1 && id < (int)source.GetWeaponsProtos().size())
    {
        const auto &p = source.GetWeaponsProtos()[id];
        visualRotation_ = p.visual_rotation;
        tracer_=p.tracer;
        if(!p.IsLaser()&&((p.spiral_speed>0&&p.spiral_radius>0)||(p.chaos_factor>0&&p.chaos_radius>0)))
        {
            effects_->motion.reset(new NC_STACK_ypabact);
            auto &motion=*effects_->motion;
            motion._rotation=mat3x3::Ident();motion._gid=0x44425052u;
            motion._spiral_speed=p.spiral_speed;motion._spiral_radius=p.spiral_radius;
            motion._chaos_factor=p.chaos_factor;motion._chaos_radius=p.chaos_radius;
            motion._fly_dir=vec3d(0,0,1);
            motion._fly_dir_length=p.start_speed>0?p.start_speed:(p.airconst>0?p.force/p.airconst:0);
        }
        if (p.IsLaser() && p.laser_mesh.enabled)
        {
            // Reuse the real mesh beam renderer, without a shooter or damage tick.
            laserConfig_ = p.laser_mesh;
            laser_ = !p.laser_mesh.mesh_path.empty() && resources.GetSharedExternalMesh(p.laser_mesh.mesh_path);
            radius_ = 240.0f;
        }
        else
        {
            auto *body = Resolve(resources,p.vp_normal,p.visual_3ds.normal,p.visual_base.normal);
            const int count = p.IsLaser() ? std::max(2,std::min(80,(int)(400.0f / std::max(5.0f,p.laser_spacing)))) : 1;
            for (int i = 0; i < count; ++i)
            {
                const size_t before=parts_.size();
                Add(body,vec3d(count == 1 ? 0 : -200.0 + 400.0 * i / (count-1),0,0),
                    mat3x3::Ident(),p.visual_scale,p.visual_tint,true);
                if(parts_.size()>before)
                {
                    auto &part=parts_.back();part.weapon=true;part.spin=p.visual_spin;
                    part.trailScale=p.vp_trail_scale;part.trailSpin=p.vp_trail_spin;part.trailTint=p.vp_trail_tint;
                }
            }
        }
        AddDecoration(p.decoration_fx,vec3d(),mat3x3::Ident());
    }
    else if (category == 2 && id < (int)source.GetBuildProtos().size())
    {
        const auto &p = source.GetBuildProtos()[id];
        if (p.Index < 0) return false;
        AddSector(resources,p.SecType);
        AddDecoration(p.DecorationFX,vec3d(),mat3x3::Ident());
        for (auto &part : parts_) part.tint = p.tint;
        for (const auto &gun : p.Guns)
        {
            if (gun.VhclID <= 0 || gun.VhclID >= (int)source.GetVhclProtos().size()) continue;
            const auto &g = source.GetVhclProtos()[gun.VhclID];
            const auto rotation=World::InitialGunRotation(gun.Dir).Transpose();
            const size_t before=parts_.size();
            Add(Resolve(resources,g.vp_normal,g.visual_3ds.normal,g.visual_base.normal),
                gun.Pos,rotation,g.visual_scale,g.visual_tint,false);
            if(parts_.size()>before)
            {parts_.back().spin=g.visual_spin;parts_.back().visualRotation=g.visual_rotation;}
            AddDecoration(g.decoration_fx,gun.Pos,World::InitialGunRotation(gun.Dir).Transpose());
        }
    }
    FitBounds();
    if(category==0&&id>=0&&id<(int)source.GetVhclProtos().size())
    {
        const auto &p=source.GetVhclProtos()[id];
        const float authored=p.BriefingRadius();
        if(std::isfinite(authored)&&authored>0)radius_=authored;
    }
    return Available();
}

void ModelPreview::FitBounds()
{
    bool measured = false;
    vec3d minimum, maximum;
    auto measurePart=[&](const Part &part)
    {
        const mat3x3 rotation = (part.root ? Orientation(visualRotation_) : mat3x3::Ident()) * part.rotation * Orientation(part.visualRotation);
        std::function<void(NC_STACK_base *, const mat3x3 &, const vec3d &)> measure;
        measure = [&](NC_STACK_base *node, const mat3x3 &localRotation, const vec3d &translation) {
            auto pointBounds = [&](const vec3d &vertex)
                {
                    vec3d point = translation + localRotation.Transform(vertex);
                    point = rotation.Transform(vec3d(point.x * part.scale.x, point.y * part.scale.y,
                                                     point.z * part.scale.z)) + part.offset;
                    if (!measured) minimum = maximum = point;
                    else
                    {
                        minimum = vec3d(std::min(minimum.x,point.x),std::min(minimum.y,point.y),std::min(minimum.z,point.z));
                        maximum = vec3d(std::max(maximum.x,point.x),std::max(maximum.y,point.y),std::max(maximum.z,point.z));
                    }
                    measured = true;
                };
            // Skeletons can contain helper points far outside the visible model.
            // Frame the rendered mesh instead, retaining skeleton-only previews.
            if (!node->Meshes.empty())
                for (const auto &mesh : node->Meshes)
                    for (const auto &vertex : mesh.Vertexes) pointBounds(vec3d(vertex.Pos));
            else if (node->GetSkeleton() && node->GetSkeleton()->GetSkelet())
                for (const auto &vertex : node->GetSkeleton()->GetSkelet()->POO) pointBounds(vertex);
            for (auto *child : node->GetKidList())
                measure(child,localRotation * child->TForm().SclRot,
                        translation + localRotation.Transform(child->GetPos()));
        };
        measure(part.base,mat3x3::Ident(),vec3d());
    };
    for(const auto &part:parts_)measurePart(part);
    for(const auto &source:effects_->sources)
    {
        Part part;part.base=source.base;part.scale=source.config.scale;
        part.rotation=source.rotation;
        part.offset=source.offset+source.rotation.Transform(source.config.offset);
        measurePart(part);
    }
    if (measured)
    {
        center_ = (minimum + maximum) * 0.5;
        radius_ = std::max(laser_?240.0f:1.0f,(float)((maximum - minimum) * 0.5).length());
    }
    if(tracer_.enabled)radius_=std::max(radius_,(float)tracer_.pos.length()+tracer_.size_z);
    if(effects_->motion)radius_+=std::max(effects_->motion->_chaos_radius,effects_->motion->_spiral_radius);
}

mat3x3 ModelPreview::ViewOrientation(const vec3d &position,int yaw,int tilt,bool faceCamera)
{
    const auto orientation=Orientation(vec3d(tilt,yaw,0));
    if(!faceCamera)return orientation;
    // A preview sits off the screen centre. Align its presentation to that
    // viewing ray, otherwise its screen position exposes the underside.
    return mat3x3::RotateY(std::atan2(position.x,position.z))*
           mat3x3::RotateX(std::atan2(position.y,std::hypot(position.x,position.z)))*orientation;
}

void ModelPreview::Render(baseRender_msg &arg, const vec3d &position, int yaw, int tilt,
                           bool authoredTransforms, bool center, bool faceCamera)
{
    const mat3x3 rotation = ViewOrientation(position,yaw,tilt,faceCamera);
    auto renderContext=arg;
    if(authoredTransforms)renderContext.particleSystem=&effects_->particles;
    if(authoredTransforms)age_+=std::max(0,arg.frameTime);
    vec3d motionOffset;mat3x3 motionRotation=mat3x3::Ident();
    if(authoredTransforms&&effects_->motion)
    {
        effects_->motion->_clock=age_;
        effects_->motion->GetProjectileVisualMotionDelta(&motionOffset,&motionRotation);
    }
    if(authoredTransforms&&resources_&&tracer_.enabled)
    {
        const vec3d end=position+rotation.Transform(motionOffset+tracer_.pos);
        resources_->RenderWeaponTracerSegment(&renderContext,end-rotation.Transform(motionRotation.Transform(vec3d(0,0,tracer_.size_z))),
                                               end,tracer_,0.0f,1.0f,0x44425052u);
    }
    if (laser_ && resources_)
        resources_->RenderLaserMeshSegment(&renderContext,position + rotation.Transform(vec3d(-200,0,0)),
            position + rotation.Transform(vec3d(200,0,0)),laserConfig_,false,rotation,0x44425052u);
    for (auto &part : parts_)
    {
        auto *base = part.base;
        const auto savedTransform = base->TForm();
        const bool savedStatic = base->IsStatic();
        const int savedLimit = base->GetVisLimit(), savedFade = base->GetFadeLength();
        base->SetStatic(false);
        base->SetVizLimit(16000);
        base->SetFadeLength(100);
        base->SetPosition(position + rotation.Transform(part.offset + (part.weapon?motionOffset:vec3d()) - (center ? center_ : vec3d())));
        base->SetEulerRotation(0,0,0);
        base->SetScale(authoredTransforms ? part.scale : vec3d(1.0));
        base->TForm().SclRot = rotation * (authoredTransforms && part.root ? Orientation(visualRotation_) : mat3x3::Ident()) * part.rotation * base->TForm().SclRot;
        if(authoredTransforms)
        {
            // Match gameplay order: authored orientation, visual motion, spin, scale.
            base->TForm().SclRot=rotation*(part.root?Orientation(visualRotation_):mat3x3::Ident())*
                part.rotation*Orientation(part.visualRotation)*(part.weapon?motionRotation:mat3x3::Ident())*
                World::Spin::BuildMatrix(part.spin,age_)*mat3x3::Scale(part.scale);
        }
        auto render = renderContext;
        if (authoredTransforms)
        {
            render.tint = GFX::TGLColor(part.tint.r,part.tint.g,part.tint.b,part.tint.a);
            render.colorizeTint = part.tint.ColorizesRGB();
            render.excludeSectorTerrainFromTint = !part.root;
            const auto &tint=part.weapon?part.trailTint:part.tint;
            render.particleTint=GFX::TGLColor(tint.r,tint.g,tint.b,tint.a);
            render.particleColorizeTint=tint.ColorizesRGB();
            render.particleScale=part.weapon?vec3d(part.trailScale.x,part.trailScale.y,1.0):part.scale;
            render.particleLifetimeScale=part.weapon?part.trailScale.z:1.0f;
            render.particleSpin=part.weapon?part.trailSpin:part.spin;
        }
        base->Render(&render,part.instance.get());
        base->SetStatic(savedStatic);
        base->SetVizLimit(savedLimit);
        base->SetFadeLength(savedFade);
        base->TForm() = savedTransform;
    }
    if(authoredTransforms&&resources_)
    {
        for(size_t index=0;index<effects_->sources.size();++index)
        {
            auto &source=effects_->sources[index];const auto &config=source.config;
            auto roll=[&](int a,int b){return std::uniform_int_distribution<int>(std::min(a,b),std::max(a,b))(effects_->random);};
            const bool persistent=config.mode==World::DECORATION_FX_PERSISTENT;
            if(!persistent&&source.next<=0)
                source.next=age_+roll(config.interval_min,config.interval_max);
            if(age_>=source.next)
            {
                const int count=persistent?1:roll(std::max(0,std::min(32,config.count_min)),std::max(0,std::min(32,config.count_max)));
                for(int i=0;i<count;++i)
                {
                    NC_STACK_ypaworld::TTransientVP fx(source.base,vec3d(),mat3x3::Ident(),persistent?0:(config.duration>0?config.duration:1000));
                    fx.id=(int)index;
                    fx.axisScale=vec3d(config.scale.x>0?config.scale.x:1,config.scale.y>0?config.scale.y:1,config.scale.z>0?config.scale.z:1);
                    fx.spin=config.spin;fx.tint=config.tint;
                    fx.particleControls=NC_STACK_ypaworld::TTransientVPParticleControls(config);
                    fx.fadeIn=config.fade_in;fx.fadeOut=config.fade_out;
                    const double spread=std::max(0.0f,config.random_pos);
                    auto jitter=[&](){return std::uniform_real_distribution<double>(-spread,spread)(effects_->random);};
                    fx.followLocalOffset=config.offset+vec3d(jitter(),jitter(),jitter());
                    effects_->active.push_back(std::move(fx));
                }
                source.next=persistent?std::numeric_limits<int>::max():age_+std::max(1,roll(config.interval_min,config.interval_max));
            }
        }
        // The real effect renderer handles particles, fade and spin. Its list
        // belongs to this preview, so selection never touches mission effects.
        std::vector<std::pair<NC_STACK_base *,TF::TForm3D>> saved;
        for(auto &fx:effects_->active)
        {
            const auto &source=effects_->sources[fx.id];
            fx.pos=position+rotation.Transform(motionOffset+source.offset+source.rotation.Transform(motionRotation.Transform(fx.followLocalOffset))-(center?center_:vec3d()));
            fx.rot=(rotation*source.rotation*motionRotation).Transpose();
            auto *base=fx.vp->Bas;
            if(std::none_of(saved.begin(),saved.end(),[&](const auto &entry){return entry.first==base;}))saved.emplace_back(base,base->TForm());
        }
        yw_RenderTransientVPs(resources_,&effects_->active,&renderContext);
        for(const auto &entry:saved)entry.first->TForm()=entry.second;
        area_arg_65 particleRender;
        particleRender.ViewTForm=TF::Engine.GetViewPoint();
        particleRender.timeStamp=arg.globTime;particleRender.frameTime=arg.frameTime;
        particleRender.minZ=arg.minZ;particleRender.maxZ=arg.maxZ;
        particleRender.particleSystem=&effects_->particles;
        effects_->particles.UpdateRender(&particleRender,arg.frameTime);
    }
}
