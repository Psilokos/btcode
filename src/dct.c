#include <math.h>
#include <stdlib.h>
#include "common.h"
#include "dct.h"

#define ROUND2(x, n)    ((x + (1 << ((n) - 1))) >> (n))

static int64_t cm[BLKSZ * BLKSZ];
static float cm_f[BLKSZ * BLKSZ];

#include <stdio.h>

void
dct_init_f(void)
{
    for (int i = 0; i < BLKSZ ; ++i)
        cm_f[0 * BLKSZ + i] = 1.f / sqrtf(BLKSZ);

    for (int i = 1; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
            cm_f[i * BLKSZ + j] = sqrtf(2.f / BLKSZ) *
                cos(i * (2.f * j + 1.f) * M_PI / (2.f * BLKSZ));

    for (int i = 0; i < BLKSZ; ++i)
    {
        for (int j = 0; j < BLKSZ; ++j)
            fprintf(stderr, "%f ", cm_f[i * BLKSZ + j]);
        fprintf(stderr, "\n");
    }
}

void
dct_forward_f(float *fm, uint8_t *sm, unsigned int stride)
{
    float tm[BLKSZ * BLKSZ];

    for (int v = 0; v < BLKSZ; ++v)
        for (int i = 0; i < BLKSZ; ++i)
        {
            float sum = 0;
            for (int j = 0; j < BLKSZ; ++j)
                sum += sm[i * stride + j] * cm_f[v * BLKSZ + j];
            tm[v * BLKSZ + i] = sum;
        }
    for (int u = 0; u < BLKSZ; ++u)
        for (int v = 0; v < BLKSZ; ++v)
        {
            float sum = 0;
            for (int i = 0; i < BLKSZ; ++i)
                sum += tm[v * BLKSZ + i] * cm_f[u * BLKSZ + i];
            fm[u * stride + v] = sum;
        }
}

void
dct_backward_f(int8_t *sm, float *fm, unsigned int stride)
{
    float tm[BLKSZ * BLKSZ];

    for (int j = 0; j < BLKSZ; ++j)
        for (int u = 0; u < BLKSZ; ++u)
        {
            float sum = 0;
            for (int v = 0; v < BLKSZ; ++v)
                sum += fm[u * stride + v] * cm_f[v * BLKSZ + j];
            tm[j * BLKSZ + u] = sum;
        }
    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
        {
            float sum = 0;
            for (int u = 0; u < BLKSZ; ++u)
                sum += tm[j * BLKSZ + u] * cm_f[u * BLKSZ + i];
            sm[i * stride + j] = roundf(sum);
        }
}

/* -------------------------------------------------------------------------- */

void
dct_init(void)
{
    int lshift = 64 - 1 - 3;

    for (int i = 0; i < BLKSZ ; ++i)
        cm[0 * BLKSZ + i] = roundf((1UL << lshift) / sqrtf(BLKSZ));

    for (int i = 1; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
            cm[i * BLKSZ + j] = roundf(
                (1UL << lshift) * sqrtf(2.f / BLKSZ) *
                cos(i * (2.f * j + 1.f) * M_PI / (2.f * BLKSZ)));
}

void
dct_forward(int64_t *fm, uint8_t *sm, unsigned int stride)
{
    int rnd = 30; // lshift / 2
    int64_t tm[BLKSZ * BLKSZ];

    for (int v = 0; v < BLKSZ; ++v)
        for (int i = 0; i < BLKSZ; ++i)
        {
            int64_t sum = 0;
            for (int j = 0; j < BLKSZ; ++j)
                sum += sm[i * stride + j] * cm[v * BLKSZ + j];
            tm[v * BLKSZ + i] = ROUND2(sum, rnd);
        }
    for (int u = 0; u < BLKSZ; ++u)
        for (int v = 0; v < BLKSZ; ++v)
        {
            int64_t sum = 0;
            for (int i = 0; i < BLKSZ; ++i)
                sum += tm[v * BLKSZ + i] * ROUND2(cm[u * BLKSZ + i], rnd);
            fm[u * stride + v] = ROUND2(sum, rnd + 24);
        }
}

void
dct_backward(int8_t *sm, int64_t *fm, unsigned int stride)
{
    int rnd = 30;
    int64_t tm[BLKSZ * BLKSZ];

    for (int j = 0; j < BLKSZ; ++j)
        for (int u = 0; u < BLKSZ; ++u)
        {
            int64_t sum = 0;
            for (int v = 0; v < BLKSZ; ++v)
                sum += fm[u * stride + v] * ROUND2(cm[v * BLKSZ + j], rnd - 24);
            tm[j * BLKSZ + u] = ROUND2(sum, rnd);
        }
    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
        {
            int64_t sum = 0;
            for (int u = 0; u < BLKSZ; ++u)
                sum += tm[j * BLKSZ + u] * ROUND2(cm[u * BLKSZ + i], rnd);
            sm[i * stride + j] = ROUND2(sum, 2 * rnd - 1);
        }
}
