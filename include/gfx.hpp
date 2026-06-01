#ifndef GFX_HPP
#define GFX_HPP

#include <EGL/egl.h>

#include "context.hpp"
#include "surface.hpp"

class GfxContext {
   public:
    EGLDisplay display{EGL_NO_DISPLAY};
    EGLContext context{EGL_NO_CONTEXT};
    EGLSurface surface{EGL_NO_SURFACE};

    GfxContext(WaylandContext& wl, LayerSurface& ls);
    ~GfxContext();

    GfxContext(const GfxContext&) = delete;
    GfxContext& operator=(const GfxContext&) = delete;

    void swapBuffers() const;
};

#endif  // GFX_HPP