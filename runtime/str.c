
// ------------------------------------------------------------
//
// string
//
// ------------------------------------------------------------

#define js_make_primitive_string(p) \
    js_make_primitive((p), js_prim_is_string)

#define js_make_primitive_symbol(p) \
    js_make_primitive((p), js_prim_is_symbol)

// ------------------------------------------------------------
//
// js_str_is_white_space
//
// ------------------------------------------------------------

static bool js_str_is_white_space (wchar_t ch) {

    switch (ch) {
        case 0x0009: case 0x000A: case 0x000B: case 0x000C:
        case 0x000D: case 0x0020: case 0x00A0: case 0x1680:
        case 0x2000: case 0x2001: case 0x2002: case 0x2003:
        case 0x2004: case 0x2005: case 0x2006: case 0x2007:
        case 0x2008: case 0x2009: case 0x200A: case 0x2028:
        case 0x2029: case 0x202F: case 0x205F: case 0x3000:
        case 0xFEFF: return true;
        default:     return false;
    }
    return false;
}

// ------------------------------------------------------------
//
// js_str_is_interned
// js_str_intern
//
// ------------------------------------------------------------

#define js_str_is_interned(id)              \
    ((id)->flags & js_str_in_objset)

#define js_str_flag_as_interned(id)         \
    js_compare_and_swap_16(&(id)->flags,    \
            (uint16_t)-1, js_str_in_objset)

#define js_str_intern(id) {                 \
    objset_id *id2 = objset_intern(         \
            &env->strings_set, (id), NULL); \
    if (likely(id2 != (id))) {              \
        js_check_alloc(id2);                \
        (id) = id2;                         \
    } else if (!js_str_is_interned(id))     \
        js_str_flag_as_interned(id); }

// ------------------------------------------------------------
//
// js_newstr
//
// this function should be called during the initialization
// of a module, to initialize all the strings used in that
// module.  the input pointer should point to static data,
// which can
// and the program should not modify the memory bytes at the
// pointer after passing it to this function.
//
// the 'intern' parameter is a performance hint that says
// the string will index into objects, so should be interned.
//
// strings created by js_newstr () are not tracked by
// the gc, see also gc_mark_vals ()
//
// ------------------------------------------------------------

js_val js_newstr (js_environ *env, const wchar_t *ptr) {

    objset_id *id = (objset_id *)ptr;
    const int objset_flag_or_zero = id->flags ^
                (js_str_is_string | js_str_is_static);
    if (objset_flag_or_zero == js_str_in_objset) {
        js_str_intern(id);
    } else if (objset_flag_or_zero) {
        fprintf(stderr, "Bad string!\n");
        exit(1);
    }
    return js_make_primitive_string(id);
}

// ------------------------------------------------------------
//
// js_str_c2
//
// create a non-interned string from ascii text.
// used by js_num_tostring (), and by js_str_c ().
//
// ------------------------------------------------------------

static js_val js_str_c2 (js_environ *env,
                         const char *ptr_chars,
                         int num_chars) {

    int wlen = num_chars << 1;
    objset_id *id = js_malloc(sizeof(objset_id) + wlen);
    id->len = wlen;
    id->flags = js_str_is_string;

    wchar_t *ptr_wch = id->data;
    while (num_chars-- > 0)
        *ptr_wch++ = *ptr_chars++;

    return js_gc_manage(env,
                js_make_primitive_string(id));
}

// ------------------------------------------------------------
//
// js_str_c
//
// create an interned string from ascii text
//
// ------------------------------------------------------------

static js_val js_str_c (js_environ *env,
                        const char *ascii_text) {

    int num_chars = strlen(ascii_text);
    if (!num_chars)
        return env->str_empty;

    js_val str = js_str_c2(env, ascii_text, num_chars);

    objset_id *id = (objset_id *)js_get_pointer(str);
    objset_id *id2 =
            objset_intern(&env->strings_set, id, NULL);

    if (id2 == id) {
        // the input id has been interned and returned,
        // so we should flag the id as such
        js_str_flag_as_interned(id);

    } else {
        // we received an id that was already interned
        js_check_alloc(id2);
        str = js_make_primitive_string(id2);
    }

    return str;
}

// ------------------------------------------------------------
//
// js_str_search_or_intern
//
// ------------------------------------------------------------

static js_val js_str_search_or_intern (
        js_environ *env, const wchar_t *data, int wlen) {

    objset_id *id = objset_search(
                        env->strings_set, data, wlen);
    if (id)
        return js_make_primitive_string(id);

    id = js_malloc(sizeof(objset_id) + wlen);
    id->len = wlen;
    id->flags = js_str_is_string | js_str_in_objset;
    memcpy(id->data, data, wlen);

    objset_id *id2 =
        objset_intern(&env->strings_set, id, NULL);

    if (unlikely(id != id2)) {
        // should never happen unless out of mem
        js_check_alloc(id2);
        fprintf(stderr, "Corrupted!\n");
        exit(1); // __builtin_trap();
    }

    return js_gc_manage(env,
                js_make_primitive_string(id));
}

// ------------------------------------------------------------
//
// js_str_concat
//
// concatenates two input strings into a new string.
// the new string is not interned.
//
// ------------------------------------------------------------

static js_val js_str_concat (js_environ *env,
                             js_val left, js_val right) {

    const objset_id *left_id =
            js_get_pointer(left = js_tostring(env, left));

    const objset_id *right_id =
            js_get_pointer(right = js_tostring(env, right));

    if (left_id->len == 0)
        return right;
    if (right_id->len == 0)
        return left;

    int len = left_id->len + right_id->len;
    objset_id *new_id = js_malloc(
                            sizeof(objset_id) + len);
    new_id->len = len;
    new_id->flags = js_str_is_string;

    memcpy(new_id->data, left_id->data, left_id->len);
    memcpy((char *)new_id->data + left_id->len,
           right_id->data, right_id->len);

    return js_gc_manage(env,
                js_make_primitive_string(new_id));
}

// ------------------------------------------------------------
//
// js_str_equals
//
// ------------------------------------------------------------

static bool js_str_equals (js_environ *env,
                           js_val left, js_val right) {

    const objset_id *left_id = js_get_pointer(left);
    const objset_id *right_id = js_get_pointer(right);

    if (left_id == right_id)
        return true;

    else if ((left_id->flags & js_str_in_objset)
         && (right_id->flags & js_str_in_objset)) {

        // both are interned, but pointers are not same
        return false;
    }

    // check if string lengths are different
    int len = left_id->len;
    if (len != right_id->len)
        return false;

    // check if any character is different
    const wchar_t *left_ch = left_id->data;
    const wchar_t *right_ch = right_id->data;
    while (len) {
        if (*left_ch++ != *right_ch++)
            return false;
        len -= 2;
    }

    return true; // strings equal by value
}

// ------------------------------------------------------------
//
// js_str_compare
//
// ------------------------------------------------------------

static int js_str_compare (js_val left, js_val right) {

    const objset_id *left_id = js_get_pointer(left);
    const objset_id *right_id = js_get_pointer(right);

    if (left_id == right_id)
        return 0;

    // check if string lengths are different
    int len = left_id->len;
    if (len > right_id->len)
        len = right_id->len;

    // check if any character is different
    const wchar_t *left_ch = left_id->data;
    const wchar_t *right_ch = right_id->data;
    while (len) {
        int cmp = *left_ch++ - *right_ch++;
        if (cmp != 0)
            return cmp;
        len -= 2;
    }

    return (left_id->len - right_id->len);
}

// ------------------------------------------------------------
//
// js_str_is_length_or_number
//
// if 'prop' is in range 0 <= prop <= js_max_index,
// or is a string which starts with a digit and convertible
// to such a number (so e.g. '-0' would not apply), then
// return that index value, plus one.  if 'prop' is exactly
// the string 'length', return js_len_index.
// otherwise, returns js_not_index.
//
// ------------------------------------------------------------

#define js_max_index 0xFFFFFFF9U
#define js_len_index 0xFFFFFFFCU
#define js_not_index 0xFFFFFFFEU

#if (js_max_index + 1) != js_arr_max_length
#error mis-match length and max index
#endif

static uint32_t js_str_is_length_or_number (
                            js_environ *env, js_val prop) {

    //
    // if this is an object value, convert it to primitive.
    // if this is a number value, check if integer in range
    //

    if (js_is_object(prop)) {

        prop = js_obj_to_primitive_string(env, prop);
    }

    if (js_is_number(prop)) {

        const uint32_t num = js_get_number(prop);
        if (js_make_number(num).raw == prop.raw
                             && num <= js_max_index)
            return (num + 1);

        return js_not_index;
    }

    //
    // check if string value or string object.
    // if some other type of object, also handle here.
    //

    const objset_id *id = NULL;

    if (js_is_primitive(prop)) {

        const int primitive_type =
                        js_get_primitive_type(prop);

        if (unlikely(primitive_type == js_prim_is_bigint))
            prop = js_big_tostring(env, prop, 10);

        else if (unlikely(primitive_type != js_prim_is_string))
            return js_not_index;

        id = js_get_pointer(prop);

    } else
        return js_not_index;

    //
    // check if the string is 'length',
    // return 'js_len_index
    //

    const wchar_t *text_ptr = id->data;
    const objset_id *cmp = js_get_pointer(env->str_length);

    if (id->flags & js_str_in_objset) {
        // if both objects are in the objset,
        // then we can just compare pointers
        if (id == cmp) {

            return js_len_index;
        }

    } else {
        // compare bytes to see if 'length'
        if (id->len == cmp->len && 0 ==
                memcmp(text_ptr, cmp->data, cmp->len)) {

            return js_len_index;
        }
    }

    //
    // check if empty string, or a single '0' character
    //

    wchar_t *text_end =
                (wchar_t *)((char *)text_ptr + id->len);

    if (text_ptr >= text_end)
        return js_not_index;

    wchar_t ch = *text_ptr++;
    if (ch == L'0') {

        if (text_ptr >= text_end) {

            // return 1 if string exactly '0' for index 0
            return 1;

        } else
            return js_not_index;
    }

    //
    // check if the string contains an acceptable integer
    //

    uint64_t num = ch - L'0';
    if (num > 9)
        return js_not_index;

    while (text_ptr < text_end) {

        ch = *text_ptr++;
        uint32_t digit = ch - L'0';
        if (digit > 9)
            return js_not_index;

        num = num * 10 + digit;
        if (num > js_max_index)
            return js_not_index;
    }

    return (uint32_t)(num + 1);
}

// ------------------------------------------------------------
//
// js_str_index
//
// create an interned string from integer index
//
// ------------------------------------------------------------

static js_val js_str_index (js_environ *env,
                            uint32_t prop_idx) {

    wchar_t digits[28]; // room for two copies
    int len_digits;

    if (--prop_idx <= js_max_index) {
        // prop_idx is in range 1 .. (js_max_index + 1)
        wchar_t *ch_R = (wchar_t *)
                    ((char *)digits + sizeof(digits));
        *--ch_R = 0;
        do {
            int digit = prop_idx % 10;
            prop_idx = prop_idx / 10;
            *--ch_R = digit + L'0';
        } while (prop_idx);
        wchar_t *ch_L = digits;
        while ((*ch_L = *ch_R)) {
            ch_L++;
            ch_R++;
        }
        len_digits = (char *)ch_L - (char *)digits;

    } else {
        // prop_idx is zero or invalid
        digits[0] = L'0';
        len_digits = sizeof(wchar_t);
    }

    return js_str_search_or_intern(
                            env, digits, len_digits);
}

// ------------------------------------------------------------
//
// js_str_setprop
//
// ------------------------------------------------------------

static bool js_str_setprop (
                js_environ *env, js_val obj, js_val prop) {

    uint32_t prop_idx = js_str_is_length_or_number(env, prop);

    if (prop_idx != js_len_index) {

        objset_id *str_ptr = js_get_pointer(obj);
        uint32_t str_len = str_ptr->len >> 1;

        if (prop_idx - 1 >= str_len) {

            // property index is not 'length', and is not
            // an integer index between 0 and the actual
            // length of the string, so we return false,
            // so js_setprop () can continue checking on
            // the prototype chain, see also propset.c
            return false;
        }
    }

    // property index specifies a valid property on the
    // string primitive value.  if this is strict mode,
    // we should throw an error.  otherwise, just ignore.

    js_throw_if_strict_0("TypeError_primitiveProperty");
    return true;
}

// ------------------------------------------------------------
//
// js_str_getprop
//
// returns the length of the string, as a number value,
// if prop is 'length'.  otherwise if prop is (or can be
// converted to) a number between 0 and length, returns
// an (interned) string matching the character at that
// position in the string.  otherwise returns js_deleted.
//
// ------------------------------------------------------------

static js_val js_str_getprop (js_environ *env,
                              js_val obj, js_val prop) {

    uint32_t prop_idx = js_str_is_length_or_number(env, prop);

    objset_id *str_ptr = js_get_pointer(obj);
    uint32_t str_len = str_ptr->len >> 1;

    if (prop_idx == js_len_index)
        return js_make_number(str_len);

    if (prop_idx <= str_len) {

        const wchar_t *the_char = &str_ptr->data[prop_idx - 1];
        return js_str_search_or_intern(
                            env, the_char, sizeof(wchar_t));
    }

    // property index is not 'length', and is not an integer
    // index between 0 and the actual length of the string,
    // so we return false, so js_getprop () can continue
    // checking on the prototype chain, see also propget.c
    return js_deleted;
}

// ------------------------------------------------------------
//
// js_tostring
//
// ------------------------------------------------------------

static js_val js_tostring (js_environ *env, js_val val) {

    if (js_is_object(val)) {
        // note there is no 'else' at the end of this
        // condition, because we want the rest of the
        // function to look at this new primitive value
        val = js_obj_to_primitive_string(env, val);
    }

    if (js_is_primitive(val)) {

        switch (js_get_primitive_type(val)) {

            case js_prim_is_string:
                return val;

            case js_prim_is_bigint:
                return js_big_tostring(env, val, 10);
        }

    } else if (js_is_number(val)) {
        return js_num_tostring(env, val, 10);

    } else if (js_is_undefined(val))
        return env->str_undefined;

    else if (val.raw == js_null.raw)
        return env->str_null;

    else if (val.raw == js_false.raw)
        return env->str_false;

    else if (val.raw == js_true.raw)
        return env->str_true;

    // we should only get here for a symbol value, but
    // this is also a catch-all for any unexpected value
    js_callthrow("TypeError_convert_symbol_to_string");
    return js_make_number(0);
}

// ------------------------------------------------------------
//
// js_str_init
//
// ------------------------------------------------------------

static void js_str_init (js_environ *env) {

    //
    // define the empty string ''
    //

    env->strings_set = js_check_alloc(objset_create());

    objset_id *empty = js_malloc(sizeof(objset_id));
    empty->len = 0;
    empty->flags = js_str_is_string
                 | js_str_in_objset
                 | js_str_is_static;
    empty = js_check_alloc(objset_intern(
                    &env->strings_set, empty, NULL));

    env->str_empty = js_make_primitive_string(empty);

    //
    // define rest of the well-known strings
    //

#define declare_string(s) env->str_##s = js_str_c(env, #s)

    // typeof strings
    declare_string(arguments);
    declare_string(bigint);
    declare_string(boolean);
    declare_string(callee);
    declare_string(caller);
    declare_string(configurable);
    declare_string(constructor);
    declare_string(default);
    declare_string(done);
    declare_string(enumerable);
    declare_string(false);
    declare_string(function);
    declare_string(get);
    declare_string(length);
    declare_string(name);
    declare_string(next);
    declare_string(null);
    declare_string(number);
    declare_string(object);
    declare_string(prototype);
    declare_string(return);
    declare_string(set);
    declare_string(string);
    declare_string(symbol);
    declare_string(throw);
    declare_string(toString);
    declare_string(true);
    declare_string(undefined);
    declare_string(value);
    declare_string(valueOf);
    declare_string(writable);

    env->str_nan          = js_str_c(env, "NaN");
}

// ------------------------------------------------------------
//
// js_str_init_2
//
// ------------------------------------------------------------

#include "strsup.c"

static void js_str_init_2 (js_environ *env) {

    js_val obj;

    // prototype for string primitive
    obj = js_emptyobj(env);
    env->str_proto = (void *)((uintptr_t)js_get_pointer(obj));

    // prototype for symbol primitive
    obj = js_emptyobj(env);
    env->sym_proto = (void *)((uintptr_t)js_get_pointer(obj));

    js_str_init_sup(env);
}
