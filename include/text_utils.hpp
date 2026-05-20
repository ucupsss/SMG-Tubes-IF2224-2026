#ifndef TEXT_UTILS_HPP
#define TEXT_UTILS_HPP

#include <algorithm>
#include <cctype>
#include <string>

namespace text_util {

inline std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

} // namespace text_util

#endif
