#ifndef EVENT_H
#define EVENT_H

#include <common/basic_types.h>

struct input_event  {
	virtual int event_id() = 0;
};

#define EVENT_ID_TAG(i) \
	constexpr static int id = i; \
	int event_id() { return id; }


class event_keypress : public input_event {
public:
    EVENT_ID_TAG(0)

	event_keypress(u32 id_in, bool release_in) : _id(id_in), _release(release_in) {};
	u32 key_id() { return _id; }
	bool release() { return _release; }
private:
	u32 _id;
	bool _release;
};

class event_mousebutton : public input_event {
public:
    EVENT_ID_TAG(1)

	event_mousebutton(screen_coords pos_in, bool release_in) : _pos(pos_in), _release(release_in) {};
    screen_coords pos() const { return _pos; }
    bool release() const { return _release; }
private:
    screen_coords _pos {};
    bool _release;
};

class event_cursor : public input_event {
public:
	EVENT_ID_TAG(2)

	event_cursor(screen_coords pos_in) : _pos(pos_in) {};
	screen_coords pos() const { return _pos; }
private:
	screen_coords _pos {};
};

enum command {
	move_up,
	move_left,
	move_down,
	move_right,
	interact,
	toggle_inventory,
	toggle_options_menu
};

#endif // EVENT_H
