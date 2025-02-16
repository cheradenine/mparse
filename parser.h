#ifndef __PARSER_H__
#define __PARSER_H__

#include <string>
#include <string_view>
#include <optional>
#include <functional>
#include <vector>

using unit = std::monostate;
template <class T>
struct ParseResult
{
    std::optional<T> result;
    std::string_view input;

    operator bool() const { return result.has_value(); }
    bool operator!() const { return !result.has_value(); }
    bool has_value() const { return result.has_value(); }
    T& value() { return *result; }
    const T& value() const { return *result; }
};

// Helper functions for constructing ParseResults
template<typename T>
ParseResult<T> make_parse_result(T value, std::string_view remaining) {
    return ParseResult<T>{
        .result = std::move(value),
        .input = remaining
    };
}

template<typename T>
ParseResult<T> empty_parse_result(std::string_view input) {
    return ParseResult<T>{
        .result = std::nullopt,
        .input = input
    };
}
template <class T>
class Parser
{
public:
    using value_type = T;
    using Parse = std::function<ParseResult<T>(std::string_view)>;

    Parser(Parse parse) : parse_(parse) {}

    ParseResult<T> operator()(std::string_view input) const
    {
        return parse_(input);
    }

    Parser<T> or_else(Parser<T> parser) const
    {
        Parse self = parse_;
        return Parser<T>([self, parser](std::string_view input) {
            auto result = self(input);
            if (!result) {
                return parser(input);
            }
            return result;
        });
    }

    // fn is a function from T to a Parser<U>
    template <typename F>
    requires (!std::is_base_of_v<Parser<typename std::invoke_result_t<F, T>::value_type>, F>)
    auto and_then(F&& fn) const
    {
        using ReturnParser = std::invoke_result_t<F, T>;
        using U = typename ReturnParser::value_type; // Extract U from Parser<U>
        Parser self = parse_;
        return Parser<U>([self, fn](std::string_view input) {
            auto result = self(input);
            if (!result) {
                return empty_parse_result<U>(input);
            }
            ReturnParser then_parser = fn(result.value());
            return then_parser(result.input);
        });
    }

    template<typename U>
    auto and_then(const Parser<U>& next) const {
        Parser self = parse_;
        return Parser<U>([self, next](std::string_view input) {
            auto result = self(input);
            if (!result) {
                return empty_parse_result<U>(input);
            }
            return next(result.input);
        });
    }

    template<typename U>
    auto skip(const Parser<U>& next) const {
        Parser self = parse_;
        return Parser<T>([self, next](std::string_view input) {
            auto result = self(input);
            if (!result) {
                return empty_parse_result<T>(input);
            }
            auto next_result = next(result.input);
            if (!next_result) {
                return empty_parse_result<T>(result.input);
            }
            return make_parse_result(
                result.value(),
                next_result.input
            );
        });
    }

    template<typename F>
    auto transform(F&& fn) const {
        using U = std::invoke_result_t<F, T>;
        Parser self = parse_;
        return Parser<U>([self, fn](std::string_view input) {
            auto result = self(input);
            if (!result) {
                return empty_parse_result<U>(input);
            }
            return make_parse_result<U>(
                fn(result.value()),
                result.input);
        });
    }

    template<typename U>
    auto as(U value) const {
        return transform([value](auto&&) -> U {
            return value;
        });
    }

private:
    Parse parse_;
};

using StringParser = Parser<std::string_view>;

template<typename T>
Parser<T> parse_never() {
    return Parser<T>([](std::string_view input) {
        return empty_parse_result<T>(input);
    });
}

template<typename T>
Parser<T> pure(T value) {
    return Parser<T>([value](std::string_view input) {
        return make_parse_result(value, input);  // Succeeds with value, doesn't consume input
    });
}

namespace detail {

    StringParser parse_char_class(std::function<bool(char)> matcher) {
        return StringParser([matcher] (std::string_view input){
            if (!input.empty()) {
                char ch = input.front();
                if (matcher(ch)) {
                    return make_parse_result<std::string_view>(
                        input.substr(0, 1),
                        input.substr(1)
                    );
                }
            }
            return empty_parse_result<std::string_view>(input);
        });
    }
    
    bool str_contains(std::string_view str, char ch) {
        for (char c : str) {
            if (c == ch) {
                return true;
            }
        }
        return false;
    }    
}

StringParser parse_literal(char ch)
{
    return StringParser([ch](std::string_view input) {
        if (!input.empty() && input.front() == ch) {
            return make_parse_result(input.substr(0, 1),
                input.substr(1));
        } else {
            return empty_parse_result<std::string_view>(input);
        }
    });
}

StringParser parse_range(char first, char last)
{
    return StringParser([first, last](std::string_view input) {
        if (!input.empty()) {
            char ch = input.front();
            if (ch >= first && ch <= last) {
                return make_parse_result(input.substr(0, 1),
                    input.substr(1));
            }
        }
        return empty_parse_result<std::string_view>(input);
    });
}

StringParser parse_str(std::string_view str)
{
    return StringParser([str](std::string_view input) {
        if (input.starts_with(str)) {
            return make_parse_result(
                input.substr(0, str.size()),
                input.substr(str.size())
            );
        } else {
            return empty_parse_result<std::string_view>(input);
        } 
    });
}

// Matches a char if it is in the set of chars in src.
StringParser parse_any_of(std::string_view str)
{
    return StringParser([str](std::string_view input){
        if (!detail::str_contains(str, input.front())) {
            return empty_parse_result<std::string_view>(input);
        }
        return make_parse_result(input.substr(0, 1), input.substr(1));
    });
}

Parser<int> parse_digit(int first = 0, int last = 9)
{
    return Parser<int>([first, last](std::string_view input) {
        if (!input.empty()) {
            char ch = input.front();
            if (std::isdigit(ch)) {
                int digit  = ch - '0';
                if (digit >= first && digit <= last) {
                    return make_parse_result(digit, input.substr(1));
                }
            }
        }
        return empty_parse_result<int>(input);
    });
}

template <typename T>
Parser<std::vector<T>> parse_some(Parser<T> parser)
{
    return Parser<std::vector<T>>([parser](std::string_view input) {
        std::vector<T> results;
        while (!input.empty()) {
            auto result = parser(input);
            if (!result) {
                //std::cerr << "did not parse " << input << std::endl;
                break;
            }
            results.push_back(result.value());
            input = result.input;
        }
        return make_parse_result(results, input);
    });
}

template <typename T>
Parser<std::vector<T>> parse_n(Parser<T> parser, int min, std::optional<int> max = std::nullopt)
{
    return Parser<std::vector<T>>([parser, min, max](std::string_view input) {
        std::vector<T> results;
        int count = 0;
        int count_max = max.value_or(min);

        while (!input.empty() && count < count_max) {
            auto result = parser(input);
            if (!result) {
                break;
            }
            results.push_back(result.value());
            input = result.input;
            
            ++count;
        }
        if (count < min) {
            return empty_parse_result<std::vector<T>>(input);
        }
        return make_parse_result(results, input);
    });
}

StringParser parse_some(StringParser parser)
{
    return StringParser([parser](std::string_view input) {
        size_t count = 0;
        std::string_view inp = input;
        while (!inp.empty()) {
            auto result = parser(inp);
            if (!result) {
                break;
            }
            count += result.value().size();
            inp = result.input;
        }
        // if (count == 0) {
        //     return empty_parse_result<std::string_view>(input);
        // }
        return make_parse_result(input.substr(0, count), inp);
    });
}

StringParser parse_n(StringParser parser, int min, std::optional<int> max = std::nullopt)
{
    return StringParser([parser, min, max](std::string_view input) {
        int count = 0;
        int count_max = max.value_or(min);
        size_t pos = 0;
        std::string_view inp = input;
        while (!inp.empty() && count < count_max) {
            auto result = parser(inp);
            if (!result) {
                break;
            }
            pos += result.value().size();
            inp = result.input;
            ++count;
        }
        if (count < min) {
            return empty_parse_result<std::string_view>(inp);
        }
        return make_parse_result(input.substr(0, pos), inp);
    });
}

StringParser parse_sequence(std::initializer_list<StringParser> parsers) {
    std::vector<StringParser> ps(parsers);
    return StringParser([ps] (std::string_view input) {
        size_t count = 0;
        std::string_view inp = input;
        for (auto parser: ps) {
            if (input.empty()) {
                return empty_parse_result<std::string_view>(inp);
            }
            auto result = parser(inp);
            if (!result) {
                return result;
            }
            count += result.value().size();
            inp = result.input;
        }
        return make_parse_result<std::string_view>(
            input.substr(0, count),
            input.substr(count)
        );
    });
}

template<typename T, typename D, typename S>
Parser<std::vector<T>> parse_delimited_by(
    Parser<T> parser,
    Parser<D> delimiter,
    Parser<S> terminator,
//    std::string_view terminator,
    std::optional<int> max = std::nullopt
) {
    return Parser<std::vector<T>>([=](std::string_view input) {
        std::vector<T> results;

//        auto parse_delim = parse_any_of(delimiter);
        auto parse_term = terminator; //parse_any_of(terminator);
        int times = 10;

        while(!input.empty()) {
            if (parse_term(input)) {
                break;
            }

            bool valid = false;
            auto item = parser(input);
            if (item) {
                results.emplace_back(std::move(item.value()));
                input = item.input;
                valid = true;
            }
            auto delim_result = delimiter(input);
            if (delim_result) {
                input = delim_result.input;
                valid = true;
            }

            if (!valid) {
                return empty_parse_result<std::vector<T>>(input);
            }

            if (--times == 0) {
                break;
            }
        }
        return make_parse_result(results, input);
    });
}

StringParser parse_alpha() {
    return detail::parse_char_class([] (char ch) { return std::isalpha(ch); } );
}

StringParser parse_alnum() {
    return detail::parse_char_class([] (char ch) { return std::isalnum(ch); } );
}

Parser<unit> whitespace() {
    return Parser<unit>([] (std::string_view input) {
        while (!input.empty() && (std::isblank(input.front()) || input.front() == '\n')) {
            input.remove_prefix(1);
        }
        return make_parse_result(unit{}, input);
    });
}

template <typename T>
Parser<T> recursive_parser(std::function<Parser<T>(const Parser<T>&)> parser_builder) {
    auto recursive_parser_ptr = std::make_shared<Parser<T>>(parse_never<T>());

    // Build the parser using the parser_builder lambda
    Parser<T> full_parser = parser_builder(*recursive_parser_ptr);

    *recursive_parser_ptr = full_parser;

    return *recursive_parser_ptr;
}

#endif // __PARSER_H__