#include <stddef.h>
#include <reverse.h>

void *reverse_memcpy(void *dest, const void *src, size_t sz) {
  size_t i;
  for (i = 0; i < sz; ++i) {
    ((char *) dest)[i] = ((const char *) src)[sz - i - 1];
  }
  return dest;
}
