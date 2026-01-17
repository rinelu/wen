/* wen - v0.3.0 - Public Domain - https://github.com/rinelu/wen

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

          for (;;) {
              if (!wen_poll(&link, &ev)) continue;
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

   # Memory Model

     wen itself does not allocate memory dynamically by default.

     All temporary allocations use a user-owned arena (wen_arena).
     Slices returned to the user remain valid until wen_release() is called.
     Individual allocations cannot be freed; arenas are reset in bulk.
     At most one slice event may be outstanding at any time.
     The caller must call wen_release() before the next slice can be produced.
     Incoming data may be buffered until the slice is released.

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

        - WEN_NO_MALLOC      - Disable all dynamic memory usage inside wen.
        - WEN_DETERMINISTIC  - Enforce deterministic behavior. Non-deterministic code paths become unreachable.
        - WEN_ENABLE_WS      - Enable the built-in WebSocket codec.

     ## Size Limits

        - WEN_MAX_SLICE - Maximum size of a slice returned to the user.
        - WEN_RX_BUFFER - Internal receive buffer size per link.
        - WEN_TX_BUFFER - Internal transmit buffer size per link.

        These are compile-time constants and must be large enough for your protocol.
*/

#ifndef WEN_H_
#define WEN_H_

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define WEN_VMAJOR 0
#define WEN_VMINOR 3
#define WEN_VPATCH 0

// Convert version to a single integer.
// ex: 0.1.0 becomes 1000 (0 * 1000000 + 1 * 1000 + 0)
#define WEN_VNUMBER ((WEN_VMAJOR * 1000000) + (WEN_VMINOR * 1000) + WEN_VPATCH)
#define WEN__STR_HELP(...) #__VA_ARGS__
#define WEN_STR(...) WEN__STR_HELP(__VA_ARGS__)
#define WEN_VSTRING WEN_STR(WEN_VMAJOR)"." WEN_STR(WEN_VMINOR)"." WEN_STR(WEN_VPATCH)

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

// Maximum number of events that can be queued internally
#ifndef WEN_EVENT_QUEUE_CAP
#    define WEN_EVENT_QUEUE_CAP 16
#endif

#ifndef WENDEF
#    define WENDEF static inline
#endif

#ifdef WEN_WARN_DEPRECATED
#    ifndef WEN_DEPRECATED
#        if defined(__GNUC__) || defined(__clang__)
#            define WEN_DEPRECATED(message) __attribute__((deprecated(message)))
#        elif defined(_MSC_VER)
#            define WEN_DEPRECATED(message) __declspec(deprecated(message))
#        else
#            define WEN_DEPRECATED(...)
#        endif
#    endif /* WEN_DEPRECATED */
#else
#    define WEN_DEPRECATED(...)
#endif /* WEN_WARN_DEPRECATED */

#ifndef WEN_ASSERT
#   include <assert.h>
#   define WEN_ASSERT assert
#endif

#define WEN_UNUSED(x) ((void)(x))
#define WEN_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define WEN_MIN(a, b) ((a) < (b) ? (a) : (b))
#define WEN_MIN3(a,b,c) (((a)<(b)?(a):(b)) < (c) ? ((a)<(b)?(a):(b)) : (c))
#define WEN_MAX(a, b) ((a) > (b) ? (a) : (b))
#define WEN_SWAP(type, a, b) do { type _tmp = (a); (a) = (b); (b) = _tmp; } while (0)
#define WEN_TODO(message) do { fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); abort(); } while(0)
#define WEN_UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

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
#ifdef WEN_ENABLE_WS
    WEN_EV_FRAME,
    WEN_EV_PING,
    WEN_EV_PONG,
#endif // WEN_ENABLE_WS
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
    bool owns_memory;
} wen_arena;

// A snapshot of the arena state.
//
// Used to roll back temporary allocations.
typedef unsigned long wen_arena_snapshot;

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
    wen_arena_snapshot snapshot;
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

// Status returned by a protocol handshake.
typedef enum {
    WEN_HANDSHAKE_INCOMPLETE = 0,
    WEN_HANDSHAKE_COMPLETE,
    WEN_HANDSHAKE_FAILED
} wen_handshake_status;

// Interface implemented by wire-level protocols.
//
// WebSocket is provided as one such codec.
typedef struct wen_codec {
    const char *name;

    // Performs protocol-specific handshake.
    wen_handshake_status (*handshake)(void *codec_state,
                                      const void *in, unsigned long in_len, 
                                      unsigned long *consumed, void *out,
                                      unsigned long out_cap, unsigned long *out_len);

    // Consumes raw bytes and produces events.
    //
    // NOTE: decode() must NOT consume or assume ownership of input bytes.
    // The input buffer remains owned by wen and will only be advanced
    // when a slice is emitted.
    wen_result (*decode)(void *codec_state, const void *data, unsigned long len);

    // Encodes an outgoing message or control frame.
    wen_result (*encode)(void *codec_state, unsigned opcode, const void *data, unsigned long len,
                         void *out, unsigned long out_cap, unsigned long *out_len);
} wen_codec;

// Fixed-capacity ring buffer for queued events.
typedef struct {
    wen_event q[WEN_EVENT_QUEUE_CAP];
    unsigned head;
    unsigned tail;
} wen_event_queue;

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

    // bytes of current frame
    unsigned long frame_len;

    const wen_codec *codec;
    void *codec_state;

    void *user_data;
    wen_event_queue evq;
    wen_arena arena;

    bool slice_outstanding;
    bool close_queued;
} wen_link;

// WebSocket protocol GUID used during the handshake.
#ifdef WEN_ENABLE_WS
#    define WEN_WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// WebSocket opcodes.
#    define WEN_WS_OP_CONT 0x0
#    define WEN_WS_OP_TEXT 0x1
#    define WEN_WS_OP_BINARY 0x2
#    define WEN_WS_OP_CLOSE 0x8
#    define WEN_WS_OP_PING 0x9
#    define WEN_WS_OP_PONG 0xA
#endif // WEN_ENABLE_WS

// Initializes a link with the given IO backend.
WENDEF wen_result wen_link_init(wen_link *link, wen_io io);

// Attaches a codec to the link.
//
// Must be called before polling.
WENDEF void wen_link_attach_codec(wen_link *link, const wen_codec *codec, void *codec_state);

// Polls for the next available event.
//
// Returns true if an event was produced.
// When WEN_NO_MALLOC is enabled, the user must call wen_arena_bind() before polling.
WENDEF bool wen_poll(wen_link *link, wen_event *ev);

WENDEF unsigned wen__poll_flush_tx(wen_link *link, wen_event *ev);
WENDEF unsigned wen__poll_read_rx(wen_link *link, wen_event *ev);
WENDEF bool wen__poll_handshake(wen_link *link, wen_event *ev);
WENDEF bool wen__poll_decode(wen_link *link, wen_event *ev);

// Releases a slice previously returned by wen_poll().
WENDEF void wen_release(wen_link *link, wen_slice slice);

// Sends an application message using the active codec.
WENDEF wen_result wen_send(wen_link *link, unsigned opcode, const void *data, unsigned long len);

// Initiates a clean protocol-level close.
WENDEF wen_result wen_close(wen_link *link, unsigned code, unsigned opcode);

// Clears internal RX and TX buffer lengths without touching memory
WENDEF void wen_link_reset_buffers(wen_link *link);

// Pushes an event onto the event queue.
//
// Returns true on success, zero if the false is full.
WENDEF bool wen_evq_push(wen_event_queue *q, const wen_event *ev);

// Pops the next event from the event queue.
//
// Returns true if an event was returned, false if the queue is empty.
WENDEF bool wen_evq_pop(wen_event_queue *q, wen_event *ev);

#define WEN_STATIC_ASSERT(cond, name) typedef char wen_static_assert_##name[(cond) ? 1 : -1]

WEN_STATIC_ASSERT(WEN_RX_BUFFER >= 1024, rx_buffer_too_small);
WEN_STATIC_ASSERT(WEN_TX_BUFFER >= 1024, tx_buffer_too_small);

//////////////////////////////////////////////////////////////////////////////

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

#ifdef WEN_IMPLEMENTATION

WENDEF void wen_link_reset_buffers(wen_link *link)
{
    link->rx_len = 0;
    link->tx_len = 0;
}

WENDEF wen_result wen_link_init(wen_link *link, wen_io io) {
    if (!link || !io.read || !io.write) return WEN_ERR_STATE;

    WEN_ASSERT(io.read != NULL && io.write != NULL);

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
    WEN_ASSERT(codec->handshake && "codec must provide handshake()");

    link->codec       = codec;
    link->codec_state = codec_state;
    link->state       = WEN_LINK_HANDSHAKE;
}

WENDEF bool wen_poll(wen_link *link, wen_event *ev)
{
    if (!link || !ev) return false;

    // The event queue has priority.
    // If something was already generated on a previous call, return it before doing any I/O.
    if (wen_evq_pop(&link->evq, ev)) {
        if (ev->type == WEN_EV_CLOSE && link->state != WEN_LINK_CLOSED) {
            link->state = WEN_LINK_CLOSED;
            link->close_queued = false;

#ifndef WEN_NO_MALLOC
            if (link->arena.owns_memory && link->arena.base)
                free(link->arena.base);
#endif

            link->arena.base = NULL;
        }
        return true;
    }

    if (link->state == WEN_LINK_CLOSED) return false;

    if (!link->codec) {
        ev->type = WEN_EV_ERROR;
        ev->as.error = WEN_ERR_UNSUPPORTED;
        return true;
    }

    // Flush pending TX
    unsigned tx_err = wen__poll_flush_tx(link, ev);
    if (tx_err != (unsigned)-1) return tx_err;

    // Single RX read
    unsigned rx_err = wen__poll_read_rx(link, ev);
    if (rx_err != (unsigned)-1) return rx_err;

    if (link->state == WEN_LINK_HANDSHAKE) 
        return wen__poll_handshake(link, ev);

    return wen__poll_decode(link, ev);
}

WENDEF unsigned wen__poll_flush_tx(wen_link *link, wen_event *ev)
{
    if (link->tx_len == 0) return -1;

    long nw = link->io.write(link->io.user, link->tx_buf, link->tx_len);
    if (nw < 0) {
        ev->type = WEN_EV_ERROR;
        ev->as.error = WEN_ERR_IO;
        return true;
    }
    if ((unsigned long)nw < link->tx_len) {
        memmove(link->tx_buf, link->tx_buf + nw, link->tx_len - (unsigned long)nw);
        link->tx_len -= (unsigned long)nw;
    } else {
        link->tx_len = 0;
    }
    if (!link->close_queued && link->state >= WEN_LINK_CLOSING && !link->slice_outstanding) {
        wen_event cev = { .type = WEN_EV_CLOSE };
        wen_evq_push(&link->evq, &cev);
        link->close_queued = true;
    }
    return false;
}

WENDEF unsigned wen__poll_read_rx(wen_link *link, wen_event *ev)
{
    if (link->rx_len < WEN_RX_BUFFER) {
        long nread = link->io.read(link->io.user, link->rx_buf + link->rx_len, WEN_RX_BUFFER - link->rx_len);

        if (nread < 0) {
            ev->type = WEN_EV_ERROR;
            ev->as.error = WEN_ERR_IO;
            return true;
        }

        if (nread == 0) {
            if (link->state < WEN_LINK_CLOSING)
                link->state = WEN_LINK_CLOSING;

            if (!link->close_queued && !link->slice_outstanding) {
                wen_event cev = { .type = WEN_EV_CLOSE };
                wen_evq_push(&link->evq, &cev);
                link->close_queued = true;
            }

            return false;
        }

        link->rx_len += (unsigned long)nread;
    }
    return -1;
}

WENDEF bool wen__poll_handshake(wen_link *link, wen_event *ev)
{
    unsigned long consumed = 0;
    unsigned long out_len = 0;

    wen_handshake_status hs =
        link->codec->handshake(
            link->codec_state,
            link->rx_buf, link->rx_len,
            &consumed,
            link->tx_buf, WEN_TX_BUFFER,
            &out_len);

    if (out_len) link->tx_len = out_len;

    memmove(link->rx_buf, link->rx_buf + consumed, link->rx_len - consumed);
    link->rx_len -= consumed;

    if (hs == WEN_HANDSHAKE_COMPLETE) {
        link->state = WEN_LINK_OPEN;
        ev->type = WEN_EV_OPEN;
        return true;
    }

    if (hs == WEN_HANDSHAKE_FAILED) {
        ev->type = WEN_EV_ERROR;
        ev->as.error = WEN_ERR_PROTOCOL;
        return true;
    }

    return false;
}

WENDEF bool wen__poll_decode(wen_link *link, wen_event *ev)
{
    unsigned long slice_length =
        link->frame_len ? WEN_MIN(link->frame_len, WEN_MAX_SLICE) : WEN_MIN(link->rx_len, WEN_MAX_SLICE);

    // Decode is codec-specific and opaque
    if (link->codec->decode) {
        wen_result r = link->codec->decode(link->codec_state, link->rx_buf, slice_length);
        if (r != WEN_OK) {
            ev->type = WEN_EV_ERROR;
            ev->as.error = r;
            return true;
        }
    }

    slice_length = WEN_MIN3(slice_length, WEN_MAX_SLICE, link->rx_len);

    // Create slice event
    if (slice_length == 0) return false;
    WEN_ASSERT(!link->slice_outstanding && "wen_poll: previous slice not released");
    wen_arena_snapshot snap = link->arena.used;
    void *dst = wen_arena_alloc(&link->arena, slice_length);
    if (!dst) {
        ev->type = WEN_EV_ERROR;
        ev->as.error = WEN_ERR_OVERFLOW;
        return true;
    }

    memcpy(dst, link->rx_buf, slice_length);

    wen_event sev = {
        .type              = WEN_EV_SLICE,
        .as.slice.data     = dst,
        .as.slice.len      = slice_length,
        .as.slice.flags    = WEN_SLICE_BEGIN | WEN_SLICE_END,
        .as.slice.snapshot = snap,
    };

    // Enqueue event
    if (!wen_evq_push(&link->evq, &sev)) {
        wen_arena_reset(&link->arena, snap);
        ev->type = WEN_EV_ERROR;
        ev->as.error = WEN_ERR_OVERFLOW;
        return true;
    }

    memmove(link->rx_buf,
            link->rx_buf + slice_length,
            link->rx_len - slice_length);
    link->rx_len -= slice_length;
    link->slice_outstanding = true;

    *ev = sev;

    if (link->frame_len) link->frame_len -= slice_length;
    return false;
}

WENDEF void wen_release(wen_link *link, wen_slice slice)
{
    WEN_ASSERT(link && "wen_release: link is NULL");
    WEN_ASSERT(link->slice_outstanding && "wen_release called with no outstanding slice");

    wen_arena_reset(&link->arena, slice.snapshot);
    link->slice_outstanding = false;
}

WENDEF wen_result wen_send(wen_link *link, unsigned opcode, const void *data, unsigned long len)
{
    if (!link || !link->codec) return WEN_ERR_STATE;
    if (!link->codec->encode)  return WEN_ERR_UNSUPPORTED;
    if (link->tx_len >= WEN_TX_BUFFER) return WEN_ERR_OVERFLOW;

    unsigned long out_len = 0;

    wen_result r = link->codec->encode(
        link->codec_state,
        opcode,
        data,
        len,
        link->tx_buf + link->tx_len,
        WEN_TX_BUFFER - link->tx_len,
        &out_len);

    if (r != WEN_OK) return r;

    link->tx_len += out_len;
    return WEN_OK;
}

WENDEF wen_result wen_close(wen_link *link, unsigned code, unsigned opcode)
{
    if (!link) return WEN_ERR_STATE;
    if (link->state >= WEN_LINK_CLOSED) return WEN_OK;
    if (link->tx_len != 0) return WEN_ERR_STATE;

    link->state = WEN_LINK_CLOSING;
    if (link->codec && link->codec->encode) {
        unsigned long out_len = 0;
        if (link->codec->encode(
                link->codec_state,
                opcode,
                &code, sizeof(code),
                link->tx_buf, WEN_TX_BUFFER,
                &out_len) == WEN_OK) {
            link->tx_len = out_len;
        }
    }

    return WEN_OK;
}

WENDEF bool wen_evq_push(wen_event_queue *q, const wen_event *ev)
{
    unsigned next = (q->tail + 1) % WEN_EVENT_QUEUE_CAP;
    if (next == q->head) return 0;
    q->q[q->tail] = *ev;
    q->tail = next;
    return 1;
}

WENDEF bool wen_evq_pop(wen_event_queue *q, wen_event *ev)
{
    if (q->head == q->tail) return 0;
    *ev = q->q[q->head];
    q->head = (q->head + 1) % WEN_EVENT_QUEUE_CAP;
    return 1;
}

//////////////////////////////////////////////////////////////////////////////

WENDEF wen_result wen_arena_init(wen_arena *arena, unsigned long size)
{
    if (!arena || size == 0) return WEN_ERR_STATE;

#ifndef WEN_NO_MALLOC
    arena->base = (unsigned char *)malloc(size);
    if (!arena->base) return WEN_ERR_IO;
    arena->owns_memory = true;
#endif
    arena->capacity = size;
    arena->used = 0;

    return WEN_OK;
}

WENDEF void wen_arena_bind(wen_arena *arena, void *mem, unsigned long size)
{
    arena->base = (unsigned char *)mem;
    arena->capacity = size;
    arena->used = 0;
}

WENDEF void wen_arena_reset(wen_arena *a, wen_arena_snapshot mark)
{
    WEN_ASSERT(mark <= a->used && "wen_arena_reset: invalid snapshot");
    if (mark > a->used) {
        WEN_UNREACHABLE("wen_arena_reset");
        return;
    }

    a->used = mark;
}

WENDEF void *wen_arena_alloc(wen_arena *a, unsigned long size)
{
    if (!a || !a->base || size == 0) return NULL;
 
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

#endif // WEN_IMPLEMENTATION

#ifdef WEN_ENABLE_WS

// static const wen_codec ws_ws_codec = {
//     .name = "wen-ws",
//     .handshake = wen_ws_handshake,
//     .decode = wen_ws_decode,
//     .encode = wen_ws_encode,
// };

#endif // WEN_ENABLE_WS

#endif // WEN_H_
