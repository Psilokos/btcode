#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

int
read_input(char const *filename,
           uint8_t **outbuf_ptr, size_t *size_ptr)
{
    int ret = BTCODE_SUCCESS;
    char const *errfct;
    uint8_t *outbuf = NULL;

    int fd = open(filename, O_RDONLY);
    if (fd == -1) { errfct = "open"; goto file_error; }

    struct stat statbuf;
    ret = fstat(fd, &statbuf);
    if (ret == -1) { errfct = "stat"; goto file_error; }
    size_t size = statbuf.st_size;

    outbuf = malloc(size);
    if (!outbuf) goto alloc_error;

    ssize_t rdsz = read(fd, outbuf, size);
    if (rdsz == -1) { errfct = "read"; goto file_error; }

    *outbuf_ptr = outbuf;
    *size_ptr = size;
    goto ret;

alloc_error:
    fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
    goto error;
file_error:
    fprintf(stderr, "failed to %s %s: %s\n",
            errfct, filename, strerror(errno));
error:
    free(outbuf);
    ret = BTCODE_ERR(errno);
ret:
    close(fd);
    return ret;
}
