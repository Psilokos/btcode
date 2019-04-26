#include <math.h>
#include "common.h"

#define ROUND2(x, n)    (((x) + (1LL << ((n) - 1))) >> (n))

static int64_t *cm;
static int64_t *tm;
static uint16_t n;
static unsigned shift;

static inline unsigned int
log2i(uint32_t const x)
{
    unsigned base = 31 - __builtin_clz(x);
    return base + (__builtin_ctz(x) != base);
}

int
dct_init(uint16_t const n_, unsigned const max)
{
    n = n_;
    cm = malloc(n * n * sizeof(*cm)); if (!cm) return errno;
    tm = malloc(n * n * sizeof(*tm)); if (!tm) return errno;

    /* precision bits = (64 - sign_bit - 2 * log2(n) - log2(x) - log2(4)) / 2 */
    shift = (64 - 1 - 2 * log2i(n) - log2i(max) - 2) / 2;

    for (int u = 0; u < n; ++u)
        for (int i = 0; i < n; ++i)
            cm[u * n + i] = roundf(
                    (1LL << shift) *
                    cosf(u * (2.f * i + 1.f) * M_PI / (2.f * n)));

    return BTCODE_SUCCESS;
}

void
dct_destroy(void)
{
    free(cm);
    free(tm);
}

void
dct_forward(int64_t *fm, uint8_t const *sm)
{
    for (int v = 0; v < n; ++v)
        for (int i = 0; i < n; ++i)
        {
            int64_t sum = 0;
            for (int j = 0; j < n; ++j)
                sum += 2 * sm[i * n + j] * cm[v * n + j];
            tm[v * n + i] = sum;
        }

    for (int u = 0; u < n; ++u)
        for (int v = 0; v < n; ++v)
        {
            int64_t sum = 0;
            for (int i = 0; i < n; ++i)
                sum += 2 * tm[v * n + i] * cm[u * n + i];
            fm[u * n + v] = ROUND2(sum, 2 * shift);
        }
}

void
dct_backward(uint8_t *sm, int64_t const *fm)
{
    for (int j = 0; j < n; ++j)
        for (int u = 0; u < n; ++u)
        {
            int64_t sum = 0;
            for (int v = 0; v < n; ++v)
            {
                int64_t tmp = fm[u * n + v] * cm[v * n + j];
                if (v == 0)
                    tmp = ROUND2(tmp, 1);
                sum += tmp;
            }
            tm[j * n + u] = sum;
        }

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            int64_t sum = 0;
            for (int u = 0; u < n; ++u)
            {
                int64_t tmp = tm[j * n + u] * cm[u * n + i];
                if (u == 0)
                    tmp = ROUND2(tmp, 1);
                sum += tmp;
            }
            sm[i * n + j] = ROUND2(sum / (n * n), 2 * shift);
        }
}
