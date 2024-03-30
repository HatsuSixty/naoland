#ifndef NAOLAND_RENDERER_HPP
#define NAOLAND_RENDERER_HPP

#include "server.hpp"

#include "wlr-wrap-start.hpp"
#include <wayland-server-protocol.h>
#include "wlr/render/wlr_renderer.h"
#include "wlr-wrap-end.hpp"

namespace Renderer {

struct NodeRenderOptions {
    Server& server;
    wlr_render_pass* render_pass;
    wlr_scene_output* scene_output;
    wl_output_transform transform;
};

void render_scene_node(wlr_scene_node* node, NodeRenderOptions* options);

}

#endif
