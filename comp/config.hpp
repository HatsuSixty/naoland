#ifndef NAOLAND_CONFIG_HPP
#define NAOLAND_CONFIG_HPP

#include <cstdint>
#include <list>
#include <string>
#include <xkbcommon/xkbcommon.h>

enum CompositorCommand {
    NAOLAND_COMMAND_QUIT_SERVER,
    NAOLAND_COMMAND_SWITCH_TASK,
};

enum KeyActionKind {
    NAOLAND_ACTION_COMMAND,
    NAOLAND_ACTION_SPAWN,
};

struct KeyAction {
    KeyActionKind kind;
    std::string spawn_command;
    CompositorCommand compositor_command;
};

struct Keybinding {
    uint32_t modifiers;
    xkb_keysym_t keysym;
    KeyAction action;
};

struct Config {
    std::list<Keybinding> keybindings;

    Config();
};

#endif
