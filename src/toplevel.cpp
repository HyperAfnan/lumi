#include "toplevel.hpp"

ToplevelContext& ToplevelContext::get() {
    static ToplevelContext instance;
    return instance;
}

void ToplevelContext::setBackend(std::unique_ptr<ToplevelBackend> backend) {
    this->backend = std::move(backend);

    if (!this->backend) return;

    this->backend->setEvents({
        .appOpened = &onAppOpen,
        .appClosed = &onAppClose,
    });

    this->backend->init(&appIdCount);
}

void ToplevelContext::replayOpenApps() const {
    if (!onAppOpen) return;

    for (const auto& [appId, count] : appIdCount) {
        if (count > 0) onAppOpen(appId);
    }
}

template <typename Handle>
static void handleAppIdUpdate(
    std::unordered_map<Handle, std::string>& handleAppIds,
    AppIds* appIdCount,
    const ToplevelEvents& events,
    Handle handle,
    const char* appId) {
    if (!appIdCount) return;

    auto& current{handleAppIds[handle]};

    if (!current.empty()) {
        auto it{appIdCount->find(current)};
        if (it != appIdCount->end()) {
            if (--it->second == 0) {
                appIdCount->erase(it);
                if (events.appClosed && *events.appClosed)
                    (*events.appClosed)(current);
            }
        }
    }

    current = appId;
    auto& count{(*appIdCount)[current]};
    ++count;

    if (events.appOpened && *events.appOpened)
        (*events.appOpened)(current);
}

template <typename Handle>
static void handleClosedCleanup(
    std::unordered_map<Handle, std::string>& handleAppIds,
    AppIds* appIdCount,
    const ToplevelEvents& events,
    Handle handle) {
    if (auto it{handleAppIds.find(handle)}; it != handleAppIds.end()) {
        const auto appId{it->second};
        if (!appId.empty() && appIdCount) {
            auto countIt{appIdCount->find(appId)};
            if (countIt != appIdCount->end()) {
                if (--countIt->second == 0) {
                    appIdCount->erase(countIt);
                    if (events.appClosed && *events.appClosed)
                        (*events.appClosed)(appId);
                }
            }
        }
        handleAppIds.erase(it);
    }
}

ExtForeignBackend::ExtForeignBackend(ext_foreign_toplevel_list_v1* manager)
    : manager{manager} {}

void ExtForeignBackend::init(AppIds* appIdCount) {
    this->appIdCount = appIdCount;
    ext_foreign_toplevel_list_v1_add_listener(manager, &managerListener, this);
}

void ExtForeignBackend::onManagerToplevel(
    void* data, ext_foreign_toplevel_list_v1*,
    ext_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<ExtForeignBackend*>(data)};

    ext_foreign_toplevel_handle_v1_add_listener(handle, &handleListener, data);
    self.handleAppIds[handle] = "";
}

void ExtForeignBackend::onHandleAppId(void* data,
                                      ext_foreign_toplevel_handle_v1* handle,
                                      const char* appId) {
    auto& self{*static_cast<ExtForeignBackend*>(data)};
    handleAppIdUpdate(self.handleAppIds, self.appIdCount, self.events, handle,
                      appId);
}

void ExtForeignBackend::onHandleClosed(void* data,
                                       ext_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<ExtForeignBackend*>(data)};
    handleClosedCleanup(self.handleAppIds, self.appIdCount, self.events, handle);
    ext_foreign_toplevel_handle_v1_destroy(handle);
}

WlrForeignBackend::WlrForeignBackend(zwlr_foreign_toplevel_manager_v1* manager)
    : manager{manager} {}

void WlrForeignBackend::init(AppIds* appIdCount) {
    this->appIdCount = appIdCount;

    zwlr_foreign_toplevel_manager_v1_add_listener(manager, &managerListener,
                                                  this);
}

void WlrForeignBackend::onManagerToplevel(
    void* data, zwlr_foreign_toplevel_manager_v1*,
    zwlr_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<WlrForeignBackend*>(data)};

    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &handleListener, data);

    self.handleAppIds[handle] = "";
}

void WlrForeignBackend::onHandleAppId(void* data,
                                      zwlr_foreign_toplevel_handle_v1* handle,
                                      const char* appId) {
    auto& self{*static_cast<WlrForeignBackend*>(data)};
    handleAppIdUpdate(self.handleAppIds, self.appIdCount, self.events, handle,
                      appId);
}

void WlrForeignBackend::onHandleState(void* data,
                                      zwlr_foreign_toplevel_handle_v1* handle,
                                      wl_array* state) {
    auto& self{*static_cast<WlrForeignBackend*>(data)};

    bool active{false};
    if (state && state->size > 0 && state->data != nullptr) {
        for (const uint32_t* entry = static_cast<const uint32_t*>(state->data);
             reinterpret_cast<const char*>(entry) <
             reinterpret_cast<const char*>(state->data) + state->size;
             ++entry) {
            if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
                active = true;
                break;
            }
        }
    }

    if (active) {
        self.activeHandles.insert(handle);
    } else {
        self.activeHandles.erase(handle);
    }
}

void WlrForeignBackend::onHandleClosed(
    void* data, zwlr_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<WlrForeignBackend*>(data)};

    self.activeHandles.erase(handle);
    handleClosedCleanup(self.handleAppIds, self.appIdCount, self.events, handle);
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
}