#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

int
write_output(char const *filename,
             uint8_t *btable, size_t n)
{
    int ret;
    char const *errfct;

    int fd = open(filename,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR);
    if (fd == -1) { errfct = "open"; goto error; }

    for (size_t i = 0; i < n * n; ++i)
        btable[i] += '0';
    for (size_t i = 0; i < n; ++i)
    {
        ret = write(fd, btable + i * n, n);
        if (ret == -1) { errfct = "write"; goto error; }

        char const newline = '\n';
        ret = write(fd, &newline, 1);
        if (ret == -1) { errfct = "write"; goto error; }
    }

    ret = BTCODE_SUCCESS;
    goto ret;

error:
    fprintf(stderr, "failed to %s %s: %s\n",
            errfct, filename, strerror(errno));
    ret = BTCODE_ERR(errno);
ret:
    close(fd);
    return ret;
}
