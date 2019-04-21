#include <math.h>
#include "common.h"

static float *tm;
static uint16_t n;

int
dct_init(uint16_t const n_)
{
    n = n_;
    tm = malloc(n * n * sizeof(*tm));
    return tm ? BTCODE_SUCCESS : BTCODE_ERR(errno);
}

void
dct_destroy(void)
{
    free(tm);
}

void
dct_forward(float *fm, uint8_t const *sm)
{
    for (int v = 0; v < n; ++v)
        for (int i = 0; i < n; ++i)
        {
            float sum = 0;
            for (int j = 0; j < n; ++j)
            {
                float coef = cos(v * (2.f * j + 1.f) * M_PI / (2.f * n));
                sum += 2 * sm[i * n + j] * coef;
            }
            tm[v * n + i] = sum;
        }

    for (int u = 0; u < n; ++u)
        for (int v = 0; v < n; ++v)
        {
            float sum = 0;
            for (int i = 0; i < n; ++i)
            {
                float coef = cos(u * (2.f * i + 1.f) * M_PI / (2.f * n));
                sum += 2 * tm[v * n + i] * coef;
            }
            fm[u * n + v] = sum;
        }
}

void
dct_backward(uint8_t *sm, float const *fm)
{
    for (int j = 0; j < n; ++j)
        for (int u = 0; u < n; ++u)
        {
            float sum = 0;
            for (int v = 0; v < n; ++v)
            {
                float cv = !v ? .5f : 1.f;
                float coef = cos(v * (2.f * j + 1.f) * M_PI / (2.f * n));
                sum += cv * fm[u * n + v] * coef;
            }
            tm[j * n + u] = sum;
        }

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            float sum = 0;
            for (int u = 0; u < n; ++u)
            {
                float cu = !u ? .5f : 1.f;
                float coef = cos(u * (2.f * i + 1) * M_PI / (2.f * n));
                sum += cu * tm[j * n + u] * coef;
            }
            sm[i * n + j] = roundf(sum / (n * n));
        }
}
