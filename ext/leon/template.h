#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <channel.h>

VALUE to_template(VALUE, VALUE);
VALUE spec_value_to_sorted_hash(spec_value_t *);
VALUE to_sorted_template(VALUE, VALUE);
VALUE type_gcd(VALUE);
uint8_t pluck(VALUE, VALUE, VALUE *);

#endif
