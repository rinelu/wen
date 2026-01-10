#ifdef TEST
typedef struct {
    unsigned char in[1024];
    unsigned long in_len;
    unsigned long in_pos;

    unsigned char out[1024];
    unsigned long out_len;

    int closed;
    int handshake_kick;
} fake_io;

static long fake_read(void *user, void *buf, unsigned long len)
{
    fake_io *io = user;
    if (io->closed) return 0; // EOF

    if (!io->handshake_kick) {
        io->handshake_kick = 1;
        ((unsigned char *)buf)[0] = 0;
        return 1;
    }

    unsigned long remaining = io->in_len - io->in_pos;
    if (remaining == 0) return 0;

    unsigned long n = WEN_MIN(len, remaining);
    memcpy(buf, io->in + io->in_pos, n);
    io->in_pos += n;

    return (long)n;
}

static long fake_write(void *user, const void *buf, unsigned long len)
{
    fake_io *io = user;
    if (io->closed) return -1;
    ASSERTN(len + io->out_len <= sizeof(io->out));

    memcpy(io->out + io->out_len, buf, len);
    io->out_len += len;
    return (long)len;
}

static wen_handshake_status fake_handshake(void *codec_state, const void *in, unsigned long in_len,
                                           unsigned long *consumed,
                                           void *out, unsigned long out_cap, unsigned long *out_len)
{
    WEN_UNUSED(codec_state);
    WEN_UNUSED(in);
    WEN_UNUSED(out);
    WEN_UNUSED(out_cap);

    if (in_len == 0) {
        *consumed = 0;
        *out_len = 0;
        return WEN_HANDSHAKE_INCOMPLETE;
    }

    *consumed = in_len;
    *out_len = 0;
    return WEN_HANDSHAKE_COMPLETE;
}


static void fake_feed(fake_io *io, unsigned opcode, const unsigned char *payload, unsigned long len)
{
    /* ASSERT(len <= 125);                        // only simple frames for test */
    io->in[io->in_len++] = 0x80 | opcode;      // FIN=1 + opcode
    io->in[io->in_len++] = (unsigned char)len; // no mask
    memcpy(io->in + io->in_len, payload, len);
    io->in_len += len;
}

static void fake_close(fake_io *io)
{
    io->closed = 1;
}

static wen_result fake_decode(void *state, const void *data, unsigned long len) {
    WEN_UNUSED(state);
    WEN_UNUSED(data);
    WEN_UNUSED(len);
    return WEN_OK;
}

static wen_result fake_encode(void *codec_state, unsigned opcode, const void *data, unsigned long len,
                              void *out, unsigned long out_cap, unsigned long *out_len) {
    WEN_UNUSED(codec_state);
    if (len > 125) return WEN_ERR_IO;
    unsigned char *b = out;
    b[0] = 0x80 | (unsigned char)opcode;
    b[1] = (unsigned char)len;
    memcpy(b + 2, data, len);
    *out_len = 2 + len;
    return WEN_OK;
}

static const wen_codec fake_codec = {
    .name = "fake",
    .handshake = fake_handshake,
    .decode = fake_decode,
    .encode = fake_encode
};

static void test_fake_ws(void)
{
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    wen_io io = {
        .user  = &fio,
        .read  = fake_read,
        .write = fake_write
    };

    ASSERT(wen_link_init(&link, io) == WEN_OK);
    printf("Link initialized.\n");

    wen_link_attach_codec(&link, &fake_codec, NULL);
    printf("Codec attached.\n");

    while (!wen_poll(&link, &ev));

    ASSERT(ev.type == WEN_EV_OPEN);
    printf("Connection opened.\n");

    fake_feed(&fio, WEN_WS_OP_TEXT, (unsigned char *)"hello", 5);
    while (!wen_poll(&link, &ev));

    ASSERT(ev.type == WEN_EV_SLICE);
    printf("Received slice: %.*s\n",
           (int)ev.as.slice.len,
           (const char *)ev.as.slice.data);

    wen_release(&link, ev.as.slice);
    fake_close(&fio);

    for (;;) {
        if (!wen_poll(&link, &ev)) continue;
        if (ev.type == WEN_EV_CLOSE) break;
    }
    ASSERT(ev.type == WEN_EV_CLOSE);

    printf("Connection closed.\n");

    ASSERT(wen_close(&link, 1000, WEN_WS_OP_CLOSE) == WEN_OK);
    printf("Link shutdown complete.\n");
}
#endif // TEST
