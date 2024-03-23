#include "config.hpp"

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_keyboard.h>
#include "wlr-wrap-end.hpp"

Config::Config()
{
    /* List of keysyms can be found at:
     * https://www.x.org/releases/current/doc/xproto/x11protocol.html#Latin_1_KEYSYMs
     */

    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT,
            .keysym = 0xFF09, // TAB
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = NAOLAND_COMMAND_SWITCH_TASK,
            },
        });
    keybindings.push_back(Keybinding {
            .modifiers = WLR_MODIFIER_ALT,
            .keysym = 0xFF1B, // ESC
            .action = KeyAction {
                .kind = NAOLAND_ACTION_COMMAND,
                .compositor_command = NAOLAND_COMMAND_QUIT_SERVER,
            },
        });
}
