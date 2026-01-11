#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32)
#    include <process.h>
#    define test_execl(path) _execl((path), (path), NULL)
#else
#    include <unistd.h>
#    define test_execl(path) execl((path), (path), NULL)
#endif

#define C_RESET "\x1b[0m"
#define C_BOLD "\x1b[1m"
#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_CYAN "\x1b[36m"

static int current_test_failed = 0;

#define TEST_FAIL(msg)                                                         \
    do {                                                                       \
        fprintf(stderr, C_RED C_BOLD "FAIL   " C_RESET " %s:%d: %s\n",         \
                __FILE__, __LINE__, msg);                                      \
        current_test_failed = 1;                                               \
        return;                                                                \
    } while (0)

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr,                                                    \
                    C_RED C_BOLD "ASSERT " C_RESET                             \
                                 " %s:%d: assertion failed: %s\n",             \
                    __FILE__, __LINE__, #cond);                                \
            current_test_failed = 1;                                           \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERTN(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr,                                                    \
                    C_RED C_BOLD "ASSERT " C_RESET                             \
                                 " %s:%d: assertion failed: %s\n",             \
                    __FILE__, __LINE__, #cond);                                \
            current_test_failed = 1;                                           \
            return -1;                                                         \
        }                                                                      \
    } while (0)

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr,                                                    \
                    C_YELLOW C_BOLD "WARN   " C_RESET                          \
                                    " %s:%d: expectation failed: %s\n",        \
                    __FILE__, __LINE__, #cond);                                \
            current_test_failed = 1;                                           \
        }                                                                      \
    } while (0)

#define WEN_ASSERT(cond)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, C_YELLOW C_BOLD "ASSERT " C_RESET " %s:%d: %s\n",  \
                    __FILE__, __LINE__, #cond);                                \
        }                                                                      \
    } while (0)
#define WEN_IMPLEMENTATION
#include "../wen.h"

/* Self rebuild */

static void rebuild_self(const char *src, const char *exe)
{
    if (getenv("SELF_REBUILT")) return;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "cc -Wall -Wextra -Werror -Wno-unused -ggdb %s -o %s",
             src, exe);

    fprintf(stderr, C_YELLOW "[build]" C_RESET " rebuilding %s\n", exe);

    if (system(cmd) != 0) {
        fprintf(stderr, C_RED "[build] rebuild failed\n" C_RESET);
        exit(1);
    }

    setenv("SELF_REBUILT", "1", 1);
    test_execl(exe);
    perror("execl");
    exit(1);
}

/* Tests */
#define TEST
#include "test_fake_ws.c"
#include "test_arena_alloc_and_reset.c"
#include "test_decode_error_becomes_event.c"
#include "test_evq_fifo.c"
#include "test_slice_must_be_released.c"
#include "test_remote_close_generates_event_once.c"
#include "test_tx_flush_before_rx.c"
#include "test_slice_size_limit.c"

/* Runner */

static int tests_run = 0;
static int tests_pass = 0;

#define RUN_TEST(fn)                                                           \
    do {                                                                       \
        clock_t _start = clock();                                              \
        current_test_failed = 0;                                               \
                                                                               \
        printf(C_CYAN C_BOLD "RUN    " C_RESET " %s\n", #fn);                  \
        fflush(stdout);                                                        \
                                                                               \
        fn();                                                                  \
                                                                               \
        clock_t _end = clock();                                                \
        double _ms = (double)(_end - _start) * 1000.0 / CLOCKS_PER_SEC;        \
                                                                               \
        tests_run++;                                                           \
        if (current_test_failed) {                                             \
            printf(C_RED C_BOLD "FAIL   " C_RESET " %s (%.2f ms)\n", #fn,      \
                   _ms);                                                       \
        } else {                                                               \
            tests_pass++;                                                      \
            printf(C_GREEN C_BOLD "OK     " C_RESET " %s (%.2f ms)\n", #fn,    \
                   _ms);                                                       \
        }                                                                      \
                                                                               \
        printf(C_BLUE "\n");                                                   \
    } while (0)

int main(int, char *argv[]) {
    rebuild_self(__FILE__, argv[0]);

    printf(C_BOLD "wen test runner\n" C_RESET);
    printf(C_BLUE "--------------------------------------------------" C_RESET
                  "\n");
    printf("WEN version: %s\n", WEN_VSTRING);
    printf("RX buffer:   %d\n", WEN_RX_BUFFER);
    printf("TX buffer:   %d\n", WEN_TX_BUFFER);
    printf("Event cap:   %d\n", WEN_EVENT_QUEUE_CAP);
#ifdef WEN_NO_MALLOC
    printf("Allocator:   user-supplied (WEN_NO_MALLOC)\n");
#else
    printf("Allocator:   malloc\n");
#endif
    printf(C_BLUE "--------------------------------------------------" C_RESET
                  "\n\n");

    RUN_TEST(test_fake_ws);
    RUN_TEST(test_arena_alloc_and_reset);
    RUN_TEST(test_decode_error_becomes_event);
    RUN_TEST(test_event_queue_fifo);
    RUN_TEST(test_slice_must_be_released);
    RUN_TEST(test_remote_close_generates_event_once);
    RUN_TEST(test_tx_flush_before_rx);
    RUN_TEST(test_slice_size_limit);

    printf(C_BOLD "Summary:\n" C_RESET);
    printf("  Tests run:    %d\n", tests_run);
    printf("  Tests passed: %d\n", tests_pass);
    printf("  Tests failed: %d\n", tests_run - tests_pass);
    printf(C_BLUE "--------------------------------------------------" C_RESET
                  "\n");

    if (tests_run == tests_pass)
        printf(C_GREEN C_BOLD "ALL TESTS PASSED\n" C_RESET);
    else
        printf(C_RED C_BOLD "TESTS FAILED\n" C_RESET);

    return tests_run == tests_pass ? 0 : 1;
}
