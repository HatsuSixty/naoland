#include "output.hpp"

#include "config.hpp"
#include "server.hpp"
#include "surface/layer.hpp"
#include "surface/view.hpp"
#include "types.hpp"

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

static void scene_node_get_size(wlr_scene_node* node, int* width, int* height)
{
    *width = 0;
    *height = 0;

    switch (node->type) {
    case WLR_SCENE_NODE_TREE:
        return;
    case WLR_SCENE_NODE_RECT: {
        wlr_scene_rect* scene_rect = wlr_scene_rect_from_node(node);
        *width = scene_rect->width;
        *height = scene_rect->height;
    } break;
    case WLR_SCENE_NODE_BUFFER: {
        wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);
        if (scene_buffer->dst_width > 0 && scene_buffer->dst_height > 0) {
            *width = scene_buffer->dst_width;
            *height = scene_buffer->dst_height;
        } else if (scene_buffer->buffer) {
            if (scene_buffer->transform & WL_OUTPUT_TRANSFORM_90) {
                *height = scene_buffer->buffer->width;
                *width = scene_buffer->buffer->height;
            } else {
                *width = scene_buffer->buffer->width;
                *height = scene_buffer->buffer->height;
            }
        }
    } break;
    }
}

static wlr_texture* scene_buffer_get_texture(wlr_scene_buffer* scene_buffer,
                                             wlr_renderer* renderer)
{
    wlr_client_buffer* client_buffer
        = wlr_client_buffer_get(scene_buffer->buffer);
    if (client_buffer != NULL) {
        return client_buffer->texture;
    }

    if (scene_buffer->texture != NULL) {
        return scene_buffer->texture;
    }

    scene_buffer->texture
        = wlr_texture_from_buffer(renderer, scene_buffer->buffer);
    return scene_buffer->texture;
}

static void render_window_borders(wlr_render_pass* pass, wlr_box window_box, float const color[4], int width)
{
    wlr_render_rect_options rect_options = {
        .color = {
            .r = color[0],
            .g = color[1],
            .b = color[2],
            .a = color[3],
        },
    };

    rect_options.box = {
        .x = window_box.x - width,
        .y = window_box.y - width,
        .width = window_box.width + width * 2,
        .height = width,
    };
    wlr_render_pass_add_rect(pass, &rect_options);

    rect_options.box = {
        .x = window_box.x - width,
        .y = window_box.y + window_box.height,
        .width = window_box.width + width * 2,
        .height = width,
    };
    wlr_render_pass_add_rect(pass, &rect_options);

    rect_options.box = {
        .x = window_box.x - width,
        .y = window_box.y,
        .width = width,
        .height = window_box.height,
    };
    wlr_render_pass_add_rect(pass, &rect_options);

    rect_options.box = {
        .x = window_box.x + window_box.width,
        .y = window_box.y,
        .width = width,
        .height = window_box.height,
    };
    wlr_render_pass_add_rect(pass, &rect_options);
}

struct NodeRenderOptions {
    Server& server;
    wlr_render_pass* render_pass;
    wlr_scene_output* scene_output;
    wl_output_transform transform;
};

static void scene_node_render(wlr_scene_node* node, NodeRenderOptions* options)
{
    if (!node->enabled)
        return;

    switch (node->type) {
    case WLR_SCENE_NODE_RECT:
        wlr_log(WLR_ERROR, "Rendering rectangles is not implemented yet\n");
        break;
    case WLR_SCENE_NODE_TREE: {
        wlr_scene_tree* tree = wlr_scene_tree_from_node(node);
        wlr_scene_node* n = {};
        wl_list_for_each(n, &tree->children, link)
        {
            scene_node_render(n, options);
        }
    } break;
    case WLR_SCENE_NODE_BUFFER: {
        wlr_box dst_box = {};
        wlr_scene_node_coords(node, &dst_box.x, &dst_box.y);
        dst_box.x -= options->scene_output->x;
        dst_box.y -= options->scene_output->y;
        scene_node_get_size(node, &dst_box.width, &dst_box.height);

        wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);
        wlr_texture* texture
            = scene_buffer_get_texture(scene_buffer, options->server.renderer);

        wl_output_transform transform = wlr_output_transform_invert(scene_buffer->transform);
        transform = wlr_output_transform_compose(transform, options->transform);

        wlr_render_texture_options render_options = {
            .texture = texture,
            .src_box = scene_buffer->src_box,
            .dst_box = dst_box,
            .alpha = &scene_buffer->opacity,
            .transform = transform,
            .filter_mode = scene_buffer->filter_mode,
        };
        wlr_render_pass_add_texture(options->render_pass, &render_options);

        View* view = nullptr;
        {
            wlr_surface* wlr_surface = wlr_scene_surface_try_from_buffer(scene_buffer)->surface;
            Surface* surface = nullptr;
            if (wlr_surface)
                surface = static_cast<Surface*>(wlr_surface->data);
            if (surface)
                if (surface->is_view())
                    view = dynamic_cast<View*>(surface);
        }

        if (view) {
            float color[4];
            int_to_float_array(view->is_active
                               ? options->server.config.border.color.focused
                               : options->server.config.border.color.unfocused, color);
            render_window_borders(options->render_pass, dst_box, color,
                                  options->server.config.border.width);
        }
    } break;
    }
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

        NodeRenderOptions node_render_options = {
            .server = output.server,
            .render_pass = pass,
            .scene_output = scene_output,
            .transform = output.wlr.transform,
        };
        scene_node_render(&scene_output->scene->tree.node,
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
