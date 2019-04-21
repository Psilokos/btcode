#ifndef DCT_H
# define DCT_H

# include "common.h"

int  dct_init(uint16_t const n);
void dct_destroy(void);
void dct_forward(float *freq_mtx, uint8_t const *spatial_mtx);
void dct_backward(uint8_t *spatial_mtx, float const *freq_mtx);

#endif
