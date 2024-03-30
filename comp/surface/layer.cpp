#include "layer.hpp"

#include "output.hpp"
#include "popup.hpp"
#include "server.hpp"
#include "surface.hpp"
#include "types.hpp"

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include "wlr-wrap-end.hpp"

static NaolandSceneLayer
naoland_layer_from_wlr_layer(zwlr_layer_shell_v1_layer const layer)
{
    switch (layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        return NAOLAND_SCENE_LAYER_BACKGROUND;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        return NAOLAND_SCENE_LAYER_BOTTOM;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        return NAOLAND_SCENE_LAYER_TOP;
    default:
        return NAOLAND_SCENE_LAYER_OVERLAY;
    }
}

static void subsurface_map_notify(wl_listener* listener, void*)
{
    LayerSubsurface& subsurface
        = naoland_container_of(listener, subsurface, map);

    wlr_surface_send_enter(subsurface.subsurface.surface,
                           &subsurface.parent.output.wlr);
}

static void subsurface_destroy_notify(wl_listener* listener, void*)
{
    LayerSubsurface& subsurface
        = naoland_container_of(listener, subsurface, destroy);

    subsurface.parent.subsurfaces.erase(&subsurface);
    delete &subsurface;
}

LayerSubsurface::LayerSubsurface(Layer& parent,
                                 wlr_subsurface& subsurface) noexcept
    : listeners(*this)
    , parent(parent)
    , subsurface(subsurface)
{
    listeners.map.notify = subsurface_map_notify;
    wl_signal_add(&subsurface.surface->events.map, &listeners.map);
    listeners.destroy.notify = subsurface_destroy_notify;
    wl_signal_add(&subsurface.events.destroy, &listeners.destroy);
}

LayerSubsurface::~LayerSubsurface() noexcept
{
    wl_list_remove(&listeners.map.link);
    wl_list_remove(&listeners.destroy.link);
}

/* Called when the surface is mapped, or ready to display on-screen. */
static void wlr_layer_surface_v1_map_notify(wl_listener* listener, void*)
{
    Layer& layer = naoland_container_of(listener, layer, map);

    wlr_scene_node_set_enabled(&layer.scene_tree->node, true);
    wlr_surface_send_enter(layer.get_wlr_surface(), &layer.output.wlr);
}

/* Called when the surface is unmapped, and should no longer be shown. */
static void wlr_layer_surface_v1_unmap_notify(wl_listener* listener, void*)
{
    Layer& layer = naoland_container_of(listener, layer, unmap);

    wlr_scene_node_set_enabled(&layer.scene_tree->node, false);
}

/* Called when the surface is destroyed and should never be shown again. */
static void wlr_layer_surface_v1_destroy_notify(wl_listener* listener, void*)
{
    Layer& layer = naoland_container_of(listener, layer, destroy);

    layer.output.layers.erase(&layer);
    delete &layer;
}

static void wlr_layer_surface_v1_commit_notify(wl_listener* listener, void*)
{
    Layer& layer = naoland_container_of(listener, layer, commit);

    Server const& server = layer.output.server;
    wlr_layer_surface_v1 const& surface = layer.layer_surface;

    uint32_t const committed = surface.current.committed;
    if (committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
        NaolandSceneLayer const chosen_layer
            = naoland_layer_from_wlr_layer(surface.current.layer);
        wlr_scene_node_reparent(&layer.scene_tree->node,
                                server.scene_layers[chosen_layer]);
    }

    if (committed) {
        layer.output.update_layout();
    }
}

static void wlr_layer_surface_v1_new_popup_notify(wl_listener* listener,
                                                  void* data)
{
    Layer& layer = naoland_container_of(listener, layer, new_popup);
    auto const* surface
        = static_cast<Surface*>(layer.layer_surface.surface->data);

    new Popup(*surface, *static_cast<wlr_xdg_popup*>(data));
}

static void wlr_layer_surface_v1_new_subsurface_notify(wl_listener* listener,
                                                       void* data)
{
    Layer& layer = naoland_container_of(listener, layer, new_subsurface);
    auto& subsurface = *static_cast<wlr_subsurface*>(data);

    layer.subsurfaces.emplace(new LayerSubsurface(layer, subsurface));
}

Layer::Layer(Output& output, wlr_layer_surface_v1& surface) noexcept
    : listeners(*this)
    , server(output.server)
    , output(output)
    , layer_surface(surface)
{
    NaolandSceneLayer const chosen_layer
        = naoland_layer_from_wlr_layer(surface.current.layer);
    scene_layer_surface = wlr_scene_layer_surface_v1_create(
        output.server.scene_layers[chosen_layer], &surface);
    scene_tree = scene_layer_surface->tree;

    scene_tree->node.data = this;
    surface.surface->data = this;

    listeners.map.notify = wlr_layer_surface_v1_map_notify;
    wl_signal_add(&surface.surface->events.map, &listeners.map);
    listeners.unmap.notify = wlr_layer_surface_v1_unmap_notify;
    wl_signal_add(&surface.surface->events.unmap, &listeners.unmap);
    listeners.destroy.notify = wlr_layer_surface_v1_destroy_notify;
    wl_signal_add(&surface.events.destroy, &listeners.destroy);
    listeners.commit.notify = wlr_layer_surface_v1_commit_notify;
    wl_signal_add(&surface.surface->events.commit, &listeners.commit);
    listeners.new_popup.notify = wlr_layer_surface_v1_new_popup_notify;
    wl_signal_add(&surface.events.new_popup, &listeners.new_popup);
    listeners.new_subsurface.notify
        = wlr_layer_surface_v1_new_subsurface_notify;
    wl_signal_add(&surface.surface->events.new_subsurface,
                  &listeners.new_subsurface);
}

Layer::~Layer() noexcept
{
    wl_list_remove(&listeners.map.link);
    wl_list_remove(&listeners.unmap.link);
    wl_list_remove(&listeners.destroy.link);
    wl_list_remove(&listeners.commit.link);
    wl_list_remove(&listeners.new_popup.link);
    wl_list_remove(&listeners.new_subsurface.link);
}

constexpr wlr_surface* Layer::get_wlr_surface() const
{
    return layer_surface.surface;
}

constexpr Server& Layer::get_server() const { return server; }

constexpr bool Layer::is_view() const { return false; }
