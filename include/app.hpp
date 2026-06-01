#ifndef APP_HPP
#define APP_HPP

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

class App {
   public:
    std::string className;

    App(std::string className, bool isVirtual = false);
    bool matchesAppId(std::string_view appId) const;
    void launch() const;

    // desktop entries
    std::optional<std::string> Icon;
    std::optional<std::string> Exec;
    std::optional<std::string> StartupWMClass;

   private:
    std::optional<fs::path> desktopFile;

    void normalize();
    std::optional<fs::path> findDesktopFile() const;
    std::optional<fs::path> fuzzySearch() const;

    void parseDesktopFile(const fs::path& path);
};

#endif  // APP_HPP