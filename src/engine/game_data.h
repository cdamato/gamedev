#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "ecs.h"
#include <unordered_map>

struct item_data_manager {
    std::unordered_map<std::string, int> id_lookup;
    std::vector<std::string> name_lookup;
    item_data_manager();
};

// Contains all the game's flexible data, loaded from text files at game startup.
struct game_data_manager {
    item_data_manager item_data;
};

#endif //GAME_DATA_H
