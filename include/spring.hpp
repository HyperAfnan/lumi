#ifndef SPRING_HPP
#define SPRING_HPP

struct SpringConfig {
    float stiffness{420.f};
    float damping{28.f};
    float mass{1.f};
};

class Spring {
   public:
    explicit Spring(float value = 0.f, SpringConfig config = SpringConfig{});

    void setTarget(float target);
    void update(float dt);

    float get() const;
    float velocity() const;
    bool resting() const;

   private:
    float current;
    float target;
    float velocityValue{0.f};

    SpringConfig config;
};

#endif  // SPRING_HPP