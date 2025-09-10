#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <wayland-client.h>

#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-client-protocol.h"

#include "ply-logger.h"
#include "ply-renderer-plugin.h"

struct _ply_renderer_head
{
        ply_renderer_backend_t     *backend;
        struct wl_output           *output;

        ply_rectangle_t             area;
        int32_t                     scale;
        ply_pixel_buffer_rotation_t rotation;
        bool                        damaged;

        int32_t                     size;
        ply_pixel_buffer_t         *pixel_buffer;
        void                       *shm_data;
        struct wl_buffer           *buffer;

        struct wl_surface          *surface;
        struct xdg_surface         *xdg_surface;
        struct xdg_toplevel        *toplevel;
};

struct _ply_renderer_input_source
{
        ply_buffer_t                       *key_buffer;
        ply_renderer_input_source_handler_t handler;
        void                               *user_data;

        struct wl_keyboard                 *keyboard;
        struct xkb_context                 *xkb_ctx;
        struct xkb_keymap                  *keymap;
        struct xkb_state                   *state;
};

struct _ply_renderer_backend
{
        ply_event_loop_t           *loop;
        ply_fd_watch_t             *display_watch;

        struct wl_display          *display;
        struct wl_registry         *registry;
        struct wl_compositor       *compositor;
        struct wl_shm              *shm;
        struct xdg_wm_base         *wm_base;
        struct wl_seat             *seat;

        ply_list_t                 *heads;
        ply_renderer_input_source_t input_source;

        bool                        is_active;
};

ply_renderer_plugin_interface_t *ply_renderer_backend_get_interface (void);

static void
noop ()
{
}

static ply_renderer_backend_t *
create_backend (const char     *device_name,
                ply_terminal_t *terminal,
                ply_terminal_t *local_console_terminal)
{
        ply_renderer_backend_t *backend;

        backend = calloc (1, sizeof(ply_renderer_backend_t));

        backend->loop = ply_event_loop_get_default ();
        backend->heads = ply_list_new ();
        backend->input_source.key_buffer = ply_buffer_new ();

        return backend;
}

static void
destroy_backend (ply_renderer_backend_t *backend)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (backend->heads);
        while (node != NULL) {
                ply_list_node_t *next_node;
                ply_renderer_head_t *head;

                head = (ply_renderer_head_t *) ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (backend->heads, node);

                free (head);
                node = next_node;
        }

        ply_buffer_free (backend->input_source.key_buffer);
        ply_list_free (backend->heads);
        free (backend);
}

static void
on_display_event (void *user_data,
                  int   source_fd)
{
        ply_renderer_backend_t *backend = user_data;

        while (wl_display_prepare_read (backend->display)) {
                wl_display_dispatch_pending (backend->display);
        }

        wl_display_read_events (backend->display);
        wl_display_dispatch_pending (backend->display);
}

static void
output_mode (void             *data,
             struct wl_output *wl_output,
             uint32_t          flags,
             int32_t           width,
             int32_t           height,
             int32_t           refresh)
{
        ply_renderer_head_t *head = data;

        head->area.width = width;
        head->area.height = height;
}

static const struct wl_output_listener output_listener = {
        .geometry    = (void (*)(void *, struct wl_output *,        int,                 int, int, int, int, const char *, const char *, int)) noop,
        .mode        = output_mode,
        .done        = (void (*)(void *, struct wl_output *)) noop,
        .scale       = (void (*)(void *, struct wl_output *,        int)) noop,
        .name        = (void (*)(void *, struct wl_output *,        const char *)) noop,
        .description = (void (*)(void *, struct wl_output *,        const char *)) noop
};

static void
wm_base_ping (void               *data,
              struct xdg_wm_base *xdg_wm_base,
              uint32_t            serial)
{
        xdg_wm_base_pong (xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
        .ping = wm_base_ping
};

static void
keyboard_keymap (void               *data,
                 struct wl_keyboard *wl_keyboard,
                 uint32_t            format,
                 int32_t             fd,
                 uint32_t            size)
{
        char *map_shm;
        ply_renderer_input_source_t *input = data;

        assert (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

        xkb_keymap_unref (input->keymap);
        xkb_state_unref (input->state);

        map_shm = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        input->keymap = xkb_keymap_new_from_string (input->xkb_ctx, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap (map_shm, size);
        close (fd);

        input->state = xkb_state_new (input->keymap);
}

static void
keyboard_key (void               *data,
              struct wl_keyboard *wl_keyboard,
              uint32_t            serial,
              uint32_t            time,
              uint32_t            key,
              uint32_t            state)
{
        xkb_keysym_t keysym;
        ply_renderer_input_source_t *input = data;

        if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
                return;

        keysym = xkb_state_key_get_one_sym (input->state, key + 8);
        if (keysym == XKB_KEY_Return) {
                ply_buffer_append_bytes (input->key_buffer, "\n", 1);
        } else if (keysym == XKB_KEY_Escape) {
                ply_buffer_append_bytes (input->key_buffer, "\033", 1);
        } else if (keysym == XKB_KEY_BackSpace) {
                ply_buffer_append_bytes (input->key_buffer, "\177", 1);
        } else {
                char buf[7];
                int num_bytes;

                num_bytes = xkb_state_key_get_utf8 (input->state, key + 8, buf, sizeof(buf));
                ply_buffer_append_bytes (input->key_buffer, buf, num_bytes);
        }

        if (input->handler)
                input->handler (input->user_data, input->key_buffer, input);
}

static void
keyboard_modifiers (void               *data,
                    struct wl_keyboard *wl_keyboard,
                    uint32_t            serial,
                    uint32_t            mods_depressed,
                    uint32_t            mods_latched,
                    uint32_t            mods_locked,
                    uint32_t            group)
{
        ply_renderer_input_source_t *input = data;

        xkb_state_update_mask (input->state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static const struct wl_keyboard_listener keyboard_listener = {
        .keymap    = keyboard_keymap,
        .enter     = (void (*)(void *,struct wl_keyboard *,  unsigned int, struct wl_surface *,        struct wl_array *)) noop,
        .leave     = (void (*)(void *,struct wl_keyboard *,  unsigned int, struct wl_surface *)) noop,
        .key       = keyboard_key,
        .modifiers = keyboard_modifiers
};

static void
seat_capabilities (void           *data,
                   struct wl_seat *wl_seat,
                   uint32_t        capabilities)
{
        ply_renderer_input_source_t *input = data;

        if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD && !input->keyboard) {
                input->keyboard = wl_seat_get_keyboard (wl_seat);
                wl_keyboard_add_listener (input->keyboard, &keyboard_listener, data);
        } else if (input->keyboard) {
                wl_keyboard_release (input->keyboard);
                input->keyboard = NULL;
        }
}

static const struct wl_seat_listener seat_listener = {
        .capabilities = seat_capabilities,
        .name         = (void (*)(void *, struct wl_seat *,const char *)) noop,
};

static void
registry (void               *data,
          struct wl_registry *wl_registry,
          uint32_t            name,
          const char         *interface,
          uint32_t            version)
{
        ply_renderer_backend_t *backend = data;

        if (!strcmp (interface, wl_output_interface.name)) {
                ply_renderer_head_t *head;

                head = calloc (1, sizeof(ply_renderer_head_t));

                head->backend = backend;
                head->output = wl_registry_bind (wl_registry, name, &wl_output_interface, 4);
                wl_output_add_listener (head->output, &output_listener, head);

                ply_list_append_data (backend->heads, head);
        } else if (!strcmp (interface, wl_compositor_interface.name)) {
                backend->compositor = wl_registry_bind (wl_registry, name, &wl_compositor_interface, 6);
        } else if (!strcmp (interface, wl_shm_interface.name)) {
                backend->shm = wl_registry_bind (wl_registry, name, &wl_shm_interface, 1);
        } else if (!strcmp (interface, xdg_wm_base_interface.name)) {
                backend->wm_base = wl_registry_bind (wl_registry, name, &xdg_wm_base_interface, 3);
                xdg_wm_base_add_listener (backend->wm_base, &wm_base_listener, data);
        } else if (!strcmp (interface, wl_seat_interface.name)) {
                backend->seat = wl_registry_bind (wl_registry, name, &wl_seat_interface, 3);
                wl_seat_add_listener (backend->seat, &seat_listener, &backend->input_source);
        }
}

static const struct wl_registry_listener registry_listener = {
        .global = registry
};

static bool
open_device (ply_renderer_backend_t *backend)
{
        backend->display = wl_display_connect (NULL);
        if (!backend->display) {
                ply_trace ("Couldn't connect to wayland display");
                return false;
        }

        backend->display_watch = ply_event_loop_watch_fd (backend->loop, wl_display_get_fd (backend->display), PLY_EVENT_LOOP_FD_STATUS_HAS_DATA, on_display_event, NULL, backend);

        backend->registry = wl_display_get_registry (backend->display);
        wl_registry_add_listener (backend->registry, &registry_listener, backend);
        wl_display_roundtrip (backend->display);

        backend->input_source.xkb_ctx = xkb_context_new (XKB_CONTEXT_NO_FLAGS);

        return true;
}

static void
close_device (ply_renderer_backend_t *backend)
{
        ply_event_loop_stop_watching_fd (backend->loop, backend->display_watch);
        backend->display_watch = NULL;
        wl_display_disconnect (backend->display);
        xkb_context_unref (backend->input_source.xkb_ctx);
}

static void randname (char *buf)
{
        struct timespec ts;
        clock_gettime (CLOCK_REALTIME, &ts);
        long r = ts.tv_nsec;
        for (int i = 0; i < 6; ++i) {
                buf[i] = 'A' + (r & 15) + (r & 16) * 2;
                r >>= 5;
        }
}

static int anonymous_shm_open (void)
{
        char name[] = "/plymouth-XXXXXX";
        int retries = 100;

        do {
                randname (name + strlen (name) - 6);

                --retries;
                // shm_open guarantees that O_CLOEXEC is set
                int fd = shm_open (name, O_RDWR | O_CREAT | O_EXCL, 0600);
                if (fd >= 0) {
                        shm_unlink (name);
                        return fd;
                }
        } while (retries > 0 && errno == EEXIST);

        return -1;
}

static int create_shm_file (off_t size)
{
        int fd = anonymous_shm_open ();
        if (fd < 0) {
                return fd;
        }

        if (ftruncate (fd, size) < 0) {
                close (fd);
                return -1;
        }

        return fd;
}

static bool
query_device (ply_renderer_backend_t *backend,
              bool                    force)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (backend->heads);
        while (node != NULL) {
                ply_list_node_t *next_node;
                ply_renderer_head_t *head;
                int fd;
                struct wl_shm_pool *pool;

                head = (ply_renderer_head_t *) ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (backend->heads, node);

                wl_display_roundtrip (backend->display);

                head->pixel_buffer = ply_pixel_buffer_new (head->area.width, head->area.height);
                ply_pixel_buffer_fill_with_color (head->pixel_buffer, &head->area, 0.0, 0.0, 0.0, 1.0);
                head->size = head->area.width * 4 * head->area.height;
                fd = create_shm_file (head->size);
                head->shm_data = mmap (NULL, head->size, PROT_WRITE, MAP_SHARED, fd, 0);
                pool = wl_shm_create_pool (backend->shm, fd, head->size);
                head->buffer = wl_shm_pool_create_buffer (pool, 0, head->area.width, head->area.height, head->area.width * 4, WL_SHM_FORMAT_ARGB8888);
                wl_shm_pool_destroy (pool);
                close (fd);

                node = next_node;
        }

        return true;
}

static void surface_preferred_buffer_scale (void              *data,
                                            struct wl_surface *wl_surface,
                                            int32_t            factor)
{
        ply_renderer_head_t *head = data;

        head->scale = factor;
        ply_pixel_buffer_set_device_scale (head->pixel_buffer, head->scale);
        wl_surface_set_buffer_scale (wl_surface, head->scale);
}

static void
surface_preferred_buffer_transform (void              *data,
                                    struct wl_surface *wl_surface,
                                    uint32_t           transform)
{
        ply_renderer_head_t *head = data;

        switch (transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
                head->rotation = PLY_PIXEL_BUFFER_ROTATE_UPRIGHT;
                break;
        case WL_OUTPUT_TRANSFORM_90:
                head->rotation = PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE;
                break;
        case WL_OUTPUT_TRANSFORM_180:
                head->rotation = PLY_PIXEL_BUFFER_ROTATE_UPSIDE_DOWN;
                break;
        case WL_OUTPUT_TRANSFORM_270:
                head->rotation = PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE;
                break;
        default:
                head->rotation = PLY_PIXEL_BUFFER_ROTATE_UPRIGHT;
                transform = WL_OUTPUT_TRANSFORM_NORMAL;
                break;
        }

        ply_pixel_buffer_set_device_rotation (head->pixel_buffer, head->rotation);
        wl_surface_set_buffer_transform (wl_surface, transform);
}

static const struct wl_surface_listener surface_listener = {
        .enter                      = (void (*)(void *,               struct wl_surface *, struct wl_output *)) noop,
        .leave                      = (void (*)(void *,               struct wl_surface *, struct wl_output *)) noop,
        .preferred_buffer_scale     = surface_preferred_buffer_scale,
        .preferred_buffer_transform = surface_preferred_buffer_transform
};

static void
surface_configure (void               *data,
                   struct xdg_surface *xdg_surface,
                   uint32_t            serial)
{
        ply_renderer_head_t *head = data;

        xdg_surface_ack_configure (xdg_surface, serial);
        wl_surface_commit (head->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
        .configure = surface_configure
};

static void
toplevel_configure (void                *data,
                    struct xdg_toplevel *xdg_toplevel,
                    int32_t              width,
                    int32_t              height,
                    struct wl_array     *states)
{
        int fd;
        struct wl_shm_pool *pool;
        ply_renderer_head_t *head = data;

        if ((!width && !height) || (width == head->area.width && height == head->area.height)) return;

        wl_buffer_destroy (head->buffer);
        munmap (head->shm_data, head->size);

        head->area.width = width;
        head->area.height = height;

        head->pixel_buffer = ply_pixel_buffer_resize (head->pixel_buffer, head->area.width, head->area.height);
        head->size = head->area.width * 4 * head->area.height;
        fd = create_shm_file (head->size);
        head->shm_data = mmap (NULL, head->size, PROT_WRITE, MAP_SHARED, fd, 0);
        pool = wl_shm_create_pool (head->backend->shm, fd, head->size);
        head->buffer = wl_shm_pool_create_buffer (pool, 0, head->area.width, head->area.height, head->area.width * 4, WL_SHM_FORMAT_ARGB8888);
        wl_shm_pool_destroy (pool);
        close (fd);

        wl_surface_attach (head->surface, head->buffer, 0, 0);
        wl_surface_damage_buffer (head->surface, 0, 0, UINT32_MAX, UINT32_MAX);
        wl_surface_commit (head->surface);
}

static const struct xdg_toplevel_listener toplevel_listener = {
        .configure = toplevel_configure
};

static const struct wl_callback_listener frame_listener;

static void
frame (void               *data,
       struct wl_callback *wl_callback,
       uint32_t            callback_data)
{
        ply_renderer_head_t *head = data;

        wl_callback_destroy (wl_callback);
        wl_callback = wl_surface_frame (head->surface);
        wl_callback_add_listener (wl_callback, &frame_listener, data);

        if (head->damaged) {
                memcpy (head->shm_data, ply_pixel_buffer_get_argb32_data (head->pixel_buffer), head->size);
                wl_surface_damage_buffer (head->surface, 0, 0, head->area.width, head->area.height);
                wl_surface_commit (head->surface);
                head->damaged = false;
        }
}

static const struct wl_callback_listener frame_listener = {
        .done = frame
};

static bool
map_to_device (ply_renderer_backend_t *backend)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (backend->heads);
        while (node != NULL) {
                ply_list_node_t *next_node;
                ply_renderer_head_t *head;

                head = (ply_renderer_head_t *) ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (backend->heads, node);

                if (!head->surface) {
                        struct wl_callback *cb;

                        head->surface = wl_compositor_create_surface (backend->compositor);
                        wl_surface_add_listener (head->surface, &surface_listener, head);
                        head->xdg_surface = xdg_wm_base_get_xdg_surface (backend->wm_base, head->surface);
                        xdg_surface_add_listener (head->xdg_surface, &xdg_surface_listener, head);
                        head->toplevel = xdg_surface_get_toplevel (head->xdg_surface);
                        xdg_toplevel_add_listener (head->toplevel, &toplevel_listener, head);

                        memcpy (head->shm_data, ply_pixel_buffer_get_argb32_data (head->pixel_buffer), head->size);
                        wl_surface_attach (head->surface, head->buffer, 0, 0);
                        wl_surface_damage_buffer (head->surface, 0, 0, head->area.width, head->area.height);

                        xdg_toplevel_set_title (head->toplevel, "Plymouth");
                        xdg_toplevel_set_fullscreen (head->toplevel, head->output);
                        wl_surface_commit (head->surface);

                        cb = wl_surface_frame (head->surface);
                        wl_callback_add_listener (cb, &frame_listener, head);
                }

                node = next_node;
        }

        backend->is_active = true;
        wl_display_roundtrip (backend->display);

        return true;
}

static void
unmap_from_device (ply_renderer_backend_t *backend)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (backend->heads);
        while (node != NULL) {
                ply_list_node_t *next_node;
                ply_renderer_head_t *head;

                head = (ply_renderer_head_t *) ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (backend->heads, node);

                xdg_toplevel_destroy (head->toplevel);
                xdg_surface_destroy (head->xdg_surface);
                wl_surface_destroy (head->surface);
                wl_buffer_destroy (head->buffer);
                munmap (head->shm_data, head->size);
                ply_pixel_buffer_free (head->pixel_buffer);
                head->surface = NULL;

                node = next_node;
        }
}

static void
activate (ply_renderer_backend_t *backend)
{
        backend->is_active = true;
}

static void
deactivate (ply_renderer_backend_t *backend)
{
        backend->is_active = false;
}

static void
flush_head (ply_renderer_backend_t *backend,
            ply_renderer_head_t    *head)
{
        if (!backend->is_active)
                return;

        head->damaged = true;
        wl_display_roundtrip (backend->display);
}

static ply_list_t *
get_heads (ply_renderer_backend_t *backend)
{
        return backend->heads;
}

static ply_pixel_buffer_t *
get_buffer_for_head (ply_renderer_backend_t *backend,
                     ply_renderer_head_t    *head)
{
        return head->pixel_buffer;
}

static ply_renderer_input_source_t *
get_input_source (ply_renderer_backend_t *backend)
{
        return &backend->input_source;
}

static bool
open_input_source (ply_renderer_backend_t      *backend,
                   ply_renderer_input_source_t *input_source)
{
        return input_source->keyboard != NULL;
}

static void
set_handler_for_input_source (ply_renderer_backend_t             *backend,
                              ply_renderer_input_source_t        *input_source,
                              ply_renderer_input_source_handler_t handler,
                              void                               *user_data)
{
        input_source->handler = handler;
        input_source->user_data = user_data;
}

static const char *
get_device_name (ply_renderer_backend_t *backend)
{
        return "wayland";
}

ply_renderer_plugin_interface_t *
ply_renderer_backend_get_interface (void)
{
        static ply_renderer_plugin_interface_t plugin_interface = {
                .create_backend               = create_backend,
                .destroy_backend              = destroy_backend,
                .open_device                  = open_device,
                .close_device                 = close_device,
                .query_device                 = query_device,
                .map_to_device                = map_to_device,
                .unmap_from_device            = unmap_from_device,
                .activate                     = activate,
                .deactivate                   = deactivate,
                .flush_head                   = flush_head,
                .get_heads                    = get_heads,
                .get_buffer_for_head          = get_buffer_for_head,
                .get_input_source             = get_input_source,
                .open_input_source            = open_input_source,
                .set_handler_for_input_source = set_handler_for_input_source,
                .close_input_source           = (void (*)(ply_renderer_backend_t *,ply_renderer_input_source_t *)) noop,
                .get_device_name              = get_device_name
        };

        return &plugin_interface;
}
