#ifndef RENDERER_HPP
#define RENDERER_HPP
#define NANOVG_GLES3_IMPLEMENTATION

#include <nanovg.h>

class Renderer {
   public:
    NVGcontext* vg{nullptr};

    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void beginFrame(int w, int h, float devicePixelRatio = 1.f) const;
    void endFrame() const;
    void clearViewport(int w, int h) const;
};

#endif  // RENDERER_HPP
