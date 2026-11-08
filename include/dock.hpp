#ifndef DOCK_HPP
#define DOCK_HPP

#include <nanovg.h>

#include "icon.hpp"
#include "surface.hpp"

struct DockRect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    bool operator==(const DockRect&) const = default;
};

float getDockRestingWidth();
float getDockVisualHeight();
float getDockAnimatedWidth();

DockRect calculateDockInputRegion(int surfaceWidth, int surfaceHeight,
                                  float slideOffset, bool autoHideEnabled,
                                  float revealHeight);

void handleDock(NVGcontext* vg, IconRenderer& iconRenderer, LayerSurface& ls,
                float dt);

#endif  // DOCK_HPP
