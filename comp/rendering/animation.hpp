#ifndef NAOLAND_ANIMATION_HPP
#define NAOLAND_ANIMATION_HPP

#include <cstdint>

typedef void (*AnimationFinishCallback)(void*);

enum AnimationKind {
    ANIMATION_FADE_IN,
    ANIMATION_FADE_OUT,
};

struct AnimationOptions {
    AnimationKind kind;
    int duration;
    AnimationFinishCallback callback;
    void* callback_data;
};

class Animation {
private:
    bool animating;
    int64_t start_time;
    double animation_factor = 0;
    AnimationOptions options;

public:
    void start(AnimationOptions options);
    double get_factor();
    bool is_animating();
    void update();
};

#endif
