#include "tablet.hpp"

#include "types.hpp"
#include "server.hpp"

#include "wlr-wrap-start.hpp"
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/util/log.h>
#include "wlr-wrap-end.hpp"

#include <linux/input-event-codes.h>

static void tablet_destroy_notify(wl_listener* listener, void*)
{
    DrawingTablet& tablet = naoland_container_of(listener, tablet, destroy);

    delete &tablet;
}

static void tablet_button_notify(wl_listener*, void*)
{
    wlr_log(WLR_ERROR, "TODO: tablet_button_notify\n");

    // DrawingTablet& tablet = naoland_container_of(listener, tablet, button);
    // auto* ev = static_cast<wlr_tablet_tool_button_event*>(data);

    // uint32_t button = tablet_get_mapped_button(ev->button);
    // if (!button) {
    //     return;
    // }

    // cursor_emulate_button(tablet->seat, button, ev->state, ev->time_msec);
}

static void tablet_tip_notify(wl_listener* listener, void* data)
{
    DrawingTablet& tablet = naoland_container_of(listener, tablet, tip);
    auto* ev = static_cast<wlr_tablet_tool_tip_event*>(data);

    tablet.seat.cursor.emulate_button(
        tablet.seat.server.config.tablet.press_action,
        ev->state == WLR_TABLET_TOOL_TIP_DOWN ? WLR_BUTTON_PRESSED
                                              : WLR_BUTTON_RELEASED,
        ev->time_msec);
}

static void tablet_axis_notify(wl_listener* listener, void* data)
{
    DrawingTablet& tablet = naoland_container_of(listener, tablet, axis);
    auto* ev = static_cast<wlr_tablet_tool_axis_event*>(data);

    if (ev->updated_axes & (WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y)) {
        if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_X) {
            tablet.x = ev->x;
        }
        if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_Y) {
            tablet.y = ev->y;
        }

        tablet.seat.cursor.emulate_move_absolute(&ev->tablet->base,
                                                 tablet.x, tablet.y,
                                                 ev->time_msec);
    }
}

DrawingTablet::DrawingTablet(Seat& seat, wlr_input_device* input_device) noexcept
    : listeners(*this)
    , seat(seat)
{
    wlr = wlr_tablet_from_input_device(input_device);
    wlr->data = this;
    x = y = 0;

    listeners.axis.notify = tablet_axis_notify;
    wl_signal_add(&wlr->events.axis, &listeners.axis);

    listeners.tip.notify = tablet_tip_notify;
    wl_signal_add(&wlr->events.tip, &listeners.tip);

    listeners.button.notify = tablet_button_notify;
    wl_signal_add(&wlr->events.button, &listeners.button);

    listeners.destroy.notify = tablet_destroy_notify;
    wl_signal_add(&input_device->events.destroy, &listeners.destroy);
}

DrawingTablet::~DrawingTablet() noexcept
{
    wl_list_remove(&listeners.axis.link);
    wl_list_remove(&listeners.tip.link);
    wl_list_remove(&listeners.button.link);
    wl_list_remove(&listeners.destroy.link);
}
