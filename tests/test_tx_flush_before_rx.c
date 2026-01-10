#ifdef TEST

static void test_tx_flush_before_rx(void)
{
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    wen_io io = {.user=&fio, .read=fake_read, .write=fake_write};
    ASSERT(wen_link_init(&link, io) == WEN_OK);
    wen_link_attach_codec(&link, &null_codec, NULL);

    while (!wen_poll(&link, &ev)); // open

    ASSERT(wen_send(&link, 1, "x", 1) == WEN_OK);
    ASSERT(link.tx_len == 0); // null_encode writes nothing

    // Poll should not read new RX while TX pending
    ASSERT(!wen_poll(&link, &ev));
}

#endif /* ifdef  TEST */
