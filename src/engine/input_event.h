#ifndef EVENT_H
#define EVENT_H

#include <common/basic_types.h>
#include <common/parser.h>
#include <string>

struct input_event : public serializable, deserializable {
	virtual int event_id() = 0;
	virtual ~input_event() {};
};

#define EVENT_ACCESS_FUNCTIONS(type_name, var_name) \
    type_name var_name() { return _##var_name; }

#define EVENT_MEMBER_DECLARATIONS(type_name, var_name) \
    type_name _##var_name;

#define EVENT_CONSTRUCTOR_PARAMS(type_name, var_name) \
    type_name var_name##_in,

#define EVENT_INIT_LIST(type_name, var_name) \
    _##var_name(var_name##_in),

#define EVENT_CONSTRUCTOR(class_name, member_list) \
    class_name(member_list(EVENT_CONSTRUCTOR_PARAMS) bool local_in = true) \
        : member_list(EVENT_INIT_LIST) _local(local_in) {}

#define EVENT_PARSER_APPEND(type_name, var_name) \
    printer.append(#var_name, _##var_name);

#define EVENT_PARSER_EXTRACT(type_name, var_name) \
    dict->get<type_name>(#var_name),


#define EVENT_SERIALIZE(member_list)   \
    std::string serialize() {          \
        dsl_printer printer;           \
        printer.append("");            \
        member_list(EVENT_PARSER_APPEND); \
        printer.close_dict();          \
        return printer.data();         \
    }

#define EVENT_DESERIALIZE(class_name, member_list) \
    static class_name deserialize(parser_dict* object) {  \
        return class_name(member_list(EVENT_PARSER_EXTRACT) false); \
    }

#define EVENT_ID_TAG(i) \
	constexpr static int id = i; \
	int event_id() { return id; }

#define CREATE_EVENT_CLASS(class_name, event_id_in, member_list)  \
    public: \
        EVENT_CONSTRUCTOR(class_name, member_list)                \
		EVENT_ID_TAG(event_id_in)                                 \
		EVENT_SERIALIZE(member_list)                              \
		member_list(EVENT_ACCESS_FUNCTIONS) \
    private: \
        member_list(EVENT_MEMBER_DECLARATIONS) \
        bool _local; \

/* |-------------------------------|
 * | Event auto-generation system: |
 * |-------------------------------|
 *
 * For (de)/serialization auto-generation, a macro specifying a list of members (here of the form m(typename, varname) is required.
 * These also generate member declarations and access functions, and a constructor initializing all members.
 * Member declarations are in a member `typename _varname;`, and functions in the form `typename varname() { return _varname }`.
 * Additionally, an integer param gives events a compile-time static identifier.
 */

class event_keypress : public input_event {
    #define EVENT_KEYPRESS_MEMBER_LIST(m) \
        m(u32, key_id)                    \
        m(bool, release)
    CREATE_EVENT_CLASS(event_keypress, 0, EVENT_KEYPRESS_MEMBER_LIST)
};

class event_mousebutton : public input_event {
    #define EVENT_MOUSEBUTTON_MEMBER_LIST(m) \
        m(screen_coords, pos)                \
        m(bool, release)
    CREATE_EVENT_CLASS(event_mousebutton, 1, EVENT_MOUSEBUTTON_MEMBER_LIST)
};

class event_cursor : public input_event {
    #define EVENT_CURSOR_MEMBER_LIST(m) \
        m(screen_coords, pos)
    CREATE_EVENT_CLASS(event_cursor, 2, EVENT_CURSOR_MEMBER_LIST)
};

class event_textinput : public input_event {
    #define EVENT_TEXTINPUT_MEMBER_LIST(m) \
        m(std::string, text)
    CREATE_EVENT_CLASS(event_textinput, 3, EVENT_TEXTINPUT_MEMBER_LIST)
};

class event_windowresize : public input_event {
    #define EVENT_WINDOWSIZE_MEMBER_LIST(m) \
        m(screen_coords, size)
    CREATE_EVENT_CLASS(event_windowresize, 4, EVENT_WINDOWSIZE_MEMBER_LIST)
};

class event_quit : public input_event {
public:
    EVENT_ID_TAG(5)
};

// todo - maybe use something better than this?
enum command {
	move_up, move_left,	move_down, move_right, interact,
	toggle_inventory, toggle_options_menu,
	nav_up, nav_left, nav_down, nav_right, nav_activate,
	text_backspace, text_delete,
};

#endif // EVENT_H
