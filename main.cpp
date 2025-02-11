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

// A parser for things
// is a function from strings
// to lists of pairs of strings and things.
// using Parser = std::function

void dump_result(const std::vector<std::string_view>& res) {
    std::cout << "[";
    for (auto it = res.begin(); it != res.end(); ++it) {
        std::cout << *it;
        if ((it + 1) != res.end()) {
            std::cout << ",";
        }
    }
    std::cout << "]";
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "enter a string" << std::endl;
        return -1;
    }

    std::string_view input = argv[1];
    
    auto digit = cparse::digit();
    auto letter = cparse::letter();
    auto ws = cparse::whitespace();

    {
        auto parser = cparse::some(digit) >> ws >> cparse::some(letter) >> cparse::literal(';');
//        auto parser = (cparse::seq("hello") | cparse::seq("goodbye")) >> ws >> cparse::literal(';');
        auto [result, remaining] = parser(input);
        std::cout << "[";
        dump_result(result);
        std::cout << "," << remaining << "]" << std::endl;
    }

}
