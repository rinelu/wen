#ifdef TEST

static void test_remote_close_generates_event_once(void)
{
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    wen_io io = {.user=&fio, .read=fake_read, .write=fake_write};
    ASSERT(wen_link_init(&link, io) == WEN_OK);
    wen_link_attach_codec(&link, &null_codec, NULL);

    while (!wen_poll(&link, &ev));
    ASSERT(ev.type == WEN_EV_OPEN);

    fake_close(&fio);

    while (!wen_poll(&link, &ev));
    ASSERT(ev.type == WEN_EV_CLOSE);

    // No duplicate close
    ASSERT(!wen_poll(&link, &ev));
}

#endif /* ifdef TEST */
