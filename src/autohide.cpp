#include "autohide.hpp"

void AutoHide::update(float dt, bool isEnabled, float hideDelay,
                      bool keepVisible, float slideDistance) {
    if (!isEnabled) {
        if (enabled) {
            slideSpring = Spring{0.f};
            hideTimer = 0.f;
            enabled = false;
        }
        return;
    }

    if (!enabled) {
        slideSpring = Spring{slideDistance};
        hideTimer = hideDelay;
        enabled = true;
    }

    if (keepVisible) {
        hideTimer = 0.f;
        slideSpring.setTarget(0.f);
    } else {
        hideTimer += dt;
        if (hideTimer >= hideDelay) slideSpring.setTarget(slideDistance);
    }

    slideSpring.update(dt);
}

float AutoHide::offset() const { return enabled ? slideSpring.get() : 0.f; }
