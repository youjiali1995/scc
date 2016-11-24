#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "util.h"

void errorf(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "[ERROR] %s:%d: ", __FILE__, __LINE__);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}
