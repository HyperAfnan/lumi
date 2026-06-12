#include "popup.hpp"

#include <nanovg.h>

#include <cstdio>

#include "app.hpp"
#include "config.hpp"
#include "context.hpp"
#include "gfx.hpp"
#include "surface.hpp"

static void drawPopupBg(NVGcontext* vg, float x, float y, float w, float h,
                        float r) {
    NVGpaint shadow{nvgBoxGradient(vg, x, y + h * 0.5f, w, h * 0.5f, r, 24.f,
                                   nvgRGBAf(0.f, 0.f, 0.f, 0.35f),
                                   nvgRGBAf(0.f, 0.f, 0.f, 0.f))};
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x - 20.f, y, w + 40.f, h + 32.f, r);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillColor(vg, nvgRGBAf(0.05f, 0.05f, 0.06f, 0.72f));
    nvgFill(vg);

    NVGpaint glow{nvgLinearGradient(vg, x, y, x, y + h,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.08f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f))};
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, r);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.18f));
    nvgStroke(vg);

    float midX{x + w * 0.5f};

    NVGpaint rimL{nvgLinearGradient(vg, x + r, y, midX, y,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.35f))};
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + r, y + 1.f);
    nvgLineTo(vg, midX, y + 1.f);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokePaint(vg, rimL);
    nvgStroke(vg);

    NVGpaint rimR{nvgLinearGradient(vg, midX, y, x + w - r, y,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.35f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f))};
    nvgBeginPath(vg);
    nvgMoveTo(vg, midX, y + 1.f);
    nvgLineTo(vg, x + w - r, y + 1.f);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokePaint(vg, rimR);
    nvgStroke(vg);
}

Popup& Popup::get() {
    static Popup instance;
    return instance;
}

Popup::~Popup() {
    if (eglSurface != EGL_NO_SURFACE) {
        auto* gfx{GfxContext::get()};
        if (gfx) {
            eglMakeCurrent(gfx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
            eglDestroySurface(gfx->display, eglSurface);
        }
    }
    if (eglWindow) wl_egl_window_destroy(eglWindow);
    if (xdgPopup) xdg_popup_destroy(xdgPopup);
    if (xdgSurface) xdg_surface_destroy(xdgSurface);
    if (surface) wl_surface_destroy(surface);
}

void Popup::onXdgConfigure(void*, xdg_surface* xdgSurface,
                           std::uint32_t serial) {
    auto& self{get()};
    xdg_surface_ack_configure(xdgSurface, serial);

    if (!self.eglWindow) {
        auto gfx{GfxContext::get()};

        self.eglWindow =
            wl_egl_window_create(self.surface, self.width, self.height);
        if (gfx) {
            self.eglSurface = eglCreateWindowSurface(
                gfx->display, gfx->config,
                reinterpret_cast<EGLNativeWindowType>(self.eglWindow), nullptr);
        }
    } else {
        wl_egl_window_resize(self.eglWindow, self.width, self.height, 0, 0);
    }

    self.configured = true;
}

void Popup::onPopupDone(void*, xdg_popup*) { get().destroy(); }

void Popup::onPopupConfigure(void*, xdg_popup*, std::int32_t, std::int32_t,
                             std::int32_t, std::int32_t) {}

void Popup::destroy() {
    auto* gfx{GfxContext::get()};

    if (gfx && eglSurface != EGL_NO_SURFACE) {
        eglMakeCurrent(gfx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroySurface(gfx->display, eglSurface);
        eglSurface = EGL_NO_SURFACE;
        if (WaylandContext::get().layerShell) {
            eglMakeCurrent(gfx->display, gfx->surface, gfx->surface,
                           gfx->context);
        }
    }
    if (eglWindow) {
        wl_egl_window_destroy(eglWindow);
        eglWindow = nullptr;
    }
    if (xdgPopup) {
        xdg_popup_destroy(xdgPopup);
        xdgPopup = nullptr;
    }
    if (xdgSurface) {
        xdg_surface_destroy(xdgSurface);
        xdgSurface = nullptr;
    }
    if (surface) {
        wl_surface_destroy(surface);
        surface = nullptr;
    }
    configured = false;
    srcAppIndex = -1;
}

void Popup::create(LayerSurface& ls, int appIndex, int iconX, int iconY,
                   int iconWidth, int iconHeight, int menuWidth, int menuHeight,
                   std::uint32_t serial) {
    if (surface) destroy();

    auto& wl{WaylandContext::get()};

    srcAppIndex = appIndex;
    width = menuWidth;
    height = menuHeight;
    configured = false;

    surface = wl_compositor_create_surface(wl.compositor);

    xdgSurface = xdg_wm_base_get_xdg_surface(wl.xdgWmBase, surface);
    xdg_surface_add_listener(xdgSurface, &xdgSurfaceListener, nullptr);

    xdg_positioner* positioner{xdg_wm_base_create_positioner(wl.xdgWmBase)};
    xdg_positioner_set_size(positioner, menuWidth, menuHeight);
    xdg_positioner_set_anchor_rect(positioner, iconX, iconY, iconWidth,
                                   iconHeight);
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_TOP);
    xdg_positioner_set_constraint_adjustment(
        positioner, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    xdg_positioner_set_offset(positioner, 0, -8);

    xdgPopup = xdg_surface_get_popup(xdgSurface, nullptr, positioner);
    xdg_popup_add_listener(xdgPopup, &xdgPopupListener, nullptr);

    xdg_positioner_destroy(positioner);

    zwlr_layer_surface_v1_get_popup(ls.layerSurface, xdgPopup);
    xdg_popup_grab(xdgPopup, wl.seat, serial);

    wl_surface_commit(surface);

    anchorX = iconX;
    anchorY = iconY;
    anchorSize = iconWidth;
}

void Popup::reposition(int iconX, int iconY, int iconWidth, int iconHeight) {
    if (!xdgPopup) return;

    if (anchorX == iconX && anchorY == iconY && anchorSize == iconWidth) return;

    anchorX = iconX;
    anchorY = iconY;
    anchorSize = iconWidth;

    auto& wl{WaylandContext::get()};

    xdg_positioner* positioner{xdg_wm_base_create_positioner(wl.xdgWmBase)};
    xdg_positioner_set_size(positioner, width, height);
    xdg_positioner_set_anchor_rect(positioner, iconX, iconY, iconWidth,
                                   iconHeight);
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_TOP);
    xdg_positioner_set_constraint_adjustment(
        positioner, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    xdg_positioner_set_offset(positioner, 0, -8);

    xdg_popup_reposition(xdgPopup, positioner, 0);
    xdg_positioner_destroy(positioner);

    wl_surface_commit(surface);
}

void Popup::render(NVGcontext* vg) {
    auto& config{DockConfig::get()};
    auto& mouseCtx{MouseContext::get()};
    auto& items{config.items};

    if (srcAppIndex < 0 || srcAppIndex >= static_cast<int>(items.size()))
        return;

    drawPopupBg(vg, 0.f, 0.f, width, height, 12.f);

    auto& clickedItem{items.at(srcAppIndex)};
    const auto& actions{clickedItem.app.actions};

    static bool prevPressed{false};

    float itemY{8.f};
    for (std::size_t i{0}; i < actions.size(); i++) {
        float rowY{itemY + i * 36.f};

        bool hovered{false};
        if (mouseCtx.inside && mouseCtx.currentSurface == surface &&
            mouseCtx.x >= 0 && mouseCtx.x <= width && mouseCtx.y >= rowY &&
            mouseCtx.y < rowY + 36.f) {
            hovered = true;
        }

        if (hovered && mouseCtx.pressed && !prevPressed) {
            clickedItem.app.launchAction(actions[i]);
            destroy();
            break;
        }

        if (hovered) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, 6.f, rowY + 2.f, width - 12.f, 32.f, 6.f);
            nvgFillColor(vg, nvgRGBAf(0.5f, 0.5f, 0.5f, 0.5f));
            nvgFill(vg);
        }

        nvgFontSize(vg, 13.f);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 1.f));
        nvgText(vg, 16.f, rowY + 18.f, actions[i].displayName.c_str(), nullptr);
    }

    prevPressed = mouseCtx.pressed;
}
