#ifndef BTCODE_H
# define BTCODE_H

# include "common.h"

int btcode_encode(uint8_t **outbuf_ptr, size_t *outbuf_size_ptr,
                  uint8_t const *btable, uint16_t const btable_n);

int btcode_decode(uint8_t **btable_ptr, size_t *btable_n_ptr,
                  uint8_t const *inbuf, size_t inbuf_size);

#endif
