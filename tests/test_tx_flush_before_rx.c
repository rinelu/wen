#ifdef TEST

static void test_tx_flush_before_rx(void)
{
    fake_io fio = {0};
    wen_link link;
    wen_event ev;

    wen_io io = {.user=&fio, .read=fake_read, .write=fake_write};
    ASSERT(wen_link_init(&link, io) == WEN_OK);
    wen_link_attach_codec(&link, &fake_codec, NULL);

    while (!wen_poll(&link, &ev)); // handshake complete, EV_OPEN

    // Send a message
    ASSERT(wen_send(&link, 1, "x", 1) == WEN_OK);

    // TX buffer should be non-empty
    ASSERT(link.tx_len > 0);

    // Poll should flush TX first, not read RX yet
    ASSERT(!wen_poll(&link, &ev));
    ASSERT(link.tx_len == 0);      // TX flushed
}


#endif /* ifdef  TEST */
