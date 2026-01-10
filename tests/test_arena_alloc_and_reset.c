#ifdef TEST
static void test_arena_alloc_and_reset(void)
{
    wen_arena a;
    ASSERT(wen_arena_init(&a, 64) == WEN_OK);

    void *p1 = wen_arena_alloc(&a, 16);
    ASSERT(p1);

    wen_arena_snapshot snap = a.used;

    void *p2 = wen_arena_alloc(&a, 16);
    ASSERT(p2);

    wen_arena_reset(&a, snap);

    void *p3 = wen_arena_alloc(&a, 16);
    ASSERT(p3);
    ASSERT(p3 == p2); // memory reused
}
#endif // TEST
