#ifdef  TEST

static void test_slice_size_limit(void)
{
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    char big[WEN_MAX_SLICE + 10];
    memset(big, 'a', sizeof(big));

    wen_io io = {.user=&fio, .read=fake_read, .write=fake_write};
    ASSERT(wen_link_init(&link, io) == WEN_OK);
    wen_link_attach_codec(&link, &fake_codec, NULL);


while (true) {
    if (!wen_poll(&link, &ev)) continue;

    if (ev.type == WEN_EV_SLICE) {
        ASSERT(ev.as.slice.len == WEN_MAX_SLICE);
        wen_release(&link, ev.as.slice);
        break;
    }
    if (ev.type == WEN_EV_ERROR) {
        break;
    }
    if (ev.type == WEN_EV_CLOSE) {
        break;
    }
}
}

#endif // TEST
