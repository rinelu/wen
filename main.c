#include <assert.h>
#include <stdio.h>

#define WEN_IMPLEMENTATION
#include "wen.h"

typedef struct {
    unsigned char in[1024];
    unsigned long in_len;
    unsigned long in_pos;

    unsigned char out[1024];
    unsigned long out_len;
} fake_io;

static long fake_read(void *user, void *buf, unsigned long len) {
    fake_io *io = user;

    unsigned long remaining = io->in_len - io->in_pos;
    unsigned long n = WEN_MIN(len, remaining);

    if (n == 0)
        return 0;

    memcpy(buf, io->in + io->in_pos, n);
    io->in_pos += n;
    return (long)n;
}

static long fake_write(void *user, const void *buf, unsigned long len) {
    fake_io *io = user;

    if (len > sizeof(io->out) - io->out_len)
        return -1;

    memcpy(io->out + io->out_len, buf, len);
    io->out_len += len;
    return (long)len;
}

static wen_result null_handshake(void *state, const void *in, unsigned long in_len, void *out, unsigned long out_cap, unsigned long *out_len) {
    WEN_UNUSED(state);
    WEN_UNUSED(in);
    WEN_UNUSED(in_len);
    WEN_UNUSED(out);
    WEN_UNUSED(out_cap);

    *out_len = 0;
    return WEN_OK;
}

static wen_result null_decode(void *state, const void *data, unsigned long len) {
    WEN_UNUSED(state);
    WEN_UNUSED(data);
    WEN_UNUSED(len);
    return WEN_OK;
}

static wen_result null_encode(void *state, unsigned opcode, const void *data, unsigned long len) {
    WEN_UNUSED(state);
    WEN_UNUSED(opcode);
    WEN_UNUSED(data);
    WEN_UNUSED(len);
    return WEN_OK;
}

static const wen_codec null_codec = {.name = "null", .handshake = null_handshake, .decode = null_decode, .encode = null_encode};

int main(void) {
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    wen_io io = {.user = &fio, .read = fake_read, .write = fake_write};

    assert(wen_link_init(&link, io) == WEN_OK);
    printf("Link initialized.\n");

    wen_link_attach_codec(&link, &null_codec, NULL);
    printf("Codec attached.\n");

    assert(wen_poll(&link, &ev) == 1);
    assert(ev.type == WEN_EV_OPEN);
    printf("Connection opened: Event type = %d\n", ev.type);

    // Setup input data
    memcpy(fio.in, "hello", 5);
    fio.in_len = 5;

    // First poll after putting data in
    assert(wen_poll(&link, &ev) >= 0);
    printf("First poll completed.\n");

    // Close the link and check for success
    assert(wen_close(&link, 1000) == WEN_OK);
    printf("Link closed.\n");

    return 0;
}
