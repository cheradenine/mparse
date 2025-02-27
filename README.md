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
        .skip(parse_opt_ws())
        .and_then(parse_literal(','))
        .skip(parse_opt_ws())
        .and_then(parse_str("world"));

    auto result = parser("hello, world");

    // Note that the result is the last matched parse.
    // If each token is needed you can use parse_seq,
    // or parse_n, or use and_then or transform.
    // also parse_delimited_by is a powerful way
    // to split up input.
    assert(result.value() == "world");

```

### Handling whitespace

Sometimes you want to ignore whitespace and sometimes you don't. For example, in the
stylesheet example we can ignore blank lines and spaces around delimiters like `'{'`, etc. but
for some property values whitespace becomes an important delimiter like in padding.

To deal with this the library has different ways to express parsing whitespace. Use

`parse_ws()` - When failing to find whitespace would be a parse error

`parse_opt_ws()` - When whitespace is optional.

`parse_ignoring_ws()` - to wrap a parser with optional whitespace on either end of it.

## Status

This is very early experimental code. There minimal error reporting. There may be bugs. There aren't enough tests. There will be breaking changes.

More combinators may be useful, like `parse_until` or `skip_until` for comments. It is not
hard to come up with more and build them out of the
existing parsers.

Input now is assumed to be in a single buffer and accessed via string_view. It is a great abstraction for hadling parsing but input will likely need to be enhanced to handler large input streams.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
