#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

static int g_fail = 0;

#define CHECK(cond) do { if (!(cond)) { \
  fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

#define CHECK_EQ_INT(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
  fprintf(stderr, "FAIL %s:%d  %s == %s   (%ld != %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); \
  g_fail++; } } while (0)

#define CHECK_NEAR(a, b, eps) do { double _a = (double)(a), _b = (double)(b); if (fabs(_a - _b) > (eps)) { \
  fprintf(stderr, "FAIL %s:%d  %s ~= %s   (%f vs %f)\n", __FILE__, __LINE__, #a, #b, _a, _b); \
  g_fail++; } } while (0)

static void test_harness_self(void) {
  CHECK(1 + 1 == 2);
  CHECK_EQ_INT(40 + 2, 42);
  CHECK_NEAR(0.1 + 0.2, 0.3, 1e-9);
}

int main(void) {
  test_harness_self();
  if (g_fail) { fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
  printf("all host tests passed\n");
  return 0;
}
