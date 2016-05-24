#ifndef TYPES_H
#define TYPES_H

#include <ruby.h>

#define LEON_UINT8 0xff
#define LEON_INT8 0xfe
#define LEON_UINT16 0xfd
#define LEON_INT16 0xfc
#define LEON_UINT32 0xfb
#define LEON_INT32 0xfa
#define LEON_FLOAT 0xf9
#define LEON_DOUBLE 0xf8
#define LEON_LONG 0xf7
#define LEON_ARRAY 0xf6
#define LEON_OBJECT 0xf5
#define LEON_STRING 0xf4
#define LEON_TRUE 0xf3
#define LEON_FALSE 0xf2
#define LEON_BOOLEAN 0xf1
#define LEON_NULL 0xf0
#define LEON_UNDEFINED 0xef
#define LEON_DATE 0xee
#define LEON_BUFFER 0xed
#define LEON_REGEXP 0xec
#define LEON_NAN 0xeb
#define LEON_NATIVE_OBJECT 0xea
#define LEON_INDIRECT 0xe9
#define LEON_CONSTANT 0xe8
#define LEON_INFINITY 0xe7
#define LEON_MINUS_INFINITY 0xe6
#define LEON_EMPTY 0xe5

#define LEON_RATIONAL 0xe4
#define LEON_COMPLEX 0xe3
#define LEON_DATA 0xe2
#define LEON_SYMBOL 0xe1
#define LEON_DATETIME 0xe0
#define LEON_TIME 0xdf
#define LEON_STRUCT 0xde

#define LEON_CHANNEL 0xdc
#define LEON_DYNAMIC 0xdd

uint8_t integer_type_check(long);

uint8_t fp_type_check(double);

uint8_t type_check(VALUE);

uint8_t type_check_with_hint(VALUE, uint8_t);

VALUE rb_type_check(VALUE, VALUE);

#endif
