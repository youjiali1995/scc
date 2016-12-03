#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "util.h"

void _errorf(char *file, int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d [ERROR]: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}
