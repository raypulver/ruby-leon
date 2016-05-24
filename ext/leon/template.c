#include <math.h>
#include <ruby.h>
#include <types.h>
#include <channel.h>
#include <stubs.h>
#include <template.h>

#define ABS(x) (x < 0 ? -x : x)

int push_assoc_types(VALUE key, VALUE val, VALUE out) {
  VALUE str;
  if (TYPE(key) == T_FIXNUM) {
    str = rb_sym2str(ID2SYM(key));
    rb_hash_aset(out, rb_str_new(RSTRING_PTR(str) + 1, RSTRING_LEN(str) - 1), to_template(Qnil, val));
  }
  else rb_hash_aset(out, key, to_template(Qnil, val));
  return 0;
}

int pluck_each_prop(VALUE key, VALUE val, VALUE out) {
  VALUE plucked;
  if (!pluck(((VALUE *) out)[1], key, &plucked)) {
    *((uint8_t **) out)[2] = 1;
    return 1;
  }
  rb_hash_aset(((VALUE *) out)[0], key, type_gcd(plucked));
  return 0;
}

uint8_t pluck(VALUE arr, VALUE key, VALUE *out) {
  long i;
  VALUE item;
  *out = rb_ary_new();
  for (i = 0; i < RARRAY_LEN(arr); ++i) {
    item = RARRAY_AREF(arr, i);
    switch (TYPE(item)) {
      case T_HASH:
        rb_ary_push(*out, rb_hash_aref(item, key));
        break;
      case T_OBJECT:
      case T_STRUCT:
      case T_DATA:
        rb_ary_push(*out, rb_ivar_get(item, key));
        break;
      default:
        return 0;
    }
  }
  return 1;
}

uint8_t array_flatten(VALUE arr, VALUE *flat) {
  long i, j;
  VALUE item;
  *flat = rb_ary_new();
  for (i = 0; i < RARRAY_LEN(arr); ++i) {
    item = RARRAY_AREF(arr, i);
    if (TYPE(item) != T_ARRAY) return 0;
    for (j = 0; j < RARRAY_LEN(item); ++j) {
      rb_ary_push(*flat, RARRAY_AREF(item, j));
    }
  }
  return 1;
}

VALUE type_gcd(VALUE arr) {
  uint8_t type, fp, sign;
  size_t i;
  double magnitude, val;
  VALUE foreach_args[3];
  VALUE flat, item = RARRAY_AREF(arr, 0);
  type = type_check(item);
  switch (type) {
    case LEON_UINT8:
    case LEON_INT8:
    case LEON_UINT16:
    case LEON_INT16:
    case LEON_UINT32:
    case LEON_INT32:
    case LEON_FLOAT:
    case LEON_DOUBLE:
      sign = 0;
      fp = 0;
      val = NUM2DBL(item);
      if (val < 0) sign = 1;
      if (type == LEON_FLOAT || type == LEON_DOUBLE) fp = 1;
      magnitude = ABS(val);
      for (i = 1; i < (size_t) RARRAY_LEN(arr); ++i) {
        item = RARRAY_AREF(arr, i);
        switch (TYPE(item)) {
          case T_BIGNUM:
          case T_FIXNUM:
          case T_FLOAT:
            val = NUM2DBL(item);
            if (val < 0) sign = 1;
            if (TYPE(item) == T_FLOAT) fp = 1;
            if (ABS(val) > magnitude) magnitude = ABS(val);
            break;
          default:
            return INT2NUM(LEON_DYNAMIC);
        }
      }
      if (fp) return INT2NUM(type_check_with_hint(DBL2NUM((sign ? -1 : 1)*magnitude), fp));
      else return INT2NUM(type_check_with_hint(INT2NUM((sign ? - 1 : 1)*((long) magnitude)), fp));
    case LEON_ARRAY:
      if (!array_flatten(arr, &flat)) return INT2NUM(LEON_DYNAMIC);
      item = rb_ary_new();
      rb_ary_push(item, type_gcd(flat));
      return item;
    case LEON_OBJECT:
      sign = 0;
      flat = rb_hash_new();
      foreach_args[0] = flat;
      foreach_args[1] = arr;
      foreach_args[2] = (VALUE) &sign;
      rb_hash_foreach(item, &pluck_each_prop, (VALUE) foreach_args);
      if (sign) return INT2NUM(LEON_DYNAMIC);
      return flat;
    case LEON_DATA:
    case LEON_STRUCT:
    case LEON_NATIVE_OBJECT:
      sign = 0;
      flat = rb_hash_new();
      foreach_args[0] = flat;
      foreach_args[1] = arr;
      foreach_args[2] = (VALUE) &sign;
      rb_ivar_foreach(item, &pluck_each_prop, (VALUE) foreach_args);
      if (sign) return INT2NUM(LEON_DYNAMIC);
      return flat;
    default:
      for (i = 1; i < (size_t) RARRAY_LEN(arr); ++i) {
        if (type_check(RARRAY_AREF(arr, i)) != type) return INT2NUM(LEON_DYNAMIC);
      }
      return INT2NUM(type);
  }
}

VALUE to_template(VALUE self, VALUE data) {
  VALUE out;
  uint8_t type = type_check(data);
  switch (type) {
    case LEON_ARRAY:
      out = rb_ary_new();
      rb_ary_push(out, type_gcd(data));
      return out;
    case LEON_OBJECT:
      out = rb_hash_new();
      rb_hash_foreach(data, &push_assoc_types, out);
      return out;
    case LEON_DATA:
    case LEON_STRUCT:
    case LEON_NATIVE_OBJECT:
      out = rb_hash_new();
      rb_ivar_foreach(data, &push_assoc_types, out);
      return out;
    case LEON_TRUE:
    case LEON_FALSE:
      return INT2NUM(LEON_BOOLEAN);
    case LEON_NULL:
    case LEON_UNDEFINED:
    case LEON_INFINITY:
    case LEON_MINUS_INFINITY:
      return INT2NUM(LEON_DYNAMIC);
    default:
      return INT2NUM(type);
  }
}

VALUE spec_value_to_sorted_hash(spec_value_t *spec) {
  VALUE out, pair;
  size_t i;
  switch (SPEC_TYPE_P(spec)) {
    case TYPE_LITERAL:
      return INT2NUM(SPEC_LIT_P(spec));
    case TYPE_ARRAY:
      out = rb_ary_new();
      rb_ary_push(out, spec_value_to_sorted_hash(SPEC_VALUES_P(spec)[0]));
      return out;
    case TYPE_OBJECT:
      out = rb_ary_new();
      for (i = 0; i < SPEC_LEN_P(spec); i += 2) {
        pair = rb_hash_new();
        rb_hash_aset(pair, ID2SYM(i_key), SPEC_KEY_P(SPEC_VALUES_P(spec)[i]));
        rb_hash_aset(pair, ID2SYM(i_type), spec_value_to_sorted_hash(SPEC_VALUES_P(spec)[i + 1]));
        rb_ary_push(out, pair);
      }
      return out;
    default:
      rb_raise(rb_eRuntimeError, "Corrupted type information.");
      return Qnil;
  }
}

VALUE to_sorted_template(VALUE self, VALUE data) {
  spec_value_t *sorted;
  VALUE out, spec = to_template(self, data);
  sorted = spec_value_new();
  load_spec(sorted, spec);
  sort_spec(sorted);
  out = spec_value_to_sorted_hash(sorted);
  spec_value_free(sorted);
  return out;
}
