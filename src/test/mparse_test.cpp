#include "../parser.h"
#include "../style_sheet.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;
using ::testing::Eq;

namespace {
template <typename T>
std::ostream& dump_vec(std::ostream& out, const std::vector<T>& values) {
  out << "[";
  for (auto it = values.begin(); it != values.end(); ++it) {
    out << *it;
    if ((it + 1) != values.end()) {
      out << ",";
    }
  }
  out << "]";

  return out;
}

template <class T>
std::ostream& dump_map(std::ostream& out,
                       const std::unordered_map<std::string, T>& values) {
  out << '{';
  size_t count = values.size();
  size_t idx = 0;
  for (auto it = values.begin(); it != values.end(); ++it, ++idx) {
    auto& [name, value] = *it;
    out << '"' << name << "\":" << value;
    if ((idx + 1) != count) {
      out << ',';
    }
  }
  out << '}';
  return out;
}
}  // namespace

namespace parsers {
auto hexit = parse_range('A', 'F')
                 .or_else(parse_range('a', 'f'))
                 .transform([](auto sv) {
                   char ch = std::toupper(sv.front());
                   return static_cast<int>(10 + (ch - 'A'));
                 })
                 .or_else(parse_range('0', '9').transform([](auto sv) {
                   return static_cast<int>(sv.front() - '0');
                 }));

auto hexbyte = parse_n(hexit, 1, 2).transform([](auto hexs) {
  int val = 0;
  for (auto h : hexs) {
    val = (val << 4) + h;
  }
  return val;
});

Parser<int> number() {
  auto positive_number = parse_digit(1, 9).and_then([](int val) {
    return parse_some(parse_digit()).transform([val](std::vector<int> digits) {
      int result = val;
      for (auto d : digits) {
        result = (result * 10) + d;
      }
      return result;
    });
  });

  auto zero = parse_digit(0, 0).and_not(parse_digit(0, 9));

  auto integer = positive_number.or_else(zero).or_else(
      parse_literal('-').and_then(positive_number).transform([](int val) {
        return -val;
      }));
  return integer;
}
}  // namespace parsers
// Demonstrate some basic assertions.
TEST(ParserTest, ParseStringTest) {
  auto parser = parse_str("hello");
  auto result = parser("hello");

  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(result.value(), Eq("hello"));
}

TEST(ParserTest, ParseHelloWorld) {
  auto parser = parse_str("hello")
                    .skip(parse_opt_ws())
                    .and_then(parse_str(","))
                    .skip(parse_opt_ws())
                    .and_then(parse_str("world"));

  auto result = parser("hello, world");
  assert(result.value() == "world");

  EXPECT_THAT(result.value(), Eq("world"));
}

TEST(ParserTest, ContainsStr) {
  EXPECT_TRUE(detail::str_contains("hello", 'h'));
  EXPECT_FALSE(detail::str_contains("hello", 'x'));
}

TEST(ParserTest, HexNumbers) {
  auto lowercase = {"a", "b", "c", "d", "e", "f"};
  int expected = 10;
  for (auto input : lowercase) {
    EXPECT_EQ(parsers::hexit(input).value(), expected);
    ++expected;
  }

  auto uppercase = {"A", "B", "C", "D", "E", "F"};
  expected = 10;
  for (auto input : uppercase) {
    EXPECT_EQ(parsers::hexit(input).value(), expected);
    ++expected;
  }

  auto digits = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
  expected = 0;
  for (auto input : digits) {
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

TEST(ParserTest, ParseDigit) {
  auto digit = parse_digit();
  EXPECT_TRUE(digit("0").value() == 0);
  auto digit_with_range = parse_digit(2, 4);
  EXPECT_FALSE(digit_with_range("1").has_value());
  EXPECT_TRUE(digit_with_range("2").has_value());
  EXPECT_TRUE(digit_with_range("3").has_value());
  EXPECT_TRUE(digit_with_range("4").has_value());
}

TEST(ParserTest, ParseNumber) {
  // These are complicated enough that they should probably go in the library
  auto integer = parsers::number();

  EXPECT_EQ(integer("0").value(), 0);
  EXPECT_EQ(integer("1").value(), 1);
  EXPECT_EQ(integer("123").value(), 123);
  EXPECT_EQ(integer("-123").value(), -123);
  EXPECT_FALSE(integer("01"));
  EXPECT_FALSE(integer("-0"));
}

TEST(ParserTest, DelimitedBy) {
  auto token_parser = parse_n(parse_any_of("abcd"), 1, 2);
  auto parser =
      parse_delimited_by(token_parser, parse_literal(','), parse_literal(';'));

  auto result = parser("a,bc,d;");
  ASSERT_TRUE(result);
  EXPECT_THAT(result.value(), ElementsAre("a", "bc", "d"));
  EXPECT_EQ(result.input.front(), ';');
}

TEST(ParserTest, DelimitedByMultiple) {
  auto token = parse_some(parse_any_of("abcde"));
  auto delimiter = parse_ignoring_ws(parse_literal(','));
  auto terminator = parse_literal(';');

  auto parser = parse_delimited_by(token, delimiter, terminator);

  auto result = parser("a ,bc, d,e;");
  ASSERT_TRUE(result);
  EXPECT_THAT(result.value(), ElementsAre("a", "bc", "d", "e"));
}

TEST(ParserTest, AnyOf) {
  auto parser = parse_any_of("abc&!");

  auto some = parse_some(parser);
  auto result = some("!!cb&baa");

  EXPECT_TRUE(result);
  EXPECT_THAT(result.value(), Eq("!!cb&baa"));
}

TEST(ParserTest, ParseN) {
  auto parser = parse_n(parse_any_of("abc"), 1, 2);
  auto result = parser("ab");

  EXPECT_TRUE(result);
  EXPECT_THAT(result.value(), Eq("ab"));

  result = parser("bd");
  EXPECT_TRUE(result);
  // EXPECT_STRV_EQ(result.value(), "b");
  EXPECT_EQ(result.input.front(), 'd');
}

TEST(ParserTest, AndNot) {
  auto parser = parse_n(parse_range('a', 'z').and_not(parse_literal('x')), 4);

  auto result = parser("abyz");
  ASSERT_TRUE(result);

  result = parser("uvxy");
  ASSERT_FALSE(result);
}

TEST(ParserTest, RGB) {
  auto hex = parse_str("0x").and_then(parsers::hexbyte);
  auto delimiter = parse_ignoring_ws(parse_literal(','));
  auto parser =
      parse_str("rgb")
          .skip(parse_opt_ws())
          .and_then(parse_literal('('))
          .and_then(parse_delimited_by(hex, delimiter, parse_literal(')')))
          .skip(parse_literal(')'));

  auto result = parser("rgb(0xFF, 0xA0, 0x45)");
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value().size(), 3);
  EXPECT_THAT(result.value(), ElementsAre(0xFF, 0xA0, 0x45));
  EXPECT_TRUE(result.input.empty());
}

int decode_hex_str(std::string_view str) {
  char buf[3] = {str[0], str[1], '\0'};
  return (uint8_t)(std::strtoul(buf, nullptr, 16));
}

TEST(ParserTest, HexColor) {
  auto is_hexit = [](char ch) {
    ch = toupper(ch);
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F');
  };

  auto parse_hex_digit(detail::parse_char_class(is_hexit));

  auto hex_color_parser =
      parse_literal('#')
          .and_then(parse_n(parse_hex_digit, 6))
          .transform([](std::string_view value) {
            return Color{.r = decode_hex_str(value.substr(0, 2)),
                         .g = decode_hex_str(value.substr(2, 4)),
                         .b = decode_hex_str(value.substr(4, 6))};
          });
  //    EXPECT_TRUE(hex_color_parser("#004488;"));
  auto result = hex_color_parser("#A87F01;");
  Color c = result.value();
  EXPECT_THAT(c.r, Eq(0xA8));
  EXPECT_THAT(c.g, Eq(0x7F));
  EXPECT_THAT(c.b, Eq(0x01));
  EXPECT_THAT(result.input.front(), Eq(';'));
}

TEST(ParserTest, Spacing) {
  auto dimension_parser = parsers::number().and_then([](auto value) {
    return parse_str("px")
        .as(Dimension{.value = value, .units = Dimension::px})
        .or_else(parse_literal('%').as(
            Dimension{.value = value, .units = Dimension::pct}));
  });

  auto spacing =
      parse_delimited_by(dimension_parser, parse_ws(), parse_literal(';'))
          .transform([](const std::vector<Dimension>& values) -> Spacing {
            Spacing sp;
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
                sp.left = sp.right = values[1];
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
  auto result = spacing("10px 22px;");
  ASSERT_TRUE(result);
  EXPECT_TRUE(result.value().top.value == 10);
  EXPECT_TRUE(result.value().right.value == 22);
  EXPECT_THAT(result.input.front(), Eq(';'));
}

TEST(ParserTest, RecursiveParser) {
  Parser<int> parse_term = parse_recursive<int>([](const Parser<int>& term) {
    return parsers::number().or_else(
        parse_literal('(').and_then(parse_ref(term)).skip(parse_literal(')')));
  });

  EXPECT_TRUE(parse_term("1"));
  EXPECT_THAT(parse_term("(20)").value(), Eq(20));
}

TEST(ParserTest, Expression) {
  // expr ::= term + expr | term - expr | term
  // term ::= factor * term | factor / term | factor
  // factor ::= (expression) | number

  Parser<int> parse_expr = parse_recursive<int>([](const Parser<int>& expr) {
    Parser<int> parse_factor = parsers::number().or_else(
        parse_literal('(').and_then(parse_ref(expr)).skip(parse_literal(')')));

    Parser<int> parse_term = parse_recursive<int>([=](const Parser<int>& term) {
      return parse_factor.skip(parse_literal('*'))
          .and_then([&term](int lhs) {
            return parse_ref(term).transform(
                [lhs](int rhs) { return lhs * rhs; });
          })
          .or_else(parse_factor);
    });

    return parse_term.skip(parse_literal('+'))
        .and_then([&expr](int lhs) {
          return parse_ref(expr).transform(
              [lhs](int rhs) { return lhs + rhs; });
        })
        .or_else(parse_term);
  });

  EXPECT_THAT(parse_expr("1+2").value(), Eq(3));
  EXPECT_THAT(parse_expr("2*8").value(), Eq(16));
  EXPECT_THAT(parse_expr("1+2*8").value(), Eq(17));
  EXPECT_THAT(parse_expr("(1+2)*8").value(), Eq(24));
  EXPECT_THAT(parse_expr("(1+2)*(5+3)").value(), Eq(24));
}

struct Json {
  std::variant<int, std::string, bool, std::vector<Json>,
               std::unordered_map<std::string, Json>>
      value;
};

std::ostream& operator<<(std::ostream& out, const std::vector<Json>& vec) {
  return dump_vec(out, vec);
}

std::ostream& operator<<(std::ostream& out,
                         const std::unordered_map<std::string, Json>& map) {
  return dump_map(out, map);
}

std::ostream& operator<<(std::ostream& out, const Json& js) {
  std::visit([&out](auto&& arg) { out << arg; }, js.value);
  return out;
}

template <class T>
Json make_json_value(const T& val) {
  return Json{.value = val};
}

TEST(ParserTest, ParseJson) {
  //    <json> ::= <primitive> | <container>
  //    <primitive> ::= <number> | <string> | <boolean>
  //    <container> ::= <object> | <array>
  //    <array> ::= '[' [ <json> *(', ' <json>) ] ']' ; A sequence of JSON
  //    values separated by commas <object> ::= '{' [ <member> *(', ' <member>)
  //    ] '}' ; A sequence of 'members' <member> ::= <string> ': ' <json> ; A
  //    pair consisting of a name, and a JSON value
  auto quote = parse_literal('"');
  auto open_curly = parse_ignoring_ws(parse_literal('{'));
  auto close_curly = parse_ignoring_ws(parse_literal('}'));
  auto open_square = parse_ignoring_ws(parse_literal('['));
  auto close_square = parse_ignoring_ws(parse_literal(']'));
  auto comma = parse_ignoring_ws(parse_literal(','));
  auto colon = parse_ignoring_ws(parse_literal(':'));

  auto number = parsers::number();
  auto string = quote.and_then(parse_some(parse_not(quote))).skip(quote);

  auto boolean =
      parse_str("true").as(true).or_else(parse_str("false").as(false));

  auto primitive = number.transform(make_json_value<int>)
                       .or_else(string
                                    .transform([](std::string_view val) {
                                      return std::string(val);
                                    })
                                    .transform(make_json_value<std::string>))
                       .or_else(boolean.transform(make_json_value<bool>));

  auto parser = parse_recursive<Json>([=](const Parser<Json>& json) {
    auto member = string.skip(colon).and_then([&json](std::string_view name) {
      return parse_ref(json).transform([name](const Json& val) {
        return make_pair(std::string(name), val);
      });
    });

    auto obj = open_curly.and_then([=](auto _) {
      return parse_delimited_by(member, comma, close_curly)
          .skip(close_curly)
          .transform(
              [](const std::vector<std::pair<std::string, Json>>& members) {
                std::unordered_map<std::string, Json> value(members.begin(),
                                                            members.end());
                return make_json_value(value);
              });
    });

    auto list = open_square.and_then([=, &json](auto _) {
      return parse_delimited_by(parse_ref(json), comma, close_square)
          .skip(close_square)
          .transform(make_json_value<std::vector<Json>>);
    });

    return obj.or_else(list).or_else(primitive);
  });

  EXPECT_TRUE(parser("100"));
  EXPECT_TRUE(parser("true"));
  EXPECT_TRUE(parser("\"a string\""));
  EXPECT_TRUE(parser("[1,2,3]"));

  auto result = parser(R"(
  {
    "x": {
        "name": "Fred",
        "age": 99
    },
    "y": [1, 2, 3, true, { "a": "bc" }],
    "z": {
        "email":"somebody@examle.com",
        "phone": "(123) - 456 - 7890",
        "json": true
    }
  }
  )");

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    std::cout << result.value() << std::endl;
  } else {
    std::cout << result.error << std::endl;
  }
}
