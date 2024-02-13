// ------------------------------------------------------------
//
// math library
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
            x = js_make_number(cname((x).num));         \
        }                                               \
        js_return(x);                                   \
    }

// ------------------------------------------------------------

js_math_func_impl(abs,    fabs)
js_math_func_impl(cbrt,   cbrt)
js_math_func_impl(ceil,   ceil)
js_math_func_impl(exp,    exp)
js_math_func_impl(expm1,  expm1)
js_math_func_impl(floor,  floor)
js_math_func_impl(fround, (float))
js_math_func_impl(round,  js_round)
js_math_func_impl(sign,   js_sign)
js_math_func_impl(sqrt,   sqrt)
js_math_func_impl(trunc,  trunc)

js_math_func_impl(log,   log)
js_math_func_impl(log1p, log1p)
js_math_func_impl(log2,  log2)
js_math_func_impl(log10, log10)

js_math_func_impl(acos,  acos)
js_math_func_impl(acosh, acosh)
js_math_func_impl(asin,  asin)
js_math_func_impl(asinh, asinh)
js_math_func_impl(atan,  atan)
js_math_func_impl(atanh, atanh)
js_math_func_impl(cos,   cos)
js_math_func_impl(cosh,  cosh)
js_math_func_impl(sin,   sin)
js_math_func_impl(sinh,  sinh)
js_math_func_impl(tan,   tan)
js_math_func_impl(tanh,  tanh)

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
// js_math_clz32
//
// ------------------------------------------------------------

static js_val js_math_clz32 (js_c_func_args) {

    uint32_t x = 0;
    js_link *arg_ptr = stk_args->next;
    if (likely(arg_ptr != js_stk_top)) {
        js_val x_val = arg_ptr->value;
        if (unlikely(!js_is_number(x_val)))
            x_val = js_tonumber(env, x_val);
        x = (uint32_t)x_val.num;
    }
    x = (!x) ? 32 : __builtin_clz(x);
    js_return(js_make_number((double)x));
}

// ------------------------------------------------------------
//
// js_math_hypot
//
// ------------------------------------------------------------

static js_val js_math_hypot (js_c_func_args) {

    js_link *arg_ptr = stk_args->next;
    js_val x, y;
    if (unlikely(arg_ptr == js_stk_top))
        x = y = js_make_number(0);
    else {
        x = arg_ptr->value;
        if (unlikely(!js_is_number(x)))
            x = js_tonumber(env, x);

        arg_ptr = arg_ptr->next;
        if (unlikely(arg_ptr == js_stk_top))
            y = js_make_number(0);
        else {
            y = arg_ptr->value;
            if (unlikely(!js_is_number(y)))
                y = js_tonumber(env, y);

            arg_ptr = arg_ptr->next;
            if (likely(arg_ptr == js_stk_top)) {

                // typical case of two arguments
                js_return(js_make_number(
                            hypot(x.num, y.num)));
            }
        }
    }

    // if not called with exactly two arguments
    bool is_inf = isinf(x.num);
    bool is_nan = isnan(x.num);
    double scale = fabs(x.num);
    double sumsq = 1;
    for (;;) {

        is_inf |= isinf(y.num);
        is_nan |= isnan(y.num);
        double next = fabs(y.num);
        if (scale < next) {
            const double z = scale / next;
            sumsq = 1 + sumsq * z * z;
            scale = next;
        } else if (scale != 0) {
            const double z = next / scale;
            sumsq += z * z;
        }

        if (likely(arg_ptr == js_stk_top))
            break;

        y = arg_ptr->value;
        if (unlikely(!js_is_number(y)))
            y = js_tonumber(env, y);

        arg_ptr = arg_ptr->next;
    }

    double r = (is_inf ? INFINITY
             : (is_nan ? NAN
             : (scale * sqrt(sumsq))));
    js_return(js_make_number(r));
}

// ------------------------------------------------------------
//
// js_math_imul
//
// ------------------------------------------------------------

static js_val js_math_imul (js_c_func_args) {

    int32_t r = 0;
    js_link *arg_ptr = stk_args->next;
    if (likely(arg_ptr != js_stk_top)) {
        js_val x_val = arg_ptr->value;
        if (unlikely(!js_is_number(x_val)))
            x_val = js_tonumber(env, x_val);

        arg_ptr = arg_ptr->next;
        if (likely(arg_ptr != js_stk_top)) {
            js_val y_val = arg_ptr->value;
            if (unlikely(!js_is_number(y_val)))
                y_val = js_tonumber(env, y_val);

            r = (uint32_t)x_val.num
              * (uint32_t)y_val.num;
        }
    }
    js_return(js_make_number((double)r));
}

// ------------------------------------------------------------
//
// js_math_max
//
// ------------------------------------------------------------

static js_val js_math_max (js_c_func_args) {

    double r = -INFINITY;
    js_link *arg_ptr = stk_args;
    for (;;) {
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        js_val x = arg_ptr->value;
        if (unlikely(!js_is_number(x)))
            x = js_tonumber(env, x);
        if (x.num > r || x.num != x.num)
            r = x.num;
    }
    js_return(js_make_number(r));
}

// ------------------------------------------------------------
//
// js_math_min
//
// ------------------------------------------------------------

static js_val js_math_min (js_c_func_args) {

    double r = INFINITY;
    js_link *arg_ptr = stk_args;
    for (;;) {
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        js_val x = arg_ptr->value;
        if (unlikely(!js_is_number(x)))
            x = js_tonumber(env, x);
        if (x.num < r || x.num != x.num)
            r = x.num;
    }
    js_return(js_make_number(r));
}

// ------------------------------------------------------------
//
// js_math_pow
//
// ------------------------------------------------------------

static js_val js_math_pow (js_c_func_args) {

    js_val r = js_nan;
    js_link *arg_ptr = stk_args->next;
    if (likely(arg_ptr != js_stk_top)) {
        js_val x_val = arg_ptr->value;
        if (unlikely(!js_is_number(x_val)))
            x_val = js_tonumber(env, x_val);

        arg_ptr = arg_ptr->next;
        if (likely(arg_ptr != js_stk_top)) {
            js_val y_val = arg_ptr->value;
            if (unlikely(!js_is_number(y_val)))
                y_val = js_tonumber(env, y_val);

            r = js_make_number(
                    js_pow(x_val.num, y_val.num));
        }
    }
    js_return(r);
}

// ------------------------------------------------------------
//
// js_math_random
//
// ------------------------------------------------------------

static js_val js_math_random (js_c_func_args) {

    // https://en.wikipedia.org/wiki/Xorshift
    uint64_t x = env->math_random_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    env->math_random_state = x;
    const double r = (double)x
                   / (double)(uint64_t)-1ULL;
    js_return(js_make_number(r));
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

#define js_math_func_decl_n(jsname,n)                   \
    name = js_str_c(env, #jsname);                      \
    js_newprop(env, math, name) =                       \
        js_newfunc(env, js_math_##jsname, name, NULL,   \
               js_strict_mode | (n), /* closures */ 0);

#define js_math_func_decl(jsname) \
                            js_math_func_decl_n(jsname,1)

    js_math_func_decl(abs);
    js_math_func_decl(cbrt);
    js_math_func_decl(ceil);
    js_math_func_decl(clz32);
    js_math_func_decl(exp);
    js_math_func_decl(expm1);
    js_math_func_decl(floor);
    js_math_func_decl(fround);
    js_math_func_decl_n(hypot, 2);
    js_math_func_decl_n(imul, 2);
    js_math_func_decl_n(max, 2);
    js_math_func_decl_n(min, 2);
    js_math_func_decl_n(pow, 2);
    js_math_func_decl(random);
    js_math_func_decl(round);
    js_math_func_decl(sign);
    js_math_func_decl(sqrt);
    js_math_func_decl(trunc);

    js_math_func_decl(log);
    js_math_func_decl(log1p);
    js_math_func_decl(log2);
    js_math_func_decl(log10);

    js_math_func_decl(acos);
    js_math_func_decl(acosh);
    js_math_func_decl(asin);
    js_math_func_decl(asinh);
    js_math_func_decl(atan);
    js_math_func_decl(atanh);
    js_math_func_decl(atan2);
    js_math_func_decl(cos);
    js_math_func_decl(cosh);
    js_math_func_decl(sin);
    js_math_func_decl(sinh);
    js_math_func_decl(tan);
    js_math_func_decl(tanh);

    env->math_random_state = js_current_time();
}

// ------------------------------------------------------------

#undef js_math_func_decl
#undef js_math_func_decl_n
#undef js_math_func_impl
