#include "game_data.h"
#include <common/parser.h>

item_data_manager::item_data_manager() {
    config_parser p("config/items.txt");
    auto d = p.parse();
    int i = 0;
    for (auto it = d->begin(); it != d->end(); it++) {
        std::string key = it->first;
        id_lookup[key] = i;
        name_lookup.emplace_back(key);
        i++;
    }
}