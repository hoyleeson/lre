#ifndef _FILES_WATCHD_UTILS_H
#define _FILES_WATCHD_UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME_32 0x9e370001UL


static inline uint32_t hash_str(const char *str, int bits)
{
    unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;

    while (*str) {
        hash = hash * seed + (*str++);
    }

    return hash >> (32 - bits);
}

static inline uint32_t hash_32(int val, unsigned int bits)
{
    /* On some cpus multiply is faster, on others gcc will do shifts */
    uint32_t hash = val * GOLDEN_RATIO_PRIME_32;

    /* High bits are more random, so use them. */
    return hash >> (32 - bits);
}


/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({          \
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})


#define min(a, b)   (((a) > (b)) ? (b) : (a))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define assert_ptr(ptr)  	\
	do { 					\
		if((ptr) == NULL) { \
			fatal("assert_ptr: Null Pointer Exception.(%s:%d)", __func__, __LINE__); \
		} 					\
	} while(0)


#ifdef __cplusplus
extern "C" {
#endif


void *xzalloc(int size);
int xread(int fd, void *to, int len);
int xwrite(int fd, const void *from, int len);
void *xrealloc(void *block, size_t  size);
char *xgetline(char *buf, int size, FILE *file);
int xsnprintf(char *buf, size_t size, const char *fmt, ...);

int is_digit(const char *str);
int is_double_digit(const char *str);

#define  xrenew(p,count)  (p) = xrealloc((p),sizeof(*(p))*(count))

/* conversion functions */
static inline unsigned char str2hexnum(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0; /* foo */
}

static inline unsigned long str2hex(unsigned char *str)
{
    int value = 0;

    while (*str) {
        value = value << 4;
        value |= str2hexnum(*str++);
    }

    return value;
}

#ifdef __cplusplus
}
#endif


#endif

