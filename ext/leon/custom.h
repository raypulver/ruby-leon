#ifndef CUSTOM_H
#include <types.h>

typedef struct _custom_type_t {
  uint8_t active;
  VALUE encode;
  VALUE decode;
  size_t size;
} custom_type_t;

typedef struct _custom_type_check_t {
  VALUE detect;
  uint8_t constant;
} custom_type_check_t;

typedef struct _custom_type_index_t {
  custom_type_t types[0x100];
  custom_type_check_t type_check[0x100];
  size_t len;
} custom_type_index_t;

extern custom_type_index_t custom_types;

void type_index_init(void);
void type_zero(size_t);
void type_check_splice(size_t);
void type_check_push(VALUE, uint8_t);

#define CUSTOM_TYPE(idx) custom_types.types[idx]
#define CUSTOM_TYPE_LEN() custom_types.len
#define CUSTOM_TYPE_ACTIVE(idx) ((CUSTOM_TYPE(idx)).active)
#define CUSTOM_TYPE_ENCODE(idx) ((CUSTOM_TYPE(idx)).encode)
#define CUSTOM_TYPE_DECODE(idx) ((CUSTOM_TYPE(idx)).decode)
#define CUSTOM_TYPE_SIZE(idx) ((CUSTOM_TYPE(idx)).size)
#define CUSTOM_TYPE_CHECK(idx) (custom_types.type_check[idx])
#define CUSTOM_TYPE_CHECK_CONSTANT(idx) (custom_types.type_check[idx].constant)
#define CUSTOM_TYPE_CHECK_DETECT(idx) (custom_types.type_check[idx].detect)

VALUE define_type(int, VALUE *, VALUE);
VALUE undefine_type(VALUE, VALUE);

#endif
