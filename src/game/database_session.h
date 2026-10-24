#ifndef GAME_DATABASE_SESSION_H
#define GAME_DATABASE_SESSION_H

#include "game/model_preview.h"
#include "world/database_catalog.h"

struct WorkshopOffer
{
    std::string key;
    int vehicle = -1;
    uint64_t cost = 0;
};

struct DatabaseSession
{
    DatabaseSession();
    ~DatabaseSession();
    bool LoadResources();
    bool LoadDefinitions(const NC_STACK_ypaworld &source);
    void LoadOffers(const NC_STACK_ypaworld &source);
    void StopAudio();
    void SelectAudio(const World::TVhclSound *definition, bool loop);

    std::unique_ptr<NC_STACK_ypaworld> resources;
    std::unique_ptr<NC_STACK_ypaworld> definitions;
    World::Database::Catalog catalog;
    World::Database::Card card;
    ModelPreview preview;
    std::vector<WorkshopOffer> offers;
    std::vector<int> ownedHosts;
    std::string transactionMessage;
    std::vector<std::string> text;
    std::vector<std::pair<int,int>> weaponRows;
    std::vector<std::pair<int,int>> versusRows;
    TSndCarrier carrier;
    World::TVhclSound sound;
    int angle = 0, rotationTime = 0;
    bool loaded = false;
};

// One layout is used for controls, model fitting and the information panel.
struct DatabaseLayout
{
    int line, rows, listWidth, right, width, previewTop, previewWidth,
        previewHeight, statsLeft, statsWidth, textTop, textLines, bottom;
    static DatabaseLayout For(const NC_STACK_ypaworld &world);
};

#endif
