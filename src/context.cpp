#include "context.hpp"

#include <wayland-client-protocol.h>

#include <cstdlib>
#include <cstring>

#include "logger.hpp"
#include "toplevel.hpp"

WaylandContext& WaylandContext::get() {
    static WaylandContext instance;
    return instance;
}

WaylandContext::WaylandContext() {
    display = wl_display_connect(nullptr);

    if (!display) {
        logger::error("Cannot connect to Wayland display");
        std::exit(EXIT_FAILURE);
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, this);
    roundtrip();

    if (!compositor || !layerShell) {
        logger::error("Missing wl_compositor or zwlr_layer_shell_v1");
        std::exit(EXIT_FAILURE);
    }
}

WaylandContext::~WaylandContext() {
    if (pointer) wl_pointer_destroy(pointer);
    if (seat) wl_seat_destroy(seat);
    if (layerShell) zwlr_layer_shell_v1_destroy(layerShell);
    if (compositor) wl_compositor_destroy(compositor);
    if (registry) wl_registry_destroy(registry);
    if (display) wl_display_disconnect(display);
    if (toplevelManager) ext_foreign_toplevel_list_v1_destroy(toplevelManager);
}

void WaylandContext::roundtrip() const { wl_display_roundtrip(display); }

int WaylandContext::dispatch() const { return wl_display_dispatch(display); }

void WaylandContext::onGlobal(void* data, wl_registry* registry,
                              std::uint32_t name, const char* interface,
                              std::uint32_t version) {
    auto& self{*static_cast<WaylandContext*>(data)};

    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        self.compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
    } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) ==
               0) {
        self.layerShell = static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(
            registry, name, &zwlr_layer_shell_v1_interface, 1));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        self.seat = static_cast<wl_seat*>(
            wl_registry_bind(registry, name, &wl_seat_interface, 7));
        wl_seat_add_listener(self.seat, &seatListener, &self);
    } else if (std::strcmp(interface,
                           ext_foreign_toplevel_list_v1_interface.name) == 0) {
        self.toplevelManager =
            static_cast<ext_foreign_toplevel_list_v1*>(wl_registry_bind(
                registry, name, &ext_foreign_toplevel_list_v1_interface, 1));

        ToplevelContext::get().init(self.toplevelManager);
    }
}

void WaylandContext::onGlobalRemove(void* data, wl_registry* registry,
                                    std::uint32_t name) {
    auto& self{*static_cast<WaylandContext*>(data)};

    if (self.compositor &&
        wl_proxy_get_id((wl_proxy*)self.compositor) == name) {
        wl_compositor_destroy(self.compositor);
        self.compositor = nullptr;
    } else if (self.layerShell &&
               wl_proxy_get_id((wl_proxy*)self.layerShell) == name) {
        zwlr_layer_shell_v1_destroy(self.layerShell);
        self.layerShell = nullptr;
    }
}

void WaylandContext::onSeatCapabilities(void* data, wl_seat* seat,
                                        uint32_t caps) {
    auto& self{*static_cast<WaylandContext*>(data)};
    bool hasPointer{static_cast<bool>(caps & WL_SEAT_CAPABILITY_POINTER)};

    if (hasPointer && !self.pointer) {
        self.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(self.pointer, &pointerListener, &self);
    } else if (!hasPointer && self.pointer) {
        wl_pointer_destroy(self.pointer);
        self.pointer = nullptr;
    }
}

void WaylandContext::onPointerEnter(void* data, wl_pointer*, uint32_t,
                                    wl_surface*, wl_fixed_t sx, wl_fixed_t sy) {
    auto& self{*static_cast<WaylandContext*>(data)};

    auto& mouseContext{MouseContext::get()};

    mouseContext.x = wl_fixed_to_double(sx);
    mouseContext.y = wl_fixed_to_double(sy);
    mouseContext.inside = true;
}

void WaylandContext::onPointerLeave(void* data, wl_pointer*, uint32_t,
                                    wl_surface*) {
    auto& self{*static_cast<WaylandContext*>(data)};

    auto& mouseContext{MouseContext::get()};
    mouseContext.inside = false;
    mouseContext.x = -9999.f;
    mouseContext.y = -9999.f;
}

void WaylandContext::onPointerMotion(void* data, wl_pointer*, uint32_t,
                                     wl_fixed_t sx, wl_fixed_t sy) {
    auto& self{*static_cast<WaylandContext*>(data)};

    auto& mouseContext{MouseContext::get()};
    mouseContext.x = wl_fixed_to_double(sx);
    mouseContext.y = wl_fixed_to_double(sy);
}

void WaylandContext::onPointerButton(void* data, wl_pointer*, uint32_t serial,
                                     uint32_t /*time*/, uint32_t button,
                                     uint32_t state) {
    auto& self{*static_cast<WaylandContext*>(data)};

    if (button != 0x110) return;

    auto& mouseContext{MouseContext::get()};

    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        mouseContext.pressed = true;
        mouseContext.clickX = mouseContext.x;
        mouseContext.clickY = mouseContext.y;
    } else {
        mouseContext.pressed = false;
    }
}