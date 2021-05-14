#include "engine/engine.h"
#include <mutex>
#include "common/parser.h"
#include <fstream>
#include "main/basic_entity_funcs.h"


void init_main_menu(engine& eng);
void test_enemies_gone(engine& e) {

    auto& enemy_pool = e.ecs.pool<ecs::c_enemy>();
    if (enemy_pool.size() == 0) {
        e.logic.remove(&test_enemies_gone);
        init_main_menu(e);
    }
}

void init_dungeon(engine& e) {

    e.in_dungeon = true;
    world_coords map_size(32, 32);
    set_map(e, map_size, generate_map(map_size));

    e.create_entity(egen_enemy, world_coords(8, 8));
    e.create_entity(egen_enemy, world_coords(3, 9));
    e.logic.add(&test_enemies_gone);
}


void init_npc_hub(engine& g) {
    printf("clicked\n");
    config_parser p("config/main_hub.txt");
    auto d = p.parse();

    const auto* mapsize = dynamic_cast<const config_list*>(d->get("map_size"));
    world_coords map_size(mapsize->get<int>(0), mapsize->get<int>(1));

    std::string tiledata_string = d->get<std::string>("tile_data");
    std::vector<u8> tile_data(tiledata_string.size());

    for (size_t i = 0; i < tile_data.size(); i++) {
        tile_data[i] = tiledata_string[i] - 48;
    }

    setup_player(g);
    set_map(g, map_size, tile_data);

    const config_dict* storagechest_dict = dynamic_cast<const config_dict*>(d->get("storage_chest"));
    const config_list* scpos = dynamic_cast<const config_list*>(storagechest_dict->get("pos"));
    const config_list* scsize = dynamic_cast<const config_list*>(storagechest_dict->get("size"));
    world_coords sc_origin(scpos->get<int>(0), scpos->get<int>(1));
    world_coords sc_size(scsize->get<int>(0), scsize->get<int>(1));

    g.create_entity([&](entity e, engine& g) {
        basic_sprite_setup(e, g, render_layers::sprites, sc_origin, sc_size, point<f32>(0, 0), size<f32>(1, 1), "button");

        ecs::c_inventory& inv = g.ecs.add<ecs::c_inventory>(e);
        ecs::c_proximity& prox = g.ecs.add<ecs::c_proximity>(e);
        prox.shape = ecs::c_proximity::shape::rectangle;
        prox.origin = world_coords(sc_origin.x - 1, sc_origin.y);
        prox.radii = world_coords(sc_size.x + 2, sc_size.y + 1);

        ecs::c_keyevent& on_activate = g.ecs.add<ecs::c_keyevent>(e);
        on_activate.add_event([&](entity e, const event& ev, engine& g_in) {
            if (g_in.command_states.test(command::toggle_inventory)) {
                g_in.destroy_entity(g_in.ui.focus);
                g.ui.focus = 65535;
                g.ui.cursor = 65535;
                g_in.command_states.set(command::toggle_inventory, false);
                return true;
            }
            g_in.create_entity([&](entity e, engine& g) { inv_transfer_init(e, g_in, inv); });
            return true;
        }, g);
    });

    const config_dict* dungeonportal_dict = dynamic_cast<const config_dict*>(d->get("dungeon_portal"));
    const config_list* dppos = dynamic_cast<const config_list*>(dungeonportal_dict->get("pos"));
    const config_list* dpsize = dynamic_cast<const config_list*>(dungeonportal_dict->get("size"));
    world_coords dp_origin(dppos->get<int>(0), dppos->get<int>(1));
    world_coords dp_size(dpsize->get<int>(0), dpsize->get<int>(1));

    g.create_entity([&](entity e, engine& g) {
        //basic_sprite_setup(e, g, render_layers::sprites, dp_origin, dp_size, point<f32>(0, 0), size<f32>(1, 1), "button");

        ecs::c_proximity& prox = g.ecs.add<ecs::c_proximity>(e);
        prox.shape = ecs::c_proximity::shape::rectangle;
        prox.origin = world_coords(dp_origin.x - 1, dp_origin.y);
        prox.radii = world_coords(dp_size.x + 2, dp_size.y + 1);

        ecs::c_keyevent& on_activate = g.ecs.add<ecs::c_keyevent>(e);
        on_activate.add_event([&](entity e, const event& ev, engine& g_in) {
            g.destroy_entity(e);
            init_dungeon(g_in);
            return true;
        }, g);
    });



}


void init_main_menu(engine& eng) {
    eng.create_entity([&](entity e, engine& g) {
        basic_sprite_setup(e, g, render_layers::ui, sprite_coords(100, 500), sprite_coords(64, 64), sprite_coords(0, 0), size<f32>(1, 1), "button");
        make_widget(e, g, eng.ui.root);

        ecs::c_mouseevent& on_click = g.ecs.add<ecs::c_mouseevent>(e);
        on_click.add_event([&](entity e, const event& ev, engine& g_in) {
            if (ev.active_state()) return true;
            g.destroy_entity(e);
            init_npc_hub(g_in);
            return true;
        }, g);
    });
}



int main() {
    printf("Exec begin\n");
    engine w;

    bool loop = true;
    timer t;
    t.start();
    timer::microseconds lag(0);


    timer fpscounter;
    int numframes = 0;

    init_main_menu(w);
    while (loop)
    {
        int ms_per_frame = 1000000 / (30 * w.settings.framerate_multiplier);
        lag += t.elapsed<timer::microseconds>();
        t.start();

        loop = w.process_events();

        if ( fpscounter.elapsed<timer::seconds>().count() >= 1.0 ) {
            printf("%f ms/frame\n", 1000.0f / double(numframes));
            numframes = 0;
            fpscounter.start();
        }

        while (lag >= timer::microseconds(ms_per_frame)) {
            w.run_tick();
            lag -= timer::microseconds(ms_per_frame);
        }


        if (!w.window().renderer_busy()) {
            numframes++;
            w.render();
        }
    }
    return 0;
}