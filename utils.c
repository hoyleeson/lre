#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#include "log.h"

void *xzalloc(int size)
{
    void *ptr;
    ptr = malloc(size);
    if (!ptr)
        fatal("Failed to alloc memory.");;

    memset(ptr, 0, size);
    return ptr;
}

int xread(int fd, void *to, int len)
{
    int ret;

    do {
        ret = read(fd, to, len);
    } while (ret < 0 && errno == EINTR);

    return ret;
}

int xwrite(int fd, const void *from, int len)
{
    int ret;

    do {
        ret = write(fd, from, len);
    } while (ret < 0 && errno == EINTR);

    return ret;
}

void *xrealloc(void *block, size_t  size)
{
    void *p = realloc(block, size);

    if (p == NULL && size > 0)
        fatal("not enough memory");

    return p;
}

int xsnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsnprintf(buf, size, fmt, args);
    va_end(args);

    return (i > size) ? size : i;
}

int is_digit(const char *str)
{
    char *p = (char *)str;

    while (*p != '\0') {
        if (!isdigit(*p))
            return 0;
        p++;
    }
    return 1;
}

int is_double_digit(const char *str)
{
    int flags = 0;
    char *p = (char *)str;

    if (*p == '.')
        return 0;

    while (*p != '\0') {
        if (*p == '.')
            flags++;

        if (!isdigit(*p))
            return 0;
        p++;
    }

    if (flags != 1)
        return 0;

    return 1;
}

char *xgetline(char *buf, int size, FILE *file)
{
    int cnt = 0;
    int eof = 0;
    int eol = 0;
    int c;

    if (size < 1) {
        return NULL;
    }

    while (cnt < (size - 1)) {
        c = getc(file);
        if (c == EOF) {
            eof = 1;
            break;
        }

        *(buf + cnt) = c;
        cnt++;

        if (c == '\n') {
            eol = 1;
            break;
        }
    }

    /* Null terminate what we've read */
    *(buf + cnt) = '\0';

    if (eof) {
        if (cnt) {
            return buf;
        } else {
            return NULL;
        }
    } else if (eol) {
        return buf;
    } else {
        /* The line is too long.  Read till a newline or EOF.
         *          * If EOF, return null, if newline, return an empty buffer.
         *                   */
        while (1) {
            c = getc(file);
            if (c == EOF) {
                return NULL;
            } else if (c == '\n') {
                *buf = '\0';
                return buf;
            }
        }
    }
}
