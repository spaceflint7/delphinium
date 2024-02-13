
// ------------------------------------------------------------
//
// js_newbig
//
// ------------------------------------------------------------

js_val js_newbig (js_environ *env, int len, uint32_t *ptr) {

    uint32_t *big = js_big_alloc(env, len);
    uint32_t *ptr2 = big + 1;
    while (len--)
        *ptr2++ = *ptr++;
    js_big_trim(big, ptr2);
    return js_make_primitive_bigint(big); // not gc managed
}

// ------------------------------------------------------------
//
// js_big_from_num
//
// ------------------------------------------------------------

static js_val js_big_from_num (js_environ *env, js_val input) {

    uint32_t *big = NULL;

    if (js_is_number(input) &&
            /* is_finite_and_not_nan */
                    (~input.raw & js_exponent_mask)) {

        double dbl_value = input.num;

        int32_t int32_value = (int64_t)dbl_value;
        if ((double)int32_value == dbl_value) {

            // input fits in a 32-bit integer,
            // convert to a one-word bigint

            big = js_malloc(sizeof(uint32_t[1 + 1]));
            big[0] = 1;
            big[1] = int32_value;

        } else {

            // input fits in a 64-bit integer
            // convert to a two-word bigint

            int64_t int64_value = (int64_t)dbl_value;
            if ((double)int64_value == dbl_value) {

                big = js_malloc(sizeof(uint32_t[2 + 1]));
                *(int64_t *)&big[1] = int64_value;
                js_big_trim(big, big + 1 + 2);

            } else {

                // input does not fit in an integer,
                // so convert the input to a string,
                // and convert the string to a bigint

                char *buf = env->num_string_buffer;
                int len =
                    sprintf(buf, "%.100g", input.num);

                char *ptr = buf + len;
                while (ptr > buf && *--ptr != '.')
                    ;

                if (*ptr != '.') {
                    js_val str_value =
                            js_str_c2(env, buf, len);
                    js_val big_value =
                            js_big_from_str(env, str_value);
                    return big_value;
                }
            }
        }
    }

    return (big ? js_make_primitive_bigint_gc(big)
                : js_undefined);
}

// ------------------------------------------------------------
//
// js_big_from_str
//
// ------------------------------------------------------------

static js_val js_big_from_str (js_environ *env, js_val input) {

    // convert a string to a bigint.  if starts with 0x,
    // 0o, 0b, parse as hex, octal or binary, resepctively.
    // otherwise may start with a minus sign.

    const objset_id *id = js_get_pointer(input);
    int len = id->len >> 1; // byte length to char count

    // skip leading whitespace
    const wchar_t *src = id->data;
    while (len && js_str_is_white_space(*src)) {
        len--;
        src++;
    }

    if (!len)
        return env->big_zero;

    //
    // parse base prefix, or minus prefix
    //

    int base = 10;
    int64_t right_multiplier = 1;
    if (*src == '0') {
        switch (src[1]) {
            case 'b': case 'B': base = 2;  break;
            case 'o': case 'O': base = 8;  break;
            case 'x': case 'X': base = 16; break;
        }
        if (base != 10) {
            src += 2;
            len -= 2;
        }

    } else if (*src == '-') {
        src++;
        len--;
        right_multiplier = -1;
    }

    //
    //
    //

    js_val output = env->big_zero;
    js_val big_base =
            js_big_from_num(env, js_make_number(base));
    bool ok = false;

    while (len) {

        char digit = *src++;
        len--;
        if (digit >= '0' && digit <= '9')
            digit -= '0';
        else if (digit >= 'A' && digit <= 'Z')
            digit -= 'A';
        else if (digit >= 'a' && digit <= 'z')
            digit -= 'a';

        else if (js_str_is_white_space(digit)) {
            // skip trailing whitespace, but fail
            // if anything follows the whitespace
            for (;;) {
                if (!len)
                    break;
                if (!js_str_is_white_space(*src)) {
                    ok = false;
                    break;
                }
                src++;
                len--;
            }
            break;

        } else {
            // fail because unrecognized character
            ok = false;
            break;
        }
        if (digit >= base) {
            // fail because digit too large for base
            ok = false;
            break;
        }

        // insert next digit into the result
        output = js_big_multiply(env, output, big_base);
        output = js_big_add_sub(env, output,
                        js_big_from_num(
                            env, js_make_number(digit)),
                        right_multiplier);
        ok = true;
    }

    if (!ok) {
        output = js_undefined;

        // don't let the compiler optimize away the reference
        // to the 'input' parameter, to prevent gc freeing it
        // while we are still accessing the input text buffer
        volatile js_val dont_discard_input = input;
        (void)dont_discard_input;
    }

    return output;
}

// ------------------------------------------------------------
//
// js_big_check_truthy
//
// ------------------------------------------------------------

static bool js_big_check_truthy (js_val value) {

    uint32_t *big = js_get_pointer(value);
    const uint32_t big_0_ = js_big_length(big);
    bool is_zero = (big_0_ == 1) && (big[1] == 0);
    return !is_zero;
}

// ------------------------------------------------------------
//
// js_big_compare_num
//
// ------------------------------------------------------------

static int js_big_compare_num (js_environ *env,
                               js_val big, double num) {

    if (num == INFINITY) {
        // if number is positive infinity, then any
        // finite bigint number must always be smaller
        return -1;
    }
    if (num == -INFINITY) {
        // if number is negative infinity, then any
        // finite bigint number must always be greater
        return 1;
    }

    double integer_part;
    double fraction_part = modf(num, &integer_part);

    js_val big2 = js_big_from_num(
                    env, js_make_number(integer_part));
    int cmp = js_big_compare(big, big2);
    if (cmp == 0 && fraction_part != 0.0) {
        // integer part is equal to bigint, but number
        // also has a fraction, which must be considered
        cmp = (fraction_part < 0.0) ? 1 : -1;
    }

    return cmp;
}

// ------------------------------------------------------------
//
// js_big_tostring
//
// ------------------------------------------------------------

static js_val js_big_tostring (
                    js_environ *env, js_val val, int radix) {

    // if the input value can fit in the temporary tostring
    // buffer allocated by js_num_init (), then use that,
    // otherwise allocate a larger buffer

    uint32_t *ptr = js_get_pointer(val);
    uint32_t  len = js_big_length(ptr);

    // rough estimate of log(2**32) / log(radix)
    uint32_t buf_size = num_string_buffer_size - 4;
    const uint32_t chars_per_word =
            (radix >= 16) ? 8 : ((radix >= 8) ? 11 : 32);
    const uint32_t words_in_buffer =
                        buf_size / chars_per_word;

    char *ch0, *buf_to_free;
    if (len <= words_in_buffer) {
        // input value will fit in the default buffer
        ch0 = env->num_string_buffer;
        buf_to_free = NULL;
    } else {
        // input value is too long to fit in the
        // default buffer and requires a dedicated one
        buf_size = (len + 1) * chars_per_word;
        ch0 = js_malloc(buf_size + 4);
        buf_to_free = ch0;
    }

    // if input value is negative, insert a minus sign
    char *chL = ch0;
    if ((int32_t)ptr[len] >> 31) {
        *chL++ = '-';
        // negate the input because division is unsigned
        ptr = js_big_negate2(env, 1ULL, ptr, len);
        val = js_make_primitive_bigint_gc(ptr);
    }

    char *chR = ch0 + buf_size;
    *--chR = 0;

    for (;;) {
        // divide value by radix, and use the remainder
        // to index into the table of base-36 digits
        uint32_t rem32;
        val = js_big_divide_by_integer(
                            env, val, radix, &rem32);
        *--chR = js_num_digits[rem32];

        // stop looping when the value becomes zero
        ptr = js_get_pointer(val);
        const uint32_t ptr_0_ = js_big_length(ptr);
        if ((ptr_0_ == 1) && (ptr[1] == 0))
            break;
    }

    // copy the digits to the start of the buffer
    while (*chR)
        *chL++ = *chR++;

    js_val ret_val = js_str_c2(env, ch0, chL - ch0);
    js_free(buf_to_free);
    return ret_val;
}

// ------------------------------------------------------------
//
// js_big_binary_op
//
// ------------------------------------------------------------

static js_val js_big_binary_op (js_environ *env, int op,
                                js_val left, js_val right) {

    js_val remainder = { .raw = 0 };
    switch (op) {

        case '+':
            return js_big_add_sub(env, left, right, 1);

        case '-':
            return js_big_add_sub(env, left, right, -1);

        case '*':
            return js_big_multiply(env, left, right);

        case '/':
            return js_big_divide_with_remainder(
                        env, left, right, NULL);

        case '%':
            js_big_divide_with_remainder(
                        env, left, right, &remainder);
            return remainder;

        case 0x2A2A:    // power **
            return js_big_power(env, left, right);

        case '|':
        case '&':
        case '^':
            return js_big_bitwise_op(
                        env, op, left, right);

        case 0x3C3C:    // '<<'
            return js_big_shift(env, left, right, 1);

        case 0x3E3E:    // '>>'
            return js_big_shift(env, left, right, -1);

      //case 0x3E33:    // '>3' which stands for '>>>'
        default:
            js_callthrow("TypeError_unsupported_operation");
            return js_undefined;
    }
}

// ------------------------------------------------------------
//
// js_big_truncate
//
// ------------------------------------------------------------

static js_val js_big_truncate (js_environ *env, js_val input,
                               uint64_t bits, bool add_zero) {

    uint32_t *input_ptr = js_get_pointer(input);
    uint32_t input_len = js_big_length(input_ptr);

    uint64_t output_len = (bits + 31) >> 5;
    if (output_len > input_len)
        output_len = input_len;
    // allocate an extra word for zero sign, if unsigned
    uint32_t *output_big =
        js_big_alloc(env, output_len + (add_zero ? 1 : 0));
    uint32_t *output_ptr = output_big;

    while (output_len--)
        *++output_ptr = *++input_ptr;

    if (add_zero) {
        // zero-extend into high bits of last word
        bits = 32 - bits;
        uint32_t last_word = *output_ptr << bits;
        *output_ptr = last_word >> bits;
        // unsigned, add extra zero sign word
        *++output_ptr = 0;
    } else {
        // sign-extend into high bits of last word
        bits = 32 - bits;
        int32_t last_word = *(int32_t *)output_ptr << bits;
        *output_ptr = last_word >> bits;
    }

    js_big_trim(output_big, output_ptr + 1);
    return js_make_primitive_bigint_gc(output_big);
}

// ------------------------------------------------------------
//
// js_big_util
//
// ------------------------------------------------------------

static js_val js_big_util (js_c_func_args) {

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

        if (js_is_object(input))
            input = js_obj_to_primitive_number(env, input);

        if (js_is_boolean(input)) {

            // conversion: true -> 1.0, false -> 0.0
            input = js_make_number(input.raw & 1);
        }

        if (js_is_number(input)) {
            ret_val = js_big_from_num(env, input);
            if (js_is_undefined(ret_val))
                js_callthrow("RangeError_invalid_argument");

        } else if (js_is_primitive_string(input)) {
            ret_val = js_big_from_str(env, input);
            if (js_is_undefined(ret_val))
                js_callthrow("SyntaxError_invalid_argument");

        } else if (js_is_primitive_bigint(input))
            ret_val = input;

        if (!js_is_primitive_bigint(ret_val))
            js_callthrow("TypeError_invalid_argument");

        js_return(ret_val);
    }

    if (!js_is_primitive_bigint(input))
        js_callthrow("TypeError_expected_bigint");

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
        ret_val = js_big_tostring(env, input, radix);

    // second parameter is 'I' for asIntN, 'U' for asUintN
    } else if (which.num == /* 0x55 */ (double)'U'
           ||  which.num == /* 0x49 */ (double)'I') {

        js_val val_bits = js_tonumber(env, extra);
        uint64_t bits = (uint64_t)val_bits.num;
        if (bits < 0 || bits > MAX_SAFE_INTEGER)
            js_callthrow("TypeError_invalid_argument");

        ret_val = js_big_truncate(env, input, bits,
                    which.num == /* 0x55 */ (double)'U');
    }

    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_big_unary_negate
//
// ------------------------------------------------------------

static js_val js_big_unary_negate (js_environ *env,
                                   js_val input,
            uint32_t zero_for_negate_or_one_for_bitwise_not) {

    uint64_t carry = zero_for_negate_or_one_for_bitwise_not;
    uint32_t *ptr = js_get_pointer(input);
    uint32_t  len = js_big_length(ptr);
    uint32_t *output = js_big_negate2(env, carry, ptr, len);
    return js_make_primitive_bigint_gc(output);
}

// ------------------------------------------------------------
//
// js_big_init
//
// ------------------------------------------------------------

static void js_big_init (js_environ *env) {

    js_val obj;

    // prototype for bigint primitive
    obj = js_emptyobj(env);
    env->big_proto = (void *)((uintptr_t)js_get_pointer(obj));

    // shadow.js_big_util function
    js_newprop(env, env->shadow_obj,
        js_str_c(env, "js_big_util")) =
            js_unnamed_func(js_big_util, 3);

    // bigint zero
    env->big_zero = js_big_from_num(env, js_make_number(0.0));
}
