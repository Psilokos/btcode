#include <stdio.h>
#include <stdlib.h>
#include <btcode.h>

int parse_file(char const *filename, uint8_t **ptr, unsigned int *p_size);

static inline int
usage(int ret)
{
    fprintf(stderr, "usage:\n"
                    "    " PROGRAM_NAME " <file>\n");
    return ret;
}

int
main(int argc, char **argv)
{
    if (argc == 1)
        return usage(1);

    int ret = 0;

    uint8_t *inbuf = NULL;
    uint8_t *outbuf = NULL;
    unsigned int inbuf_size;
    if (parse_file(*++argv, &inbuf, &inbuf_size))
        goto error;

#if BTCODE_ENCODE
    int r = btcode_encode(&outbuf, inbuf, inbuf_size);
#else
    int r = btcode_decode(&outbuf, inbuf, inbuf_size);
#endif
    if (r < 0)
        goto error;

    goto ret;

error:
    ret = BTCODE_EGENERIC;
ret:
    free(inbuf);
    free(outbuf);
    return ret;
}
