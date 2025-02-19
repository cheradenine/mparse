#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <variant>
#include <cstdarg>
#include <optional>
#include <charconv>
#include <system_error>

#include "parser.h"
#include "style_sheet.h"

// A parser for things
// is a function from strings
// to lists of pairs of strings and things.

template<typename T>
void dump_result(const std::vector<T> &res)
{
    std::cout << "[";
    for (auto it = res.begin(); it != res.end(); ++it)
    {
        std::cout << *it;
        if ((it + 1) != res.end())
        {
            std::cout << ",";
        }
    }
    std::cout << "]";
}

Parser<int> parse_number() {
    auto positive_number = parse_digit(1, 9).and_then([] (int val) {
        return parse_some(parse_digit()).transform([val] (std::vector<int> digits) {
            int result = val;
            for (auto d : digits) {
                result = (result * 10) + d;
            }
            return result;
        });
    });

    auto zero = parse_digit(0, 0).and_not(parse_digit(0, 9));

    return positive_number
        .or_else(zero)
        .or_else(
            parse_literal('-')
            .and_then(positive_number)
            .transform([] (int val) { return -val; })
        );
}

// For style sheet - esque sample.

Parser<Dimension> parse_dimension()
{
    auto dimension_parser = parse_number().and_then([](auto value) {
        return parse_str("px").as(Dimension {
                .value = value,
                .units = Dimension::px
        }).or_else(parse_literal('%').as(Dimension {
                .value = value,
                .units = Dimension::pct
        }));
    });
    return dimension_parser;
}

Parser<Spacing> parse_spacing()
{
    auto spacing =
        parse_delimited_by(parse_dimension(), parse_ws(true), parse_literal(';'))
        .transform([] (const std::vector<Dimension>& values) -> Spacing {
            Spacing sp;
            dump_result(values);
            std::cout << std::endl;
            switch (values.size()) {
                case 1:
                    sp.top = sp.right = sp.bottom = sp.left = values[0];
                    return sp;
                case 2:
                    sp.top = sp.bottom = values[0];
                    sp.right = sp.left = values[1];
                    return sp;
                case 3:
                    sp.top = values[0];
                    sp.left = sp.right  = values[1];
                    sp.bottom = values[2];
                    return sp;
                case 4:
                    sp.top = values[0];
                    sp.right = values[1];
                    sp.bottom = values[2];
                    sp.left = values[3];
                    return sp;
            }
            return sp;
        });
    return spacing;
}

int decode_hex_str(std::string_view str) {
    char buf[3] = {str[0], str[1], '\0'};
    return (uint8_t)(std::strtoul(buf, nullptr, 16));
}

int combine_digits(const std::vector<int> digits) {
    int val = 0;
    for (auto d : digits) {
        val = (val * 10) + d;
    }
    return val;
}

auto parse_hex_digit = detail::parse_char_class([](char ch) {
    ch = toupper(ch);
    return (ch >= '0' || ch <= '9') || (ch >= 'A' || ch <= 'F');
});

Parser<uint8_t> parse_byte() {
    auto parser =
        parse_str("0x").or_else(parse_str("0X"))
        .and_then(parse_n(parse_hex_digit, 2).transform(decode_hex_str))
        .or_else(parse_n(parse_digit(), 1, 3).transform(combine_digits));
    return parser.transform([](int val) {return static_cast<uint8_t>(val); });
}

Parser<Color> parse_color()
{
    auto is_hexit = [](char ch) {
        ch = toupper(ch);
        return (ch >= '0' || ch <= '9') || (ch >= 'A' || ch <= 'F');
    };

    auto parse_hex_digit(detail::parse_char_class(is_hexit));

    auto hex_color_parser = 
        parse_literal('#')
        .and_then(parse_n(parse_hex_digit, 6))
        .transform([](std::string_view value) {
            Color color;
            color.r = decode_hex_str(value.substr(0, 2));
            color.g = decode_hex_str(value.substr(2, 4));
            color.b = decode_hex_str(value.substr(4, 6));
            return color;
        });

    auto delimiter = parse_ignoring_ws(parse_literal(','));
    auto rgb_parser = parse_str("rgb")
        .skip(parse_ws())
        .and_then(parse_literal('('))
        .and_then(
            parse_delimited_by(parse_byte(), delimiter, parse_literal(')')))
        .skip(parse_literal(')'))
        .transform([] (auto values) {
            return Color{
                .r = values[0],
                .g = values[1],
                .b = values[2]
            };
        });

    return hex_color_parser.or_else(rgb_parser);
}

template<typename T>
Parser<Rule> parse_rule(std::string_view property, Parser<T> rule_value) {
    return rule_value().transform([property](const T& value) {
        return Rule {
            .property = std::string(property),
            .value = value
        };
    });
}

Parser<Rule> parse_dimension_rule(std::string_view property) {
    return parse_dimension().transform([property](const Dimension& dim) {
        return Rule {
            .property = std::string(property),
            .value = dim
        };
    });
}

Parser<Rule> parse_color_rule(std::string_view property) {
    return parse_color().transform([property](const Color& color) {
        return Rule {
            .property = std::string(property),
            .value = color
        };
    });
}

Parser<Rule> parse_spacing_rule(std::string_view property) {
    return parse_spacing().transform([property](const Spacing& spacing) {
        std::cout << "spacing = " << spacing << std::endl;
        return Rule {
            .property = std::string(property),
            .value = spacing
        };
    });
}

void print_stylesheet(const StyleSheet& ss) {
    for (auto it = ss.selectors.begin(); it != ss.selectors.end(); ++it) {
        std::cout << it->first << ":" << std::endl;
        for (auto rule : it->second) {
            std::cout << "  " << rule.property << " = ";
            std::visit([](auto&& arg){
                std::cout << arg << std::endl;
            }, rule.value);
        }
    }
}

void ParseStyleSheet(std::string_view input) {

    auto variable = parse_sequence({
        parse_any_of("_.#").or_else(parse_alpha()),
        parse_some(parse_alnum())
    });

    auto rule = variable
        .skip(parse_ws())
        .skip(parse_literal(':'))
        .skip(parse_ws())
        .and_then([](std::string_view prop_name) {
            // TODO:
            // This is where we will inject a function that takes a prop_name
            // and retruns the apropriate parser.
            return parse_dimension_rule(prop_name)
//            .or_else(parse_spacing_rule(prop_name))
            .or_else(parse_color_rule(prop_name));
        })
        .skip(parse_literal(';'))
        .skip(parse_ws());

    auto selector = variable
        .skip(parse_ws())
        .skip(parse_literal('{'))
        .skip(parse_ws())
        .and_then([rule](std::string_view sel_name) {
            return parse_some(rule).transform([sel_name](const std::vector<Rule>& rules) {
                return make_pair(sel_name, rules);
            });
        })
        .skip(parse_ws())
        .skip(parse_literal('}'))
        .skip(parse_ws());

    auto styles = parse_some(selector).transform([](auto selectors) {
        StyleSheet ss;
        for (auto [selector, rules] : selectors) {
            ss.selectors[std::string(selector)] = rules;
        }
        return ss;
    });

    auto result = styles(input);
    if (result) {
        if (result.error.size()) {
            std::cerr << "It says it worked but: " << result.error << std::endl;
        }
        print_stylesheet(result.value());
    } else {
        std::cerr << "fail at " << result.input << std::endl;
    }
}

std::string read_file(std::string_view filename)
{
  std::ifstream f(filename, 0);
  if (!f) {
    throw std::runtime_error( "failed to open file" );
  }

  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "enter a filename" << std::endl;
        return -1;
    }

    std::string content = read_file(argv[1]);

    ParseStyleSheet(content);
}
