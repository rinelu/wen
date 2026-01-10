#ifdef TEST
static wen_result fail_decode(void *s, const void *d, unsigned long l) {
    WEN_UNUSED(s); WEN_UNUSED(d); WEN_UNUSED(l);
    return WEN_ERR_PROTOCOL;
}

static const wen_codec fail_codec = {
    .name="fail",
    .handshake=null_handshake,
    .decode=fail_decode,
    .encode=null_encode
};

static void test_decode_error_becomes_event(void)
{
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    wen_io io = {.user=&fio, .read=fake_read, .write=fake_write};
    ASSERT(wen_link_init(&link, io) == WEN_OK);
    wen_link_attach_codec(&link, &fail_codec, NULL);

    while (!wen_poll(&link, &ev));
    ASSERT(ev.type == WEN_EV_OPEN);

    fake_feed(&fio, "x", 1);
    while (!wen_poll(&link, &ev));
    ASSERT(ev.type == WEN_EV_ERROR);
    ASSERT(ev.as.error == WEN_ERR_PROTOCOL);
}
#endif /* ifdef MACRO */
