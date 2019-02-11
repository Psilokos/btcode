#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "dct.h"

#include <math.h>
#include <stdio.h>

#define BLKSZ2  (BLKSZ * BLKSZ)

struct dct_ctx
{
    uint8_t *bt;
    int8_t *dec_bt;
    int64_t *freq_bt;
    unsigned int stride;
    unsigned int num_block;
    unsigned int thread_id;
};

static void *
freqency_transform(void *arg)
{
    struct dct_ctx *c = (struct dct_ctx *)arg;

    int y = c->thread_id * c->num_block * BLKSZ2 / c->stride;
    int x = c->thread_id * c->num_block * BLKSZ % c->stride;
    unsigned int shift = y * c->stride + x;
    c->bt += shift;
    c->dec_bt += shift;
    c->freq_bt += shift;

    for (int i = 0; i < c->num_block; ++i)
    {
        dct_forward(c->freq_bt, c->bt, c->stride);
        dct_backward(c->dec_bt, c->freq_bt, c->stride);

        shift = BLKSZ;
        if ((i + 1) * BLKSZ % c->stride == 0)
            shift += (BLKSZ - 1) * c->stride;
        c->bt += shift;
        c->dec_bt += shift;
        c->freq_bt += shift;
    }

    return NULL;
}

static inline void
test_dct(void)
{
    uint8_t sm[BLKSZ2] =
    {
        0, 0, 1, 0, 0, 0, 0, 1,
        0, 1, 1, 0, 1, 0, 1, 0,
        1, 0, 0, 1, 0, 0, 1, 0,
        1, 1, 0, 0, 1, 0, 1, 1,
        0, 1, 1, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 1, 0, 1,
        1, 1, 0, 1, 0, 1, 0, 0,
        0, 0, 1, 0, 1, 0, 0, 0,
    };
    int64_t fm[BLKSZ2];
    int8_t dsm[BLKSZ2];
    dct_forward(fm, sm, 8);
    dct_backward(dsm, fm, 8);

    for (int i = 0; i < BLKSZ; ++i)
    {
        for (int j = 0; j < BLKSZ; ++j)
            fprintf(stderr, "%i ", sm[i + 8 + j]);
        fprintf(stderr, "\n");
        for (int j = 0; j < BLKSZ; ++j)
            fprintf(stderr, "%i ", dsm[i * 8 + j] < 0 ? 0 : 1);
        for (int j = 0; j < BLKSZ; ++j)
            fprintf(stderr, "%li ", fm[i * 8 + j]);
        fprintf(stderr, "\n\n");
    }
}

int
btcode_encode(uint8_t **p_outbuf, uint8_t *bt, unsigned int n)
{
    int ret = BTCODE_SUCCESS;

    test_dct();

    unsigned int nmax = (n + BLKSZ - 1) & ~(BLKSZ - 1);
    unsigned int nmax2 = nmax * nmax;

    uint8_t *padded_bt = malloc(nmax2 * sizeof(*padded_bt));
    if (!padded_bt) return BTCODE_ENOMEM;
    int8_t *dec_bt = malloc(nmax2 * sizeof(*dec_bt));
    if (!dec_bt) return BTCODE_ENOMEM;
    int64_t *freq_bt = malloc(nmax2 * sizeof(*freq_bt));
    if (!freq_bt) return BTCODE_ENOMEM;

    for (int i = 0, j = 0; i < n * nmax; ++i)
        if (i / nmax < n)
        {
            if (i % nmax < n)
                padded_bt[i] = bt[j++];
            else
                padded_bt[i] = 0;
        }
    memset(padded_bt + n * nmax, 0, (nmax - n) * nmax * sizeof(*padded_bt));

    dct_init();

    pthread_t threads[8] = {0};
    struct dct_ctx c[8];
    for (int i = 0; i < 8; ++i)
    {
        c[i] = (struct dct_ctx)
        {
            .bt = padded_bt,
            .dec_bt = dec_bt,
            .freq_bt = freq_bt,
            .stride = nmax,
            .num_block = nmax2 / (BLKSZ2 * 8),
            .thread_id = i,
        };
        if (pthread_create(threads + i, NULL, freqency_transform, c + i))
            goto error;
    }
    for (int i = 0; i < 8; ++i)
        if (pthread_join(threads[i], NULL))
            goto error;

    // dct_forward(freq_bt, bt, n);

    // uint64_t min = (1 << 64) - 1;
    // uint64_t max = 0;
    // for (int i = 0; i < N_MAX; ++i)
    //     for (int j = 0; j < N_MAX; ++j)
    //     {
    //         uint64_t f = llabs(freq_bt[i * N_MAX + j]);
    //         if (f < min)
    //             min = f;
    //         if (f > max)
    //             max = f;
    //     }

    // int counters[10] = {0};
    // uint64_t delta = max + 1 - min;
    // for (int i = 0; i < N_MAX; ++i)
    //     for (int j = 0; j < N_MAX; ++j)
    //     {
    //         uint64_t f = llabs(freq_bt[i * N_MAX + j]) - min;
    //         for (int k = 0; k < 10; ++k)
    //             if (f < (uint64_t)roundf((k + 1) * delta / 10.f))
    //             {
    //                 if (k == 2)
    //                     freq_bt[i * N_MAX + j] = 0;
    //                 ++counters[k];
    //                 break;
    //             }
    //     }

    // dct_backward(dec_bt, freq_bt, n);

    for (int i = 0, j = 0; i < n * nmax; ++i)
        if (i % nmax < n)
            bt[j++] ^= dec_bt[i] <= 0 ? 0 : 1;

    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
            printf("%i", bt[i * n + j]);
        if (i + 1 < n)
            puts("");
    }

    FILE *filp = fopen("freq.mtx", "w");
    for (int i = 0; i < nmax; ++i)
    {
        for (int j = 0; j < nmax; ++j)
            fprintf(filp, "%10ld\t", freq_bt[i * nmax + j]);
        fprintf(filp, "\n");
    }
    fclose(filp);

    // fprintf(stderr, "\n");
    // for (int i = 0; i < 10; ++i)
    //     fprintf(stderr, "%3i%%: %d\n", (i + 1) * 10, counters[i]);

    // fprintf(stderr, "\nmin=%ld\nmax=%ld\n", min, max);

error:
    ret = BTCODE_EGENERIC;
ret:
    free(padded_bt);
    free(dec_bt);
    free(freq_bt);

    return ret;
}

int
btcode_decode(uint8_t **p_bt, uint8_t *inbuf, size_t inbuf_size)
{
    (void)p_bt; (void)inbuf; (void)inbuf_size;
    return BTCODE_SUCCESS;
}
