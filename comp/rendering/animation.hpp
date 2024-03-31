#ifndef NAOLAND_ANIMATION_HPP
#define NAOLAND_ANIMATION_HPP

#include <cstdint>

enum AnimationKind {
    ANIMATION_FADE_IN,
    ANIMATION_FADE_OUT,
};

class Animation {
private:
    bool animating;
    int64_t start_time;
    double animation_factor = 0;
    AnimationKind kind;
    int duration;

public:
    void start(AnimationKind kind, int duration);
    double get_factor();
    bool is_animating();
    void update();
};

#endif
