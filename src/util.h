#ifndef UTIL_H__
#define UTIL_H__

#define errorf(fmt, ...) _errorf(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

void _errorf(char *file, int line, const char *fmt, ...);
char *format(const char *fmt, ...);
char *unescape(const char *str);

#endif
