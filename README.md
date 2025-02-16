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
        .skip(whitespace())
        .and_then(parse_literal(','))
        .skip(whitespace())
        .and_then(parse_str("world"));

    auto result = parser("hello, world");

    // Note that the result is the last matched parse.
    // If each token is needed you can use parse_seq,
    // or parse_n, or use and_then or transform.
    // also parse_delimited_by is a powerful way
    // to split up input.
    assert(result.value() == "world");

```

## Status
This is very early experimental code. There no error reporting. There may be bugs. There aren't enough tests.

More combinators may be useful, like `parse_until` or `skip_until` for comments. It is not
hard to come up with more and build them out of the
existing parsers.

Input now is assumed to be in a single buffer and accessed via string_piece. It is a great abstraction for hadling parsing but I will have to extend this to
handle buffering of larger input streams.
