#include "config.hpp"

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_keyboard.h>
#include "wlr-wrap-end.hpp"

#include <xkbcommon/xkbcommon-keysyms.h>

Config::Config()
{
    /*
     * Default configuration
     */

    // Keybindings
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT,
            .keysym = XKB_KEY_Tab,
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = {
                    .kind = NAOLAND_COMMAND_SWITCH_TASK,
                },
            },
        });
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT,
            .keysym = XKB_KEY_Escape,
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = {
                    .kind = NAOLAND_COMMAND_QUIT_SERVER,
                },
            },
        });
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT,
            .keysym = XKB_KEY_Q,
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = {
                    .kind = NAOLAND_COMMAND_CLOSE,
                },
            },
        });
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT,
            .keysym = XKB_KEY_1,
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = {
                    .kind = NAOLAND_COMMAND_SWITCH_WORKSPACE,
                    .param = 1,
                },
            },
        });
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT,
            .keysym = XKB_KEY_2,
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = {
                    .kind = NAOLAND_COMMAND_SWITCH_WORKSPACE,
                    .param = 2,
                },
            },
        });

    // Border
    border.width = 3;
    border.color.focused = 0xFFFF00FF;
    border.color.unfocused = 0xFFFFFFFF;

    // Animations
    animation.enabled = true;
    animation.duration = 200;
    animation.play_percentage = 0.25;
}

void int_to_float_array(uint32_t color, float dst[4])
{
    dst[3] = (float)((uint8_t)(color) & 0xFF)/255;
    dst[2] = (float)((uint8_t)(color >> 8) & 0xFF)/255;
    dst[1] = (float)((uint8_t)(color >> 16) & 0xFF)/255;
    dst[0] = (float)((uint8_t)(color >> 24) & 0xFF)/255;
}
