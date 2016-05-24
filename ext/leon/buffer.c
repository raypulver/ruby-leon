#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <reverse.h>
#include <endianness.h>
#include <base64.h>
#include <stubs.h>
#include <types.h>
#include <channel.h>
#include <buffer.h>

buffer_t *buffer_new(void) {
  buffer_t *retval = malloc(sizeof(buffer_t));
  if (!retval && errno == ENOMEM) rb_raise(rb_eNoMemError, "Buffer allocation failed.");
  BUFFER_IS_SLAVE_P(retval) = 0;
  BUFFER_DATA_P(retval) = malloc(BUFFER_DEFAULT_ALLOC);
  if (!BUFFER_DATA_P(retval) && errno == ENOMEM) rb_raise(rb_eNoMemError, "Buffer allocation failed.");
  BUFFER_ALLOC_P(retval) = BUFFER_DEFAULT_ALLOC;
  BUFFER_ON_HEAP_P(retval) = 1;
  BUFFER_LEN_P(retval) = 0;
  return retval;
}

buffer_t *buffer_new_stack(buffer_t *retval) {
  BUFFER_DATA_P(retval) = malloc(BUFFER_DEFAULT_ALLOC);
  if (!BUFFER_DATA_P(retval) && errno == ENOMEM) rb_raise(rb_eNoMemError, "Buffer allocation failed.");
  BUFFER_IS_SLAVE_P(retval) = 0;
  BUFFER_ALLOC_P(retval) = BUFFER_DEFAULT_ALLOC;
  BUFFER_ON_HEAP_P(retval) = 0;
  BUFFER_LEN_P(retval) = 0;
  return retval;
}

void buffer_free(buffer_t *buf) {
  if (!BUFFER_IS_SLAVE_P(buf)) free(BUFFER_DATA_P(buf));
  if (!BUFFER_ON_HEAP_P(buf)) free(buf);
}

VALUE is_view(VALUE self) {
  buffer_t *buffer_intern;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (BUFFER_IS_SLAVE_P(buffer_intern)) return Qtrue;
  else return Qfalse;
}

static size_t relative_bits(size_t a, size_t b) {
  size_t retval = 0;
  while (b < a) {
    ++retval;
    a >>= 1;
  }
  return retval;
} 
static size_t normalize_index(size_t len, long index) {
  if (index + (long) len < 0) goto err;
  if (index < 0) index += len;
  return index;
  err:
    rb_raise(rb_eRangeError, "Index %li out of range.", index);
}

VALUE make_view(int argc, VALUE *argv, VALUE self) {
  buffer_t *buffer_intern, *out_buffer;
  long start, end;
  size_t startadj, endadj, l;
  VALUE startv, endv, out = rb_funcall(cBuffer, i_new, 0);
  Data_Get_Struct(out, buffer_t, out_buffer);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  free(BUFFER_DATA_P(out_buffer));
  BUFFER_IS_SLAVE_P(out_buffer) = 1;
  BUFFER_MASTER_P(out_buffer) = self;
  rb_gc_mark(self);
  rb_scan_args(argc, argv, "11", &startv, &endv);
  Check_Type(startv, T_FIXNUM);
  start = NUM2LONG(startv);
  if (BUFFER_IS_SLAVE_P(buffer_intern)) l = BUFFER_SLICE_LEN_P(buffer_intern);
  else l = BUFFER_LEN_P(buffer_intern);
  startadj = normalize_index(l, start);
  if (endv == Qnil) {
    endadj = l;
  } else {
    Check_Type(endv, T_FIXNUM);
    end = NUM2LONG(endv);
    endadj = normalize_index(l, end);
  }
  if (endadj < startadj) rb_raise(rb_eRangeError, "End index must not be before start index.");
  BUFFER_SLICE_LEN_P(out_buffer) = endadj - startadj;
  BUFFER_OFFSET_P(out_buffer) = startadj;
  return out;
}

static void buffer_set_impl(buffer_t *buf, const void *d, size_t sz, long idx, long orig, size_t offset, size_t len, uint8_t top);

static void buffer_append_impl(buffer_t *buf, const void *d, size_t sz) {
  void *status;
  size_t last_alloc, sum, len, offset = 0;
  uint8_t top = 1;
  while (BUFFER_IS_SLAVE_P(buf)) {
    Data_Get_Struct(BUFFER_MASTER_P(buf), buffer_t, buf);
    if (top) {
      len = BUFFER_SLICE_LEN_P(buf);
      top = 0;
    }
    sum = sz + offset + len;
    if (sum > BUFFER_SLICE_LEN_P(buf)) BUFFER_SLICE_LEN_P(buf) = sum;
    offset += BUFFER_OFFSET_P(buf);
  }
  if (top) {
    len = BUFFER_LEN_P(buf);
    sum = offset + len + sz;
  }
  if (len + offset != BUFFER_LEN_P(buf)) {
    buffer_set_impl(buf, d, sz, (long) len, 0, offset, 0, 0);
    return;
  }
  if (sum > BUFFER_ALLOC_P(buf)) {
    last_alloc = BUFFER_ALLOC_P(buf);
    BUFFER_ALLOC_P(buf) <<= relative_bits(sum, BUFFER_ALLOC_P(buf));
    if (BUFFER_ON_HEAP_P(buf)) {
      status = realloc(BUFFER_DATA_P(buf), BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      BUFFER_DATA_P(buf) = status;
    } else {
      status = malloc(BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      memcpy(status, BUFFER_DATA_P(buf), last_alloc);
      BUFFER_ON_HEAP_P(buf) = 1;
      BUFFER_DATA_P(buf) = status;
    }
  }
  memcpy(BUFFER_DATA_P(buf) + BUFFER_LEN_P(buf) + offset, d, sz);
  BUFFER_LEN_P(buf) = sum;
}

void buffer_append(buffer_t *buf, const void *d, size_t sz) {
  buffer_append_impl(buf, d, sz);
}

static void buffer_set_impl(buffer_t *buf, const void *d, size_t sz, long idx, long orig, size_t offset, size_t len, uint8_t top) {
  void *status;
  size_t last_alloc, sum;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buf)) {
    Data_Get_Struct(BUFFER_MASTER_P(buf), buffer_t, parent_buffer);
    if (top) {
      len = BUFFER_SLICE_LEN_P(buf);
      orig = idx;
      idx = normalize_index(len, idx);
    }
    sum = idx + sz + offset; 
    buffer_set_impl(parent_buffer, d, sz, idx, orig, offset + BUFFER_OFFSET_P(buf), len, 0);
    if (BUFFER_SLICE_LEN_P(buf) < sum) BUFFER_SLICE_LEN_P(buf) = sum;
    return;
  }
  if (top) {
    len = BUFFER_LEN_P(buf);
    orig = idx;
    idx = normalize_index(len, idx);
  }
  sum = idx + sz + offset;
  if (sum > BUFFER_ALLOC_P(buf)) {
    last_alloc = BUFFER_ALLOC_P(buf);
    BUFFER_ALLOC_P(buf) <<= relative_bits(sum, BUFFER_ALLOC_P(buf));
    if (BUFFER_ON_HEAP_P(buf)) {
      status = realloc(BUFFER_DATA_P(buf), BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      BUFFER_DATA_P(buf) = status;
    } else {
      status = malloc(BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      memcpy(status, BUFFER_DATA_P(buf), last_alloc);
      BUFFER_ON_HEAP_P(buf) = 1;
      BUFFER_DATA_P(buf) = status;
    }
  }
  if ((size_t) idx + offset > len) memset(BUFFER_DATA_P(buf) + BUFFER_LEN_P(buf), 0, idx + offset - len);
  memcpy(BUFFER_DATA_P(buf) + idx + offset, d, sz);
  if (BUFFER_LEN_P(buf) < sum) BUFFER_LEN_P(buf) = sum;
}

void buffer_set(buffer_t *buf, const void *d, size_t sz, long idx) {
  buffer_set_impl(buf, d, sz, idx, 0, 0, 0, 1);
}

static void buffer_fill_impl(buffer_t *buf, long idx, uint8_t fill, size_t sz, size_t offset, uint8_t top) {
  void *status;
  size_t last_alloc;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buf)) {
    Data_Get_Struct(BUFFER_MASTER_P(buf), buffer_t, parent_buffer);
    if (top) idx = normalize_index(BUFFER_SLICE_LEN_P(buf), idx);
    buffer_fill_impl(parent_buffer, idx, fill, sz, offset + BUFFER_OFFSET_P(buf), 0);
    if (BUFFER_SLICE_LEN_P(buf) < idx + sz + offset) {
      BUFFER_SLICE_LEN_P(buf) = idx + sz + offset;
    }
    return;
  }
  if (idx + offset + sz > BUFFER_ALLOC_P(buf)) {
    last_alloc = BUFFER_ALLOC_P(buf);
    BUFFER_ALLOC_P(buf) <<= relative_bits(idx + offset + sz, BUFFER_ALLOC_P(buf));
    if (BUFFER_ON_HEAP_P(buf)) {
      status = realloc(BUFFER_DATA_P(buf), BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      BUFFER_DATA_P(buf) = status;
    } else {
      status = malloc(BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      memcpy(status, BUFFER_DATA_P(buf), last_alloc);
      BUFFER_ON_HEAP_P(buf) = 1;
      BUFFER_DATA_P(buf) = status;
    }
  }
  if ((size_t) idx + offset > BUFFER_LEN_P(buf)) memset(BUFFER_DATA_P(buf) + BUFFER_LEN_P(buf), fill, idx + offset - BUFFER_LEN_P(buf));
  if (sz > 0) memset(BUFFER_DATA_P(buf) + idx + offset, fill, sz);
  if (BUFFER_LEN_P(buf) < idx + offset + sz) {
    BUFFER_LEN_P(buf) = idx + offset + sz;
  }
}

static void buffer_appendr_impl(buffer_t *buf, const void *d, size_t sz) {
  void *status;
  size_t last_alloc, sum, len, offset = 0;
  uint8_t top = 1;
  while (BUFFER_IS_SLAVE_P(buf)) {
    Data_Get_Struct(BUFFER_MASTER_P(buf), buffer_t, buf);
    if (top) {
      len = BUFFER_SLICE_LEN_P(buf);
      top = 0;
    }
    sum = sz + offset + len;
    if (sum > BUFFER_SLICE_LEN_P(buf)) BUFFER_SLICE_LEN_P(buf) = sum;
    offset += BUFFER_OFFSET_P(buf);
  }
  if (top) {
    len = BUFFER_LEN_P(buf);
    sum = offset + len + sz;
  }
  if (len + offset != BUFFER_LEN_P(buf)) {
    buffer_set_impl(buf, d, sz, (long) len, 0, offset, 0, 0);
    return;
  }
  if (sum > BUFFER_ALLOC_P(buf)) {
    last_alloc = BUFFER_ALLOC_P(buf);
    BUFFER_ALLOC_P(buf) <<= relative_bits(sum, BUFFER_ALLOC_P(buf));
    if (BUFFER_ON_HEAP_P(buf)) {
      status = realloc(BUFFER_DATA_P(buf), BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      BUFFER_DATA_P(buf) = status;
    } else {
      status = malloc(BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      memcpy(status, BUFFER_DATA_P(buf), last_alloc);
      BUFFER_ON_HEAP_P(buf) = 1;
      BUFFER_DATA_P(buf) = status;
    }
  }
  reverse_memcpy(BUFFER_DATA_P(buf) + BUFFER_LEN_P(buf) + offset, d, sz);
  BUFFER_LEN_P(buf) = sum;
}

void buffer_appendr(buffer_t *buf, const void *d, size_t sz) {
  buffer_appendr_impl(buf, d, sz);
}

static void buffer_setr_impl(buffer_t *buf, const void *d, size_t sz, long idx, long orig, size_t offset, size_t len, uint8_t top) {
  void *status;
  size_t last_alloc, sum;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buf)) {
    Data_Get_Struct(BUFFER_MASTER_P(buf), buffer_t, parent_buffer);
    if (top) {
      len = BUFFER_SLICE_LEN_P(buf);
      orig = idx;
      idx = normalize_index(len, idx);
    }
    sum = idx + sz + offset; 
    buffer_set_impl(parent_buffer, d, sz, idx, orig, offset + BUFFER_OFFSET_P(buf), len, 0);
    if (BUFFER_SLICE_LEN_P(buf) < sum) BUFFER_SLICE_LEN_P(buf) = sum;
    return;
  }
  if (top) {
    len = BUFFER_LEN_P(buf);
    orig = idx;  
    idx = normalize_index(len, idx);
  }
  sum = idx + sz + offset;
  if (sum > BUFFER_ALLOC_P(buf)) {
    last_alloc = BUFFER_ALLOC_P(buf);
    BUFFER_ALLOC_P(buf) <<= relative_bits(sum, BUFFER_ALLOC_P(buf));
    if (BUFFER_ON_HEAP_P(buf)) {
      status = realloc(BUFFER_DATA_P(buf), BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      BUFFER_DATA_P(buf) = status;
    } else {
      status = malloc(BUFFER_ALLOC_P(buf));
      if (!status && errno == ENOMEM) {
        BUFFER_ALLOC_P(buf) = last_alloc;
        rb_raise(rb_eNoMemError, "Buffer allocation failed.");
      }
      memcpy(status, BUFFER_DATA_P(buf), last_alloc);
      BUFFER_ON_HEAP_P(buf) = 1;
      BUFFER_DATA_P(buf) = status;
    }
  }
  if ((size_t) idx + offset > len) memset(BUFFER_DATA_P(buf) + BUFFER_LEN_P(buf), 0, idx + offset - len);
  reverse_memcpy(BUFFER_DATA_P(buf) + idx + offset, d, sz);
  if (BUFFER_LEN_P(buf) < sum) BUFFER_LEN_P(buf) = sum;
}

void buffer_setr(buffer_t *buf, const void *d, size_t sz, long idx) {
  buffer_setr_impl(buf, d, sz, idx, 0, 0, 0, 1);
}

VALUE alloc_buffer(VALUE klass) {
  return Data_Wrap_Struct(klass, NULL, &buffer_free, (void *) buffer_new());
}

VALUE buffer_initialize(int argc, VALUE *argv, VALUE self) {
  VALUE arg, item;
  buffer_t *buffer_intern;
  size_t i;
  uint8_t c, t;
  long l;
  rb_scan_args(argc, argv, "01", &arg);
  if (arg == Qnil) return self;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  switch (TYPE(arg)) {
    case T_STRING:
      buffer_from_string(self, arg);
      return self;
    case T_ARRAY:
      for (i = 0; i < (size_t) RARRAY_LEN(arg); ++i) {
        item = RARRAY_AREF(arg, i);
        switch (TYPE(item)) {
          case T_FIXNUM:
          case T_FLOAT:
            l = NUM2LONG(item);
            t = integer_type_check(l);
            if (t != LEON_UINT8) goto err;
            c = l;
            buffer_append(buffer_intern, &c, 1);
            break;
          default:
            goto err;
        }
      }
      break;
    case T_FIXNUM:
    case T_BIGNUM:
      while (BUFFER_LEN_P(buffer_intern) < (size_t) NUM2LONG(arg)) {
        buffer_append(buffer_intern, "", 1);
      }
      break;
    default:
      goto err;
  }
  return self;
  err:
    rb_raise(rb_eTypeError, "LEON::Buffer.new accepts only a raw string, an array of values between 0-255, or a value representing the length of the buffer.");
    return self;
}

void buffer_from_string(VALUE self, VALUE arg) {
  buffer_t *buffer_intern;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  buffer_append(buffer_intern, RSTRING_PTR(arg), RSTRING_LEN(arg));
}

char prefix[] = "0x";

VALUE buffer_from_hex(VALUE self, VALUE arg) {
  buffer_t *buffer_intern;
  uint8_t byte, alloc;
  char *str;
  size_t len, i;
  VALUE out;
  Check_Type(arg, T_STRING);
  out = rb_funcall(cBuffer, i_new, 0);
  Data_Get_Struct(out, buffer_t, buffer_intern);
  alloc = 0;
  if (RSTRING_LEN(arg) >= 2 && !memcmp(RSTRING_PTR(arg), prefix, 2)) {
    if (RSTRING_LEN(arg) % 2) {
      str = ALLOC_N(char, RSTRING_LEN(arg) - 1);
      str[0] = '0';
      memcpy(str + 1, RSTRING_PTR(arg) + 2, RSTRING_LEN(arg) - 2);
      len = RSTRING_LEN(arg) - 1;
    } else {
      str = ALLOC_N(char, RSTRING_LEN(arg) - 2);
      memcpy(str, RSTRING_PTR(arg) + 2, RSTRING_LEN(arg) - 2);
      len = RSTRING_LEN(arg) - 2;
    }
    alloc = 1;
  } else {
    if (RSTRING_LEN(arg) % 2) {
      str = ALLOC_N(char, RSTRING_LEN(arg) + 1);
      str[0] = '0';
      memcpy(str + 1, RSTRING_PTR(arg), RSTRING_LEN(arg));
      alloc = 1;
      len = RSTRING_LEN(arg) + 1;
    } else {
      str = RSTRING_PTR(arg);
      len = RSTRING_LEN(arg);
    }
  }
  for (i = 0; i < len; i += 2) {
    sscanf(str + i, "%02hhx", &byte);
    buffer_append(buffer_intern, &byte, 1);
  }
  if (alloc) free(str);
  return out;
}

VALUE buffer_write_uint8(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  uint8_t data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (val == Qnil) buffer_append(buffer_intern, &data, 1);
  else buffer_set(buffer_intern, &data, 1, ladj);
  return self;
} 
 
VALUE buffer_write_int8(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  int8_t data;
  long lidx, ladj = 0;
  buffer_t *buffer_intern;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (val == Qnil) buffer_append(buffer_intern, &data, 1);
  else buffer_set(buffer_intern, &data, 1, ladj);
  return self;
} 

VALUE buffer_write_uint16_le(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  uint16_t data;
  long lidx, ladj = 0;
  buffer_t *buffer_intern;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_append(buffer_intern, &data, 2);
    else buffer_set(buffer_intern, &data, 2, ladj);
  } else {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 2);
    else buffer_setr(buffer_intern, &data, 2, ladj);
  }
  return self;
} 

VALUE buffer_write_uint16_be(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  uint16_t data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 2);
    else buffer_setr(buffer_intern, &data, 2, ladj);
  } else {
    if (val == Qnil) buffer_append(buffer_intern, &data, 2);
    else buffer_set(buffer_intern, &data, 2, ladj);
  }
  return self;
}

VALUE buffer_write_int16_le(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  int16_t data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_append(buffer_intern, &data, 2);
    else buffer_set(buffer_intern, &data, 2, ladj);
  } else {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 2);
    else buffer_setr(buffer_intern, &data, 2, ladj);
  }
  return self;
}

VALUE buffer_write_int16_be(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  int16_t data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 2);
    else buffer_setr(buffer_intern, &data, 2, ladj);
  } else {
    if (val == Qnil) buffer_append(buffer_intern, &data, 2);
    else buffer_set(buffer_intern, &data, 2, ladj);
  }
  return self;
}
VALUE buffer_write_uint32_le(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  uint32_t data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_append(buffer_intern, &data, 4);
    else buffer_set(buffer_intern, &data, 4, ladj);
  } else {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 4);
    else buffer_setr(buffer_intern, &data, 4, ladj);
  }
  return self;
} 
VALUE buffer_write_uint32_be(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  uint32_t data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 4);
    else buffer_setr(buffer_intern, &data, 4, ladj);
  } else {
    if (val == Qnil) buffer_append(buffer_intern, &data, 4);
    else buffer_set(buffer_intern, &data, 4, ladj);
  }
  return self;
}

VALUE buffer_write_int32_le(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  int32_t data;
  long lidx, ladj = 0;
  buffer_t *buffer_intern;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_append(buffer_intern, &data, 4);
    else buffer_set(buffer_intern, &data, 4, ladj);
  } else {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 4);
    else buffer_setr(buffer_intern, &data, 4, ladj);
  }
  return self;
}

VALUE buffer_write_int32_be(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  int32_t data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) data = NUM2LONG(idx);
  else {
    Check_Type(val, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    data = NUM2LONG(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 4);
    else buffer_setr(buffer_intern, &data, 4, ladj);
  } else {
    if (val == Qnil) buffer_append(buffer_intern, &data, 4);
    else buffer_set(buffer_intern, &data, 4, ladj);
  }
  return self;
}

VALUE buffer_write_float_le(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  float data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) {
    if (TYPE(idx) != T_FLOAT && TYPE(idx) != T_FIXNUM) Check_Type(idx, T_FLOAT);
    data = NUM2DBL(idx);
  }
  else {
    Check_Type(idx, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    if (TYPE(val) != T_FLOAT && TYPE(val) != T_FIXNUM) Check_Type(val, T_FLOAT);
    data = NUM2DBL(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_append(buffer_intern, &data, 4);
    else buffer_set(buffer_intern, &data, 4, ladj);
  } else {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 4);
    else buffer_setr(buffer_intern, &data, 4, ladj);
  }
  return self;
}

VALUE buffer_write_float_be(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  float data;
  buffer_t *buffer_intern;
  long lidx, ladj = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) {
    if (TYPE(idx) != T_FLOAT && TYPE(idx) != T_FIXNUM) Check_Type(idx, T_FLOAT);
    data = NUM2DBL(idx);
  }
  else {
    Check_Type(idx, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    if (TYPE(val) != T_FLOAT && TYPE(val) != T_FIXNUM) Check_Type(val, T_FLOAT);
    data = NUM2DBL(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 4);
    else buffer_setr(buffer_intern, &data, 4, ladj);
  } else {
    if (val == Qnil) buffer_append(buffer_intern, &data, 4);
    else buffer_set(buffer_intern, &data, 4, ladj);
  }
  return self;
}

VALUE buffer_write_double_le(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  double data;
  long lidx, ladj = 0;
  buffer_t *buffer_intern;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) {
    if (TYPE(idx) != T_FLOAT && TYPE(idx) != T_FIXNUM && TYPE(idx) != T_BIGNUM) Check_Type(idx, T_FLOAT);
    data = NUM2DBL(idx);
  }
  else {
    Check_Type(idx, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (lidx < 0) ladj = BUFFER_LEN_P(buffer_intern) + lidx;
    else ladj = lidx;
    if (ladj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", lidx);
    if (TYPE(val) != T_FLOAT && TYPE(val) != T_FIXNUM && TYPE(val) != T_BIGNUM) Check_Type(val, T_FLOAT);
    data = NUM2DBL(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_append(buffer_intern, &data, 8);
    else buffer_set(buffer_intern, &data, 8, ladj);
  } else {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 8);
    else buffer_setr(buffer_intern, &data, 8, ladj);
  }
  return self;
}

VALUE buffer_write_double_be(int argc, VALUE *argv, VALUE self) {
  VALUE val, idx;
  buffer_t *buffer_intern;
  double data;
  long lidx = 0;
  rb_scan_args(argc, argv, "11", &idx, &val);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (val == Qnil) {
    if (TYPE(idx) != T_FLOAT && TYPE(idx) != T_FIXNUM && TYPE(idx) != T_BIGNUM) Check_Type(idx, T_FLOAT);
    data = NUM2DBL(idx);
  }
  else {
    Check_Type(idx, T_FIXNUM);
    lidx = NUM2LONG(idx);
    if (TYPE(val) != T_FLOAT && TYPE(val) != T_FIXNUM && TYPE(val) != T_BIGNUM) Check_Type(val, T_FLOAT);
    data = NUM2DBL(val);
  }
  if (isLE) {
    if (val == Qnil) buffer_appendr(buffer_intern, &data, 8);
    else buffer_setr(buffer_intern, &data, 8, lidx);
  } else {
    if (val == Qnil) buffer_append(buffer_intern, &data, 8);
    else buffer_set(buffer_intern, &data, 8, lidx);
  }
  return self;
}

VALUE buffer_write_dynamic(int argc, VALUE *argv, VALUE self) {
  VALUE idx, data;
  buffer_t out, *buffer_intern;
  spec_value_t *spec;
  char buf[BUFFER_DEFAULT_ALLOC];
  long start;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  rb_scan_args(argc, argv, "11", &idx, &data);
  if (data == Qnil) {
    spec = spec_value_new();
    SPEC_TYPE_P(spec) = TYPE_LITERAL;
    SPEC_LIT_P(spec) = LEON_DYNAMIC;
    BUFFER_IS_SLAVE(out) = 0;
    BUFFER_ALLOC(out) = BUFFER_DEFAULT_ALLOC;
    BUFFER_LEN(out) = 0;
    BUFFER_DATA(out) = buf;
    BUFFER_ON_HEAP(out) = 0;
    leon_encode(&out, idx, spec);
    spec_value_free(spec);
    buffer_append(buffer_intern, BUFFER_DATA(out), BUFFER_LEN(out));
    return INT2NUM(BUFFER_LEN(out) + BUFFER_LEN_P(buffer_intern));
  } else {
    Check_Type(idx, T_FIXNUM);
    start = NUM2LONG(idx);
    spec = spec_value_new();
    SPEC_TYPE_P(spec) = TYPE_LITERAL;
    SPEC_LIT_P(spec) = LEON_DYNAMIC;
    BUFFER_ALLOC(out) = BUFFER_DEFAULT_ALLOC;
    BUFFER_IS_SLAVE(out) = 0;
    BUFFER_LEN(out) = 0;
    BUFFER_DATA(out) = buf;
    BUFFER_ON_HEAP(out) = 0;
    leon_encode(&out, data, spec);
    spec_value_free(spec);
    buffer_set(buffer_intern, &out, BUFFER_LEN(out), start);
    return INT2NUM(start + BUFFER_LEN(out));
  }
} 

static uint8_t read_uint8_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_uint8_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  return *((uint8_t *) BUFFER_DATA_P(buffer_intern) + iadj);
}

static int8_t read_int8_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_int8_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  return *((int8_t *) BUFFER_DATA_P(buffer_intern) + iadj);
}

static uint16_t read_uint16_le_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  uint16_t data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_uint16_le_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 1 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (isLE) return *((uint16_t *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((uint16_t *) (BUFFER_DATA_P(buffer_intern) + iadj)), 2);
    return data;
  }
}

static uint16_t read_uint16_be_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  uint16_t data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_uint16_be_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 1 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (!isLE) return *((uint16_t *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((uint16_t *) (BUFFER_DATA_P(buffer_intern) + iadj)), 2);
    return data;
  }
}

static int16_t read_int16_le_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  int16_t data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_int16_le_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 1 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (isLE) return *((int16_t *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((int16_t *) (BUFFER_DATA_P(buffer_intern) + iadj)), 2);
    return data;
  }
}

static int16_t read_int16_be_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  int16_t data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_int16_be_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 1 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (!isLE) return *((int16_t *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((int16_t *) (BUFFER_DATA_P(buffer_intern) + iadj)), 2);
    return data;
  }
}

static uint32_t read_uint32_le_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  uint32_t data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_uint32_le_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 3 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (isLE) return *((uint32_t *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((uint32_t *) (BUFFER_DATA_P(buffer_intern) + iadj)), 4);
    return data;
  }
}

static uint32_t read_uint32_be_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  uint32_t data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_uint32_be_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 3 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (!isLE) return *((uint32_t *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((uint32_t *) (BUFFER_DATA_P(buffer_intern) + iadj)), 4);
    return data;
  }
}

static int32_t read_int32_le_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  int32_t data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_int32_le_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 3 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (isLE) return *((int32_t *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((int32_t *) (BUFFER_DATA_P(buffer_intern) + iadj)), 4);
    return data;
  }
}

static int32_t read_int32_be_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  int32_t data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_int32_be_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 3 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (!isLE) return *((int32_t *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((int32_t *) (BUFFER_DATA_P(buffer_intern) + iadj)), 4);
    return data;
  }
}

static float read_float_le_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  float data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_float_le_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 3 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (isLE) return *((float *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((float *) (BUFFER_DATA_P(buffer_intern) + iadj)), 4);
    return data;
  }
}

static float read_float_be_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  float data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_float_be_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 3 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (!isLE) return *((float *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((float *) (BUFFER_DATA_P(buffer_intern) + iadj)), 4);
    return data;
  }
}

static double read_double_le_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  double data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_double_le_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 7 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (isLE) return *((double *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((double *) (BUFFER_DATA_P(buffer_intern) + iadj)), 8);
    return data;
  }
}

static double read_double_be_impl(buffer_t *buffer_intern, long i, size_t offset, size_t len, uint8_t top) {
  long iadj;
  double data;
  buffer_t *parent_buffer;
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, parent_buffer);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return read_double_be_impl(parent_buffer, i, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (i < 0) iadj = i + len + offset;
  else iadj = i + offset;
  if ((size_t) iadj >= BUFFER_LEN_P(buffer_intern) - 7 || iadj < 0) rb_raise(rb_eRangeError, "Index %li out of range.", i);
  if (!isLE) return *((double *) (BUFFER_DATA_P(buffer_intern) + iadj));
  else {
    reverse_memcpy(&data, ((double *) (BUFFER_DATA_P(buffer_intern) + iadj)), 8);
    return data;
  }
}

VALUE buffer_read_uint8(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  i = NUM2LONG(idx);
  return INT2NUM(read_uint8_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_int8(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  i = NUM2LONG(idx);
  return INT2NUM(read_int8_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_uint16_le(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return INT2NUM(read_uint16_le_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_uint16_be(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return INT2NUM(read_uint16_be_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_int16_le(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return INT2NUM(read_int16_le_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_int16_be(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return INT2NUM(read_int16_be_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_uint32_le(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return INT2NUM(read_uint32_le_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_uint32_be(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return INT2NUM(read_uint32_be_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_int32_le(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return INT2NUM(read_int32_le_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_int32_be(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return INT2NUM(read_int32_be_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_float_le(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return DBL2NUM(read_float_le_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_float_be(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return DBL2NUM(read_float_be_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_double_le(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return DBL2NUM(read_double_le_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_read_double_be(VALUE self, VALUE idx) {
  buffer_t *buffer_intern;
  long i;
  Check_Type(idx, T_FIXNUM);
  i = NUM2LONG(idx);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  return DBL2NUM(read_double_be_impl(buffer_intern, i, 0, 0, 1));
}

VALUE buffer_partition(VALUE buf, long beg, long end, int excluded, size_t offset, size_t len, uint8_t top) {
  buffer_t *buffer_intern, *out_buffer;
  long begadj, endadj;
  VALUE out;
  Data_Get_Struct(buf, buffer_t, buffer_intern);
  if (BUFFER_IS_SLAVE_P(buffer_intern)) {
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    return buffer_partition(BUFFER_MASTER_P(buffer_intern), beg, end, excluded, offset + BUFFER_OFFSET_P(buffer_intern), len, 0);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  if (beg < 0) begadj = beg + len;
  else begadj = beg;
  if (end < 0) endadj = end + len;
  else endadj = end;
  if (excluded) --endadj;
  if (begadj < 0 || (size_t) begadj + offset >= BUFFER_LEN_P(buffer_intern)) rb_raise(rb_eRangeError, "Start index %li out of range.", beg);
  if (endadj < 0 || (size_t) endadj + offset > BUFFER_LEN_P(buffer_intern)) rb_raise(rb_eRangeError, "End index %li out of range.", end); 
  if (begadj > endadj) rb_raise(rb_eRangeError, "Start index must not be after end index.");
  out = rb_funcall(cBuffer, i_new, 0);
  Data_Get_Struct(out, buffer_t, out_buffer);
  buffer_append(out_buffer, BUFFER_DATA_P(buffer_intern) + begadj + offset, endadj - begadj);
  return out;
}

VALUE buffer_slice(int argc, VALUE *argv, VALUE self) {
  VALUE start, end;
  buffer_t *buffer_intern;
  rb_scan_args(argc, argv, "11", &start, &end);
  Check_Type(start, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (end == Qnil) return buffer_partition(self, NUM2LONG(start), BUFFER_LEN_P(buffer_intern), 1, 0, 0, 1);
  else {
    Check_Type(end, T_FIXNUM);
    return buffer_partition(self, NUM2LONG(start), NUM2LONG(end), 1, 0, 0, 1);
  }
}

VALUE buffer_slice_bang(int argc, VALUE *argv, VALUE self) {
  VALUE start, end, tmp;
  buffer_t *buffer_intern, *tmp_buffer;
  rb_scan_args(argc, argv, "11", &start, &end);
  Check_Type(start, T_FIXNUM);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (end == Qnil) tmp = buffer_partition(self, NUM2LONG(start), BUFFER_LEN_P(buffer_intern), 1, 0, 0, 1);
  else {
    Check_Type(end, T_FIXNUM);
    tmp = buffer_partition(self, NUM2LONG(start), NUM2LONG(end), 1, 0, 0, 1);
  }
  Data_Get_Struct(tmp, buffer_t, tmp_buffer);
  buffer_set(buffer_intern, BUFFER_DATA_P(tmp_buffer), BUFFER_LEN_P(tmp_buffer), 0);
  BUFFER_LEN_P(buffer_intern) = BUFFER_LEN_P(tmp_buffer);
  return self;
}

VALUE buffer_operator_bracket(VALUE self, VALUE idx) {
  VALUE begv, endv;
  int excluded;
  long beg, end;
  if (TYPE(idx) == T_STRUCT && CLASS_OF(idx) == rb_cRange) {
    rb_range_values(idx, &begv, &endv, &excluded);
    beg = NUM2LONG(begv);
    end = NUM2LONG(endv);
    return buffer_partition(self, beg, end, excluded, 0, 0, 1);
  }
  Check_Type(idx, T_FIXNUM);
  return buffer_read_uint8(self, idx);
}

VALUE buffer_operator_bracket_equals(VALUE self, VALUE idx, VALUE val) {
  VALUE argv[2] = { idx, val };
  return buffer_write_uint8(2, argv, self);
}

VALUE buffer_compare(VALUE self, VALUE other) {
  buffer_t *a, *b;
  size_t offset_a = 0, len_a = 0, offset_b = 0, len_b = 0;
  uint8_t top = 1;
  if (TYPE(other) != T_DATA || CLASS_OF(other) != cBuffer) rb_raise(rb_eTypeError, "Tried to compare a LEON::Buffer to a different type of value.");
  Data_Get_Struct(self, buffer_t, a);
  Data_Get_Struct(self, buffer_t, b);
  while (BUFFER_IS_SLAVE_P(a)) {
    offset_a += BUFFER_OFFSET_P(a);
    if (top) len_a = BUFFER_SLICE_LEN_P(a);
    Data_Get_Struct(BUFFER_MASTER_P(a), buffer_t, a);
    top = 0;
  }
  if (top) len_a = BUFFER_LEN_P(a);
  top = 1;
  while (BUFFER_IS_SLAVE_P(b)) {
    offset_b += BUFFER_OFFSET_P(b);
    if (top) len_b = BUFFER_SLICE_LEN_P(b);
    Data_Get_Struct(BUFFER_MASTER_P(b), buffer_t, b);
    top = 0;
  }
  if (top) len_b = BUFFER_LEN_P(b);
  if (len_a > len_b) return INT2NUM(1);
  else if (len_a == len_b) return INT2NUM(memcmp(BUFFER_DATA_P(a) + offset_a, BUFFER_DATA_P(b) + offset_b, len_a));
  else return INT2NUM(-1);
}

VALUE buffer_length(VALUE self) {
  buffer_t *buffer_intern;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  if (BUFFER_IS_SLAVE_P(buffer_intern)) return INT2NUM(BUFFER_SLICE_LEN_P(buffer_intern));
  return INT2NUM(BUFFER_LEN_P(buffer_intern));
}

VALUE buffer_length_equals(VALUE self, VALUE length) {
  buffer_t *buffer_intern, *buffer_top;
  long len;
  size_t bitshift, offset = 0;
  len = NUM2LONG(length);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  buffer_top = buffer_intern;
  while (BUFFER_IS_SLAVE_P(buffer_intern)) {
    if (BUFFER_SLICE_LEN_P(buffer_intern) < len + offset) BUFFER_SLICE_LEN_P(buffer_intern) = len + offset;
    offset += BUFFER_OFFSET_P(buffer_intern);
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, buffer_intern);
  }
  if (len < 0) rb_raise(rb_eRangeError, "Length can't be negative.");
  if ((size_t) len + offset > BUFFER_LEN_P(buffer_intern)) {
    buffer_fill_impl(buffer_intern, len, 0, 0, offset, 1);
  } else if ((size_t) len + offset < BUFFER_LEN_P(buffer_intern)) {
    if (BUFFER_IS_SLAVE_P(buffer_top)) BUFFER_SLICE_LEN_P(buffer_top) = len;
    else {
      bitshift = relative_bits(len, BUFFER_LEN_P(buffer_intern));
      if (bitshift) {
        BUFFER_ALLOC_P(buffer_intern) >>= bitshift;
        BUFFER_DATA_P(buffer_intern) = realloc(BUFFER_DATA_P(buffer_intern), BUFFER_ALLOC_P(buffer_intern));
      }
      BUFFER_LEN_P(buffer_intern) = len;
    }
  }
  return self;
}

VALUE buffer_inspect(VALUE self) {
  char append[3];
  char ptrval[19];
  size_t i, max, offset, len;
  uint8_t top;
  VALUE out;
  buffer_t *buffer_intern;
  top = 1;
  offset = 0;
  max = (size_t) NUM2LONG(rb_const_get(cBuffer, i_INSPECT_MAX_BYTES));
  Data_Get_Struct(self, buffer_t, buffer_intern);
  while (BUFFER_IS_SLAVE_P(buffer_intern)) {
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    offset += BUFFER_OFFSET_P(buffer_intern);
    top = 0;
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, buffer_intern);
  }
  if (top) {
    len = BUFFER_LEN_P(buffer_intern);
    out = rb_str_new("#<LEON::Buffer:", sizeof("#<LEON::Buffer:") - 1);
  } else {
    out = rb_str_new("#<LEON::Buffer(View):", sizeof("#<LEON::Buffer(View):") - 1);
  }
  if (!len) {
    if (sizeof(VALUE) == 8) {
      sprintf(ptrval, "%#016zx", (size_t) self);
      rb_str_cat(out, ptrval, 18);
    } else {
      sprintf(ptrval, "%#08zx", (size_t) self);
      rb_str_cat(out, ptrval, 10);
    }
  }
  else for (i = 0; i < len && i < max; ++i) {
    sprintf(append, "%02x", (uint8_t) BUFFER_DATA_P(buffer_intern)[i + offset]);
    rb_str_cat(out, append, 2);
    if (i < len - 1 && i < max - 1) rb_str_cat(out, " ", 1);
    if (i == max - 1) rb_str_cat(out, "...", 3);
  }
  rb_str_cat(out, ">", 1);
  return out;
}

VALUE buffer_to_s(VALUE self) {
  buffer_t *buffer_intern;
  VALUE out;
  size_t len, offset = 0;
  uint8_t top = 1;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  while (BUFFER_IS_SLAVE_P(buffer_intern)) {
    offset += BUFFER_OFFSET_P(buffer_intern);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    top = 0;
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, buffer_intern);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  out = rb_str_new(BUFFER_DATA_P(buffer_intern) + offset, len);
  return out;
}

VALUE buffer_to_hex(VALUE self) {
  buffer_t *buffer_intern;
  VALUE out;
  char append[9];
  size_t i, len, offset = 0;
  uint8_t top = 1;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  while (BUFFER_IS_SLAVE_P(buffer_intern)) {
    offset += BUFFER_OFFSET_P(buffer_intern);
    if (top) len = BUFFER_SLICE_LEN_P(buffer_intern);
    top = 0;
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, buffer_intern);
  }
  if (top) len = BUFFER_LEN_P(buffer_intern);
  out = rb_str_new("", 0);
  for (i = 0; i < len; ++i) {
    sprintf(append, "%02x", BUFFER_DATA_P(buffer_intern)[i + offset]);
    rb_str_cat(out, append, 2);
  }
  return out;
}

VALUE buffer_equals(VALUE self, VALUE other) {
  buffer_t *a, *b;
  size_t len_a, len_b, offset_a = 0, offset_b = 0;
  uint8_t top = 1;
  if (TYPE(other) != T_DATA || CLASS_OF(other) != cBuffer) rb_raise(rb_eTypeError, "Tried to compare a LEON::Buffer to an unrelated value.");
  Data_Get_Struct(self, buffer_t, a);
  Data_Get_Struct(other, buffer_t, b);
  while (BUFFER_IS_SLAVE_P(a)) {
    offset_a += BUFFER_OFFSET_P(a);
    if (top) len_a = BUFFER_SLICE_LEN_P(a);
    top = 0;
    Data_Get_Struct(BUFFER_MASTER_P(a), buffer_t, a);
  }
  if (top) len_a = BUFFER_LEN_P(a);
  top = 1;
  while (BUFFER_IS_SLAVE_P(b)) {
    offset_b += BUFFER_OFFSET_P(b);
    if (top) len_b = BUFFER_SLICE_LEN_P(b);
    top = 0;
    Data_Get_Struct(BUFFER_MASTER_P(b), buffer_t, b);
  }
  if (top) len_b = BUFFER_LEN_P(b);
  if (len_a != len_b) return Qfalse;
  if (!memcmp(BUFFER_DATA_P(a) + offset_a, BUFFER_DATA_P(b) + offset_b, len_a)) return Qtrue;
  return Qfalse;
}

VALUE buffer_dump(VALUE self, VALUE depth) {
  buffer_t *buffer_intern;
  uint32_t len;
  VALUE out;
  size_t l, offset = 0;
  uint8_t top = 1;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  while (BUFFER_IS_SLAVE_P(buffer_intern)) {
    offset += BUFFER_OFFSET_P(buffer_intern);
    if (top) l = BUFFER_SLICE_LEN_P(buffer_intern);
    top = 0;
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, buffer_intern);
  }
  if (top) l = BUFFER_LEN_P(buffer_intern);
  out = rb_str_new((char *) &len, 4);
  rb_str_cat(out, BUFFER_DATA_P(buffer_intern) + offset, l);
  return out;
}

VALUE buffer_load(VALUE self, VALUE str) {
  uint32_t len;
  VALUE out;
  buffer_t *buffer_intern;
  if (RSTRING_LEN(str) < 4) goto err;
  len = *((uint32_t *) RSTRING_PTR(str));
  if (RSTRING_LEN(str) < 4 + len) goto err;
  out = rb_funcall(cBuffer, i_new, 0);
  Data_Get_Struct(out, buffer_t, buffer_intern);
  buffer_append(buffer_intern, RSTRING_PTR(str) + 4, len);
  return out;
  err:
    rb_raise(rb_eArgError, "Invalid serialized LEON::Buffer.");
}

VALUE buffer_to_base64(VALUE self) {
  buffer_t *buffer_intern;
  VALUE out;
  int len;
  char *encoded;
  size_t l, offset = 0;
  uint8_t top = 1;
  Data_Get_Struct(self, buffer_t, buffer_intern);
  while (BUFFER_IS_SLAVE_P(buffer_intern)) {
    offset += BUFFER_OFFSET_P(buffer_intern);
    if (top) l = BUFFER_SLICE_LEN_P(buffer_intern);
    top = 0;
    Data_Get_Struct(BUFFER_MASTER_P(buffer_intern), buffer_t, buffer_intern);
  }
  if (top) l = BUFFER_LEN_P(buffer_intern);
  Data_Get_Struct(self, buffer_t, buffer_intern);
  len = Base64encode_len(l);
  encoded = ALLOC_N(char, len);
  Base64encode(encoded, BUFFER_DATA_P(buffer_intern) + offset, l);
  out = rb_str_new(encoded, len - 1);
  free(encoded);
  return out;
}

VALUE buffer_from_base64(VALUE self, VALUE data) {
  buffer_t *buffer_intern;
  int len;
  char *encoded, *decoded;
  VALUE out = rb_funcall(cBuffer, i_new, 0);
  Data_Get_Struct(out, buffer_t, buffer_intern);
  len = Base64decode_len(RSTRING_PTR(data));
  encoded = ALLOC_N(char, RSTRING_LEN(data) + 1);
  memcpy(encoded, RSTRING_PTR(data), RSTRING_LEN(data));
  encoded[RSTRING_LEN(data)] = '\0';
  decoded = ALLOC_N(char, len);
  Base64decode(decoded, encoded);
  free(encoded);
  buffer_append(buffer_intern, decoded, len - 3);
  free(decoded);
  return out;
}

VALUE buffer_to_file(int argc, VALUE *argv, VALUE self) {
  VALUE filepath, opts, file, retval;
  rb_scan_args(argc, argv, "10:", &filepath, &opts);
  Check_Type(filepath, T_STRING);
  if (opts != Qnil && rb_hash_aref(opts, ID2SYM(rb_intern("append")) == Qtrue)) {
    file = rb_funcall(cFile, i_new, 2, filepath, rb_str_new("a+", 2));
  } else {
    file = rb_funcall(cFile, i_new, 2, filepath, rb_str_new("w+", 2));
  }
  retval = rb_funcall(file, i_write, 1, rb_funcall(self, i_to_s, 0));
  rb_funcall(file, i_close, 0);
  return retval;
}

VALUE buffer_from_file(VALUE self, VALUE path) {
  Check_Type(path, T_STRING);
  return rb_funcall(cBuffer, i_new, 1, rb_funcall(cFile, i_read, 1, path));
}

buffer_iterator_t *buffer_iterator_new(void) {
  buffer_iterator_t *retval = ALLOC_N(buffer_iterator_t, 1);
  memset(retval, 0, sizeof(buffer_iterator_t));
  return retval;
}

void buffer_iterator_free(buffer_iterator_t *buf) {
  free(buf);
}

VALUE alloc_iterator(VALUE klass) {
  return Data_Wrap_Struct(klass, NULL, &buffer_iterator_free, (void *) buffer_iterator_new());
}

VALUE buffer_iterator_initialize(int argc, VALUE *argv, VALUE self) {
  VALUE buf, offset;
  buffer_iterator_t *iterator;
  buffer_t *buffer_intern;
  rb_scan_args(argc, argv, "02", &buf, &offset);
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  if (offset == Qnil) ITERATOR_OFFSET_P(iterator) = 0;
  if (buf != Qnil) {
    if (TYPE(buf) != T_DATA || CLASS_OF(buf) != cBuffer) rb_raise(rb_eTypeError, "Argument to LEON::BufferIterator.new must be a LEON::Channel if provided.");
    Data_Get_Struct(buf, buffer_t, buffer_intern);
    ITERATOR_BUF_P(iterator) = buffer_intern;
    ITERATOR_RBBUF_P(iterator) = buf;
    rb_gc_mark(buf);
  }
  return self;
}

VALUE iterator_from_native_buffer(buffer_t *buf, size_t offset) {
  buffer_iterator_t *iterator;
  VALUE out = rb_funcall(cBufferIterator, i_new, 0);
  Data_Get_Struct(out, buffer_iterator_t, iterator);
  ITERATOR_OFFSET_P(iterator) = offset;
  ITERATOR_BUF_P(iterator) = buf;
  return out;
}

VALUE buffer_iterator_write_uint8(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  uint8_t uc;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  uc = ldata;
  buffer_set_impl(ITERATOR_BUF_P(iterator), &uc, 1, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ++ITERATOR_OFFSET_P(iterator);
  return self;
}

VALUE buffer_iterator_write_int8(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  int8_t c;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  c = ldata;
  buffer_set_impl(ITERATOR_BUF_P(iterator), &c, 1, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ++ITERATOR_OFFSET_P(iterator);
  return self;
}

VALUE buffer_iterator_write_uint16_le(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  uint16_t us;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_BIGNUM:
    case T_FIXNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  us = ldata;
  if (isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &us, 2, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &us, 2, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 2;
  return self;
}

VALUE buffer_iterator_write_uint16_be(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  uint16_t us;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  us = ldata;
  if (!isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &us, 2, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &us, 2, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 2;
  return self;
}

VALUE buffer_iterator_write_int16_le(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  int16_t s;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  s = ldata;
  if (isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &s, 2, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &s, 2, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 2;
  return self;
}

VALUE buffer_iterator_write_int16_be(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  int16_t s;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_BIGNUM:
    case T_FIXNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  s = ldata;
  if (!isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &s, 2, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &s, 2, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 2;
  return self;
}

VALUE buffer_iterator_write_uint32_le(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  uint32_t ui;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  ui = ldata;
  if (isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &ui, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &ui, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 4;
  return self;
}

VALUE buffer_iterator_write_uint32_be(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  uint32_t ui;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  ui = ldata;
  if (!isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &ui, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &ui, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 4;
  return self;
}

VALUE buffer_iterator_write_int32_le(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  int32_t i;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  i = ldata;
  if (isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &i, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &i, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 4;
  return self;
}

VALUE buffer_iterator_write_int32_be(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  long ldata;
  uint32_t i;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ldata = NUM2LONG(data);
  i = ldata;
  if (!isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &i, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &i, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 4;
  return self;
}

VALUE buffer_iterator_write_float_le(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  double ddata;
  float f;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ddata = NUM2DBL(data);
  f = ddata;
  if (isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &f, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &f, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 4;
  return self;
}

VALUE buffer_iterator_write_float_be(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  double ddata;
  float f;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ddata = NUM2DBL(data);
  f = ddata;
  if (!isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &f, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &f, 4, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 4;
  return self;
}

VALUE buffer_iterator_write_double_le(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  double ddata;
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ddata = NUM2DBL(data);
  if (isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &ddata, 8, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &ddata, 8, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 8;
  return self;
}

VALUE buffer_iterator_write_double_be(VALUE self, VALUE data) {
  buffer_iterator_t *iterator;
  double ddata;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  switch (TYPE(data)) {
    case T_FLOAT:
    case T_FIXNUM:
    case T_BIGNUM:
      break;
    default:
      rb_raise(rb_eTypeError, "Type must be numeric.");
      break;
  }
  ddata = NUM2DBL(data);
  if (!isLE) buffer_set_impl(ITERATOR_BUF_P(iterator), &ddata, 8, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  else buffer_setr_impl(ITERATOR_BUF_P(iterator), &ddata, 8, ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += 4;
  return self;
}

VALUE buffer_iterator_write_value(VALUE self, VALUE channel, VALUE data) {
  spec_value_t *spec_intern;
  buffer_iterator_t *iterator;
  char buf[BUFFER_DEFAULT_ALLOC];
  buffer_t out;
  if (TYPE(channel) != T_DATA || CLASS_OF(channel) != cChannel) rb_raise(rb_eTypeError, "First argument must be a LEON::Channel.");
  BUFFER_STACK(out, buf);
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  Data_Get_Struct(self, spec_value_t, spec_intern);
  leon_encode(&out, data, spec_intern);
  buffer_set_impl(ITERATOR_BUF_P(iterator), BUFFER_DATA(out), BUFFER_LEN(out), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), 0, 0, 0, 1);
  ITERATOR_POS_P(iterator) += BUFFER_LEN(out);
  return self;
}


VALUE buffer_iterator_write_string_utf8(VALUE self, VALUE str) {
  buffer_iterator_t *iterator;
  if (TYPE(str) != T_STRING) rb_raise(rb_eTypeError, "Argument must be a String.");
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ITERATOR_POS_P(iterator) += set_str_utf8(ITERATOR_BUF_P(iterator), ITERATOR_POS_P(iterator) + ITERATOR_OFFSET_P(iterator), str);
  return self;
}

VALUE buffer_iterator_write_string_utf16(VALUE self, VALUE str) {
  buffer_iterator_t *iterator;
  if (TYPE(str) != T_STRING) rb_raise(rb_eTypeError, "Argument must be a String.");
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ITERATOR_POS_P(iterator) += set_str_utf16(ITERATOR_BUF_P(iterator), ITERATOR_POS_P(iterator) + ITERATOR_OFFSET_P(iterator), str);
  return self;
}

VALUE buffer_iterator_write_string_ascii(VALUE self, VALUE str) {
  buffer_iterator_t *iterator;
  if (TYPE(str) != T_STRING) rb_raise(rb_eTypeError, "Argument must be a String.");
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ITERATOR_POS_P(iterator) += set_str_ascii(ITERATOR_BUF_P(iterator), ITERATOR_POS_P(iterator) + ITERATOR_OFFSET_P(iterator), str);
  return self;
}

inline void iterator_maybe_eof(buffer_iterator_t *it, size_t bytes) {
  if ((long) ITERATOR_OFFSET_P(it) + ITERATOR_POS_P(it) > (long) BUFFER_LEN_P(ITERATOR_BUF_P(it)) - bytes) rb_raise(rb_eRangeError, "Iterator tried to read past end of buffer.");
}

VALUE buffer_iterator_read_uint8(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 1);
  ++ITERATOR_POS_P(iterator);
  return INT2NUM(read_uint8_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 1, 0, 0, 1));
}

VALUE buffer_iterator_read_int8(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 1);
  ++ITERATOR_POS_P(iterator);
  return INT2NUM(read_int8_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 1, 0, 0, 1));
}

VALUE buffer_iterator_read_uint16_le(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 2);
  ITERATOR_POS_P(iterator) += 2;
  return INT2NUM(read_uint16_le_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 2, 0, 0, 1));
}

VALUE buffer_iterator_read_uint16_be(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 2);
  ITERATOR_POS_P(iterator) += 2;
  return INT2NUM(read_uint16_be_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 2, 0, 0, 1));
}

VALUE buffer_iterator_read_int16_le(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 2);
  ITERATOR_POS_P(iterator) += 2;
  return INT2NUM(read_int16_le_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 2, 0, 0, 1));
}

VALUE buffer_iterator_read_int16_be(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 2);
  ITERATOR_POS_P(iterator) += 2;
  return INT2NUM(read_int16_be_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 2, 0, 0, 1));
}

VALUE buffer_iterator_read_uint32_le(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 4);
  ITERATOR_POS_P(iterator) += 4;
  return INT2NUM(read_uint32_le_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 4, 0, 0, 1));
}

VALUE buffer_iterator_read_uint32_be(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 4);
  ITERATOR_POS_P(iterator) += 4;
  return INT2NUM(read_uint32_be_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 4, 0, 0, 1));
}

VALUE buffer_iterator_read_int32_le(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 4);
  ITERATOR_POS_P(iterator) += 4;
  return INT2NUM(read_int32_le_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 4, 0, 0, 1));
}

VALUE buffer_iterator_read_int32_be(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 4);
  ITERATOR_POS_P(iterator) += 4;
  return INT2NUM(read_int32_be_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 4, 0, 0, 1));
}

VALUE buffer_iterator_read_float_le(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 4);
  ITERATOR_POS_P(iterator) += 4;
  return INT2NUM(read_float_le_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 4, 0, 0, 1));
}

VALUE buffer_iterator_read_float_be(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 4);
  ITERATOR_POS_P(iterator) += 4;
  return INT2NUM(read_float_be_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 4, 0, 0, 1));
}

VALUE buffer_iterator_read_double_le(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 8);
  ITERATOR_POS_P(iterator) += 8;
  return INT2NUM(read_double_le_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 8, 0, 0, 1));
}

VALUE buffer_iterator_read_double_be(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  iterator_maybe_eof(iterator, 8);
  ITERATOR_POS_P(iterator) += 8;
  return INT2NUM(read_double_be_impl(ITERATOR_BUF_P(iterator), ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator) - 8, 0, 0, 1));
}

VALUE buffer_iterator_read_value(VALUE self, VALUE channel) {
  spec_value_t *spec_intern;
  buffer_iterator_t *iterator;
  VALUE str, decoded;
  size_t i;
  if (TYPE(channel) != T_DATA || CLASS_OF(channel) != cChannel) rb_raise(rb_eTypeError, "Argument must be a LEON::Channel.");
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  Data_Get_Struct(self, spec_value_t, spec_intern);
  str = rb_str_new(BUFFER_DATA_P(ITERATOR_BUF_P(iterator)) + ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator), BUFFER_LEN_P(ITERATOR_BUF_P(iterator)) - (ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator)));
  i = 0;
  decoded = leon_decode(str, &i, spec_intern);
  ITERATOR_POS_P(iterator) += i;
  return decoded;
}

VALUE buffer_iterator_read_string_utf8(VALUE self) {
  buffer_iterator_t *iterator;
  size_t i;
  VALUE str;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  i = ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator);
  str = buffer_read_string_utf8(ITERATOR_BUF_P(iterator), &i);
  ITERATOR_POS_P(iterator) = i - ITERATOR_OFFSET_P(iterator);
  return str;
}

VALUE buffer_iterator_read_string_utf16(VALUE self) {
  buffer_iterator_t *iterator;
  size_t i;
  VALUE str;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  i = ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator);
  str = buffer_read_string_utf16(ITERATOR_BUF_P(iterator), &i);
  ITERATOR_POS_P(iterator) = i - ITERATOR_OFFSET_P(iterator);
  return str;
}

VALUE buffer_iterator_read_string_ascii(VALUE self) {
  buffer_iterator_t *iterator;
  size_t i;
  VALUE str;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  i = ITERATOR_OFFSET_P(iterator) + ITERATOR_POS_P(iterator);
  str = buffer_read_string_ascii(ITERATOR_BUF_P(iterator), &i);
  ITERATOR_POS_P(iterator) = i - ITERATOR_OFFSET_P(iterator);
  return str;
}

VALUE buffer_iterator_reset(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  ITERATOR_POS_P(iterator) = 0;
  return self;
}
 
VALUE buffer_iterator_buffer(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  return ITERATOR_RBBUF_P(iterator);
}

VALUE buffer_iterator_position(VALUE self) {
  buffer_iterator_t *iterator;
  Data_Get_Struct(self, buffer_iterator_t, iterator);
  return INT2NUM(ITERATOR_POS_P(iterator));
}
