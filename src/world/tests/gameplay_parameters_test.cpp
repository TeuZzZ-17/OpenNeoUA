// Include this production unit to exercise its private Smart selector directly.
#include "game/ypabact.cpp"
#include "game/ypamissile.h"
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
    size_t DebuffCount() const { return _debuff_list.size(); }
    float CombinedForceMalus() const { return _active_debuff.force_malus; }
    size_t BuffCount() const { return _buff_list.size(); }
};
struct Missile : NC_STACK_ypamissile {
    using NC_STACK_ypamissile::ApplyBlastToUnits;
    using NC_STACK_ypamissile::ApplyBlastToBuildings;
    using NC_STACK_ypamissile::ApplyBlastToSectors;
    using NC_STACK_ypamissile::ApplyEnergyToBact;
    using NC_STACK_ypamissile::RememberDirectHitUnit;
    using NC_STACK_ypamissile::IsArtilleryAlly;
    using NC_STACK_ypamissile::GetAreaPushSkipReason;
    using NC_STACK_ypamissile::_mislDirectHitUnits;
    using NC_STACK_ypamissile::_isArtilleryShellProjectile;
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

void CheckArtilleryRules()
{
    struct DamageWorld : TestWorld {
        std::vector<vec3d> damagePositions;
        void ypaworld_func129(yw_arg129 *arg) override { damagePositions.push_back(arg->pos); }
    };
    auto fixture = std::make_unique<DamageWorld>();
    auto &world = *fixture;
    struct Shell : Missile {
        using NC_STACK_ypamissile::UpdateArtilleryShellBallistic;
        void SetState(setState_msg *) override {}
        void ChangeSectorEnergy(yw_arg129 *arg) override { NC_STACK_ypamissile::ChangeSectorEnergy(arg); }
    };

    Unit source, aircraft, host;
    const vec3d launch = World::SectorIDToCenterPos3({2, 3});
    const vec3d airborne = World::SectorIDToCenterPos3({3, 3}) - vec3d::OY(300);
    source.Bind(world, 1, 1, launch);
    aircraft.Bind(world, 2, 2, airborne);
    host.Bind(world, 2, 3, World::SectorIDToCenterPos3({5, 3}) - vec3d::OY(450));
    host._bact_type = BACT_TYPES_ROBO;
    auto &building = world.SectorAt({4, 3});
    auto &nearBuilding = world.SectorAt({4, 4});
    building.PurposeType = cellArea::PT_POWERSTATION;
    nearBuilding.PurposeType = cellArea::PT_BUILDINGS;
    building.owner = nearBuilding.owner = 2;
    building.AddToViewMask(1);
    nearBuilding.AddToViewMask(1);
    world.SectorAt({3, 3}).AddToViewMask(1);

    Check(Parse(world, {"new_weapon 1", "model = artillery_shell", "end"}), "artillery model parses");
    auto &weapon = world.GetWeaponsProtos()[1];
    weapon.artillery_shell_max_range = 4100;
    weapon.artillery_shell_min_range = 400;
    vec3d target;
    auto selects = [&](const vec3d &expected) {
        return ypabact_FindArtilleryShellTargetZone(&source, weapon, &target) && target == expected;
    };
    weapon.artillery_shell_requires_radar = 0;
    Check(selects(host._position), "farther host station outranks a building and aircraft");
    weapon.artillery_shell_requires_radar = 1;
    Check(selects(building.GetBuildingSlotCenter(0, 0)), "nearest visible functional building outranks an aircraft");
    NC_STACK_ypagun turret;
    turret._bact_type = BACT_TYPES_GUN;
    turret._status = BACT_STATUS_NORMAL;
    turret._status_flg = 0;
    turret._owner = building.owner;
    turret._position = building.GetBuildingSlotCenter(0, 0) + vec3d(75, -300, 0);
    turret._cellRef = building.unitsList.push_back(&turret);
    weapon.artillery_shell_airburst = 1;
    Check(selects(turret._position), "airburst uses the raised turret position above the building slot");
    weapon.artillery_shell_airburst = 0;
    Check(selects(building.GetBuildingSlotCenter(0, 0)), "ground burst still aims at the building slot");
    turret._cellRef.Detach();
    building.buildings_health.At(0, 0) = 0;
    Check(selects(nearBuilding.GetBuildingSlotCenter(0, 0)), "destroyed building is skipped");
    nearBuilding.owner = source._owner;
    Check(selects(airborne), "same-faction building is skipped and aircraft keeps world Y");
    weapon.artillery_shell_min_range = 1300;
    Check(!ypabact_FindArtilleryShellTargetZone(&source, weapon, &target), "minimum range rejects the aircraft");

    auto &impact = world.SectorAt({3, 3});
    impact.PurposeType = cellArea::PT_POWERSTATION;
    impact.owner = source._owner;
    aircraft._radius = 40;
    world._userRobo = &source;
    for (const bool vertical : {false, true})
    {
        Shell shell;
        aircraft._position = airborne - vec3d::OY(20);
        shell.Bind(world, source, launch);
        shell._status = BACT_STATUS_NORMAL;
        shell._wrldSize = vec2d(8400, -8400);
        shell._radius = 40;
        shell._energy = 5000;
        shell._pSector = &impact;
        shell.SetBlast(400, 5000, 0);
        const auto initialEnergy = aircraft._energy;
        const int flight = vertical
            ? shell.SetupArtilleryShellVerticalBarrage(launch, airborne, 0, 600, 1000, 0, 0, false)
            : shell.SetupArtilleryShell(launch, airborne, 600, 1000, false);
        update_msg tick{};
        tick.frameTime = flight - 100;
        shell.UpdateArtilleryShellBallistic(&tick);
        tick.frameTime = 100;
        shell.UpdateArtilleryShellBallistic(&tick);
        Check(aircraft._energy < initialEnergy, vertical
            ? "vertical airburst terminal contact deals direct damage"
            : "ballistic airburst terminal contact deals direct damage");
        Check(shell._position.y == airborne.y, "terminal contact preserves selected airburst Y in both modes");
        Check(world.damagePositions.empty(), "same-faction power station receives neither direct nor blast damage");
        aircraft._energy = initialEnergy;
    }

    Shell shell;
    shell.Bind(world, source, launch);
    shell.SetBlast(400, 5000, 0);
    shell._position = impact.GetBuildingSlotCenter(0, 0);
    yw_arg129 damage{};
    damage.pos = shell._position;
    damage.field_10 = 5000;
    damage.unit = &source;
    shell.ChangeSectorEnergy(&damage);
    shell.ApplyBlastToBuildings();
    Check(world.damagePositions.empty(), "a deflected artillery round still spares friendly functional buildings");
    impact.owner = 2;
    shell.ChangeSectorEnergy(&damage);
    shell.ApplyBlastToBuildings();
    Check(world.damagePositions.size() == 2, "enemy functional buildings take direct and blast damage");
    impact.owner = source._owner;
    impact.PurposeType = cellArea::PT_NONE;
    world.damagePositions.clear();
    shell.ChangeSectorEnergy(&damage);
    shell.ApplyBlastToSectors();
    Check(world.damagePositions.size() == 2, "friendly ordinary sectors still take direct and blast damage");
}
}

int main(int argc, char **argv) {
    System::IniConf::Init();
    CheckArtilleryRules();
    const int perMedal = argc>1 ? std::max(1, std::atoi(argv[1])) : 5;
    System::IniConf::GameEliteUnitKillsPerMedal.Value=std::to_string(perMedal);
    TestWorld world;
    Check(Parse(world, {"new_weapon 1", "model = rocket", "energy = 100", "Blast_radius = 1500",
        "Blast_energy = 200", "Blast_Push = 3", "Blast_falloff = 1", "end"}), "shared blast parser");
    auto &weapon=world._weaponProtos[1];
    Check(weapon.blast_radius==1500 && weapon.blast_energy==200 && weapon.blast_push==3 && weapon.blast_falloff==1, "four blast values stored");
    Check(Parse(world, {"modify_weapon 1", "Blast_radius = nan", "Blast_energy = -50", "end"}), "invalid blast parser safe");
    Check(weapon.blast_radius==0 && weapon.blast_energy==0, "invalid blast disables energy and radius");
    Check(weapon.blast_debuff_id==0, "blast_debuff_id is disabled by default");
    Check(Parse(world, {"modify_weapon 1", "blast_debuff_id = 4", "end"}) && weapon.blast_debuff_id==4, "blast_debuff_id stored");
    Check(Parse(world, {"modify_weapon 1", "blast_debuff_id = -3", "end"}) && weapon.blast_debuff_id==0, "invalid blast_debuff_id disables it");
    Check(Parse(world, {"modify_weapon 1", "blast_debuff_id = shock", "debuff_id = Broken_Armor", "end"}) &&
          weapon.blast_debuff_name=="shock" && weapon.blast_debuff_id==0 &&
          weapon.debuff_name=="Broken_Armor" && weapon.debuff_id==-1, "debuff names stored");
    Check(Parse(world, {"modify_weapon 1", "blast_debuff_id = 4", "debuff_id = 0", "end"}) &&
          weapon.blast_debuff_name.empty() && weapon.blast_debuff_id==4 &&
          weapon.debuff_name.empty() && weapon.debuff_id==0, "an ID replaces a previous name");

    const vec3d centre=World::SectorIDToCenterPos3(Common::Point(3,3));
    Unit source, enemy, ally, outside;
    source.Bind(world,1,1,centre+vec3d(50,0,0));
    enemy.Bind(world,2,2,centre+vec3d(100,0,0));
    ally.Bind(world,1,3,centre+vec3d(120,0,0));
    outside.Bind(world,2,4,centre+vec3d(1700,0,0));
    world._userRobo=&source; world._userUnit=&source;
    weapon.blast_debuff.valid=true; weapon.blast_debuff.duration=10000; weapon.blast_debuff.tick_time=1000;
    Missile missile; missile.Bind(world,source,centre); missile.SetBlast(1500,0,0);
    missile.ApplyBlastToUnits();
    Check(enemy.Debuffed() && !ally.Debuffed() && !source.Debuffed(), "blast_debuff reaches enemies only");
    Check(!outside.Debuffed(), "debuff respects radius");
    enemy.ClearActiveDebuff();
    World::TVehicleBuffConfig shield, ward, cloak;
    shield.valid=ward.valid=cloak.valid=true;
    shield.profile_id=1; shield.deflect_charges=2;
    ward.profile_id=2; ward.deflect_charges=1;
    cloak.profile_id=3; cloak.invulnerable=true;
    enemy.ApplyBuff(shield);
    missile.ApplyBlastToUnits();
    Check(!enemy.Debuffed() && enemy.GetBuffDeflectChargesRemaining()==1, "Deflect stops debuff-only blast once");
    enemy.ClearBuffDeflectCharges(true);
    missile.RememberDirectHitUnit(&enemy);
    missile.ApplyBlastToUnits();
    Check(enemy.Debuffed(), "direct target without debuff_id takes blast_debuff");
    enemy.ClearActiveDebuff();
    weapon.debuff.valid=true; weapon.debuff.duration=10000; weapon.debuff.tick_time=1000;
    missile.ApplyBlastToUnits();
    Check(!enemy.Debuffed(), "debuff_id replaces blast_debuff on the direct target");
    weapon.debuff.valid=false; weapon.blast_debuff.valid=false;

    // Debuffs of different profiles coexist; a re-hit only refreshes its own entry.
    enemy.ClearActiveDebuff();
    World::TWeaponDebuffConfig slow, burn;
    slow.valid=burn.valid=true; slow.duration=burn.duration=10000;
    slow.profile_id=1; burn.profile_id=2; slow.force_malus=0.5f; burn.force_malus=0.5f;
    enemy.ApplyDebuff(slow,&source); enemy.ApplyDebuff(burn,&source); enemy.ApplyDebuff(slow,&source);
    Check(enemy.DebuffCount()==2 && std::fabs(enemy.CombinedForceMalus()-0.75f)<0.001f,
          "different Debuff profiles coexist and their maluses multiply");
    enemy.ClearActiveDebuff();
    Check(enemy.DebuffCount()==0 && !enemy.Debuffed(), "clearing removes every Debuff");

    // Buffs of different profiles coexist; Deflects are spent in order and nothing is lost.
    enemy.ResetBuffState();
    enemy.ApplyBuff(shield); enemy.ApplyBuff(ward); enemy.ApplyBuff(cloak); enemy.ApplyBuff(shield);
    Check(enemy.BuffCount()==3 && enemy.GetBuffDeflectChargesRemaining()==3 && enemy._invulnerable,
          "different Buff profiles coexist and Deflect charges add up");
    enemy.ConsumeBuffDeflectCharge(); enemy.ConsumeBuffDeflectCharge(); enemy.ConsumeBuffDeflectCharge();
    Check(!enemy.HasDeflectBuff() && enemy.BuffCount()==3 && enemy._invulnerable && enemy.HasActiveBuff(),
          "spent Deflects leave the other Buffs alone");
    enemy.SetBuffDeflectChargesRemaining(1);
    Check(enemy.GetBuffDeflectChargesRemaining()==1, "saved Deflect charges are restored");
    enemy.ResetBuffState(); enemy._invulnerable=false;

    enemy._owner=2; enemy._energy=10000; missile.SetBlast(1500,200,0);
    missile._energy=100; missile.ApplyBlastToUnits();
    Check(enemy._energy==10000, "direct target with energy takes no blast energy");
    missile._energy=0; missile.ApplyBlastToUnits();
    Check(enemy._energy==9800, "direct target without energy takes blast energy");
    missile._mislDirectHitUnits.clear(); enemy._energy=10000;
    missile.ApplyBlastToUnits();
    Check(enemy._energy==9800, "other targets take blast energy");
    missile._isArtilleryShellProjectile=true;
    Check(missile.IsArtilleryAlly(&ally) && !missile.IsArtilleryAlly(&enemy), "artillery recognises allies");
    Check(missile.GetAreaPushSkipReason(&ally) && !missile.GetAreaPushSkipReason(&enemy), "artillery never pushes allies");
    missile._isArtilleryShellProjectile=false;
    Check(!missile.GetAreaPushSkipReason(&ally), "other weapons keep their push rule");
    enemy._energy=10000;
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
    System::IniConf::UiWireframeTintOwner1.Value=std::string("50|150|255|255");
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
        ini << System::IniConf::UiWireframeTintOwner1.Name.substr(3) << " = 255|255|255|255\n";
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
