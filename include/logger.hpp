#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <unistd.h>

#include <cstdio>
#include <string_view>

namespace logger {

namespace detail {

constexpr std::string_view RESET{"\033[0m"};
constexpr std::string_view BOLD{"\033[1m"};
constexpr std::string_view CYAN{"\033[1;96m"};
constexpr std::string_view YELLOW{"\033[1;93m"};
constexpr std::string_view RED{"\033[1;91m"};
constexpr std::string_view MAGENTA{"\033[1;95m"};

inline bool supportsColor(int fd) noexcept { return isatty(fd) != 0; }

inline void writeColored(FILE* stream, int fd, std::string_view color,
                         std::string_view label,
                         std::string_view msg) noexcept {
    if (supportsColor(fd)) {
        std::fwrite(color.data(), 1, color.size(), stream);
        std::fwrite(label.data(), 1, label.size(), stream);
        std::fwrite(RESET.data(), 1, RESET.size(), stream);
    } else {
        std::fwrite(label.data(), 1, label.size(), stream);
    }

    std::fwrite(msg.data(), 1, msg.size(), stream);
    std::fputc('\n', stream);
}

}  // namespace detail

inline void info(std::string_view msg) noexcept {
    detail::writeColored(stdout, STDOUT_FILENO, detail::CYAN, "[INFO] ", msg);
}

inline void warning(std::string_view msg) noexcept {
    detail::writeColored(stderr, STDERR_FILENO, detail::YELLOW, "[WARNING] ",
                         msg);
}

inline void error(std::string_view msg) noexcept {
    detail::writeColored(stderr, STDERR_FILENO, detail::RED, "[ERROR] ", msg);
}

inline void debug(std::string_view msg) noexcept {
    detail::writeColored(stdout, STDOUT_FILENO, detail::MAGENTA, "[DEBUG] ",
                         msg);
}

}  // namespace logger

#endif  // LOGGER_HPP