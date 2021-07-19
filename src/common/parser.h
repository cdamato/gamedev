#ifndef PARSE_H
#define PARSE_H

#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <unordered_map>

class serializable {};
class deserializable {};

class config_object {
public:
    virtual ~config_object() noexcept = default;
    config_object(config_object&&) = delete;
    config_object(const config_object&) = delete;
    config_object& operator=(config_object&&) = delete;
    config_object& operator=(const config_object&) = delete;
protected:
    config_object() noexcept = default;
};

template <typename T>
class config_type final : public config_object {
public:
    T get() const noexcept { return value; }
private:
    T value;
    friend class config_parser;
};

class config_dict final : public config_object {
public:
    template <typename T>
    T get(std::string identifier) const noexcept {
        if constexpr (std::is_base_of<deserializable, T>::value) {
            auto *dict_ptr = get(identifier);
            return T::deserialize(dict_ptr);
        } else if constexpr (std::is_base_of<config_object, std::remove_pointer_t<T>>::value) {
            return dynamic_cast<T>(get(identifier));
        } else {
            auto* val_ptr = get(identifier);
            return val_ptr ? dynamic_cast<const config_type<T>*>(val_ptr)->get() : T();
        }
    }
    const config_object* get(const std::string name) const noexcept;
    auto begin() const { return map.begin(); }
    auto end() const { return map.end(); }
private:
    std::unordered_map<std::string, std::unique_ptr<config_object>> map;
    friend class config_parser;
};

class config_list final : public config_object {
public:
    template <typename value_type>
    value_type get(unsigned index) const noexcept {
        auto* ptr = get(index);
        return ptr ? dynamic_cast<const config_type<value_type>*>(ptr)->get() : value_type();
    }
    const config_object* get(unsigned id) const noexcept;
private:
    std::vector<std::unique_ptr<config_object>> list;

    friend class config_parser;
};

using config_bool = config_type<bool>;
using config_int = config_type<int>;
using config_string = config_type<std::string>;
using parsed_file = std::unique_ptr<config_dict>;
using parser_dict = config_dict;
using parser_list = config_list;
using parser_object = config_object;

class config_parser {
public:
    explicit config_parser(std::string file) noexcept : source(file) {}

    std::unique_ptr<config_dict> parse();
private:
    enum class byte_class : unsigned char {
        skippable,
        identifier,
        other,
    };

    void parse_dict_items(config_dict& dict);
    std::string parse_identifier();
    std::unique_ptr<config_object> parse_primitive_value();
    std::unique_ptr<config_string> parse_string();
    std::unique_ptr<config_list> parse_list();
    bool skip_skippables();
    bool skip_comment();
    std::optional<char> peek_byte();
    void discard_peek();
    static byte_class classify_byte(char c) noexcept;

    std::ifstream source;
};



class dsl_printer {
public:
    void write(std::string filename) {
        std::ofstream out(filename);
        out << _data;
    }

    // Append functions add text to the buffer, printing a newline afterward
    template <typename T>
    void append(std::string key, T value) {
        std::string serialized_value;
        if constexpr (std::is_base_of<serializable, T>::value) {
            serialized_value = value.serialize();
        } else {
            serialized_value = format_value(value);
        }
        _data += tab_buffer + key + " = " + serialized_value + "\n";
    }
    void append(std::string text) { _data += tab_buffer + text + '\n'; }
    template <typename... Args>
    void append(std::string key, Args... args) { _data += tab_buffer + key + " = {" + format_value_variadic(args...) + "}\n"; }
    // These functions handle formatting a collection of key/value pairs
    void open_dict(std::string text) {
        _data += tab_buffer + text + " {\n";
        num_indents++;
        regen_tab_buffer();
    }
    void close_dict() {
        num_indents--;
        regen_tab_buffer();
        _data += tab_buffer + "}";
    }
    std::string data() { return _data; }
private:
    std::string format_value(std::string value) { return "\"" + value + "\"";};
    template <typename T>
    std::string format_value(T value) { return std::to_string(value); };
    std::string format_value(bool value) { return value ? "true" : "false"; };
    template <typename T>
    std::string format_value_variadic(T value) { return format_value(value); };
    template <typename T, typename... Args>
    std::string format_value_variadic(T value, Args... args) { return format_value(value) + ", " + format_value_variadic(args...); };
    // Generate tab buffers, for indentation.
    void regen_tab_buffer() {
        std::string new_buffer = "";
        for (int i = 0; i < num_indents; i++) {
            new_buffer += "    ";
        }
        tab_buffer = new_buffer;
    }
    std::string tab_buffer = "";
    std::string _data = "";
    int num_indents = 0;
};

#endif //PARSE_H
