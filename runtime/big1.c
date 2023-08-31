
// ------------------------------------------------------------
//
// js_big_alloc
//
// ------------------------------------------------------------

static uint32_t *js_big_alloc(js_environ *env, uint32_t len) {

    if (len >= (1 << 24))
        js_callthrow("RangeError_bigint_too_large");
    return js_malloc((len + 1) * sizeof(uint32_t));
}

// ------------------------------------------------------------
//
// js_big_trim
//
// ------------------------------------------------------------

static void js_big_trim (uint32_t *big, uint32_t *endptr) {

    // discard high words from a bigint as long as it does
    // not change the sign of the value.  this is done by
    // discarding a high word if it is all 0 or all 1 bits,
    // but only if same as highest bit of preceding word.
    uint32_t len = endptr - (big + 1);
    uint32_t w = big[len];
    uint32_t sign = ((int32_t)w) >> 31;
    while (len > 1 && w == sign) {
        w = big[len - 1];
        if (((int32_t)w) >> 31 != sign)
            break;
        --len;
    }
    *big = len;
}

// ------------------------------------------------------------
//
// js_big_negate1
//
// ------------------------------------------------------------

static void js_big_negate1 (uint32_t *ptr, uint32_t *endptr) {

    // negate in place by flipping bits and adding 1.
    uint64_t carry = 1;
    while (++ptr != endptr) {
        carry += (uint32_t)~*ptr;
        *ptr = (uint32_t)carry;
        carry >>= 32;
    }
}

// ------------------------------------------------------------
//
// js_big_negate2
//
// ------------------------------------------------------------

static uint32_t *js_big_negate2 (
                            js_environ *env, uint64_t carry,
                            uint32_t *ptr, uint32_t len) {

    // stores the flipped bits of a source bigint into
    // a destination bigint, allocated with same size.
    // pass 0 in carry to just flip bits, a bitwise NOT.
    // pass 1 in carry to do two's complement negation.

    //uint64_t carry = 1;
    uint32_t *newbig = js_big_alloc(env, len);
    uint32_t *ptr2 = newbig;
    *ptr2 = len;
    while (len--) {
        carry += (uint32_t)~*++ptr;
        *++ptr2 = (uint32_t)carry;
        carry >>= 32;
    }
    return newbig;
}

// ------------------------------------------------------------
//
// js_big_compare
//
// ------------------------------------------------------------

static int js_big_compare (js_val left, js_val right) {

    //
    // compare the number of words in each bigint, excluding
    // any leading zero words.  the longer bigint is larger.
    // if same number of words, compare each 32-bit word.
    //

    uint32_t *big1 = js_get_pointer(left);
    uint32_t len1 = *big1;

    uint32_t *big2 = js_get_pointer(right);
    uint32_t len2 = *big2;

    int32_t cmp = len1 - len2;
    while (cmp == 0 && len1 != 0) {
        cmp = big1[len1] - big2[len1];
        --len1;
    }
    return ((int32_t)cmp) >> 31;
}

// ------------------------------------------------------------
//
// js_big_bitwise_op
//
// ------------------------------------------------------------

static js_val js_big_bitwise_op (
                            js_environ *env, int operator,
                            js_val left, js_val right) {

    uint32_t *big1 = js_get_pointer(left);
    uint32_t len1 = *big1;

    uint32_t *big2 = js_get_pointer(right);
    uint32_t len2 = *big2;

    if (len1 > len2) {
        uint32_t *swap_ptr = big1;
        big1 = big2;
        big2 = swap_ptr;
        uint32_t swap_len = len1;
        len1 = len2;
        len2 = swap_len;
    }

    uint32_t sign1 = (int32_t)big1[len1] >> 31;

    uint32_t *big3 = js_big_alloc(env, len2);
    uint32_t *ptr3 = big3;
    *ptr3 = len2;
    len2 -= len1;

    if (operator == '|') {

        while (len1--)
            *++ptr3 = (*++big2) | (*++big1);
        while (len2--)
            *++ptr3 = (*++big2) | sign1;

    } else if (operator == '&') {

        while (len1--)
            *++ptr3 = (*++big2) & (*++big1);
        while (len2--)
            *++ptr3 = (*++big2) & sign1;

    } else if (operator == '^') {

        while (len1--)
            *++ptr3 = (*++big2) ^ (*++big1);
        while (len2--)
            *++ptr3 = (*++big2) ^ sign1;
    }

    return js_make_primitive(big3, js_prim_is_bigint);
}

// ------------------------------------------------------------
//
// js_big_shift_left
//
// ------------------------------------------------------------

static uint32_t *js_big_shift_left (
                    js_environ *env, int32_t shift_count,
                    uint32_t *input_ptr, int32_t input_len) {

    // processing works in pairs of words (64-bit), so may
    // require one or two additional words, depending on
    // whether the input has an even or odd number of words.
    uint32_t output_len = input_len
                        + ((shift_count + 31) >> 5);
    output_len += output_len & 1;
    if (output_len <= input_len) {
        // if overflow, make sure js_big_alloc will throw
        output_len = (1 << 30);
    }
    uint32_t *output_big = js_big_alloc(env, output_len);
    uint32_t *output_ptr = output_big + 1;
    input_ptr++; // advance past word count

    // insert pairs of zero-words for shift counts that
    // are larger than 64.  if the resulting shift count
    // is still 32 or above, insert another zero-word,
    // so the final shift count is always less than 32.
    while (shift_count >= 64) {
        *(uint64_t *)output_ptr = 0;
        output_ptr += 2;
        shift_count -= 64;
    }
    if (shift_count >= 32) {
        *output_ptr++ = 0;
        shift_count -= 32;
    }

    //
    // if the final shift count is zero, just copy
    // the rest of the input, without shift processing
    //
    if (shift_count == 0) {

        while (input_len > 1) {
            *(uint64_t *)output_ptr =
                            *(uint64_t *)input_ptr;
            output_ptr += 2;
            input_ptr += 2;
            input_len -= 2;
        }
        if (input_len)
            *output_ptr++ = *input_ptr;
        *output_big = output_ptr - (output_big + 1);

    } else {

        //
        // for an input that has more than one word,
        // process input in blocks of 64-bit, and then
        // insert one or two additional words for sign
        //

        const int right_shift_count = 64 - shift_count;
        uint64_t words = 0;
        uint64_t carry = 0;

        while (input_len > 1) {
            words = *(uint64_t *)input_ptr;
            *(uint64_t *)output_ptr =
                    (words << shift_count) | carry;
            carry = words >> right_shift_count;
            output_ptr += 2;
            input_ptr += 2;
            input_len -= 2;
        }

        if (input_len) {
            // input has an odd number of words, pick up
            // the last word, it is already sign-extended
            words = (int64_t)*(int32_t *)input_ptr;
        } else {
            // the input has an even number of words, so
            // grab the sign bit from the last word read
            words = ((int64_t)words) >> 63;
        }
        *(uint64_t *)output_ptr =
                (words << shift_count) | carry;
        output_ptr += 2;

        js_big_trim(output_big, output_ptr);
    }

    return output_big;
}

// ------------------------------------------------------------
//
// js_big_shift_right
//
// ------------------------------------------------------------

static uint32_t *js_big_shift_right (
                    js_environ *env, int32_t shift_count,
                    uint32_t *input_ptr, int32_t input_len) {

    // the calling function (usually js_big_shift ()) should
    // make sure that the shift count is smaller than the
    // size of the input, so a simple subtraction is enough
    // to determine the size of the output.
    uint32_t shift_word_count = shift_count >> 5;
    uint32_t output_len = input_len - shift_word_count;
    uint32_t *output_big = js_big_alloc(env, output_len);
    uint32_t *output_ptr = output_big + 1;

    // advance the input pointer a number of words based
    // on the number of full words in the shift count,
    // and keep the final shift count between 0 and 31
    input_ptr += shift_word_count
              +  1; // advance past word count
    input_len -= shift_word_count;
    shift_count &= 31;

    //
    // if the final shift count is zero, just copy
    // the rest of the input, without shift processing
    //
    if (shift_count == 0) {

        while (input_len > 1) {
            *(uint64_t *)output_ptr =
                                *(uint64_t *)input_ptr;
            output_ptr += 2;
            input_ptr += 2;
            input_len -= 2;
        }
        if (input_len)
            *output_ptr++ = *input_ptr;
        *output_big = output_ptr - (output_big + 1);

    } else {

        // process input one word at a time, but with
        // a lookahead to the next-higher word.  this
        // 64-bit value is shifted right and stored
        // in the output, one word at a time.  at the
        // end of each iteration, the next-higher word
        // is sign-extended into its position as the
        // low word, and this sign is discard at the
        // start of the following iteration, except
        // at the end of the loop, when the sign is
        // stored as part of the last word.

        int64_t words = *input_ptr++;
        while (--input_len) {
            words = (uint32_t)words; // zero high word
            words |= (uint64_t)(*input_ptr++) << 32;
            *output_ptr++ =
                    (uint32_t)(words >> shift_count);
            words >>= 32; // sign-extended right-shift
        }
        *output_ptr++ = (uint32_t)(words >> shift_count);

        js_big_trim(output_big, output_ptr);
    }

    return output_big;
}

// ------------------------------------------------------------
//
// js_big_shift
//
// ------------------------------------------------------------

static js_val js_big_shift (js_environ *env, js_val value,
                            js_val shift, int shift_dir) {

    //
    // convert shift count bigint to a 64-bit number.
    // there cannot be more than 2^24 words in a bigint,
    // and with 5 bits in a word, the shift count cannot
    // be larger than 2^29, which fits in one word.
    //

    uint32_t *shift_ptr = js_get_pointer(shift);
    int32_t shift_count;
    //if (js_big_trimmed_len(shift_ptr) == 1)
    if (*shift_ptr == 1)
        shift_count = *(int32_t *)(shift_ptr + 1);
    else {
        // get sign from highest word (bit 31)
        shift_count = shift_ptr[*shift_ptr];
        shift_count >>= 31;
        // flip bit 30 so the count is always too big
        shift_count ^= (1 << 30);
    }

    //
    // right shift.  but if the number of bits to
    // shift is higher than the bits in the input
    // number, then just return the sign of input
    //

    uint32_t *input_ptr = js_get_pointer(value);
    //uint32_t input_len = js_big_trimmed_len(input_ptr);
    uint32_t input_len = *input_ptr;
    uint32_t *output;

    shift_count *= shift_dir;
    if (shift_count < 0) {

        shift_count = -shift_count;
        if (shift_count >= (input_len << 5)) {

            int32_t sign =
                ((int32_t)input_ptr[input_len]) >> 31;

            output = js_malloc(sizeof(uint32_t[1 + 1]));
            output[0] = 1;
            output[1] = sign;

        } else {

            output = js_big_shift_right(env, shift_count,
                                    input_ptr, input_len);
        }

    } else {

        //
        // left shift.  but if the shift count is
        // zero, then return the input value as is
        //

        if (shift_count == 0)
            return value;

        output = js_big_shift_left(env, shift_count,
                                    input_ptr, input_len);
    }

    return js_make_primitive(output, js_prim_is_bigint);
}

// ------------------------------------------------------------
//
// js_big_add_sub
//
// ------------------------------------------------------------

static js_val js_big_add_sub (js_environ *env,
                              js_val left, js_val right,
                              int64_t right_multiplier) {

    // allocate a new number that is one word longer than
    // the longer of the two input bigints.  then sum the
    // words from the input bigint into the new bigint.

    uint32_t *big1 = js_get_pointer(left);
    uint32_t len1 = *big1;

    uint32_t *big2 = js_get_pointer(right);
    uint32_t len2 = *big2;

    // allocate an extra word, only if bigs are same length
    uint32_t *big3, *ptr3;
    int64_t carry = 0;

    if (len1 > len2) {

        //
        // left operand has more words than right operand
        //

        big3 = js_big_alloc(env, len1);
        ptr3 = big3 + 1;

        len1 -= len2;
        while (len2--) {
            carry += *(++big1);
            carry += *(++big2) * right_multiplier;
            *(ptr3++) = (uint32_t)(carry);
            carry >>= 32;
        }
        while (len1--) {
            carry += *(++big1);
            carry += ((*(int32_t *)big2) >> 31) // sign2
                        * right_multiplier;
            *(ptr3++) = (uint32_t)(carry);
            carry >>= 32;
        }

    } else if (len2 > len1) {

        //
        // right operand has more words than left operand
        //

        big3 = js_big_alloc(env, len2);
        ptr3 = big3 + 1;

        len2 -= len1;
        while (len1--) {
            carry += *(++big1);
            carry += *(++big2) * right_multiplier;
            *(ptr3++) = (uint32_t)(carry);
            carry >>= 32;
        }
        while (len2--) {
            carry += (*(int32_t *)big1) >> 31; // sign1
            carry += *(int32_t *)(++big2)
                        * right_multiplier;
            *(ptr3++) = (uint32_t)(carry);
            carry >>= 32;
        }

    } else {

        //
        // both operands have the same number of words,
        // this may cause overflow in the calculation,
        // thus requires the allocation of an extra word
        //

        big3 = js_big_alloc(env, len1 + 1);
        ptr3 = big3 + 1;

        while (len1--) {
            carry += *(++big1);
            carry += *(++big2) * right_multiplier;
            *ptr3++ = (uint32_t)(carry);
            carry >>= 32;
        }
        // sign-extend into the last remaining word
        *ptr3++ = (uint32_t)carry;
    }

    js_big_trim(big3, ptr3);
    return js_make_primitive(big3, js_prim_is_bigint);
}

// ------------------------------------------------------------
//
// js_big_multiply
//
// ------------------------------------------------------------

static js_val js_big_multiply (js_environ *env,
                               js_val left, js_val right) {

    // multiply two input bigints using the naive multiply
    // algorithm, where each word in big2 is multiplied by
    // each word in big1, and for every word in big1, we
    // shift the result one word to the right:
    //
    //    1234 <-- 4 words
    // *    56 <-- 2 words
    //    ----
    //    7404 <-- first iteration, no shift
    // + 6170  <-- shifted by one word
    //   -----
    //   69104 <-- result cannot be more than 4+2 = 6 words
    //
    // this naive algorithm is not guaranteed to work with
    // negative inputs, so if either input is negative, we
    // multiply it by -1.  if only one input was negative,
    // we negate the result.

    bool negate_result = false;

    uint32_t *big1 = js_get_pointer(left);
    uint32_t len1 = *big1;
    if (((int32_t)big1[len1]) >> 31) {
        negate_result = true;
        // allocates a new bigint with same length
        big1 = js_big_negate2(env, 1ULL, big1, len1);
    }

    uint32_t *big2 = js_get_pointer(right);
    uint32_t len2 = *big2;
    if (((int32_t)big2[len2]) >> 31) {
        negate_result = !negate_result;
        // allocates a new bigint with same length
        big2 = js_big_negate2(env, 1ULL, big2, len2);
    }

    // allocate the result, clear it to zero
    uint32_t len3 = len1 + len2 + 1;
    uint32_t *big3 = js_big_alloc(env, len3);
    uint32_t *big3end = big3 + 1;
    while (len3 > 1) {
        *(uint64_t *)big3end = 0;
        big3end += 2;
        len3 -= 2;
    }
    if (len3)
        *big3end++ = 0;

    for (int idx1 = 1; idx1 <= len1; ++idx1) {

        uint64_t carry = 0;
        for (int idx2 = 1; idx2 <= len2; ++idx2) {

            // note that the outer loop index determines the
            // base index in the output, and grows by 1, i.e.
            // it shifts right one word, with each iteration.

            carry += (uint64_t)big1[idx1]
                   * (uint64_t)big2[idx2];
            carry += big3[idx1 + idx2 - 1];
            big3[idx1 + idx2 - 1] = (uint32_t)carry;
            carry >>= 32;
        }

        // we don't subtract 1 from the index here, because
        // we want to write a word past the length of 'big2'
        big3[idx1 + len2] += carry;
    }

    // negate the result if signs of inputs are not same
    if (negate_result)
        js_big_negate1(big3, big3end);

    js_big_trim(big3, big3end);
    return js_make_primitive(big3, js_prim_is_bigint);
}

// ------------------------------------------------------------
//
// js_big_divide_by_integer
//
// ------------------------------------------------------------

static js_val js_big_divide_by_integer (
                js_environ *env, js_val big_dividend,
                uint32_t divisor, uint32_t *out_remainder) {

    // long division by a 32-bit unsigned integer.  we divide
    // the highest two words (64 bits) from the dividend by
    // the divisor.  the 64-bit quotient is pushed into the
    // result, and the remainder becomes the high 32 bits of
    // the dividend, and the next word from the input bigint
    // becomes the low 32 bits of the 64-bit dividend.
    //
    // every two digits in the example below represent a word:
    //
    //          12345678 / 43 = 287108 r 34
    //
    // step 1.  1234     / 43 = 28 r 30
    //          12 and 34 are the first two words in the input
    //          28 is pushed into the result
    //
    // step 2.  3056     / 43 = 71 r 3
    //          30 is the remainder from step 1.
    //          56 is the following word.
    //          71 is pushed into the result
    //
    // step 3.  0378     / 43 = 08 r 34
    //          03 is the reminder from step 2.
    //          78 is the following word
    //          08 is pushed into the result
    //
    // result.  12345678 / 43 = 287108 r 34
    //

    uint32_t *input_big = js_get_pointer(big_dividend);
    uint32_t len = *input_big;

    uint32_t *output_big = js_big_alloc(env, len);
    uint32_t *output_end = output_big + 1 + len;
    uint32_t *output_ptr = output_end;

    // start by combining the two highest words into
    // a 64-bit number, and dividing by the divisor.
    // this always produces two words in the output,
    // but if the higher of the two words is zero,
    // then js_big_trim () will shorten the output.

    uint64_t remainder;

    if (len > 1) {

        uint64_t dividend =
                    *(int64_t *)&input_big[--len];
        len--;

        int64_t quot = dividend / divisor;
           remainder = dividend % divisor;

        *--output_ptr = quot >> 32;
        *--output_ptr = (uint32_t)quot;

    } else
        remainder = 0;

    //
    // process the rest of the words in the input
    //

    while (len) {

        uint64_t dividend = (remainder << 32)
                          | input_big[len--];

        int64_t quot = dividend / divisor;
           remainder = dividend % divisor;

        //if (quot >> 32) { __builtin_trap(); }
        *--output_ptr = (uint32_t)quot;

        //if (output_ptr <= output_big) {__builtin_trap(); }
    }

    //if (output_ptr != output_big + 1) { __builtin_trap(); }

    *out_remainder = remainder;

    js_big_trim(output_big, output_end);
    return js_make_primitive(output_big, js_prim_is_bigint);
}

// ------------------------------------------------------------
//
// js_big_divide_long
//
// ------------------------------------------------------------

static void js_big_divide_long (
            uint32_t *ptr_dividend, uint32_t len_dividend,
            uint32_t *ptr_divisor,  uint32_t len_divisor,
            uint32_t *ptr_quotient, uint32_t len_quotient) {

    // long division based on public domain source code which
    // is based on Knuth.  this function should not be invoked
    // directly as it has strict requirements for its parameters.
    // see the calling logic in js_big_divide_with_remainder ().
    //
    // the main loop runs j over N highest words of the dividend,
    // where N is the number of words in the divisor.  each step,
    // two words at position j,j-1 from the dividend are divided
    // by the highest word of the divisor to produce q^ and r^,
    // estimates of the number of times the divisor fits in that
    // part of the dividend.
    //
    // q^ and r^ are then further refined by comparing the three
    // dividend words j,j-1,j-2 to the two highest divisor words.
    // the refined  q^ is either correct, or just one too high.
    //
    // the multiple of q^ * divisor is then subtracted from the
    // N highest words of the dividend starting at position j.
    // if this produces negative result in the highest word, then
    // q^ was estimated one too high, and one value of divisor is
    // added back to the N highest words of the dividend starting
    // at position j.
    //
    // the (possibly adjusted) q^ is stored into the quotient
    // buffer.  on return, the dividend parameter receives the
    // remainder.  the divisor parameter is not modified.

    ++ptr_dividend;
    ++ptr_divisor;
    ++ptr_quotient;

    uint32_t divisor_h1 = ptr_divisor[len_divisor - 1];
    uint32_t divisor_h2 = ptr_divisor[len_divisor - 2];
    const uint64_t _100000000 = ((uint64_t)1) << 32;

    for (uint32_t j = len_dividend - 1 - len_divisor;
                  j != -1; j--) {

        uint64_t p    = ptr_dividend[j + len_divisor];
        p = (p << 32) | ptr_dividend[j + len_divisor - 1];

        uint64_t qhat = p / divisor_h1;
        uint64_t rhat = p % divisor_h1;

        for (;;) {
            uint64_t limit = (rhat << 32)
                      | ptr_dividend[j + len_divisor - 2];
            if (qhat >= _100000000
            ||  qhat * divisor_h2 > limit) {

                --qhat;
                rhat += divisor_h1;
                if (rhat < _100000000)
                    continue;
            }
            break;
        }

        int64_t t;
        uint32_t k = 0;
        uint32_t i;
        for (i = 0; i < len_divisor; i++) {
            p = qhat * ptr_divisor[i];
            t = ptr_dividend[i + j];
            t = t - k - (uint32_t)p;
            ptr_dividend[i + j] = (uint32_t)t;
            k = (uint32_t)(p >> 32) - (uint32_t)(t >> 32);
        }
        t = ptr_dividend[i + j] - k;
        ptr_dividend[i + j] = (uint32_t)t;

        if (t < 0) {
            --qhat;

            k = 0;
            for (i = 0; i < len_divisor; i++) {
                t = ptr_dividend[i + j];
                t = t + ptr_divisor[i] - k;
                ptr_dividend[i + j] = (uint32_t)t;
                k = (uint32_t)(t >> 32);
            }
            ptr_dividend[i + j] += k;
        }

        ptr_quotient[j] = (uint32_t)qhat;
    }
}

// ------------------------------------------------------------
//
// js_big_divide_with_remainder
//
// ------------------------------------------------------------

static js_val js_big_divide_with_remainder (
                    js_environ *env, js_val big_dividend,
                    js_val big_divisor, js_val *remainder) {

    //
    // convert a negative dividend into a positive one.
    // required whether the divisor is a single-word
    // integer, or a multiple-word bigint, because in
    // both cases, the division process is unsigned.
    //

    js_val big_dividend_0 = big_dividend;

    uint32_t *ptr_dividend = js_get_pointer(big_dividend);
    uint32_t len_dividend = *ptr_dividend;

    int32_t sign_dividend =
            ((int32_t)ptr_dividend[len_dividend]) >> 31;

    if (sign_dividend) {

        // allocates a new bigint with same length
        ptr_dividend = js_big_negate2(env, 1ULL,
                            ptr_dividend, len_dividend);
        big_dividend = js_make_primitive(
                        ptr_dividend, js_prim_is_bigint);
    }

    // if divisor is just a single word (or two words,
    // but the high word is zero), then do fast division
    // by integer.  we also catch division by zero here.

    uint32_t *ptr_divisor = js_get_pointer(big_divisor);
    uint32_t len_divisor = *ptr_divisor;

    if (len_divisor == 1
    || (len_divisor == 2 && ptr_divisor[2] == 0)) {

        int32_t divisor = ptr_divisor[1];
        if (divisor == 0)
            js_callthrow("RangeError_division_by_zero");

        int32_t sign_result = sign_dividend; // -1 or 0
        if (len_divisor == 1 && divisor < 0) {
            divisor = -divisor;
            sign_result = ~sign_result;
        }

        uint32_t rem32;
        js_val quotient = js_big_divide_by_integer(
                    env, big_dividend, divisor, &rem32);

        if (sign_result) {

            uint32_t *ptr_quot = js_get_pointer(quotient);
            uint32_t *end_quot = ptr_quot + *ptr_quot + 1;
            js_big_negate1(ptr_quot, end_quot);
        }

        if (remainder) {

            // convert 0 to 1, but -1 stays -1
            sign_dividend +=
                (((uint32_t)sign_dividend) >> 31) ^ 1;

            uint32_t *ptr =
                    js_malloc(sizeof(uint32_t[1 + 1]));
            ptr[0] = 1;
            ptr[1] = (int32_t)rem32 * sign_dividend;

            *remainder =
                js_make_primitive(ptr, js_prim_is_bigint);
        }

        return quotient;
    }

    //
    // if divisor is negative, negate it as well
    //

    int32_t sign_divisor =
            ((int32_t)ptr_divisor[len_divisor]) >> 31;

    if (sign_divisor) {
        // allocates a new bigint with same length
        ptr_divisor = js_big_negate2(env, 1ULL,
                            ptr_divisor, len_divisor);
        big_divisor = js_make_primitive(
                        ptr_divisor, js_prim_is_bigint);
    }

    //
    // if divisor is larger than dividend, then return
    // with quotient <- 0, and remainder <- dividend.
    //

    int cmp = js_big_compare(big_divisor, big_dividend);
    if (cmp > 0) {
        // divisor is larger than dividiend.  note that
        // only the sign of the dividend determines the
        // sign of the remainder, so we can just return
        // the original dividend, before any negation
        *remainder = big_dividend_0;
        return env->big_zero; // quotient 0
    }

    //
    // otherwise we need to do long division.  this will be
    // handled by js_big_divide_helper () but that function
    // has strict requirements for its input, specifically:
    // (1) both dividend and divisor must not be negative.
    // (2) the divisor must be left-shifted a number of bits
    // to make sure its leftmost bit is set.
    // (3) the dividend must be left-shifted the same number
    // of bits as the divisor.  it must also have one 'spare'
    // high word, compare to its initial value.  if the shift
    // did not cause the allocation of this one extra word,
    // we have to do that explicitly.
    // (4) the quotient buffer is allocated to hold a number
    // of words equal to number of words in the dividend,
    // minus number of words in the divisor, plus 1.
    // (5) the remainder, returned by js_big_divide_helper ()
    // in the dividend buffer, must be shifted right the same
    // number of bits the divisor and dividend were shifted.

    //
    // find the highest set bit in the divisor, then
    // (1) shift the divisor by that offset, so its
    // highest bit at the leftmost position is set,
    // (2) shift the dividend by the same offset.
    //

    uint32_t len_dividend_0 = len_dividend;

    uint32_t word = ptr_divisor[len_divisor];
    uint32_t test_bit = 1U << 31;
    int shift_count = 0;
    while (!(word & test_bit)) {
        test_bit >>= 1;
        ++shift_count;
    }

    if (shift_count) {

        ptr_divisor = js_big_shift_left(
            env, shift_count, ptr_divisor, len_divisor);
        len_divisor = *ptr_divisor;

        // remove an extra zero word for sign, which may
        // have been added by the shift operation, because
        // the leftmost bit in the divisor must be set

        if (ptr_divisor[len_divisor] == 0)
            --len_divisor;

        // now shift left the dividend

        ptr_dividend = js_big_shift_left(
            env, shift_count, ptr_dividend, len_dividend);
        len_dividend = *ptr_dividend;
    }

    //
    // js_big_divide_helper () requires that its input
    // dividend has a spare word, so if the number of
    // words in the shifted dividend is still the same
    // as the input dividend, allocate the extra word.
    //

    if (len_dividend == len_dividend_0) {

        uint32_t *new_dividend =
                    js_big_alloc(env, ++len_dividend);
        uint32_t *ptr = new_dividend;
        *ptr++ = len_dividend;
        do {
            *ptr++ = *(++ptr_dividend);
        } while (--len_dividend);
        *ptr++ = 0;

        ptr_dividend = new_dividend;
    }

    //
    // call secondary function to do long division
    //

    uint32_t len_quotient = len_dividend - len_divisor
                          + /* extra word */ 1;
    uint32_t *ptr_quotient =
                        js_big_alloc(env, len_quotient);

    js_big_divide_long(ptr_dividend, len_dividend,
                       ptr_divisor,  len_divisor,
                       ptr_quotient, len_quotient);

    if (remainder) {

        // only the sign of the dividend determines the
        // sign of the remainder.  if the dividend was
        // negative, then negate the remainder.

        if (shift_count) {

            ptr_dividend = js_big_shift_right(
                            env, shift_count,
                            ptr_dividend, len_dividend);

            len_dividend = *ptr_dividend;
        }

        uint32_t *end_dividend =
                    ptr_dividend + 1 + len_dividend;

        if (sign_dividend)
            js_big_negate1(ptr_dividend, end_dividend);

        js_big_trim(ptr_dividend, end_dividend);

        *remainder = js_make_primitive(
                        ptr_dividend, js_prim_is_bigint);
    }

    // if the sign of the divisor and the sign of the
    // dividend differ, then negate the quotient

    uint32_t *end_quotient =
                    ptr_quotient + 1 + len_quotient;

    if (sign_dividend != sign_divisor)
        js_big_negate1(ptr_quotient, end_quotient);

    js_big_trim(ptr_quotient, end_quotient);
    return js_make_primitive(
                ptr_quotient, js_prim_is_bigint);
}

// ------------------------------------------------------------
//
// js_big_power
//
// ------------------------------------------------------------

static js_val js_big_power (js_environ *env,
                            js_val base, js_val exponent) {

    uint32_t *big_exp = js_get_pointer(exponent);
    uint32_t len_exp = *big_exp;

    if (((int32_t)big_exp[len_exp]) >> 31) {
        // negative exponent is not permitted
        js_callthrow("RangeError_invalid_argument");
    }

    uint32_t *temp_ptr =
                    js_malloc(sizeof(uint32_t[1 + 1]));
    temp_ptr[0] = 1;
    temp_ptr[1] = 1;
    js_val temp = js_make_primitive(temp_ptr,
                                    js_prim_is_bigint);

    // if exponent is zero, do nothing, return 1
    if (len_exp == 1 && big_exp[1] == 0)
        return temp;

    for (;;) {

        if (big_exp[1] & 1)
            temp = js_big_multiply(env, base, temp);

        if (len_exp == 1 && big_exp[1] <= 1)
            break; // if exponent <= 1, break

        base = js_big_multiply(env, base, base);

        big_exp = js_big_shift_right(
                        env, 1, big_exp, len_exp);
        len_exp = *big_exp;
    }

    return temp;
}
