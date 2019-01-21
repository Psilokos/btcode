#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "dct.h"
#include "tables.h"

#include <stdio.h>

#define ROUND2(x, n)    ((x + (1 << (n - 1))) >> n)

static int cnt = 0;

#define NUM_THREADS 8

#define join_threads(threads) \
do \
{ \
    for (int i = 0; i < NUM_THREADS; ++i) \
        if (!joined_threads[i]) \
            joined_threads[i] = pthread_join(threads[i], NULL) == 0; \
} while (0)

union domain
{
    uint8_t *sd;
    int64_t *fd;
};

struct dct_ctx
{
    union domain src;
    union domain dst;
    int it;
    unsigned int n;
    int shift;
    int maxcnt;
};

static int64_t tmp[N_MAX * N_MAX];
static pthread_t threads[NUM_THREADS] = {0};
static int joined_threads[NUM_THREADS];
static struct dct_ctx dct[NUM_THREADS];

static void *
dct_forward_0(void *arg)
{
    struct dct_ctx *dct = (struct dct_ctx *)arg;
    for (int i = 0; i < dct->n; ++i)
    {
        int64_t sum = 0;
        for (int j = 0; j < dct->n; ++j)
            sum += dct->src.sd[i * dct->n + j]
                * tb_dct_coefs[dct->it * N_MAX + j];
        dct->dst.fd[dct->it * N_MAX + i] = ROUND2(sum, dct->shift);
    }
    for (int i = dct->n; i < N_MAX; ++i)
        dct->dst.fd[dct->it * N_MAX + i] = 0;
    return NULL;
}

static void *
dct_forward_1(void *arg)
{
    struct dct_ctx *dct = (struct dct_ctx *)arg;
    for (int v = 0; v < N_MAX; ++v)
    {
        int64_t sum = 0;
        for (int i = 0; i < dct->n; ++i)
            sum += dct->src.fd[v * N_MAX + i]
                * ROUND2(tb_dct_coefs[dct->it * N_MAX + i], dct->shift);
        dct->dst.fd[dct->it * N_MAX + v] = ROUND2(sum, dct->shift);
    }
    return NULL;
}

void
dct_forward(int64_t *freq_mtx, uint8_t *data_mtx, unsigned int n)
{
    int shift = 2 * ((64 - 2 * ceilf(log2f(N_MAX)) - 1) / 4);
    int maxcnt = 2 * N_MAX + 2 * n;

    for (int i = 0; i < NUM_THREADS; ++i)
        joined_threads[i] = 1;

    for (int v = 0; v < N_MAX; ++v)
    {
        dct[v % NUM_THREADS] = (struct dct_ctx)
        {
            .src.sd = data_mtx,
            .dst.fd = tmp,
            .it = v,
            .n = n,
            .shift = shift,
        };
        if (pthread_create(threads + v % NUM_THREADS, NULL, dct_forward_0, dct + v % NUM_THREADS))
            abort();
        joined_threads[v % NUM_THREADS] = 0;
        if ((v + 1) % NUM_THREADS == 0)
            join_threads(threads);
        fprintf(stderr, "\r%.3f%%", 100 * ++cnt / (float)maxcnt);
    }
    join_threads(threads);

    for (int u = 0; u < N_MAX; ++u)
    {
        dct[u % NUM_THREADS] = (struct dct_ctx)
        {
            .src.fd = tmp,
            .dst.fd = freq_mtx,
            .it = u,
            .n = n,
            .shift = shift,
        };
        if (pthread_create(threads + u % NUM_THREADS, NULL, dct_forward_1, dct + u % NUM_THREADS))
            abort();
        joined_threads[u % NUM_THREADS] = 0;
        if ((u + 1) % NUM_THREADS == 0)
            join_threads(threads);
        fprintf(stderr, "\r%.3f%%", 100 * ++cnt / (float)maxcnt);
    }
    join_threads(threads);
}

static void *
dct_backward_0(void *arg)
{
    struct dct_ctx *dct = (struct dct_ctx *)arg;
    for (int u = 0; u < N_MAX; ++u)
    {
        int64_t sum = 0;
        for (int v = 0; v < N_MAX; ++v)
            sum += dct->src.fd[u * N_MAX + v]
                * ROUND2(tb_dct_coefs[v * N_MAX + dct->it], dct->shift);
        dct->dst.fd[dct->it * N_MAX + u] = ROUND2(sum, dct->shift);
    }
    return NULL;
}

static void *
dct_backward_1(void *arg)
{
    struct dct_ctx *dct = (struct dct_ctx *)arg;
    for (int j = 0; j < dct->n; ++j)
    {
        int64_t sum = 0;
        for (int u = 0; u < N_MAX; ++u)
            sum += dct->src.fd[j * N_MAX + u]
                * ROUND2(tb_dct_coefs[u * N_MAX + dct->it], dct->shift);
        ((int8_t *)dct->dst.sd)[dct->it * dct->n + j] = ROUND2(sum, 2 * dct->shift);
    }
    return NULL;
}

void
dct_backward(int8_t *data_mtx, int64_t *freq_mtx, unsigned int n)
{
    int shift = 2 * ((64 - 2 * ceilf(log2f(N_MAX)) - 1) / 4);
    int maxcnt = 2 * N_MAX + 2 * n;

    for (int i = 0; i < NUM_THREADS; ++i)
        joined_threads[i] = 1;

    for (int j = 0; j < n; ++j)
    {
        dct[j % NUM_THREADS] = (struct dct_ctx)
        {
            .src.fd = freq_mtx,
            .dst.fd = tmp,
            .it = j,
            .shift = shift,
        };
        if (pthread_create(threads + j % NUM_THREADS, NULL, dct_backward_0, dct + j % NUM_THREADS))
            abort();
        joined_threads[j % NUM_THREADS] = 0;
        if ((j + 1) % NUM_THREADS == 0)
            join_threads(threads);
        fprintf(stderr, "\r%.3f%%", 100 * ++cnt / (float)maxcnt);
    }
    join_threads(threads);

    for (int i = 0; i < n; ++i)
    {
        dct[i % NUM_THREADS] = (struct dct_ctx)
        {
            .src.fd = tmp,
            .dst.sd = (uint8_t *)data_mtx,
            .it = i,
            .n = n,
            .shift = shift,
        };
        if (pthread_create(threads + i % NUM_THREADS, NULL, dct_backward_1, dct + i % NUM_THREADS))
            abort();
        joined_threads[i % NUM_THREADS] = 0;
        if ((i + 1) % NUM_THREADS == 0)
            join_threads(threads);
        fprintf(stderr, "\r%.3f%%", 100 * ++cnt / (float)maxcnt);
    }
    join_threads(threads);
}
