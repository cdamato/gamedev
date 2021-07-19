#include "parser.h"

#include <climits>
#include <array>


const config_object* config_dict::get(const std::string name) const noexcept
{
    const auto it = map.find(name);
    return (it != map.end()) ? it->second.get() : nullptr;
}


const config_object* config_list::get(unsigned id) const noexcept
{
    if (id >= list.size())
        return nullptr;
    return list[id].get();
}

std::unique_ptr<config_dict> config_parser::parse()
{
    auto result = std::unique_ptr<config_dict>(new config_dict);
    parse_dict_items(*result);
    return result;
}

// dict_items:
//     (dict_item)*
//
// dict_item:
//     IDENTIFIER dict_item_value
//
// dict_item_value:
//     '=' primitive_value
//     '{' dict_items '}'
void config_parser::parse_dict_items(config_dict& dict)
{
    for(;;) {
        if(!skip_skippables()) {
            break;
        }
        const auto first_byte = peek_byte().value();
        if(classify_byte(first_byte) != byte_class::identifier) {
            break;
        }
        auto item_name = parse_identifier();
        if(!skip_skippables()) {
            throw std::runtime_error("Expected item declaration but end of input found");
        }
        const auto c = peek_byte().value();
        if(c == '{') {
            discard_peek();
            auto new_dict = std::unique_ptr<config_dict>(new config_dict);
            parse_dict_items(*new_dict);
            skip_skippables();
            if(peek_byte() != '}') {
                throw std::runtime_error("Untermined dict found");
            }
            discard_peek();
            dict.map.insert_or_assign(std::move(item_name), std::unique_ptr<config_object>(std::move(new_dict)));
        } else if (c == '=') {
            discard_peek();
            auto value = parse_primitive_value();
            dict.map.insert_or_assign(std::move(item_name), std::move(value));
        } else {
            throw std::runtime_error(std::string("Unexpected character: ") + c);
        }
    }
}

std::string config_parser::parse_identifier()
{
    std::string result;
    for(;;) {
        const auto maybe_byte = peek_byte();
        if(!maybe_byte) {
            break;
        }
        const auto c = *maybe_byte;
        if(classify_byte(c) != byte_class::identifier) {
            break;
        }
        discard_peek();
        result.push_back(c);
    }
    return result;
}


std::unique_ptr<config_list> config_parser::parse_list() {
    discard_peek();
    auto list = std::make_unique<config_list>();
    for (;;) {
        skip_skippables();
        const auto maybe_byte = peek_byte();
        if(!maybe_byte) {
            throw std::runtime_error(std::string("List unterminated at end of file"));
        }
        switch(*maybe_byte) {
            case '}': {
                discard_peek();
                return list;
            }
            case ',':
                discard_peek();
                [[fallthrough]];
            default:
                list->list.emplace_back(parse_primitive_value());
                break;
        }
    }
}

// primitive_value:
//     'true'
//     'false'
//     INTEGER
//     STRING
std::unique_ptr<config_object> config_parser::parse_primitive_value()
{
    const auto is_numeric = [](const std::string& string) {
        if(string.empty()) {
            return false;
        }
        const std::size_t start_index = (string[0] == '-') ? 1 : 0;
        for(auto i = start_index; i < string.size(); ++i) {
            const auto c = string[i];
            if(c < '0' || c > '9') {
                return false;
            }
        }
        return true;
    };

    if(!skip_skippables()) {
        throw std::runtime_error("Expected a primitive value but end of input found");
    }
    if(peek_byte().value() == '"') {
        return parse_string();
    }
    if(peek_byte().value() == '{') {
        skip_skippables();
        return parse_list();
    }
    const auto identifier = parse_identifier();
    if(identifier == "true" || identifier == "false") {
        auto new_bool = std::unique_ptr<config_bool>(new config_bool);
        new_bool->value = identifier == "true";
        return new_bool;
    } else if(is_numeric(identifier)) {
        auto new_int = std::unique_ptr<config_int>(new config_int);
        new_int->value = std::stoi(identifier);
        return new_int;
    } else {
        throw std::runtime_error("Unexpected identifier: " + identifier);
    }
}

std::unique_ptr<config_string> config_parser::parse_string()
{
    discard_peek();
    auto new_string = std::unique_ptr<config_string>(new config_string);
    for(;;) {
        const auto maybe_byte = peek_byte();
        if(!maybe_byte) {
            throw std::runtime_error("End of input found within a string literal");
        }
        discard_peek();
        const auto c = *maybe_byte;
        if(c == '"') {
            break;
        } else if (c == '\\') {
        	continue;
        } else if (c == '\n') {
        	continue;
        }
        new_string->value.push_back(c);
    }
    return new_string;
}

bool config_parser::skip_skippables()
{
    for(;;) {
        const auto maybe_byte = peek_byte();
        if(!maybe_byte) {
            return false;
        }
        const auto c = *maybe_byte;
        if(c == '#') {
            discard_peek();
            if(!skip_comment()) {
                return false;
            }
        } else if(classify_byte(c) == byte_class::skippable) {
            discard_peek();
        } else {
            return true;
        }
    }
}

bool config_parser::skip_comment()
{
    for(;;) {
        const auto maybe_byte = peek_byte();
        if(!maybe_byte) {
            return false;
        }
        if(*maybe_byte == '\n') {
            return true;
        }
        discard_peek();
    }
}

std::optional<char> config_parser::peek_byte()
{
    const auto maybe_byte = source.peek();
    return (maybe_byte != std::char_traits<char>::eof())
        ? std::optional(static_cast<char>(maybe_byte))
        : std::nullopt;
}

void config_parser::discard_peek()
{
    source.get();
}

auto config_parser::classify_byte(char c) noexcept -> byte_class
{
    static auto byte_classes = []{
        std::array<byte_class, UCHAR_MAX> array = {};
        array.fill(byte_class::other);
        for(unsigned c = 'a'; c <= 'z'; ++c) {
            array[c] = byte_class::identifier;
        }
        for(unsigned c = 'A'; c <= 'Z'; ++c) {
            array[c] = byte_class::identifier;
        }
        for(unsigned c = '0'; c <= '9'; ++c) {
            array[c] = byte_class::identifier;
        }
        array['-'] = byte_class::identifier;
        array['_'] = byte_class::identifier;
        array[' '] = byte_class::skippable;
        array['#'] = byte_class::skippable;
        array[' '] = byte_class::skippable;
        array['\n'] = byte_class::skippable;
        array['\r'] = byte_class::skippable;
        array['\t'] = byte_class::skippable;
        array['\f'] = byte_class::skippable;
        array['\v'] = byte_class::skippable;
        return array;
    }();
    return byte_classes[static_cast<unsigned char>(c)];
}
