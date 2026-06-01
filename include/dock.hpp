#ifndef DOCK_HPP
#define DOCK_HPP

#include <nanovg.h>

#include "icon.hpp"

void handleDock(NVGcontext* vg, IconRenderer& iconRenderer, int w, int h,
                float dt);

#endif  // DOCK_HPP
