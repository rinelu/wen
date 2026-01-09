/* wen - v0.1.0 - Public Domain - https://github.com/rinelu/wen

   wen is a deterministic, zero-allocation networking core
   focused on explicit lifetimes and user-managed I/O.

   # Quick Example

      ```c
      // main.c
      #include "wen.h"

      int main(void)
      {
          wen_link link;
          wen_event ev;

          wen_io io = {
              .user  = my_socket,
              .read  = my_read_fn,
              .write = my_write_fn
          };

          wen_link_init(&link, io);
          wen_link_attach_codec(&link, &my_codec, my_codec_state);

          while (wen_poll(&link, &ev)) {
              switch (ev.type) {
              case WEN_EV_OPEN:
                  // connection established
                  break;
              case WEN_EV_SLICE:
                  // consume ev.as.slice
                  wen_release(&link, ev.as.slice);
                  break;
              case WEN_EV_CLOSE:
                  // connection closed
                  break;
              case WEN_EV_ERROR:
                  // handle error
                  break;
              default: break;
              }
          }

          return 0;
      }
      ```

   # VERSIONING

     Version format: MAJOR.MINOR.PATCH

     PATCH: bug fixes, no semantic changes
     MINOR: additive features, no breaking changes
     MAJOR: breaking API or semantic changes
     v0.x.y indicates the API is still evolving.

   # Memory Model

     wen itself does not allocate memory dynamically by default.

     All temporary allocations use a user-owned arena (wen_arena).
     Slices returned to the user remain valid until wen_release() is called.
     Individual allocations cannot be freed; arenas are reset in bulk.

     If WEN_NO_MALLOC is enabled, no calls to malloc/free/realloc are made.

   # Thread Safety

     wen is NOT thread-safe.

     Each wen_link must be confined to a single thread.
     Synchronization is the responsibility of the caller.

   # Error Handling

     Errors are reported explicitly via:
       return values (wen_result)
       WEN_EV_ERROR events

     wen never aborts or exits the program.

   # Macro Interface

     All configuration macros must be defined BEFORE including wen.h.

     ## Flags

        - WEN_NO_MALLOC - Disable all dynamic memory usage inside wen.
          Default: 1
        - WEN_DETERMINISTIC - Enforce deterministic behavior. Non-deterministic code paths become unreachable.
          Default: 0
        - WEN_ENABLE_WS - Enable the built-in WebSocket codec (if provided).
          Default: 1

     ## Size Limits

        - WEN_MAX_SLICE - Maximum size of a slice returned to the user.
        - WEN_RX_BUFFER - Internal receive buffer size per link.
        - WEN_TX_BUFFER - Internal transmit buffer size per link.

        These are compile-time constants and must be large enough for your protocol.

*/

#ifndef WEN_H_
#define WEN_H_

#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define WEN_VMAJOR 0
#define WEN_VMINOR 1
#define WEN_VPATCH 0

// Convert version to a single integer.
// ex: 0.1.0 becomes 1000 (0 * 1000000 + 1 * 1000 + 0)
#define WEN_VNUMBER ((WEN_VMAJOR * 1000000) + (WEN_VMINOR * 1000) + WEN_VPATCH)
#define WEN__STR_HELP(...) #__VA_ARGS__
#define WEN_STR(...) WEN__STR_HELP(__VA_ARGS__)
#define WEN_VSTRING WEN_STR(WEN_VMAJOR)"." WEN_STR(WEN_VMINOR)"." WEN_STR(WEN_VPATCH)

// Disables all dynamic memory usage inside wen.
#ifndef WEN_NO_MALLOC
#    define WEN_NO_MALLOC 1
#endif

// Enables deterministic behavior
#ifndef WEN_DETERMINISTIC
#    define WEN_DETERMINISTIC 0
#endif

// Enables the built-in WebSocket codec.
#ifndef WEN_ENABLE_WS
#    define WEN_ENABLE_WS 1
#endif

// Maximum size of a slice returned to the user.
#ifndef WEN_MAX_SLICE
#    define WEN_MAX_SLICE 4096
#endif

// Internal receive buffer size.
#ifndef WEN_RX_BUFFER
#    define WEN_RX_BUFFER 8192
#endif

// Internal transmit buffer size.
#ifndef WEN_TX_BUFFER
#    define WEN_TX_BUFFER 8192
#endif

#ifndef WENDEF
#    define WENDEF static inline
#endif

#if defined(_WIN32) || defined(_WIN64)
#    define WEN_PLATFORM_WINDOWS 1
#else
#    define WEN_PLATFORM_POSIX 1
#endif

#define WEN_UNUSED(x) ((void)(x))
#define WEN_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define WEN_MIN(a, b) ((a) < (b) ? (a) : (b))
#define WEN_MAX(a, b) ((a) > (b) ? (a) : (b))
#define WEN_SWAP(type, a, b)                                                   \
    do {                                                                       \
        type _tmp = (a);                                                       \
        (a) = (b);                                                             \
        (b) = _tmp;                                                            \
    } while (0)
#if defined(__GNUC__) || defined(__clang__)
#    define WEN_UNREACHABLE() __builtin_unreachable()
#else
#    define WEN_UNREACHABLE() ((void)0)
#endif

// Result codes returned by wen functions.
typedef enum {
    WEN_OK = 0,
    WEN_ERR_IO,
    WEN_ERR_PROTOCOL,
    WEN_ERR_OVERFLOW,
    WEN_ERR_STATE,
    WEN_ERR_UNSUPPORTED,
    WEN_ERR_CLOSED
} wen_result;

// Current state of a link.
typedef enum {
    WEN_LINK_INIT = 0,
    WEN_LINK_HANDSHAKE,
    WEN_LINK_OPEN,
    WEN_LINK_CLOSING,
    WEN_LINK_CLOSED
} wen_link_state;

// Indicates where a slice lies within a message stream.
typedef enum {
    WEN_SLICE_BEGIN = 1 << 0,
    WEN_SLICE_CONT  = 1 << 1,
    WEN_SLICE_END   = 1 << 2
} wen_slice_flags;

// Type of event produced by wen_poll().
typedef enum {
    WEN_EV_NONE = 0,
    WEN_EV_OPEN,
    WEN_EV_SLICE,
    WEN_EV_FRAME,
    WEN_EV_PING,
    WEN_EV_PONG,
    WEN_EV_CLOSE,
    WEN_EV_ERROR
} wen_event_type;

// An allocation arena with linear growth.
//
// Memory allocated from the arena is freed all at once by resetting it.
// Individual allocations cannot be freed.
typedef struct {
    unsigned char *base;
    unsigned long capacity;
    unsigned long used;
} wen_arena;

// Transport abstraction used by wen.
//
// The user provides read/write callbacks backed by TCP, TLS, or something else.
typedef struct wen_io {
    void *user;
    long (*read)(void *user, void *buf, unsigned long len);
    long (*write)(void *user, const void *buf, unsigned long len);
} wen_io;

// A zero-copy view into received data.
//
// The memory remains valid until wen_release() is called.
typedef struct {
    const void *data;
    unsigned long len;
    unsigned flags;
} wen_slice;

// Metadata for a decoded wire frame.
//
// This is exposed for protocol inspection and debugging.
typedef struct {
    unsigned fin    : 1;
    unsigned masked : 1;
    unsigned opcode : 4;
    unsigned long long length;
} wen_frame;

// Event returned by wen_poll().
//// Only the union member corresponding to the event type is valid.
typedef struct {
    wen_event_type type;
    union {
        wen_slice slice;
        wen_frame frame;
        unsigned close_code;
        wen_result error;
    } as;
} wen_event;

// Interface implemented by wire-level protocols.
//
// WebSocket is provided as one such codec.
typedef struct wen_codec {
    const char *name;

    // Performs protocol-specific handshake.
    wen_result (*handshake)(void *codec_state, const void *in,
                            unsigned long in_len, void *out,
                            unsigned long out_cap, unsigned long *out_len);

    // Consumes raw bytes and produces events.
    wen_result (*decode)(void *codec_state, const void *data, unsigned long len);

    // Encodes an outgoing message or control frame.
    wen_result (*encode)(void *codec_state, unsigned opcode, const void *data, unsigned long len);
} wen_codec;

// Represents a single wire connection.
//
// A link owns its buffers, codec state, and event queue.
typedef struct wen_link {
    wen_link_state state;
    wen_io io;

    unsigned char rx_buf[WEN_RX_BUFFER];
    unsigned long rx_len;

    unsigned char tx_buf[WEN_TX_BUFFER];
    unsigned long tx_len;

    const wen_codec *codec;
    void *codec_state;

    void *user_data;
    wen_arena arena;
} wen_link;

// WebSocket protocol GUID used during the handshake.
#define WEN_WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// WebSocket opcodes.
#define WEN_WS_OP_CONT 0x0
#define WEN_WS_OP_TEXT 0x1
#define WEN_WS_OP_BINARY 0x2
#define WEN_WS_OP_CLOSE 0x8
#define WEN_WS_OP_PING 0x9
#define WEN_WS_OP_PONG 0xA

// Initializes a link with the given IO backend.
WENDEF wen_result wen_link_init(wen_link *link, wen_io io);

// Attaches a codec to the link.
//
// Must be called before polling.
WENDEF void wen_link_attach_codec(wen_link *link, const wen_codec *codec, void *codec_state);

// Polls for the next available event.
//
// Returns non-zero if an event was produced.
WENDEF int wen_poll(wen_link *link, wen_event *ev);

// Releases a slice previously returned by wen_poll().
WENDEF void wen_release(wen_link *link, wen_slice slice);

// Sends an application message using the active codec.
WENDEF wen_result wen_send(wen_link *link, unsigned opcode, const void *data, unsigned long len);

// Initiates a clean protocol-level close.
WENDEF wen_result wen_close(wen_link *link, unsigned code);

// Reset wen_link buffers.
WENDEF void wen_link_reset_buffers(wen_link *link);

#define WEN_STATIC_ASSERT(cond, name) typedef char wen_static_assert_##name[(cond) ? 1 : -1]

WEN_STATIC_ASSERT(WEN_RX_BUFFER >= 1024, rx_buffer_too_small);
WEN_STATIC_ASSERT(WEN_TX_BUFFER >= 1024, tx_buffer_too_small);

//////////////////////////////////////////////////////////////////////////////

// A snapshot of the arena state.
//
// Used to roll back temporary allocations.
typedef unsigned long wen_arena_snapshot;

// Returns the number of bytes remaining in the arena.
#define WEN_ARENA_REMAINING(a) ((a)->capacity - (a)->used)

// Returns non-zero if the arena has at least [n] bytes available.
#define WEN_ARENA_CAN_ALLOC(a, n) ((a)->used + (n) <= (a)->capacity)

// Aligns [x] up to the next multiple of [a].
#define WEN_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

// Default alignment used by arena allocations.
#ifndef WEN_ARENA_ALIGN
#    define WEN_ARENA_ALIGN (sizeof(void *))
#endif

// Resets the arena to a previous mark.
//
// This invalidates all allocations made after the mark.
WENDEF void wen_arena_reset(wen_arena *a, wen_arena_snapshot snapshot);

// Allocates [size] bytes to arena memory.
WENDEF wen_result wen_arena_init(wen_arena *arena, unsigned long size);

// Allocates [size] bytes from the arena.
//
// The returned memory is uninitialized.
// Returns NULL if the arena does not have enough space.
WENDEF void *wen_arena_alloc(wen_arena *a, unsigned long size);

// Allocates [count] elements of [size] bytes each.
//
// The returned memory is zero-initialized.
WENDEF void *wen_arena_calloc(wen_arena *a, unsigned long count, unsigned long size);

#define WEN_IMPLEMENTATION
#ifdef WEN_IMPLEMENTATION

WENDEF void wen_link_reset_buffers(wen_link *link)
{
    link->rx_len = 0;
    link->tx_len = 0;
}

WENDEF wen_result wen_link_init(wen_link *link, wen_io io) {
    if (!link || !io.read || !io.write) return WEN_ERR_STATE;

    memset(link, 0, sizeof(*link));

    link->state = WEN_LINK_INIT;
    link->io    = io;

    unsigned long arena_size = WEN_RX_BUFFER + WEN_TX_BUFFER;
    wen_result result = wen_arena_init(&link->arena, arena_size);
    if (result != WEN_OK) return result;

    wen_link_reset_buffers(link);
    wen_arena_reset(&link->arena, 0);

    return WEN_OK;
}


WENDEF void wen_link_attach_codec(wen_link *link, const wen_codec *codec, void *codec_state)
{
    if (!link || !codec) return;

    link->codec       = codec;
    link->codec_state = codec_state;
    link->state       = WEN_LINK_HANDSHAKE;
}

WENDEF int wen_poll(wen_link *link, wen_event *ev)
{
    long nread;

    if (!link || !ev) return false;

    ev->type = WEN_EV_NONE;

    if (link->state == WEN_LINK_CLOSED) return false;

    if (!link->codec) {
        ev->type = WEN_EV_ERROR;
        ev->as.error = WEN_ERR_UNSUPPORTED;
        return true;
    }

    // TODO: Real handshake phase
    if (link->state == WEN_LINK_HANDSHAKE) {
        link->state = WEN_LINK_OPEN;
        ev->type = WEN_EV_OPEN;
        return true;
    }

    nread = link->io.read(
        link->io.user,
        link->rx_buf + link->rx_len,
        WEN_RX_BUFFER - link->rx_len
    );

    if (nread < 0) {
        ev->type = WEN_EV_ERROR;
        ev->as.error = WEN_ERR_IO;
        return true;
    }

    if (nread == 0) {
        link->state = WEN_LINK_CLOSED;
        ev->type = WEN_EV_CLOSE;
        ev->as.close_code = 0;
        return true;
    }

    link->rx_len += (unsigned long)nread;

    // Decode is codec-specific and opaque
    if (link->codec->decode) {
        // TODO: slice_length
        unsigned long slice_length = link->rx_len;
        ev->as.slice.data = wen_arena_alloc(&link->arena, slice_length);
        if (!ev->as.slice.data) {
            ev->type = WEN_EV_ERROR;
            ev->as.error = WEN_ERR_IO;
            return true;
        }
        wen_result r = link->codec->decode(link->codec_state, ev->as.slice.data, slice_length);

        if (r != WEN_OK) {
            ev->type = WEN_EV_ERROR;
            ev->as.error = r;
            return true;
        }
    }

    // TODO: slice/frame queue
    return false;
}

WENDEF void wen_release(wen_link *link, wen_slice slice) {
    WEN_UNUSED(slice);
    if (!link) return;

    wen_arena_reset(&link->arena, link->arena.used);
}


WENDEF wen_result wen_send(wen_link *link, unsigned opcode, const void *data, unsigned long len)
{
    if (!link || !link->codec) return WEN_ERR_STATE;
    if (!link->codec->encode)  return WEN_ERR_UNSUPPORTED;
    if (link->tx_len != 0)     return WEN_ERR_STATE;

    wen_result r = link->codec->encode(link->codec_state, opcode, data, len);

    if (r != WEN_OK) return r;

    long nwritten = link->io.write(link->io.user, link->tx_buf, link->tx_len);
    if (nwritten < 0 || (unsigned long)nwritten != link->tx_len)
        return WEN_ERR_IO;

    link->tx_len = 0;
    return WEN_OK;
}

WENDEF wen_result wen_close(wen_link *link, unsigned code) {
    if (!link) return WEN_ERR_STATE;
    if (link->state == WEN_LINK_CLOSED) return WEN_OK;

    link->state = WEN_LINK_CLOSING;
    link->state = WEN_LINK_CLOSED;

    free(link->arena.base);
    link->arena.base = NULL;

    WEN_UNUSED(code);
    return WEN_OK;
}

//////////////////////////////////////////////////////////////////////////////

WENDEF wen_result wen_arena_init(wen_arena *arena, unsigned long size)
{
    if (!arena || size == 0) return WEN_ERR_STATE;

    arena->base = (unsigned char *)malloc(size);
    if (!arena->base) return WEN_ERR_IO;

    arena->capacity = size;
    arena->used = 0;

    return WEN_OK;
}

WENDEF void wen_arena_reset(wen_arena *a, wen_arena_snapshot mark)
{
    if (mark > a->used) {
        WEN_UNREACHABLE();
        return;
    }

    a->used = mark;
}

WENDEF void *wen_arena_alloc(wen_arena *a, unsigned long size)
{
    if (size == 0) return NULL;
 
    unsigned long aligned_used = WEN_ALIGN_UP(a->used, WEN_ARENA_ALIGN);
    unsigned long aligned_size = WEN_ALIGN_UP(size, WEN_ARENA_ALIGN);

    if (aligned_used > a->capacity || aligned_size > a->capacity - aligned_used)
        return NULL;

    unsigned long new_used = aligned_used + aligned_size;
    if (new_used > a->capacity) return NULL;

    a->used = new_used;
    return a->base + aligned_used;
}

WENDEF void *wen_arena_calloc(wen_arena *a, unsigned long count, unsigned long size)
{
    if (count == 0 || size == 0) return NULL;
    if (count > (~0UL / size)) return NULL;

    unsigned long total = count * size;
    void *ptr = wen_arena_alloc(a, total);
    if (!ptr) return NULL;

    memset(ptr, 0, total);
    return ptr;
}

#endif

#endif // WEN_H_
