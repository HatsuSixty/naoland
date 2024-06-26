#include "view.hpp"

#include "foreign_toplevel.hpp"
#include "input/seat.hpp"
#include "output.hpp"
#include "server.hpp"
#include "surface.hpp"
#include "types.hpp"

#include <cstdlib>
#include <wayland-server-core.h>

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_seat.h>
#include "wlr-wrap-end.hpp"

/* Called when the surface is mapped, or ready to display on-screen. */
static void xwayland_surface_map_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, map);

    view.map();
}

/* Called when the surface is unmapped, and should no longer be shown. */
static void xwayland_surface_unmap_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, unmap);

    view.unmap();
}

static void xwayland_surface_associate_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, associate);

    view.listeners.map.notify = xwayland_surface_map_notify;
    wl_signal_add(&view.xwayland_surface.surface->events.map,
                  &view.listeners.map);
    view.listeners.unmap.notify = xwayland_surface_unmap_notify;
    wl_signal_add(&view.xwayland_surface.surface->events.unmap,
                  &view.listeners.unmap);
}

static void xwayland_surface_dissociate_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, dissociate);

    wl_list_remove(&view.listeners.map.link);
    wl_list_remove(&view.listeners.unmap.link);
}

/* Called when the surface is destroyed and should never be shown again. */
static void xwayland_surface_destroy_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, destroy);

    view.server.views.remove(&view);
    delete &view;
}

static void xwayland_surface_request_configure_notify(wl_listener* listener,
                                                      void* data)
{
    XWaylandView& view = naoland_container_of(listener, view, request_configure);
    auto const& event
        = *static_cast<wlr_xwayland_surface_configure_event*>(data);

    view.set_geometry(event.x, event.y, event.width, event.height);
}

static void xwayland_surface_set_geometry_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, set_geometry);

    if (view.server.grabbed_view != &view) {
        wlr_xwayland_surface const& surface = view.xwayland_surface;
        if (view.curr_placement == VIEW_PLACEMENT_STACKING) {
            view.previous = view.current;
        }
        view.current = { surface.x, surface.y, surface.width, surface.height };
    }
}

/* This event is raised when a client would like to begin an interactive
 * move, typically because the user clicked on their client-side
 * decorations. Note that a more sophisticated compositor should check the
 * provided serial against a list of button press serials sent to this
 * client, to prevent the client from requesting this whenever they want. */
static void xwayland_surface_request_move_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, request_move);

    view.set_placement(VIEW_PLACEMENT_STACKING);
    view.begin_interactive(NAOLAND_CURSOR_MOVE, 0);
}

/* This event is raised when a client would like to begin an interactive
 * resize, typically because the user clicked on their client-side
 * decorations. Note that a more sophisticated compositor should check the
 * provided serial against a list of button press serials sent to this
 * client, to prevent the client from requesting this whenever they want. */
static void xwayland_surface_request_resize_notify(wl_listener* listener,
                                                   void* data)
{
    XWaylandView& view = naoland_container_of(listener, view, request_resize);
    auto const* event = static_cast<wlr_xwayland_resize_event*>(data);

    view.set_placement(VIEW_PLACEMENT_STACKING);
    view.begin_interactive(NAOLAND_CURSOR_RESIZE, event->edges);
}

static void xwayland_surface_request_maximize_notify(wl_listener* listener,
                                                     void*)
{
    XWaylandView& view = naoland_container_of(listener, view, request_maximize);

    view.toggle_maximize();
}

static void xwayland_surface_request_fullscreen_notify(wl_listener* listener,
                                                       void*)
{
    XWaylandView& view
        = naoland_container_of(listener, view, request_fullscreen);

    view.toggle_fullscreen();
}

static void xwayland_surface_set_title_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, set_title);

    if (view.toplevel_handle.has_value()) {
        view.toplevel_handle->set_title(view.xwayland_surface.title);
    }
}

static void xwayland_surface_set_class_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, set_class);

    if (view.toplevel_handle.has_value()) {
        view.toplevel_handle->set_app_id(view.xwayland_surface._class);
    }
}

static void xwayland_surface_set_parent_notify(wl_listener* listener, void*)
{
    XWaylandView& view = naoland_container_of(listener, view, set_parent);

    if (view.xwayland_surface.parent != nullptr) {
        auto* m_view = dynamic_cast<View*>(
            static_cast<Surface*>(view.xwayland_surface.parent->data));
        if (m_view != nullptr && view.scene_tree != nullptr) {
            wlr_scene_node_reparent(&view.scene_tree->node,
                                    m_view->scene_tree);
            if (view.toplevel_handle.has_value()
                && m_view->toplevel_handle.has_value()) {
                view.toplevel_handle->set_parent(
                    m_view->toplevel_handle.value());
            }
            return;
        }
    }

    if (view.toplevel_handle.has_value()) {
        view.toplevel_handle->set_parent({});
    }
}

XWaylandView::XWaylandView(Server& server,
                           wlr_xwayland_surface& surface) noexcept
    : listeners(*this)
    , server(server)
    , xwayland_surface(surface)
{
    listeners.associate.notify = xwayland_surface_associate_notify;
    wl_signal_add(&surface.events.associate, &listeners.associate);
    listeners.dissociate.notify = xwayland_surface_dissociate_notify;
    wl_signal_add(&surface.events.dissociate, &listeners.dissociate);
    listeners.destroy.notify = xwayland_surface_destroy_notify;
    wl_signal_add(&surface.events.destroy, &listeners.destroy);
    listeners.request_configure.notify
        = xwayland_surface_request_configure_notify;
    wl_signal_add(&surface.events.request_configure,
                  &listeners.request_configure);
    listeners.request_move.notify = xwayland_surface_request_move_notify;
    wl_signal_add(&surface.events.request_move, &listeners.request_move);
    listeners.request_resize.notify = xwayland_surface_request_resize_notify;
    wl_signal_add(&surface.events.request_resize, &listeners.request_resize);
    listeners.request_maximize.notify
        = xwayland_surface_request_maximize_notify;
    wl_signal_add(&surface.events.request_maximize,
                  &listeners.request_maximize);
    listeners.request_fullscreen.notify
        = xwayland_surface_request_fullscreen_notify;
    wl_signal_add(&surface.events.request_fullscreen,
                  &listeners.request_fullscreen);
    listeners.set_geometry.notify = xwayland_surface_set_geometry_notify;
    wl_signal_add(&surface.events.set_geometry, &listeners.set_geometry);
    listeners.set_title.notify = xwayland_surface_set_title_notify;
    wl_signal_add(&surface.events.set_title, &listeners.set_title);
    listeners.set_class.notify = xwayland_surface_set_class_notify;
    wl_signal_add(&surface.events.set_class, &listeners.set_class);
    listeners.set_parent.notify = xwayland_surface_set_parent_notify;
    wl_signal_add(&surface.events.set_parent, &listeners.set_parent);
}

XWaylandView::~XWaylandView() noexcept
{
    wl_list_remove(&listeners.associate.link);
    wl_list_remove(&listeners.destroy.link);
    wl_list_remove(&listeners.request_configure.link);
    wl_list_remove(&listeners.request_move.link);
    wl_list_remove(&listeners.request_resize.link);
    wl_list_remove(&listeners.set_geometry.link);
    wl_list_remove(&listeners.set_title.link);
    wl_list_remove(&listeners.set_class.link);
    wl_list_remove(&listeners.set_parent.link);
}

constexpr wlr_surface* XWaylandView::get_wlr_surface() const
{
    return xwayland_surface.surface;
}

constexpr Server& XWaylandView::get_server() const { return server; }

bool XWaylandView::is_x11() const
{
    return true;
}

constexpr wlr_box XWaylandView::get_geometry() const
{
    return { xwayland_surface.x, xwayland_surface.y, xwayland_surface.width,
             xwayland_surface.height };
}

constexpr wlr_box XWaylandView::get_min_size() const
{
    wlr_box min = { 0, 0, 0, 0 };
    if (xwayland_surface.size_hints != nullptr) {
        auto const& hints = *xwayland_surface.size_hints;
        min.width = std::max(hints.min_width, hints.base_width);
        min.height = std::max(hints.min_height, hints.base_height);
    }
    return min;
}

constexpr wlr_box XWaylandView::get_max_size() const
{
    wlr_box max = { 0, 0, UINT16_MAX, UINT16_MAX };
    if (xwayland_surface.size_hints != nullptr) {
        auto const& hints = *xwayland_surface.size_hints;
        max.width = hints.max_width > 0 ? hints.max_width : UINT16_MAX;
        max.height = hints.max_height > 0 ? hints.max_height : UINT16_MAX;
    }
    return max;
}

void XWaylandView::unmap()
{
    wlr_scene_node_set_enabled(&scene_tree->node, false);
    wlr_scene_node_destroy(&scene_tree->node);
    scene_tree = nullptr;
    Cursor& cursor = server.seat->cursor;

    /* Reset the cursor mode if the grabbed view was unmapped. */
    if (this == server.grabbed_view) {
        cursor.reset_mode();
    }

    if (this == server.focused_view) {
        server.focused_view = nullptr;
    }

    if (server.seat->wlr->keyboard_state.focused_surface
        == xwayland_surface.surface) {
        server.seat->wlr->keyboard_state.focused_surface = nullptr;
    }

    server.views.remove(this);

    toplevel_handle.reset();
}

void XWaylandView::close() { wlr_xwayland_surface_close(&xwayland_surface); }

static constexpr int16_t trunc(int32_t const int32)
{
    if (int32 > INT16_MAX) {
        return INT16_MAX;
    }

    if (int32 < INT16_MIN) {
        return INT16_MIN;
    }

    return static_cast<int16_t>(int32);
}

void XWaylandView::impl_map()
{
    xwayland_surface.data = this;
    /* When rendering the scene tree, this `data` field
     * is expected to be pointing to a valid View in order
     * to draw window borders.
     */
    xwayland_surface.surface->data = this;

    toplevel_handle.emplace(*this);
    toplevel_handle->set_title(xwayland_surface.title);
    toplevel_handle->set_app_id(xwayland_surface._class);

    scene_tree = wlr_scene_subsurface_tree_create(
        server.scene_layers[NAOLAND_SCENE_LAYER_NORMAL], xwayland_surface.surface);
    scene_tree->node.data = this;

    if (xwayland_surface.parent != nullptr) {
        auto const* m_view = dynamic_cast<View*>(
            static_cast<Surface*>(xwayland_surface.parent->data));
        if (m_view != nullptr) {
            wlr_scene_node_reparent(&scene_tree->node, m_view->scene_tree);
            toplevel_handle->set_parent(m_view->toplevel_handle);
        }
    }

    wlr_scene_node_set_enabled(&scene_tree->node, true);
    wlr_scene_node_set_position(&scene_tree->node, current.x, current.y);

    if (xwayland_surface.fullscreen) {
        set_placement(VIEW_PLACEMENT_FULLSCREEN);
    } else if (xwayland_surface.maximized_horz
               && xwayland_surface.maximized_vert) {
        set_placement(VIEW_PLACEMENT_MAXIMIZED);
    }

    server.views.insert(server.views.begin(), this);
    update_outputs(true);
    server.focus_view(this);
}

void XWaylandView::impl_set_position(int32_t const x, int32_t const y)
{
    wlr_xwayland_surface_configure(&xwayland_surface, trunc(x), trunc(y),
                                   current.width, current.height);
}

void XWaylandView::impl_set_size(int32_t const width, int32_t const height)
{
    wlr_xwayland_surface_configure(&xwayland_surface, trunc(current.x),
                                   trunc(current.y), width, height);
}

void XWaylandView::impl_set_geometry(int32_t const x, int32_t const y,
                                     int32_t const width, int32_t const height)
{
    wlr_xwayland_surface_configure(&xwayland_surface, trunc(x), trunc(y),
                                   trunc(width), trunc(height));
}

void XWaylandView::impl_set_activated(bool const activated)
{
    wlr_xwayland_surface_activate(&xwayland_surface, activated);
    if (activated) {
        wlr_xwayland_surface_restack(&xwayland_surface, nullptr,
                                     XCB_STACK_MODE_ABOVE);
    }
}

void XWaylandView::impl_set_fullscreen(bool const fullscreen)
{
    wlr_xwayland_surface_set_fullscreen(&xwayland_surface, fullscreen);
}

void XWaylandView::impl_set_maximized(bool const maximized)
{
    wlr_xwayland_surface_set_maximized(&xwayland_surface, maximized);
}

void XWaylandView::impl_set_minimized(bool const minimized)
{
    wlr_xwayland_surface_set_minimized(&xwayland_surface, minimized);
}
