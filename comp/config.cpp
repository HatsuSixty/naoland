#include "config.hpp"

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_keyboard.h>
#include "wlr-wrap-end.hpp"

#include <xkbcommon/xkbcommon-keysyms.h>

Config::Config()
{
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT,
            .keysym = XKB_KEY_Tab,
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = NAOLAND_COMMAND_SWITCH_TASK,
            },
        });
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT,
            .keysym = XKB_KEY_Escape,
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = NAOLAND_COMMAND_QUIT_SERVER,
            },
        });
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT,
            .keysym = XKB_KEY_Q,
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = NAOLAND_COMMAND_CLOSE,
            },
        });
}
