# mparse

## Introduction

This is a tiny library I wrote for fun after being tasked with writing a css parser (for a very small subset of css). I initially implemented that as a recursive decent parser but remembered back
to some cool stuff I learned about with Haskell and Monadic parsing. And since I'm working in C++ for
part of my work project I figured I'd try to write my own little monadic parser combinator toolkit in C++ 23.

## How does this thing work?

The best way to understand it is to look at some of [the tests](./mparse_test.cpp). Basically you start with a parser and then chain operators together to keep parsing more stuff, collecting data as needed
along the way.

```
    // A simple example.

    auto parser = parse_str("hello")
        .and_then(parse_literal(',').trim())
        .and_then(parse_str("world"));

    auto result = parser("hello, world");

    // Note that the result is the last matched parse.
    // If each token is needed you can use parse_seq,
    // or parse_n, or use and_then or transform.
    // also parse_delimited_by is a powerful way
    // to split up input.
    assert(result.value() == "world");

```
### Recursive parsing

Sometimes you need a parser that can refer to itself to handle recursive structures. For example a JSON value
can be a list of JSON values. A recursive parser takes a lamda that is passed a parser which becomes the parser returned by the lambda. Kind of mind bending but it works!

```
  // Example JSON parser (also see unit tests)

  struct Json {
    std::variant<int, std::string, bool, unit, std::vector<Json>,
               std::unordered_map<std::string, Json>>
    value;
  };

  template <class T>
  Json make_json_value(const T& val) {
    return Json{.value = val};
  }

  auto quote = parse_literal('"');
  auto open_curly = parse_literal('{').trim();
  auto close_curly = parse_literal('}').trim();
  auto open_square = parse_literal('[').trim();
  auto close_square = parse_literal(']').trim();
  auto comma = parse_literal(',').trim();
  auto colon = parse_literal(':').trim();

  // Elided for brevity. See tests.
  auto number = parsers::number();
  auto string = quote.and_then(parse_some(parse_not(quote))).skip(quote);

  auto boolean =
      parse_str("true").as(true).or_else(parse_str("false").as(false));

  auto null_ = parse_str("null").as(unit{});

  auto primitive = number.transform(make_json_value<int>)
                       .or_else(string
                                    .transform([](std::string_view val) {
                                      return std::string(val);
                                    })
                                    .transform(make_json_value<std::string>))
                       .or_else(boolean.transform(make_json_value<bool>))
                       .or_else(null_.transform(make_json_value<unit>));

  auto parser =
      parse_recursive<Json>([=](const Parser<Json>& json) {
        auto member =
            string.skip(colon).and_then([&json](std::string_view name) {
              return parse_ref(json).transform([name](const Json& val) {
                return make_pair(std::string(name), val);
              });
            });

        auto obj = open_curly.and_then(
            parse_delimited_by(member, comma, close_curly)
                .skip(close_curly)
                .transform([](const auto& members) {
                  std::unordered_map<std::string, Json> value(members.begin(),
                                                              members.end());
                  return make_json_value(value);
                }));

        auto list = open_square.and_then(
            parse_delimited_by(parse_ref(json), comma, close_square)
                .skip(close_square)
                .transform(make_json_value<std::vector<Json>>));

        return obj.or_else(list).or_else(primitive);
      }).skip(parse_end());
```

### Handling whitespace

Sometimes you want to ignore whitespace and sometimes you don't. For example, in the
stylesheet example we can ignore blank lines and spaces around delimiters like `'{'`, etc. but
for some property values whitespace becomes an important delimiter like in padding.

To deal with this the library has different ways to express parsing whitespace. Use

`parse_ws()` - When failing to find whitespace would be a parse error.

`parse_opt_ws()` - When whitespace is optional.

## Status

This is very early experimental code. There minimal error reporting. There may be bugs. There aren't enough tests. There will be breaking changes.

More combinators may be useful, like `parse_until` or `skip_until` for comments. It is not
hard to come up with more and build them out of the
existing parsers.

Input now is assumed to be in a single buffer and accessed via string_view. It is a great abstraction for hadling parsing but input will likely need to be enhanced to handler large input streams.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
