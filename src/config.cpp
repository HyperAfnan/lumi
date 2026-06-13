#define RYML_SINGLE_HDR_DEFINE_NOW
#include "config.hpp"

#include <fstream>
#include <optional>

#include "logger.hpp"
#include "ryml.hpp"
#include "utils.hpp"

DockItem makeItem(const std::string& className, bool active, bool virtualApp) {
    DockItem item{
        .app = {className, virtualApp},
        .label = className,
        .active = active,
    };

    if (active) item.dotSpring.setTarget(0.f);

    return item;
}

static bool nodeHasValue(ryml::ConstNodeRef node) {
    if (node.invalid() || node.is_seed()) return false;
    if (!node.has_val()) return false;

    auto value{node.val()};
    return value.len > 0;
}

float nodeFloat(ryml::ConstNodeRef node, float fallback) {
    if (!nodeHasValue(node)) return fallback;

    float v{fallback};
    node >> v;

    return v;
}

float childFloat(ryml::ConstNodeRef parent, const char* key, float fallback) {
    if (parent.invalid() || !parent.has_child(ryml::to_csubstr(key)))
        return fallback;

    auto child{parent[ryml::to_csubstr(key)]};
    return nodeFloat(child, fallback);
}

inline SidesConfig _parseSides(ryml::ConstNodeRef node,
                               const SidesConfig& def) {
    if (node.invalid() || node.is_seed()) return def;
    if (!node.has_children()) return def;

    return SidesConfig{
        childFloat(node, "left", def.left),
        childFloat(node, "right", def.right),
        childFloat(node, "top", def.top),
        childFloat(node, "bottom", def.bottom),
    };
}

inline ColorConfig _parseColor(ryml::ConstNodeRef node,
                               const ColorConfig& def) {
    if (node.invalid() || node.is_seed()) return def;

    if (!node.is_seq() || node.num_children() != 4) {
        logger::warning(
            "backgroundColor must be an array of 4 values: [R, G, B, A]");
        return def;
    }

    float r{nodeFloat(node[0], def.r * 255.f)};
    float g{nodeFloat(node[1], def.g * 255.f)};
    float b{nodeFloat(node[2], def.b * 255.f)};
    float a{nodeFloat(node[3], def.a)};

    return ColorConfig{r / 255.f, g / 255.f, b / 255.f, a};
}

inline ContextMenuConfig _parseContextMenuConfig(ryml::ConstNodeRef node,
                                                 const ContextMenuConfig& def) {
    if (node.invalid() || node.is_seed()) return def;

    ContextMenuConfig config{def};

    if (node.has_child("backgroundColor"))
        config.backgroundColor =
            _parseColor(node["backgroundColor"], def.backgroundColor);
    if (node.has_child("hoverColor"))
        config.hoverColor = _parseColor(node["hoverColor"], def.hoverColor);

    return config;
}

bool DockConfig::reloadConfig() {
    auto configPath{configFile()};
    auto& dockConfig{*this};

    if (!fs::exists(configPath)) {
        logger::warning("config not found: " + configPath.string());
        return false;
    }

    std::ifstream file(configPath);
    if (!file.is_open()) {
        logger::warning("failed to open config: " + configPath.string());
        return false;
    }

    std::string contents((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    auto tree{ryml::parse_in_arena(ryml::to_csubstr(contents))};

    auto root{tree.rootref()};

    if (root.is_stream() && root.has_children()) root = root.first_child();
    if (root.is_doc() && root.has_children()) root = root.first_child();

    if (!root.is_map()) {
        logger::warning("config root is not a map: " + configPath.string());
        return false;
    }

    auto assignFloat = [](ryml::ConstNodeRef parent, const char* key,
                          float& target, float fallback) {
        if (parent.has_child(ryml::to_csubstr(key))) {
            target = nodeFloat(parent[ryml::to_csubstr(key)], fallback);
        }
    };

    auto assignString = [](ryml::ConstNodeRef parent, const char* key,
                           std::optional<std::string>& target) {
        if (parent.has_child(ryml::to_csubstr(key)) &&
            parent[ryml::to_csubstr(key)].has_val()) {
            std::string valStr;
            parent[ryml::to_csubstr(key)] >> valStr;
            target = valStr;
        }
    };

    if (root.has_child("looks")) {
        auto looksNode{root["looks"]};

        if (looksNode.is_map()) {
            assignFloat(looksNode, "cornerRadius", dockConfig.cornerRadius,
                        DockDefaults::cornerRadius);
            assignFloat(looksNode, "itemSize", dockConfig.itemSize,
                        DockDefaults::itemSize);
            assignFloat(looksNode, "itemSpacing", dockConfig.itemSpacing,
                        DockDefaults::itemSpacing);
            assignFloat(looksNode, "activeDotSize", dockConfig.activeDotSize,
                        DockDefaults::activeDotSize);
            assignFloat(looksNode, "maxScale", dockConfig.maxScale,
                        DockDefaults::maxScale);
            assignFloat(looksNode, "maxLiftAmount", dockConfig.maxLiftAmount,
                        DockDefaults::maxLiftAmount);

            if (looksNode.has_child("backgroundColor")) {
                dockConfig.backgroundColor =
                    _parseColor(looksNode["backgroundColor"],
                                DockDefaults::backgroundColor);
            }

            if (looksNode.has_child("contextMenu")) {
                dockConfig.contextMenu = _parseContextMenuConfig(
                    looksNode["contextMenu"], dockConfig.contextMenu);
            }

            if (looksNode.has_child("font")) {
                auto fontNode{looksNode["font"]};
                if (fontNode.is_map()) {
                    if (fontNode.has_child("name") &&
                        fontNode["name"].has_val()) {
                        fontNode["name"] >> dockConfig.font.name;
                    }
                    assignFloat(fontNode, "size", dockConfig.font.size,
                                dockConfig.font.size);
                    if (fontNode.has_child("color")) {
                        dockConfig.font.color = _parseColor(
                            fontNode["color"], dockConfig.font.color);
                    }

                    logger::info(
                        "font parsed: name=" + dockConfig.font.name +
                        " size=" + std::to_string(dockConfig.font.size) +
                        " color=(" + std::to_string(dockConfig.font.color.r) +
                        "," + std::to_string(dockConfig.font.color.g) + "," +
                        std::to_string(dockConfig.font.color.b) + "," +
                        std::to_string(dockConfig.font.color.a) + ")");
                } else {
                    logger::warning("font node is not a map");
                }
            } else {
                logger::warning("no font node found under looks");
            }

            if (looksNode.has_child("padding"))
                dockConfig.padding =
                    _parseSides(looksNode["padding"], DockDefaults::padding);
            if (looksNode.has_child("margin"))
                dockConfig.margin =
                    _parseSides(looksNode["margin"], DockDefaults::margin);
        }
    }

    if (root.has_child("items")) {
        auto itemsNode{root["items"]};

        if (itemsNode.is_seq()) {
            dockConfig.items.clear();

            for (auto itemNode : itemsNode) {
                if (itemNode.is_val()) {
                    std::string className;
                    itemNode >> className;
                    dockConfig.items.emplace_back(makeItem(className, false));
                } else if (itemNode.is_map()) {
                    if (!itemNode.has_child("class") ||
                        !itemNode["class"].has_val()) {
                        continue;
                    }

                    std::string className;
                    itemNode["class"] >> className;
                    auto item{makeItem(className, false, true)};

                    assignString(itemNode, "Icon", item.app.Icon);
                    assignString(itemNode, "Exec", item.app.Exec);
                    assignString(itemNode, "StartupWMClass",
                                 item.app.StartupWMClass);

                    dockConfig.items.emplace_back(std::move(item));
                }
            }
        }
    }

    logger::info("config loaded: " + configPath.string());
    return true;
}

fs::path DockConfig::configFile() {
    std::vector<fs::path> dirs;

    auto home{getEnv("HOME")};
    auto xdgConfig{getEnv("XDG_CONFIG_HOME")};

    if (xdgConfig)
        dirs.emplace_back(fs::path(*xdgConfig) / "lumi");
    else if (home)
        dirs.emplace_back(fs::path(*home) / ".config/lumi");

    std::array<std::string, 2> extensions{"yaml", "yml"};
    std::array<std::string, 3> names{"dock", "lumi", "config"};

    for (const auto& dir : dirs) {
        for (const auto& name : names) {
            for (const auto& extension : extensions) {
                fs::path configPath{dir / (name + "." + extension)};
                if (fs::exists(configPath)) {
                    return configPath;
                }
            }
        }
    }

    return dirs.front() / "dock.yaml";
}
