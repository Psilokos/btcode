#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

static void
dct_init_coefs(int64_t dct_coefs[N_MAX * N_MAX])
{
}

#define BUFSZ   (1 << 14)

#define dprintf(fd, ...) \
do { \
    int max = BUFSZ - cnt; \
    int r = snprintf(buf + cnt, max, __VA_ARGS__); \
    if (r < max--) \
        cnt += r; \
    else \
    { \
        if (write(fd, buf, BUFSZ - 1) <= 0) return 1; \
        r = snprintf(buf, BUFSZ, __VA_ARGS__); \
        memmove(buf, buf + max, r - max + 1); \
        cnt = r - max; \
    } \
} while (0)

#define write_table(table, w, h, type, t) \
do { \
    dprintf(fd, "\n" #type " const tb_" #table "[" #h " * " #w "] = \n{\n"); \
    for (int i = 0; i < h; ++i) \
    { \
        for (int j = 0; j < w; ++j) \
            dprintf(fd, "    %" #t ",", table[i * w + j]); \
        dprintf(fd, "\n"); \
    } \
    dprintf(fd, "};\n"); \
} while (0)

static inline int
write_tables_c(int fd, int64_t const dct_coefs[N_MAX * N_MAX])
{
    char buf[BUFSZ];
    int cnt = 0;
    dprintf(fd, "#include \"tables.h\"\n");
    write_table(dct_coefs, N_MAX, N_MAX, int64_t, li);
    return write(fd, buf, cnt) <= 0;
}

#undef write_table

#define write_table(table, w, h, type) \
do { \
    dprintf(fd, "\nextern " #type " const tb_" #table "[" #h " * " #w "];\n"); \
} while (0)

static inline int
write_tables_h(int fd)
{
    char buf[BUFSZ];
    int cnt = 0;
    dprintf(fd, "#ifndef TABLES_H\n"
                "# define TABLES_H\n\n"
                "# include <stdint.h>\n");
    write_table(dct_coefs, N_MAX, N_MAX, int64_t);
    dprintf(fd, "\n#endif\n");
    return write(fd, buf, cnt) <= 0;
}

#undef write_table

int
main(void)
{
    int64_t *dct_coefs = malloc(N_MAX * N_MAX * sizeof(*dct_coefs));
    if (!dct_coefs) return 1;
    dct_init_coefs(dct_coefs);

    int fd;
#define OPEN_FILE(filename) \
do { \
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); \
    if (fd == -1) \
        return 1; \
} while (0)

    OPEN_FILE(CURDIR "tables.c");
    write_tables_c(fd, dct_coefs);
    close(fd);

    OPEN_FILE(CURDIR "tables.h");
    write_tables_h(fd);
    close(fd);

    free(dct_coefs);
    return 0;
}
