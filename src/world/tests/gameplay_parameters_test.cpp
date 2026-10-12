// Include this production unit to exercise its private Smart selector directly.
#include "../../ypabact.cpp"
#include "../../ypamissile.h"
#include "../parsers.h"
#include <cstdio>
#include <fstream>
#undef main

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char *name) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", name); }
}
struct TestWorld : NC_STACK_ypaworld {
    TestWorld() {
        _mapSize = Common::Point(7, 7); _cells.Resize(_mapSize);
        _vhclProtos.resize(256); _weaponProtos.resize(256);
        _buildProtos.resize(256); _roboProtos.resize(1);
        for (int y=0; y<7; ++y) for (int x=0; x<7; ++x) {
            auto &cell = _cells(x,y);
            cell.CellId=Common::Point(x,y); cell.Flags=0; cell.SectorType=1;
            cell.buildings_health.At(0,0)=100;
        }
    }
    void ypaworld_func180(yw_arg180 *) override {}
};
struct Unit : NC_STACK_ypabact {
    void Bind(TestWorld &world, int owner, int id, const vec3d &position) {
        _world=&world; _owner=owner; _gid=id; _vehicleID=id;
        _position=position; _rotation=mat3x3::Ident();
        _bact_type=BACT_TYPES_BACT; _status=BACT_STATUS_NORMAL;
        _status_flg=0; _oflags=0; _energy=_energy_max=10000; _shield=0;
        _vp_normal=_vp_dead=_vp_genesis=nullptr; _soundcarrier.Resize(7);
        _cellRef=world.SectorAt(World::PositionToSectorID(position)).unitsList.push_back(this);
    }
    void ModifyEnergy(bact_arg84 *arg) override { _energy += arg->energy; }
    bool Debuffed() const { return _active_debuff.active; }
};
struct Missile : NC_STACK_ypamissile {
    using NC_STACK_ypamissile::ApplyBlastToUnits;
    using NC_STACK_ypamissile::ApplyBlastToBuildings;
    using NC_STACK_ypamissile::ApplyBlastToSectors;
    using NC_STACK_ypamissile::ApplyEnergyToBact;
    using NC_STACK_ypamissile::RememberDirectHitUnit;
    using NC_STACK_ypamissile::_mislBlastRadius;
    using NC_STACK_ypamissile::_mislBlastEnergy;
    int buildingHits=0, sectorHits=0, lastEnergy=0;
    void Bind(TestWorld &world, Unit &emitter, const vec3d &position) {
        _world=&world; _mislEmitter=&emitter; _position=position;
        _owner=emitter._owner; _vehicleID=1; _bact_type=BACT_TYPES_MISSLE;
        _oflags=0; _status_flg=0; SetPowerHeli(1000); SetPowerTank(1000);
        SetPowerFlyer(1000); SetPowerRobo(1000);
    }
    void ChangeSectorEnergy(yw_arg129 *arg) override {
        const auto &cell=_world->SectorAt(World::PositionToSectorID(arg->pos));
        if (cell.PurposeType==cellArea::PT_NONE) ++sectorHits; else ++buildingHits;
        lastEnergy=arg->field_10;
    }
};
bool Parse(TestWorld &world, const Engine::StringList &lines) {
    ScriptParser::HandlersList handlers {
        new World::Parsers::VhclProtoParser(&world),
        new World::Parsers::WeaponProtoParser(&world)
    };
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
}

int main(int argc, char **argv) {
    System::IniConf::Init();
    const int perMedal = argc>1 ? std::max(1, std::atoi(argv[1])) : 5;
    System::IniConf::GameEliteUnitKillsPerMedal.Value=std::to_string(perMedal);
    TestWorld world;
    Check(Parse(world, {"new_weapon 1", "model = rocket", "energy = 100", "Blast_radius = 1500",
        "Blast_energy = 200", "Blast_Push = 3", "Blast_falloff = 1", "end"}), "shared blast parser");
    auto &weapon=world._weaponProtos[1];
    Check(weapon.blast_radius==1500 && weapon.blast_energy==200 && weapon.blast_push==3 && weapon.blast_falloff==1, "four blast values stored");
    Check(Parse(world, {"modify_weapon 1", "Blast_radius = nan", "Blast_energy = -50", "end"}), "invalid blast parser safe");
    Check(weapon.blast_radius==0 && weapon.blast_energy==0, "invalid blast disables energy and radius");

    const vec3d centre=World::SectorIDToCenterPos3(Common::Point(3,3));
    Unit source, enemy, ally, outside;
    source.Bind(world,1,1,centre+vec3d(50,0,0));
    enemy.Bind(world,2,2,centre+vec3d(100,0,0));
    ally.Bind(world,1,3,centre+vec3d(120,0,0));
    outside.Bind(world,2,4,centre+vec3d(1700,0,0));
    world._userRobo=&source; world._userUnit=&source;
    weapon.debuff.valid=true; weapon.debuff.duration=10000; weapon.debuff.tick_time=1000;
    Missile missile; missile.Bind(world,source,centre); missile.SetBlast(1500,0,0);
    missile.ApplyBlastToUnits();
    Check(enemy.Debuffed() && ally.Debuffed() && source.Debuffed(), "debuff-only blast includes enemy ally and shooter");
    Check(!outside.Debuffed(), "debuff respects radius");
    enemy.ClearActiveDebuff(); ally.ClearActiveDebuff(); source.ClearActiveDebuff();
    ally._buff.valid=true; ally._buff.deflect_charges=2;
    missile.ApplyBlastToUnits();
    Check(!ally.Debuffed() && ally.GetBuffDeflectChargesRemaining()==1, "Deflect stops debuff-only blast once");
    ally._buff.deflect_charges=0;
    weapon.debuff.mindcontrol=true;
    ally.ClearActiveDebuff(); missile.ApplyBlastToUnits();
    Check(ally.Debuffed() && ally._owner==1, "allied target keeps other debuff properties without mindcontrol");
    weapon.debuff.mindcontrol=false;

    enemy._owner=2; enemy._energy=10000; missile.SetBlast(1500,200,0);
    missile.ApplyEnergyToBact(&enemy,100); missile.RememberDirectHitUnit(&enemy);
    missile.ApplyBlastToUnits();
    Check(enemy._energy==9700, "direct energy and blast energy add");
    world._cells(3,3).PurposeType=cellArea::PT_POWERSTATION;
    world._cells(4,3).PurposeType=cellArea::PT_NONE;
    missile.ApplyBlastToBuildings(); missile.ApplyBlastToSectors();
    Check(missile.buildingHits>0 && missile.sectorHits>0 && missile.lastEnergy==200, "one blast energy reaches building and sector layers");

    Check(Parse(world, {"new_weapon 2", "model = rocket", "energy = 60", "energy_flyer = 3", "end"}), "second selectable weapon");
    source._weapon=1; source._extra_weapons={2};
    world._weaponProtos[1].energy=100; world._weaponProtos[1].energy_flyer=1;
    world._weaponProtos[2].energy=60; world._weaponProtos[2].energy_flyer=3;
    world._weaponProtos[2].energy_plane=4; world._weaponProtos[2].energy_plane_defined=true;
    enemy._bact_type=BACT_TYPES_FLYER;
    for (auto combat : {World::VEHICLE_COMBAT_CLASS_PLANE, World::VEHICLE_COMBAT_CLASS_GLIDER,
                       World::VEHICLE_COMBAT_CLASS_CRUISER, World::VEHICLE_COMBAT_CLASS_ZEPPELIN}) {
        world._vhclProtos[enemy._vehicleID].combat_class=combat;
        int slot=-1;
        for (int i=0; i<20; ++i) Check(ypabact_SelectSmartAIPrimaryWeaponSlot(&source,&enemy,&slot)==2,
            "Smart consistently chooses effective energy including class defaults");
    }
    source._bact_type=BACT_TYPES_BACT;
    for (int i=1; i<=perMedal*2; ++i) {
        source.AwardSessionKill(false);
        Check(source.GetSessionKillMarks()==i/perMedal, "medal requires configured additional kills");
    }
    source.AwardSessionKill(true);
    Check(source.GetSessionKillMarks()==4, "Host Station kill awards maximum medals");
    source._bact_type=BACT_TYPES_UFO;
    Check(source.GetSessionKillMarks()==0, "excluded classes cannot use medals");
    System::IniConf::UiWireframeTintOwner1.Value=std::string("50_150_255_255");
    auto tint=World::GetOwnerWireframeTint(1);
    Check(tint.r==50/255.0f && tint.g==150/255.0f && tint.b==1, "owner wireframe tint");
    System::IniConf::UiWireframeTintOwner1.Value=std::string("0");
    Check(World::GetOwnerWireframeTint(1).IsNeutral(), "zero wireframe tint uses original colors");
    {
        std::ofstream ini("Data/Build/probes/owner_tints.ini");
        ini << "ui.wireframe_tints_owner_01 = 70_80_90_255\n";
        ini << "ui.wireframe_tints_owner_2 = 10_20_30_255\n";
        ini << "ui.wireframe_tints_owner_00 = 0\n";
        ini << "ui.wireframe_tints_owner_007 = 1_2_3_4\n";
        // Unprefixed names must not override the newly registered UI keys.
        ini << System::IniConf::UiWireframeTintOwner1.Name.substr(3) << " = 255_255_255_255\n";
        ini.close();
        FSMgr::iDir::setBaseDir(".");
        Check(System::IniConf::ReadFromIni("Data/Build/probes/owner_tints.ini"), "owner tint INI parsed");
        Check(World::GetOwnerWireframeTint(1).r==70/255.0f && World::GetOwnerWireframeTint(2).g==20/255.0f,
              "padded and unpadded owner names resolve the same global keys");
        Check(World::GetOwnerWireframeTint(0).IsNeutral() && World::GetOwnerWireframeTint(7).r==1/255.0f,
              "UI owner keys preserve zero default and padded owner seven");
    }
    std::printf("CHECKS %d FAILURES %d\n",checks,failures);
    return failures?1:0;
}
