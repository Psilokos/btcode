#ifndef BTCODE_H
# define BTCODE_H

# include <stddef.h>
# include "common.h"

int btcode_encode(uint8_t **p_outbuf, uint8_t *bt, unsigned int n);
int btcode_decode(uint8_t **p_bt, uint8_t *inbuf, size_t inbuf_size);

#endif
