#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION

#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>


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
    
    renderer.loadConfiguredFont();

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

    auto configPath = DockConfig::configFile();
    std::string configFilename = configPath.filename().string();
    std::string configDir = configPath.parent_path().string();

    int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd < 0) {
        logger::error("Failed to initialize inotify");
    }

    int watch_desc = inotify_add_watch(inotify_fd, configDir.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO);

    int wl_fd = wl_display_get_fd(wl.display);

    struct pollfd fds[2];
    fds[0].fd = wl_fd;
    fds[0].events = POLLIN;
    
    fds[1].fd = inotify_fd;
    fds[1].events = POLLIN;

    renderFrame();

    while (!ls.closed) {
        wl_display_flush(wl.display);

        int ret = poll(fds, 2, 16); 

        if (ret < 0) {
            if (errno == EINTR) continue;
            logger::error("Poll failed");
            break;
        }

        if (fds[0].revents & POLLIN) {
            if (wl_display_dispatch(wl.display) == -1) {
                break;
            }
        }
        
        if (fds[1].revents & POLLIN) {
            char buffer[4096]
                __attribute__((aligned(__alignof__(struct inotify_event))));
            const struct inotify_event *event;
            ssize_t len;

            while ((len = read(inotify_fd, buffer, sizeof(buffer))) > 0) {
                char *ptr = buffer;
                
                while (ptr < buffer + len) {
                    event = (const struct inotify_event *)ptr;

                    if (event->len && configFilename == event->name) {
                        logger::info("Config change detected via inotify.");
                        
                        if (dockConfig.reloadConfig()) {
                            if (popup.isOpen()) {
                                popup.destroy();
                            }

                            iconIndex.preload(
                                dockConfig.items |
                                std::views::transform([](const DockItem& item) { return item.app; }) |
                                std::ranges::to<std::vector>());

                            renderer.loadConfiguredFont();

                            int newHeight = static_cast<int>(dockConfig.height());
                            int marginTop = static_cast<int>(dockConfig.margin.top);
                            int marginRight = static_cast<int>(dockConfig.margin.right);
                            int marginBottom = static_cast<int>(dockConfig.margin.bottom);
                            int marginLeft = static_cast<int>(dockConfig.margin.left);

                            zwlr_layer_surface_v1_set_size(ls.layerSurface, 0, newHeight);
                            zwlr_layer_surface_v1_set_exclusive_zone(ls.layerSurface, dockConfig.surfaceHeight());
                            zwlr_layer_surface_v1_set_margin(ls.layerSurface, marginTop, marginRight, marginBottom, marginLeft);
                            
                            wl_surface_commit(ls.surface);
                        }
                    }
                    
                    ptr += sizeof(struct inotify_event) + event->len;
                }
            }
        }

        renderFrame();
    }

    if (inotify_fd >= 0) {
        close(inotify_fd);
    }
    return 0;
}
