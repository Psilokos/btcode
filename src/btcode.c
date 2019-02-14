#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "dct.h"

#include <math.h>
#include <stdio.h>

#define BLKSZ2  (BLKSZ * BLKSZ)

static inline void
test_dct(void)
{
    uint8_t sm[BLKSZ2] =
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    };
    uint8_t sm2[BLKSZ2] =
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

    fprintf(stderr, "coefs before\n");
    for (int i = 0; i < BLKSZ; ++i)
    {
        for (int j = 0; j < BLKSZ; ++j)
            fprintf(stderr, "%03li\t", fm[i * 8 + j]);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");

    uint64_t min = (1UL << 64) - 1;
    uint64_t max = 0;
    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
        {
            uint64_t f = llabs(fm[i * BLKSZ + j]);
            if (f < min)
                min = f;
            if (f > max)
                max = f;
        }

    int cnt = 0;
    int counters[1000] = {0};
    uint64_t delta = max + 1 - min;
    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
        {
            uint64_t f = llabs(fm[i * BLKSZ + j]) - min;
            for (int k = 0; k < 1000; ++k)
                if (f < (uint64_t)roundf((k + 1) * delta / 1000.f))
                {
                    if (k < 718 || (k == 718 && j % 2 == 1))
                        fm[i * BLKSZ + j] = 0;
                    ++counters[k];
                    ++cnt;
                    break;
                }
        }
    fprintf(stderr, "cnt=%i\n", cnt);

    fprintf(stderr, "counters:\n");
    for (int i = 0; i < 1000; ++i)
        fprintf(stderr, "%4i%%: %03d\n", i + 1, counters[i]);
    fprintf(stderr, "\nmin=%ld\nmax=%ld\n", min, max);

    fprintf(stderr, "\ncoefs after:\n");
    for (int i = 0; i < BLKSZ; ++i)
    {
        for (int j = 0; j < BLKSZ; ++j)
            fprintf(stderr, "%03li\t", fm[i * 8 + j]);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");

    dct_backward(dsm, fm, 8);

    for (int i = 0; i < BLKSZ; ++i)
    {
        for (int j = 0; j < BLKSZ; ++j)
            fprintf(stderr, "%i ", sm[i * 8 + j]);
        fprintf(stderr, "\n");
        for (int j = 0; j < BLKSZ; ++j)
            fprintf(stderr, "%i ", dsm[i * 8 + j] <= 0 ? 0 : 1);
        fprintf(stderr, "\n\n");
    }
}

struct dct_ctx
{
    uint8_t *bt;
    int8_t *dec_bt;
    int64_t *freq_bt;
    unsigned int stride;
    unsigned int num_block;
    unsigned int thread_id;
};

static int
freqcmp(void const *p_f0, void const *p_f1)
{
    int64_t diff = *(int64_t *)p_f0 - *(int64_t *)p_f1;
    if (diff == 0) return 0;
    return diff < 0 ? -1 : 1;
}

static void
filter_frequencies(int64_t *freqs, unsigned int stride)
{
    struct { int64_t key; int val; } freq_dict[BLKSZ2];
    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
        {
            int idx = i * stride + j;
            freq_dict[i * BLKSZ + j].key = llabs(freqs[idx]);
            freq_dict[i * BLKSZ + j].val = idx;
        }
    qsort(freq_dict, BLKSZ2, sizeof(*freq_dict), freqcmp);
    for (int i = 0; i < BLKSZ2 - BLKSZ; ++i)
        freqs[freq_dict[i].val] = 0;
}

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
        filter_frequencies(c->freq_bt, c->stride);
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

int
btcode_encode(uint8_t **p_outbuf, uint8_t *bt, unsigned int n)
{
    int ret = BTCODE_SUCCESS;

    unsigned int nmax = (n + BLKSZ - 1) & ~(BLKSZ - 1);
    unsigned int nmax2 = nmax * nmax;

    uint8_t *padded_bt = malloc(nmax2 * sizeof(*padded_bt));
    if (!padded_bt) return BTCODE_ENOMEM;
    int8_t *dec_bt = malloc(nmax2 * sizeof(*dec_bt));
    if (!dec_bt) return BTCODE_ENOMEM;
    int64_t *freq_bt = malloc(nmax2 * sizeof(*freq_bt));
    if (!freq_bt) return BTCODE_ENOMEM;

    fprintf(stderr, "%u %u\n", n, nmax);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < nmax; ++j)
            padded_bt[i * nmax + j] = j < n ? bt[i * n + j] : 0;
    memset(padded_bt + n * nmax, 0, (nmax - n) * nmax * sizeof(*padded_bt));

    for (int i = 0; i < n; ++i)
        if (memcmp(padded_bt, bt, n))
            goto error;

    dct_init();

    // test_dct();

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

    for (int i = 0; i < nmax; ++i)
        for (int j = 0; j < nmax; ++j)
            if (i < n && j < n)
                bt[i * n + j] ^= dec_bt[i * nmax + j] <= 0 ? 0 : 1;

    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
            printf("%i", bt[i * n + j]);
        puts("");
    }

    int freq_cnt = 0;
    for (int i = 0; i < nmax; ++i)
        for (int j = 0; j < nmax; ++j)
            if (freq_bt[i * nmax + j])
                ++freq_cnt;
    fprintf(stderr, "frequencies count: %d\n", freq_cnt);

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
