#ifndef NAOLAND_TYPES_HPP
#define NAOLAND_TYPES_HPP

class Server;
class XWayland;
class Output;
struct Config;

class Seat;
class Keyboard;
class Cursor;
class PointerConstraint;

struct Surface;
struct View;
class XdgView;
class XWaylandView;
class Layer;
class LayerSubsurface;
class Popup;

class ForeignToplevelHandle;

enum ViewPlacement {
    VIEW_PLACEMENT_STACKING,
    VIEW_PLACEMENT_MAXIMIZED,
    VIEW_PLACEMENT_FULLSCREEN,
};

#define naoland_container_of(ptr, sample, member)                           \
    (__extension__({                                                        \
        std::remove_reference<decltype(sample)>::type::Listeners* container \
            = wl_container_of(ptr, container, member);                      \
        container->parent;                                                  \
    }))

#endif
