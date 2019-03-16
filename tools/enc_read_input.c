#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

static int
copy_to_tmpbuf(char **tmp_ptr, int *tmpsize_ptr,
               char const buf[], int bufsize)
{
    char *tmp = *tmp_ptr;
    int tmpsize = *tmpsize_ptr + bufsize;

    tmp = realloc(tmp, tmpsize);
    if (!tmp) return BTCODE_ERR(errno);
    memcpy(tmp + tmpsize - bufsize, buf, bufsize);

    *tmp_ptr = tmp;
    *tmpsize_ptr = tmpsize;
    return BTCODE_SUCCESS;
}

static int
fill_table(uint8_t **btable_ptr, unsigned int n,
           char buf[], int size)
{
    static unsigned int ncnt = 0;
    static uint8_t const *lastn;
    if (!lastn)
        lastn = *btable_ptr;

    uint8_t *btable = *btable_ptr;
    for (int i = 0; i < size; ++i)
        if (buf[i] != '\n')
        {
            int bit = buf[i] - '0';
            if (bit >> 1) return 1;
            *btable = bit;
            ++btable;
        }
        else
        {
            ++ncnt;
            if (btable - lastn != n || ncnt > n || (ncnt == n && i + 1 < size))
                return BTCODE_EGENERIC;
            lastn = btable;
        }

    *btable_ptr = btable;
    return BTCODE_SUCCESS;
}

int
read_input(char const *filename,
           uint8_t **btable_ptr, size_t *nptr)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr,"failed to open %s: %s\n",
                filename, strerror(errno));
        return BTCODE_ERR(errno);
    }

    int ret = 0;

    uint8_t *btable = NULL;
    char *tmp = NULL;
    int tmpsize = 0;
    while (1)
    {
        char buf[1024 + 1];
        int rdsz = read(fd, buf, 1024);
        if (rdsz == -1) goto read_error;
        buf[rdsz] = '\0';
        char *newline = strchr(buf, '\n');
        if (!btable)
        {
            if (newline)
            {
                int n = tmpsize + newline - buf;
                int n2 = n * n;
                btable = malloc(n2 * sizeof(*btable));
                *btable_ptr = btable;
                *nptr = n;
                if (fill_table(&btable, *nptr, tmp, tmpsize))
                    goto input_error;
            }
            else
                if (copy_to_tmpbuf(&tmp, &tmpsize, buf, rdsz))
                    goto error;
        }
        if (btable && fill_table(&btable, *nptr, buf, rdsz))
            goto input_error;

        if (rdsz < 1024)
            break;
    }

    if (btable - *btable_ptr < *nptr * *nptr)
        goto input_error;

    goto ret;

input_error:
    fprintf(stderr, "syntax error\n");
    ret = BTCODE_EGENERIC;
    goto error;
read_error:
    fprintf(stderr, "failed to read from %s: %s\n",
            filename, strerror(errno));
    ret = BTCODE_ERR(errno);
    goto error;
error:
    free(*btable_ptr);
ret:
    free(tmp);
    close(fd);
    return ret;
}
