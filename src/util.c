#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "buffer.h"

void _errorf(char *file, int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d [ERROR]: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

char *format(const char *fmt, ...)
{
    char *s;
    va_list ap;
    int size;

    va_start(ap, fmt);
    size = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    s = malloc(sizeof(char) * (size + 1));
    va_start(ap, fmt);
    vsprintf(s, fmt, ap);
    va_end(ap);
    return s;
}

char *unescape(const char *str)
{
    buffer_t *buf;
    char *s;

    buf = make_buffer();
    for (; *str; str++) {
        switch (*str) {
        case '\"':
            buffer_push(buf, "\\\"", 2);
            break;
        case '\\':
            buffer_push(buf, "\\\\", 2);
            break;
        case '\b':
            buffer_push(buf, "\\b", 2);
            break;
        case '\f':
            buffer_push(buf, "\\f", 2);
            break;
        case '\n':
            buffer_push(buf, "\\n", 2);
            break;
        case '\r':
            buffer_push(buf, "\\r", 2);
            break;
        case '\t':
            buffer_push(buf, "\\t", 2);
            break;

        default:
            buffer_push(buf, str, 1);
            break;
        }
    }

    s = malloc(sizeof(char) * (buf->top + 1));
    memcpy(s, buf->stack, buf->top);
    s[buf->top] = '\0';
    free_buffer(buf);
    return s;
}
