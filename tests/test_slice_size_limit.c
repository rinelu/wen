#ifdef TEST

static void test_slice_size_limit(void)
{
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    char big[WEN_MAX_SLICE + 10];
    memset(big, 'a', sizeof(big));
    fake_feed(&fio, WEN_WS_OP_BINARY, (unsigned char *)big, sizeof(big));

    wen_io io = {.user=&fio, .read=fake_read, .write=fake_write};
    ASSERT(wen_link_init(&link, io) == WEN_OK);
    wen_link_attach_codec(&link, &fake_codec, NULL);

    bool got_slice = false;

    while (true) {
        if (!wen_poll(&link, &ev)) continue;

        if (ev.type == WEN_EV_SLICE) {
            ASSERT(ev.as.slice.len == WEN_MAX_SLICE);
            wen_release(&link, ev.as.slice);
            got_slice = true;
            break;
        }

        if (ev.type == WEN_EV_ERROR || ev.type == WEN_EV_CLOSE)
            break;
    }

    ASSERT(got_slice);
    ASSERT(wen_close(&link, 1000, WEN_WS_OP_CLOSE) == WEN_OK);
}

#endif // TEST
