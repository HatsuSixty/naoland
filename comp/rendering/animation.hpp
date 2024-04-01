#ifndef NAOLAND_ANIMATION_HPP
#define NAOLAND_ANIMATION_HPP

#include "surface/surface.hpp"

#include <cstdint>

typedef void (*AnimationFinishCallback)(void*);

enum AnimationKind {
    ANIMATION_FADE_IN,
    ANIMATION_FADE_OUT,
};

struct AnimationOptions {
    AnimationKind kind;
    AnimationFinishCallback callback;
    void* callback_data;
};

class Animation {
private:
    bool animating;
    int64_t start_time;
    double animation_factor = 0;
    AnimationOptions options;
    Surface& surface;

public:
    Animation(Surface& surface);

    void start(AnimationOptions options);
    double get_factor();
    bool is_animating();
    void update();
};

#endif
