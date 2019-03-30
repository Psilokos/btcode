#include <stdio.h>
#include <string.h>
#include <time.h>

static struct
{
    char const *name;
    int (*func)(void);
} const
tests[] =
{
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
