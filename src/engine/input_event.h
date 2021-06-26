#ifndef EVENT_H
#define EVENT_H

#include <common/basic_types.h>
#include <string>

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

class event_textinput : public input_event {
public:
    EVENT_ID_TAG(3)

    event_textinput(std::string text_in) : _text(text_in) {};
    std::string text() const { return _text; }
private:
    std::string _text = "";
};

// todo - maybe use something better than this?
enum command {
	move_up,
	move_left,
	move_down,
	move_right,
	interact,
	toggle_inventory,
	toggle_options_menu,
	nav_up,
	nav_left,
	nav_down,
	nav_right,
	nav_activate,
	text_backspace,
	text_delete,
};

#endif // EVENT_H
