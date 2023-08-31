
// ------------------------------------------------------------
//
// str.c
//
// ------------------------------------------------------------

static js_val js_tostring (js_environ *env, js_val val);

// ------------------------------------------------------------
//
// num.c
//
// ------------------------------------------------------------

static js_val js_num_tostring (
                    js_environ *env, js_val val, int radix);

// ------------------------------------------------------------
//
// big.c
//
// ------------------------------------------------------------

static js_val js_big_from_num (js_environ *env, js_val input);

static js_val js_big_from_str (js_environ *env, js_val input);

static bool js_big_check_truthy (js_val value);

static js_val js_big_tostring (
                    js_environ *env, js_val val, int radix);

static js_val js_big_unary_negate (js_environ *env,
                                   js_val input,
            uint32_t zero_for_negate_or_one_for_bitwise_not);

// ------------------------------------------------------------
//
// obj.c
//
// ------------------------------------------------------------

static js_val js_obj_to_primitive_string (
                            js_environ *env, js_val obj_val);

// ------------------------------------------------------------
//
// arr.c
//
// ------------------------------------------------------------

static js_val js_arr_get (js_val obj, uint32_t prop_idx);

static void js_arr_set (js_environ *env, js_val obj,
                        uint32_t prop_idx, js_val value);

static uint32_t js_arr_check_length (
                        js_environ *env, js_val value);

// ------------------------------------------------------------
//
// func.c
//
// ------------------------------------------------------------

static js_val js_callfunc1 (
                        js_environ *env, js_val func_val,
                        js_val this_val, js_val arg_val);

static js_val js_callshadow (js_environ *env,
                    const char *func_name, js_val arg_val);

#define js_callthrow(func_name) \
                    js_callshadow(env,func_name,js_undefined)

// ------------------------------------------------------------
//
// obj.c
//
// ------------------------------------------------------------

#define js_emptyobj(env) js_newobj((env), (env)->shape_empty)

#define js_newprop(env,obj,prop) *js_ownprop((env),(obj),(prop),true)

static void *js_newexobj (
                    js_obj *proto, const js_shape *shape);

static js_val *js_ownprop (
    js_environ *env, js_val obj, js_val prop, bool can_add);

// ------------------------------------------------------------
//
// prop2.c
//
//
// ------------------------------------------------------------

static bool js_setprop_array_length (
                js_environ *env, js_val obj, js_val value);

// ------------------------------------------------------------
//
// stack.c
//
// ------------------------------------------------------------

static void js_throw_if_notfunc (
                        js_environ *env, js_val func_val);

static void js_throw_if_notobj (js_environ *env, js_val obj_val);

static void js_throw_if_nullobj (js_environ *env, js_val obj_val);

static void js_throw_if_not_extensible (js_environ *env, js_val obj);

static void js_throw_if_strict (
                js_environ *env, const char *func_name, js_val arg);

#define js_throw_if_strict_0(func_name) \
    js_throw_if_strict(env,func_name,env->str_empty)

#define js_throw_if_strict_1(func_name,func_arg) \
    js_throw_if_strict(env,func_name,func_arg)

// ------------------------------------------------------------
//
// debug
//
// ------------------------------------------------------------

static void js_print (js_environ *env, js_val val);
