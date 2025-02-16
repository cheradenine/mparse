#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "parser.h"

#define EXPECT_STRV_EQ(s1, s2) \
    do { std::string s1s(s1); std::string s2s(s2); EXPECT_STREQ(s1s.c_str(), s2s.c_str()); } while(0)
// Demonstrate some basic assertions.
TEST(ParserTest, ParseStringTest) {
  auto parser = parse_str("hello");
  auto result = parser("hello");

  EXPECT_TRUE(result.has_value());
  // can't use string view??
//  std::string x(result.value());
  EXPECT_STRV_EQ(result.value(), "hello");
}

TEST(ParserTest, ContainsStr) {
    EXPECT_TRUE(detail::str_contains("hello", 'h'));
    EXPECT_FALSE(detail::str_contains("hello", 'x'));
}

TEST(ParserTest, ParseNumber) {
    // These are complicated enough that they should probably go in the library
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

    auto integer = positive_number
        .or_else(zero)
        .or_else(
            parse_literal('-')
            .and_then(positive_number)
            .transform([] (int val) { return -val; })
        );

    EXPECT_EQ(integer("0").value(), 0);
    EXPECT_EQ(integer("123").value(), 123);
    EXPECT_EQ(integer("-123").value(), -123);
    EXPECT_FALSE(integer("01"));
    EXPECT_FALSE(integer("-0"));
}

TEST(ParserTest, DelimitedBy) {
    auto token_parser = parse_n(parse_any_of("abcd"),1,2);
    auto parser = parse_delimited_by(
        token_parser, parse_literal(','), parse_literal(';'));

    auto result = parser("a,bc,d;");
    EXPECT_TRUE(result);
    EXPECT_STRV_EQ(result.value()[0], "a");
    EXPECT_STRV_EQ(result.value()[1], "bc");
    EXPECT_STRV_EQ(result.value()[2], "d");
    EXPECT_EQ(result.input.front(), ';');
}

TEST(ParserTest, DelimitedByMultiple) {
    auto token_parser = parse_n(parse_any_of("abcde"),1,20);
    auto parser = parse_delimited_by(
        token_parser, parse_any_of(", "), parse_literal(';'));

    auto result = parser("a ,bc, d,e ;");
    EXPECT_TRUE(result);
    EXPECT_STRV_EQ(result.value()[0], "a");
    EXPECT_STRV_EQ(result.value()[1], "bc");
    EXPECT_STRV_EQ(result.value()[2], "d");
    EXPECT_STRV_EQ(result.value()[3], "e");
}

TEST(ParserTest, AnyOf) {
    auto parser = parse_any_of("abc&!");

    auto some = parse_some(parser);
    auto result = some("!!cb&baa");

    EXPECT_TRUE(result);
    EXPECT_STRV_EQ(result.value(), "!!cb&baa");
}

TEST(ParserTest, ParseN) {
    auto parser = parse_n(parse_any_of("abc"), 1, 2);
    auto result = parser("ab");

    EXPECT_TRUE(result);
    EXPECT_STRV_EQ(result.value(), "ab");

    result = parser("bd");
    EXPECT_TRUE(result);
    //EXPECT_STRV_EQ(result.value(), "b");
    EXPECT_EQ(result.input.front(), 'd');
}

TEST(ParserTest, AndNot) {
    auto parser = parse_n(
        parse_range('a', 'z').and_not(parse_literal('x')), 4
    );

    auto result = parser("abyz");
    ASSERT_TRUE(result);

    result = parser("uvxy");
    ASSERT_FALSE(result);
}

TEST(ParserTest, RGB) {
    auto token_parser = parse_n(parse_digit(), 1, 3).transform([](const std::vector<int>& vals)
    {
        int val = 0;
        for (auto v : vals) {
            val = (val * 10) + v;
        }
        return val;
    });

    auto delimiter = parse_any_of(", ");

    auto parser = parse_str("rgb")
        .skip(whitespace())
        .and_then(parse_literal('('))
        .and_then(
            parse_delimited_by(token_parser, parse_any_of(", "), parse_literal(')')))
        .skip(parse_literal(')'));

    auto result = parser("rgb(12, 240, 45)");
    EXPECT_TRUE(result);
    EXPECT_EQ(result.value().size(), 3);
    EXPECT_THAT(result.value(), ::testing::ElementsAre(12, 240, 45));
}
