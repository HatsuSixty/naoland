#ifndef NAOLAND_SURFACE_HPP
#define NAOLAND_SURFACE_HPP

#include "types.hpp"

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_scene.h>
#include "wlr-wrap-end.hpp"

enum SurfaceType {
    NAOLAND_SURFACE_TYPE_VIEW,
    NAOLAND_SURFACE_TYPE_LAYER,
    NAOLAND_SURFACE_TYPE_POPUP
};

struct Surface {
    wlr_scene_tree* scene_tree = nullptr;

    virtual ~Surface() noexcept = default;

    [[nodiscard]] virtual constexpr Server& get_server() const = 0;
    [[nodiscard]] virtual constexpr wlr_surface* get_wlr_surface() const = 0;
    [[nodiscard]] virtual constexpr bool is_view() const = 0;
};

#endif
