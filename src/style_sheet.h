#ifndef __STYLE_SHEET_H__
#define __STYLE_SHEET_H__

#include <string>
#include <string_view>
#include <iostream>
#include <unordered_map>

struct Dimension
{
    int value;
    enum
    {
        pct,
        px,
    } units;
};

std::ostream& operator <<(std::ostream& out, const Dimension& dim) {
    out << dim.value << (dim.units == Dimension::px ? "px" : "pct");
    return out;
}
struct Color {
    int r;
    int g;
    int b;
};

std::ostream& operator <<(std::ostream& out, const Color& c) {
    out << "rgb(" << c.r << "," << c.g << "," << c.b << ")";
    return out;
}

struct Spacing {
    Dimension top;
    Dimension right;
    Dimension bottom;
    Dimension left;
};

std::ostream& operator <<(std::ostream& out, const Spacing& s) {
    out << s.top << "," << s.right << "," << s.bottom << "," << s.left;
    return out;
}
struct Rule {
    std::string property;
    std::variant<int, std::string, Dimension, Color, Spacing> value;
};
struct StyleSheet {
    std::unordered_map<std::string, std::vector<Rule>> selectors;
};

#endif  // __STYLE_SHEET_H__