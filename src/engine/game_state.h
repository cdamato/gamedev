#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <vector>
#include <common/parser.h>
#include "game_data.h"

class game_data_manager;
struct hub_state_manager {
    struct item_entry {
        u32 item_id;
        int current_amount;
        int num_required;
    };
    std::vector<item_entry> requirements;
    void new_state(game_data_manager&);
    void load_state(game_data_manager&, parsed_file&);
    void save_state(game_data_manager&,dsl_printer&);
};

// Contains the state for a current savegame.
struct game_state_manager {
    void new_state(game_data_manager&);
    void load_state(game_data_manager&);
    void save_state(game_data_manager&);

    hub_state_manager hub_state;
};

#endif //GAME_STATE_H
