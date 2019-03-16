#ifndef COMMON_H
# define COMMON_H

# include <errno.h>
# include <stddef.h>
# include <stdint.h>
# include <stdlib.h>

# ifndef NDEBUG
#  include <stdio.h>
# endif

# define BTCODE_SUCCESS     0
# define BTCODE_EGENERIC    1

/* handle esoteric platforms with already negated error codes */
# if ENOMEM > 0
#  define BTCODE_ERR(x) (-(x))
# else
#  define BTCODE_ERR(x) (x)
# endif

/* handy posix wrappers */
# define free(p) free((void *)p)

#endif
