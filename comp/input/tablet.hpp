#ifndef NAOLAND_TABLET_HPP
#define NAOLAND_TABLET_HPP

#include <functional>

#include "seat.hpp"

#include "wlr-wrap-start.hpp"
#include <wayland-server-core.h>
#include <wlr/types/wlr_tablet_tool.h>
#include "wlr-wrap-end.hpp"

class DrawingTablet {
public:
    struct Listeners {
        std::reference_wrapper<DrawingTablet> parent;
        struct wl_listener axis;
        struct wl_listener tip;
        struct wl_listener button;
        struct wl_listener destroy;
        explicit Listeners(DrawingTablet& parent) noexcept
            : parent(parent)
        {
        }
    };

private:
    Listeners listeners;

public:
    Seat& seat;
    wlr_tablet* wlr;
    double x;
    double y;

    explicit DrawingTablet(Seat& seat, wlr_input_device* input_device) noexcept;
    ~DrawingTablet() noexcept;
};

#endif
