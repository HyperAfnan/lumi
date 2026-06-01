#ifndef GTK_HPP
#define GTK_HPP

#include <dlfcn.h>

struct GtkAPI {
    using GtkInitFn = void (*)(void);
    using GdkDisplayGetDefaultFn = void* (*)(void);
    using GtkIconThemeGetForDisplayFn = void* (*)(void*);
    using GtkIconThemeLookupIconFn = void* (*)(void*, const char*, const char*,
                                               int, int, int, int);
    using GtkIconPaintableGetFileFn = void* (*)(void*);
    using GFileGetPathFn = char* (*)(void*);
    using GObjectUnrefFn = void (*)(void*);
    using GFreeFn = void (*)(void*);

    void* gtkLib{nullptr};
    void* gioLib{nullptr};
    void* glibLib{nullptr};
    void* gobjLib{nullptr};

    GtkInitFn gtkInit{nullptr};
    GdkDisplayGetDefaultFn getDisplay{nullptr};
    GtkIconThemeGetForDisplayFn getTheme{nullptr};
    GtkIconThemeLookupIconFn lookupIcon{nullptr};
    GtkIconPaintableGetFileFn getFile{nullptr};
    GFileGetPathFn getPath{nullptr};
    GObjectUnrefFn unref{nullptr};
    GFreeFn freeMem{nullptr};

    bool valid{false};

    GtkAPI() {
        gtkLib = dlopen("libgtk-4.so.1", RTLD_LAZY);
        gioLib = dlopen("libgio-2.0.so.0", RTLD_LAZY);
        glibLib = dlopen("libglib-2.0.so.0", RTLD_LAZY);
        gobjLib = dlopen("libgobject-2.0.so.0", RTLD_LAZY);

        if (!gtkLib || !gioLib || !glibLib || !gobjLib) return;

        gtkInit = load<GtkInitFn>(gtkLib, "gtk_init");
        getDisplay =
            load<GdkDisplayGetDefaultFn>(gtkLib, "gdk_display_get_default");
        getTheme = load<GtkIconThemeGetForDisplayFn>(
            gtkLib, "gtk_icon_theme_get_for_display");
        lookupIcon = load<GtkIconThemeLookupIconFn>(
            gtkLib, "gtk_icon_theme_lookup_icon");
        getFile = load<GtkIconPaintableGetFileFn>(
            gtkLib, "gtk_icon_paintable_get_file");
        getPath = load<GFileGetPathFn>(gioLib, "g_file_get_path");
        unref = load<GObjectUnrefFn>(gobjLib, "g_object_unref");
        freeMem = load<GFreeFn>(glibLib, "g_free");

        valid = gtkInit && getDisplay && getTheme && lookupIcon && getFile &&
                getPath && unref && freeMem;

        if (valid) gtkInit();
    }

    GtkAPI(const GtkAPI&) = delete;
    GtkAPI& operator=(const GtkAPI&) = delete;
    GtkAPI(GtkAPI&&) = delete;
    GtkAPI& operator=(GtkAPI&&) = delete;

    ~GtkAPI() {
        if (gobjLib) dlclose(gobjLib);
        if (glibLib) dlclose(glibLib);
        if (gioLib) dlclose(gioLib);
        if (gtkLib) dlclose(gtkLib);
    }

   private:
    template <typename T>
    static T load(void* lib, const char* symbol) {
        dlerror();

        void* sym{dlsym(lib, symbol)};

        return dlerror() == nullptr ? reinterpret_cast<T>(sym) : nullptr;
    }
};

#endif  // GTK_HPP