#include "parser.h"

namespace cparse
{
    namespace
    {
        using ResultList = std::vector<std::string_view>;

        static ParseResult empty_result;

        using CharMatcher = std::function<bool(int)>;

        Parser match_char(CharMatcher matcher)
        {
            auto parser = [=](std::string_view input)
            {
                if (matcher(input.front()))
                {
                    auto result = input.substr(0, 1);
                    input.remove_prefix(1);
                    return std::make_pair(ResultList({result}), input);
                }
                else
                {
                    return empty_result;
                }
            };
            return parser;
        }
    }

    Parser digit()
    {
        return match_char(std::isdigit);
    }

    Parser letter()
    {
        return match_char(std::isalpha);
    }

    bool is_whitespace(int ch)
    {
        return std::isblank(ch) || ch == '\n';
    }
    Parser whitespace()
    {
        return match_char(is_whitespace);
    }
    Parser literal(char ch)
    {
        return match_char([ch](char c)
                          { return ch == c; });
    }

    Parser any_char()
    {
        return [](std::string_view input)
        {
            if (input.empty())
            {
                return empty_result;
            }
            auto res = input.substr(0, 1);
            input.remove_prefix(1);
            return std::make_pair(
                ResultList({res}), input);
        };
    }

    Parser seq(const std::string &str)
    {
        return [=](std::string_view input)
        {
            if (input.starts_with(str))
            {
                auto res = input.substr(0, str.size());
                input.remove_prefix(str.size());
                return std::make_pair(
                    ResultList({res}), input);
            }
            return empty_result;
        };
    }

    Parser some(Parser p)
    {
        auto parser = [=](std::string_view input)
        {
            std::string_view inp = input;
            int matched = 0;
            while (!inp.empty())
            {
                auto [result, remaining] = p(inp);
                if (result.empty())
                {
                    break;
                }
                else
                {
                    inp = remaining;
                    ++matched;
                }
            }
            if (matched)
            {
                return std::make_pair(
                    ResultList({input.substr(0, matched)}),
                    inp);
            }
            else
            {
                return empty_result;
            }
        };
        return parser;
    };
}

using namespace cparse;

Parser operator>>(Parser a, Parser b)
{
    return [=](std::string_view input)
    {
        // parse a, then b, contact rhe results
        auto [result, remainder] = a(input);
        if (!result.empty())
        {
            auto [resultb, remainderb] = b(remainder);
            if (!resultb.empty())
            {
                result.insert(result.end(), resultb.begin(), resultb.end());
                return std::make_pair(result, remainderb);
            }
        }
        return empty_result;
    };
}

Parser operator|(const Parser &a, const Parser &b)
{
    return [=](std::string_view input)
    {
        auto result = a(input);
        return result.first.size() ? result : b(input);
    };
}