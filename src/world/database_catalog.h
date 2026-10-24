#ifndef WORLD_DATABASE_CATALOG_H
#define WORLD_DATABASE_CATALOG_H

#include <string>
#include <cstdint>
#include <vector>
#include <utility>
#include <memory>

class NC_STACK_ypaworld;

namespace World
{
namespace Database
{

enum Category
{
    Vehicle = 0,
    Weapon = 1,
    Building = 2
};

enum class Metric
{
    Attack,
    Defense,
    Health,
    Speed
};

struct Rating
{
    Metric kind;
    double value;
    int blocks;
};

struct Card
{
    bool valid = false;
    std::string name;
    std::string description;
    std::string className;
    uint32_t icon = 0;
    int64_t purchaseCost = -1;
    int productionCost = -1;
    int guns = 0;
    int healingPower = 0;
    int guardians = 0;
    int radarPower = 0;
    std::vector<std::pair<std::string, int>> versus;
    std::vector<Rating> ratings;
    std::vector<int> weapons;
};

class Catalog
{
public:
    void Build(NC_STACK_ypaworld &world);
    const std::vector<int> &Entries(int category) const;
    Card Describe(const NC_STACK_ypaworld &world, int category, int id) const;

private:
    std::vector<int> entries_[3];
};
}
}

#endif
