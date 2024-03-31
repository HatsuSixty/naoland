#include "output.hpp"

#include "config.hpp"
#include "server.hpp"
#include "surface/layer.hpp"
#include "surface/view.hpp"
#include "types.hpp"
#include "rendering/renderer.hpp"

#include <set>
#include <utility>

#include "wlr-wrap-start.hpp"
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>
#include <wlr/util/box.h>
#include "wlr-wrap-end.hpp"

/* This function is called every time an output is ready to display a frame,
 * generally at the output's refresh rate (e.g. 60Hz). */
static void output_request_state_notify(wl_listener* listener, void* data)
{
    Output& output = naoland_container_of(listener, output, request_state);
    auto const* event = static_cast<wlr_output_event_request_state*>(data);

    wlr_output_commit_state(&output.wlr, event->state);
    output.update_layout();
}

/* This function is called every time an output is ready to display a frame,
 * generally at the output's refresh rate (e.g. 60Hz). */
static void output_frame_notify(wl_listener* listener, void*)
{
    Output& output = naoland_container_of(listener, output, frame);

    wlr_scene_output* scene_output
        = wlr_scene_get_scene_output(output.server.scene, &output.wlr);

    if (scene_output == nullptr || output.is_leased || !output.wlr.enabled) {
        return;
    }

    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_render_pass* pass
        = wlr_output_begin_render_pass(&output.wlr, &state, nullptr, nullptr);

    if (pass) {
        // Clear screen
        wlr_render_rect_options clear_options = {
            .box = { .width = output.wlr.width, .height = output.wlr.height },
            .color = { .3, .3, .3, 1 },
        };
        wlr_render_pass_add_rect(pass, &clear_options);

        Renderer::NodeRenderOptions node_render_options = {
            .server = output.server,
            .render_pass = pass,
            .scene_output = scene_output,
            .transform = output.wlr.transform,
        };
        Renderer::render_scene_node(&scene_output->scene->tree.node,
                                    &node_render_options);

        wlr_render_pass_submit(pass);
    }
    wlr_output_commit_state(&output.wlr, &state);
    wlr_output_state_finish(&state);

    timespec now = {};
    timespec_get(&now, TIME_UTC);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_destroy_notify(wl_listener* listener, void*)
{
    Output& output = naoland_container_of(listener, output, destroy);

    output.server.outputs.erase(&output);
    for (auto const* layer : std::as_const(output.layers)) {
        wlr_layer_surface_v1_destroy(&layer->layer_surface);
    }

    delete &output;
}

Output::Output(Server& server, wlr_output& wlr) noexcept
    : listeners(*this)
    , server(server)
    , wlr(wlr)
{
    wlr.data = this;

    wlr_output_init_render(&wlr, server.allocator, server.renderer);

    wlr_output_state state = {};
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    wlr_output_mode* mode = wlr_output_preferred_mode(&wlr);
    if (mode != nullptr) {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_commit_state(&wlr, &state);
    wlr_output_state_finish(&state);

    listeners.request_state.notify = output_request_state_notify;
    wl_signal_add(&wlr.events.request_state, &listeners.request_state);
    listeners.frame.notify = output_frame_notify;
    wl_signal_add(&wlr.events.frame, &listeners.frame);
    listeners.destroy.notify = output_destroy_notify;
    wl_signal_add(&wlr.events.destroy, &listeners.destroy);

    wlr_output_layout_output* layout_output
        = wlr_output_layout_add_auto(server.output_layout, &wlr);
    wlr_scene_output* scene_output
        = wlr_scene_output_create(server.scene, &wlr);
    wlr_scene_output_layout_add_output(server.scene_layout, layout_output,
                                       scene_output);
}

Output::~Output() noexcept
{
    wl_list_remove(&listeners.request_state.link);
    wl_list_remove(&listeners.frame.link);
    wl_list_remove(&listeners.destroy.link);
}

void Output::update_layout()
{
    wlr_scene_output const* scene_output
        = wlr_scene_get_scene_output(server.scene, &wlr);
    if (scene_output == nullptr) {
        return;
    }

    full_area.x = scene_output->x;
    full_area.y = scene_output->y;
    wlr_output_effective_resolution(&wlr, &full_area.width, &full_area.height);

    usable_area = full_area;

    for (auto const* layer : std::as_const(layers)) {
        wlr_scene_layer_surface_v1_configure(layer->scene_layer_surface,
                                             &full_area, &usable_area);
    }
}
