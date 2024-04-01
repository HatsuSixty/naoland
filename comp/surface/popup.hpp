#ifndef NAOLAND_POPUP_HPP
#define NAOLAND_POPUP_HPP

#include "rendering/animation.hpp"
#include "surface.hpp"
#include "types.hpp"

#include <functional>

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_xdg_shell.h>
#include "wlr-wrap-end.hpp"

class Popup final : public Surface {
public:
    struct Listeners {
        std::reference_wrapper<Popup> parent;
        wl_listener map = {};
        wl_listener destroy = {};
        wl_listener new_popup = {};
        explicit Listeners(Popup& parent) noexcept
            : parent(parent)
        {
        }
    };

private:
    Listeners listeners;

public:
    Server& server;
    Surface const& parent;
    wlr_xdg_popup& wlr;
    Animation animation;

    Popup(Surface const& parent, wlr_xdg_popup& wlr) noexcept;
    ~Popup() noexcept override;

    [[nodiscard]] constexpr wlr_surface* get_wlr_surface() const override;
    [[nodiscard]] constexpr Server& get_server() const override;
    [[nodiscard]] constexpr bool is_view() const override;
    [[nodiscard]] constexpr bool is_popup() const override;
};

#endif
