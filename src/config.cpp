#define RYML_SINGLE_HDR_DEFINE_NOW
#include "config.hpp"

#include <fstream>

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

// TODO: lator add hot reload support
bool DockConfig::reloadConfig() {
    auto configPath{configFile()};
    auto& dockConfig{*this};

    if (!fs::exists(configPath)) {
        return false;
    }

    std::ifstream file(configPath);
    if (!file.is_open()) return false;

    std::string contents((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    auto tree{ryml::parse_in_arena(ryml::to_csubstr(contents))};

    auto root{tree.rootref()};

    if (root.is_stream() && root.has_children()) root = root.first_child();
    if (root.is_doc() && root.has_children()) root = root.first_child();

    if (!root.is_map()) return false;

    if (root.has_child("looks")) {
        auto looksNode{root["looks"]};

        if (looksNode.is_map()) {
            for (auto child : looksNode) {
                if (child.key() == "cornerRadius")
                    dockConfig.cornerRadius =
                        nodeFloat(child, DockDefaults::cornerRadius);
                else if (child.key() == "padding")
                    dockConfig.padding =
                        _parseSides(child, DockDefaults::padding);
                else if (child.key() == "margin")
                    dockConfig.margin =
                        _parseSides(child, DockDefaults::margin);
                else if (child.key() == "itemSize")
                    dockConfig.itemSize =
                        nodeFloat(child, DockDefaults::itemSize);
                else if (child.key() == "itemSpacing")
                    dockConfig.itemSpacing =
                        nodeFloat(child, DockDefaults::itemSpacing);
                else if (child.key() == "activeDotSize")
                    dockConfig.activeDotSize =
                        nodeFloat(child, DockDefaults::activeDotSize);
                else if (child.key() == "maxScale")
                    dockConfig.maxScale =
                        nodeFloat(child, DockDefaults::maxScale);
                else if (child.key() == "maxLiftAmount")
                    dockConfig.maxLiftAmount =
                        nodeFloat(child, DockDefaults::maxLiftAmount);
            }
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
                    std::string className;

                    if (itemNode.has_child("class")) {
                        itemNode["class"] >> className;
                    } else {
                        continue;
                    }

                    auto item{makeItem(className, false, true)};

                    for (auto propertyNode : itemNode) {
                        std::string keyStr;
                        propertyNode >> ryml::key(keyStr);

                        if (!nodeHasValue(propertyNode)) continue;

                        std::string valStr;
                        propertyNode >> valStr;

                        if (keyStr == "Icon") {
                            item.app.Icon = valStr;
                        } else if (keyStr == "Exec") {
                            item.app.Exec = valStr;
                        } else if (keyStr == "StartupWMClass") {
                            item.app.StartupWMClass = valStr;
                        }

                        if (item.app.Icon && item.app.Exec &&
                            item.app.StartupWMClass) {
                            break;
                        }
                    }

                    dockConfig.items.emplace_back(std::move(item));
                }
            }
        }
    }

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
