#include "view.hpp"

#include <algorithm>
#include <utility>

#include "foreign_toplevel.hpp"
#include "input/seat.hpp"
#include "output.hpp"
#include "server.hpp"
#include "types.hpp"

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/edges.h>
#include "wlr-wrap-end.hpp"

View::View() noexcept
    : listeners(*this)
{
}

std::optional<std::reference_wrapper<Output>>
View::find_output_for_maximize() const
{
    Server const& server = get_server();

    if (server.outputs.empty()) {
        return {};
    }

    Cursor const& cursor = server.seat->cursor;
    Output* best_output = nullptr;
    int64_t best_area = 0;

    for (auto* output : server.outputs) {
        if (!wlr_output_layout_intersects(server.output_layout, &output->wlr,
                                          &previous)) {
            continue;
        }

        wlr_box output_box = {};
        wlr_output_layout_get_box(server.output_layout, &output->wlr,
                                  &output_box);
        wlr_box intersection = {};
        wlr_box_intersection(&intersection, &previous, &output_box);
        int64_t const intersection_area
            = intersection.width * intersection.height;

        if (intersection.width * intersection.height > best_area) {
            best_area = intersection_area;
            best_output = output;
        }
    }

    // if it's outside of all outputs, just use the pointer position
    if (best_output == nullptr) {
        for (auto* output : server.outputs) {
            auto const cx = static_cast<int32_t>(std::round(cursor.wlr.x));
            auto const cy = static_cast<int32_t>(std::round(cursor.wlr.y));
            if (wlr_output_layout_contains_point(server.output_layout,
                                                 &output->wlr, cx, cy)) {
                best_output = output;
                break;
            }
        }
    }

    // still nothing? use the first output in the list
    if (best_output == nullptr) {
        best_output = static_cast<Output*>(
            wlr_output_layout_get_center_output(server.output_layout)->data);
    }

    if (best_output == nullptr) {
        return {};
    }

    return std::ref(*best_output);
}

void View::begin_interactive(CursorMode const mode, uint32_t const edges)
{
    Server& server = get_server();

    Cursor& cursor = server.seat->cursor;
    wlr_surface* focused_surface
        = server.seat->wlr->pointer_state.focused_surface;

    if (get_wlr_surface() != wlr_surface_get_root_surface(focused_surface)) {
        /* Deny move/resize requests from unfocused clients. */
        return;
    }

    server.grabbed_view = this;
    cursor.mode = mode;

    if (mode == NAOLAND_CURSOR_MOVE) {
        server.grab_x = cursor.wlr.x - current.x;
        server.grab_y = cursor.wlr.y - current.y;
    } else {
        wlr_box const geo_box = get_geometry();

        double const border_x = current.x + geo_box.x
            + (edges & WLR_EDGE_RIGHT ? geo_box.width : 0);
        double const border_y = current.y + geo_box.y
            + (edges & WLR_EDGE_BOTTOM ? geo_box.height : 0);
        server.grab_x = cursor.wlr.x - border_x;
        server.grab_y = cursor.wlr.y - border_y;

        server.grab_geobox = geo_box;
        server.grab_geobox.x += current.x;
        server.grab_geobox.y += current.y;

        server.resize_edges = edges;
    }
}

void View::set_geometry(int32_t const x, int32_t const y, int32_t const width,
                        int32_t const height)
{
    wlr_box const min_size = get_min_size();
    wlr_box const max_size = get_max_size();
    int32_t const bounded_width
        = std::clamp(width, min_size.width, max_size.width);
    int32_t const bounded_height
        = std::clamp(height, min_size.height, max_size.height);

    if (curr_placement == VIEW_PLACEMENT_STACKING) {
        previous = current;
    }
    current = { x, std::max(y, 0), bounded_width, bounded_height };
    if (scene_tree != nullptr) {
        wlr_scene_node_set_position(&scene_tree->node, current.x, current.y);
    }
    impl_set_geometry(current.x, current.y, current.width, current.height);
}

void View::set_position(int32_t const x, int32_t const y)
{
    if (curr_placement == VIEW_PLACEMENT_STACKING) {
        previous.x = current.x;
        previous.y = current.y;
    }
    current.x = x;
    current.y = std::max(y, 0);
    if (scene_tree != nullptr) {
        wlr_scene_node_set_position(&scene_tree->node, current.x, current.y);
    }
    impl_set_position(current.x, current.y);
}

void View::set_size(int32_t const width, int32_t const height)
{
    wlr_box const min_size = get_min_size();
    wlr_box const max_size = get_max_size();
    int32_t const bounded_width
        = std::clamp(width, min_size.width, max_size.width);
    int32_t const bounded_height
        = std::clamp(height, min_size.height, max_size.height);

    if (curr_placement == VIEW_PLACEMENT_STACKING) {
        previous.width = current.width;
        previous.height = current.height;
    }
    current.width = bounded_width;
    current.height = bounded_height;
    impl_set_size(current.width, current.height);
}

void View::update_outputs(bool const ignore_previous) const
{
    for (auto& output : std::as_const(get_server().outputs)) {
        wlr_box output_area = output->full_area;
        wlr_box prev_intersect = {}, curr_intersect = {};
        wlr_box_intersection(&prev_intersect, &previous, &output_area);
        wlr_box_intersection(&curr_intersect, &current, &output_area);

        if (ignore_previous) {
            if (!wlr_box_empty(&curr_intersect)) {
                wlr_surface_send_enter(get_wlr_surface(), &output->wlr);
                toplevel_handle->output_enter(*output);
            }
        } else if (wlr_box_empty(&prev_intersect)
                   && !wlr_box_empty(&curr_intersect)) {
            wlr_surface_send_enter(get_wlr_surface(), &output->wlr);
            toplevel_handle->output_enter(*output);
        } else if (!wlr_box_empty(&prev_intersect)
                   && wlr_box_empty(&curr_intersect)) {
            wlr_surface_send_leave(get_wlr_surface(), &output->wlr);
            toplevel_handle->output_leave(*output);
        }
    }
}

void View::set_activated(bool const activated)
{
    impl_set_activated(activated);

    if (toplevel_handle.has_value()) {
        toplevel_handle->set_activated(activated);
    }

    is_active = activated;
}

void View::set_placement(ViewPlacement const new_placement, bool const force)
{
    Server const& server = get_server();

    if (!force) {
        if (curr_placement == new_placement) {
            return;
        }

        wlr_surface* focused_surface
            = server.seat->wlr->pointer_state.focused_surface;
        if (focused_surface == nullptr
            || get_wlr_surface()
                != wlr_surface_get_root_surface(focused_surface)) {
            /* Deny placement requests from unfocused clients. */
            return;
        }
    }

    bool res = true;

    switch (new_placement) {
    case VIEW_PLACEMENT_STACKING:
        stack();
        break;
    case VIEW_PLACEMENT_MAXIMIZED:
        res = maximize();
        break;
    case VIEW_PLACEMENT_FULLSCREEN:
        res = fullscreen();
        break;
    }

    if (res) {
        prev_placement = curr_placement;
        curr_placement = new_placement;
        if (toplevel_handle.has_value()) {
            toplevel_handle->set_placement(new_placement);
        }
    }
}

void View::stack()
{
    impl_set_maximized(false);
    impl_set_fullscreen(false);
    set_geometry(previous.x, previous.y, previous.width, previous.height);
    update_outputs();
}

bool View::maximize()
{
    auto const best_output = find_output_for_maximize();
    if (!best_output.has_value()) {
        return false;
    }

    wlr_box const output_box = best_output->get().usable_area;

    wlr_box const min_size = get_min_size();
    if (output_box.width < min_size.width
        || output_box.height < min_size.height) {
        return false;
    }

    wlr_box const max_size = get_max_size();
    if (output_box.width > max_size.width
        || output_box.height > max_size.height) {
        return false;
    }

    impl_set_fullscreen(false);
    impl_set_maximized(true);
    set_geometry(output_box.x, output_box.y, output_box.width,
                 output_box.height);
    update_outputs();

    return true;
}

bool View::fullscreen()
{
    auto const best_output = find_output_for_maximize();
    if (!best_output.has_value()) {
        return false;
    }

    wlr_box const output_box = best_output->get().full_area;

    wlr_box const min_size = get_min_size();
    if (output_box.width < min_size.width
        || output_box.height < min_size.height) {
        return false;
    }

    wlr_box const max_size = get_max_size();
    if (output_box.width > max_size.width
        || output_box.height > max_size.height) {
        return false;
    }

    impl_set_fullscreen(true);
    set_geometry(output_box.x, output_box.y, output_box.width,
                 output_box.height);
    update_outputs();

    return true;
}

void View::set_minimized(bool const minimized)
{
    if (minimized == is_minimized) {
        return;
    }

    if (toplevel_handle.has_value()) {
        toplevel_handle->set_minimized(minimized);
    }
    impl_set_minimized(minimized);
    this->is_minimized = minimized;

    if (minimized) {
        wlr_scene_node_set_enabled(&scene_tree->node, false);
        set_activated(false);
    } else {
        wlr_scene_node_set_enabled(&scene_tree->node, true);
    }
}

void View::toggle_maximize()
{
    if (curr_placement != VIEW_PLACEMENT_FULLSCREEN) {
        set_placement(curr_placement != VIEW_PLACEMENT_MAXIMIZED
                          ? VIEW_PLACEMENT_MAXIMIZED
                          : VIEW_PLACEMENT_STACKING);
    }
}

void View::toggle_fullscreen()
{
    if (curr_placement == VIEW_PLACEMENT_FULLSCREEN) {
        set_placement(prev_placement);
    } else {
        set_placement(VIEW_PLACEMENT_FULLSCREEN);
    }
}

static void xdg_toplevel_request_decoration_mode_notify(wl_listener* listener, void*)
{
    View& view = naoland_container_of(listener, view, request_decoration_mode);
    wlr_xdg_toplevel_decoration_v1_set_mode(view.xdg_toplevel_decoration,
                                            WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void xdg_toplevel_destroy_decoration_notify(wl_listener* listener, void*)
{
    View& view = naoland_container_of(listener, view, destroy_decoration);
    view.destroy_decorations();
}

void View::setup_decorations(wlr_xdg_toplevel_decoration_v1 *decoration)
{
    xdg_toplevel_decoration = decoration;

    listeners.request_decoration_mode.notify = xdg_toplevel_request_decoration_mode_notify;
    wl_signal_add(&decoration->events.request_mode, &listeners.request_decoration_mode);

    listeners.destroy_decoration.notify = xdg_toplevel_destroy_decoration_notify;
    wl_signal_add(&decoration->events.destroy, &listeners.destroy_decoration);

    xdg_toplevel_request_decoration_mode_notify(&listeners.request_decoration_mode, decoration);
}

void View::destroy_decorations()
{
    wl_list_remove(&listeners.destroy_decoration.link);
    wl_list_remove(&listeners.request_decoration_mode.link);
}
