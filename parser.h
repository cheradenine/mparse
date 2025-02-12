#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <string>
#include <functional>
#include <optional>
#include <variant>

#include "convert.h"

namespace cparse
{
    template <typename T>
    using ParseResult = std::pair<std::optional<T>, std::string_view>;
    template <typename T>
    using Parser = std::function<ParseResult<T>(std::string_view)>;

    using unit = std::monostate;

    template <typename T, typename U>
    std::optional<U> fmap(std::optional<T> value, std::function<U(T)> f)
    {
        if (value)
        {
            return f(*value);
        }
        else
        {
            return std::nullopt;
        }
    }

    template<typename T, typename U>
    Parser<U> bind(Parser<T> parser, std::function<Parser<U>(T)> f) {
        return [=](std::string_view input) -> ParseResult<U> {
            auto [result, remaining] = parser(input);
            if (result) {
                return f(*result)(remaining);
            }
            return {std::nullopt, input};
        };
    }

    // Add return/pure operation
    template<typename T>
    Parser<T> pure(T value) {
        return [=](std::string_view input) -> ParseResult<T> {
            return {value, input};
        };
    }

    template <typename T, typename U>
    std::pair<T, U> combine(T t, U u)
    {
        return {t, u};
    }

    template<typename T>
    T combine(T t, unit u) {
        return t;
    }

    template<typename T>
    T combine(unit u, T t) {
        return t;
    }

    // only works when they are actually contiguous
    std::string_view combine(std::string_view t, std::string_view u)
    {
        return std::string_view(t.data(), t.size() + u.size());
    }

    int combine(int d1, int d2)
    {
        return d1 * 10 + d2;
    }

    template <typename T>
    struct sequence
    {
        void append(T t)
        {
            items.push_back(t);
        }
        std::vector<T> items;
    };

    template <>
    struct sequence<std::string_view>
    {
        // WARNING: must be contiguous
        void append(std::string_view s)
        {
            if (items.empty())
            {
                items = s;
            }
            else
            {
                items = std::string_view(items.data(), items.size() + s.size());
            }
        }
        std::string_view items;
    };

    template <>
    struct sequence<int>
    {
        void append(int d)
        {
            items = (items * 10) + d;
        }
        int items = 0;
    };

    template <typename T, typename U>
    using combine_result_t = decltype(combine(std::declval<T>(), std::declval<U>()));

    template <typename T, typename U>
    Parser<U> fmap(Parser<T> p, std::function<U(T)> fn)
    {
        return [=](std::string_view input)
        {
            auto [res, rem] = p(input);
            return std::make_pair(fmap(res, fn), rem);
        };
    }

    Parser<char> char_range(char first, char last)
    {
        return [=](std::string_view input) -> ParseResult<char>
        {
            char ch = input.front();
            if (ch >= first && ch <= last)
            {
                return {ch, input.substr(1)};
            }
            return {std::nullopt, input};
        };
    }

    Parser<std::string_view> char_class(std::function<bool(char)> f)
    {
        return [=](std::string_view input) -> ParseResult<std::string_view>
        {
            char ch = input.front();
            if (f(ch))
            {
                return {input.substr(0, 1), input.substr(1)};
            }
            return {std::nullopt, input};
        };
    }

    Parser<int> digit()
    {
        return [](std::string_view input) -> ParseResult<int>
        {
            auto [result, remainder] = char_range('0', '9')(input);
            if (result)
            {
                return {*result - '0', remainder};
            }
            return {std::nullopt, input};
        };
    }

    Parser<std::string_view> letter()
    {
        return char_class([](char ch)
                          { return std::isalpha(ch); });
    }

    Parser<unit> whitespace()
    {
        return [](std::string_view input)
        {
            size_t pos = 0;
            while (pos < input.size() && std::isspace(input[pos]))
            {
                ++pos;
            }
            return std::make_pair(unit{}, input.substr(pos));
        };
    }

    Parser<std::string_view> literal(char ch)
    {
        return [=](std::string_view input) -> ParseResult<std::string_view>
        {
            if (input.front() == ch)
            {
                return {input.substr(0,1), input.substr(1)};
            }
            return {std::nullopt, input};
        };
    }

    Parser<char> any_char();
    Parser<std::string_view> seq(const std::string &str);

    template <typename T>
    Parser<sequence<T>> some(Parser<T> p)
    {
        return [=](std::string_view input) -> ParseResult<sequence<T>>
        {
            sequence<T> results;
            std::string_view inp = input;
            int matched = 0;
            while (!inp.empty())
            {
                auto [result, remainder] = p(inp);
                if (!result.has_value())
                {
                    break;
                }
                results.append(result.value());
                inp = remainder;
            }
            if (inp.data() != input.data())
            {
                return std::make_pair(results, inp);
            }
            else
            {
                return std::make_pair(std::nullopt, input);
            }
        };
    }

    template<typename T>
    Parser<sequence<T>> count(int n, Parser<T> parser) {
        return [=](std::string_view input) -> ParseResult<std::vector<T>> {
            sequence<T> results;
            std::string_view remaining = input;
            
            for (int i = 0; i < n; i++) {
                auto [result, rem] = parser(remaining);
                if (!result) {
                    return {std::nullopt, input};
                }
                results.apppend(*result);
                remaining = rem;
            }
            return {results, remaining};
        };
    }

} // nemspace cparser

template <typename T, typename U>
cparse::Parser<cparse::combine_result_t<T, U>> operator>>(cparse::Parser<T> a, cparse::Parser<U> b)
{
    return [=](std::string_view input) -> cparse::ParseResult<cparse::combine_result_t<T, U>>
    {
        auto [resa, rema] = a(input);
        if (resa.has_value())
        {
            auto [resb, remb] = b(rema);
            if (resb.has_value())
            {
                auto combined = cparse::combine(resa.value(), resb.value());
                return std::make_pair(combined, remb);
                //                return std::make_pair(std::make_pair(resa.value(), resb.value()), remb);
            }
        }
        return std::make_pair(std::nullopt, input);
    };
}

template <typename T, typename U>
cparse::Parser<std::variant<T, U>> operator|(cparse::Parser<T> a, cparse::Parser<U> b)
{
    return [=](std::string_view input)
    {
        auto [result, remainder] = a(input);
        if (result)
        {
            return std::make_pair(result, remainder);
        }
        else
        {
            return b(input);
        }
    };
}

#endif // __PARSER_H__