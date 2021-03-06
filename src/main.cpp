#include "engine/engine.h"
#include <mutex>
#include "common/parser.h"
#include <fstream>
#include <world/basic_entity_funcs.h>
#include <ui/ui.h>
#include <unicode/unistr.h>
#include <unicode/stringpiece.h>
#include <unicode/brkiter.h>



void init_main_menu(engine& eng);
void test_enemies_gone(engine& e) {

    auto& enemy_pool = e.ecs.pool<ecs::enemy>();
    if (enemy_pool.size() == 0) {
        e.logic.remove(&test_enemies_gone);
        init_main_menu(e);
    }
}

void activate_dungeon(entity e, engine& g, bool release) {
    g.destroy_entity(e);
    g.in_dungeon = true;
    world_coords map_size(32, 32);
    set_map(g, map_size, generate_map(map_size));

    g.create_entity(egen_enemy, world_coords(8, 8));
    g.create_entity(egen_enemy, world_coords(3, 9));
    g.logic.add(&test_enemies_gone);
}




void storage_chest_init(entity e, engine& g, bool release) {
    if (g.command_states.test(command::toggle_inventory)) {
        g.destroy_entity(g.ui.focus);
        g.ui.focus = 65535;
        g.ui.cursor = 65535;
        g.command_states.set(command::toggle_inventory, false);
        return;
    }
    //g.create_entity([&](entity new_e, engine& g) { inv_transfer_init(new_e, g, g.ecs.get<ecs::inventory>(e)); });
}


void init_npc_hub(entity e, engine& g, bool release) {
    g.destroy_entity(e);
    printf("clicked\n");
    config_parser p("config/main_hub.txt");
    auto d = p.parse();

    world_coords map_size = d->get<sprite_coords>("map_size").to<f32>();

    std::string tiledata_string = d->get<std::string>("tile_data");
    std::vector<u8> tile_data(tiledata_string.size());

    for (size_t i = 0; i < tile_data.size(); i++) {
        tile_data[i] = tiledata_string[i] - 48;
    }

    setup_player(g);
    set_map(g, map_size, tile_data);

    const config_dict* sc_dict = d->get<const config_dict*>("storage_chest");
    world_coords sc_origin = sc_dict->get<screen_coords>("pos").to<f32>();
    world_coords sc_size = sc_dict->get<screen_coords>("size").to<f32>();

    g.create_entity([&](entity e, engine& g) {
        basic_sprite_setup(e, g, render_layers::sprites, sc_origin, sc_size, 0, "button");
        make_widget(e, g, g.ui.root);

        ecs::inventory& inv = g.ecs.add<ecs::inventory>(e);
        ecs::proximity& prox = g.ecs.add<ecs::proximity>(e);
        prox.shape =  ecs::proximity::shape::rectangle;
        prox.origin = world_coords(sc_origin.x - 1, sc_origin.y);
        prox.radii = world_coords(sc_size.x + 2, sc_size.y + 1);

        auto& w = g.ecs.get<ecs::widget>(e);
        w.on_activate = storage_chest_init;
    });

    const config_dict* dp_dict = d->get<const config_dict*>("dungeon_portal");
    world_coords dp_origin = dp_dict->get<screen_coords>("pos").to<f32>();
    world_coords dp_size = dp_dict->get<screen_coords>("size").to<f32>();

    g.create_entity([&](entity e, engine& g) {
        basic_sprite_setup(e, g, render_layers::sprites, dp_origin, dp_size, 0, "button");
        make_widget(e, g, g.ui.root);

         ecs::proximity& prox = g.ecs.add<ecs::proximity>(e);
        prox.shape =  ecs::proximity::shape::rectangle;
        prox.origin = world_coords(dp_origin.x - 1, dp_origin.y);
        prox.radii = world_coords(dp_size.x + 2, dp_size.y + 1);

        auto& w = g.ecs.get<ecs::widget>(e);
        w.on_activate = activate_dungeon;
    });
}

void haha(entity e, engine&, bool release) {

}
void init_main_menu(engine& eng) {
    eng.create_entity([&](entity e, engine& g) {
        initialize_button_group(e, g.ui.root, 5, g, 1);
        add_button(e, g, sprite_coords(100, 400), eng.get_text_size("DARTH_PLAGUEIS_COPYPASTA").to<f32>(), 0, init_npc_hub, "DARTH_PLAGUEIS_COPYPASTA");
        //add_button(e, g, sprite_coords(196, 400), sprite_coords(64, 64), 1, haha, "MM_BUTTON_2");

    });
}

int main(int, char **) {
    printf("Exec begin\n");
    engine w;
    bool loop = true;
    timer t;
    t.start();
    timer::microseconds lag(0);


    timer fpscounter;
    int numframes = 0;

    event_mousebutton mb(screen_coords(1312, 420), true);
    printf("%s\n", mb.serialize().c_str());


    init_main_menu(w);
    while (!w.quit_received)
    {
        int ms_per_frame = 1000000 / (30 * w.settings.framerate_multiplier);
        lag += t.elapsed<timer::microseconds>();
        t.start();

        w.process_events();

        if ( fpscounter.elapsed<timer::seconds>().count() >= 1.0 ) {
            printf("%f ms/frame\n", 1000.0f / double(numframes));
            numframes = 0;
            fpscounter.start();
            //resize_ui(w, 1.25);
        }


        while (lag >= timer::microseconds(ms_per_frame)) {
            w.run_tick();
            lag -= timer::microseconds(ms_per_frame);
        }
        
        numframes++;
        w.render();
    }
    return 0;
}