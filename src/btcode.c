#include "common.h"

int
btcode_encode(uint8_t **outbuf_ptr, size_t *outbuf_size_ptr,
              uint8_t const *btable, uint16_t const btable_n)
{
    (void)outbuf_ptr; (void)outbuf_size_ptr; (void)btable; (void)btable_n;
    return BTCODE_SUCCESS;
}

int
btcode_decode(uint8_t **btable_ptr, size_t *btable_n_ptr,
              uint8_t const *inbuf, size_t inbuf_size)
{
    (void)btable_ptr; (void)btable_n_ptr; (void)inbuf; (void)inbuf_size;
    return BTCODE_SUCCESS;
}
