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
    wen_link_attach_codec(&link, &null_codec, NULL);

    while (!wen_poll(&link, &ev));
    fake_feed(&fio, big, sizeof(big));

    while (!wen_poll(&link, &ev));
    ASSERT(ev.type == WEN_EV_SLICE);
    ASSERT(ev.as.slice.len == WEN_MAX_SLICE);
}

#endif /* ifdef  TEST */
