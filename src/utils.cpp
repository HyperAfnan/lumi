#include "utils.hpp"

#include <algorithm>
#include <cstdlib>

std::optional<std::string_view> getEnv(std::string_view name) {
    auto value{std::getenv(name.data())};
    if (!value) return std::nullopt;

    return std::string_view{value};
};

bool startsWith(std::string_view str, std::string_view prefix) {
    return str.size() >= prefix.size() &&
           str.substr(0, prefix.size()) == prefix;
};

bool icontains(std::string_view a, std::string_view b) {
    auto lowerA{toLower(a)};
    auto lowerB{toLower(b)};

    return lowerA.contains(lowerB);
};

std::string toLower(std::string_view str) {
    std::string result(str);
    std::ranges::transform(result, result.begin(), ::tolower);
    return result;
};