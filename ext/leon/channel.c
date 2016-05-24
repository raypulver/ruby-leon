#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <ruby.h>
#include <ruby/encoding.h>
#include <types.h>
#include <channel.h>
#include <buffer.h>
#include <endianness.h>
#include <swizzle.h>
#include <reverse.h>
#include <stubs.h>
#include <encoding.h>
#include <custom.h>

VALUE cChannel;

spec_value_t *spec_value_new() {
  spec_value_t *retval = malloc(sizeof(spec_value_t));
  if (!retval && errno == ENOMEM) rb_raise(rb_eNoMemError, "Could not allocate template.");
  SPEC_TYPE_P(retval) = TYPE_LITERAL;
  return retval;
}

void spec_value_free(spec_value_t *spec) {
  uint32_t i;
  switch (SPEC_TYPE_P(spec)) {
    case TYPE_ARRAY:
      spec_value_free(SPEC_ARR_P(spec)->values[0]);
      free(SPEC_ARR_P(spec));
      break;
    case TYPE_MULTIARRAY:
    case TYPE_OBJECT:
      for (i = 0; i < SPEC_ARR_P(spec)->len; ++i) {
        spec_value_free(SPEC_ARR_P(spec)->values[i]);
      }
      free(SPEC_ARR_P(spec));
      break;
  }
  free(spec);
}

type_array_t *type_array_new(void) {
  type_array_t *retval = malloc(sizeof(type_array_t));
  if (!retval && errno == ENOMEM) rb_raise(rb_eNoMemError, "Could not allocate template.");
  retval->alloc = TYPE_ARRAY_DEFAULT_ALLOC;
  retval->len = 0;
  retval->values = malloc(TYPE_ARRAY_DEFAULT_ALLOC*sizeof(spec_value_t *));
  if (!retval->values && errno == ENOMEM) rb_raise(rb_eNoMemError, "Could not allocate template.");
  return retval;
}

type_array_t *type_array_new_single_alloc(void) {
  type_array_t *retval = malloc(sizeof(type_array_t));
  if (!retval && errno == ENOMEM) rb_raise(rb_eNoMemError, "Could not allocate template.");
  retval->alloc = 1;
  retval->len = 0;
  retval->values = malloc(sizeof(spec_value_t *));
  if (!retval->values && errno == ENOMEM) rb_raise(rb_eNoMemError, "Could not allocate template.");
  return retval;
}

void type_array_maybe_alloc(type_array_t *t) {
  void *status;
  if (t->alloc < t->len + 1) {
    t->alloc <<= 1;
    status = realloc(t->values, t->alloc*sizeof(spec_value_t *));
    if (!status && errno == ENOMEM) rb_raise(rb_eNoMemError, "Could not allocate template.");
    t->values = status;
  }
}

void type_array_push(type_array_t *t, spec_value_t *sv) {
  type_array_maybe_alloc(t);
  t->values[t->len] = sv;
  ++t->len;
}

int push_each_pair(VALUE key, VALUE val, VALUE arr) {
  spec_value_t *k, *v;
  type_array_maybe_alloc((type_array_t *) arr);
  k = spec_value_new();
  SPEC_TYPE_P(k) = TYPE_KEY;
  SPEC_KEY_P(k) = key;
  rb_gc_mark(key);
  ((type_array_t *) arr)->values[((type_array_t *) arr)->len] = k;
  ((type_array_t *) arr)->len++;
  type_array_maybe_alloc((type_array_t *) arr);
  v = spec_value_new();
  load_spec(v, val);
  ((type_array_t *) arr)->values[((type_array_t *) arr)->len] = v;
  ((type_array_t *) arr)->len++;
  return 0;
}

int sort_spec_cb(const void *, const void *);

void sort_spec(spec_value_t *spec) {
  uint32_t i;
  switch (SPEC_TYPE_P(spec)) {
    case TYPE_OBJECT: 
      qsort(SPEC_ARR_P(spec)->values, SPEC_ARR_P(spec)->len / 2, sizeof(spec_value_t *)*2, &sort_spec_cb);
      for (i = 1; i < SPEC_ARR_P(spec)->len; i += 2) {
        sort_spec(SPEC_ARR_P(spec)->values[i]);
      }
      break;
    case TYPE_ARRAY:
      sort_spec(SPEC_ARR_P(spec)->values[0]);
      break;
  }
}

int sort_spec_cb(const void *a, const void *b) {
  size_t min;
  int retval;
  int cmp;
  VALUE astr, bstr;
  if (TYPE(SPEC_KEY_P(*((const spec_value_t **) a))) == T_SYMBOL) astr = rb_sym2str(SPEC_KEY_P(*((const spec_value_t **) a)));
  else astr = SPEC_KEY_P(*((const spec_value_t **) a));;
  if (TYPE(SPEC_KEY_P(*((const spec_value_t **) b))) == T_SYMBOL) bstr = rb_sym2str(SPEC_KEY_P(*((const spec_value_t **) b)));
  else bstr = SPEC_KEY_P(*((const spec_value_t **) a));;
  if (RSTRING_LEN(astr) < RSTRING_LEN(bstr)) {
    min = RSTRING_LEN(astr);
    retval = -1;
  } else if (RSTRING_LEN(astr) == RSTRING_LEN(bstr)) {
    min = RSTRING_LEN(astr);
    retval = 0;
  } else {
    min = RSTRING_LEN(bstr);
    retval = 1;
  }
  if (cmp = memcmp(RSTRING_PTR(astr), RSTRING_PTR(bstr), min)) return cmp;
  else return retval;
}
    
void load_spec(spec_value_t *spec_intern, VALUE spec) {
  spec_value_t *sv;
  long i;
  switch (type_check(spec)) {
    case LEON_UINT8:
      SPEC_LIT_P(spec_intern) = NUM2CHR(spec);
      break;
    case LEON_ARRAY:
      if (RARRAY_LEN(spec) == 0) rb_raise(rb_eTypeError, "Arrays supplied to LEON::Channel.new must not be empty.");
      else if (RARRAY_LEN(spec) == 1) {
        SPEC_TYPE_P(spec_intern) = TYPE_ARRAY;
        SPEC_ARR_P(spec_intern) = type_array_new_single_alloc();
        SPEC_ARR_P(spec_intern)->len++;
        SPEC_ARR_P(spec_intern)->values[0] = spec_value_new();
        load_spec(SPEC_ARR_P(spec_intern)->values[0], RARRAY_AREF(spec, 0));
      } else {
        SPEC_TYPE_P(spec_intern) = TYPE_MULTIARRAY;
        SPEC_ARR_P(spec_intern) = type_array_new();
        for (i = 0; i < RARRAY_LEN(spec); ++i) {
          sv = spec_value_new();
          load_spec(sv, RARRAY_AREF(spec, i));
          type_array_push(SPEC_ARR_P(spec_intern), sv);
        }
      }
      break;
    case LEON_OBJECT:
      SPEC_TYPE_P(spec_intern) = TYPE_OBJECT;
      SPEC_ARR_P(spec_intern) = type_array_new();
      rb_hash_foreach(spec, &push_each_pair, (VALUE) SPEC_ARR_P(spec_intern));
      break;
    case LEON_CHANNEL:
      Data_Get_Struct(spec, spec_value_t, sv);
      SPEC_TYPE_P(spec_intern) = SPEC_TYPE_P(sv);
      switch (SPEC_TYPE_P(spec_intern)) {
        case TYPE_LITERAL:
          SPEC_LIT_P(spec_intern) = SPEC_LIT_P(sv);
          break;
        case TYPE_ARRAY:
        case TYPE_MULTIARRAY:
        case TYPE_OBJECT:
          SPEC_ARR_P(spec_intern) = SPEC_ARR_P(spec_intern);
          break;
        case TYPE_KEY:
          SPEC_KEY_P(spec_intern) = SPEC_KEY_P(spec_intern);
          break;
        default:
          rb_raise(rb_eRuntimeError, "Template data corrupted.");
          break;
      }
      break;
    default:
      rb_raise(rb_eTypeError, "Must supply only valid LEON type constants, Arrays with one element, or Hashes.");
  }
}

static void load_sorted_spec(spec_value_t *spec_intern, VALUE spec) {
  VALUE key, val, pair;
  spec_value_t *k, *v;
  long i;
  switch (type_check(spec)) {
    case LEON_UINT8:
      SPEC_LIT_P(spec_intern) = NUM2CHR(spec);
      break;
    case LEON_CHANNEL:
      Data_Get_Struct(spec, spec_value_t, v);
      SPEC_TYPE_P(spec_intern) = SPEC_TYPE_P(v);
      switch (SPEC_TYPE_P(spec_intern)) {
        case TYPE_LITERAL:
          SPEC_LIT_P(spec_intern) = SPEC_LIT_P(v);
          break;
        case TYPE_ARRAY:
        case TYPE_MULTIARRAY:
        case TYPE_OBJECT:
          SPEC_ARR_P(spec_intern) = SPEC_ARR_P(spec_intern);
          break;
        case TYPE_KEY:
          SPEC_KEY_P(spec_intern) = SPEC_KEY_P(spec_intern);
          break;
        default:
          rb_raise(rb_eRuntimeError, "Template data corrupted.");
          break;
      }
      break;
    case LEON_ARRAY:
      if (RARRAY_LEN(spec) > 0) {
        if (TYPE(RARRAY_AREF(spec, 0)) == T_HASH) {
          SPEC_TYPE_P(spec_intern) = TYPE_OBJECT;
          SPEC_ARR_P(spec_intern) = type_array_new();
          for (i = 0; i < RARRAY_LEN(spec); ++i) {
            pair = RARRAY_AREF(spec, i);
            if (TYPE(pair) != T_HASH) rb_raise(rb_eArgError, "Arrays with multiple elements must contain key-type pairs.");
            key = rb_hash_aref(pair, rb_str_new("key", sizeof("key") - 1));
            if (key == Qnil) key = rb_hash_aref(pair, ID2SYM(i_key));
            if (key == Qnil) rb_raise(rb_eArgError, "Object key missing from sorted template.");
            val = rb_hash_aref(pair, rb_str_new("type", sizeof("type") - 1));
            if (val == Qnil) val = rb_hash_aref(pair, ID2SYM(i_type));
            if (val == Qnil) rb_raise(rb_eArgError, "Object value type missing from sorted template.");
            k = spec_value_new();
            SPEC_TYPE_P(k) = TYPE_KEY;
            SPEC_KEY_P(k) = key;
            rb_gc_mark(key);
            type_array_maybe_alloc(SPEC_ARR_P(spec_intern));
            SPEC_VALUES_P(spec_intern)[SPEC_LEN_P(spec_intern)] = k;
            SPEC_LEN_P(spec_intern)++;
            v = spec_value_new();
            load_sorted_spec(v, val);
            type_array_maybe_alloc(SPEC_ARR_P(spec_intern));
            SPEC_VALUES_P(spec_intern)[SPEC_LEN_P(spec_intern)] = v;
            SPEC_LEN_P(spec_intern)++;
          }
        } else {
          if (RARRAY_LEN(spec) != 1) rb_raise(rb_eArgError, "Arrays with more than one element must contain key-type pairs.");
          SPEC_TYPE_P(spec_intern) = TYPE_ARRAY;
          SPEC_ARR_P(spec_intern) = type_array_new_single_alloc();
          SPEC_ARR_P(spec_intern)->len++;
          SPEC_ARR_P(spec_intern)->values[0] = spec_value_new();
          load_sorted_spec(SPEC_ARR_P(spec_intern)->values[0], RARRAY_AREF(spec, 0));
        }
      } else rb_raise(rb_eArgError, "Arrays in template must not be empty.");
      break;
    default:
      rb_raise(rb_eTypeError, "Must supply only valid LEON type constants or Arrays with either one element or key-type pairs.");
  }
}

void append_spec(spec_value_t *spec, spec_value_t *add) {
  spec_value_t *child;
  if (SPEC_TYPE_P(spec) == TYPE_MULTIARRAY) type_array_push(SPEC_ARR_P(spec), add);
  else {
    child = spec_value_new();
    SPEC_TYPE_P(child) = SPEC_TYPE_P(spec);
    switch (SPEC_TYPE_P(child)) {
      case TYPE_LITERAL:
        SPEC_LIT_P(child) = SPEC_LIT_P(spec);
        break;
      case TYPE_OBJECT:
      case TYPE_ARRAY:
        SPEC_ARR_P(child) = SPEC_ARR_P(spec);
        break;
      case TYPE_KEY:
        SPEC_KEY_P(child) = SPEC_KEY_P(spec);
        break;
      default:
        rb_raise(rb_eRuntimeError, "Template data corrupted.");
        return;
    }
    SPEC_TYPE_P(spec) = TYPE_MULTIARRAY;
    SPEC_ARR_P(spec) = type_array_new();
    type_array_push(SPEC_ARR_P(spec), child);
    type_array_push(SPEC_ARR_P(spec), add);
  }
}

VALUE channel_from_sorted(VALUE self, VALUE spec) {
  spec_value_t *spec_intern;
  VALUE out = rb_funcall(cChannel, i_new, 1, INT2NUM(LEON_UINT8));
  Data_Get_Struct(out, spec_value_t, spec_intern);
  load_sorted_spec(spec_intern, spec);
  return out;
}

VALUE initialize_channel(VALUE self, VALUE args) {
  spec_value_t *spec_intern, *spec_add;
  int i;
  VALUE first;
  Data_Get_Struct(self, spec_value_t, spec_intern);
  if (RARRAY_LEN(args) < 1) first = INT2NUM(LEON_DYNAMIC);
  else first = RARRAY_AREF(args, 0);
  load_spec(spec_intern, first);
  sort_spec(spec_intern);
  for (i = 1; i < RARRAY_LEN(args); ++i) {
    spec_add = spec_value_new();
    load_spec(spec_add, RARRAY_AREF(args, i));
    sort_spec(spec_add);
    append_spec(spec_intern, spec_add);
  }
  return self;
}

VALUE alloc_channel(VALUE klass) {
  return Data_Wrap_Struct(klass, NULL, &free_channel, (void *) spec_value_new());
}

static inline void append(buffer_t *out, void *bytes, size_t sz) {
  buffer_append(out, bytes, sz);
}

static inline void appendr(buffer_t *out, void *bytes, size_t sz) {
  buffer_appendr(out, bytes, sz);
}

static inline void appendc(buffer_t *out, uint8_t byte) {
  buffer_append(out, (void *) &byte, 1);
}


static void encode_regexp(buffer_t *out, VALUE regexp) {
  VALUE src, flags;
  src = rb_funcall(regexp, i_source, 0);
  flags = rb_funcall(regexp, i_options, 0);
  append_str_utf8(out, src);
  encode_int(out, NUM2INT(flags), LEON_UINT8);
}

static void encode_date(buffer_t *out, VALUE date) {
  VALUE intval, timeval;
  uint8_t date_t = type_check(date);
  switch (date_t) {
    case LEON_TIME:
      intval = rb_funcall(date, i_to_i, 0);
      encode_double(out, ((double) NUM2INT(intval)) * 1000, LEON_DOUBLE);
      break;
    case LEON_DATE:
    case LEON_DATETIME:
      timeval = rb_funcall(date, i_to_time, 0);
      intval = rb_funcall(timeval, i_to_i, 0);
      encode_double(out, ((double) NUM2INT(intval)) * 1000, LEON_DOUBLE);
      break;
    default:
      rb_raise(rb_eTypeError, "Expected a Date, Time, or DateTime.");
      break;
  }
}

void encode_rational(buffer_t *out, VALUE data) {
  encode_double(out, (double) NUM2DBL(rb_rational_num(data)), LEON_DOUBLE);
  encode_double(out, (double) NUM2DBL(rb_rational_den(data)), LEON_DOUBLE);
}

void encode_complex(buffer_t *out, VALUE data) {
  double r, imag;
  r = NUM2DBL(rb_funcall(data, i_real, 0));
  imag = NUM2DBL(rb_funcall(data, i_imag, 0));
  encode_double(out, r, LEON_DOUBLE);
  encode_double(out, imag, LEON_DOUBLE);
}

int count_elements(VALUE key, VALUE val, VALUE count) {
  *((size_t *) count) = ((*((size_t *) count)) + 1);
  return 0;
}

static inline size_t hash_num_elements(VALUE hash) {
  size_t count = 0;
  rb_hash_foreach(hash, &count_elements, (VALUE) &count);
  return count;
}

static inline size_t obj_num_elements(VALUE obj) {
  size_t count;
  rb_ivar_foreach(obj, &count_elements, (VALUE) &count);
  return count;
}

int encode_each_pair(VALUE key, VALUE val, VALUE aux) {
  VALUE str;
  if (TYPE(key) == T_STRING) append_str_utf8((buffer_t *) aux, key);
  else if (TYPE(key) == T_FIXNUM) {
    str = rb_sym2str(ID2SYM(key));
    encode_sym((buffer_t *) aux, str);
  } else {
    str = rb_sym2str(key);
    encode_sym((buffer_t *) aux, str);
  }
  encode_value((buffer_t *) aux, val, type_check(val));
  return 0;
} 

int encode_each_obj_pair(VALUE key, VALUE val, VALUE aux) {
  VALUE str;
  if (TYPE(key) == T_FIXNUM) {
    str = rb_sym2str(ID2SYM(key));
    encode_sym((buffer_t *) aux, rb_str_new(RSTRING_PTR(str) + 1, RSTRING_LEN(str) - 1));
  } else {
    str = rb_sym2str(key);
    encode_sym((buffer_t *) aux, rb_str_new(RSTRING_PTR(str) + 1, RSTRING_LEN(str) - 1));
  }
  encode_value((buffer_t *) aux, val, type_check(val));
  return 0;
}

static inline void encode_hash(buffer_t *out, VALUE hash) {
  rb_hash_foreach(hash, &encode_each_pair, (VALUE) out);
}

static inline void encode_obj(buffer_t *out, VALUE obj) {
  rb_ivar_foreach(obj, &encode_each_obj_pair, (VALUE) out);
}

uint8_t set_len(buffer_t *out, size_t idx, size_t len) {
  union {
    uint16_t us;
    uint32_t ui;
    uint8_t uc;
  } na;
  if (len < 0xfe) {
    na.uc = len;
    buffer_setr(out, &na.uc, 1, idx);
    return 1;
  }
  if (len < 0x100) {
    na.uc = len;
    buffer_setr(out, &na.uc, 1, idx);
    na.uc = 0xff;
    buffer_setr(out, &na.uc, 1, idx + 1);
    return 2;
  }
  if (len < 0xff00) {
    na.uc = 0xff;
    buffer_setr(out, &na.uc, 1, idx);
    na.us = len;
    if (!isLE) buffer_set(out, &na.us, 2, idx + 1);
    else buffer_setr(out, &na.us, 2, idx + 1);
    return 3;
  }
  if (len < 0xff000000) {
    na.uc = 0xfe;
    buffer_setr(out, &na.uc, 1, idx);
    na.ui = len;
    if (!isLE) buffer_set(out, &na.ui, 4, idx + 1);
    else buffer_setr(out, &na.ui, 4, idx + 1);
    return 5;
  }
  rb_raise(rb_eArgError, "LEON only supports array and string lengths up to %i.", 0xfeffffff);
  return 0;
}

uint8_t encode_len(buffer_t *out, size_t len) {
  union {
    uint16_t us;
    uint32_t ui;
  } na;
  if (len < 0xfe) {
    appendc(out, len);
    return 1;
  }
  if (len < 0x100) {
    appendc(out, len);
    appendc(out, 0xff);
    return 2;
  }
  if (len < 0xff00) {
    appendc(out, 0xff);
    na.us = len;
    if (!isLE) buffer_append(out, &na.us, 2);
    else buffer_appendr(out, &na.us, 2);
    return 3;
  }
  if (len < 0xff000000) {
    appendc(out, 0xfe);
    na.ui = len;
    if (!isLE) buffer_append(out, &na.ui, 4);
    else buffer_appendr(out, &na.ui, 4);
    return 5;
  }
  rb_raise(rb_eArgError, "LEON only supports array and string lengths up to %i.", 0xfeffffff);
  return 0;
}

size_t append_str_utf16(buffer_t *out, VALUE str) {
  uint8_t len_bytes;
  if (rb_funcall(str, i_encoding, 0) != encUTF16) str = rb_funcall(str, i_encode, 1, encUTF16);
  len_bytes = encode_len(out, RSTRING_LEN(str) / 2);
  append(out, RSTRING_PTR(str), RSTRING_LEN(str));
  return RSTRING_LEN(str) + len_bytes;
}

size_t append_str_utf8(buffer_t *out, VALUE str) {
  uint8_t len_bytes;
  if (rb_funcall(str, i_encoding, 0) != encUTF8) str = rb_funcall(str, i_encode, 1, encUTF8);
  len_bytes = encode_len(out, RSTRING_LEN(str));
  append(out, RSTRING_PTR(str), RSTRING_LEN(str));
  return RSTRING_LEN(str) + len_bytes;
}

size_t append_str_ascii(buffer_t *out, VALUE str) {
  uint8_t len_bytes;
  if (rb_funcall(str, i_encoding, 0) != encASCII) str = rb_funcall(str, i_encode, 1, encASCII);
  len_bytes = encode_len(out, RSTRING_LEN(str));
  append(out, RSTRING_PTR(str), RSTRING_LEN(str));
  return RSTRING_LEN(str) + len_bytes;
}

size_t set_str_ascii(buffer_t *out, size_t idx, VALUE str) {
  uint8_t len_bytes;
  if (rb_funcall(str, i_encoding, 0) != encASCII) str = rb_funcall(str, i_encode, 1, encASCII);
  len_bytes = set_len(out, idx, RSTRING_LEN(str));
  buffer_set(out, RSTRING_PTR(str), RSTRING_LEN(str), idx + len_bytes);
  return RSTRING_LEN(str) + len_bytes;
}
size_t set_str_utf8(buffer_t *out, size_t idx, VALUE str) {
  uint8_t len_bytes;
  if (rb_funcall(str, i_encoding, 0) != encUTF8) str = rb_funcall(str, i_encode, 1, encUTF8);
  len_bytes = set_len(out, idx, RSTRING_LEN(str));
  buffer_set(out, RSTRING_PTR(str), RSTRING_LEN(str), idx + len_bytes);
  return RSTRING_LEN(str) + len_bytes;
}
size_t set_str_utf16(buffer_t *out, size_t idx, VALUE str) {
  uint8_t len_bytes;
  if (rb_funcall(str, i_encoding, 0) != encUTF16) str = rb_funcall(str, i_encode, 1, encUTF16);
  len_bytes = set_len(out, idx, RSTRING_LEN(str));
  buffer_set(out, RSTRING_PTR(str), RSTRING_LEN(str), idx + len_bytes);
  return RSTRING_LEN(str) + len_bytes;
}

void encode_sym(buffer_t *out, VALUE sym) {
  append_str_utf8(out, sym);
}

static void encode_buffer(buffer_t *out, VALUE buffer) {
  buffer_t *buffer_intern;
  Data_Get_Struct(buffer, buffer_t, buffer_intern);
  encode_len(out, BUFFER_LEN_P(buffer_intern));
  append(out, BUFFER_DATA_P(buffer_intern), BUFFER_LEN_P(buffer_intern));
}

void encode_value(buffer_t *out, VALUE data, uint8_t type) {
  size_t idx;
  VALUE v;
  if (CUSTOM_TYPE_ACTIVE(type)) {
    appendc(out, type);
    v = iterator_from_native_buffer(out, BUFFER_LEN_P(out));
    if (!CUSTOM_TYPE_ENCODE(type)) rb_raise(rb_eRuntimeError, "Tried to encode type %i which has no defined encode Proc.", type);
    rb_funcall(CUSTOM_TYPE_ENCODE(type), i_call, 2, v, data);
    return;
  }
  switch (type) {
    case LEON_UINT8:
    case LEON_INT8:
    case LEON_UINT16:
    case LEON_INT16:
    case LEON_UINT32:
    case LEON_INT32:
      appendc(out, type);
      encode_int(out, NUM2LONG(data), type);
      break;
    case LEON_FLOAT:
    case LEON_DOUBLE:
      appendc(out, type);
      encode_double(out, NUM2DBL(data), type);
      break;
    case LEON_RATIONAL:
      appendc(out, type);
      encode_rational(out, data);
      break;
    case LEON_COMPLEX:
      appendc(out, type);
      encode_complex(out, data);
      break;
    case LEON_STRING:
      appendc(out, type);
      append_str_utf8(out, data);
      break;
    case LEON_SYMBOL:
      appendc(out, type);
      encode_sym(out, rb_sym2str(data));
      break;
    case LEON_REGEXP:
      appendc(out, type);
      encode_regexp(out, data);
      break;
    case LEON_BUFFER:
      appendc(out, type);
      encode_buffer(out, data);
      break;
    case LEON_DATE:
    case LEON_DATETIME:
    case LEON_TIME:
      appendc(out, type);
      encode_date(out, data);
      break;
    case LEON_BOOLEAN:
      if (data) appendc(out, LEON_TRUE);
      else appendc(out, LEON_FALSE);
      break;
    case LEON_ARRAY:
      appendc(out, type);
      encode_len(out, RARRAY_LEN(data));
      for (idx = 0; idx < (size_t) RARRAY_LEN(data); ++idx) {
        v = RARRAY_AREF(data, idx);
        encode_value(out, v, type_check(v));
      }
      break;
    case LEON_OBJECT:
      appendc(out, type);
      encode_len(out, hash_num_elements(data));
      encode_hash(out, data);
      break;
    case LEON_STRUCT:
    case LEON_DATA:
    case LEON_NATIVE_OBJECT:
      appendc(out, LEON_OBJECT);
      encode_len(out, obj_num_elements(data));
      encode_obj(out, data);
      break;
    default:
      appendc(out, type);
      break;
  }
}

void leon_encode(buffer_t *out, VALUE data, spec_value_t *spec) {
  uint32_t idx;
  VALUE it;
  uint8_t type = type_check(data);
  char *key;
  switch (SPEC_TYPE_P(spec)) {
    case TYPE_OBJECT:
      switch (type) {
        case LEON_OBJECT:
          for (idx = 0; idx < SPEC_ARR_P(spec)->len; idx += 2) {
            leon_encode(out, rb_hash_aref(data, SPEC_KEY_P(SPEC_ARR_P(spec)->values[idx])), SPEC_ARR_P(spec)->values[idx + 1]);
          }
          break;
        case LEON_NATIVE_OBJECT:
        case LEON_DATA:
        case LEON_STRUCT:
          for (idx = 0; idx < SPEC_ARR_P(spec)->len; idx += 2) {
            key = ALLOC_N(char, RSTRING_LEN(SPEC_KEY_P(SPEC_ARR_P(spec)->values[idx])) + 2);
            key[0] = '@';
            memcpy(key + 1, RSTRING_PTR(SPEC_KEY_P(SPEC_ARR_P(spec)->values[idx])), RSTRING_LEN(SPEC_KEY_P(SPEC_ARR_P(spec)->values[idx])));
            key[RSTRING_LEN(SPEC_KEY_P(SPEC_ARR_P(spec)->values[idx])) + 1] = '\0';
            leon_encode(out, rb_ivar_get(data, rb_intern(key)), SPEC_ARR_P(spec)->values[idx + 1]);
            free(key);
          }
          break;
        default:
          rb_raise(rb_eTypeError, "Was expecting a Hash or Object.");
          break;
      }
      break;
    case TYPE_MULTIARRAY:
      if ((size_t) RARRAY_LEN(data) < SPEC_LEN_P(spec)) rb_raise(rb_eArgError, "Expected an array with %li values, got %li.", (long) SPEC_LEN_P(spec), RARRAY_LEN(data));
      for (idx = 0; idx < SPEC_LEN_P(spec); ++idx) {
        leon_encode(out, RARRAY_AREF(data, idx), SPEC_VALUES_P(spec)[idx]);
      }
      break;
    case TYPE_ARRAY:
      if (type != LEON_ARRAY) {
        rb_raise(rb_eTypeError, "Was expecting an Array.");
        break;
      }
      encode_len(out, RARRAY_LEN(data));
      for (idx = 0; idx < RARRAY_LEN(data); ++idx) {
        leon_encode(out, RARRAY_AREF(data, idx), SPEC_ARR_P(spec)->values[0]);
      }
      break;
    case TYPE_LITERAL:
      if (CUSTOM_TYPE_ACTIVE(SPEC_LIT_P(spec))) {
        if (!CUSTOM_TYPE_ENCODE(SPEC_LIT_P(spec))) rb_raise(rb_eRuntimeError, "Tried to decode type %i which has no defined decode Proc.", SPEC_LIT_P(spec));
        it = iterator_from_native_buffer(out, BUFFER_LEN_P(out));
        rb_funcall(CUSTOM_TYPE_ENCODE(SPEC_LIT_P(spec)), i_call, 2, it, data);
        return;
      }
      switch (SPEC_LIT_P(spec)) {
        case LEON_UINT8:
        case LEON_INT8:
        case LEON_UINT16:
        case LEON_INT16:
        case LEON_UINT32:
        case LEON_INT32:
          encode_int(out, NUM2LONG(data), SPEC_LIT_P(spec));
          break;
        case LEON_FLOAT:
        case LEON_DOUBLE:
          encode_double(out, NUM2DBL(data), SPEC_LIT_P(spec));
          break;
        case LEON_RATIONAL:
          encode_rational(out, data);
          break;
        case LEON_COMPLEX:
          encode_complex(out, data);
          break;
        case LEON_STRING:
          append_str_utf8(out, data);
          break;
        case LEON_SYMBOL:
          encode_sym(out, rb_sym2str(data));
          break;
        case LEON_BOOLEAN:
          if (!data) appendc(out, LEON_FALSE);
          else appendc(out, LEON_TRUE);
          break;
        case LEON_REGEXP:
          encode_regexp(out, data);
          break;
        case LEON_BUFFER:
          encode_buffer(out, data);
          break;
        case LEON_DATE:
        case LEON_TIME:
        case LEON_DATETIME:
          encode_date(out, data);
          break;
        case LEON_DYNAMIC:
          encode_value(out, data, type);
          break;
        default:
          appendc(out, SPEC_LIT_P(spec));
          break;
      }
      break;
  }
}

void encode_int(buffer_t *out, int64_t l, uint8_t t) {
  if (isLE) {
    switch (t) {
      case LEON_UINT8:
      case LEON_INT8:
        append(out, (void *) &l, 1);
        break;
      case LEON_UINT16:
      case LEON_INT16:
        append(out, (void *) &l, 2);
        break;
      case LEON_INT32:
      case LEON_UINT32:
        append(out, (void *) &l, 4);
    }
  } else {
    switch (t) {
      case LEON_UINT8:
      case LEON_INT8:
        append(out, (void *) &l, 1);
      case LEON_UINT16:
      case LEON_INT16:
        appendr(out, (void *) &l, 2);
      case LEON_UINT32:
      case LEON_INT32:
        appendr(out, (void *) &l, 4);
    }
  }
}

void encode_double(buffer_t *out, double d, uint8_t t) {
  float f;
  if (isLE) {
    switch (t) {
      case LEON_FLOAT:
        f = (float) d;
        append(out, (uint8_t *) &f, 4);
        break;
      case LEON_DOUBLE:
        append(out, (uint8_t *) &d, 8);
        break;
    }
  } else {
    switch (t) {
      case LEON_FLOAT:
        f = (float) d;
        appendr(out, (void *) &f, 4);
        break;
      case LEON_DOUBLE:
        append(out, (void *) &d, 8);
        break;
    }
  }
}
static inline void maybe_eof(VALUE data, size_t *i, size_t alloc) {
  if (*i + alloc > (size_t) RSTRING_LEN(data)) {
    rb_raise(rb_eArgError, "Encoded data is shorter than expected.");
  }
}

void buffer_maybe_eof(buffer_t *data, size_t *i, size_t alloc) {
  size_t offset = 0;
  while (BUFFER_IS_SLAVE_P(data)) {
    Data_Get_Struct(BUFFER_MASTER_P(data), buffer_t, data);
    offset += BUFFER_OFFSET_P(data);
  }
  if (*i + alloc + offset > BUFFER_LEN_P(data)) {
    rb_raise(rb_eArgError, "Tried to read past end of buffer.");
  }
}

size_t buffer_read_len(buffer_t *data, size_t *i) {
  uint8_t byte;
  union {
    uint16_t us;
    uint32_t ui;
  } na;
  size_t offset = 0;
  while (BUFFER_IS_SLAVE_P(data)) {
    Data_Get_Struct(BUFFER_MASTER_P(data), buffer_t, data);
    offset += BUFFER_OFFSET_P(data);
  }
  buffer_maybe_eof(data, i, 1);
  byte = ((uint8_t *) BUFFER_DATA_P(data))[*i + offset];
  if (byte < 0xfe) {
    ++(*i);
    return (size_t) byte;
  }
  if (byte == 0xfe) {
    buffer_maybe_eof(data, i, 2);
    if (((uint8_t *) BUFFER_DATA_P(data))[*i + offset + 1] == 0xff) {
      *i += 2;
      return 0xfe;
    }
    buffer_maybe_eof(data, i, 3);
    if (isLE) {
      reverse_memcpy(&na.us, BUFFER_DATA_P(data) + offset + *i + 1, 2);
      *i += 3;
      return (size_t) na.us;
    } else {
      memcpy(&na.us, BUFFER_DATA_P(data) + offset + *i + 1, 2);
      *i += 3;
      return (size_t) na.us;
    }
  } else {
    buffer_maybe_eof(data, i, 2);
    if (((uint8_t *) BUFFER_DATA_P(data))[offset + *i + 1] == 0xff) {
      *i += 2;
      return 0xff;
    }
    buffer_maybe_eof(data, i, 5);
    if (isLE) {
      reverse_memcpy(&na.ui, BUFFER_DATA_P(data) + offset + *i + 1, 4);
      *i += 5;
      return (size_t) na.ui;
    } else {
      memcpy(&na.ui, BUFFER_DATA_P(data) + offset + *i + 1, 4);
      *i += 5;
      return (size_t) na.ui;
    }
  }
}

size_t read_len(VALUE data, size_t *i) {
  uint8_t byte;
  union {
    uint16_t us;
    uint32_t ui;
  } na;
  maybe_eof(data, i, 1);
  byte = ((uint8_t *) RSTRING_PTR(data))[*i];
  if (byte < 0xfe) {
    ++(*i);
    return (size_t) byte;
  }
  if (byte == 0xfe) {
    maybe_eof(data, i, 2);
    if (((uint8_t *) RSTRING_PTR(data))[*i + 1] == 0xff) {
      *i += 2;
      return 0xfe;
    }
    maybe_eof(data, i, 3);
    if (isLE) {
      reverse_memcpy(&na.us, RSTRING_PTR(data) + *i + 1, 2);
      *i += 3;
      return (size_t) na.us;
    } else {
      memcpy(&na.us, RSTRING_PTR(data) + *i + 1, 2);
      *i += 3;
      return (size_t) na.us;
    }
  } else {
    maybe_eof(data, i, 2);
    if (((uint8_t *) RSTRING_PTR(data))[*i + 1] == 0xff) {
      *i += 2;
      return 0xff;
    }
    maybe_eof(data, i, 5);
    if (isLE) {
      reverse_memcpy(&na.ui, RSTRING_PTR(data) + *i + 1, 4);
      *i += 5;
      return (size_t) na.ui;
    } else {
      memcpy(&na.ui, RSTRING_PTR(data) + *i + 1, 4);
      *i += 5;
      return (size_t) na.ui;
    }
  }
}

static inline uint8_t read_uint8(VALUE data, size_t *i) {
  maybe_eof(data, i, 1);
  (*i)++;
  return ((uint8_t *) RSTRING_PTR(data))[*i - 1];
}

static long read_int(VALUE data, size_t *i, uint8_t type) {
  union {
    uint16_t us[2];
    int16_t s[2];
    uint32_t ui;
    int32_t i;
  } na = { .ui = 0 };
  switch (type) {
    case LEON_UINT8:
      maybe_eof(data, i, 1);
      (*i)++;
      return (long) (uint8_t) RSTRING_PTR(data)[*i - 1];
    case LEON_INT8:
      maybe_eof(data, i, 1);
      (*i)++;
      return (long) (int8_t) RSTRING_PTR(data)[*i - 1];
    case LEON_UINT16:
      maybe_eof(data, i, 2);
      (*i) += 2;
      if (isLE) return (long) *((uint16_t *) (RSTRING_PTR(data) + *i - 2));
      else {
        reverse_memcpy(na.us, &RSTRING_PTR(data)[*i - 2], 2);
        return (long) na.us[0];
      }
    case LEON_INT16:
      maybe_eof(data, i, 2);
      (*i) += 2;
      if (isLE) return (long) *((int16_t *) (RSTRING_PTR(data) + *i - 2));
      else {
        reverse_memcpy(na.s, &RSTRING_PTR(data)[*i - 2], 2);
        return (long) na.s[0];
      }
    case LEON_UINT32:
      maybe_eof(data, i, 4);
      (*i) += 4;
      if (isLE) return (long) *((uint32_t *) (RSTRING_PTR(data) + *i - 4));
      else {
        reverse_memcpy(&na.ui, &RSTRING_PTR(data)[*i - 4], 4);
        return (long) na.ui;
      }
    case LEON_INT32:
      maybe_eof(data, i, 4);
      (*i) += 4;
      if (isLE) return (long) *((int32_t *) (RSTRING_PTR(data) + *i - 4));
      else {
        reverse_memcpy(&na.i, &RSTRING_PTR(data)[*i - 4], 4);
        return (long) na.i;
      }
    default:
      rb_raise(rb_eTypeError, "Was expecting an integer.");
      return 0;
  }
}

static double read_double(VALUE data, size_t *i, uint8_t type) {
  union {
    float f[2];
    double d;
  } na = { .d = 0 };
  switch (type) {
    case LEON_FLOAT:
      maybe_eof(data, i, 4);
      (*i) += 4;
      if (isLE) return (double) *((float *) (RSTRING_PTR(data) + *i - 4));
      else {
        reverse_memcpy(na.f, &RSTRING_PTR(data)[*i - 4], 4);
        return (double) na.f[0];
      }
    case LEON_DOUBLE:
      maybe_eof(data, i, 8);
      (*i) += 8;
      if (isLE) return *((double *) (RSTRING_PTR(data) + *i - 8));
      else {
        reverse_memcpy(&na.d, &RSTRING_PTR(data)[*i - 8], 8);
        return (double) na.d;
      }
    default:
      rb_raise(rb_eTypeError, "Expected a floating-point value.");
      return 0;
  }
}

static VALUE read_complex(VALUE data, size_t *i) {
  return rb_complex_new(DBL2NUM(read_double(data, i, LEON_DOUBLE)), DBL2NUM(read_double(data, i, LEON_DOUBLE)));
}

static VALUE read_rational(VALUE data, size_t *i) {
  return rb_rational_new2(DBL2NUM(read_double(data, i, LEON_DOUBLE)), DBL2NUM(read_double(data, i, LEON_DOUBLE)));
}

VALUE read_string_utf8(VALUE data, size_t *i) {
  size_t len = read_len(data, i);
  maybe_eof(data, i, len);
  (*i) += len;
  return rb_utf8_str_new(RSTRING_PTR(data) + *i - len, len);
}

VALUE read_string_ascii(VALUE data, size_t *i) {
  VALUE out;
  size_t len = read_len(data, i);
  maybe_eof(data, i, len);
  (*i) += len;
  out = rb_str_new(RSTRING_PTR(data) + *i - len, len);
  rb_funcall(out, i_force_encoding, encASCII);
  return out;
}

VALUE read_string_utf16(VALUE data, size_t *i) {
  VALUE out;
  size_t len = read_len(data, i);
  len <<= 1;
  maybe_eof(data, i, len);
  (*i) += len;
  out = rb_str_new(RSTRING_PTR(data) + *i - len, len);
  rb_funcall(out, i_force_encoding, encUTF16);
  return out;
}

VALUE buffer_read_string_utf8(buffer_t *data, size_t *i) {
  size_t offset = 0, len = buffer_read_len(data, i);
  buffer_maybe_eof(data, i, len);
  (*i) += len;
  while (BUFFER_IS_SLAVE_P(data)) {
    offset += BUFFER_OFFSET_P(data);
    Data_Get_Struct(BUFFER_MASTER_P(data), buffer_t, data);
  }
  return rb_utf8_str_new(BUFFER_DATA_P(data) + offset + *i - len, len);
}

VALUE buffer_read_string_ascii(buffer_t *data, size_t *i) {
  VALUE out;
  size_t offset = 0, len = buffer_read_len(data, i);
  buffer_maybe_eof(data, i, len);
  (*i) += len;
  while (BUFFER_IS_SLAVE_P(data)) {
    offset += BUFFER_OFFSET_P(data);
    Data_Get_Struct(BUFFER_MASTER_P(data), buffer_t, data);
  }
  out = rb_str_new(BUFFER_DATA_P(data) + offset + *i - len, len);
  rb_funcall(out, i_force_encoding, 1, encASCII);
  return out;
}

VALUE buffer_read_string_utf16(buffer_t *data, size_t *i) {
  VALUE out;
  size_t offset = 0, len = buffer_read_len(data, i);
  len <<= 1;
  buffer_maybe_eof(data, i, len);
  (*i) += len;
  while (BUFFER_IS_SLAVE_P(data)) {
    offset += BUFFER_OFFSET_P(data);
    Data_Get_Struct(BUFFER_MASTER_P(data), buffer_t, data);
  }
  out = rb_str_new(BUFFER_DATA_P(data) + offset + *i - len, len);
  rb_funcall(out, i_force_encoding, 1, encUTF16);
  return out;
}

static VALUE read_symbol(VALUE data, size_t *i) {
  size_t len = read_len(data, i);
  char buf[len + 1];
  maybe_eof(data, i, len);
  memcpy(buf, RSTRING_PTR(data) + *i, len);
  buf[sizeof(buf) - 1] = '\0';
  (*i) += len;
  return ID2SYM(rb_intern(buf));
}

static VALUE read_date(VALUE data, size_t *i, uint8_t type) {
  VALUE out = rb_funcall(cTime, i_at, 1, LONG2NUM((long) (read_double(data, i, LEON_DOUBLE) / 1000 + 0.5)));
  switch (type) {
    case LEON_TIME:
      return out;
    case LEON_DATE:
      return rb_funcall(out, i_to_date, 0);
    case LEON_DATETIME:
      return rb_funcall(out, i_to_datetime, 0);
    default:
      rb_raise(rb_eTypeError, "Was expecting a Date, Time, or DateTime.");
      return Qnil;
  }
}

static VALUE read_buffer(VALUE data, size_t *i) {
  VALUE out;
  buffer_t *buffer_intern;
  size_t len = read_len(data, i);
  maybe_eof(data, i, len);
  out = rb_funcall(cBuffer, i_new, 0);
  Data_Get_Struct(out, buffer_t, buffer_intern);
  buffer_append(buffer_intern, RSTRING_PTR(data) + *i, len);
  *i += len;
  return out;
}

static VALUE read_regexp(VALUE data, size_t *i) {
  VALUE src = read_string_utf8(data, i);
  uint8_t flags = read_uint8(data, i);
  if (flags & 0x01) return rb_funcall(rb_cRegexp, i_new, 2, src, rb_str_new("i", 1));
  else return rb_funcall(rb_cRegexp, i_new, 1, src);
}

static VALUE read_value(VALUE data, size_t *i, uint8_t type) {
  VALUE out, key;
  size_t len, idx;
  buffer_t tmp;
  if (CUSTOM_TYPE_ACTIVE(type)) {
    if (!CUSTOM_TYPE_DECODE(type)) rb_raise(rb_eRuntimeError, "Tried to decode type %i which has no defined decode Proc.", type);
    BUFFER_LEN(tmp) = RSTRING_LEN(data);
    BUFFER_ALLOC(tmp) = RSTRING_LEN(data);
    BUFFER_IS_SLAVE(tmp) = 0;
    BUFFER_DATA(tmp) = RSTRING_PTR(data);
    key = iterator_from_native_buffer(&tmp, *i);
    out = rb_funcall(CUSTOM_TYPE_DECODE(type), i_call, 1, key);
    *i += NUM2LONG(buffer_iterator_position(key));
    return out;
  }
  switch (type) {
    case LEON_UINT8:
    case LEON_INT8:
    case LEON_UINT16:
    case LEON_INT16:
    case LEON_UINT32:
    case LEON_INT32:
      return INT2NUM(read_int(data, i, type));
    case LEON_FLOAT:
    case LEON_DOUBLE:
      return DBL2NUM(read_double(data, i, type));
    case LEON_COMPLEX:
      return read_complex(data, i);
    case LEON_RATIONAL:
      return read_rational(data, i);
    case LEON_STRING:
      return read_string_utf8(data, i);
    case LEON_SYMBOL:
      return read_symbol(data, i);
    case LEON_DATE:
    case LEON_TIME:
    case LEON_DATETIME:
      return read_date(data, i, type);
    case LEON_BUFFER:
      return read_buffer(data, i);
    case LEON_REGEXP:
      return read_regexp(data, i);
    case LEON_TRUE:
      return Qtrue;
    case LEON_FALSE:
      return Qfalse;
    case LEON_NULL:
      return Qnil;
    case LEON_UNDEFINED:
      return cUndefined;
    case LEON_NAN:
      return cNaN;
    case LEON_INFINITY:
      return cInfinity;
    case LEON_MINUS_INFINITY:
      return cMinusInfinity;
    case LEON_ARRAY:
      out = rb_ary_new();
      len = read_len(data, i);
      for (idx = 0; idx < len; ++idx) {
        rb_ary_push(out, read_value(data, i, read_uint8(data, i)));
      }
      return out;
    case LEON_OBJECT:
      out = rb_hash_new();
      len = read_len(data, i);
      for (idx = 0; idx < len; ++idx) {
        key = read_string_utf8(data, i);
        rb_hash_aset(out, key, read_value(data, i, read_uint8(data, i)));
      }
      return out;
    default:
      rb_raise(rb_eTypeError, "Invalid type constant at [%li]: %i", *i, type);
      return Qnil;
  }
}

VALUE leon_decode(VALUE data, size_t *i, spec_value_t *spec) {
  VALUE out, it;
  size_t idx;
  size_t len;
  buffer_t tmp;
  switch (SPEC_TYPE_P(spec)) {
    case TYPE_OBJECT:
      out = rb_hash_new();
      for (idx = 0; idx < SPEC_ARR_P(spec)->len; idx += 2) {
        rb_hash_aset(out, SPEC_KEY_P(SPEC_ARR_P(spec)->values[idx]), leon_decode(data, i, SPEC_ARR_P(spec)->values[idx + 1]));
      }
      return out;
    case TYPE_ARRAY:
      out = rb_ary_new();
      len = read_len(data, i);
      for (idx = 0; idx < len; ++idx) {
        rb_ary_push(out, leon_decode(data, i, SPEC_ARR_P(spec)->values[0]));
      }
      return out;
    case TYPE_MULTIARRAY:
      out = rb_ary_new();
      for (idx = 0; idx < SPEC_LEN_P(spec); ++idx) {
        rb_ary_push(out, leon_decode(data, i, SPEC_VALUES_P(spec)[idx]));
      }
      return out;
    case TYPE_LITERAL:
      if (CUSTOM_TYPE_ACTIVE(SPEC_LIT_P(spec))) {
        BUFFER_LEN(tmp) = RSTRING_LEN(data);
        BUFFER_ALLOC(tmp) = RSTRING_LEN(data);
        BUFFER_IS_SLAVE(tmp) = 0;
        BUFFER_DATA(tmp) = RSTRING_PTR(data);
        it = iterator_from_native_buffer(&tmp, *i);
        if (!CUSTOM_TYPE_DECODE(SPEC_LIT_P(spec))) rb_raise(rb_eRuntimeError, "Tried to decode type %i which has no defined decode Proc.", SPEC_LIT_P(spec));
        out = rb_funcall(CUSTOM_TYPE_DECODE(SPEC_LIT_P(spec)), i_call, 1, it);
        *i += NUM2LONG(buffer_iterator_position(it));
        return out;
      }
      switch (SPEC_LIT_P(spec)) {
        case LEON_UINT8:
        case LEON_INT8:
        case LEON_UINT16:
        case LEON_INT16:
        case LEON_UINT32:
        case LEON_INT32:
          return INT2NUM(read_int(data, i, SPEC_LIT_P(spec)));
        case LEON_FLOAT:
        case LEON_DOUBLE:
          return DBL2NUM(read_double(data, i, SPEC_LIT_P(spec)));
        case LEON_COMPLEX:
          return read_complex(data, i);
        case LEON_RATIONAL:
          return read_rational(data, i);
        case LEON_STRING:
          return read_string_utf8(data, i);
        case LEON_SYMBOL:
          return read_symbol(data, i);
        case LEON_DATE:
        case LEON_TIME:
        case LEON_DATETIME:
          return read_date(data, i, SPEC_LIT_P(spec));
        case LEON_BUFFER:
          return read_buffer(data, i);
        case LEON_REGEXP:
          return read_regexp(data, i);
        case LEON_BOOLEAN:
          if (read_uint8(data, i) == LEON_TRUE) return Qtrue;
          else return Qfalse;
        case LEON_NULL:
          return Qnil;
        case LEON_UNDEFINED:
          return cUndefined;
        case LEON_NAN:
          return cNaN;
        case LEON_INFINITY:
          return cInfinity;
        case LEON_MINUS_INFINITY:
          return cMinusInfinity;
        case LEON_DYNAMIC:
          return read_value(data, i, read_uint8(data, i));
        default:
          rb_raise(rb_eTypeError, "Invalid type constant at [%li]: %i", *i, SPEC_LIT_P(spec));
          return Qnil;
    }
    default:
      rb_raise(rb_eRuntimeError, "Template data corrupted.");
      return Qnil;
  }
}

VALUE channel_encode(VALUE self, VALUE data) {
  VALUE retval;
  buffer_t out;
  char buf[BUFFER_DEFAULT_ALLOC];
  spec_value_t *spec_intern;
  BUFFER_IS_SLAVE(out) = 0;
  BUFFER_ALLOC(out) = BUFFER_DEFAULT_ALLOC;
  BUFFER_LEN(out) = 0;
  BUFFER_DATA(out) = buf;
  BUFFER_ON_HEAP(out) = 0;
  Data_Get_Struct(self, spec_value_t, spec_intern);
  leon_encode(&out, data, spec_intern);
  retval = rb_str_new(BUFFER_DATA(out), BUFFER_LEN(out));
  if (BUFFER_ON_HEAP(out)) free(BUFFER_DATA(out));
  return retval;
}

VALUE channel_decode(VALUE self, VALUE data) {
  size_t i;
  spec_value_t *spec_intern;
  Check_Type(data, T_STRING);
  Data_Get_Struct(self, spec_value_t, spec_intern);
  i = 0;
  return leon_decode(data, &i, spec_intern);
}

void spec_to_str(buffer_t *out, spec_value_t *spec) {
  size_t idx;
  uint8_t key_t;
  VALUE as_str;
  switch (SPEC_TYPE_P(spec)) {
    case TYPE_LITERAL:
      appendc(out, SPEC_TYPE_P(spec));
      appendc(out, SPEC_LIT_P(spec));
      break;
    case TYPE_ARRAY:
      appendc(out, SPEC_TYPE_P(spec));
      spec_to_str(out, SPEC_VALUES_P(spec)[0]);
      break;
    case TYPE_MULTIARRAY:
    case TYPE_OBJECT:
      appendc(out, SPEC_TYPE_P(spec));
      encode_len(out, SPEC_LEN_P(spec));
      for (idx = 0; idx < SPEC_LEN_P(spec); ++idx) {
        spec_to_str(out, SPEC_VALUES_P(spec)[idx]);
      }
      break;
    case TYPE_KEY:
      switch (TYPE(SPEC_KEY_P(spec))) {
        case T_STRING:
          key_t = 0;
          as_str = SPEC_KEY_P(spec);
          break;
        case T_SYMBOL:
          key_t = 1;
          as_str = rb_sym2str(SPEC_KEY_P(spec));
          break;
        default:
          rb_raise(rb_eRuntimeError, "Template data corrupted.");
          return;
      }
      appendc(out, key_t);
      encode_len(out, RSTRING_LEN(as_str));
      append(out, RSTRING_PTR(as_str), RSTRING_LEN(as_str));
      break;
    default:
      rb_raise(rb_eRuntimeError, "Template data corrupted.");
      break;
  }
}

void str_to_spec(spec_value_t *spec, VALUE str, size_t *i, uint8_t type) {
  spec_value_t *el;
  size_t len, idx;
  VALUE key;
  switch (type) {
    case TYPE_LITERAL:
      SPEC_TYPE_P(spec) = TYPE_LITERAL;
      SPEC_LIT_P(spec) = read_uint8(str, i);
      break;
    case TYPE_ARRAY:
      SPEC_TYPE_P(spec) = TYPE_ARRAY;
      SPEC_ARR_P(spec) = type_array_new_single_alloc();
      el = spec_value_new();
      str_to_spec(el, str, i, read_uint8(str, i));
      SPEC_VALUES_P(spec)[0] = el;
      break;
    case TYPE_MULTIARRAY:
      SPEC_TYPE_P(spec) = TYPE_MULTIARRAY;
      SPEC_ARR_P(spec) = type_array_new();
      len = read_len(str, i);
      for (idx = 0; idx < len; ++idx) {
        el = spec_value_new();
        str_to_spec(el, str, i, read_uint8(str, i));
        type_array_push(SPEC_ARR_P(spec), el);
      }
      break;
    case TYPE_OBJECT:
      SPEC_TYPE_P(spec) = TYPE_OBJECT;
      SPEC_ARR_P(spec) = type_array_new();
      len = read_len(str, i);
      for (idx = 0; idx < len; idx += 2) {
        el = spec_value_new();
        str_to_spec(el, str, i, TYPE_KEY);
        SPEC_VALUES_P(spec)[SPEC_LEN_P(spec)] = el;
        ++SPEC_LEN_P(spec);
        el = spec_value_new();
        str_to_spec(el, str, i, read_uint8(str, i));
        SPEC_VALUES_P(spec)[SPEC_LEN_P(spec)] = el;
        ++SPEC_LEN_P(spec);
      }
      break;
    case TYPE_KEY:
      SPEC_TYPE_P(spec) = TYPE_KEY;
      switch (read_uint8(str, i)) {
        case 0:
          key = read_string_utf8(str, i);
          rb_gc_mark(key);
          SPEC_KEY_P(spec) = key;
          break;
        case 1:
          key = read_string_utf8(str, i);
          key = ID2SYM(rb_intern(RSTRING_PTR(key)));
          rb_gc_mark(key);
          SPEC_KEY_P(spec) = key;
          break;
        default:
          rb_raise(rb_eArgError, "Invalid serialization of LEON::Channel object.");
          return;
      }
      break;
  }
}     

VALUE channel_dump(VALUE self, VALUE depth) {
  spec_value_t *spec_intern;
  buffer_t out;
  char buf[BUFFER_DEFAULT_ALLOC];
  BUFFER_IS_SLAVE(out) = 0;
  BUFFER_ALLOC(out) = BUFFER_DEFAULT_ALLOC;
  BUFFER_DATA(out) = buf;
  BUFFER_LEN(out) = 0;
  Data_Get_Struct(self, spec_value_t, spec_intern);
  spec_to_str(&out, spec_intern);
  return rb_str_new(BUFFER_DATA(out), BUFFER_LEN(out));
}

VALUE channel_load(VALUE self, VALUE serialized) {
  spec_value_t *spec_intern;
  size_t i;
  VALUE out = rb_funcall(cChannel, i_new, 1, INT2NUM(LEON_UINT8));
  Data_Get_Struct(out, spec_value_t, spec_intern);
  i = 0;
  str_to_spec(spec_intern, serialized, &i, read_uint8(serialized, &i));
  return out;
}

void free_channel(void *spec) {
  spec_value_free((spec_value_t *) spec);
}
