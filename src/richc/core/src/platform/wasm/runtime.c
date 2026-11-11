// runtime.c
//
// The four libc symbols compilers may call on their own (for struct copies, or at -O0). There is no
// libc on wasm32, so we supply them.

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t size);
void *memmove(void *dst, const void *src, size_t size);
void *memset(void *dst, int value, size_t size);
int memcmp(const void *a, const void *b, size_t size);

// With bulk memory these builtins are single memory.copy / memory.fill instructions, not calls back
// into the functions they live in.
void *memcpy(void *dst, const void *src, size_t size)
{
    __builtin_memcpy(dst, src, size);
    return dst;
}


void *memmove(void *dst, const void *src, size_t size)
{
    __builtin_memmove(dst, src, size);
    return dst;
}


void *memset(void *dst, int value, size_t size)
{
    __builtin_memset(dst, value, size);
    return dst;
}


// No instruction for this one, so we walk the bytes.
int memcmp(const void *a, const void *b, size_t size)
{
    const uint8_t *pa = a;
    const uint8_t *pb = b;

    for (size_t i = 0; i < size; i++) {
        if (pa[i] != pb[i]) {
            return pa[i] < pb[i] ? -1 : 1;
        }
    }
    return 0;
}
