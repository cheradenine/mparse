#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <string>
#include <functional>

#include "convert.h"

namespace cparse {

// Instead of vector of std::string_view it is a type
// that can combine parse results.
// Parser results are trees?

using ParseResult = std::pair<std::vector<std::string_view>, std::string_view>;

// template<typename A, typename B>
// ParseResult<B> and_then(ParseResult<A>, std::function<ParseResult<B>(A)>);

using Parser = std::function<ParseResult(std::string_view)>;

Parser digit();
Parser letter();
Parser whitespace();

Parser literal(char ch);
Parser any_char();
Parser seq(const std::string& str);

Parser some(Parser p);

}  // nemspace cparser

cparse::Parser operator>>(cparse::Parser a, cparse::Parser b);
cparse::Parser operator|(const cparse::Parser& a, const cparse::Parser& b);


#endif  // __PARSER_H__