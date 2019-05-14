#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "dct.h"

#define NUM_THREADS 8
#define BLKSZ2 (BLKSZ * BLKSZ)

static int
freqcmp(void const *f0ptr, void const *f1ptr)
{
    int64_t delta = *(int64_t *)f0ptr - *(int64_t *)f1ptr;
    if (delta == 0) return 0;
    return delta < 0 ? -1 : 1;
}

static int
filter_freqs(int64_t *freq_mtx, uint16_t const n, float const threshold)
{
    int ret = BTCODE_SUCCESS;

    struct { int64_t key; int val; } *freq_dict;
    freq_dict = malloc(BLKSZ2 * sizeof(*freq_dict));
    if (!freq_dict)
        goto error;

    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
        {
            int const idx_dict = i * BLKSZ + j;
            int const idx_mtx = i * n + j;
            freq_dict[idx_dict].key = llabs(freq_mtx[idx_mtx]);
            freq_dict[idx_dict].val = idx_mtx;
        }
    qsort(freq_dict, BLKSZ2, sizeof(*freq_dict), freqcmp);

    for (int i = 0; (float)i / BLKSZ2 < threshold; ++i)
        freq_mtx[freq_dict[i].val] = 0;

    goto ret;

error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    ret = BTCODE_ERR(errno);
ret:
    free(freq_dict);
    return ret;
}

typedef void (*tfblk_fptr)(uint8_t *, int64_t *, uint8_t const *, uint16_t);
struct tfblk_ctx
{
    uint8_t const *sm_in;
    uint8_t *sm_out;
    int64_t *fm;
    uint16_t n;
    unsigned int num_blocks;
    int thread_num;
    tfblk_fptr tfblk;
};

static inline unsigned int
count_residuals(uint8_t const *sm0, uint8_t const *sm1, uint16_t const n)
{
    unsigned int cnt = 0;
    for (int i = 0; i < BLKSZ; ++i)
        for (int j = 0; j < BLKSZ; ++j)
            if (sm0[i * n + j] != sm1[i * n + j])
                ++cnt;
    return cnt;
}

static void
enc_tfblk(uint8_t *sm_out, int64_t *fm, uint8_t const *sm_in, uint16_t const n)
{
    dct_forward(fm, sm_in);

    size_t const fm_size = (n * (BLKSZ - 1) + BLKSZ) * sizeof(int64_t);
    int64_t *fm_tmp = malloc(fm_size);

    float threshold = .001f;
    float delta = .0f;
    unsigned int prev_cnt = 0;
    while (1)
    {
        memcpy(fm_tmp, fm, fm_size);
        filter_freqs(fm_tmp, n, threshold);
        dct_backward(sm_out, fm_tmp);
        unsigned int const cnt = count_residuals(sm_in, sm_out, n);
#if 0
        if (cnt >= 1 || threshold + .001f >= 1.f)
            break;
        threshold += .001f;
#else
        if (cnt > prev_cnt)
        {
            if (delta == .0f)
                delta = -.25f;
            else
                delta = -delta / 2.f;
        }
        else if (cnt < prev_cnt)
            delta = +delta / 2;
        else
        {
            if (delta == .0f)
                delta = +.5f;
            else
                break;
        }
        threshold += delta;
        prev_cnt = cnt;
#endif
    }
#if 0
    if (threshold < 1.f)
    {
        threshold -= .001f;
        filter_freqs(fm, n, threshold);
        dct_backward(sm_out, fm);
        printf("cnt => %u\n", count_residuals(sm_in, sm_out, n));
    }
    else
        memcpy(fm, fm_tmp, fm_size);
#else
    memcpy(fm, fm_tmp, fm_size);
#endif
    free(fm_tmp);
    fprintf(stderr, "threshold used => %f\n", threshold);
}

static void
dec_tfblk(uint8_t *sm, int64_t *fm, uint8_t const *_0, uint16_t _1)
{
    dct_backward(sm, fm);
}

static void *
transform_blocks(void *arg)
{
    struct tfblk_ctx *ctx = (typeof(ctx))arg;
    int const x = ctx->thread_num * ctx->num_blocks * BLKSZ % ctx->n;
    int const y = (ctx->thread_num * ctx->num_blocks * BLKSZ / ctx->n) * BLKSZ;
    int reloff = y * ctx->n + x;

    for (int i = 0; i < ctx->num_blocks; ++i)
    {
        ctx->tfblk(ctx->sm_out + reloff,
                   ctx->fm + reloff,
                   ctx->sm_in + reloff,
                   ctx->n);

        reloff += BLKSZ;
        if ((i + 1) * BLKSZ % ctx->n == 0)
        {
            assert(((i + 1) * BLKSZ & (ctx->n - 1)) == 0);
            reloff += ctx->n * (BLKSZ - 1);
        }
    }

    return NULL;
}

static int
frequency_transform(uint8_t *spatial_mtx_out,
                    int64_t *freq_mtx,
                    uint8_t const *spatial_mtx_in,
                    uint16_t const n,
                    tfblk_fptr tfblk)
{
    int ret = BTCODE_SUCCESS;

    dct_init(n);

    pthread_t threads[NUM_THREADS] = {0};
    struct tfblk_ctx ctx[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; ++i)
    {
        ctx[i] = (struct tfblk_ctx)
        {
            .sm_in = spatial_mtx_in,
            .sm_out = spatial_mtx_out,
            .fm = freq_mtx,
            .n = n,
            .num_blocks = (n * n) / (BLKSZ2 * NUM_THREADS),
            .thread_num = i,
            .tfblk = tfblk,
        };
        if (pthread_create(threads + i, NULL, transform_blocks, ctx + i))
            goto error;
    }
    for (int i = 0; i < NUM_THREADS; ++i)
        if (pthread_join(threads[i], NULL))
            goto error;
    goto ret;

error:
    ret = BTCODE_EGENERIC;
ret:
    return ret;
}

static int
encode_frequencies(int64_t **buf_ptr, size_t *bufsize_ptr,
                   int64_t const *freq_mtx, uint16_t const n)
{
    int ret = BTCODE_SUCCESS;

    int64_t *buf = NULL;
    size_t bufsize = 0;
    size_t size = 0;

    unsigned int cnt = 0;
    uint32_t const n2 = n * n;
    for (unsigned int i = 0; i < n2 / BLKSZ2; ++i)
    {
        int const x = i * BLKSZ % n;
        int const y = (i * BLKSZ / n) * BLKSZ;
        int has_freq = 0;
        for (int j = y; j < y + BLKSZ; ++j)
        {
            for (int k = x; k < x + BLKSZ; ++k)
            {
                int64_t const f = freq_mtx[j * n + k];
                if (f != 0)
                {
                    if (size + 3 * 8 >= bufsize)
                    {
                        bufsize += 4096;
                        buf = realloc(buf, bufsize); if (!buf) goto error;
                        *buf_ptr = buf;
                    }
                    if (!has_freq)
                    {
                        // TODO 32-bit freqs
                        buf[size >> 3] = i;
                        size += 8;
                        ++cnt;
                    }
                    buf[size >> 3] = ((uint64_t)(j - y) << 32) | (k - x);
                    size += 8;
                    buf[size >> 3] = f;
                    size += 8;
                    has_freq = 1;
                }
            }
        }
#if 0
        if (has_freq)
        {
            for (int j = y; j < y + BLKSZ; ++j)
            {
                for (int k = x; k < x + BLKSZ; ++k)
                    printf("%llx ", llabs(freq_mtx[j * n + k]));
                printf("\n");
            }
            printf("FREQ FOUND\n\n");
        }
#endif
    }
    printf("encoded %lu \"\"\"frequencies\"\"\"\n", size >> 3);
    printf("%u / %u blocks with frequencies\n", cnt, n2 / BLKSZ2);

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
                 uint32_t const n2)
{
    int ret = BTCODE_SUCCESS;

    size_t size = 0;
    size_t bufsize = 256 * sizeof(uint32_t);
    uint32_t *buf = malloc(bufsize); if (!buf) goto error;

    for (uint32_t i = 0; i < n2; ++i)
    {
        if (btable[i] == dec_btable[i])
            continue;
        assert(size != bufsize);
        buf[size >> 2] = i;
        size += 4;
    }
    printf("encoded %lu residuals\n", size >> 2);

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
       int64_t const *freq_mtx,
       uint16_t const n, uint16_t const nmax)
{
    int ret = BTCODE_SUCCESS;

    int64_t *frequencies = NULL;
    uint32_t *residuals = NULL;

    size_t frequencies_size;
    if (encode_frequencies(&frequencies, &frequencies_size, freq_mtx, nmax))
        goto error;

    size_t residuals_size;
    if (encode_residuals(&residuals, &residuals_size, btable, dec_btable, nmax * nmax))
        goto error;

    *outbuf_size_ptr = 2 + 4 + 1 + frequencies_size + residuals_size;
    *outbuf_ptr = malloc(*outbuf_size_ptr); if (!*outbuf_ptr) goto error;
    uint8_t *outbuf = *outbuf_ptr;

    *(uint16_t *)outbuf = n;
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
    ret = BTCODE_ERR(errno);
ret:
    free(residuals);
    free(frequencies);
    return ret;
}

int
btcode_encode(uint8_t **outbuf_ptr, size_t *outbuf_size_ptr,
              uint8_t const *btable, uint16_t const btable_n)
{
    int ret = BTCODE_SUCCESS;

    uint8_t *padded_btable = NULL;
    int64_t *freq_mtx = NULL;
    uint8_t *dec_btable = NULL;

    uint16_t const nmax = (btable_n + BLKSZ - 1) & ~(BLKSZ - 1);
    uint32_t const nmax2 = nmax * nmax;

    padded_btable = malloc(nmax2);
    if (!padded_btable)
        goto alloc_error;
    freq_mtx = malloc(nmax2 * sizeof(*freq_mtx));
    if (!freq_mtx)
        goto alloc_error;
    dec_btable = malloc(nmax2);
    if (!dec_btable)
        goto alloc_error;

    for (int i = 0; i < btable_n; ++i)
    {
        memcpy(padded_btable + i * nmax, btable + i * btable_n, btable_n);
        memset(padded_btable + i * nmax + btable_n, 0, nmax - btable_n);
    }
    memset(padded_btable + btable_n * nmax, 0, (nmax - btable_n) * nmax);

    frequency_transform(dec_btable, freq_mtx, padded_btable, nmax, enc_tfblk);
    ret = encode(outbuf_ptr, outbuf_size_ptr,
                 btable, dec_btable, freq_mtx, btable_n, nmax);
    goto ret;

alloc_error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    ret = BTCODE_ERR(errno);
ret:
    free(dec_btable);
    free(freq_mtx);
    free(padded_btable);
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

    uint16_t const n = *(uint16_t *)inbuf;
    inbuf += 2;
    uint16_t const nmax = (n + BLKSZ - 1) & ~(BLKSZ - 1);
    uint32_t const nmax2 = nmax * nmax;

    int64_t *freq_mtx = NULL;
    uint8_t *padded_btable = NULL;
    uint8_t *dec_btable = NULL;

    freq_mtx = malloc(nmax2 * sizeof(*freq_mtx));
    if (!freq_mtx)
        goto alloc_error;
    padded_btable = malloc(nmax2);
    if (!padded_btable)
        goto alloc_error;
    dec_btable = malloc(n * n);
    if (!dec_btable)
        goto alloc_error;

    size_t const frequencies_size = *(uint32_t *)inbuf << 3;
    inbuf += 4;
    size_t const residuals_size = *inbuf++ << 2;

    if (frequencies_size > nmax2 || residuals_size > nmax2)
        goto input_error;

    decode_frequencies(freq_mtx, (int64_t *)inbuf, frequencies_size >> 3);
    inbuf += frequencies_size;
    frequency_transform(padded_btable, freq_mtx, NULL, nmax, dec_tfblk);
    apply_residuals(padded_btable, (uint32_t *)inbuf, residuals_size >> 2);

    for (int i = 0; i < n; ++i)
        memcpy(dec_btable + i * n, padded_btable + i * nmax, n);

    *btable_ptr = dec_btable;
    *btable_n_ptr = n;
    goto ret;

alloc_error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    ret = BTCODE_ERR(errno);
    goto ret;
input_error:
    fprintf(stderr, "format mismatch\n");
    ret = BTCODE_EGENERIC;
ret:
    free(padded_btable);
    free(freq_mtx);
    return ret;
}
