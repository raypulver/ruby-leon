#include <stddef.h>
#include <stdint.h>

void swizzle(void *start, size_t len) {
  void *end = (void *) ((char *) start + len - 1);
  while (start < end) {
    uint8_t tmp = *((char *) start);
    *((char *) start) = *((char *) end);
    start = (void *) ((char *) start + 1);
    *((char *) end) = tmp;
    end = (void *) ((char *) end - 1);
  }
}
