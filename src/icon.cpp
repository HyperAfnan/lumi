#include "icon.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ranges>

#include "ini.hpp"
#include "logger.hpp"
#include "utils.hpp"

IconRenderer::IconRenderer(NVGcontext* vg)
    : vg(vg), rast(nsvgCreateRasterizer()), cache(64, [this](int img) {
          if (img != -1) nvgDeleteImage(this->vg, img);
      }) {}

IconRenderer::~IconRenderer() { nsvgDeleteRasterizer(rast); }

int IconRenderer::rasterizeSVG(const fs::path& path, int size) {
    NSVGimage* svg{nsvgParseFromFile(path.c_str(), "px", 96.f)};
    if (!svg) {
        logger::error("failed to parse svg: " + path.string());
        return -1;
    }

    int renderSize{size * 2};
    float maxDim{std::max(svg->width, svg->height)};
    float scale{maxDim > 0.f ? renderSize / maxDim : 1.f};

    std::vector<std::uint8_t> pixels(renderSize * renderSize * 4, 0);
    nsvgRasterize(rast, svg, 0, 0, scale, pixels.data(), renderSize, renderSize,
                  renderSize * 4);

    nsvgDelete(svg);

    int img{nvgCreateImageRGBA(vg, renderSize, renderSize,
                               NVG_IMAGE_PREMULTIPLIED, pixels.data())};
    return img;
}

int IconRenderer::loadPNG(const fs::path& path) {
    return nvgCreateImage(vg, path.c_str(), 0);
}

int IconRenderer::load(const fs::path& path) {
    auto key{path.string()};

    if (auto cached{cache.get(key)}; cached) return *cached;

    int img{-1};
    if (path.extension() == ".svg") {
        img = rasterizeSVG(path, 128);
    } else if (path.extension() == ".png" || path.extension() == ".jpg") {
        img = loadPNG(path);
    } else {
        logger::warning("unsupported icon format: " + path.string());
    }

    cache.set(key, img);

    return img;
}

void IconRenderer::draw(const fs::path& path, float cx, float cy, float size,
                        float cornerRadius) {
    int img{load(path)};
    if (img == -1) return;

    float x{cx - size * 0.5f};
    float y{cy - size * 0.5f};
    float r{std::min(cornerRadius, size * 0.5f)};

    NVGpaint paint{nvgImagePattern(vg, x, y, size, size, 0.f, img, 1.f)};

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, size, size, r);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
}

// in memory icon index
std::optional<std::string> parseGtkSettings(const fs::path& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    std::string line;

    constexpr std::string_view key{"gtk-icon-theme-name"};
    while (std::getline(file, line)) {
        auto pos{line.find(key)};
        if (pos == std::string::npos) continue;

        pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string theme{line.substr(pos + 1)};

        theme.erase(theme.begin(), std::find_if(theme.begin(), theme.end(),
                                                [](unsigned char c) {
                                                    return !std::isspace(c);
                                                }));

        theme.erase(
            std::find_if(theme.rbegin(), theme.rend(),
                         [](unsigned char c) { return !std::isspace(c); })
                .base(),
            theme.end());

        if (!theme.empty()) {
            return theme;
        }
    }

    return std::nullopt;
}

std::optional<std::string> detectCurrentTheme() {
    const auto home{getEnv("HOME")};
    if (!home) {
        return std::nullopt;
    }

    if (auto theme{execCommand(
            "gsettings get org.gnome.desktop.interface icon-theme")};
        theme.has_value()) {
        std::string themeStr{*theme};

        if (themeStr.size() >= 2 && themeStr.front() == '\'' &&
            themeStr.back() == '\'') {
            themeStr = themeStr.substr(1, themeStr.size() - 2);
        }

        if (!themeStr.empty()) {
            return themeStr;
        }
    }

    if (auto theme{
            parseGtkSettings(fs::path(*home) / ".config/gtk-3.0/settings.ini")})
        return theme;
    if (auto theme{
            parseGtkSettings(fs::path(*home) / ".config/gtk-4.0/settings.ini")})
        return theme;
    if (auto theme{parseGtkSettings("/etc/gtk-3.0/settings.ini")}) return theme;
    if (auto theme{parseGtkSettings("/etc/gtk-4.0/settings.ini")}) return theme;

    return std::nullopt;
}

std::optional<fs::path> findThemeDirectory(const std::vector<fs::path>& roots,
                                           std::string_view theme) {
    for (auto& root : roots) {
        auto path{root / theme};

        if (fs::exists(path)) return path;
    }

    return std::nullopt;
}

void parseDirectory(const fs::path& themeDir, const std::string& subdir,
                    mINI::INIStructure& ini, Theme& theme) {
    auto fullDir{themeDir / subdir};
    if (!fs::exists(fullDir) || !fs::is_directory(fullDir)) {
        logger::warning("Directory " + fullDir.string() +
                        " does not exist or is not a directory, skipping");
        return;
    }

    IconEntry templateEntry;

    auto sectionIt{ini[subdir]};

    if (auto size{sectionIt["Size"]}; !size.empty()) {
        int value{0};
        auto [ptr, ec]{
            std::from_chars(size.data(), size.data() + size.size(), value)};
        if (ec == std::errc()) {
            templateEntry.size = value;
        } else {
            logger::warning("Invalid size value for " + subdir + " in theme " +
                            theme.name + ": " + size);
        }
    }

    if (auto scale{sectionIt["Scale"]}; !scale.empty()) {
        int value{0};
        auto [ptr, ec]{
            std::from_chars(scale.data(), scale.data() + scale.size(), value)};
        if (ec == std::errc()) {
            templateEntry.scale = value;
        } else {
            logger::warning("Invalid scale value for " + subdir + " in theme " +
                            theme.name + ": " + scale);
        }
    }

    if (auto minSize{sectionIt["MinSize"]}; !minSize.empty()) {
        int value{0};
        auto [ptr, ec]{std::from_chars(minSize.data(),
                                       minSize.data() + minSize.size(), value)};
        if (ec == std::errc()) {
            templateEntry.minSize = value;
        } else {
            logger::warning("Invalid min size value for " + subdir +
                            " in theme " + theme.name + ": " + minSize);
        }
    }

    if (auto maxSize{sectionIt["MaxSize"]}; !maxSize.empty()) {
        int value{0};
        auto [ptr, ec]{std::from_chars(maxSize.data(),
                                       maxSize.data() + maxSize.size(), value)};
        if (ec == std::errc()) {
            templateEntry.maxSize = value;
        } else {
            logger::warning("Invalid max size value for " + subdir +
                            " in theme " + theme.name + ": " + maxSize);
        }
    }

    std::uint32_t dirIndex{static_cast<std::uint32_t>(theme.dirPool.size())};
    theme.dirPool.push_back(fullDir);
    bool usedPool{false};

    for (const auto& file : fs::directory_iterator(fullDir)) {
        if (!file.is_regular_file()) continue;

        auto ext{file.path().extension()};
        if (ext != ".png" && ext != ".svg") continue;

        auto iconName{file.path().stem().string()};
        auto entry{templateEntry};

        entry.dirIndex = dirIndex;
        entry.isSvg = (ext == ".svg");

        theme.icons[iconName].push_back(std::move(entry));
        usedPool = true;
    }

    if (!usedPool) {
        theme.dirPool.pop_back();
    }
};

void parseIndexTheme(const fs::path& dir, Theme& theme) {
    auto indexPath{dir / "index.theme"};
    if (!fs::exists(indexPath)) return;

    mINI::INIFile file(indexPath.string());
    mINI::INIStructure ini;

    file.read(ini);

    auto inherits{ini["Icon Theme"]["inherits"]};
    if (!inherits.empty()) {
        auto splitted{inherits | std::views::split(',') |
                      std::ranges::to<std::set<std::string>>()};
        if (!splitted.empty()) {
            theme.inherits = std::move(splitted);
        }
    }

    std::vector<std::string> dirs;

    auto directories{ini["Icon Theme"]["Directories"]};
    auto scaledDirs{ini["Icon Theme"]["ScaledDirectories"]};

    if (!directories.empty()) {
        auto splitted{directories | std::views::split(',') |
                      std::ranges::to<std::vector<std::string>>()};
        dirs.insert(dirs.end(), splitted.begin(), splitted.end());
    }

    if (!scaledDirs.empty()) {
        auto splitted{scaledDirs | std::views::split(',') |
                      std::ranges::to<std::vector<std::string>>()};
        dirs.insert(dirs.end(), splitted.begin(), splitted.end());
    }

    for (auto& subdir : dirs) {
        parseDirectory(dir, subdir, ini, theme);
    }
}

bool directoryMatches(const IconEntry& e, int size, int scale) {
    if (e.scale != scale) return false;

    switch (e.type) {
        case DirectoryType::Fixed:
            return e.size == size;

        case DirectoryType::Scalable:
            return size >= e.minSize && size <= e.maxSize;

        case DirectoryType::Threshold:
            return size >= e.size - e.threshold && size <= e.size + e.threshold;
    }

    return false;
}

int directoryDistance(const IconEntry& entry, int iconSize, int iconScale) {
    const int wanted{iconSize * iconScale};

    switch (entry.type) {
        case DirectoryType::Fixed: {
            const int actual{entry.size * entry.scale};

            return std::abs(actual - wanted);
        }

        case DirectoryType::Scalable: {
            const int min{entry.minSize * entry.scale};
            const int max{entry.maxSize * entry.scale};

            if (wanted < min) return min - wanted;

            if (wanted > max) return wanted - max;

            return 0;
        }

        case DirectoryType::Threshold: {
            const int min{(entry.size - entry.threshold) * entry.scale};
            const int max{(entry.size + entry.threshold) * entry.scale};

            if (wanted < min) return min - wanted;

            if (wanted > max) return wanted - max;

            return 0;
        }
    }

    return std::numeric_limits<int>::max();
}

constexpr int DEFAULT_ICON_SIZE{48};
constexpr int DEFAULT_ICON_SCALE{1};

std::string getCacheKey(std::string_view iconName, int size, int scale) {
    return std::format("{}:{}:{}", iconName, size, scale);
};

MemIconIndex& MemIconIndex::get() {
    static MemIconIndex instance;
    return instance;
}

MemIconIndex::MemIconIndex() {
    if (const auto home{getEnv("HOME")}; home) {
        roots.emplace_back(fs::path(*home) / ".local/share/icons");
        roots.emplace_back(fs::path(*home) / ".icons");
    }

    roots.emplace_back("/usr/share/icons");
    roots.emplace_back("/usr/local/share/icons");
    roots.emplace_back("/usr/share/pixmaps");
}

void MemIconIndex::build() {
    currentTheme = detectCurrentTheme();
    if (currentTheme) loadThemeRecursive(*currentTheme);

    loadThemeRecursive("hicolor");
};

void MemIconIndex::loadThemeRecursive(std::string_view themeName) {
    if (themes.contains(themeName)) return;

    loadTheme(themeName);

    auto it{themes.find(themeName)};
    if (it == themes.end()) return;

    for (auto& parent : it->second.inherits) loadThemeRecursive(parent);
}

void MemIconIndex::loadTheme(std::string_view themeName) {
    auto themeDir{findThemeDirectory(roots, themeName)};

    if (!themeDir) return;

    Theme theme;
    theme.name = themeName;

    parseIndexTheme(*themeDir, theme);

    themes.emplace(themeName, std::move(theme));
}

void MemIconIndex::preload(const std::vector<App>& apps) {
    for (const auto& app : apps) {
        find(app);
    }
};

std::optional<fs::path> MemIconIndex::find(std::string_view icon, int size,
                                           int scale) const {
    auto key{getCacheKey(icon, size, scale)};

    if (auto it{lookupCache.find(key)}; it != lookupCache.end()) {
        return it->second;
    }

    if (currentTheme) {
        auto result{lookupTheme(*currentTheme, icon, size, scale)};

        if (result) {
            lookupCache[key] = *result;

            return result;
        }
    }

    auto result{lookupTheme("hicolor", icon, size, scale)};

    if (result) {
        lookupCache[key] = *result;

        return result;
    }

    auto fallback{lookupFallback(icon)};
    if (fallback) {
        lookupCache[key] = *fallback;

        return fallback;
    }

    return std::nullopt;
}

std::optional<fs::path> MemIconIndex::find(const App& app) const {
    if (!app.Icon) return std::nullopt;

    if (fs::exists(*app.Icon)) return *app.Icon;

    return find(*app.Icon, DEFAULT_ICON_SIZE, DEFAULT_ICON_SCALE);
}

std::optional<fs::path> MemIconIndex::lookupTheme(std::string_view themeName,
                                                  std::string_view iconName,
                                                  int size, int scale) const {
    auto themeIt{themes.find(std::string(themeName))};

    if (themeIt == themes.end()) return std::nullopt;

    auto iconIt{themeIt->second.icons.find(std::string(iconName))};

    if (iconIt != themeIt->second.icons.end()) {
        const IconEntry* best{nullptr};
        int bestDistance{INT_MAX};

        for (auto& entry : iconIt->second) {
            if (directoryMatches(entry, size, scale)) {
                return themeIt->second.dirPool[entry.dirIndex] /
                       (std::string(iconName) +
                        (entry.isSvg ? ".svg" : ".png"));
            }

            int distance{directoryDistance(entry, size, scale)};

            if (distance < bestDistance) {
                bestDistance = distance;
                best = &entry;
            }
        }

        if (best) {
            return themeIt->second.dirPool[best->dirIndex] /
                   (std::string(iconName) + (best->isSvg ? ".svg" : ".png"));
        }
    }

    for (auto& parent : themeIt->second.inherits) {
        auto result{lookupTheme(parent, iconName, size, scale)};

        if (result) return result;
    }

    return std::nullopt;
}

std::optional<fs::path> MemIconIndex::lookupFallback(
    std::string_view iconName) const {
    auto clonedRoots{roots};
    std::stable_partition(
        clonedRoots.begin(), clonedRoots.end(),
        [](const fs::path& path) { return path.filename() == "pixmaps"; });

    for (const auto& root : clonedRoots) {
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;

            if (entry.path().stem() == iconName) {
                auto ext{entry.path().extension()};

                if (ext == ".png" || ext == ".svg") {
                    return entry.path();
                }
            }
        }
    }

    return std::nullopt;
}

void MemIconIndex::clear() {
    themes.clear();
    lookupCache.clear();
}

// persistent local icon index

static fs::path cacheDir() {
    auto home{getEnv("HOME")};
    return home ? fs::path(*home) / ".cache/lumi" : fs::path("/tmp/lumi-cache");
}

static constexpr std::uint32_t CACHE_MAGIC{0x4C554D49};
static constexpr std::uint32_t CACHE_VERSION{1};

IconIndex& IconIndex::get() {
    static IconIndex instance;
    return instance;
}

IconIndex::IconIndex() : cachePath{cacheDir() / "icon-cache"} {
    fs::create_directories(cacheDir());
    load();
}

bool IconIndex::load() {
    std::ifstream file{cachePath, std::ios::binary};
    if (!file) return false;

    std::uint32_t magic, version, count;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != CACHE_MAGIC) return false;

    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != CACHE_VERSION) return false;

    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (std::uint32_t i{0}; i < count; i++) {
        std::uint32_t keyLen, valLen;
        file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        std::string key(keyLen, '\0');
        file.read(key.data(), keyLen);

        file.read(reinterpret_cast<char*>(&valLen), sizeof(valLen));
        std::string val(valLen, '\0');
        file.read(val.data(), valLen);

        cache.emplace(std::move(key), fs::path{std::move(val)});
    }

    logger::info("loaded " + std::to_string(count) + " icon cache entries");
    return true;
}

bool IconIndex::save() const {
    std::ofstream file{cachePath, std::ios::binary | std::ios::trunc};
    if (!file) {
        logger::warning("failed to write icon cache");
        return false;
    }

    std::uint32_t magic{CACHE_MAGIC};
    std::uint32_t version{CACHE_VERSION};
    std::uint32_t count{static_cast<std::uint32_t>(cache.size())};

    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [key, val] : cache) {
        std::uint32_t keyLen{static_cast<std::uint32_t>(key.size())};
        auto valStr{val.string()};
        std::uint32_t valLen{static_cast<std::uint32_t>(valStr.size())};

        file.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
        file.write(key.data(), keyLen);
        file.write(reinterpret_cast<const char*>(&valLen), sizeof(valLen));
        file.write(valStr.data(), valLen);
    }

    logger::info("saved " + std::to_string(count) + " icon cache entries");
    return true;
}

std::optional<fs::path> IconIndex::resolve(std::string_view iconName) {
    if (auto it{cache.find(iconName)}; it != cache.end()) return it->second;

    if (!built) {
        memIndex.build();
        built = true;
    }

    auto resolved{
        memIndex.find(iconName, DEFAULT_ICON_SIZE, DEFAULT_ICON_SCALE)};
    if (resolved) {
        cache.emplace(iconName, *resolved);
    }

    // not sure if this is the best place to clear the memIndex, but it should
    // be fine
    if (built) {
        memIndex.clear();
        built = false;
    }

    return resolved;
}

std::optional<fs::path> IconIndex::find(const App& app) {
    if (!app.Icon) return std::nullopt;
    if (fs::exists(*app.Icon)) return *app.Icon;

    return resolve(*app.Icon);
}

void IconIndex::preload(const std::vector<App>& apps) {
    bool hadMissing{false};

    for (const auto& app : apps) {
        if (!app.Icon) continue;
        if (fs::exists(*app.Icon)) continue;
        if (cache.contains(*app.Icon)) continue;

        if (!hadMissing) {
            if (!built) {
                memIndex.build();
                built = true;
            }
            hadMissing = true;
        }

        auto resolved{
            memIndex.find(*app.Icon, DEFAULT_ICON_SIZE, DEFAULT_ICON_SCALE)};
        if (resolved) {
            cache.emplace(*app.Icon, *resolved);
        }
    }

    if (hadMissing) {
        save();
    }

    if (built) {
        memIndex.clear();
        built = false;
    }
}