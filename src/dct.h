#ifndef DCT_H
# define DCT_H

# include "common.h"

void dct_forward(float *freq_mtx, uint8_t const *spatial_mtx, uint16_t const n);
void dct_backward(uint8_t *spatial_mtx, float const *freq_mtx, uint16_t const n);

#endif
