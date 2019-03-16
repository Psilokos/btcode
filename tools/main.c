#include <stdio.h>
#include <string.h>
#include <btcode.h>

int read_input(char const *filename,
               uint8_t **inbuf_ptr, size_t *inbuf_size_ptr);

int write_output(char const *filename,
                 uint8_t *outbuf, size_t outbuf_size);

static inline int
usage(int ret)
{
    fprintf(stderr, "usage:\n"
                    "    " PROGRAM_NAME " <file>\n");
    return ret;
}

static inline int
make_ofname(char **ofname_ptr, char const *ifname)
{
#if BTCODE_ENCODE
    char const *fix = ".bt";
#else
    char const *fix = "dec_";
#endif

    *ofname_ptr = malloc(strlen(ifname) + strlen(fix) + 1);
    if (!*ofname_ptr)
        return BTCODE_ERR(ENOMEM);

#if BTCODE_ENCODE
    strcpy(*ofname_ptr, ifname);
    strcat(*ofname_ptr, fix);
#else
    /* FIXME extract filename from path */
    strcpy(*ofname_ptr, fix);
    strcat(*ofname_ptr, ifname);
#endif

    return BTCODE_SUCCESS;
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

    errno = 0;

    char *ifname = *++argv;
    char *ofname = NULL;

    // FIXME remove init
    uint8_t *inbuf = NULL;
    uint8_t *outbuf = NULL;
    size_t inbuf_size = 0;
    size_t outbuf_size = 0;

    int ret = read_input(ifname, &inbuf, &inbuf_size);
    if (ret) goto ret;

    ret = btcode_code(&outbuf, &outbuf_size, inbuf, inbuf_size);
    free(inbuf);
    if (ret) goto ret;

    ret = make_ofname(&ofname, ifname);
    if (ret) goto ret;

    ret = write_output(ofname, outbuf, outbuf_size);

ret:
    free(ofname);
    free(outbuf);
    return ret;
}
