
// ------------------------------------------------------------
//
// js_str_print
//
// ------------------------------------------------------------

static js_val js_str_print (js_c_func_args) {

    const wchar_t *txt_ptr = L"?";
               int txt_len = 1;

    js_link *arg_ptr = stk_args->next;
    if (arg_ptr != js_stk_top) {

        js_val arg_val = arg_ptr->value;
        if (js_is_primitive_string(arg_val)) {

            const objset_id *id = js_get_pointer(arg_val);
            txt_ptr = id->data;
            txt_len = id->len >> 1;

        // while the primary purpose is to print strings,
        // it also serves as a general debug-print utility
        } else {
            printf("<dbg>");
            js_print(env, arg_val);
            js_return(js_undefined);
        }
    }

    printf("%*.*ls", txt_len, txt_len, txt_ptr);
    js_return(js_undefined);
}

// ------------------------------------------------------------
//
// js_sym_util - utility function for managing Symbols.
// if first argument is a string, creates a new symbol value
// with that string description.  if the first argument is
// a symbol, returns the description string for the symbol.
//
// ------------------------------------------------------------

static js_val js_sym_util (js_c_func_args) {
    js_prolog_stack_frame();

    js_val arg_val = js_undefined;
    js_val ret_val = js_undefined;

    js_link *arg_ptr = stk_args->next;
    if (arg_ptr != js_stk_top)
        arg_val = arg_ptr->value;

    do {

        if (!js_is_primitive(arg_val))
            break;

        int prim_type = js_get_primitive_type(arg_val);
        if (prim_type != js_prim_is_string
        &&  prim_type != js_prim_is_symbol)
            break;

        objset_id *id = js_get_pointer(arg_val);

        objset_id *id2 = js_malloc(
                            sizeof(objset_id) + id->len);
        memcpy(id2->data, id->data,
                            (id2->len = id->len));

        // return a symbol for a descr string,
        // return a descr string for a symbol
        if (prim_type == js_prim_is_string) {
            id2->flags = js_str_is_symbol;
            ret_val = js_make_primitive_symbol(id2);
        } else {
            id2->flags = js_str_is_string;
            ret_val = js_make_primitive_string(id2);
        }
        js_gc_manage(env, ret_val);

    } while (false);

    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_str_utf16 - convert utf-16 representations
// from numeric to string and vice versa.
//
// ------------------------------------------------------------

static js_val js_str_utf16 (js_c_func_args) {
    js_prolog_stack_frame();

    js_val arg_val = js_undefined;
    js_val ret_val = js_undefined;

    js_link *arg_ptr = stk_args->next;
    if (arg_ptr != js_stk_top)
        arg_val = arg_ptr->value;

    if (js_is_primitive_string(arg_val)) {

        objset_id *id = js_get_pointer(arg_val);
        if (id->len >= sizeof(wchar_t))
            ret_val.num = (uint16_t)id->data[0];
        else
            ret_val = js_nan;

    } else if (js_is_number(arg_val)) {

        uint32_t arg_int = arg_val.num;
        objset_id *id;

        if (arg_int <= 0xFFFF) {

            id = js_malloc(sizeof(objset_id)
                         + sizeof(wchar_t));
            id->len = sizeof(wchar_t);
            id->data[0] = arg_int;

        } else {

            id = js_malloc(sizeof(objset_id)
                         + sizeof(wchar_t) * 2);
            id->len = sizeof(wchar_t) * 2;
            id->data[0] = 0xD800
                        + (((uint16_t)arg_int) >> 10);
            id->data[1] = 0xDC00
                        + (arg_int & 0x3FF);
        }

        id->flags = js_str_is_string;
        ret_val = js_gc_manage(env,
                    js_make_primitive_string(id));

    } else if (js_is_object(arg_val)) {

        js_arr *arr = js_get_pointer(arg_val);
        if (js_obj_is_exotic(arr, js_obj_is_array)) {
            uint32_t len = arr->length;
            if (len && len != -1U) {

                objset_id *id = js_malloc(sizeof(objset_id)
                                  + len * sizeof(wchar_t));
                id->len = len * sizeof(wchar_t);
                id->flags = js_str_is_string;

                uint16_t *data = id->data;
                js_val *val = arr->values;
                for (;;) {
                    *data = (uint16_t)(val->num);
                    if (!(--len))
                        break;
                    data++;
                    val++;
                }

                ret_val = js_gc_manage(env,
                            js_make_primitive_string(id));
            }
        }
    }

    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_str_trim
//
// ------------------------------------------------------------

static js_val js_str_trim (js_c_func_args) {
    js_prolog_stack_frame();

    js_val ret_val = js_undefined;

    do {

        js_link *arg_ptr = stk_args->next;
        js_val str_val = (arg_ptr != js_stk_top)
                ? arg_ptr->value : js_undefined;
        if (!js_is_primitive_string(str_val)) {
            js_throw_if_nullobj(env, str_val);
            str_val = js_tostring(env, str_val);
        }

        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        js_val cmd_val = arg_ptr->value;
        if (!js_is_number(cmd_val))
            break;

        objset_id *id = js_get_pointer(str_val);
        wchar_t *txt_ptr = id->data;
        int txt_len = id->len >> 1;
        int txt_len_0 = txt_len;

        if (cmd_val.num <= 0) {
            while (txt_len) {
                const char ch = *txt_ptr;
                if (!js_str_is_white_space(ch))
                    break;
                ++txt_ptr;
                --txt_len;
            }
        }

        if (cmd_val.num >= 0) {
            wchar_t *end_ptr = txt_ptr + txt_len;
            while (txt_len) {
                const char ch = *(--end_ptr);
                if (!js_str_is_white_space(ch))
                    break;
                --txt_len;
            }
        }

        if (txt_len == txt_len_0)
            ret_val = str_val; // same as input

        else if (!txt_len)
            ret_val = env->str_empty;

        else {
            // create a new sub-string
            id = js_malloc(sizeof(objset_id)
                            + (txt_len <<= 1));
            id->len = txt_len;
            id->flags = js_str_is_string;
            memcpy(id->data, txt_ptr, txt_len);

            ret_val = js_gc_manage(env,
                js_make_primitive_string(id));
        }

    } while (0);
    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_str_sub_helper
//
// ------------------------------------------------------------

static js_val js_str_sub_helper (
                        js_environ *env, js_val str_val,
                        js_link *arg_ptr, int which) {

    // convert input 'this' value to string
    // and get its length
    if (!js_is_primitive_string(str_val)) {
        js_throw_if_nullobj(env, str_val);
        str_val = js_tostring(env, str_val);
    }

    objset_id *id = js_get_pointer(str_val);
    int32_t str_len = id->len >> 1;

    // use doubles in comparison (see below) to
    // catch finite as well as infinite values
    // and clamp index between zero and length.
    // except at() does special bounds checks.
    double str_len_dbl = str_len;
    if (which == 'a')
        str_len_dbl = js_nan.num;

    // get the two optional index parameters.
    // for the first parameter, the default
    // is zero; for the second, it is length.
    int32_t index[2] = { 0, str_len };
    for (int i = 0; i < 2; ++i) {
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        js_val arg_val = arg_ptr->value;
        if (!js_is_number(arg_val))
            arg_val = js_tonumber(env, arg_val);
        if (arg_val.num >= 0) {
            index[i] =  arg_val.num >= str_len_dbl
                     ?  str_len : arg_val.num;
        } else if (arg_val.num <= 0) {
            index[i] = -arg_val.num >= str_len_dbl
                     ? -str_len : arg_val.num;
        } else // NaN
            index[i] = 0;
    }

    //
    // transform the index parameters
    // according to particular function
    //
    switch (which) {

        // at, index[1] is not given
        case 'a':
            if (index[0] < 0) {
                index[0] += str_len;
                if (index[0] < 0)
                    return js_undefined;
            }
            if (index[0] >= str_len)
                return js_undefined;
            index[1] = index[0] + 1;
            break;

        // charAt, index[1] is not given
        case 'A':
            if (index[0] < 0
                        || index[0] >= str_len)
                return env->str_empty;
            index[1] = index[0] + 1;
            break;

        // slice, index[1] is end index
        case 'e':
            if (index[0] < 0)
                index[0] += str_len;
            if (index[1] < 0)
                index[1] += str_len;
            if (index[0] >= index[1])
                return env->str_empty;
            break;

        // substr, index[1] is length
        case 'r':
            if (index[0] < 0)
                index[0] += str_len;
            if (index[1] <= 0)
                return env->str_empty;
            index[1] += index[0];
            if (index[1] > str_len)
                index[1] = str_len;
            break;

        // substring, index[1] is end index
        case 'g':
            if (index[0] < 0)
                index[0] = 0;
            if (index[1] < 0)
                index[1] = 0;
            if (index[0] >= index[1]) {
                int tmp = index[0];
                if (tmp == index[1])
                    return env->str_empty;
                index[0] = index[1];
                index[1] = tmp;
            }
            break;

        default:
            fprintf(stderr, "Substr!\n");
            exit(1);
    }

    //
    // create and return a new substring
    //

    const wchar_t *str_txt = id->data + index[0];
    uint32_t new_len = (index[1] - index[0]) << 1;

    id = js_malloc(sizeof(objset_id) + new_len);
    id->len = new_len;
    id->flags = js_str_is_string;
    memcpy(id->data, str_txt, new_len);

    return js_gc_manage(env,
                js_make_primitive_string(id));
}

// ------------------------------------------------------------
//
// js_str_at
//
// ------------------------------------------------------------

static js_val js_str_at (js_c_func_args) {
    js_prolog_stack_frame();
    const js_val ret_val = js_str_sub_helper(
                    env, this_val, stk_args, 'a');
    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_str_charAt
//
// ------------------------------------------------------------

static js_val js_str_charAt (js_c_func_args) {
    js_prolog_stack_frame();
    const js_val ret_val = js_str_sub_helper(
                    env, this_val, stk_args, 'A');
    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_str_slice
//
// ------------------------------------------------------------

static js_val js_str_slice (js_c_func_args) {
    js_prolog_stack_frame();
    const js_val ret_val = js_str_sub_helper(
                    env, this_val, stk_args, 'e');
    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_str_substr
//
// ------------------------------------------------------------

static js_val js_str_substr (js_c_func_args) {
    js_prolog_stack_frame();
    const js_val ret_val = js_str_sub_helper(
                    env, this_val, stk_args, 'r');
    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_str_substring
//
// ------------------------------------------------------------

static js_val js_str_substring (js_c_func_args) {
    js_prolog_stack_frame();
    const js_val ret_val = js_str_sub_helper(
                    env, this_val, stk_args, 'g');
    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_str_concat_v
//
// ------------------------------------------------------------

static js_val js_str_concat_v (js_c_func_args) {
    js_prolog_stack_frame();

    // convert input 'this' value to string
    if (!js_is_primitive_string(this_val)) {
        js_throw_if_nullobj(env, this_val);
        this_val = js_tostring(env, this_val);
    }
    // add its length to the combined count
    objset_id *src_id = js_get_pointer(this_val);
    uint32_t dst_len = src_id->len;

    // convert each parameter to string, note
    // that undefined are null are allowed here
    js_link *arg_ptr = stk_args;
    for (;;) {
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        js_val arg_val = arg_ptr->value;
        if (!js_is_primitive_string(arg_val)) {
            arg_val = js_tostring(env, arg_val);
            arg_ptr->value = arg_val;
        }
        // add its length to the combined count
        src_id = js_get_pointer(arg_val);
        dst_len += src_id->len;
    }

    // if the combined length equals 'this' length
    // then there is nothing to actually combine
    src_id = js_get_pointer(this_val);
    if (dst_len == src_id->len)
        js_return(this_val);

    // allocate room for the combined string
    objset_id *dst_id =
        js_malloc(sizeof(objset_id) + dst_len);
    dst_id->len = dst_len;
    dst_id->flags = js_str_is_string;
    char *dst_ptr = (char *)dst_id->data;

    // append the (possibly stringified) 'this',
    // followed by all parameters, to the string
    arg_ptr = stk_args;
    for (;;) {
        memcpy(dst_ptr, src_id->data, src_id->len);
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        dst_ptr += src_id->len;
        src_id = js_get_pointer(arg_ptr->value);
    }

    js_return(js_gc_manage(env,
                js_make_primitive_string(dst_id)));
}

// ------------------------------------------------------------
//
// js_str_init_sup
//
// ------------------------------------------------------------

static void js_str_init_sup (js_environ *env) {

    js_val name;

    // shadow.js_str_print function
    js_newprop(env, env->shadow_obj,
            js_str_c(env, "js_str_print")) =
                js_unnamed_func(js_str_print, 1);

    // shadow.js_sym_util function
    js_newprop(env, env->shadow_obj,
            js_str_c(env, "js_sym_util")) =
                js_unnamed_func(js_sym_util, 1);

    // shadow.js_str_utf16 function
    js_newprop(env, env->shadow_obj,
            js_str_c(env, "js_str_utf16")) =
                js_unnamed_func(js_str_utf16, 1);

    // shadow.js_str_trim function
    js_newprop(env, env->shadow_obj,
            js_str_c(env, "js_str_trim")) =
                js_unnamed_func(js_str_trim, 2);

    // _shadow.str_sup
    js_val str_sup = js_emptyobj(env);
    js_newprop(env, env->shadow_obj,
            js_str_c(env, "str_sup")) = str_sup;

    // shadow.js_str_at function
    name = js_str_c(env, "at");
    js_newprop(env, str_sup, name) =
        js_newfunc(env, js_str_at, name, NULL,
               js_strict_mode | 1, /* closures */ 0);

    // shadow.js_str_charAt function
    name = js_str_c(env, "charAt");
    js_newprop(env, str_sup, name) =
        js_newfunc(env, js_str_charAt, name, NULL,
               js_strict_mode | 1, /* closures */ 0);

    // shadow.js_str_slice function
    name = js_str_c(env, "slice");
    js_newprop(env, str_sup, name) =
        js_newfunc(env, js_str_slice, name, NULL,
               js_strict_mode | 2, /* closures */ 0);

    // shadow.js_str_substr function
    name = js_str_c(env, "substr");
    js_newprop(env, str_sup, name) =
        js_newfunc(env, js_str_substr, name, NULL,
               js_strict_mode | 2, /* closures */ 0);

    // shadow.js_str_substring function
    name = js_str_c(env, "substring");
    js_newprop(env, str_sup, name) =
        js_newfunc(env, js_str_substring, name, NULL,
               js_strict_mode | 2, /* closures */ 0);

    // shadow.js_str_concat function
    name = js_str_c(env, "concat");
    js_newprop(env, str_sup, name) =
        js_newfunc(env, js_str_concat_v, name, NULL,
               js_strict_mode | 1, /* closures */ 0);
}
