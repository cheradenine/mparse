#ifndef __PARSER_H__
#define __PARSER_H__

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>

#include <fmt/format.h>

using unit = std::monostate;

template<class T>
class Parser;
template <class T>
struct ParseResult
{
    // Maybe result should be std::expect?
    std::optional<T> result;
    std::string_view input;
    std::string error;

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
ParseResult<T> empty_parse_result(std::string_view input, const std::string& error) {
    if (!error.empty()) {
//        std::cerr << error << std::endl;
    }
    return ParseResult<T>{
        .result = std::nullopt,
        .input = input,
        .error = error
    };
}

namespace detail {
    template<typename T>
    struct parser_impl {
        template<typename U>
        static auto skip(const Parser<T>& parser, const Parser<U>& next) {
            return Parser<T>([parser, next](std::string_view input) {
                auto result = parser(input);
                if (!result) {
                    return empty_parse_result<T>(input, result.error);
                }
                auto next_result = next(result.input);
                if (!next_result) {
                    return empty_parse_result<T>(result.input, next_result.error);
                }
                return make_parse_result(
                    result.value(),
                    next_result.input
                );
            });
        }
    };

    struct discontinuous_tag {};
    struct discontinuous_string_view : std::string_view, discontinuous_tag {
        discontinuous_string_view(const std::string_view& other) 
            : std::string_view(other) {}
        discontinuous_string_view& operator=(const std::string_view& other) {
            std::string_view::operator=(other);
            return *this;
        }        
    };

    template<typename T>
    using result_vector_t = std::conditional_t<
        std::is_same_v<T, std::string_view>,
        std::vector<discontinuous_string_view>,
        std::vector<T>
    >;


    template<class T>
    std::ostream& operator<<(std::ostream& out, const std::vector<T>& xs) {
        out << "[";
        for (auto it = xs.begin(); it != xs.end(); ++it) {
            out << *it;
            if (it + 1 != xs.end()) {
                out << ",";
            }
        }
        out << "]";
        return out;
    }

    std::ostream& operator<<(std::ostream& out, const unit _) {
        out << "unit";
        return out;
    }

    template<class T>
    std::ostream& operator<<(std::ostream& out, const ParseResult<T>& res) {
        if (res) {
            out << res.value();
        } else {
            out << res.error;
        }
        return out;
    }
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
                return empty_parse_result<U>(input, result.error);
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
                return empty_parse_result<U>(input, result.error);
            }
            return next(result.input);
        });
    }

    template<typename U>
    Parser<T> and_not(const Parser<U>& next) const {
        Parser self = parse_;
        return Parser<T>([self, next](std::string_view input) {
            auto result = self(input);
            if (!result) {
                return empty_parse_result<T>(input, result.error);
            }
            auto next_result = next(result.input);
            if (next_result) {
                return empty_parse_result<T>(
                    input,
                    fmt::format("Expected failure but parsed {}", next_result.value())
                );
            }
            return result;
        });
    }

    template<typename U>
    auto skip(const Parser<U>& next) const {
        return detail::parser_impl<T>::skip(*this, next);
    }    

    template<typename F>
    auto transform(F&& fn) const {
        using U = std::invoke_result_t<F, T>;
        Parser self = parse_;
        return Parser<U>([self, fn](std::string_view input) {
            auto result = self(input);
            if (!result) {
                return empty_parse_result<U>(input, result.error);
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

namespace detail {
template<>
struct parser_impl<std::string_view> {
    // When skipping over input the result becomes discontinuous
    template<typename U>
    static auto skip(const StringParser& parser, const Parser<U>& next) {
        Parser<detail::discontinuous_string_view> to_dc = parser.transform([](auto value) {
            return detail::discontinuous_string_view(value);
        });
        return to_dc.skip(next);
    }
};

Parser<detail::discontinuous_string_view> to_discontinuous(StringParser parser) {
    return parser.transform([](auto value) {
        return detail::discontinuous_string_view(value);
    });
}

}

template<typename T>
Parser<T> parse_never() {
    return Parser<T>([](std::string_view input) {
        return empty_parse_result<T>(input, "Error: never");
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
            return empty_parse_result<std::string_view>(
                input,
                fmt::format("Error: unexpected char {}", input.front())
            );
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
            return empty_parse_result<std::string_view>(
                input,
                fmt::format("Expected {} but saw {}", ch, input.front()));
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
        return empty_parse_result<std::string_view>(
            input,
            fmt::format("Error: expected [{}-{}] but saw {}", first, last, input.front()));
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
            return empty_parse_result<std::string_view>(
                input,
                fmt::format("Error: expected {} but saw {}", str, input)
            );
        } 
    });
}

// Matches a char if it is in the set of chars in src.
StringParser parse_any_of(std::string_view str)
{
    return StringParser([str](std::string_view input){
        if (!detail::str_contains(str, input.front())) {
            return empty_parse_result<std::string_view>(
                input,
            fmt::format("Error: expected any of {} but saw {}", str, input.front()));
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
        return empty_parse_result<int>(
            input,
            fmt::format("Error: expected digit from [{} - {}] but saw {}", first, last, input.front())
        );
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

        std::string error;
        while (!input.empty() && count < count_max) {
            auto result = parser(input);
            if (!result) {
                error = result.error;
                break;
            }
            results.push_back(result.value());
            input = result.input;
            
            ++count;
        }
        if (count < min) {
            return empty_parse_result<std::vector<T>>(
                input,
                fmt::format(
                    "Error: expected to match {} times but saw {}\n\tInner: {}",
                        min, count, error)
            );
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
        std::string error;
        while (!inp.empty() && count < count_max) {
            auto result = parser(inp);
            if (!result) {
                error = result.error;
                break;
            }
            pos += result.value().size();
            inp = result.input;
            ++count;
        }
        if (count < min) {
            return empty_parse_result<std::string_view>(
                inp,
                fmt::format(
                    "Error: expected {} occurences but only saw {}\n\tInner: {}",
                    min, count, error)

            );
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
                return empty_parse_result<std::string_view>(
                    inp,
                    fmt::format("Error: reached end of input.")
                );
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
Parser<detail::result_vector_t<T>> parse_delimited_by(
    Parser<T> parser,
    Parser<D> delimiter,
    Parser<S> terminator,
    std::optional<int> max = std::nullopt
) {
    using ResultType = detail::result_vector_t<T>;
    return Parser<ResultType>([=](std::string_view input) {
        std::cout << "-> delimited_by" << std::endl;

        auto tokens_result = parse_some(parser.skip(delimiter))(input);
        detail::operator<<(std::cout, tokens_result) << std::endl;
        if (!tokens_result) {
            return empty_parse_result<ResultType>(input, tokens_result.error);
        }
        input = tokens_result.input;
        std::cout << "  input now: " << input << std::endl;
//        auto last_token = parser.skip(terminator)(input);
        auto last_token_result = parser(input);
        detail::operator<<(std::cout, last_token_result) << std::endl;
        if (!last_token_result) {
            return empty_parse_result<ResultType>(input, last_token_result.error);
        }
        // now expect the terminator
        auto term_result = terminator(last_token_result.input);
        if (!term_result) {
            return empty_parse_result<ResultType>(term_result.input, term_result.error);
        }
        std::cout << "  input now: " << last_token_result.input << std::endl;

        std::cout << "<- delimited_by" << std::endl;

        auto results = tokens_result.value();
        results.push_back(last_token_result.value());

        return make_parse_result(results, last_token_result.input);
    });
}

StringParser parse_alpha() {
    return detail::parse_char_class([] (char ch) { return std::isalpha(ch); } );
}

StringParser parse_alnum() {
    return detail::parse_char_class([] (char ch) { return std::isalnum(ch); } );
}

Parser<unit> parse_ws(bool optional = true) {
    return Parser<unit>([optional] (std::string_view input) {
        bool saw_ws = false;
        while (!input.empty() && (std::isblank(input.front()) || input.front() == '\n')) {
            input.remove_prefix(1);
            saw_ws = true;
        }
        if (!optional && !saw_ws) {
            return empty_parse_result<unit>(input, "Error: failed to parse whitespace");
        }
        return make_parse_result(unit{}, input);
    });
}

template<typename T, typename U>
Parser<T> parse_ignoring(Parser<T> parser, Parser<U> ignore) {
    return parser.skip(ignore).or_else(ignore.and_then(parser).skip(ignore));
}

template<typename U>
Parser<detail::discontinuous_string_view> parse_ignoring(StringParser parser, Parser<U> ignore) {
    return parse_ignoring(detail::to_discontinuous(parser), ignore);
}

template<typename T>
Parser<T> parse_ignoring_ws(Parser<T> parser) {
    return parse_ignoring(parser, parse_ws());
}

Parser<detail::discontinuous_string_view> parse_ignoring_ws(StringParser parser) {
    return parse_ignoring(parser, parse_ws());
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