#include <math.h>
#include <pthread.h>
#include <string.h>
#include "common.h"
#include "tables.h"

#define NUM_THREADS 8

#define join_threads(threads) \
do \
{ \
    for (int i = 0; i < NUM_THREADS; ++i) \
        if (!joined_threads[i]) \
            joined_threads[i] = pthread_join(threads[i], NULL) == 0; \
} while (0)

#define ROUND2(x, n)    (((x) + (1LL << ((n) - 1))) >> (n))

union domain_src
{
    uint8_t const *sm;
    int64_t const *fm;
};

union domain_dst
{
    uint8_t *sm;
    int64_t *fm;
};

struct dct_ctx
{
    union domain_src src;
    union domain_dst dst;
    int it;
    unsigned int n;
    int shift;
};

static int64_t tm[N_MAX * N_MAX];
static pthread_t threads[NUM_THREADS] = {0};
static int joined_threads[NUM_THREADS];
static struct dct_ctx dct_ctx[NUM_THREADS];

static inline unsigned int
log2i(uint32_t const x)
{
    unsigned base = 31 - __builtin_clz(x);
    return base + (__builtin_ctz(x) != base);
}

static void *
dct_forward_0(void *arg)
{
    struct dct_ctx const *ctx = (struct dct_ctx *)arg;
    for (int i = 0; i < ctx->n; ++i)
    {
        int64_t sum = 0;
        for (int j = 0; j < ctx->n; ++j)
            sum += 2 * ctx->src.sm[i * ctx->n + j]
                * tb_dct_coefs[ctx->it * N_MAX + j];
        ctx->dst.fm[ctx->it * N_MAX + i] = sum;
    }
    return NULL;
}

static void *
dct_forward_1(void *arg)
{
    struct dct_ctx const *ctx = (struct dct_ctx *)arg;
    for (int v = 0; v < N_MAX; ++v)
    {
        int64_t sum = 0;
        for (int i = 0; i < ctx->n; ++i)
            sum += 2 * ctx->src.fm[v * N_MAX + i]
                * tb_dct_coefs[ctx->it * N_MAX + i];
        ctx->dst.fm[ctx->it * N_MAX + v] = ROUND2(sum, 2 * ctx->shift);
    }
    return NULL;
}

void
dct_forward(int64_t *fm, uint8_t const *sm, uint16_t const n)
{
    /* precision bits = (64 - sign_bit - 2 * log2(n) - log2(x) - log2(4)) / 2 */
    unsigned const shift = (64 - 1 - 2 * log2i(N_MAX) - 0 - 2) / 2;

    for (int i = 0; i < NUM_THREADS; ++i)
        joined_threads[i] = 1;

    for (int v = 0; v < N_MAX; ++v)
    {
        dct_ctx[v % NUM_THREADS] = (struct dct_ctx)
        {
            .src.sm = sm,
            .dst.fm = tm,
            .it = v,
            .n = n,
            .shift = shift,
        };
        if (pthread_create(threads + v % NUM_THREADS, NULL,
                           dct_forward_0, dct_ctx + v % NUM_THREADS))
            abort();
        joined_threads[v % NUM_THREADS] = 0;
        if ((v + 1) % NUM_THREADS == 0)
            join_threads(threads);
    }
    join_threads(threads);

    for (int u = 0; u < N_MAX; ++u)
    {
        dct_ctx[u % NUM_THREADS] = (struct dct_ctx)
        {
            .src.fm = tm,
            .dst.fm = fm,
            .it = u,
            .n = n,
            .shift = shift,
        };
        if (pthread_create(threads + u % NUM_THREADS, NULL,
                           dct_forward_1, dct_ctx + u % NUM_THREADS))
            abort();
        joined_threads[u % NUM_THREADS] = 0;
        if ((u + 1) % NUM_THREADS == 0)
            join_threads(threads);
    }
    join_threads(threads);
}

static void *
dct_backward_0(void *arg)
{
    struct dct_ctx const *ctx = (struct dct_ctx *)arg;
    for (int u = 0; u < N_MAX; ++u)
    {
        int64_t sum = 0;
        for (int v = 0; v < N_MAX; ++v)
        {
            int64_t tmp = ctx->src.fm[u * N_MAX + v]
                * tb_dct_coefs[v * N_MAX + ctx->it];
            if (v == 0)
                tmp = ROUND2(tmp, 1);
            sum += tmp;
        }
        ctx->dst.fm[ctx->it * N_MAX + u] = sum;
    }
    return NULL;
}

static void *
dct_backward_1(void *arg)
{
    struct dct_ctx const *ctx = (struct dct_ctx *)arg;
    for (int j = 0; j < ctx->n; ++j)
    {
        int64_t sum = 0;
        for (int u = 0; u < N_MAX; ++u)
        {
            int64_t tmp = ctx->src.fm[j * N_MAX + u]
                * tb_dct_coefs[u * N_MAX + ctx->it];
            if (u == 0)
                tmp = ROUND2(tmp, 1);
            sum += tmp;
        }
        ctx->dst.sm[ctx->it * ctx->n + j] =
            ROUND2(sum / (N_MAX * N_MAX), 2 * ctx->shift);
    }
    return NULL;
}

void
dct_backward(uint8_t *sm, int64_t const *fm, uint16_t const n)
{
    /* precision bits = (64 - sign_bit - 2 * log2(n) - log2(x) - log2(4)) / 2 */
    unsigned const shift = (64 - 1 - 2 * log2i(N_MAX) - 0 - 2) / 2;

    for (int i = 0; i < NUM_THREADS; ++i)
        joined_threads[i] = 1;

    for (int j = 0; j < n; ++j)
    {
        dct_ctx[j % NUM_THREADS] = (struct dct_ctx)
        {
            .src.fm = fm,
            .dst.fm = tm,
            .it = j,
            .shift = shift,
        };
        if (pthread_create(threads + j % NUM_THREADS, NULL,
                           dct_backward_0, dct_ctx + j % NUM_THREADS))
            abort();
        joined_threads[j % NUM_THREADS] = 0;
        if ((j + 1) % NUM_THREADS == 0)
            join_threads(threads);
    }
    join_threads(threads);

    for (int i = 0; i < n; ++i)
    {
        dct_ctx[i % NUM_THREADS] = (struct dct_ctx)
        {
            .src.fm = tm,
            .dst.sm = sm,
            .it = i,
            .n = n,
            .shift = shift,
        };
        if (pthread_create(threads + i % NUM_THREADS, NULL,
                           dct_backward_1, dct_ctx + i % NUM_THREADS))
            abort();
        joined_threads[i % NUM_THREADS] = 0;
        if ((i + 1) % NUM_THREADS == 0)
            join_threads(threads);
    }
    join_threads(threads);
}
