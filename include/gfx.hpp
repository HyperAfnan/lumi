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
    EGLConfig config{nullptr};

    static void init(WaylandContext& wl, LayerSurface& ls);
    static GfxContext* get();

    ~GfxContext();

    GfxContext(const GfxContext&) = delete;
    GfxContext& operator=(const GfxContext&) = delete;

    void swapBuffers() const;

   private:
    static GfxContext* instance;

    GfxContext(WaylandContext& wl, LayerSurface& ls);
};

#endif  // GFX_HPP