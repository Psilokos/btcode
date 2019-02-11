#include <math.h>
#include <stdlib.h>
#include "common.h"
#include "dct.h"

#define ROUND2(x, n)    ((x + (1 << (n - 1))) >> n)

static int64_t cm[BLKSZ * BLKSZ];

void
dct_init(void)
{
    int shift = (64 - 2 * ceilf(log2f(BLKSZ)) - 1);

    for (int i = 0; i < BLKSZ ; ++i)
        cm[0 * BLKSZ + i] = roundf((1UL << shift) / sqrtf(BLKSZ));

    for (int i = 1; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
            cm[i * BLKSZ + j] = roundf(
                (1UL << shift) * sqrtf(2.f / BLKSZ) *
                cos(i * (2.f * j + 1.f) * M_PI / (2.f * BLKSZ)));
}

void
dct_forward(int64_t *fm, uint8_t *sm, unsigned int stride)
{
    int shift = 2 * ((64 - 2 * ceilf(log2f(BLKSZ)) - 1) / 4);
    int64_t tm[BLKSZ * BLKSZ];

    for (int v = 0; v < BLKSZ; ++v)
        for (int i = 0; i < BLKSZ; ++i)
        {
            int64_t sum = 0;
            for (int j = 0; j < BLKSZ; ++j)
                sum += sm[i * stride + j] * cm[v * BLKSZ + j];
            tm[v * stride + i] = ROUND2(sum, shift);
        }
    for (int u = 0; u < BLKSZ; ++u)
        for (int v = 0; v < BLKSZ; ++v)
        {
            int64_t sum = 0;
            for (int i = 0; i < BLKSZ; ++i)
                sum += tm[v * stride + i] * ROUND2(cm[u * BLKSZ + i], shift);
            fm[u * stride + v] = ROUND2(sum, shift);
        }
}

void
dct_backward(int8_t *sm, int64_t *fm, unsigned int stride)
{
    int shift = 2 * ((64 - 2 * ceilf(log2f(BLKSZ)) - 1) / 4);
    int64_t tm[BLKSZ * BLKSZ];

    for (int j = 0; j < BLKSZ; ++j)
        for (int u = 0; u < BLKSZ; ++u)
        {
            int64_t sum = 0;
            for (int v = 0; v < BLKSZ; ++v)
                sum += fm[u * stride + v] * ROUND2(cm[v * BLKSZ + j], shift);
            tm[j * stride + u] = ROUND2(sum, shift);
        }
    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
        {
            int64_t sum = 0;
            for (int u = 0; u < BLKSZ; ++u)
                sum += tm[j * stride + u] * ROUND2(cm[u * BLKSZ + i], shift);
            ((int8_t *)sm)[i * stride + j] = ROUND2(sum, 2 * shift);
        }
}
