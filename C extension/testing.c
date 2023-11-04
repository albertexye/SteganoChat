#include "testing.h"

#include <inttypes.h>
#include <stdlib.h>

static uint64_t FAILURE = 0;
static uint64_t SUCCESS = 0;

void summary() {
    if (FAILURE != 0)
        printf(YEL "%llu TEST(S), %llu SUCCEEDED, %llu FAILED" RES "\n", FAILURE+SUCCESS, SUCCESS, FAILURE);
    else
        printf(GRN "%llu TEST(S), %llu SUCCEEDED, %llu FAILED" RES "\n", FAILURE+SUCCESS, SUCCESS, FAILURE);
}

void test(const char *const name, bool (*func)()) {
    static bool registered = false;
    if (!registered) {
        registered = true;
        atexit(summary);
    }
    printf("RUNNING '%s'\n", name);
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    bool result = func();
    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t time_took = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    if (result) {
        printf(GRN "PASSED '%s', %lldus" RES "\n\n", name, time_took);
        SUCCESS++;
    } else {
        printf(RED "FAILED '%s', %lldus" RES "\n\n", name, time_took);
        FAILURE++;
    }
}
