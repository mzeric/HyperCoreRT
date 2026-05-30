#include <strings.h>
#include <stdint.h>
#include "safe_printf.h"

extern "C" {

#pragma GCC diagnostic ignored "-Wnonnull-compare"

void __memset(void*src, int v, int64_t cnt) {
    char *p = (char*)src;

    while (cnt-- > 0){
        *p++ = v;
    }
}

void *memmove(void *dst, const void *src, size_t cnt) {
    char *pd = (char*)dst;
    const char *ps = (const char*)src;
    for (size_t i = 0; i < cnt; ++i)
        *pd ++ = *ps++;
    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *cu1, *cu2;

    int res = 0;

    // goto cc;
    int count = n;

    // tail
    for (cu1 = (const uint8_t*)s1, cu2 = (const uint8_t*)s2; 0 < count; ++cu1, ++cu2, count--)
        if ((res = *cu1 - *cu2) != 0)
            break;

    return res;
}


size_t strlen(const char *s)
{
    size_t ret = 0;
    if (!s)
        return 0;
    /* search end of string */
    for (; *s != '\0'; s++, ret++);
    return ret;
}
char *strcpy(char *dest, const char *src)
{
	char *save = dest;
	/* copy string */
	for (; (*dest = *src) != '\0'; dest++, src++);
	return save;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    if(!dest || !src)
        return NULL;
    char *save = dest;
    if (n > 0) {
        /* copy string with limit */
        for (; n && ((*dest = *src) != '\0'); dest++, src++, n--);
    }
    return save;
}
int strcmp(const char *a, const char *b)
{
    if(!a || !b)
        return -1;
    /* search first diff or end of string */
    for (; *a == *b && *a != '\0'; a++, b++);
    return *a - *b;
}

} /* extern "C" */