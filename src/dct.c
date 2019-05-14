#include <math.h>
#include <string.h>
#include "common.h"

#define ROUND2(x, n)    (((x) + (1LL << ((n) - 1))) >> (n))

static int64_t cm[BLKSZ * BLKSZ];
static uint16_t stride;
static unsigned shift;

static inline unsigned int
log2i(uint32_t const x)
{
    unsigned base = 31 - __builtin_clz(x);
    return base + (__builtin_ctz(x) != base);
}

// TODO add maxval
void
dct_init(uint16_t const stride_)
{
    stride = stride_;

    /* precision bits = (64 - sign_bit - 2 * log2(n) - log2(x) - log2(4)) / 2 */
    shift = (64 - 1 - 2 * log2i(BLKSZ) - 0 - 2) / 2;

    for (int u = 0; u < BLKSZ; ++u)
        for (int i = 0; i < BLKSZ; ++i)
            cm[u * BLKSZ + i] = roundf(
                    (1LL << shift) *
                    cosf(u * (2.f * i + 1.f) * M_PI / (2.f * BLKSZ)));
}

void
dct_forward(int64_t *fm, uint8_t const *sm)
{
    int64_t tm[BLKSZ * BLKSZ];

    for (int v = 0; v < BLKSZ; ++v)
        for (int i = 0; i < BLKSZ; ++i)
        {
            int64_t sum = 0;
            for (int j = 0; j < BLKSZ; ++j)
                sum += 2 * sm[i * stride + j] * cm[v * BLKSZ + j];
            tm[v * BLKSZ + i] = sum;
        }

    for (int u = 0; u < BLKSZ; ++u)
        for (int v = 0; v < BLKSZ; ++v)
        {
            int64_t sum = 0;
            for (int i = 0; i < BLKSZ; ++i)
                sum += 2 * tm[v * BLKSZ + i] * cm[u * BLKSZ + i];
            fm[u * stride + v] = ROUND2(sum, 2 * shift);
        }
}

void
dct_backward(uint8_t *sm, int64_t const *fm)
{
    int64_t tm[BLKSZ * BLKSZ];

    for (int j = 0; j < BLKSZ; ++j)
        for (int u = 0; u < BLKSZ; ++u)
        {
            int64_t sum = 0;
            for (int v = 0; v < BLKSZ; ++v)
            {
                int64_t tmp = fm[u * stride + v] * cm[v * BLKSZ + j];
                if (v == 0)
                    tmp = ROUND2(tmp, 1);
                sum += tmp;
            }
            tm[j * BLKSZ + u] = sum;
        }

    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
        {
            int64_t sum = 0;
            for (int u = 0; u < BLKSZ; ++u)
            {
                int64_t tmp = tm[j * BLKSZ + u] * cm[u * BLKSZ + i];
                if (u == 0)
                    tmp = ROUND2(tmp, 1);
                sum += tmp;
            }
            sm[i * stride + j] = ROUND2(sum / (BLKSZ * BLKSZ), 2 * shift);
        }
}
