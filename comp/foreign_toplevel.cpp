#include "foreign_toplevel.hpp"

#include "output.hpp"
#include "server.hpp"
#include "surface/view.hpp"

static void
foreign_toplevel_handle_request_maximize_notify(wl_listener* listener,
                                                void* data)
{
    ForeignToplevelHandle const& handle
        = naoland_container_of(listener, handle, request_activate);
    auto const& event
        = *static_cast<wlr_foreign_toplevel_handle_v1_maximized_event*>(data);

    auto const placement
        = event.maximized ? VIEW_PLACEMENT_MAXIMIZED : VIEW_PLACEMENT_STACKING;
    handle.view.set_placement(placement);
}

static void
foreign_toplevel_handle_request_fullscreen_notify(wl_listener* listener,
                                                  void* data)
{
    ForeignToplevelHandle const& handle
        = naoland_container_of(listener, handle, request_activate);
    auto const& event
        = *static_cast<wlr_foreign_toplevel_handle_v1_maximized_event*>(data);

    auto const placement
        = event.maximized ? VIEW_PLACEMENT_FULLSCREEN : VIEW_PLACEMENT_STACKING;
    handle.view.set_placement(placement);
}

static void
foreign_toplevel_handle_request_minimize_notify(wl_listener* listener,
                                                void* data)
{
    ForeignToplevelHandle const& handle
        = naoland_container_of(listener, handle, request_activate);
    auto const& event
        = *static_cast<wlr_foreign_toplevel_handle_v1_minimized_event*>(data);

    handle.view.set_minimized(event.minimized);
}

static void
foreign_toplevel_handle_request_activate_notify(wl_listener* listener, void*)
{
    ForeignToplevelHandle const& handle
        = naoland_container_of(listener, handle, request_activate);

    handle.view.set_minimized(false);
    handle.view.get_server().focus_view(&handle.view);
}

static void foreign_toplevel_handle_request_close_notify(wl_listener* listener,
                                                         void*)
{
    ForeignToplevelHandle const& handle
        = naoland_container_of(listener, handle, request_close);

    handle.view.close_animation();
}

static void foreign_toplevel_handle_set_rectangle_notify(wl_listener* listener,
                                                         void* data)
{
    ForeignToplevelHandle const& handle
        = naoland_container_of(listener, handle, set_rectangle);
    auto const& event
        = *static_cast<wlr_foreign_toplevel_handle_v1_set_rectangle_event*>(
            data);

    handle.view.set_position(event.x, event.y);
    handle.view.set_size(event.width, event.height);
}

ForeignToplevelHandle::ForeignToplevelHandle(View& view) noexcept
    : listeners(*this)
    , view(view)
    , handle(*wlr_foreign_toplevel_handle_v1_create(
          view.get_server().foreign_toplevel_manager))
{
    handle.data = this;

    listeners.request_maximize.notify
        = foreign_toplevel_handle_request_maximize_notify;
    wl_signal_add(&handle.events.request_maximize, &listeners.request_maximize);
    listeners.request_minimize.notify
        = foreign_toplevel_handle_request_minimize_notify;
    wl_signal_add(&handle.events.request_minimize, &listeners.request_minimize);
    listeners.request_activate.notify
        = foreign_toplevel_handle_request_activate_notify;
    wl_signal_add(&handle.events.request_activate, &listeners.request_activate);
    listeners.request_fullscreen.notify
        = foreign_toplevel_handle_request_fullscreen_notify;
    wl_signal_add(&handle.events.request_fullscreen,
                  &listeners.request_fullscreen);
    listeners.request_close.notify
        = foreign_toplevel_handle_request_close_notify;
    wl_signal_add(&handle.events.request_close, &listeners.request_close);
    listeners.set_rectangle.notify
        = foreign_toplevel_handle_set_rectangle_notify;
    wl_signal_add(&handle.events.set_rectangle, &listeners.set_rectangle);
}

ForeignToplevelHandle::~ForeignToplevelHandle() noexcept
{
    wlr_foreign_toplevel_handle_v1_destroy(&handle);
    wl_list_remove(&listeners.request_maximize.link);
    wl_list_remove(&listeners.request_minimize.link);
    wl_list_remove(&listeners.request_activate.link);
    wl_list_remove(&listeners.request_fullscreen.link);
    wl_list_remove(&listeners.request_close.link);
    wl_list_remove(&listeners.set_rectangle.link);
}

void ForeignToplevelHandle::set_title(char const* title) const
{
    if (title != nullptr) {
        wlr_foreign_toplevel_handle_v1_set_title(&handle, title);
    }
}

void ForeignToplevelHandle::set_app_id(char const* app_id) const
{
    if (app_id != nullptr) {
        wlr_foreign_toplevel_handle_v1_set_app_id(&handle, app_id);
    }
}

void ForeignToplevelHandle::set_parent(
    std::optional<std::reference_wrapper<ForeignToplevelHandle const>> const
        parent) const
{
    wlr_foreign_toplevel_handle_v1_set_parent(
        &handle, parent.has_value() ? &parent->get().handle : nullptr);
}

void ForeignToplevelHandle::set_placement(ViewPlacement const placement) const
{
    set_maximized(placement == VIEW_PLACEMENT_MAXIMIZED);
    set_fullscreen(placement == VIEW_PLACEMENT_FULLSCREEN);
}

void ForeignToplevelHandle::set_maximized(bool const maximized) const
{
    wlr_foreign_toplevel_handle_v1_set_maximized(&handle, maximized);
}

void ForeignToplevelHandle::set_minimized(bool const minimized) const
{
    wlr_foreign_toplevel_handle_v1_set_minimized(&handle, minimized);
}

void ForeignToplevelHandle::set_activated(bool const activated) const
{
    wlr_foreign_toplevel_handle_v1_set_activated(&handle, activated);
}

void ForeignToplevelHandle::set_fullscreen(bool const fullscreen) const
{
    wlr_foreign_toplevel_handle_v1_set_fullscreen(&handle, fullscreen);
}

void ForeignToplevelHandle::output_enter(Output const& output) const
{
    wlr_foreign_toplevel_handle_v1_output_enter(&handle, &output.wlr);
}

void ForeignToplevelHandle::output_leave(Output const& output) const
{
    wlr_foreign_toplevel_handle_v1_output_leave(&handle, &output.wlr);
}
