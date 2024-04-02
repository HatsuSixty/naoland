#include "animation.hpp"

#include "server.hpp"
#include "util.hpp"

Animation::Animation(Surface& surface)
    : surface(surface)
{
}

void Animation::start(AnimationOptions options)
{
    if (!surface.get_server().config.animation.enabled) return;

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

    auto config = surface.get_server().config;
    float play_percentage = 1.0f
        - (options.ignore_play_percentage ? 1.0f
                                          : config.animation.play_percentage);

    switch (options.kind) {
    case ANIMATION_FADE_IN: {
        auto now = get_time_milli();
        auto duration = now - start_time
            + config.animation.duration * play_percentage;

        animation_factor = static_cast<float>(duration) / config.animation.duration;

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

        animation_factor = 1.0f - static_cast<float>(duration) / config.animation.duration;

        if (animation_factor <= play_percentage) {
            if (options.callback)
                options.callback(options.callback_data);

            animation_factor = 0;
            animating = false;
        }
    } break;
    }
}
