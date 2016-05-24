#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <ruby.h>
#include <types.h>
#include <channel.h>
#include <endianness.h>
#include <string.h>
#include <buffer.h>
#include <template.h>
#include <stubs.h>
#include <encoding.h>
#include <custom.h>

VALUE mLEON, cDate, cTime, cDateTime, cInfinity, cMinusInfinity, cNaN, cUndefined, cBuffer, cBufferIterator, cFile, cIO;

ID i_new, i_source, i_options, i_to_i, i_to_time, i_real, i_imag, i_at, i_to_date, i_to_datetime, i_key, i_type, i_INSPECT_MAX_BYTES, i_write, i_read, i_to_s, i_close, i_call;

void Init_leon(void) {
  VALUE cEncoding;
  ID i_find;
  cEncoding = rb_path2class("Encoding");
  i_find = rb_intern("find");
  encASCII = rb_funcall(cEncoding, i_find, 1, rb_str_new2("ascii-8bit"));
  encUTF8 = rb_funcall(cEncoding, i_find, 1, rb_str_new2("utf-8"));
  encUTF16 = rb_funcall(cEncoding, i_find, 1, rb_str_new2("utf-16le"));
  i_encode = rb_intern("encode");
  i_encoding = rb_intern("encoding");
  i_force_encoding = rb_intern("force_encoding");
  type_index_init();
  rb_require("date");
  isLE = is_little_endian();
  cDate = rb_path2class("Date");
  cTime = rb_path2class("Time");
  cFile = rb_path2class("File");
  cIO = rb_path2class("IO");
  cDateTime = rb_path2class("DateTime");
  mLEON = rb_define_module("LEON");
  i_new = rb_intern("new");
  i_key = rb_intern("key");
  i_type = rb_intern("type");
  i_source = rb_intern("source");
  i_options = rb_intern("options");
  i_to_i = rb_intern("to_i");
  i_to_time = rb_intern("to_time");
  i_real = rb_intern("real");
  i_imag = rb_intern("imag");
  i_at = rb_intern("at");
  i_to_date = rb_intern("to_date");
  i_to_datetime = rb_intern("to_datetime");
  i_read = rb_intern("read");
  i_write = rb_intern("write");
  i_to_s = rb_intern("to_s");
  i_close = rb_intern("close");
  i_call = rb_intern("call");
  i_INSPECT_MAX_BYTES = rb_intern("INSPECT_MAX_BYTES");
  rb_define_const(mLEON, "UINT8", INT2NUM(LEON_UINT8));
  rb_define_const(mLEON, "INT8", INT2NUM(LEON_INT8));
  rb_define_const(mLEON, "UINT16", INT2NUM(LEON_UINT16));
  rb_define_const(mLEON, "INT16", INT2NUM(LEON_INT16));
  rb_define_const(mLEON, "UINT32", INT2NUM(LEON_UINT32));
  rb_define_const(mLEON, "INT32", INT2NUM(LEON_INT32));
  rb_define_const(mLEON, "FLOAT", INT2NUM(LEON_FLOAT));
  rb_define_const(mLEON, "DOUBLE", INT2NUM(LEON_DOUBLE));
  rb_define_const(mLEON, "STRING", INT2NUM(LEON_STRING));
  rb_define_const(mLEON, "UTF8STRING", INT2NUM(LEON_STRING));
  rb_define_const(mLEON, "SYMBOL", INT2NUM(LEON_SYMBOL));
  rb_define_const(mLEON, "BOOLEAN", INT2NUM(LEON_BOOLEAN));
  rb_define_const(mLEON, "DATE", INT2NUM(LEON_DATE));
  rb_define_const(mLEON, "BUFFER", INT2NUM(LEON_BUFFER));
  rb_define_const(mLEON, "REGEXP", INT2NUM(LEON_REGEXP));
  rb_define_const(mLEON, "RATIONAL", INT2NUM(LEON_RATIONAL));
  rb_define_const(mLEON, "COMPLEX", INT2NUM(LEON_COMPLEX));
  rb_define_const(mLEON, "DYNAMIC", INT2NUM(LEON_DYNAMIC));
  cChannel = rb_define_class_under(mLEON, "Channel", rb_cObject);
  cBuffer = rb_define_class_under(mLEON, "Buffer", rb_cObject);
  cBufferIterator = rb_define_class_under(mLEON, "BufferIterator", rb_cObject);
  rb_define_const(cBuffer, "INSPECT_MAX_BYTES", INT2NUM(BUFFER_INSPECT_MAX_BYTES));
  cInfinity = rb_define_class_under(mLEON, "Infinity", rb_cObject);
  cMinusInfinity = rb_define_class_under(mLEON, "MinusInfinity", rb_cObject);
  cNaN = rb_define_class_under(mLEON, "NaN", rb_cObject);
  cUndefined = rb_define_class_under(mLEON, "Undefined", rb_cObject);
  rb_define_singleton_method(mLEON, "define_type", &define_type, -1);
  rb_define_singleton_method(mLEON, "undefine_type", &undefine_type, 1);
  rb_define_alloc_func(cChannel, &alloc_channel);
  rb_define_method(cChannel, "initialize", &initialize_channel, -2);
  rb_define_singleton_method(cChannel, "from_sorted", &channel_from_sorted, 1);
  rb_define_method(cChannel, "_dump", &channel_dump, 1);
  rb_define_singleton_method(cChannel, "_load", &channel_load, 1);
  rb_define_method(cChannel, "encode", &channel_encode, 1);
  rb_define_method(cChannel, "decode", &channel_decode, 1);
  rb_define_alloc_func(cBuffer, &alloc_buffer);
  rb_define_method(cBuffer, "initialize", &buffer_initialize, -1);
  rb_define_method(cBuffer, "write_uint8", &buffer_write_uint8, -1);
  rb_define_method(cBuffer, "write_int8", &buffer_write_int8, -1);
  rb_define_method(cBuffer, "write_uint16_le", &buffer_write_uint16_le, -1);
  rb_define_method(cBuffer, "write_uint16_be", &buffer_write_uint16_be, -1);
  rb_define_method(cBuffer, "write_int16_le", &buffer_write_int16_le, -1);
  rb_define_method(cBuffer, "write_int16_be", &buffer_write_int16_be, -1);
  rb_define_method(cBuffer, "write_uint32_le", &buffer_write_uint32_le, -1);
  rb_define_method(cBuffer, "write_uint32_be", &buffer_write_uint32_be, -1);
  rb_define_method(cBuffer, "write_int32_le", &buffer_write_int32_le, -1);
  rb_define_method(cBuffer, "write_int32_be", &buffer_write_int32_be, -1);
  rb_define_method(cBuffer, "write_float_le", &buffer_write_float_le, -1);
  rb_define_method(cBuffer, "write_float_be", &buffer_write_float_be, -1);
  rb_define_method(cBuffer, "write_double_le", &buffer_write_double_le, -1);
  rb_define_method(cBuffer, "write_double_be", &buffer_write_double_be, -1);
  rb_define_method(cBuffer, "read_uint8", &buffer_read_uint8, 1);
  rb_define_method(cBuffer, "read_int8", &buffer_read_int8, 1);
  rb_define_method(cBuffer, "read_uint16_le", &buffer_read_uint16_le, 1);
  rb_define_method(cBuffer, "read_uint16_be", &buffer_read_uint16_be, 1);
  rb_define_method(cBuffer, "read_int16_le", &buffer_read_int16_le, 1);
  rb_define_method(cBuffer, "read_int16_be", &buffer_read_int16_be, 1);
  rb_define_method(cBuffer, "read_uint32_le", &buffer_read_uint32_le, 1);
  rb_define_method(cBuffer, "read_uint32_be", &buffer_read_uint32_be, 1);
  rb_define_method(cBuffer, "read_int32_le", &buffer_read_int32_le, 1);
  rb_define_method(cBuffer, "read_int32_be", &buffer_read_int32_be, 1);
  rb_define_method(cBuffer, "read_float_le", &buffer_read_float_le, 1);
  rb_define_method(cBuffer, "read_float_be", &buffer_read_float_be, 1);
  rb_define_method(cBuffer, "read_double_le", &buffer_read_double_le, 1);
  rb_define_method(cBuffer, "read_double_be", &buffer_read_double_be, 1);
  rb_define_method(cBuffer, "length", &buffer_length, 0);
  rb_define_method(cBuffer, "length=", &buffer_length_equals, 1);
  rb_define_method(cBuffer, "[]", &buffer_operator_bracket, 1);
  rb_define_method(cBuffer, "[]=", &buffer_operator_bracket_equals, 2);
  rb_define_method(cBuffer, "inspect", &buffer_inspect, 0);
  rb_define_method(cBuffer, "to_s", &buffer_to_s, 0);
  rb_define_method(cBuffer, "to_hex", &buffer_to_hex, 0);
  rb_define_method(cBuffer, "to_base64", &buffer_to_base64, 0);
  rb_define_method(cBuffer, "to_file", &buffer_to_file, -1);
  rb_define_method(cBuffer, "slice", &buffer_slice, -1);
  rb_define_method(cBuffer, "slice!", &buffer_slice_bang, -1);
  rb_define_singleton_method(cBuffer, "from_file", &buffer_from_file, 1);
  rb_define_singleton_method(cBuffer, "from_base64", &buffer_from_base64, 1);
  rb_define_singleton_method(cBuffer, "from_hex", &buffer_from_hex, 1);
  rb_define_method(cBuffer, "==", &buffer_equals, 1);
  rb_define_method(cBuffer, "_dump", &buffer_dump, 1);
  rb_define_singleton_method(cBuffer, "_load", &buffer_load, 1);
  rb_define_method(cBuffer, "is_view?", &is_view, 0);
  rb_define_method(cBuffer, "make_view", &make_view, -1);
  rb_define_alloc_func(cBufferIterator, &alloc_iterator);
  rb_define_method(cBufferIterator, "initialize", &buffer_iterator_initialize, -1);
  rb_define_method(cBufferIterator, "write_uint8", &buffer_iterator_write_uint8, 1);
  rb_define_method(cBufferIterator, "write_int8", &buffer_iterator_write_int8, 1);
  rb_define_method(cBufferIterator, "write_uint16_le", &buffer_iterator_write_uint16_le, 1);
  rb_define_method(cBufferIterator, "write_uint16_be", &buffer_iterator_write_uint16_be, 1);
  rb_define_method(cBufferIterator, "write_int16_le", &buffer_iterator_write_int16_le, 1);
  rb_define_method(cBufferIterator, "write_int16_be", &buffer_iterator_write_int16_be, 1);
  rb_define_method(cBufferIterator, "write_uint32_le", &buffer_iterator_write_uint32_le, 1);
  rb_define_method(cBufferIterator, "write_uint32_be", &buffer_iterator_write_uint32_be, 1);
  rb_define_method(cBufferIterator, "write_int32_le", &buffer_iterator_write_int32_le, 1);
  rb_define_method(cBufferIterator, "write_int32_be", &buffer_iterator_write_int32_be, 1);
  rb_define_method(cBufferIterator, "write_float_le", &buffer_iterator_write_float_le, 1);
  rb_define_method(cBufferIterator, "write_float_be", &buffer_iterator_write_float_be, 1);
  rb_define_method(cBufferIterator, "write_double_le", &buffer_iterator_write_double_le, 1);
  rb_define_method(cBufferIterator, "write_double_be", &buffer_iterator_write_double_be, 1);
  rb_define_method(cBufferIterator, "write_value", &buffer_iterator_write_value, 2);
  rb_define_method(cBufferIterator, "write_string_utf8", &buffer_iterator_write_string_utf8, 1);
  rb_define_method(cBufferIterator, "write_string_utf16", &buffer_iterator_write_string_utf16, 1);
  rb_define_method(cBufferIterator, "write_string_ascii", &buffer_iterator_write_string_ascii, 1);
  rb_define_method(cBufferIterator, "read_uint8", &buffer_iterator_read_uint8, 0);
  rb_define_method(cBufferIterator, "read_int8", &buffer_iterator_read_int8, 0);
  rb_define_method(cBufferIterator, "read_uint16_le", &buffer_iterator_read_uint16_le, 0);
  rb_define_method(cBufferIterator, "read_uint16_be", &buffer_iterator_read_uint16_be, 0);
  rb_define_method(cBufferIterator, "read_int16_le", &buffer_iterator_read_int16_le, 0);
  rb_define_method(cBufferIterator, "read_int16_be", &buffer_iterator_read_int16_be, 0);
  rb_define_method(cBufferIterator, "read_uint32_le", &buffer_iterator_read_uint32_le, 0);
  rb_define_method(cBufferIterator, "read_uint32_be", &buffer_iterator_read_uint32_be, 0);
  rb_define_method(cBufferIterator, "read_int32_le", &buffer_iterator_read_int32_le, 0);
  rb_define_method(cBufferIterator, "read_int32_be", &buffer_iterator_read_int32_be, 0);
  rb_define_method(cBufferIterator, "read_float_le", &buffer_iterator_read_float_le, 0);
  rb_define_method(cBufferIterator, "read_float_be", &buffer_iterator_read_float_be, 0);
  rb_define_method(cBufferIterator, "read_double_le", &buffer_iterator_read_double_le, 0);
  rb_define_method(cBufferIterator, "read_double_be", &buffer_iterator_read_double_be, 0);
  rb_define_method(cBufferIterator, "read_value", &buffer_iterator_read_value, 1);
  rb_define_method(cBufferIterator, "read_string_ascii", &buffer_iterator_read_string_ascii, 0);
  rb_define_method(cBufferIterator, "read_string_utf8", &buffer_iterator_read_string_utf8, 0);
  rb_define_method(cBufferIterator, "read_string_utf16", &buffer_iterator_read_string_utf16, 0);
  rb_define_method(cBufferIterator, "reset", &buffer_iterator_reset, 0);
  rb_define_method(cBufferIterator, "buffer", &buffer_iterator_buffer, 0);
  rb_define_method(cBufferIterator, "position", &buffer_iterator_position, 0);
  rb_define_singleton_method(mLEON, "to_template", &to_template, 1);
  rb_define_singleton_method(mLEON, "to_sorted_template", &to_sorted_template, 1);
  rb_define_singleton_method(mLEON, "type_check", &rb_type_check, 1);
}
