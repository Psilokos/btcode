#include <math.h>
#include <string.h>
#include "common.h"
#include "tables.h"

#define ROUND2(x, n)    (((x) + (1LL << ((n) - 1))) >> (n))

static int64_t tm[N_MAX * N_MAX];

static inline unsigned int
log2i(uint32_t const x)
{
    unsigned base = 31 - __builtin_clz(x);
    return base + (__builtin_ctz(x) != base);
}

void
dct_forward(int64_t *fm, uint8_t const *sm, uint16_t const n)
{
    /* precision bits = (64 - sign_bit - 2 * log2(n) - log2(x) - log2(4)) / 2 */
    unsigned const shift = (64 - 1 - 2 * log2i(N_MAX) - 0 - 2) / 2;

    for (int v = 0; v < N_MAX; ++v)
        for (int i = 0; i < n; ++i)
        {
            int64_t sum = 0;
            for (int j = 0; j < n; ++j)
                sum += 2 * sm[i * n + j] * tb_dct_coefs[v * N_MAX + j];
            tm[v * N_MAX + i] = sum;
        }

    for (int u = 0; u < N_MAX; ++u)
        for (int v = 0; v < N_MAX; ++v)
        {
            int64_t sum = 0;
            for (int i = 0; i < n; ++i)
                sum += 2 * tm[v * N_MAX + i] * tb_dct_coefs[u * N_MAX + i];
            fm[u * N_MAX + v] = ROUND2(sum, 2 * shift);
        }
}

void
dct_backward(uint8_t *sm, int64_t const *fm, uint16_t const n)
{
    /* precision bits = (64 - sign_bit - 2 * log2(n) - log2(x) - log2(4)) / 2 */
    unsigned const shift = (64 - 1 - 2 * log2i(N_MAX) - 0 - 2) / 2;

    for (int j = 0; j < n; ++j)
        for (int u = 0; u < N_MAX; ++u)
        {
            int64_t sum = 0;
            for (int v = 0; v < N_MAX; ++v)
            {
                int64_t tmp = fm[u * N_MAX + v] * tb_dct_coefs[v * N_MAX + j];
                if (v == 0)
                    tmp = ROUND2(tmp, 1);
                sum += tmp;
            }
            tm[j * N_MAX + u] = sum;
        }

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            int64_t sum = 0;
            for (int u = 0; u < N_MAX; ++u)
            {
                int64_t tmp = tm[j * N_MAX + u] * tb_dct_coefs[u * N_MAX + i];
                if (u == 0)
                    tmp = ROUND2(tmp, 1);
                sum += tmp;
            }
            sm[i * n + j] = ROUND2(sum / (N_MAX * N_MAX), 2 * shift);
        }
}
