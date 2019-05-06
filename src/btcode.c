#include <stdio.h>
#include <string.h>
#include "dct.h"

#define N2 (N_MAX * N_MAX)

int
btcode_encode(uint8_t **outbuf_ptr, size_t *outbuf_size_ptr,
              uint8_t const *btable, uint16_t const btable_n)
{
    int ret = BTCODE_SUCCESS;

    int64_t *freq_mtx = malloc(N2 * sizeof(*freq_mtx));
    if (!freq_mtx) goto alloc_error;

    dct_forward(freq_mtx, btable, btable_n);

    *outbuf_size_ptr = sizeof(uint16_t) + N2 * sizeof(*freq_mtx);
    uint8_t *outbuf = malloc(*outbuf_size_ptr);
    if (!outbuf) goto alloc_error;

    *(uint16_t *)outbuf = btable_n;
    memcpy(outbuf + sizeof(uint16_t), freq_mtx, N2 * sizeof(*freq_mtx));

    *outbuf_ptr = outbuf;
    goto ret;

alloc_error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    ret = BTCODE_ERR(errno);
ret:
    free(freq_mtx);
    return ret;
}

int
btcode_decode(uint8_t **btable_ptr, size_t *btable_n_ptr,
              uint8_t const *inbuf, size_t inbuf_size)
{
    int ret = BTCODE_SUCCESS;

    uint16_t const n = *(uint16_t *)inbuf;
    int64_t *freq_mtx = (void *)inbuf + sizeof(uint16_t);

    uint8_t *btable = malloc(n * n);
    if (!btable) goto alloc_error;

    dct_backward(btable, freq_mtx, n);

    *btable_ptr = btable;
    *btable_n_ptr = n;
    goto ret;

alloc_error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    ret = BTCODE_ERR(errno);
ret:
    return ret;
}
