#ifndef GAME_MODEL_PREVIEW_H
#define GAME_MODEL_PREVIEW_H

#include <memory>
#include <vector>
#include "nucleus/base.h"
#include "world/protos.h"

class NC_STACK_ypaworld;

// A render-only assembly. It owns instances, never the shared model definitions.
class ModelPreview
{
public:
    ModelPreview();
    ~ModelPreview();
    void Clear();
    bool Select(NC_STACK_ypaworld &resources, const NC_STACK_ypaworld &source,
                int category, int id);
    bool SelectSector(NC_STACK_ypaworld &resources, int sector);
    bool Available() const { return !parts_.empty() || laser_; }
    float Radius() const { return radius_; }
    size_t PartCount() const { return parts_.size(); }
    void Render(baseRender_msg &arg, const vec3d &position, int yaw, int tilt,
                bool authoredTransforms = true, bool center = true, bool faceCamera = false);

private:
    struct Part
    {
        NC_STACK_base *base = nullptr;
        std::unique_ptr<NC_STACK_base::Instance> instance;
        vec3d offset;
        mat3x3 rotation = mat3x3::Ident();
        vec3d visualRotation;
        vec3d scale = vec3d(1.0);
        World::TVisualTint tint;
        vec3d spin;
        vec3d trailScale = vec3d(1.0);
        vec3d trailSpin;
        World::TVisualTint trailTint;
        bool weapon = false;
        bool root = false;
    };
    void Add(NC_STACK_base *base, const vec3d &offset, const mat3x3 &rotation,
             const vec3d &scale, const World::TVisualTint &tint, bool root);
    void AddMounts(NC_STACK_ypaworld &resources, const NC_STACK_ypaworld &source,
                   const std::vector<World::TRoboGun> &guns);
    bool AddSector(NC_STACK_ypaworld &resources, int sector);
    void AddDecoration(const World::TDecorationFXConfig &config,const vec3d &offset,
                       const mat3x3 &rotation);
    void FitBounds();
    static mat3x3 ViewOrientation(const vec3d &position,int yaw,int tilt,bool faceCamera);
    std::vector<Part> parts_;
    NC_STACK_ypaworld *resources_ = nullptr;
    World::TWeapProto::TLaserMeshConfig laserConfig_;
    bool laser_ = false;
    float radius_ = 25.0f;
    vec3d center_;
    vec3d visualRotation_;
    struct Effects;
    std::unique_ptr<Effects> effects_;
    World::TWeaponTracerConfig tracer_;
    int age_ = 0;
};

#endif
