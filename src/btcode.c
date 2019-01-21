#include <stdlib.h>
#include "common.h"
#include "dct.h"

#include <math.h>
#include <stdio.h>

int
btcode_encode(uint8_t **p_outbuf, uint8_t *bt, unsigned int n)
{
    int64_t *freq_bt = malloc(N_MAX * N_MAX * sizeof(*freq_bt));
    if (!freq_bt) return BTCODE_ENOMEM;
    int8_t *dec_bt = malloc(n * n * sizeof(*dec_bt));
    if (!dec_bt) return BTCODE_ENOMEM;

    dct_forward(freq_bt, bt, n);

    uint64_t min = (1 << 64) - 1;
    uint64_t max = 0;
    for (int i = 0; i < N_MAX; ++i)
        for (int j = 0; j < N_MAX; ++j)
        {
            uint64_t f = llabs(freq_bt[i * N_MAX + j]);
            if (f < min)
                min = f;
            if (f > max)
                max = f;
        }

    int counters[10] = {0};
    uint64_t delta = max + 1 - min;
    for (int i = 0; i < N_MAX; ++i)
        for (int j = 0; j < N_MAX; ++j)
        {
            uint64_t f = llabs(freq_bt[i * N_MAX + j]) - min;
            for (int k = 0; k < 10; ++k)
                if (f < (uint64_t)roundf((k + 1) * delta / 10.f))
                {
                    if (k == 2)
                        freq_bt[i * N_MAX + j] = 0;
                    ++counters[k];
                    break;
                }
        }

    dct_backward(dec_bt, freq_bt, n);

    for (int i = 0; i < n * n; ++i)
        bt[i] ^= dec_bt[i] <= 0 ? 0 : 1;

    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
            printf("%i", bt[i * n + j]);
        if (i + 1 < n)
            puts("");
    }

    FILE *filp = fopen("freq.mtx", "w");
    for (int i = 0; i < N_MAX; ++i)
    {
        for (int j = 0; j < N_MAX; ++j)
            fprintf(filp, "%10ld\t", freq_bt[i * N_MAX + j]);
        fprintf(filp, "\n");
    }
    fclose(filp);

    fprintf(stderr, "\n");
    for (int i = 0; i < 10; ++i)
        fprintf(stderr, "%3i%%: %d\n", (i + 1) * 10, counters[i]);

    fprintf(stderr, "\nmin=%ld\nmax=%ld\n", min, max);

    free(freq_bt);
    free(dec_bt);

    return BTCODE_SUCCESS;
}

int
btcode_decode(uint8_t **p_bt, uint8_t *inbuf, size_t inbuf_size)
{
    (void)p_bt; (void)inbuf; (void)inbuf_size;
    return BTCODE_SUCCESS;
}
