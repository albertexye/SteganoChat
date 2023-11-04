#ifndef C_EXTENSION_TESTING_H
#define C_EXTENSION_TESTING_H

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#define RES "\x1B[0m"
#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define CYN "\x1B[36m"

#define VA_ARGS(...) , ##__VA_ARGS__
#define TestLog(format, ...) printf("\t" CYN "[LOG] %s: %d: " format RES "\n", __func__, __LINE__ VA_ARGS(__VA_ARGS__))
#define TestFatal(format, ...) printf("\t" RED "[FAT] %s: %d: " format RES "\n" , __func__, __LINE__ VA_ARGS(__VA_ARGS__)); return false
#define TestError(format, ...) printf("\t" YEL "[ERR] %s: %d: " format RES "\n", __func__, __LINE__ VA_ARGS(__VA_ARGS__))
#define TestAssert(condition) if (!condition) { TestFatal("Assert Error: %s is false", #condition); }
#define TestAssertNot(condition) if (!condition) { TestFatal("AssertNot Error: %s is true", #condition); }
#define TestEqual(a, b) if (a != b) { TestFatal("Equal Error: %s != %s", #a, #b); }
#define TestUnequal(a, b) if (a == b) { TestFatal("Equal Error: %s == %s", #a, #b); }

void test(const char *name, bool (*func)());

#endif //C_EXTENSION_TESTING_H
