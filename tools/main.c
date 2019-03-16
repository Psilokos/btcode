#include <stdio.h>
#include <btcode.h>

static inline int
usage(int ret)
{
    fprintf(stderr, "usage:\n"
                    "    " PROGRAM_NAME " <file>\n");
    return ret;
}

#if BTCODE_ENCODE
# define btcode_code btcode_encode
#else
# define btcode_code btcode_decode
#endif

int
main(int argc, char **argv)
{
    if (argc == 1)
        return usage(1);

    int ret = btcode_code(NULL, NULL, NULL, 0);
    if (ret) goto error;

    goto ret;

error:
ret:
    return ret;
}
