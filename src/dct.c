#include <math.h>
#include "common.h"

void
dct_forward(float *fm, uint8_t const *sm, uint16_t const n)
{
    for (int u = 0; u < n; ++u)
        for (int v = 0; v < n; ++v)
        {
            fm[u * n + v] = .0f;
            for (int i = 0; i < n; ++i)
            {
                float sum = .0f;
                for (int j = 0; j < n; ++j)
                {
                    float coef = cos(v * (2.f * j + 1.f) * M_PI / (2.f * n));
                    sum += 2 * sm[i * n + j] * coef;
                }
                float coef = cos(u * (2.f * i + 1.f) * M_PI / (2.f * n));
                fm[u * n + v] += 2 * sum * coef;
            }
        }
}

void
dct_backward(uint8_t *sm, float const *fm, uint16_t const n)
{
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            float sum_u = .0f;
            for (int u = 0; u < n; ++u)
            {
                float sum_v = .0f;
                for (int v = 0; v < n; ++v)
                {
                    float cv = !v ? .5f : 1.f;
                    float coef = cos(v * (2.f * j + 1.f) * M_PI / (2.f * n));
                    sum_v += cv * fm[u * n + v] * coef;
                }
                float cu = !u ? .5f : 1.f;
                float coef = cos(u * (2.f * i + 1) * M_PI / (2.f * n));
                sum_u += cu * sum_v * coef;
            }
            sm[i * n + j] = roundf(sum_u / (n * n));
        }
}
