
// ------------------------------------------------------------

#define num_string_buffer_size 384

static char *js_num_digits =
                    "0123456789abcdefghijklmnopqrstuvwxyz?";

static uint64_t MAX_SAFE_INTEGER = 9007199254740991ULL;

// ------------------------------------------------------------
//
// js_tonumber_integer
//
// ------------------------------------------------------------

static js_val js_tonumber_integer (const char *ptr) {

    int base;
    switch (*ptr) {
        case 'b': case 'B': base = 2; break;
        case 'o': case 'O': base = 8; break;
        case 'x': case 'X': base = 16; break;
        case '\0': return js_make_number(0);
        default:  return js_nan;
    }
    char *stop;
    uint64_t val = strtoull(++ptr, &stop, base);
    while (js_str_is_white_space(*stop))
        stop++;
    if (*stop != '\0')
        return js_nan;
    return js_make_number(val);
}

// ------------------------------------------------------------
//
// js_tonumber_decimal
//
// ------------------------------------------------------------

static js_val js_tonumber_decimal (const char *ptr) {

    js_val val;
    bool neg = false;
    if (*ptr == '+')
        ptr++;
    else if (*ptr == '-') {
        ptr++;
        neg = true;
    }

    if (strcmp(ptr, "Infinity") == 0)
        val.num = INFINITY;

    else if (*ptr >= '0' && *ptr <= '9') {
        char *stop;
        val.num = strtod(ptr, &stop);
        while (js_str_is_white_space(*stop))
            stop++;
        if (*stop != '\0')
            return js_nan;
    } else
        return js_nan;

    if (neg)
        val.raw |= 1ULL << 63;

    return val;
}

// ------------------------------------------------------------
//
// js_tonumber
//
// ------------------------------------------------------------

static js_val js_tonumber (js_environ *env, js_val val) {

    if (js_is_object(val)) {
        // note there is no 'else' at the end of this
        // condition, because we want the rest of the
        // function to look at this new primitive value
        val = js_obj_to_primitive_number(env, val);
    }

    if (js_is_number(val))
        return val;

    if (js_is_undefined(val))
        return js_nan;

    if (val.raw == js_null.raw || val.raw == js_false.raw)
        return js_make_number(0.0);

    if (val.raw == js_true.raw)
        return js_make_number(1.0);

    if (!js_is_primitive_string(val)) {
        // we should only get here for a symbol or bigint,
        // but this is a catch-all for any unexpected value
        js_callthrow("TypeError_expected_number");
    }

    //
    // convert a string to a number.  if starts with 0x,
    // 0o, 0b, parse as hex, octal or binary, resepctively.
    // if starts with 0., parse as number with fraction.
    // otherwise parse as a decimal integer.
    //

    const objset_id *id = js_get_pointer(val);
    int len = id->len >> 1; // byte length to char count

    // skip leading whitespace
    const wchar_t *src = id->data;
    while (len && js_str_is_white_space(*src)) {
        len--;
        src++;
    }

    if (!len)
        return js_nan;

    // copy js string into null-terminated ascii buffer
    char *str = env->num_string_buffer;
    char *dst = str;
    if (len >= num_string_buffer_size - 4)
        len  = num_string_buffer_size - 4;
    while (len--)
        *dst++ = *src++;
    *dst = '\0';

    // parse ascii string as a number in one of two ways
    if (str[0] == '0') {
        const char ch = str[1];
        const bool is_decimal_point_or_digit =
                (ch == '.' || (ch >= '0' && ch <= '9'));
        if (!is_decimal_point_or_digit)
            return js_tonumber_integer(str + 1);
    }
    return js_tonumber_decimal(str);
}

// ------------------------------------------------------------
//
// js_num_edit_exponent
//
// ------------------------------------------------------------

static char *js_num_edit_exponent (char *dst, char *src) {

    // overwrite 'e' just past the last non-zero digit,
    // for example:  6.000000000000000e-07  -->  6e...
    // then append the exponent digits, making sure to
    // skip leading zero digits in the exponent,
    // for example:  6.000000000000000e-07  -->  6e-7

    *dst++ = *src++;    // copy 'e'
    *dst++ = *src++;    // copy '+' or '-'

    while (*src == '0') // skip leading zeroes
        src++;

    char src_ch = *src;
    if (src_ch == '\0') // zero exponent?
        *dst++ = '0';

    else {
        do {
            *dst++ = src_ch;
            src_ch = *++src;
        } while (src_ch != '\0');
    }

    return dst;
}

// ------------------------------------------------------------
//
// js_num_tostring_0
//
// ------------------------------------------------------------

static char *js_num_tostring_0 (
                    char *ch, int radix, uint64_t input) {

    // the integer digits are extracted in reverse order,
    // so write them starting at the end of the buffer,
    // working backwards, so they end up in the correct
    // order, aligned at the far end of the buffer

    char *ch2 = ch + num_string_buffer_size - 4;
    *--ch2 = 0;

    for (;;) {
        *--ch2 = js_num_digits[(int)(input % radix)];
        input /= radix;
        if (input == 0)
            break;
    }

    // copy the digits to the start of the buffer
    while (*ch2)
        *ch++ = *ch2++;

    return ch;
}

// ------------------------------------------------------------
//
// js_num_tostring_1
//
// ------------------------------------------------------------

static char *js_num_tostring_1 (
                    char *ch, double radix, double input) {

    char *ch2 = ch + num_string_buffer_size - 4;

    // split input into integral and fractional parts
    double integ;
    double fract = modf(input, &integ);

    // the integer digits are extracted in reverse order,
    // so write them starting at the end of the buffer,
    // working backwards, so they end up in the correct
    // order, aligned at the far end of the buffer
    *--ch2 = 0;
    for (;;) {
        const double tmp = integ / radix;
        const int digit = integ - trunc(tmp) * radix;
        *--ch2 = js_num_digits[digit];
        if (tmp < 1.0)
            break;
        integ = tmp;
    }
    // copy the digits to the start of the buffer
    while (*ch2)
        *ch++ = *ch2++;

    // calculate the stop factor, more or less like v8
    double delta = nextafter(input, input + 1.0);
    delta = 0.5 * (delta - input);
    delta = fmax(FLT_MIN, delta);

    if (fract >= delta) {
        *ch++ = '.';
        for (;;) {
            fract *= radix;
            const double digit = trunc(fract);
            fract -= digit;
            *ch++ = js_num_digits[(int)digit];
            delta *= radix;
            if (fract < delta)
                break;
        }
    }

    return ch;
}

// ------------------------------------------------------------
//
// js_num_tostring_2
//
// ------------------------------------------------------------

static char *js_num_tostring_2 (char *ch, double input) {

    // special case for infinity
    if (input == INFINITY) {
        strcpy(ch, "Infinity");
        return ch + 8;
    }

    // the best way to approximate the output from
    // v8 is to do exponential-format printf with
    // 15 digits of precision, then edit the output
    // string to discard extra zeroes
    sprintf(ch, "%.15e", input);

    // ch points at the last non-zero digit found,
    // while ch2 advances forward to look for 'e'

    char *ch2 = ++ch;
    if (*ch2 == '.')
        ch2++;

    for (;;) {
        if (*ch2 == 'e')
            break;
        if (*ch2++ != '0')
            ch = ch2;
    }

    return js_num_edit_exponent(ch, ch2);
}

// ------------------------------------------------------------
//
// js_num_tostring
//
// ------------------------------------------------------------

static js_val js_num_tostring (
                    js_environ *env, js_val val, int radix) {

    // if a number is not equal to itself, it is a NaN
    if (val.num != val.num)
        return env->str_nan;

    char *buf = env->num_string_buffer;
    double num = val.num;
    if (val.raw & (1ULL << 63)) {
        num = -num;
        *buf++ = '-';
    }

    char *end;
    if ((uint64_t)num == num) {
        end = js_num_tostring_0(
                    buf, (int)radix, (uint64_t)num);
    } else {
        double scale = (radix != 10) ? 0 : log10(num);
        if (scale >= -6.0 && scale <= 21.0)
            end = js_num_tostring_1(buf, radix, num);
        else
            end = js_num_tostring_2(buf, num);
    }

    buf = env->num_string_buffer;
    return js_str_c2(env, buf, end - buf);
}

// ------------------------------------------------------------
//
// js_num_binary_op
//
// ------------------------------------------------------------

static js_val js_num_binary_op (js_environ *env, int op,
                                js_val left, js_val right) {

    switch (op) {

        case '+':
            return js_make_number(left.num + right.num);

        case '-':
            return js_make_number(left.num - right.num);

        case '*':
            return js_make_number(left.num * right.num);

        case '/':
            return js_make_number(left.num / right.num);

        case '%':
            return js_make_number(copysign(
                        remainder(left.num, right.num),
                                            left.num));

        case 0x2A2A:    // power **
            return js_make_number(pow(left.num, right.num));

        case '|':
            return js_make_number(
                (uint32_t)left.num | (uint32_t)right.num);

        case '&':
            return js_make_number(
                (uint32_t)left.num & (uint32_t)right.num);

        case '^':
            return js_make_number(
                (uint32_t)left.num ^ (uint32_t)right.num);

        case 0x3C3C:    // '<<'
            return js_make_number((int32_t)left.num <<
                            (31 & (uint32_t)right.num));

        case 0x3E3E:    // '>>'
            return js_make_number((int32_t)left.num >>
                            (31 & (uint32_t)right.num));

        case 0x3E33:    // '>3' which stands for '>>>'
            return js_make_number((uint32_t)left.num >>
                            (31 & (uint32_t)right.num));

        default:
            js_callthrow("TypeError_unsupported_operation");
            return js_undefined;
    }
}

// ------------------------------------------------------------
//
// js_num_util
//
// ------------------------------------------------------------

static js_val js_num_util (js_c_func_args) {

    js_val input   = js_undefined;
    js_val which   = js_undefined;
    js_val extra   = js_undefined;
    js_val ret_val = js_undefined;

    js_link *arg_ptr = stk_args;
    if ((arg_ptr = arg_ptr->next) != js_stk_top) {
        input = arg_ptr->value;
        if ((arg_ptr = arg_ptr->next) != js_stk_top) {
            which = arg_ptr->value;
            if ((arg_ptr = arg_ptr->next) != js_stk_top)
                extra = arg_ptr->value;
        }
    }

    // second parameter is 'C' for Constructor
    if (which.num == /* 0x43 */ (double)'C') {

        ret_val = js_tonumber(env, input);
        js_return(ret_val);
    }

    if (!js_is_number(input))
        js_callthrow("TypeError_expected_number");

    if (!js_is_undefined(extra)) {
        if (!js_is_number(extra))
            extra = js_tonumber(env, extra);
        if (!js_is_number(extra))
            js_callthrow("TypeError_invalid_argument");
    }

    // second parameter is 'S' for toString
    if (which.num == /* 0x53 */ (double)'S') {

        int radix;
        if (js_is_undefined(extra))
            radix = 10;
        else {
            radix = (int)extra.num;
            if (radix < 2 || radix > 36)
                js_callthrow("RangeError_invalid_argument");
        }
        ret_val = js_num_tostring(env, input, radix);

    } else if (/* is_infinity_or_nan */
                    !(~(input.raw) & js_exponent_mask)) {
        // if the input is not finite, then toExponential,
        // toFixed, and toPrecision, call toString before
        // validating the 'digits' parameter.  this is in
        // contrast to the description in the standard,
        // but this is how Node and SpiderMonkey behave.
        ret_val = js_num_tostring(env, input, 10);

    } else {

        // get the number of digits
        int digits;
        if (js_is_undefined(extra))
            digits = 0;
        else {
            digits = (int)extra.num;
            const int min_digits =
                    (which.num == (double)'P') ? 1 : 0;
            if (digits < min_digits || digits > 100)
                js_callthrow("RangeError_invalid_argument");
        }

        char *buf = env->num_string_buffer;
        int len = 0;

        if (which.num == /* 0x46 */ (double)'F') {
            // toFixed
            len = sprintf(buf, "%.*f", digits, input.num);

        } else {
            // toExponential and toPrecision also require
            // editing the exponent in the output string

            if (which.num == /* 0x50 */ (double)'P') {
                // toPrecision
                len = sprintf(buf, "%#.*g", digits, input.num);

            } else if (which.num == /* 0x45 */ (double)'E') {
                // toExponential
                len = sprintf(buf, "%.*e", digits, input.num);
            }

            if (len) {
                char *ptr = strchr(buf, 'e');
                if (ptr)
                    len = js_num_edit_exponent(ptr, ptr)
                        - buf;
            }
        }

        if (len)
            ret_val = js_str_c2(env, buf, len);
    }

    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_num_init
//
// ------------------------------------------------------------

static void js_num_init (js_environ *env) {

    js_val obj;

    // prototype for number primitive
    obj = js_emptyobj(env);
    env->num_proto = (void *)((uintptr_t)js_get_pointer(obj));

    // prototype for boolean primitive
    obj = js_emptyobj(env);
    env->bool_proto = (void *)((uintptr_t)js_get_pointer(obj));

    // ascii buffer work area for string conversion functions
    env->num_string_buffer = js_malloc(num_string_buffer_size);

    // shadow.js_num_util function
    js_newprop(env, env->shadow_obj,
            js_str_c(env, "js_num_util")) =
                js_unnamed_func(js_num_util, 3);
}
