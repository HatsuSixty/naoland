#include "keyboard.hpp"

#include "config.hpp"
#include "seat.hpp"
#include "server.hpp"
#include "surface/view.hpp"

#include <algorithm>
#include <xkbcommon/xkbcommon.h>

#include "wlr-wrap-start.hpp"
#include <wayland-util.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include "wlr-wrap-end.hpp"

/* This event is raised by the keyboard base wlr_input_device to signal
 * the destruction of the wlr_keyboard. It will no longer receive events
 * and should be destroyed. */
static void keyboard_handle_destroy(wl_listener* listener, void*)
{
    Keyboard& keyboard = naoland_container_of(listener, keyboard, destroy);

    std::vector<Keyboard*>& keyboards = keyboard.seat.keyboards;
    (void)std::ranges::remove(keyboards, &keyboard).begin();

    delete &keyboard;
}

static bool handle_keybinding_for_symbol(Keybinding& keybinding,
                                         xkb_keysym_t const sym,
                                         Keyboard const& keyboard)
{
    auto const modifiers = wlr_keyboard_get_modifiers(&keyboard.wlr);
    if (modifiers == (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT)
        && (sym >= XKB_KEY_XF86Switch_VT_1
            && sym <= XKB_KEY_XF86Switch_VT_12)) {
        if (wlr_backend_is_multi(keyboard.seat.server.backend)) {
            unsigned const vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
            wlr_session_change_vt(keyboard.seat.server.session, vt);
        }
        return true;
    }

    if (modifiers == keybinding.modifiers && sym == keybinding.keysym)
        switch (keybinding.action.kind) {
        case NAOLAND_ACTION_COMMAND: {
            Server& server = keyboard.seat.server;

            switch (keybinding.action.compositor_command.kind) {
            case NAOLAND_COMMAND_SWITCH_TASK: {
                /* Cycle to the next view */
                if (server.views.size() < 2) {
                    break;
                }
                View* next_view = *server.views.begin()++;
                server.focus_view(next_view);
            } break;
            case NAOLAND_COMMAND_QUIT_SERVER:
                wl_display_terminate(server.display);
                break;
            case NAOLAND_COMMAND_CLOSE:
                if (server.focused_view)
                    server.focused_view->close_animation();
                break;
            case NAOLAND_COMMAND_SWITCH_WORKSPACE:
                server.switch_workspace(keybinding.action.compositor_command.param);
                break;
            }

        } return true;
        case NAOLAND_ACTION_SPAWN:
            wlr_log(WLR_ERROR, "TODO: Spawn processes\n");
            return true;
        }

    return false;
}

/* This event is raised when a key is pressed or released. */
static void keyboard_handle_key(wl_listener* listener, void* data)
{
    Keyboard const& keyboard = naoland_container_of(listener, keyboard, key);

    auto const* event = static_cast<wlr_keyboard_key_event*>(data);
    wlr_seat* seat = keyboard.seat.wlr;

    wlr_idle_notifier_v1_notify_activity(keyboard.seat.server.idle_notifier,
                                         seat);

    /* Translate libinput keycode -> xkbcommon */
    uint32_t const keycode = event->keycode + 8;
    /* Get a list of keysyms based on the keymap for this keyboard */
    xkb_keysym_t const* syms;
    int32_t const nsyms
        = xkb_state_key_get_syms(keyboard.wlr.xkb_state, keycode, &syms);

    bool handled = false;
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // Handle keybindings
        for (int i = 0; i < nsyms; ++i) {
            for (auto& item : keyboard.seat.server.config.keybindings) {
                bool keybinding_handled =
                    handle_keybinding_for_symbol(item, syms[i], keyboard);
                if (!handled)
                    handled = keybinding_handled;
            }
        }
    }

    if (!handled) {
        /* Otherwise, we pass it along to the client. */
        wlr_seat_set_keyboard(seat, &keyboard.wlr);
        wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
                                     event->state);
    }
}

/* This event is raised when a modifier key, such as shift or alt, is
 * pressed. We simply communicate this to the client. */
static void keyboard_handle_modifiers(wl_listener* listener, void*)
{
    Keyboard& keyboard = naoland_container_of(listener, keyboard, modifiers);

    /*
     * A seat can only have one keyboard, but this is a limitation of the
     * Wayland protocol - not wlroots. We assign all connected keyboards to the
     * same seat. You can swap out the underlying wlr_keyboard like this and
     * wlr_seat handles this transparently.
     */
    wlr_seat_set_keyboard(keyboard.seat.wlr, &keyboard.wlr);
    /* Send modifiers to the client. */
    wlr_seat_keyboard_notify_modifiers(keyboard.seat.wlr,
                                       &keyboard.wlr.modifiers);
}

Keyboard::Keyboard(Seat& seat, wlr_keyboard& keyboard) noexcept
    : listeners(*this)
    , seat(seat)
    , wlr(keyboard)
{
    /* We need to prepare an XKB keymap and assign it to the keyboard. This
     * assumes the defaults (e.g. layout = "us"). */
    xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap* keymap = xkb_keymap_new_from_names(context, nullptr,
                                                   XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(&keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(&keyboard, 25, 600);

    /* Here we set up listeners for keyboard events. */
    listeners.modifiers.notify = keyboard_handle_modifiers;
    wl_signal_add(&keyboard.events.modifiers, &listeners.modifiers);
    listeners.key.notify = keyboard_handle_key;
    wl_signal_add(&keyboard.events.key, &listeners.key);
    listeners.destroy.notify = keyboard_handle_destroy;
    wl_signal_add(&keyboard.base.events.destroy, &listeners.destroy);

    wlr_seat_set_keyboard(seat.wlr, &keyboard);
}

Keyboard::~Keyboard() noexcept
{
    wl_list_remove(&listeners.modifiers.link);
    wl_list_remove(&listeners.key.link);
    wl_list_remove(&listeners.destroy.link);
}
