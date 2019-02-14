#ifndef DCT_H
# define DCT_H

# include <stdint.h>

void dct_init(void);
void dct_forward(int64_t *fm, uint8_t *sm, unsigned int stride);
void dct_backward(int8_t *sm, int64_t *fm, unsigned int stride);

void dct_init_f(void);
void dct_forward_f(float *fm, uint8_t *sm, unsigned int stride);
void dct_backward_f(int8_t *sm, float *fm, unsigned int stride);

#endif
