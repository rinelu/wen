#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define C_RESET "\x1b[0m"
#define C_BOLD "\x1b[1m"
#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_CYAN "\x1b[36m"

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, C_RED C_BOLD "[FAIL]" C_RESET " %s:%d: %s\n",      \
                    __FILE__, __LINE__, #cond);                                \
            abort();                                                           \
        }                                                                      \
    } while (0)
#define WEN_IMPLEMENTATION
#include "../wen.h"

/* Self rebuild */

static bool is_newer(const char *a, const char *b) {
    struct stat sa, sb;
    if (stat(a, &sa) != 0)
        return false;
    if (stat(b, &sb) != 0)
        return true;
    return sa.st_mtime > sb.st_mtime;
}

static void rebuild_self(const char *src, const char *exe) {
    if (!is_newer(src, exe))
        return;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cc -Wall -Wextra -Werror -ggdb %s -o %s", src,
             exe);

    fprintf(stderr, C_YELLOW "[build]" C_RESET " rebuilding %s\n", exe);

    if (system(cmd) != 0) {
        fprintf(stderr, C_RED "[build] rebuild failed\n" C_RESET);
        exit(1);
    }

    execl(exe, exe, NULL);
    exit(1);
}

/* Tests */

#include "test_fake_ws.c"

/* Runner */

static int tests_run = 0;
static int tests_pass = 0;

#define RUN_TEST(fn)                                                           \
    do {                                                                       \
        clock_t _start = clock();                                              \
        printf(C_BLUE                                                          \
               "==================================================" C_RESET    \
               "\n");                                                          \
        printf(C_CYAN C_BOLD "[ RUN ]" C_RESET " %s\n", #fn);                  \
        fflush(stdout);                                                        \
                                                                               \
        fn();                                                                  \
                                                                               \
        clock_t _end = clock();                                                \
        double _ms = (double)(_end - _start) * 1000.0 / CLOCKS_PER_SEC;        \
                                                                               \
        tests_run++;                                                           \
        tests_pass++;                                                          \
        printf(C_GREEN C_BOLD "[ OK  ]" C_RESET " %s (%.2f ms)\n", #fn, _ms);  \
        printf(C_BLUE                                                          \
               "==================================================" C_RESET    \
               "\n\n");                                                        \
    } while (0)

int main(int argc, char *argv[])
{
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
