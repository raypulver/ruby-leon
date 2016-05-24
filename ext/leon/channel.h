#ifndef CHANNEL_H
#define CHANNEL_H

#include <stdint.h>
#include <ruby.h>
#include <buffer.h>

#define TYPE_LITERAL 0x00
#define TYPE_ARRAY 0x01
#define TYPE_MULTIARRAY 0x02
#define TYPE_OBJECT 0x03
#define TYPE_KEY 0x04

#define TYPE_ARRAY_DEFAULT_ALLOC 0x80

#define SPEC_TYPE_P(spec) ((spec)->type)
#define SPEC_LIT_P(spec) ((spec)->value.literal)
#define SPEC_ARR_P(spec) ((spec)->value.arr)
#define SPEC_VALUES_P(spec) SPEC_ARR_P(spec)->values
#define SPEC_LEN_P(spec) SPEC_ARR_P(spec)->len
#define SPEC_ALLOC_P(spec) SPEC_ARR_P(spec)->alloc
#define SPEC_KEY_P(spec) ((spec)->value.key)
#define IS_DYNAMIC(spec) (((void *) spec) == (void *) 0)

extern VALUE cChannel;

struct _spec_value_t;

typedef struct _type_array_t {
  uint32_t len;
  uint32_t alloc;
  struct _spec_value_t **values;
} type_array_t;

typedef struct _spec_value_t {
  uint8_t type;
  union {
    uint8_t literal;
    type_array_t *arr;
    VALUE key;
  } value;
} spec_value_t;

spec_value_t *spec_value_new(void);
void spec_value_free(spec_value_t *);
type_array_t *type_array_new(void);
type_array_t *type_array_new_single_alloc(void);
void type_array_push(type_array_t *, spec_value_t *);
void sort_spec(spec_value_t *);
void append_spec(spec_value_t *, spec_value_t *);
void load_spec(spec_value_t *, VALUE);
VALUE initialize_channel(VALUE, VALUE);
VALUE channel_from_sorted(VALUE, VALUE);
VALUE channel_encode(VALUE, VALUE);
VALUE channel_decode(VALUE, VALUE);
VALUE alloc_channel(VALUE);
VALUE channel_dump(VALUE, VALUE);
VALUE channel_load(VALUE, VALUE);
void free_channel(void *);
void encode_int(buffer_t *, int64_t, uint8_t);
void encode_double(buffer_t *, double, uint8_t);
VALUE buffer_read_string_utf8(buffer_t *, size_t *);
VALUE buffer_read_string_utf16(buffer_t *, size_t *);
VALUE buffer_read_string_ascii(buffer_t *, size_t *);
size_t set_str_ascii(buffer_t *, size_t, VALUE);
size_t set_str_utf8(buffer_t *, size_t, VALUE);
size_t set_str_utf16(buffer_t *, size_t, VALUE);
size_t append_str_ascii(buffer_t *, VALUE);
size_t append_str_utf8(buffer_t *, VALUE);
size_t append_str_utf16(buffer_t *, VALUE);
void encode_sym(buffer_t *, VALUE);
void encode_value(buffer_t *, VALUE, uint8_t);
void leon_encode(buffer_t *, VALUE, spec_value_t *);
VALUE leon_decode(VALUE, size_t *, spec_value_t *);

#endif
