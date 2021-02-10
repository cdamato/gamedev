#ifndef PARSE_H
#define PARSE_H

#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <unordered_map>

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
    template <typename value_type>
    value_type get(std::string identifier) const noexcept{
        auto* ptr = get(identifier);
        return ptr ? dynamic_cast<const config_type<value_type>*>(ptr)->get() : value_type();
    }
    const config_object* get(const std::string& name) const noexcept;
    auto begin() { return map.begin(); }
    auto end() { return map.end(); }
    std::unordered_map<std::string, std::unique_ptr<config_object>> map;
private:

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




#endif //PARSE_H
