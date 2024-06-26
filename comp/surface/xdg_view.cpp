#include "types.hpp"
#include "view.hpp"

#include "foreign_toplevel.hpp"
#include "input/seat.hpp"
#include "output.hpp"
#include "server.hpp"
#include "surface.hpp"

#include "wlr-wrap-start.hpp"
#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/util/log.h>
#include "wlr-wrap-end.hpp"

/* Called when the surface is mapped, or ready to display on-screen. */
static void xdg_toplevel_map_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, map);

    view.map();
}

/* Called when the surface is unmapped, and should no longer be shown. */
static void xdg_toplevel_unmap_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, unmap);

    view.unmap();
}

/* Called when the surface is destroyed and should never be shown again. */
static void xdg_toplevel_destroy_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, destroy);

    view.server.views.remove(&view);
    delete &view;
}

/* This event is raised when a client would like to begin an interactive
 * move, typically because the user clicked on their client-side
 * decorations. Note that a more sophisticated compositor should check the
 * provided serial against a list of button press serials sent to this
 * client, to prevent the client from requesting this whenever they want. */
static void xdg_toplevel_request_move_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, request_move);

    view.set_placement(VIEW_PLACEMENT_STACKING);
    view.begin_interactive(NAOLAND_CURSOR_MOVE, 0);
}

/* This event is raised when a client would like to begin an interactive
 * resize, typically because the user clicked on their client-side
 * decorations. Note that a more sophisticated compositor should check the
 * provided serial against a list of button press serials sent to this
 * client, to prevent the client from requesting this whenever they want. */
static void xdg_toplevel_request_resize_notify(wl_listener* listener,
                                               void* data)
{
    XdgView& view = naoland_container_of(listener, view, request_resize);
    auto const* event = static_cast<wlr_xdg_toplevel_resize_event*>(data);

    view.set_placement(VIEW_PLACEMENT_STACKING);
    view.begin_interactive(NAOLAND_CURSOR_RESIZE, event->edges);
}

/* This event is raised when a client would like to maximize itself,
 * typically because the user clicked on the maximize button on
 * client-side decorations. */
static void xdg_toplevel_request_maximize_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, request_maximize);

    view.toggle_maximize();
    wlr_xdg_surface_schedule_configure(view.xdg_toplevel.base);
}

static void xdg_toplevel_request_fullscreen_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, request_fullscreen);

    view.toggle_fullscreen();
    wlr_xdg_surface_schedule_configure(view.xdg_toplevel.base);
}

static void xdg_toplevel_request_minimize_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, request_minimize);

    view.set_minimized(!view.is_minimized);
    wlr_xdg_surface_schedule_configure(view.xdg_toplevel.base);
}

static void xdg_toplevel_set_title_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, set_title);

    view.toplevel_handle->set_title(view.xdg_toplevel.title);
}

static void xdg_toplevel_set_app_id_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, set_app_id);

    view.toplevel_handle->set_app_id(view.xdg_toplevel.app_id);
}

static void xdg_toplevel_set_parent_notify(wl_listener* listener, void*)
{
    XdgView& view = naoland_container_of(listener, view, set_parent);

    if (view.xdg_toplevel.parent != nullptr) {
        auto const* m_view = dynamic_cast<View*>(
            static_cast<Surface*>(view.xdg_toplevel.parent->base->data));
        if (m_view != nullptr) {
            view.toplevel_handle->set_parent(m_view->toplevel_handle);
            return;
        }
    }

    view.toplevel_handle->set_parent({});
}

XdgView::XdgView(Server& server, wlr_xdg_toplevel& wlr) noexcept
    : listeners(*this)
    , server(server)
    , xdg_toplevel(wlr)
{
    scene_tree
        = wlr_scene_xdg_surface_create(server.scene_layers[NAOLAND_SCENE_LAYER_NORMAL], wlr.base);

    wlr_xdg_toplevel_set_wm_capabilities(
        &wlr,
        WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE
            | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE
            | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);

    scene_tree->node.data = this;
    /* When rendering the scene tree, this `data` field
     * is expected to be pointing to a valid View in order
     * to draw window borders.
     */
    wlr.base->surface->data = this;

    toplevel_handle.emplace(*this);
    toplevel_handle->set_title(xdg_toplevel.title);
    toplevel_handle->set_app_id(xdg_toplevel.app_id);

    if (xdg_toplevel.parent != nullptr) {
        auto* m_view = dynamic_cast<View*>(
            static_cast<Surface*>(xdg_toplevel.parent->base->data));
        if (m_view != nullptr) {
            toplevel_handle->set_parent(m_view->toplevel_handle.value());
        }
    }

    listeners.map.notify = xdg_toplevel_map_notify;
    wl_signal_add(&wlr.base->surface->events.map, &listeners.map);
    listeners.unmap.notify = xdg_toplevel_unmap_notify;
    wl_signal_add(&wlr.base->surface->events.unmap, &listeners.unmap);
    listeners.destroy.notify = xdg_toplevel_destroy_notify;
    wl_signal_add(&wlr.base->events.destroy, &listeners.destroy);
    listeners.request_move.notify = xdg_toplevel_request_move_notify;
    wl_signal_add(&xdg_toplevel.events.request_move, &listeners.request_move);
    listeners.request_resize.notify = xdg_toplevel_request_resize_notify;
    wl_signal_add(&xdg_toplevel.events.request_resize,
                  &listeners.request_resize);
    listeners.request_maximize.notify = xdg_toplevel_request_maximize_notify;
    wl_signal_add(&xdg_toplevel.events.request_maximize,
                  &listeners.request_maximize);
    listeners.request_minimize.notify = xdg_toplevel_request_minimize_notify;
    wl_signal_add(&xdg_toplevel.events.request_minimize,
                  &listeners.request_minimize);
    listeners.request_fullscreen.notify
        = xdg_toplevel_request_fullscreen_notify;
    wl_signal_add(&xdg_toplevel.events.request_fullscreen,
                  &listeners.request_fullscreen);
    listeners.set_title.notify = xdg_toplevel_set_title_notify;
    wl_signal_add(&xdg_toplevel.events.set_title, &listeners.set_title);
    listeners.set_app_id.notify = xdg_toplevel_set_app_id_notify;
    wl_signal_add(&xdg_toplevel.events.set_app_id, &listeners.set_app_id);
    listeners.set_parent.notify = xdg_toplevel_set_parent_notify;
    wl_signal_add(&xdg_toplevel.events.set_parent, &listeners.set_parent);

    server.views.push_back(this);
}

XdgView::~XdgView() noexcept
{
    wl_list_remove(&listeners.map.link);
    wl_list_remove(&listeners.unmap.link);
    wl_list_remove(&listeners.destroy.link);
    wl_list_remove(&listeners.request_move.link);
    wl_list_remove(&listeners.request_resize.link);
    wl_list_remove(&listeners.request_maximize.link);
    wl_list_remove(&listeners.request_minimize.link);
    wl_list_remove(&listeners.set_title.link);
    wl_list_remove(&listeners.set_app_id.link);
    wl_list_remove(&listeners.set_parent.link);
}

constexpr wlr_surface* XdgView::get_wlr_surface() const
{
    return xdg_toplevel.base->surface;
}

constexpr Server& XdgView::get_server() const { return server; }

bool XdgView::is_x11() const
{
    return false;
}

wlr_box XdgView::get_geometry() const
{
    wlr_box box = {};
    wlr_xdg_surface_get_geometry(xdg_toplevel.base, &box);
    return box;
}

constexpr wlr_box XdgView::get_min_size() const
{
    return { 0, 0, xdg_toplevel.current.min_width,
             xdg_toplevel.current.min_height };
}

constexpr wlr_box XdgView::get_max_size() const
{
    int32_t const max_width = xdg_toplevel.current.max_width > 0
        ? xdg_toplevel.current.max_width
        : INT32_MAX;
    int32_t const max_height = xdg_toplevel.current.max_height > 0
        ? xdg_toplevel.current.max_height
        : INT32_MAX;
    return { 0, 0, max_width, max_height };
}

void XdgView::unmap()
{
    wlr_scene_node_set_enabled(&scene_tree->node, false);

    /* Reset the cursor mode if the grabbed view was unmapped. */
    if (this == server.grabbed_view) {
        server.seat->cursor.reset_mode();
    }

    if (this == server.focused_view) {
        server.focused_view = nullptr;
    }
}

void XdgView::close() { wlr_xdg_toplevel_send_close(&xdg_toplevel); }

void XdgView::impl_map()
{
    if (pending_map) {
        wlr_xdg_surface_get_geometry(xdg_toplevel.base, &previous);
        wlr_xdg_surface_get_geometry(xdg_toplevel.base, &current);

        if (!server.outputs.empty()) {
            auto const output = static_cast<Output*>(
                wlr_output_layout_get_center_output(server.output_layout)
                    ->data);
            auto const usable_area = output->usable_area;
            auto const center_x = usable_area.x + usable_area.width / 2;
            auto const center_y = usable_area.y + usable_area.height / 2;
            set_position(center_x - current.width / 2,
                         center_y - current.height / 2);
        }

        pending_map = false;
    }

    wlr_scene_node_set_enabled(&scene_tree->node, true);
    if (xdg_toplevel.current.fullscreen) {
        set_placement(VIEW_PLACEMENT_FULLSCREEN);
    } else if (xdg_toplevel.current.maximized) {
        set_placement(VIEW_PLACEMENT_MAXIMIZED);
    }

    update_outputs(true);

    server.focus_view(this);
}

void XdgView::impl_set_position(int32_t const x, int32_t const y)
{
    (void)x;
    (void)y;
}

void XdgView::impl_set_size(int32_t const width, int32_t const height)
{
    wlr_xdg_toplevel_set_size(&xdg_toplevel, width, height);
}

void XdgView::impl_set_geometry(int const x, int const y, int const width,
                                int const height)
{
    (void)x;
    (void)y;
    wlr_xdg_toplevel_set_size(&xdg_toplevel, width, height);
}

void XdgView::impl_set_activated(bool const activated)
{
    wlr_xdg_toplevel_set_activated(&xdg_toplevel, activated);
}

void XdgView::impl_set_fullscreen(bool const fullscreen)
{
    wlr_xdg_toplevel_set_fullscreen(&xdg_toplevel, fullscreen);
}

void XdgView::impl_set_maximized(bool const maximized)
{
    wlr_xdg_toplevel_set_maximized(&xdg_toplevel, maximized);
}

void XdgView::impl_set_minimized(bool const minimized) { (void)minimized; }
