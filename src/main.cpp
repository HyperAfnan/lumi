#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION

#include <chrono>
#include <ranges>

#include "config.hpp"
#include "context.hpp"
#include "dock.hpp"
#include "gfx.hpp"
#include "icon.hpp"
#include "logger.hpp"
#include "popup.hpp"
#include "renderer.hpp"
#include "surface.hpp"
#include "toplevel.hpp"

int main() {
    auto& wl{WaylandContext::get()};
    auto& dockConfig{DockConfig::get()};
    if (!dockConfig.reloadConfig()) {
        logger::warning("using default config");
    }

    LayerSurface ls{};

    while (!ls.eglWindow) {
        wl.dispatch();
    }

    logger::info("renderer ready");

    GfxContext::init(wl, ls);
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
                if (item.active) break;

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
                if (!item.active) break;

                item.active = false;
                break;
            }
        }
    };
    toplevelCtx.replayOpenApps();

    auto lastFrame{std::chrono::steady_clock::now()};
    float smoothedDt{1.f / 60.f};

    auto& popup{Popup::get()};

    auto renderFrame = [&] {
        auto now{std::chrono::steady_clock::now()};
        float dt{std::chrono::duration<float>(now - lastFrame).count()};
        lastFrame = now;

        if (dt < 0.f) dt = 0.f;
        if (dt > 0.03f) dt = 0.03f;

        float alpha{0.2f};
        smoothedDt += (dt - smoothedDt) * alpha;

        float visualDockHeight{dockConfig.padding.vertical() +
                               dockConfig.itemMargin.vertical() +
                               dockConfig.itemSize};
        int regionHeight{static_cast<int>(visualDockHeight)};
        int regionY{ls.height - regionHeight};

        if (regionY < 0) regionY = 0;
        if (regionHeight < 0) regionHeight = 0;

        static int lastRegionY{-1};
        static int lastRegionHeight{-1};
        if (regionY != lastRegionY || regionHeight != lastRegionHeight) {
            ls.setInputRegion(0, regionY, ls.width, regionHeight);
            lastRegionY = regionY;
            lastRegionHeight = regionHeight;
        }

        ls.applyPendingResize();

        if (!ls.isResizing) {
            auto gfx{GfxContext::get()};

            if (gfx && popup.isConfigured() &&
                popup.eglSurface != EGL_NO_SURFACE) {
                eglMakeCurrent(gfx->display, gfx->surface, gfx->surface,
                               gfx->context);
            }

            renderer.clearViewport(ls.width, ls.height);
            renderer.beginFrame(ls.width, ls.height);

            handleDock(renderer.vg, iconRenderer, ls, smoothedDt);

            renderer.endFrame();

            gfx->swapBuffers();

            if (gfx && popup.isConfigured() &&
                popup.eglSurface != EGL_NO_SURFACE) {
                eglMakeCurrent(gfx->display, popup.eglSurface, popup.eglSurface,
                               gfx->context);

                renderer.clearViewport(popup.width, popup.height);
                renderer.beginFrame(popup.width, popup.height);

                popup.render(renderer.vg);

                renderer.endFrame();
                eglSwapBuffers(gfx->display, popup.eglSurface);

                eglMakeCurrent(gfx->display, gfx->surface, gfx->surface,
                               gfx->context);
            }

            wl_surface_commit(ls.surface);
        }
        wl_display_flush(wl.display);
    };

    renderFrame();

    while (!ls.closed && wl.dispatch() != -1) {
        renderFrame();
    }

    return 0;
}
