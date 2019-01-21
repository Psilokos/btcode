#ifndef DCT_H
# define DCT_H

# include <stdint.h>

void dct_forward(int64_t *freq_mtx, uint8_t *data_mtx, unsigned int n);
void dct_backward(int8_t *data_mtx, int64_t *freq_mtx, unsigned int n);

#endif
