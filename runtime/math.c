// ------------------------------------------------------------
//
// miscellaneous number support utilities
//
// ------------------------------------------------------------

// ------------------------------------------------------------
//
// js_math_func_impl
//
// ------------------------------------------------------------

#define js_math_func_impl(jsname,cname)                 \
    static js_val js_math_##jsname (js_c_func_args) {   \
        js_link *arg_ptr = stk_args->next;              \
        js_val x;                                       \
        if (unlikely(arg_ptr == js_stk_top))            \
            x = js_nan;                                 \
        else {                                          \
            x = arg_ptr->value;                         \
            if (unlikely(!js_is_number(x)))             \
                x = js_tonumber(env, x);                \
            x = js_make_number(cname(js_get_number(x)));\
        }                                               \
        js_return(x);                                   \
    }

// ------------------------------------------------------------

js_math_func_impl(abs, fabs)
js_math_func_impl(acos, acos)
js_math_func_impl(acosh, acosh)
js_math_func_impl(asin, asin)
js_math_func_impl(asinh, asinh)
js_math_func_impl(atan, atan)
js_math_func_impl(atanh, atanh)

// ------------------------------------------------------------
//
// js_math_atan2
//
// ------------------------------------------------------------

static js_val js_math_atan2 (js_c_func_args) {

    js_link *arg_ptr = stk_args->next;
    js_val x, y;
    if (unlikely(arg_ptr == js_stk_top))
        x = js_nan;
    else {
        y = arg_ptr->value;
        if (unlikely(!js_is_number(y)))
            y = js_tonumber(env, y);

        arg_ptr = arg_ptr->next;
        if (unlikely(arg_ptr == js_stk_top))
            x = js_nan;
        else {

            x = arg_ptr->value;
            if (unlikely(!js_is_number(x)))
                x = js_tonumber(env, x);
            x = js_make_number(
                    atan2(js_get_number(y),
                            js_get_number(x)));
        }
    }
    js_return(x);
}

// ------------------------------------------------------------
//
// js_math_init
//
// ------------------------------------------------------------

static void js_math_init (js_environ *env) {

    js_val name;

    js_val math = js_emptyobj(env);
    js_newprop(env, env->shadow_obj,
                            js_str_c(env, "math")) = math;

#define js_math_func_decl(jsname)                       \
    name = js_str_c(env, #jsname);                      \
    js_newprop(env, math, name) =                       \
        js_newfunc(env, js_math_##jsname, name, NULL,   \
               js_strict_mode | 1, /* closures */ 0);

    js_math_func_decl(abs);
    js_math_func_decl(acos);
    js_math_func_decl(acosh);
    js_math_func_decl(asin);
    js_math_func_decl(asinh);
    js_math_func_decl(atan);
    js_math_func_decl(atanh);
    js_math_func_decl(atan2);
}

// ------------------------------------------------------------

#undef js_math_func_decl
#undef js_math_func_impl
