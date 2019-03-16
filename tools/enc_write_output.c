#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

int
write_output(char const *filename,
             uint8_t *outbuf, size_t size)
{
    int ret = BTCODE_SUCCESS;
    char const *errfmt;

    int fd = open(filename,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        errfmt = "failed to open %s: %s\n";
        goto error;
    }

    if (write(fd, outbuf, size) == -1)
    {
        errfmt = "failed to write to %s: %s\n";
        goto error;
    }

    goto ret;

error:
    fprintf(stderr, errfmt, filename, strerror(errno));
    ret = BTCODE_ERR(errno);
ret:
    close(fd);
    return ret;
}
