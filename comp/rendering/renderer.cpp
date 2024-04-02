#include "renderer.hpp"

#include "surface/surface.hpp"
#include "surface/view.hpp"
#include "surface/popup.hpp"
#include "util.hpp"

#include <ctime>

#include "wlr-wrap-start.hpp"
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "wlr-wrap-end.hpp"

#define ANIMATION_DURATION 200

static wlr_box scale_box(wlr_box box, double scale)
{
    int new_width = static_cast<int>(box.width * scale);
    int new_height = static_cast<int>(box.height * scale);

    int new_x = box.x + (box.width - new_width) / 2;
    int new_y = box.y + (box.height - new_height) / 2;

    return wlr_box {
        .x = new_x ? new_x : 1,
        .y = new_y ? new_y : 1,
        .width = new_width ? new_width : 1,
        .height = new_height ? new_height : 1,
    };
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

void Renderer::render_scene_node(wlr_scene_node* node, NodeRenderOptions* options)
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
            render_scene_node(n, options);
        }
    } break;
    case WLR_SCENE_NODE_BUFFER: {
        /*
         * Get destination box (window region on screen)
         */
        wlr_box dst_box = {};
        wlr_scene_node_coords(node, &dst_box.x, &dst_box.y);
        dst_box.x -= options->scene_output->x;
        dst_box.y -= options->scene_output->y;
        scene_node_get_size(node, &dst_box.width, &dst_box.height);

        /*
         * Get some texture parameters
         */
        wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);
        wlr_texture* texture
            = scene_buffer_get_texture(scene_buffer, options->server.renderer);

        wl_output_transform transform = wlr_output_transform_invert(scene_buffer->transform);
        transform = wlr_output_transform_compose(transform, options->transform);

        /*
         * Get associated surface
         */
        Surface* surface = nullptr;
        bool is_view = false;
        Animation* animation = nullptr;
        {
            wlr_surface* wlr_surface = wlr_scene_surface_try_from_buffer(scene_buffer)->surface;
            if (wlr_surface)
                surface = static_cast<Surface*>(wlr_surface->data);

            if (surface) {
                if (surface->is_popup()) {
                    is_view = false;
                    animation = &dynamic_cast<Popup*>(surface)->animation;
                } else if (surface->is_view()) {
                    is_view = true;
                    animation = &dynamic_cast<View*>(surface)->animation;
                }
            }
        }

        /*
         * Apply animation factor
         */
        bool animating = animation ? animation->is_animating() : false;

        wlr_box unscaled_box = dst_box;
        if (animating && is_view) // Only scale views
            dst_box = scale_box(dst_box, animation->get_factor());

        float alpha = scene_buffer->opacity * (animating ? animation->get_factor() : 1);

        /*
         * Render texture
         */
        wlr_render_texture_options render_options = {
            .texture = texture,
            .src_box = scene_buffer->src_box,
            .dst_box = dst_box,
            .alpha = &alpha,
            .transform = transform,
            .filter_mode = scene_buffer->filter_mode,
        };
        wlr_render_pass_add_texture(options->render_pass, &render_options);

        /*
         * Render window borders
         */
        if (is_view) {
            View* view = dynamic_cast<View*>(surface);
            wlr_box geom = view->get_geometry();

            wlr_box border_box = view->is_x11()
                ? unscaled_box
                : wlr_box {
                      .x = unscaled_box.x + geom.x,
                      .y = unscaled_box.y + geom.y,
                      .width = geom.width,
                      .height = geom.height,
                  };
            if (animating)
                border_box = scale_box(border_box, animation->get_factor());

            float color[4];
            int_to_float_array(view->is_active
                               ? options->server.config.border.color.focused
                               : options->server.config.border.color.unfocused, color);
            render_window_borders(options->render_pass, border_box, color,
                                  options->server.config.border.width);
        }

        /*
         * Update animation
         */
        if (animation)
            animation->update();
    } break;
    }
}
