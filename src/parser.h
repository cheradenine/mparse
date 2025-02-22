#ifndef __PARSER_H__
#define __PARSER_H__

#include <fmt/format.h>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <assert.h>

using unit = std::monostate;

template <class T>
class Parser;
template <class T>
struct ParseResult {
  // Maybe result should be std::expect?
  std::optional<T> result;
  std::string_view input;
  std::string error;

  operator bool() const { return result.has_value(); }
  bool operator!() const { return !result.has_value(); }
  bool has_value() const { return result.has_value(); }
  T& value() { return *result; }
  T const& value() const { return *result; }
};

// Helper functions for constructing ParseResults
template <typename T>
ParseResult<T> make_parse_result(T value, std::string_view remaining) {
  return ParseResult<T>{.result = std::move(value), .input = remaining};
}

template <typename T>
ParseResult<T> empty_parse_result(std::string_view input,
                                  std::string const& error) {
  return ParseResult<T>{.result = std::nullopt, .input = input, .error = error};
}

namespace detail {
template <typename T>
struct parser_impl {
  template <typename U>
  static auto skip(Parser<T> const& parser, Parser<U> const& next) {
    return Parser<T>([parser, next](std::string_view input) {
      auto result = parser(input);
      if (!result) {
        return empty_parse_result<T>(input, result.error);
      }
      auto next_result = next(result.input);
      if (!next_result) {
        return empty_parse_result<T>(result.input, next_result.error);
      }
      return make_parse_result(result.value(), next_result.input);
    });
  }
};

struct discontinuous_tag {};
struct discontinuous_string_view : std::string_view, discontinuous_tag {
  discontinuous_string_view(std::string_view const& other)
      : std::string_view(other) {}
  discontinuous_string_view& operator=(std::string_view const& other) {
    std::string_view::operator=(other);
    return *this;
  }
};

template <typename T>
using result_vector_t =
    std::conditional_t<std::is_same_v<T, std::string_view>,
                       std::vector<discontinuous_string_view>, std::vector<T>>;

template <class T>
std::vector<T> append(T const& item, std::vector<T>& vec) {
  vec.push_back(item);
  return vec;
}

std::string_view append(std::string_view item, std::string_view sv) {
  assert(sv.data() + sv.size() == item.data());
  return std::string_view(sv.data(), sv.size() + item.size());
}

template <class T>
std::ostream& operator<<(std::ostream& out, std::vector<T> const& xs) {
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

std::ostream& operator<<(std::ostream& out, unit const _) {
  out << "unit";
  return out;
}

template <class T>
std::ostream& operator<<(std::ostream& out, ParseResult<T> const& res) {
  if (res) {
    out << res.value();
  } else {
    out << res.error;
  }
  return out;
}
}  // namespace detail

template <class T>
class Parser {
 public:
  using value_type = T;
  using Parse = std::function<ParseResult<T>(std::string_view)>;

  Parser(Parse parse) : parse_(parse) {}

  ParseResult<T> operator()(std::string_view input) const {
    return parse_(input);
  }

  Parser<T> or_else(Parser<T> parser) const {
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
    requires(!std::is_base_of_v<
             Parser<typename std::invoke_result_t<F, T>::value_type>, F>)
  auto and_then(F&& fn) const {
    using ReturnParser = std::invoke_result_t<F, T>;
    using U = typename ReturnParser::value_type;  // Extract U from Parser<U>
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

  template <typename U>
  auto and_then(Parser<U> const& next) const {
    Parser self = parse_;
    return Parser<U>([self, next](std::string_view input) {
      auto result = self(input);
      if (!result) {
        return empty_parse_result<U>(input, result.error);
      }
      return next(result.input);
    });
  }

  template <typename U>
  Parser<T> and_not(Parser<U> const& next) const {
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
            fmt::format("Expected failure but parsed {}", next_result.value()));
      }
      return result;
    });
  }

  template <typename U>
  auto skip(Parser<U> const& next) const {
    return detail::parser_impl<T>::skip(*this, next);
  }

  template <typename F>
  auto transform(F&& fn) const {
    using U = std::invoke_result_t<F, T>;
    Parser self = parse_;
    return Parser<U>([self, fn](std::string_view input) {
      auto result = self(input);
      if (!result) {
        return empty_parse_result<U>(input, result.error);
      }
      return make_parse_result<U>(fn(result.value()), result.input);
    });
  }

  template <typename U>
  auto as(U value) const {
    return transform([value](auto&&) -> U { return value; });
  }

 private:
  Parse parse_;
};

using StringParser = Parser<std::string_view>;

namespace detail {
template <>
struct parser_impl<std::string_view> {
  // When skipping over input the result becomes discontinuous
  template <typename U>
  static auto skip(StringParser const& parser, Parser<U> const& next) {
    Parser<detail::discontinuous_string_view> to_dc = parser.transform(
        [](auto value) { return detail::discontinuous_string_view(value); });
    return to_dc.skip(next);
  }
};

Parser<detail::discontinuous_string_view> to_discontinuous(
    StringParser parser) {
  return parser.transform(
      [](auto value) { return detail::discontinuous_string_view(value); });
}

}  // namespace detail

template <typename T>
Parser<T> parse_never() {
  return Parser<T>([](std::string_view input) {
    return empty_parse_result<T>(input, "Error: never");
  });
}

template <typename T>
Parser<T> pure(T value) {
  return Parser<T>([value](std::string_view input) {
    return make_parse_result(
        value, input);  // Succeeds with value, doesn't consume input
  });
}

namespace detail {

// Helper for all the std::is* functions for chars.
StringParser parse_char_class(std::function<int(int)> matcher) {
  return StringParser([matcher](std::string_view input) {
    if (matcher(static_cast<int>(input.front())) != 0) {
      return make_parse_result<std::string_view>(input.substr(0, 1),
                                                 input.substr(1));
    }
    return empty_parse_result<std::string_view>(
        input, fmt::format("Error: unexpected char {}", input.front()));
  });
}

bool str_contains(std::string_view str, char ch) {
  return std::find_if(str.begin(), str.end(),
                      [ch](char c) { return c == ch; }) != str.end();
}
}  // namespace detail

StringParser parse_literal(char ch) {
  return StringParser([ch](std::string_view input) {
    if (!input.empty() && input.front() == ch) {
      return make_parse_result(input.substr(0, 1), input.substr(1));
    } else {
      return empty_parse_result<std::string_view>(
          input, fmt::format("Expected {} but saw {}", ch, input.front()));
    }
  });
}

StringParser parse_range(char first, char last) {
  return StringParser([first, last](std::string_view input) {
    if (!input.empty()) {
      char ch = input.front();
      if (ch >= first && ch <= last) {
        return make_parse_result(input.substr(0, 1), input.substr(1));
      }
    }
    return empty_parse_result<std::string_view>(
        input, fmt::format("Error: expected [{}-{}] but saw {}", first, last,
                           input.front()));
  });
}

StringParser parse_str(std::string_view str) {
  return StringParser([str](std::string_view input) {
    if (input.starts_with(str)) {
      return make_parse_result(input.substr(0, str.size()),
                               input.substr(str.size()));
    } else {
      return empty_parse_result<std::string_view>(
          input, fmt::format("Error: expected {} but saw {}", str, input));
    }
  });
}

// Matches a char if it is in the set of chars in src.
StringParser parse_any_of(std::string_view str) {
  return StringParser([str](std::string_view input) {
    if (!detail::str_contains(str, input.front())) {
      return empty_parse_result<std::string_view>(
          input, fmt::format("Error: expected any of {} but saw {}", str,
                             input.front()));
    }
    return make_parse_result(input.substr(0, 1), input.substr(1));
  });
}

Parser<int> parse_digit(int first = 0, int last = 9) {
  auto is_valid_digit = [first, last](int ch) -> int {
    int val = ch - '0';
    return (std::isdigit(ch) && val >= first && val <= last) ? 1 : 0;
  };
  return detail::parse_char_class(is_valid_digit)
      .transform([](std::string_view str) {
        return static_cast<int>(str.front() - '0');
      });
}

// Zero or more.
template <typename T>
Parser<std::vector<T>> parse_some(Parser<T> parser,
                                  std::optional<size_t> max = std::nullopt) {
  return Parser<std::vector<T>>([parser, max](std::string_view input) {
    std::vector<T> results;
    while (!input.empty()) {
      auto result = parser(input);
      if (!result) {
        break;
      }
      if (max && results.size() == *max) {
        return empty_parse_result<std::vector<T>>(
            input, fmt::format("Error: parsed more than {} reults", *max));
      }
      results.push_back(result.value());
      input = result.input;
    }
    return make_parse_result(results, input);
  });
}

template <typename T>
Parser<std::vector<T>> parse_n(Parser<T> parser, size_t min,
                               std::optional<size_t> max = std::nullopt) {
  return parse_some(parser, max).and_then([min](auto const& results) {
    if (results.size() < min) {
      return parse_never<std::vector<T>>();
    }
    return pure(results);
  });
}

StringParser parse_some(StringParser parser,
                        std::optional<size_t> max = std::nullopt) {
  return StringParser([parser, max](std::string_view input) {
    size_t size = 0;
    size_t count = 0;
    std::string_view inp = input;
    while (!inp.empty()) {
      auto result = parser(inp);
      if (!result) {
        break;
      }
      if (max && count == *max) {
        return empty_parse_result<std::string_view>(
            input, fmt::format("Error: parsed more than {} results", *max));
      }
      ++count;
      size += result.value().size();
      inp = result.input;
    }
    return make_parse_result(input.substr(0, size), inp);
  });
}

StringParser parse_n(StringParser parser, size_t min,
                     std::optional<int> max = std::nullopt) {
  // This version can't use the same combinator as above because of the
  // continuous string_view optiization.
  return StringParser([parser, min, max](std::string_view input) {
    int count = 0;
    size_t pos = 0;
    std::string_view inp = input;
    std::string error;
    while (!inp.empty()) {
      auto result = parser(inp);
      if (!result) {
        error = result.error;
        break;
      }

      if (max && count == *max) {
        return empty_parse_result<std::string_view>(
            result.input,
            fmt::format("Error: parsed more than {} results", *max));
      }
      pos += result.value().size();
      inp = result.input;
      ++count;
    }
    if (count < min) {
      return empty_parse_result<std::string_view>(
          inp, fmt::format(
                   "Error: expected {} occurences but only saw {}\n\tInner: {}",
                   min, count, error)

      );
    }
    return make_parse_result(input.substr(0, pos), input.substr(pos));
  });
}

StringParser parse_sequence(std::initializer_list<StringParser> parsers) {
  std::vector<StringParser> ps(parsers);
  return StringParser([ps](std::string_view input) {
    size_t count = 0;
    std::string_view inp = input;
    for (auto parser : ps) {
      if (input.empty()) {
        return empty_parse_result<std::string_view>(
            inp, fmt::format("Error: reached end of input."));
      }
      auto result = parser(inp);
      if (!result) {
        return result;
      }
      count += result.value().size();
      inp = result.input;
    }
    return make_parse_result<std::string_view>(input.substr(0, count),
                                               input.substr(count));
  });
}

template <typename T, typename D, typename S>
Parser<detail::result_vector_t<T>> parse_delimited_by(
    Parser<T> parser, Parser<D> delimiter, Parser<S> terminator,
    std::optional<int> max = std::nullopt) {
  using ResultType = detail::result_vector_t<T>;
  return Parser<ResultType>([=](std::string_view input) {
    auto tokens_result = parse_some(parser.skip(delimiter))(input);

    if (!tokens_result) {
      return empty_parse_result<ResultType>(input, tokens_result.error);
    }
    input = tokens_result.input;

    auto last_token_result = parser(input);

    if (!last_token_result) {
      return empty_parse_result<ResultType>(input, last_token_result.error);
    }
    // now expect the terminator
    auto term_result = terminator(last_token_result.input);
    if (!term_result) {
      return empty_parse_result<ResultType>(term_result.input,
                                            term_result.error);
    }

    auto results = tokens_result.value();
    results.push_back(last_token_result.value());

    return make_parse_result(results, last_token_result.input);
  });
}

StringParser parse_alpha() {
  return detail::parse_char_class(static_cast<int (*)(int)>(&std::isalpha));
}

StringParser parse_alnum() {
  return detail::parse_char_class(static_cast<int (*)(int)>(&std::isalnum));
}

StringParser parse_space() {
  return detail::parse_char_class(static_cast<int (*)(int)>(&std::isspace));
}

Parser<unit> parse_opt_ws() { return parse_some(parse_space()).as(unit{}); }

Parser<unit> parse_ws() { return parse_n(parse_space(), 1).as(unit{}); }

template <typename T, typename U>
Parser<T> parse_ignoring(Parser<T> parser, Parser<U> ignore) {
  return parser.skip(ignore).or_else(ignore.and_then(parser).skip(ignore));
}

template <typename U>
Parser<detail::discontinuous_string_view> parse_ignoring(StringParser parser,
                                                         Parser<U> ignore) {
  return parse_ignoring(detail::to_discontinuous(parser), ignore);
}

Parser<detail::discontinuous_string_view> parse_ignoring_ws(
    StringParser parser) {
  return parse_ignoring(parser, parse_opt_ws());
}

// this is unsafe - the referencing parser must
// out live calls to this parser.
template<typename T>
Parser<T> parse_ref(const Parser<T>& parser) {
    return Parser<T>([&] (std::string_view input) {
        return parser(input);
    });
}

template<typename T>
Parser<T> parse_recursive(std::function<Parser<T>(const Parser<T>&)> make_parser) {
    auto parser_ptr = std::make_shared<Parser<T>>(parse_never<T>());
    *parser_ptr = make_parser(*parser_ptr);

    return Parser<T>([parser_ptr](std::string_view input) {
        // Copies shared_ptr by value which then holds the reference.
        return (*parser_ptr)(input);
    });
}

#endif  // __PARSER_H__