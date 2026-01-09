#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static bool is_newer(const char *a, const char *b)
{
    struct stat sa, sb;
    if (stat(a, &sa) != 0) return false;
    if (stat(b, &sb) != 0) return true;
    return sa.st_mtime > sb.st_mtime;
}

static void rebuild_self(const char *src, const char *exe)
{
    if (!is_newer(src, exe)) return;

    printf("[build] rebuilding %s\n", exe);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cc %s -o %s", src, exe);

    if (system(cmd) != 0) {
        fprintf(stderr, "[build] rebuild failed\n");
        exit(1);
    }

    execl(exe, exe, NULL);
    perror("[build] execl failed");
    exit(1);
}

static void cmd_run(const char *cmd)
{
    printf("[cmd] %s\n", cmd);
    if (system(cmd) != 0) {
        fprintf(stderr, "[cmd] failed\n");
        exit(1);
    }
}

int main(void)
{
    rebuild_self("build.c", "./build");

    cmd_run("cc -Wall -Wextra -Werror -ggdb -o main main.c");
    cmd_run("./main");

    return 0;
}
