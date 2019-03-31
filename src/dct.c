#include <math.h>
#include "common.h"

static float *cm;
static float *tm;
static uint16_t n;

int
dct_init(uint16_t const n_)
{
    n = n_;
    cm = malloc(n * n * sizeof(*cm)); if (!cm) return errno;
    tm = malloc(n * n * sizeof(*tm)); if (!tm) return errno;

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            cm[i * n + j] = cos(i * (2.f * j + 1.f) * M_PI / (2.f * n));

    return BTCODE_SUCCESS;
}

void
dct_destroy(void)
{
    free(cm);
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
                sum += 2 * sm[i * n + j] * cm[v * n + j];
            tm[v * n + i] = sum;
        }

    for (int u = 0; u < n; ++u)
        for (int v = 0; v < n; ++v)
        {
            float sum = 0;
            for (int i = 0; i < n; ++i)
                sum += 2 * tm[v * n + i] * cm[u * n + i];
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
                sum += (!v ? .5f : 1.f) * fm[u * n + v] * cm[v * n + j];
            tm[j * n + u] = sum;
        }

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            float sum = 0;
            for (int u = 0; u < n; ++u)
                sum += (!u ? .5f : 1.f) * tm[j * n + u] * cm[u * n + i];
            sm[i * n + j] = roundf(sum / (n * n));
        }
}
