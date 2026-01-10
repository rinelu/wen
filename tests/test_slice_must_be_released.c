#ifdef TEST
static void test_slice_must_be_released(void)
{
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    wen_io io = {.user=&fio, .read=fake_read, .write=fake_write};
    ASSERT(wen_link_init(&link, io) == WEN_OK);
    wen_link_attach_codec(&link, &null_codec, NULL);

    while (!wen_poll(&link, &ev));
    ASSERT(ev.type == WEN_EV_OPEN);

    fake_feed(&fio, "abc", 3);
    while (!wen_poll(&link, &ev));
    ASSERT(ev.type == WEN_EV_SLICE);

    // DO NOT release slice
    fake_feed(&fio, "def", 3);

    // This should assert internally
    (void)wen_poll(&link, &ev);
}
#endif
