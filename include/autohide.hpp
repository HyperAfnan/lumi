#ifndef AUTOHIDE_HPP
#define AUTOHIDE_HPP

#include "spring.hpp"

class AutoHide {
   public:
    void update(float dt, bool enabled, float hideDelay, bool keepVisible,
                float slideDistance);
    float offset() const;

    static AutoHide& get() {
        static AutoHide instance;
        return instance;
    }

   private:
    AutoHide() = default;
    AutoHide(const AutoHide&) = delete;
    AutoHide& operator=(const AutoHide&) = delete;

    Spring slideSpring{0.f};
    float hideTimer{0.f};
    bool enabled{false};
};

#endif  // AUTOHIDE_HPP
