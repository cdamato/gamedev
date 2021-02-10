#ifndef EVENT_H
#define EVENT_H

#include <common/bit_types.h>
#include <common/basic_types.h>

enum event_flags {
	// event types
	button_press,
	cursor_moved,
	key_press,
	hover,
	terminate,
	null,

	active_bit,
};


class event {
	std::bitset<8> flags;
public:
	event_flags type() const {
	    if (flags.to_ulong() == 0)
	        return event_flags::null;
	    else
	        return event_flags(__builtin_ctz(flags.to_ulong() & 0b111111));
	}
	void set_type (event_flags type) { flags.set(type, true); }
	void set_active(bool state) { flags.set(event_flags::active_bit, state); }
	bool active_state() const { return flags.test(event_flags::active_bit); }

	point<f32> pos {};
	u32 ID;
};


enum command {
	move_up,
	move_left,
	move_down,
	move_right,
	interact,
	toggle_inventory,
};

#endif // EVENT_H
