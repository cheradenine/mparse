#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../parser.h"
#include "../style_sheet.h"

using ::testing::ElementsAre;
using ::testing::Eq;

template <typename T>
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
namespace parsers
{
    auto hexit = parse_range('A', 'F')
                     .transform([](auto sv)
                                { return static_cast<int>(10 + (sv.front() - 'A')); })
                     .or_else(
                         parse_range('a', 'f')
                             .transform([](auto sv)
                                        { return static_cast<int>(10 + (sv.front() - 'a')); }))
                     .or_else(
                         parse_range('0', '9')
                             .transform([](auto sv)
                                        { return static_cast<int>(sv.front() - '0'); }));

    auto hexbyte = parse_n(hexit, 1, 2).transform([](auto hexs)
                                                  {
        int val = 0;
        for (auto h : hexs) {
            val = (val << 4) + h;
        }
        return val; });

    Parser<int> number()
    {
        auto positive_number = parse_digit(1, 9).and_then([](int val)
                                                          { return parse_some(parse_digit()).transform([val](std::vector<int> digits)
                                                                                                       {
                int result = val;
                for (auto d : digits) {
                    result = (result * 10) + d;
                }
                return result; }); });

        auto zero = parse_digit(0, 0).and_not(parse_digit(0, 9));

        auto integer = positive_number
                           .or_else(zero)
                           .or_else(
                               parse_literal('-')
                                   .and_then(positive_number)
                                   .transform([](int val)
                                              { return -val; }));
        return integer;
    }
}
// Demonstrate some basic assertions.
TEST(ParserTest, ParseStringTest)
{
    auto parser = parse_str("hello");
    auto result = parser("hello");

    EXPECT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), Eq("hello"));
}

TEST(ParserText, ParseHelloWorld)
{

    auto parser = parse_str("hello")
                      .skip(parse_ws())
                      .and_then(parse_str(","))
                      .skip(parse_ws())
                      .and_then(parse_str("world"));

    auto result = parser("hello, world");
    assert(result.value() == "world");

    EXPECT_THAT(result.value(), Eq("world"));
}

TEST(ParserTest, ContainsStr)
{
    EXPECT_TRUE(detail::str_contains("hello", 'h'));
    EXPECT_FALSE(detail::str_contains("hello", 'x'));
}

TEST(ParserTest, HexNumbers)
{
    auto lowercase = {"a", "b", "c", "d", "e", "f"};
    int expected = 10;
    for (auto input : lowercase)
    {
        EXPECT_EQ(parsers::hexit(input).value(), expected);
        ++expected;
    }

    auto uppercase = {"A", "B", "C", "D", "E", "F"};
    expected = 10;
    for (auto input : uppercase)
    {
        EXPECT_EQ(parsers::hexit(input).value(), expected);
        ++expected;
    }

    auto digits = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    expected = 0;
    for (auto input : digits)
    {
        EXPECT_EQ(parsers::hexit(input).value(), expected);
        ++expected;
    }

    EXPECT_FALSE(parsers::hexit("q"));
    EXPECT_FALSE(parsers::hexit("R"));

    EXPECT_EQ(parsers::hexbyte("0F").value(), 0x0F);
    EXPECT_EQ(parsers::hexbyte("AA").value(), 0xAA);
    EXPECT_EQ(parsers::hexbyte("7F").value(), 0x7F);
    EXPECT_EQ(parsers::hexbyte("80").value(), 0x80);

    EXPECT_FALSE(parsers::hexbyte("G7"));
    EXPECT_FALSE(parsers::hexbyte("-1"));
}

TEST(ParserTest, ParseNumber)
{
    // These are complicated enough that they should probably go in the library
    auto integer = parsers::number();

    EXPECT_EQ(integer("0").value(), 0);
    EXPECT_EQ(integer("123").value(), 123);
    EXPECT_EQ(integer("-123").value(), -123);
    EXPECT_FALSE(integer("01"));
    EXPECT_FALSE(integer("-0"));
}

TEST(ParserTest, DelimitedBy)
{
    auto token_parser = parse_n(parse_any_of("abcd"), 1, 2);
    auto parser = parse_delimited_by(
        token_parser, parse_literal(','), parse_literal(';'));

    auto result = parser("a,bc,d;");
    ASSERT_TRUE(result);
    EXPECT_THAT(result.value(), ElementsAre("a", "bc", "d"));
    EXPECT_EQ(result.input.front(), ';');
}

TEST(ParserTest, DelimitedByMultiple)
{
    auto token = parse_some(parse_any_of("abcde"));
    auto delimiter = parse_ignoring_ws(parse_literal(','));
    auto terminator = parse_literal(';');

    auto parser = parse_delimited_by(token, delimiter, terminator);

    auto result = parser("a ,bc, d,e;");
    ASSERT_TRUE(result);
    EXPECT_THAT(result.value(),
                ElementsAre("a", "bc", "d", "e"));
}

// TEST(ParserTest, DelimitedByMultiple) {
//     auto token_parser = parse_n(parse_any_of("abcde"),1,20).skip(parse_ws());
//     auto delimiter = parse_literal(',').skip(parse_ws());
//     auto parser = parse_delimited_by(
//         token_parser, delimiter, parse_literal(';'));

//     auto result = parser("a ,bc, d,e;");
//     ASSERT_TRUE(result);
//     EXPECT_THAT(result.value(),
//         ElementsAre("a", "bc", "d", "e"));
// }

TEST(ParserTest, AnyOf)
{
    auto parser = parse_any_of("abc&!");

    auto some = parse_some(parser);
    auto result = some("!!cb&baa");

    EXPECT_TRUE(result);
    EXPECT_THAT(result.value(), Eq("!!cb&baa"));
}

TEST(ParserTest, ParseN)
{
    auto parser = parse_n(parse_any_of("abc"), 1, 2);
    auto result = parser("ab");

    EXPECT_TRUE(result);
    EXPECT_THAT(result.value(), Eq("ab"));

    result = parser("bd");
    EXPECT_TRUE(result);
    // EXPECT_STRV_EQ(result.value(), "b");
    EXPECT_EQ(result.input.front(), 'd');
}

TEST(ParserTest, AndNot)
{
    auto parser = parse_n(
        parse_range('a', 'z').and_not(parse_literal('x')), 4);

    auto result = parser("abyz");
    ASSERT_TRUE(result);

    result = parser("uvxy");
    ASSERT_FALSE(result);
}

TEST(ParserTest, RGB)
{
    auto hex = parse_str("0x").and_then(parsers::hexbyte);
    auto delimiter = parse_ignoring_ws(parse_literal(','));
    auto parser = parse_str("rgb")
                      .skip(parse_ws(true))
                      .and_then(parse_literal('('))
                      .and_then(
                          parse_delimited_by(hex, delimiter, parse_literal(')')))
                      .skip(parse_literal(')'));

    auto result = parser("rgb(0xFF, 0xA0, 0x45)");
    EXPECT_TRUE(result);
    EXPECT_EQ(result.value().size(), 3);
    EXPECT_THAT(result.value(), ElementsAre(0xFF, 0xA0, 0x45));
    EXPECT_TRUE(result.input.empty());
}

TEST(ParserTest, Spacing)
{

    auto dimension_parser = parsers::number().and_then([](auto value) { 
        return parse_str("px").as(Dimension{
            .value = value,
            .units = Dimension::px
        })
        .or_else(parse_literal('%').as(Dimension{
            .value = value,
            .units = Dimension::pct
        })); 
    });

    auto spacing =
        parse_delimited_by(dimension_parser, parse_ws(false), parse_literal(';'))
            .transform([](const std::vector<Dimension> &values) -> Spacing {
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
            return sp; });
    auto result = spacing("10px 22px;");
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value().top.value == 10);
}
