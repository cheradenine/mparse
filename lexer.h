#ifndef __LEXER_H__
#define __LEXER_H__

#include <functional>
#include <optional>
#include <string>

using TokenResult = std::pair<std::optional<std::string_view>, std::string_view>;

struct LexResult {
    std::optional<std::string_view> token;
    std::string_view input;

    std::string_view value() { return token.value(); }
    bool operator!() const { return !token.has_value(); }
    bool has_value() const { return token.has_value(); }

    template<class F>
    LexResult or_else(F&& f) && {
        if (!token.has_value()) {
            return f.lex(input);
        }
        return *this;
    }
};

using LexFn = std::function<LexResult(std::string_view)>;

struct Lexer
{
    LexFn lexer;

    Lexer(LexFn fn) : lexer(fn) {}

    LexResult operator()(std::string_view input) const {
        auto [result, remaining] = lexer(input);
        return LexResult{
            .token = result,
            .input = result.has_value() ? remaining : input
        };
    }

    Lexer or_else(Lexer fn) const {
        LexFn self = lexer;
        return Lexer([=](std::string_view input) -> LexResult {
            auto [result, remaining] = self(input);
            if (result.has_value()) {
                return LexResult{
                    .token = result,
                    .input = remaining
                };
            }
            return fn(input);
        });
    }

    LexResult lex(std::string_view input) const
    {
        auto [result, remaining] = lexer(input);
        return LexResult{
            .token = result,
            .input = result.has_value() ? remaining : input
        };
    }
};

// matchers
// ch -> bool
using CharMatcher = std::function<bool(char)>;

CharMatcher match_char(char ch)
{
    return [=](char c)
    { return c == ch; };
}

CharMatcher match_char_range(char first, char last)
{
    return [=](char c)
    { return c >= first && c <= last; };
}

template <class F>
CharMatcher match_char_class(F fn)
{
    return [=](char c)
    { return fn(c); };
}

template <class Matcher>
Lexer lex_n(Matcher matcher, int n)
{
    return Lexer([matcher, n](std::string_view input) -> LexResult
    {
        int times = n;
        auto it = input.begin();
        for (; times && it != input.end(); --times, ++it)
        {
            if (!matcher(*it))
            {
                return {std::nullopt, input};
            }
        }
        size_t count = it - input.begin();
        return {input.substr(0, count), input.substr(count)};
    });
}

template <class Matcher>
Lexer lex(Matcher matcher) {
    return lex_n(matcher, 1);
}

Lexer lex_char(char ch) {
    return lex(match_char(ch));
}

template <class Matcher>
Lexer lex_some(Matcher matcher)
{
    auto fn = [=](std::string_view input) -> LexResult
    {
        auto it = input.begin();
        for (; it != input.end(); ++it)
        {
            if (input.empty() || !matcher(*it))
            {
                break;
            }
        }
        size_t count = it - input.begin();
        return {input.substr(0, count), input.substr(count)};
    };
    return Lexer(fn);
}

Lexer lex_word(std::string_view word)
{
    auto fn = [=](std::string_view input) -> LexResult
    {
        if (!input.starts_with(word))
        {
            return {std::nullopt, input};
        }
        size_t count = word.size();
        return {input.substr(0, count), input.substr(count)};
    };
    return Lexer(fn);
}

Lexer lex_seq(std::initializer_list<Lexer> lexersl)
{
    std::vector<Lexer> lexers(lexersl);
    auto fn = [lexers](std::string_view input) -> LexResult
    {
        size_t count = 0;
        auto inp = input;
        for (auto it = lexers.begin(); it != lexers.end(); ++it)
        {
            if (inp.empty()) {
                return {std::nullopt, input};
            }
            auto result = (*it)(inp);
            if (!result)
            {
                return result;
            }
            count += result.value().size();
            inp = result.input;
        }
        return LexResult{
            .token = input.substr(0, count),
            .input = input.substr(count)
        };
    };
    return Lexer(fn);
}

#endif // __LEXER_H__