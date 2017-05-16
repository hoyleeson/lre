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


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifdef __cplusplus
extern "C" {
#endif


void *xzalloc(int size);
int xread(int fd, void* to, int len);
int xwrite(int fd, const void* from, int len);
void* xrealloc(void* block, size_t  size);
char *xgetline(char *buf, int size, FILE *file);

int is_digit(const char *str);
int is_double_digit(const char *str);

#define  xrenew(p,count)  (p) = xrealloc((p),sizeof(*(p))*(count))

#ifdef __cplusplus
}
#endif


#endif

