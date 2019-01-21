#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
copy_to_tmpbuf(char **p_tmp, int *p_tmpsz, char const buf[], int bufsz)
{
    char *tmp = *p_tmp;
    int tmpsz = *p_tmpsz + bufsz;

    tmp = !tmp
        ? malloc(tmpsz * sizeof(*tmp))
        : realloc(tmp, tmpsz * sizeof(*tmp));
    if (!tmp) return 1;
    memcpy(tmp + tmpsz - bufsz, buf, bufsz);

    *p_tmp = tmp;
    *p_tmpsz = tmpsz;
    return 0;
}

static int
fill_table(uint8_t **p_bt, unsigned int n, char buf[], int bufsz)
{
    for (int i = 0; i < bufsz; ++i)
        if (buf[i] != '\n')
        {
            int bit = buf[i] - '0';
            if (bit >> 1) return 1;
            **p_bt = bit;
            ++*p_bt;
        }
    return 0;
}

int
parse_file(char const *filename, uint8_t **p_bt, unsigned int *p_n)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        /* TODO error cause */
        return 1;
    }

    int ret = 0;

    uint8_t *bt = NULL;
    char *tmp = NULL;
    int tmpsz = 0;
    while (1)
    {
        char buf[1024 + 1];
        int r = read(fd, buf, 1024);
        if (r == -1) goto read_error;
        buf[r] = '\0';
        char *nl = strchr(buf, '\n');
        if (!bt)
        {
            if (nl)
            {
                int size_1d = tmpsz + nl - buf;
                int size_2d = size_1d * size_1d;
                bt = malloc(size_2d * sizeof(**p_bt));
                *p_bt = bt;
                *p_n = size_1d;
                if (fill_table(&bt, *p_n, tmp, tmpsz))
                    goto input_error;
            }
            else
                if (copy_to_tmpbuf(&tmp, &tmpsz, buf, r))
                    goto error;
        }
        if (bt)
            if (fill_table(&bt, *p_n, buf, r))
                goto input_error;

        if (r < 1024)
            break;

        /* FIXME crashes if num_rows > num_cols */
    }

    if (bt - *p_bt < *p_n * *p_n)
        goto input_error;

    goto ret;

input_error:
    /* TODO */
    goto error;
read_error:
    /* TODO */
    goto error;
error:
    free(*p_bt);
    ret = 1;
ret:
    close(fd);
    return ret;
}
