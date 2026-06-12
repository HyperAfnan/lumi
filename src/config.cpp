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

    auto assignFloat = [](ryml::ConstNodeRef parent, const char* key, float& target, float fallback) {
        if (parent.has_child(ryml::to_csubstr(key))) {
            target = nodeFloat(parent[ryml::to_csubstr(key)], fallback);
        }
    };

    auto assignString = [](ryml::ConstNodeRef parent, const char* key, std::optional<std::string>& target) {
        if (parent.has_child(ryml::to_csubstr(key)) && parent[ryml::to_csubstr(key)].has_val()) {
            std::string valStr;
            parent[ryml::to_csubstr(key)] >> valStr;
            target = valStr;
        }
    };

    if (root.has_child("looks")) {
        auto looksNode{root["looks"]};

        if (looksNode.is_map()) {
            assignFloat(looksNode, "cornerRadius", dockConfig.cornerRadius, DockDefaults::cornerRadius);
            assignFloat(looksNode, "itemSize", dockConfig.itemSize, DockDefaults::itemSize);
            assignFloat(looksNode, "itemSpacing", dockConfig.itemSpacing, DockDefaults::itemSpacing);
            assignFloat(looksNode, "activeDotSize", dockConfig.activeDotSize, DockDefaults::activeDotSize);
            assignFloat(looksNode, "maxScale", dockConfig.maxScale, DockDefaults::maxScale);
            assignFloat(looksNode, "maxLiftAmount", dockConfig.maxLiftAmount, DockDefaults::maxLiftAmount);

            if (looksNode.has_child("padding")) dockConfig.padding = _parseSides(looksNode["padding"], DockDefaults::padding);
            if (looksNode.has_child("margin")) dockConfig.margin = _parseSides(looksNode["margin"], DockDefaults::margin);
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
                    if (!itemNode.has_child("class") || !itemNode["class"].has_val()) {
                        continue;
                    }

                    std::string className;
                    itemNode["class"] >> className;
                    auto item{makeItem(className, false, true)};

                    assignString(itemNode, "Icon", item.app.Icon);
                    assignString(itemNode, "Exec", item.app.Exec);
                    assignString(itemNode, "StartupWMClass", item.app.StartupWMClass);

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
