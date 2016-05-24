#ifndef BUFFER_H
#define BUFFER_H

#include <ruby.h>
#include <stddef.h>

#define BUFFER_DEFAULT_ALLOC 0x10000
#define BUFFER_LEN_P(p) ((p)->len)
#define BUFFER_LEN(p) ((p).len)
#define BUFFER_ALLOC(p) ((p).data.master.alloc)
#define BUFFER_ALLOC_P(p) ((p)->data.master.alloc)
#define BUFFER_DATA_P(p) ((p)->data.master.buf)
#define BUFFER_DATA(p) ((p).data.master.buf)
#define BUFFER_MASTER_P(p) ((p)->data.slave.master)
#define BUFFER_OFFSET_P(p) ((p)->data.slave.offset)
#define BUFFER_SLICE_LEN_P(p) ((p)->len)
#define BUFFER_IS_SLAVE_P(p) ((p)->slave)
#define BUFFER_IS_SLAVE(p) ((p).slave)
#define BUFFER_ON_HEAP(p) ((p).data.master.heap)
#define BUFFER_ON_HEAP_P(p) ((p)->data.master.heap)

#define BUFFER_INSPECT_MAX_BYTES 0x10

#define BUFFER_STACK(data, buf) do { \
    BUFFER_IS_SLAVE(data) = 0; \
    BUFFER_ALLOC(data) = BUFFER_DEFAULT_ALLOC; \
    BUFFER_LEN(data) = 0; \
    BUFFER_DATA(data) = buf; \
    BUFFER_ON_HEAP(data) = 0; \
  } while(0)

extern VALUE cBuffer;

typedef struct _buffer_t {
  uint8_t slave;
  size_t len;
  union {
    struct {
      char *buf;
      size_t alloc;
      uint8_t heap;
    } master;
    struct {
      VALUE master;
      size_t offset;
    } slave;
  } data;
} buffer_t;

buffer_t *buffer_new(void);
buffer_t *buffer_new_stack(buffer_t *);

void buffer_append(buffer_t *, const void *, size_t);
void buffer_appendr(buffer_t *, const void *, size_t);
void buffer_set(buffer_t *, const void *, size_t, long);
void buffer_setr(buffer_t *, const void *, size_t, long);
void buffer_free(buffer_t *);

VALUE alloc_buffer(VALUE);
void free_buffer(void *);
VALUE buffer_initialize(int, VALUE *, VALUE);
VALUE buffer_write_uint8(int, VALUE *, VALUE);
VALUE buffer_write_int8(int, VALUE *, VALUE);
VALUE buffer_write_uint16_le(int, VALUE *, VALUE);
VALUE buffer_write_uint16_be(int, VALUE *, VALUE);
VALUE buffer_write_int16_le(int, VALUE *, VALUE);
VALUE buffer_write_int16_be(int, VALUE *, VALUE);
VALUE buffer_write_uint32_le(int, VALUE *, VALUE);
VALUE buffer_write_uint32_be(int, VALUE *, VALUE);
VALUE buffer_write_int32_le(int, VALUE *, VALUE);
VALUE buffer_write_int32_be(int, VALUE *, VALUE);
VALUE buffer_write_float_le(int, VALUE *, VALUE);
VALUE buffer_write_float_be(int, VALUE *, VALUE);
VALUE buffer_write_double_le(int, VALUE *, VALUE);
VALUE buffer_write_double_be(int, VALUE *, VALUE);
VALUE buffer_read_uint8(VALUE, VALUE);
VALUE buffer_read_int8(VALUE, VALUE);
VALUE buffer_read_uint16_le(VALUE, VALUE);
VALUE buffer_read_uint16_be(VALUE, VALUE);
VALUE buffer_read_int16_le(VALUE, VALUE);
VALUE buffer_read_int16_be(VALUE, VALUE);
VALUE buffer_read_uint32_le(VALUE, VALUE);
VALUE buffer_read_uint32_be(VALUE, VALUE);
VALUE buffer_read_int32_le(VALUE, VALUE);
VALUE buffer_read_int32_be(VALUE, VALUE);
VALUE buffer_read_float_le(VALUE, VALUE);
VALUE buffer_read_float_be(VALUE, VALUE);
VALUE buffer_read_double_le(VALUE, VALUE);
VALUE buffer_read_double_be(VALUE, VALUE);
VALUE buffer_compare(VALUE, VALUE);
VALUE buffer_operator_bracket_equals(VALUE, VALUE, VALUE);
VALUE buffer_operator_bracket(VALUE, VALUE);
VALUE buffer_length(VALUE);
VALUE buffer_length_equals(VALUE, VALUE);
VALUE buffer_inspect(VALUE);
VALUE buffer_to_s(VALUE);
VALUE buffer_to_hex(VALUE);
VALUE buffer_from_hex(VALUE, VALUE);
VALUE buffer_equals(VALUE, VALUE);
VALUE buffer_dump(VALUE, VALUE);
VALUE buffer_load(VALUE, VALUE);
VALUE buffer_to_base64(VALUE);
VALUE buffer_from_base64(VALUE, VALUE);
VALUE buffer_to_file(int, VALUE *, VALUE);
VALUE buffer_from_file(VALUE, VALUE);
VALUE buffer_slice(int, VALUE *, VALUE);
VALUE buffer_slice_bang(int, VALUE *, VALUE);
VALUE make_view(int, VALUE *, VALUE);
VALUE is_view(VALUE);
void buffer_from_string(VALUE, VALUE);

#define ITERATOR_POS_P(p) ((p)->pos)
#define ITERATOR_OFFSET_P(p) ((p)->offset)
#define ITERATOR_BUF_P(p) ((p)->buf)
#define ITERATOR_RBBUF_P(p) ((p)->rbbuf)

extern VALUE cBufferIterator;

typedef struct _buffer_iterator_t {
  size_t pos;
  size_t offset;
  buffer_t *buf;
  VALUE rbbuf;
} buffer_iterator_t;

buffer_iterator_t *buffer_iterator_new(void);
void buffer_iterator_free(buffer_iterator_t *);
VALUE alloc_iterator(VALUE);
VALUE buffer_iterator_initialize(int, VALUE *, VALUE);
VALUE buffer_iterator_write_uint8(VALUE, VALUE);
VALUE buffer_iterator_write_int8(VALUE, VALUE);
VALUE buffer_iterator_write_uint16_le(VALUE, VALUE);
VALUE buffer_iterator_write_uint16_be(VALUE, VALUE);
VALUE buffer_iterator_write_int16_le(VALUE, VALUE);
VALUE buffer_iterator_write_int16_be(VALUE, VALUE);
VALUE buffer_iterator_write_uint32_le(VALUE, VALUE);
VALUE buffer_iterator_write_uint32_be(VALUE, VALUE);
VALUE buffer_iterator_write_int32_le(VALUE, VALUE);
VALUE buffer_iterator_write_int32_be(VALUE, VALUE);
VALUE buffer_iterator_write_float_le(VALUE, VALUE);
VALUE buffer_iterator_write_float_be(VALUE, VALUE);
VALUE buffer_iterator_write_double_le(VALUE, VALUE);
VALUE buffer_iterator_write_double_be(VALUE, VALUE);
VALUE buffer_iterator_write_value(VALUE, VALUE, VALUE);
VALUE buffer_iterator_write_string_utf8(VALUE, VALUE);
VALUE buffer_iterator_write_string_utf16(VALUE, VALUE);
VALUE buffer_iterator_write_string_ascii(VALUE, VALUE);
VALUE buffer_iterator_read_uint8(VALUE);
VALUE buffer_iterator_read_int8(VALUE);
VALUE buffer_iterator_read_uint16_le(VALUE);
VALUE buffer_iterator_read_uint16_be(VALUE);
VALUE buffer_iterator_read_int16_le(VALUE);
VALUE buffer_iterator_read_int16_be(VALUE);
VALUE buffer_iterator_read_uint32_le(VALUE);
VALUE buffer_iterator_read_uint32_be(VALUE);
VALUE buffer_iterator_read_int32_le(VALUE);
VALUE buffer_iterator_read_int32_be(VALUE);
VALUE buffer_iterator_read_float_le(VALUE);
VALUE buffer_iterator_read_float_be(VALUE);
VALUE buffer_iterator_read_double_le(VALUE);
VALUE buffer_iterator_read_double_be(VALUE);
VALUE buffer_iterator_read_value(VALUE, VALUE);
VALUE buffer_iterator_read_string_utf8(VALUE);
VALUE buffer_iterator_read_string_utf16(VALUE);
VALUE buffer_iterator_read_string_ascii(VALUE);
VALUE buffer_iterator_reset(VALUE);
VALUE buffer_iterator_buffer(VALUE);
VALUE buffer_iterator_position(VALUE);
VALUE iterator_from_native_buffer(buffer_t *, size_t);
#endif
