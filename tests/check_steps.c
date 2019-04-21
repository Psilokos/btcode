#include <stdio.h>
#include <string.h>
#include <time.h>
#include "dct.h"

static int
check_dct(void)
{
    int ret = BTCODE_SUCCESS;

    int const n = rand() % 128 + 1;
    int const n2 = n * n;

    uint8_t *sm_in = NULL;
    uint8_t *sm_out = NULL;
    float *fm = NULL;

    sm_in = malloc(n2); if (!sm_in) goto alloc_error;
    sm_out = malloc(n2); if (!sm_out) goto alloc_error;
    fm = malloc(n2 * sizeof(*fm)); if (!fm) goto alloc_error;
    ret = dct_init(n); if (ret) goto alloc_error;

    for (int i = 0; i < n2; ++i)
        sm_in[i] = rand() % 256;

    dct_forward(fm, sm_in);
    dct_backward(sm_out, fm);

    if (memcmp(sm_in, sm_out, n2))
        goto test_failed;
    goto ret;

alloc_error:
    ret = BTCODE_ERR(errno);
    goto ret;
test_failed:
    ret = BTCODE_EGENERIC;
ret:
    dct_destroy();
    free(sm_in);
    free(sm_out);
    free(fm);
    return ret;
}

static struct
{
    char const *name;
    int (*func)(void);
} const
tests[] =
{
    { "DCT-IDCT", check_dct },
    {0}
};

static inline void
init_rand_generator(void)
{
    srand(time(NULL));
}

static inline int
run_tests(void)
{
    int ret = BTCODE_SUCCESS;
    for (int i = 0; tests[i].func; ++i)
    {
        for (int j = 0; j < 16; ++j)
        {
            ret = tests[i].func();
            if (ret) break;
        }
        char const *status;
        if (ret < 0)
            status = "ERROR";
        else if (ret > 0)
            status = "FAIL";
        else
            status = "OK";
        char const *testname = tests[i].name;
        fprintf(stderr, "    - %s%*c%s]\n",
                testname, 21 - (uint32_t)strlen(testname), '[', status);
        if (ret < 0)
            goto error;
    }
    goto ret;
error:
    fprintf(stderr, "\nerror: %s\n", strerror(ret));
ret:
    return ret;
}

int
main(void)
{
    errno = 0;
    init_rand_generator();
    return run_tests();
}
