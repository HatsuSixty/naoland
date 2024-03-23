#ifndef NAOLAND_SERVER_HPP
#define NAOLAND_SERVER_HPP

#include "config.hpp"
#include "types.hpp"

#include <functional>
#include <list>
#include <set>

#include "wlr-wrap-start.hpp"
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include "wlr-wrap-end.hpp"

enum NaolandSceneLayer {
    NAOLAND_SCENE_LAYER_BACKGROUND = 0,
    NAOLAND_SCENE_LAYER_BOTTOM,
    NAOLAND_SCENE_LAYER_NORMAL,
    NAOLAND_SCENE_LAYER_TOP,
    NAOLAND_SCENE_LAYER_OVERLAY,
    NAOLAND_SCENE_LAYER_LOCK
};

class Server {
public:
    struct Listeners {
        std::reference_wrapper<Server> parent;
        wl_listener xdg_shell_new_xdg_surface = {};
        wl_listener layer_shell_new_layer_surface = {};
        wl_listener activation_request_activation = {};
        wl_listener backend_new_output = {};
        wl_listener drm_lease_request = {};
        wl_listener output_layout_change = {};
        wl_listener output_manager_apply = {};
        wl_listener output_power_manager_set_mode = {};
        wl_listener decoration_manager_new_toplevel_decoration = {};
        explicit Listeners(Server& parent) noexcept
            : parent(parent)
        {
        }
    };

private:
    Listeners listeners;

public:
    wl_display* display;
    wlr_session* session;
    wlr_backend* backend;
    wlr_renderer* renderer;
    wlr_allocator* allocator;
    wlr_compositor* compositor;

    XWayland* xwayland;

    wlr_scene* scene;
    wlr_scene_output_layout* scene_layout;
    wlr_scene_tree* scene_layers[NAOLAND_SCENE_LAYER_LOCK + 1] = {};

    wlr_xdg_shell* xdg_shell;

    wlr_xdg_activation_v1* xdg_activation;

    wlr_foreign_toplevel_manager_v1* foreign_toplevel_manager;

    wlr_layer_shell_v1* layer_shell;

    Seat* seat;

    std::list<View*> views;
    View* focused_view = nullptr;
    View* grabbed_view = nullptr;
    double grab_x = 0.0, grab_y = 0.0;
    wlr_box grab_geobox = {};
    uint32_t resize_edges = 0;

    wlr_output_manager_v1* output_manager;
    wlr_output_power_manager_v1* output_power_manager;
    wlr_output_layout* output_layout;
    std::set<Output*> outputs;
    uint8_t num_pending_output_layout_changes = 0;

    wlr_idle_notifier_v1* idle_notifier;
    wlr_idle_inhibit_manager_v1* idle_inhibit_manager;

    wlr_drm_lease_v1_manager* drm_manager;
    wlr_content_type_manager_v1* content_type_manager;

    wlr_xdg_decoration_manager_v1* decoration_manager;
    Config config;

    Server();

    Surface* surface_at(double lx, double ly, wlr_surface** wlr, double* sx,
                        double* sy) const;
    void focus_view(View* view, wlr_surface* surface = nullptr);
};

#endif
