#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dct.h"

#define N2 (N_MAX * N_MAX)

static int
freqcmp(void const *f0ptr, void const *f1ptr)
{
    int64_t delta = *(int64_t *)f0ptr - *(int64_t *)f1ptr;
    if (delta == 0) return 0;
    return delta < 0 ? -1 : 1;
}

static int
filter_freqs(int64_t *freqs)
{
    int ret = BTCODE_SUCCESS;

    struct { int64_t key; int val; } *freq_dict;
    freq_dict = malloc(N2 * sizeof(*freq_dict)); if (!freq_dict) goto error;

    for (int i = 0; i < N_MAX; ++i)
        for (int j = 0; j < N_MAX; ++j)
        {
            int idx = i * N_MAX + j;
            freq_dict[idx].key = llabs(freqs[idx]);
            freq_dict[idx].val = idx;
        }
    qsort(freq_dict, N2, sizeof(*freq_dict), freqcmp);

    int cnt = 0;
    for (int i = 0; (float)i / N2 <= .8f; ++i, ++cnt)
        freqs[freq_dict[i].val] = 0;

    goto ret;

error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    ret = BTCODE_ERR(errno);
ret:
    free(freq_dict);
    return ret;
}

static int
encode_frequencies(int64_t **buf_ptr, size_t *bufsize_ptr,
                   int64_t const *freq_mtx)
{
    int ret = BTCODE_SUCCESS;

    int64_t *buf = NULL;
    size_t bufsize = 0;
    size_t size = 0;

    uint32_t j;
    for (uint32_t i = 0; i < N2; i += j + 1)
    {
        j = 0;
        while (freq_mtx[i + j] == 0)
            ++j;
        if (size == bufsize)
        {
            bufsize += 4096;
            buf = realloc(buf, bufsize); if (!buf) goto error;
            *buf_ptr = buf;
        }
        buf[size >> 3] = j != 0
            ? ((uint64_t)(0x2A * 0x01010101) << 32) | j--
            : freq_mtx[i];
        size += 8;
    }

    *bufsize_ptr = size;
    goto ret;

error:
    ret = BTCODE_EGENERIC;
ret:
    return ret;
}

static int
encode_residuals(uint32_t **buf_ptr, size_t *bufsize_ptr,
                 uint8_t const *btable, uint8_t const *dec_btable,
                 uint16_t const btable_n)
{
    int ret = BTCODE_SUCCESS;

    size_t size = 0;
    size_t bufsize = 256 * sizeof(uint32_t);
    uint32_t *buf = malloc(bufsize); if (!buf) goto error;

    for (uint32_t i = 0; i < btable_n * btable_n; ++i)
    {
        if (btable[i] == dec_btable[i])
            continue;
        assert(size != bufsize);
        buf[size >> 2] = i;
        size += 4;
    }

    *buf_ptr = buf;
    *bufsize_ptr = size;
    goto ret;

error:
    ret = BTCODE_EGENERIC;
ret:
    return ret;
}

static int
encode(uint8_t **outbuf_ptr, size_t *outbuf_size_ptr,
       uint8_t const *btable, uint8_t const *dec_btable,
       int64_t const *freq_mtx, uint16_t const btable_n)
{
    int ret = BTCODE_SUCCESS;

    int64_t *frequencies = NULL;
    uint32_t *residuals = NULL;

    size_t frequencies_size;
    if (encode_frequencies(&frequencies, &frequencies_size, freq_mtx))
        goto error;

    size_t residuals_size;
    if (encode_residuals(&residuals, &residuals_size,
                         btable, dec_btable, btable_n))
        goto error;

    *outbuf_size_ptr = 2 + 4 + 1 + frequencies_size + residuals_size;
    *outbuf_ptr = malloc(*outbuf_size_ptr); if (!*outbuf_ptr) goto error;
    uint8_t *outbuf = *outbuf_ptr;

    *(uint16_t *)outbuf = btable_n;
    outbuf += 2;
    *(uint32_t *)outbuf = frequencies_size >> 3;
    outbuf += 4;
    *outbuf++ = residuals_size >> 2;

    memcpy(outbuf, frequencies, frequencies_size);
    outbuf += frequencies_size;
    memcpy(outbuf, residuals, residuals_size);

    goto ret;

error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    free(residuals);
    free(frequencies);
    ret = BTCODE_ERR(errno);
ret:
    return ret;
}

int
btcode_encode(uint8_t **outbuf_ptr, size_t *outbuf_size_ptr,
              uint8_t const *btable, uint16_t const btable_n)
{
    int ret = BTCODE_SUCCESS;

    int64_t *freq_mtx = NULL;
    uint8_t *dec_btable = NULL;

    freq_mtx = malloc(N2 * sizeof(*freq_mtx)); if (!freq_mtx) goto alloc_error;
    dec_btable = malloc(btable_n * btable_n); if (!dec_btable) goto alloc_error;

    dct_forward(freq_mtx, btable, btable_n);
    filter_freqs(freq_mtx);
    dct_backward(dec_btable, freq_mtx, btable_n);

    ret = encode(outbuf_ptr, outbuf_size_ptr,
                 btable, dec_btable, freq_mtx, btable_n);
    goto ret;

alloc_error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    ret = BTCODE_ERR(errno);
ret:
    free(dec_btable);
    free(freq_mtx);
    return ret;
}

static void
decode_frequencies(int64_t *freq_mtx,
                   int64_t const *freqs, uint32_t const num_entries)
{
    uint32_t j = 0;
    for (uint32_t i = 0; i < num_entries; ++i)
        if (freqs[i] >> 32 == 0x2A * 0x01010101)
        {
            uint32_t num_elem = freqs[i] & 0xFFFFFFFF;
            memset(freq_mtx + j, 0, num_elem << 3);
            j += num_elem;
        }
        else
            freq_mtx[j++] = freqs[i];
}

static inline void
apply_residuals(uint8_t *btable,
                uint32_t const *indices, unsigned const num_indices)
{
    for (unsigned i = 0; i < num_indices; ++i)
        btable[indices[i]] ^= 1;
}

int
btcode_decode(uint8_t **btable_ptr, size_t *btable_n_ptr,
              uint8_t const *inbuf, size_t inbuf_size)
{
    int ret = BTCODE_SUCCESS;

    uint16_t const btable_n = *(uint16_t *)inbuf;
    inbuf += 2;

    int64_t *freq_mtx = NULL;
    uint8_t *btable = NULL;

    freq_mtx = malloc(N2 * sizeof(*freq_mtx)); if (!freq_mtx) goto alloc_error;
    btable = malloc(btable_n * btable_n); if (!btable) goto alloc_error;

    size_t const frequencies_size = *(uint32_t *)inbuf << 3;
    inbuf += 4;
    size_t const residuals_size = *inbuf++ << 2;

    decode_frequencies(freq_mtx, (int64_t *)inbuf, frequencies_size >> 3);
    inbuf += frequencies_size;
    dct_backward(btable, freq_mtx, btable_n);
    apply_residuals(btable, (uint32_t *)inbuf, residuals_size >> 2);

    *btable_ptr = btable;
    *btable_n_ptr = btable_n;
    goto ret;

alloc_error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    free(freq_mtx);
    ret = BTCODE_ERR(errno);
ret:
    return ret;
}
