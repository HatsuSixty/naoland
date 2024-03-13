#ifndef NAOLAND_VIEW_HPP
#define NAOLAND_VIEW_HPP

#include "foreign_toplevel.hpp"
#include "input/cursor.hpp"
#include "surface.hpp"
#include "types.hpp"

#include <optional>

#include "wlr-wrap-start.hpp"
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/xwayland.h>
#include "wlr-wrap-end.hpp"

struct View : Surface {
    ViewPlacement prev_placement = VIEW_PLACEMENT_STACKING;
    ViewPlacement curr_placement = VIEW_PLACEMENT_STACKING;
    bool is_minimized = false;
    wlr_box current = {};
    wlr_box previous = {};
    std::optional<ForeignToplevelHandle> toplevel_handle = {};

    ~View() noexcept override = default;

    [[nodiscard]] virtual wlr_box get_geometry() const = 0;
    [[nodiscard]] virtual wlr_box get_min_size() const = 0;
    [[nodiscard]] virtual wlr_box get_max_size() const = 0;

    virtual void map() = 0;
    virtual void unmap() = 0;
    virtual void close() = 0;

    [[nodiscard]] constexpr bool is_view() const override { return true; }
    void begin_interactive(CursorMode mode, uint32_t edges);
    void set_position(int32_t x, int32_t y);
    void set_size(int32_t width, int32_t height);
    void set_geometry(int32_t x, int32_t y, int32_t width, int32_t height);
    void update_outputs(bool ignore_previous = false) const;
    void set_activated(bool activated);
    void set_placement(ViewPlacement new_placement, bool force = false);
    void set_minimized(bool minimized);
    void toggle_maximize();
    void toggle_fullscreen();

private:
    [[nodiscard]] std::optional<std::reference_wrapper<Output>>
    find_output_for_maximize() const;
    void stack();
    bool maximize();
    bool fullscreen();

protected:
    virtual void impl_set_position(int32_t x, int32_t y) = 0;
    virtual void impl_set_size(int32_t width, int32_t height) = 0;
    virtual void impl_set_geometry(int x, int y, int width, int height) = 0;
    virtual void impl_set_activated(bool activated) = 0;
    virtual void impl_set_fullscreen(bool fullscreen) = 0;
    virtual void impl_set_maximized(bool maximized) = 0;
    virtual void impl_set_minimized(bool minimized) = 0;
};

class XdgView final : public View {
public:
    struct Listeners {
        std::reference_wrapper<XdgView> parent;
        wl_listener map = {};
        wl_listener unmap = {};
        wl_listener destroy = {};
        wl_listener commit = {};
        wl_listener request_move = {};
        wl_listener request_resize = {};
        wl_listener request_maximize = {};
        wl_listener request_minimize = {};
        wl_listener request_fullscreen = {};
        wl_listener set_title = {};
        wl_listener set_app_id = {};
        wl_listener set_parent = {};
        explicit Listeners(XdgView& parent) noexcept
            : parent(parent)
        {
        }
    };

private:
    Listeners listeners;
    bool pending_map = true;

public:
    Server& server;
    wlr_xdg_toplevel& xdg_toplevel;

    XdgView(Server& server, wlr_xdg_toplevel& wlr) noexcept;
    ~XdgView() noexcept override;

    [[nodiscard]] constexpr wlr_surface* get_wlr_surface() const override;
    [[nodiscard]] constexpr Server& get_server() const override;

    [[nodiscard]] wlr_box get_geometry() const override;
    [[nodiscard]] constexpr wlr_box get_min_size() const override;
    [[nodiscard]] constexpr wlr_box get_max_size() const override;

    void map() override;
    void unmap() override;
    void close() override;

protected:
    void impl_set_position(int32_t x, int32_t y) override;
    void impl_set_size(int32_t width, int32_t height) override;
    void impl_set_geometry(int x, int y, int width, int height) override;
    void impl_set_activated(bool activated) override;
    void impl_set_fullscreen(bool fullscreen) override;
    void impl_set_maximized(bool maximized) override;
    void impl_set_minimized(bool minimized) override;
};

class XWaylandView final : public View {
public:
    struct Listeners {
        std::reference_wrapper<XWaylandView> parent;
        wl_listener map = {};
        wl_listener unmap = {};
        wl_listener associate = {};
        wl_listener dissociate = {};
        wl_listener destroy = {};
        wl_listener commit = {};
        wl_listener request_configure = {};
        wl_listener request_move = {};
        wl_listener request_resize = {};
        wl_listener request_maximize = {};
        wl_listener request_fullscreen = {};
        wl_listener set_geometry = {};
        wl_listener set_title = {};
        wl_listener set_class = {};
        wl_listener set_parent = {};
        explicit Listeners(XWaylandView& parent) noexcept
            : parent(parent)
        {
        }
    } listeners;

    Server& server;
    wlr_xwayland_surface& xwayland_surface;

    XWaylandView(Server& server, wlr_xwayland_surface& surface) noexcept;
    ~XWaylandView() noexcept override;

    [[nodiscard]] constexpr wlr_surface* get_wlr_surface() const override;
    [[nodiscard]] constexpr Server& get_server() const override;

    [[nodiscard]] constexpr wlr_box get_geometry() const override;
    [[nodiscard]] constexpr wlr_box get_min_size() const override;
    [[nodiscard]] constexpr wlr_box get_max_size() const override;

    void map() override;
    void unmap() override;
    void close() override;

protected:
    void impl_set_position(int32_t x, int32_t y) override;
    void impl_set_size(int32_t width, int32_t height) override;
    void impl_set_geometry(int32_t x, int32_t y, int width,
                           int height) override;
    void impl_set_activated(bool activated) override;
    void impl_set_fullscreen(bool fullscreen) override;
    void impl_set_maximized(bool maximized) override;
    void impl_set_minimized(bool minimized) override;
};

#endif
