#ifndef DCT_H
# define DCT_H

# include "common.h"

int  dct_init(uint16_t const n, unsigned const max);
void dct_destroy(void);
void dct_forward(int64_t *freq_mtx, uint8_t const *spatial_mtx);
void dct_backward(uint8_t *spatial_mtx, int64_t const *freq_mtx);

#endif
