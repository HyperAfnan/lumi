#ifndef POPUP_HPP
#define POPUP_HPP

#include <EGL/egl.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <cstdint>

#include "nanovg.h"
#include "surface.hpp"

extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#define namespace ns
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#undef namespace
#pragma clang diagnostic pop
}

class Popup {
   public:
    static Popup& get();

    void create(LayerSurface& ls, int appIndex, int iconX, int iconY,
                int iconWidth, int iconHeight, int menuWidth, int menuHeight,
                std::uint32_t serial);
    void destroy();
    void reposition(int iconX, int iconY, int iconWidth, int iconHeight);
    void render(NVGcontext* vg);

    bool isOpen() const { return surface != nullptr; }
    bool isConfigured() const { return configured; }
    int sourceAppIndex() const { return srcAppIndex; }
    wl_surface* wlSurface() const { return surface; }

    EGLSurface eglSurface{EGL_NO_SURFACE};
    int width{0};
    int height{0};

   private:
    Popup() = default;
    ~Popup();
    Popup(const Popup&) = delete;
    Popup& operator=(const Popup&) = delete;

    wl_surface* surface{nullptr};
    xdg_surface* xdgSurface{nullptr};
    xdg_popup* xdgPopup{nullptr};
    wl_egl_window* eglWindow{nullptr};
    int srcAppIndex{-1};
    bool configured{false};
    int anchorX{0};
    int anchorY{0};
    int anchorSize{0};

    static void onXdgConfigure(void* data, xdg_surface* xdgSurface,
                               std::uint32_t serial);
    static void onPopupDone(void* data, xdg_popup* xdgPopup);
    static void onPopupConfigure(void* data, xdg_popup* xdgPopup,
                                 std::int32_t x, std::int32_t y,
                                 std::int32_t width, std::int32_t height);
    static void onPopupRepositioned(void* data, xdg_popup* xdgPopup,
                                    std::uint32_t token);

    static inline const xdg_surface_listener xdgSurfaceListener{
        .configure = onXdgConfigure,
    };
    static inline const xdg_popup_listener xdgPopupListener{
        .configure = onPopupConfigure,
        .popup_done = onPopupDone,
        .repositioned = onPopupRepositioned,
    };
};

#endif
