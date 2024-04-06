#ifndef NAOLAND_CONFIG_HPP
#define NAOLAND_CONFIG_HPP

#include <cstdint>
#include <list>
#include <string>
#include <xkbcommon/xkbcommon.h>

#include "rendering/animation.hpp"

enum CompositorCommand {
    NAOLAND_COMMAND_QUIT_SERVER,
    NAOLAND_COMMAND_SWITCH_TASK,
    NAOLAND_COMMAND_CLOSE,
    NAOLAND_COMMAND_SWITCH_WORKSPACE,
    NAOLAND_COMMAND_MOVE_TO_WORKSPACE,
};

enum KeyActionKind {
    NAOLAND_ACTION_COMMAND,
    NAOLAND_ACTION_SPAWN,
};

struct KeyAction {
    KeyActionKind kind;
    std::string spawn_command;
    struct {
        CompositorCommand kind;
        int param;
    } compositor_command;
};

struct Keybinding {
    uint32_t modifiers;
    xkb_keysym_t keysym;
    KeyAction action;
};

struct Config {
    std::list<Keybinding> keybindings;

    struct {
        struct {
            uint32_t focused;
            uint32_t unfocused;
        } color;
        int width;
    } border;

    struct {
        bool enabled;
        int duration;
        float play_percentage;
        AnimationRole window_animation_kind;
    } animation;

    struct {
        int press_action;
    } tablet;

    Config();
};

void int_to_float_array(uint32_t color, float dst[4]);

#endif
