#include <stdint.h>
#include <endianness.h>

uint8_t isLE;

uint8_t is_little_endian() {
  union no_alias {
    char bytes[4];
    uint32_t i;
  } na = { .i = 1 };
  na.i = 1;
  if (na.bytes[0]) return 1;
  else return 0;
}
