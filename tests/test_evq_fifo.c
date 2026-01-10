#ifdef TEST
static void test_event_queue_fifo(void)
{
    wen_event_queue q = {0};
    wen_event ev;

    for (int i = 0; i < WEN_EVENT_QUEUE_CAP - 1; i++) {
        wen_event e = {.type = WEN_EV_OPEN};
        ASSERT(wen_evq_push(&q, &e));
    }

    // Queue should now be full
    wen_event e = {.type = WEN_EV_CLOSE};
    ASSERT(!wen_evq_push(&q, &e));

    for (int i = 0; i < WEN_EVENT_QUEUE_CAP - 1; i++) {
        ASSERT(wen_evq_pop(&q, &ev));
        ASSERT(ev.type == WEN_EV_OPEN);
    }

    ASSERT(!wen_evq_pop(&q, &ev));
}

#endif // TEST
