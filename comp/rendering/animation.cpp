#include "animation.hpp"

#include "util.hpp"

void Animation::start(AnimationOptions options)
{
    start_time = get_time_milli();
    animating = true;
    this->options = options;
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

    switch (options.kind) {
    case ANIMATION_FADE_IN: {
        auto now = get_time_milli();
        auto duration = now - start_time;

        animation_factor = static_cast<float>(duration) / options.duration;

        if (animation_factor >= 1) {
            if (options.callback)
                options.callback(options.callback_data);

            animation_factor = 1.0f;
            animating = false;
        }
    } break;
    case ANIMATION_FADE_OUT: {
        auto now = get_time_milli();
        auto duration = now - start_time;

        animation_factor = 1.0f - static_cast<float>(duration) / this->options.duration;

        if (animation_factor <= 0) {
            if (options.callback)
                options.callback(options.callback_data);

            animation_factor = 0;
            animating = false;
        }
    } break;
    }
}
