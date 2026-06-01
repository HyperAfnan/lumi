#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION

#include <ranges>

#include "config.hpp"
#include "context.hpp"
#include "dock.hpp"
#include "gfx.hpp"
#include "icon.hpp"
#include "renderer.hpp"
#include "surface.hpp"
#include "toplevel.hpp"

int main() {
    auto& wl{WaylandContext::get()};
    auto& dockConfig{DockConfig::get()};
    dockConfig.reloadConfig();

    LayerSurface ls{};

    while (!ls.eglWindow) {
        wl.dispatch();
    }

    GfxContext gfx(wl, ls);
    Renderer renderer;
    IconRenderer iconRenderer{renderer.vg};

    auto& iconIndex{IconIndex::get()};
    auto& toplevelCtx{ToplevelContext::get()};

    iconIndex.preload(
        dockConfig.items |
        std::views::transform([](const DockItem& item) { return item.app; }) |
        std::ranges::to<std::vector>());

    toplevelCtx.onAppOpen = [](std::string_view appId) {
        for (auto& item : DockConfig::get().items) {
            if (item.app.matchesAppId(appId)) {
                item.active = true;

                item.dotSpring = makeDotSpring();
                item.dotSpring.setTarget(0.f);

                break;
            }
        }
    };

    toplevelCtx.onAppClose = [](std::string_view appId) {
        for (auto& item : DockConfig::get().items) {
            if (item.app.matchesAppId(appId)) {
                item.active = false;
                break;
            }
        }
    };

    while (!ls.closed && wl.dispatch() != -1) {
        float visualDockHeight{dockConfig.padding.vertical() +
                               dockConfig.itemMargin.vertical() +
                               dockConfig.itemSize};
        int regionHeight{static_cast<int>(visualDockHeight)};
        int regionY{ls.height - regionHeight};

        if (regionY < 0) regionY = 0;
        if (regionHeight < 0) regionHeight = 0;

        ls.setInputRegion(0, regionY, ls.width, regionHeight);

        renderer.clearViewport(ls.width, ls.height);
        renderer.beginFrame(ls.width, ls.height);

        handleDock(renderer.vg, iconRenderer, ls.width, ls.height);

        renderer.endFrame();

        gfx.swapBuffers();
        wl_surface_commit(ls.surface);
    }

    return 0;
}
