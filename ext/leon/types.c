#include <stdint.h>
#include <ruby.h>
#include <types.h>
#include <buffer.h>
#include <channel.h>
#include <stubs.h>
#include <custom.h>

uint8_t integer_type_check(int64_t l) {
  if (( (int64_t) ((uint8_t) l) ) == l) return LEON_UINT8;
  if (( (int64_t) ((int8_t) l) ) == l) return LEON_INT8;
  if (( (int64_t) ((uint16_t) l) ) == l) return LEON_UINT16;
  if (( (int64_t) ((int16_t) l) ) == l) return LEON_INT16;
  if (( (int64_t) ((uint32_t) l) ) == l) return LEON_UINT32;
  if (( (int64_t) ((int32_t) l) ) == l) return LEON_INT32;
  return LEON_DOUBLE;
}

uint8_t fp_type_check(double d) {
  if (( (double) ((float) d) ) == d) return LEON_FLOAT;
  return LEON_DOUBLE;
}

VALUE rb_type_check(VALUE self, VALUE val) {
  return INT2NUM(type_check(val));
}

uint8_t type_check(VALUE val) {
  VALUE klass;
  uint8_t i;
  for (i = 0; i < CUSTOM_TYPE_LEN(); ++i) {
    if (TYPE(rb_funcall(CUSTOM_TYPE_CHECK_DETECT(i), i_call, 1, val)) == T_TRUE) return CUSTOM_TYPE_CHECK_CONSTANT(i);
  }
  switch (TYPE(val)) {
    case T_NIL:
      return LEON_NULL;
    case T_OBJECT:
      klass = CLASS_OF(val);
      if (klass == cInfinity) return LEON_INFINITY;
      if (klass == cMinusInfinity) return LEON_MINUS_INFINITY;
      if (klass == cNaN) return LEON_NAN;
      if (klass == cUndefined) return LEON_UNDEFINED;
      return LEON_NATIVE_OBJECT;
    case T_DATA:
      klass = CLASS_OF(val);
      if (klass == cBuffer) return LEON_BUFFER;
      if (klass == cDate) return LEON_DATE;
      if (klass == cTime) return LEON_TIME;
      if (klass == cDateTime) return LEON_DATETIME;
      if (klass == cChannel) return LEON_CHANNEL;
      return LEON_DATA;
    case T_CLASS:
      if (val == cInfinity) return LEON_INFINITY;
      if (val == cMinusInfinity) return LEON_MINUS_INFINITY;
      if (val == cNaN) return LEON_NAN;
      if (val == cUndefined) return LEON_UNDEFINED;
      return LEON_UNDEFINED;
    case T_MODULE:
    case T_FILE:
      return LEON_UNDEFINED;
    case T_FLOAT:
      return fp_type_check(NUM2DBL(val));
    case T_STRING:
      return LEON_STRING;
    case T_REGEXP:
      return LEON_REGEXP;
    case T_ARRAY:
      return LEON_ARRAY;
    case T_HASH:
      return LEON_OBJECT;
    case T_STRUCT:
      return LEON_STRUCT;
    case T_BIGNUM:
    case T_FIXNUM:
      return integer_type_check(NUM2LONG(val));
    case T_COMPLEX:
      return LEON_COMPLEX;
    case T_RATIONAL:
      return LEON_RATIONAL;
    case T_TRUE:
    case T_FALSE:
      return LEON_BOOLEAN;
    case T_SYMBOL: 
      return LEON_SYMBOL;
    default:
      return LEON_UNDEFINED;
  }
}

uint8_t type_check_with_hint(VALUE val, uint8_t fp) {
  VALUE klass;
  uint8_t i;
  for (i = 0; i < CUSTOM_TYPE_LEN(); ++i) {
    if (TYPE(rb_funcall(CUSTOM_TYPE_CHECK_DETECT(i), i_call, 1, val)) == T_TRUE) return INT2NUM(CUSTOM_TYPE_CHECK_CONSTANT(i));
  }
  switch (TYPE(val)) {
    case T_NIL:
      return LEON_NULL;
    case T_OBJECT:
      klass = CLASS_OF(val);
      if (klass == cDate) return LEON_DATE;
      if (klass == cTime) return LEON_TIME;
      if (klass == cDateTime) return LEON_DATETIME;
      if (klass == cInfinity) return LEON_INFINITY;
      if (klass == cMinusInfinity) return LEON_MINUS_INFINITY;
      if (klass == cNaN) return LEON_NAN;
      if (klass == cUndefined) return LEON_UNDEFINED;
      return LEON_NATIVE_OBJECT;
    case T_DATA:
      klass = CLASS_OF(val);
      if (klass == cBuffer) return LEON_BUFFER;
      return LEON_DATA;
    case T_CLASS:
      if (val == cInfinity) return LEON_INFINITY;
      if (val == cMinusInfinity) return LEON_MINUS_INFINITY;
      if (val == cNaN) return LEON_NAN;
      if (val == cUndefined) return LEON_UNDEFINED;
      return LEON_UNDEFINED;
    case T_MODULE:
    case T_FILE:
      return LEON_UNDEFINED;
    case T_FLOAT:
      return fp_type_check(NUM2DBL(val));
    case T_STRING:
      return LEON_STRING;
    case T_REGEXP:
      return LEON_REGEXP;
    case T_ARRAY:
      return LEON_ARRAY;
    case T_HASH:
      return LEON_OBJECT;
    case T_STRUCT:
      return LEON_STRUCT;
    case T_BIGNUM:
    case T_FIXNUM:
      if (fp) return fp_type_check(NUM2DBL(val));
      return integer_type_check(NUM2LONG(val));
    case T_COMPLEX:
      return LEON_COMPLEX;
    case T_RATIONAL:
      return LEON_RATIONAL;
    case T_TRUE:
    case T_FALSE:
      return LEON_BOOLEAN;
    case T_SYMBOL: 
      return LEON_SYMBOL;
    default:
      return LEON_UNDEFINED;
  }
}
