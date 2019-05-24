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

static void
filter_freqs(int64_t *freq_mtx, uint16_t const n, float const threshold)
{
    struct { int64_t key; int val; } freq_dict[BLKSZ2];

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

static uint8_t total_residual_count;

static void
enc_tfblk(uint8_t *sm_out, int64_t *fm, uint8_t const *sm_in, uint16_t const n)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    dct_forward(fm, sm_in);

    size_t const fm_size = (n * (BLKSZ - 1) + BLKSZ) * sizeof(int64_t);
    int64_t *fm_tmp = malloc(fm_size);

    pthread_mutex_lock(&mutex);
    unsigned int limit = total_residual_count == 255 ? 0 : 1;
    int limit_reached = 0;
    int dir = -1;
    float delta = .1f;
    float threshold = 1.f;
    while (1)
    {
        memcpy(fm_tmp, fm, fm_size);
        filter_freqs(fm_tmp, n, threshold);
        dct_backward(sm_out, fm_tmp);

        unsigned int cnt = count_residuals(sm_in, sm_out, n);
        if (cnt == 0 && threshold == 1.f)
            break;
        if (dir == -1)
        {
            if (cnt <= limit)
            {
                if (limit_reached || threshold == 1.f)
                {
                    total_residual_count += cnt;
                    break;
                }
                dir = +1;
                delta /= 10.f;
            }
        }
        else if (dir == +1)
        {
            if (cnt > limit)
            {
                limit_reached = 1;
                dir = -1;
                delta /= 10.f;
            }
        }
        threshold += dir * delta;
    }
    pthread_mutex_unlock(&mutex);
    memcpy(fm, fm_tmp, fm_size);
    free(fm_tmp);
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
    total_residual_count = 0;

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
                        buf[size >> 3] = ((uint64_t) i << 32) | (0x2A * 0x01010101);
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
    printf("%u / %u blocks with frequencies (%.3f%%)\n", cnt, n2 / BLKSZ2, (float)cnt * BLKSZ2 / n2);
    printf("encoded %lu \"\"\"frequencies\"\"\" (%lu bytes)\n", size >> 3, size);

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
    printf("encoded %lu residuals (%lu bytes)\n", size >> 2, size);

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
                 padded_btable, dec_btable, freq_mtx, btable_n, nmax);
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
decode_frequencies(int64_t *freq_mtx, int64_t const *freqs,
                   uint32_t const num_entries, uint16_t const n)
{
    assert(num_entries != 0);
    uint32_t i = 0;
    while (1)
    {
        uint32_t const blknum = freqs[i++] >> 32;
        do
        {
            int x = blknum * BLKSZ % n + (freqs[i] & 0xFFFFFFFF);
            int y = (blknum * BLKSZ / n) * BLKSZ + (freqs[i++] >> 32);
            freq_mtx[y * n + x] = freqs[i++];
            if (i == num_entries)
                return;
        } while ((freqs[i] & 0xFFFFFFFF) != 0x2A * 0x01010101);
    }
}

static inline void
apply_residuals(uint8_t *btable,
                uint32_t const *indices, unsigned const num_indices)
{
    for (unsigned i = 0; i < num_indices; ++i)
    {
        btable[indices[i]] ^= 1;
    }
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

    freq_mtx = calloc(nmax2, sizeof(*freq_mtx));
    if (!freq_mtx)
        goto alloc_error;
    padded_btable = malloc(nmax2);
    if (!padded_btable)
        goto alloc_error;
    dec_btable = malloc(n * n);
    if (!dec_btable)
        goto alloc_error;

    uint32_t const num_freq_entries = *(uint32_t *)inbuf;
    size_t const frequencies_size = num_freq_entries << 3;
    inbuf += 4;
    uint8_t const num_residuals = *inbuf++;
    size_t const residuals_size = num_residuals << 2;

    if (num_freq_entries == 0 ||
        (frequencies_size >> 3) > nmax2 ||
        (residuals_size >> 2) > nmax2)
        goto input_error;

    decode_frequencies(freq_mtx, (int64_t *)inbuf, frequencies_size >> 3, nmax);
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
    free(dec_btable);
    ret = BTCODE_EGENERIC;
ret:
    free(padded_btable);
    free(freq_mtx);
    return ret;
}
