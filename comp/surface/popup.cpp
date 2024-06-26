#include "popup.hpp"

#include "output.hpp"
#include "rendering/animation.hpp"
#include "server.hpp"
#include "surface.hpp"
#include "types.hpp"

#include <utility>

static void popup_map_notify(wl_listener* listener, void*)
{
    Popup& popup = naoland_container_of(listener, popup, map);
    popup.animation.start(AnimationOptions {
            .kind = ANIMATION_FADE_IN,
            .role = ANIMATION_FADE,
            .ignore_play_percentage = true,
        });

    wlr_box current = {};
    wlr_xdg_surface_get_geometry(popup.wlr.base, &current);

    for (auto& output : std::as_const(popup.server.outputs)) {
        wlr_box output_area = output->full_area;
        wlr_box intersect = {};
        wlr_box_intersection(&intersect, &current, &output_area);

        if (!wlr_box_empty(&current)) {
            wlr_surface_send_enter(popup.wlr.base->surface, &output->wlr);
        }
    }
}

static void popup_destroy_notify(wl_listener* listener, void*)
{
    Popup& popup = naoland_container_of(listener, popup, destroy);

    delete &popup;
}

static void popup_new_popup_notify(wl_listener* listener, void* data)
{
    Popup const& popup = naoland_container_of(listener, popup, new_popup);

    new Popup(popup, *static_cast<wlr_xdg_popup*>(data));
}

Popup::Popup(Surface const& parent, wlr_xdg_popup& wlr) noexcept
    : listeners(*this)
    , server(parent.get_server())
    , parent(parent)
    , wlr(wlr)
    , animation(*this)
{
    this->wlr = wlr;
    scene_tree
        = wlr_scene_xdg_surface_create(parent.scene_tree, wlr.base);

    scene_tree->node.data = this;
    wlr.base->surface->data = this;

    listeners.map.notify = popup_map_notify;
    wl_signal_add(&wlr.base->surface->events.map, &listeners.map);
    listeners.destroy.notify = popup_destroy_notify;
    wl_signal_add(&wlr.base->events.destroy, &listeners.destroy);
    listeners.new_popup.notify = popup_new_popup_notify;
    wl_signal_add(&wlr.base->events.new_popup, &listeners.new_popup);
}

Popup::~Popup() noexcept
{
    wl_list_remove(&listeners.map.link);
    wl_list_remove(&listeners.destroy.link);
    wl_list_remove(&listeners.new_popup.link);
}

constexpr wlr_surface* Popup::get_wlr_surface() const
{
    return wlr.base->surface;
}

constexpr Server& Popup::get_server() const { return server; }

constexpr bool Popup::is_view() const { return false; }
constexpr bool Popup::is_popup() const { return true; }
