#include <stdint.h>
#include <stdlib.h>
#include <ruby.h>
#include <custom.h>

custom_type_index_t custom_types;

void type_index_init(void) {
  memset(&custom_types, 0, sizeof(custom_type_index_t));
}

void type_zero(size_t idx) {
  memset(&CUSTOM_TYPE(idx), 0, sizeof(custom_type_t));
}

void type_check_splice(size_t idx) {
  uint8_t i = 0;
  for (i = 0; i < CUSTOM_TYPE_LEN(); ++i) {
    if (CUSTOM_TYPE_CHECK_CONSTANT(i) == idx) {
      memcpy(&CUSTOM_TYPE_CHECK(i), &CUSTOM_TYPE_CHECK(i) + 1, CUSTOM_TYPE_LEN() - i);
      --CUSTOM_TYPE_LEN();
      return;
    }
  }
}

void type_check_push(VALUE detect, uint8_t constant) {
  CUSTOM_TYPE_CHECK_DETECT(CUSTOM_TYPE_LEN()) = detect;
  rb_gc_register_address(&detect);
  CUSTOM_TYPE_CHECK_CONSTANT(CUSTOM_TYPE_LEN()) = constant;
  ++CUSTOM_TYPE_LEN();
}

VALUE define_type(int argc, VALUE *argv, VALUE self) {
  long constantl;
  VALUE constantv, definition, encode, decode, check;
  rb_scan_args(argc, argv, "10:", &constantv, &definition);
  constantl = NUM2LONG(constantv);
  if (constantl < 0 || constantl > 255) rb_raise(rb_eArgError, "Type constant must be between 0 and 255.");
  type_zero(constantl);
  type_check_splice(constantl);
  CUSTOM_TYPE_ACTIVE(constantl) = 1;
  if (definition != Qnil) {
    encode = rb_hash_aref(definition, ID2SYM(rb_intern("encode")));
    if (encode != Qnil) {
      if (TYPE(encode) != T_DATA || rb_obj_is_proc(encode) != Qtrue || rb_proc_arity(encode) != 2) rb_raise(rb_eTypeError, ":encode must be a Proc with arity 2.");
      rb_gc_register_address(&encode);
      CUSTOM_TYPE_ENCODE(constantl) = encode;
    }
    decode = rb_hash_aref(definition, ID2SYM(rb_intern("decode")));
    if (decode != Qnil) {
      if (TYPE(decode) != T_DATA || rb_obj_is_proc(decode) != Qtrue || rb_proc_arity(decode) != 1) rb_raise(rb_eTypeError, ":decode must be a Proc with arity 1.");
      rb_gc_register_address(&decode);
      CUSTOM_TYPE_DECODE(constantl) = decode;
    }
    check = rb_hash_aref(definition, ID2SYM(rb_intern("check")));
    if (check != Qnil) {
      if (TYPE(decode) != T_DATA || rb_obj_is_proc(check) != Qtrue || rb_proc_arity(check) != 1) rb_raise(rb_eTypeError, ":check must be a Proc with arity 1.");
      type_check_push(check, constantl);
    }
  }
  return self;
}

VALUE undefine_type(VALUE self, VALUE constant) {
  long constantl;
  switch (TYPE(constant)) {
    case T_FIXNUM:
    case T_BIGNUM:
    case T_FLOAT:
      break;
    default:
      rb_raise(rb_eTypeError, "Argument must be numeric.");
      break;
  }
  constantl = NUM2LONG(constant);
  type_zero(constantl);
  type_check_splice(constantl);
  return self;
}
