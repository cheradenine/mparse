#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <variant>
#include <cstdarg>
#include <optional>
#include <charconv>
#include <system_error>

#include "parser.h"
#include "lexer.h"

// A parser for things
// is a function from strings
// to lists of pairs of strings and things.
// using Parser = std::function

void dump_result(const std::vector<int> &res)
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

int combine_digits(std::vector<int> digits)
{
    int value = 0;
    for (auto d : digits)
    {
        value = value * 10 + d;
    }
    return value;
}

struct Dimension
{
    int value;
    enum
    {
        pct,
        px,
    } units;
};

void ParseNumber(std::string_view input) {
    auto positive_number = parse_digit(1, 9).and_then([] (int val) {
        return parse_some(parse_digit()).transform([val] (std::vector<int> digits) {
            int result = val;
            for (auto d : digits) {
                result = (result * 10) + d;
            }
            return result;
        });
    });

    auto number = parse_literal('-')
        .and_then(positive_number)
        .transform([](int val) { return -val; })
        .or_else(positive_number);

    auto result = positive_number(input);
    if (result) {
        std::cout << result.value() << std::endl;
    }

    result = number(input);
    if (result) {
        std::cout << result.value() << std::endl;
    }
}

void ParseDimension(std::string_view input)
{
    auto digits = parse_some(parse_digit());

    auto parse_dimension = digits.and_then([=](auto values) {
        Dimension dim;
        auto value = combine_digits(values);

        return parse_str("px").as(Dimension {
                .value = value,
                .units = Dimension::px
        }).or_else(parse_literal('%').as(Dimension {
                .value = value,
                .units = Dimension::pct
        })); 
    });

    auto result = parse_dimension(input);
    if (result)
    {
        Dimension dim = result.value();
        std::cout << dim.value << " "
                  << (dim.units == Dimension::px ? "pixels" : "percent")
                  << std::endl;
    }
}

void TestParse(std::string_view input)
{
    std::cout << "parsing " << input << std::endl;
    ParseDimension(input);
    return;

    auto parser1 = parse_str("hi").or_else(parse_str("bye"));
    auto parse_as_or_bs = parse_some(parse_literal('a').or_else(parse_literal('b')));

    auto result = parse_as_or_bs(input);
    if (result)
    {
        std::cout << result.value() << std::endl;
    }
    else
    {
        std::cerr << "fail" << std::endl;
    }

    auto positive_integer = parse_some(parse_digit());

    auto expr = parse_literal('(').and_then([](auto _)
                                            { return parse_some(parse_digit()); })
                    .and_then([](auto digits) -> Parser<int>
                              {
        auto value = combine_digits(digits);
        return parse_literal(')').transform([value](auto _) -> int {
            return value;  // Return the number instead of the ')'
        }); });

    auto expr_result = expr(input);
    if (expr_result)
    {
        std::cout << expr_result.value() << std::endl;
    }
}

void TestLex(std::string_view input)
{
    auto digit = match_char_class([](char ch)
                                  { return std::isdigit(ch); });
    auto alnum = match_char_class([](char ch)
                                  { return std::isalnum(ch); });
    auto alpha = match_char_class([](char ch)
                                  { return std::isalpha(ch); });

    auto ws = lex_some(match_char_class([](char ch)
                                        { return std::isblank(ch); }));
    // positive only
    auto _1digit = lex_n(digit, 1);
    auto _digits = lex_some(digit);

    // 100px or
    // 100%

    auto integer = lex_seq({lex(digit), lex_some(digit)});

    auto percent = lex_char('%');
    auto semicolon = lex_char(';');
    auto px = lex_word("px");

    auto dim = lex_seq({integer,
                        px.or_else(percent),
                        ws,
                        semicolon});

    auto lex = dim; // .or_else(lex_some(alpha));
    auto res = lex(input);

    if (!res.has_value())
    {
        std::cerr << "fail: " << res.input << std::endl;
    }
    else
    {
        std::cout << res.value() << std::endl;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "enter a string" << std::endl;
        return -1;
    }
    ParseNumber(argv[1]);
//    TestParse(argv[1]);
    //    TestLex(argv[1]);
}

#if 0
void TestParse(std::string_view input)
{
    auto digits = cparse::some(cparse::digit());
    auto letters = cparse::some(cparse::letter());
    auto ws = cparse::whitespace();

    auto integer = cparse::fmap(
        std::function<int(std::vector<int>)>(combine_digits),
        digits);

    auto alnum = cparse::char_class([](char ch)
                                    { return std::isalnum(ch); });

    auto variable = cparse::letter() >> cparse::some(alnum);

    {
        auto parser = variable >> ws >> integer;

        auto [result, remaining] = parser(input);
        if (!result)
        {
            std::cerr << "fail: " << remaining << std::endl;
            return;
        }
        auto val = *result;
        std::cout << std::get<0>(val) << ',' << std::get<1>(val) << std::endl;
    }
}
#endif