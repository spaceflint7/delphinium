
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

static js_val js_tonumber (js_environ *env, js_val val);

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

#define js_emptyobj(env) js_newobj((env), (env)->shape_empty)

#define js_newprop(env,obj,prop) *js_ownprop((env),(obj),(prop),true)

static void *js_newexobj (js_environ *env, js_obj *proto,
                          const js_shape *shape);

static js_val *js_ownprop (
    js_environ *env, js_val obj, js_val prop, bool can_add);

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

static bool js_throw_if_not_extensible (
                js_environ *env, js_val obj, js_val prop,
                bool always_throw_or_only_if_strict_mode);

static void js_throw_if_strict (
                js_environ *env, const char *func_name, js_val arg);

#define js_throw_if_strict_0(func_name) \
    js_throw_if_strict(env,func_name,env->str_empty)

#define js_throw_if_strict_1(func_name,func_arg) \
    js_throw_if_strict(env,func_name,func_arg)

// ------------------------------------------------------------
//
// gc.c
//
// ------------------------------------------------------------

static js_val js_gc_manage (js_environ *env, js_val val);

static void js_gc_notify (js_environ *env, js_val val);

static void js_gc_free (js_environ *env, void *ptr);

static void js_gc_mark_val (js_gc_env *gc, js_val val);

// ------------------------------------------------------------
//
// platform
//
// ------------------------------------------------------------

struct js_thread *js_thread_new (
                        void (*func)(void *), void *arg);

struct js_mutex *js_mutex_new ();
void js_mutex_enter (struct js_mutex *mutex);
void js_mutex_leave (struct js_mutex *mutex);

struct js_event *js_event_new ();
void js_event_post (struct js_event *event);
void js_event_wait (struct js_event *event,
                    struct js_mutex *mutex,
                    uint32_t timeout_in_ms_or_minus1);

struct js_lock *js_lock_new ();
void js_lock_free (struct js_lock *lock);
void js_lock_enter_shr (struct js_lock *lock);
void js_lock_leave_shr (struct js_lock *lock);
void js_lock_enter_exc (struct js_lock *lock);
void js_lock_leave_exc (struct js_lock *lock);

// interlocked compare and swap with memory barrier
uint16_t js_compare_and_swap_16 (
    void *ptr, uint16_t and_mask, uint16_t or_bits);
uint32_t js_compare_and_swap_32 (
    void *ptr, uint32_t and_mask, uint32_t or_bits);

//void js_platform_init ();
uint64_t js_current_time ();
//uint64_t js_elapsed_time ();

// ------------------------------------------------------------
//
// debug
//
// ------------------------------------------------------------

static void js_print (js_environ *env, js_val val);
