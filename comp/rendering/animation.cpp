#include "animation.hpp"

#include "util.hpp"

void Animation::start(AnimationKind kind, int duration)
{
    start_time = get_time_milli();
    animating = true;
    this->kind = kind;
    this->duration = duration;
}

double Animation::get_factor()
{
    return animation_factor;
}

bool Animation::is_animating()
{
    return animating;
}

void Animation::update()
{
    if (!animating)
        return;

    switch (kind) {
    case ANIMATION_FADE_IN: {
        auto now = get_time_milli();
        auto duration = now - start_time;

        animation_factor = static_cast<float>(duration) / this->duration;

        if (animation_factor >= 1) {
            animating = false;
            animation_factor = 1.0f;
        }
    } break;
    case ANIMATION_FADE_OUT:
        break;
    }
}
