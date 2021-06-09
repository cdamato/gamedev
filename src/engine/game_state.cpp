#include "game_state.h"


void hub_state_manager::new_state(game_data_manager& game_data) {
    auto& lookup = game_data.item_data.id_lookup;
    config_parser p("config/hub_repair.txt");
    auto d = p.parse();
    int i = 0;
    for (auto it = d->begin(); it != d->end(); it++) {
        std::string item_name = it->first;
        int num_required = dynamic_cast<config_int*>(it->second.get())->get();
        requirements.push_back(item_entry { lookup[item_name], rand(), num_required});
    }
}

void hub_state_manager::load_state(game_data_manager& game_data, parsed_file& file) {
    const config_dict* hub_data = dynamic_cast<const config_dict*>(file->get("hub_data"));
    for (auto it = hub_data->begin(); it != hub_data->end(); it++) {
        std::string item_name = it->first;
        const config_list* list = dynamic_cast<const config_list*>((&it->second)->get());
        item_entry item = item_entry { game_data.item_data.id_lookup[item_name], list->get<int>(0), list->get<int>(1)};
        requirements.push_back(item);
    }
}

void hub_state_manager::save_state(game_data_manager& game_data, dsl_printer& printer) {
    printer.open_dict("hub_data");
    for (auto requirement : requirements) {
        printer.append(game_data.item_data.name_lookup[requirement.item_id], requirement.current_amount, requirement.num_required);
    }
    printer.close_dict();
}


void game_state_manager::save_state(game_data_manager& game_data) {
    dsl_printer printer;
    hub_state.save_state(game_data, printer);
    printer.write("save.txt");
}

void game_state_manager::new_state(game_data_manager& game_data) {
    hub_state.new_state(game_data);
}

void game_state_manager::load_state(game_data_manager& game_data) {
    config_parser p("save.txt");
    auto d = p.parse();
    hub_state.load_state(game_data, d);
}