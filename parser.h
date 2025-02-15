#ifndef __PARSER_H__
#define __PARSER_H__

#include <string>
#include <optional>
#include <functional>
template <class T>
struct ParseResult
{
    std::optional<T> result;
    std::string_view input;

    operator bool() const { return result.has_value(); }
    bool operator!() const { return !result.has_value(); }
    bool has_value() const { return result.has_value(); }
    T &value() { return *result; }
};

// Helper functions for constructing ParseResults
template<typename T>
ParseResult<T> parse_result(T value, std::string_view remaining) {
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

    template<typename F>
    auto transform(F&& fn) const {
        using U = std::invoke_result_t<F, T>;
        Parser self = parse_;
        return Parser<U>([self, fn](std::string_view input) {
            auto result = self(input);
            if (!result) {
                return empty_parse_result<U>(input);
            }
            return parse_result<U>(
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

template<typename T>
Parser<T> pure(T value) {
    return Parser<T>([value](std::string_view input) {
        return make_result(value, input);  // Succeeds with value, doesn't consume input
    });
}

Parser<std::string_view> parse_literal(char ch)
{
    return Parser<std::string_view>([ch](std::string_view input) {
        if (input.front() == ch) {
            return parse_result(input.substr(0, 1),
                input.substr(1));
        } else {
            return empty_parse_result<std::string_view>(input);
        }
    });
}

Parser<std::string_view> parse_range(char first, char last)
{
    return Parser<std::string_view>([first, last](std::string_view input) {
        char ch = input.front();
        if (ch >= first && ch <= last) {
            return parse_result(input.substr(0, 1),
                input.substr(1));
        } else {
            return empty_parse_result<std::string_view>(input);
        }
    });
}

Parser<std::string_view> parse_str(std::string_view str)
{
    return Parser<std::string_view>([str](std::string_view input) {
        if (input.starts_with(str)) {
            return parse_result(
                input.substr(0, str.size()),
                input.substr(str.size())
            );
        } else {
            return empty_parse_result<std::string_view>(input);
        } 
    });
}

Parser<int> parse_digit(int first = 0, int last = 9)
{
    return Parser<int>([first, last](std::string_view input) {
        char ch = input.front();
        if (std::isdigit(ch)) {
            int value  = ch - '0';
            if (value >= first && value <= last) {
                return parse_result(ch - '0', input.substr(1));
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
        return parse_result(results, input);
    });
}

template <typename T>
Parser<std::vector<T>> parse_n(Parser<T> parser, int n)
{
    return Parser<std::vector<T>>([parser, n](std::string_view input) {
        std::vector<T> results;
        int required = n;
        while (!input.empty() && required--) {
            auto result = parser(input);
            if (!result) {
                //std::cerr << "did not parse " << input << std::endl;
                break;
            }
            results.push_back(result.value());
            input = result.input;
        }
        if (required > 0) {            // did not consume all input
            return empty_parse_result<std::vector<T>>(input);
        }
        return parse_result(results, input);
    });
}

Parser<std::string_view> parse_some(Parser<std::string_view> parser)
{
    return Parser<std::string_view>([parser](std::string_view input) {
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
        return parse_result(input.substr(0, count), inp);
    });
}

Parser<std::string_view> parse_n(Parser<std::string_view> parser, int n)
{
    return Parser<std::string_view>([parser, n](std::string_view input) {
        size_t count = 0;
        int required = n;
        std::string_view inp = input;
        while (!inp.empty() && required--) {
            auto result = parser(inp);
            if (!result) {
                break;
            }
            count += result.value().size();
            inp = result.input;
        }
        if (required > 0) {
            return empty_parse_result<std::string_view>(inp);
        }
        return parse_result(input.substr(0, count), inp);
    });
}

#endif // __PARSER_H__