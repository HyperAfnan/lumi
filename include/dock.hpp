#ifndef DOCK_HPP
#define DOCK_HPP

#include <nanovg.h>

#include "icon.hpp"
#include "surface.hpp"

void handleDock(NVGcontext* vg, IconRenderer& iconRenderer, LayerSurface& ls,
                float dt);
void handlePopup(NVGcontext* vg, PopupSurface& popup);

#endif  // DOCK_HPP
