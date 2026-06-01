#ifndef UTILS_HPP
#define UTILS_HPP

#include <optional>
#include <string>
#include <string_view>

std::optional<std::string_view> getEnv(std::string_view name);

bool startsWith(std::string_view str, std::string_view prefix);

std::string toLower(std::string_view str);

bool icontains(std::string_view a, std::string_view b);

#endif  // UTILS_HPP