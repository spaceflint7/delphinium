
// ------------------------------------------------------------
//
// js-to-c runtime, public declarations
//
// ------------------------------------------------------------

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include <math.h>

//
// javascript value type.  contains either a double-precision
// floating point number, or a NaN-tagged pointer in format:
//  63 62 . . . . 52 51 50 49 48 47  .  .  .  .  .  0
//   x  eeeeeeeeeee  q  d  x  x  pppppppppppppppppppp
// sign bit 63 is ignored; exponent bits 52-62 all set;
// quiet NaN bit 51 set; and bit 50 set, to flag this as
// a pointer value, as opposed to a real NaN value.
//

typedef union js_val { double num; uint64_t raw; } js_val;

#define js_exponent_mask        0x7FF0000000000000L
#define js_quiet_nan_bit        0x0008000000000000L
#define js_dynamic_type_bit     0x0004000000000000L
#define js_object_bit           0x0002000000000000L
#define js_primitive_bit        0x0001000000000000L
#define js_dynamic_type_mask    (js_exponent_mask | js_quiet_nan_bit | js_dynamic_type_bit)

// if any bit 50..62 is clear, then this is a number
#define js_is_number(v)         (~((v).raw) & js_dynamic_type_mask)
#define js_get_number(v)        ((v).num)
#define js_make_number(n)       ((js_val){ .num = n })
#define js_nan                  ((js_val){ .raw = js_exponent_mask | js_quiet_nan_bit })

// utility macros to handle NaN-boxed pointers
#define js_pointer_mask         0xFFFFFFFFFFF8ULL
#define js_is_pointer(ty,v)     (!(~((v).raw) & (js_dynamic_type_mask | (ty))))
#define js_get_pointer(v)       ((void *)((v).raw & js_pointer_mask))
#define js_make_pointer(ty,ptr) ((js_val){ .raw = js_dynamic_type_mask | (ty) | (uint64_t)(uintptr_t)(ptr) })

// pointer to object when NaN-boxing bits and js_object_bit are set
#define js_is_object(v)           js_is_pointer(js_object_bit | 1L,v)
#define js_make_object(p)       js_make_pointer(js_object_bit | 1L,p)

// if bit 1 is set, then it is a flagged pointer
#define js_is_flagged_pointer(v)      js_is_pointer(js_object_bit | 2L | 1L,v)
#define js_make_flagged_pointer(p)  js_make_pointer(js_object_bit | 2L | 1L,p)

// pointer to primitive when NaN-boxing bits and js_primitive_bit are set
#define js_is_primitive(v)      js_is_pointer(js_primitive_bit,v)
#define js_get_primitive_type(v) (((int)(v).raw) & 7)
#define js_make_primitive(p,ty) js_make_pointer(js_primitive_bit | (ty),p)
#define js_prim_is_string       0
#define js_prim_is_symbol       1
#define js_prim_is_bigint       2

// test if js_dynamic_type_mask and either js_object_bit or js_primitive_bit
#define js_is_object_or_primitive(v) (((((v).raw >> 48) & 0x7FFF) - 0x7FFD) <= 1)

// special value when neither js_object_bit nor js_primitive_bit is set.
#define js_deleted              ((js_val){ .raw = js_dynamic_type_mask | 0x0002 })
#define js_undefined            ((js_val){ .raw = js_dynamic_type_mask | 0x0020 })
#define js_null                 ((js_val){ .raw = js_dynamic_type_mask | 0x0040 })
#define js_false                ((js_val){ .raw = js_dynamic_type_mask | 0x0100 })
#define js_true                 ((js_val){ .raw = js_dynamic_type_mask | 0x0101 })

// check if a value is either js_undefined or js_deleted
#define js_is_undefined(v)      (((v).raw ^ (js_deleted.raw | js_undefined.raw)) <= 0x22)
#define js_is_undefined_or_null(v) \
                  (((v).raw ^ (js_deleted.raw | js_undefined.raw | js_null.raw)) <= 0x42)

// check if a value is either js_false or js_true
#define js_bool_test_mask       (js_dynamic_type_mask | js_object_bit | js_primitive_bit | 0xFFFFFFFFFFFEL)
#define js_bool_test_value      (js_dynamic_type_mask | 0x0100L)
#define js_is_boolean(v)        ((((v).raw) & js_bool_test_mask) == js_bool_test_value)

// this flag may be passed to js_newfunc in the arity parameter
#define js_strict_mode          0x80000000U
#define js_not_constructor      0x40000000U

// these special values may be passed to js_newobj2 ()
#define js_next_is_getter       ((js_val){ .raw = js_dynamic_type_mask | 0x1111 })
#define js_next_is_setter       ((js_val){ .raw = js_dynamic_type_mask | 0x1112 })
#define js_next_is_spread       ((js_val){ .raw = js_dynamic_type_mask | 0x1113 })

// this special value may be passed to js_scopewith ()
#define js_discard_scope        ((js_val){ .raw = js_dynamic_type_mask | 0x1121 })

// special value to mark uninitialized state, used internally
#define js_uninitialized        ((js_val){ .raw = js_dynamic_type_mask | 0x1141 })

// check if both operands are numbers.  note the use of
// a bitwise operator (|) and not an arithmetic operator
// (like +), because compiler optimizations or unknown
// hardware might modify NaN bits during arithmetic.
#define js_are_both_numbers(v1,v2) \
    (~(v1.raw | v2.raw) & js_dynamic_type_mask)

//
// check if value is a specific type of primitive
//

#define js_is_primitive_string(v) \
    (js_is_primitive(v) && js_get_primitive_type(v) == js_prim_is_string)

#define js_is_primitive_symbol(v) \
    (js_is_primitive(v) && js_get_primitive_type(v) == js_prim_is_symbol)

#define js_is_primitive_bigint(v) \
    (js_is_primitive(v) && js_get_primitive_type(v) == js_prim_is_bigint)

//
// truthy/falsy check:  if v > 0 with exponent 0x7FF then
// it is a boxed value or NaN/Infinity, which we quick-
// check against objects or special values, or send it to
// a slower function call.  for a number v, compare to 0.
//
// the macro js_is_special_value_or_object selects a mask
// of zero (when js_object_bit is set) or 0x1FFFFFFFFFFFE,
// and compares with 0x100 (representing js_true/js_false).
// this ensures that only objects and special values pass.
// then truthy is determined by bit 0, which is set in all
// objects, and for js_true.
//

#define js_is_special_value_or_object(v) \
    (((v).raw & 0x1FFFFFFFFFFFEL & (((int64_t)((v).raw ^ js_object_bit) << 15) >> 63)) <= 0x100)

#define js_is_truthy(v) \
    ( likely(!(~((v).raw) & js_exponent_mask)) \
        ? (   likely(js_is_special_value_or_object(v)) \
            ? ((v).raw & 1) : js_check_truthy(v)) \
        : (0 != ((v).raw << 1)))

/* xxx */ #define js_is_falsy(v) (!js_is_truthy(v))

bool js_check_truthy (js_val val);

//
// forward declarations
//

typedef struct js_environ js_environ;

typedef struct js_link js_link;

//
// string
//

#define js_str_is_string   1
#define js_str_is_symbol   2
#define js_str_in_objset   4
#define js_str_is_static   8

// define a string from a const memory layout declared as:
// static wchar_t [] { len, len, flg, char, char, ... }
// this precisely matches the memory layout of objset_id.
// the runtime assumes ownership of the passed string.
js_val js_newstr (js_environ *env, const wchar_t *ptr);

#define js_newstr_prefix(len,intern) \
        (wchar_t)(len << 1), (wchar_t)(len >> 15), \
        (js_str_is_string | js_str_is_static | \
            ((intern) * js_str_in_objset))

//
// object
//

typedef struct js_shape js_shape;

typedef struct js_obj js_obj;
struct js_obj {

    js_obj *proto;
    js_val *values;         // object properties
    js_shape *shape;
    int shape_id;           // not always same as shape->shape_id
    int max_values;         // bit 31 set if object not extensible
};

// low 3 bits of js_obj.proto field indicate exotic objects
#define js_obj_is_exotic(obj,ty) ((*(uintptr_t *)(obj) & 7) == (ty))
#define js_obj_is_ordinary  0
#define js_obj_is_array     1
#define js_obj_is_function  2
#define js_obj_is_proxy     3
#define js_obj_is_dataview  4
#define js_obj_is_private   5

js_val js_newobj (js_environ *env, const js_shape *shape, ...);

js_val js_newobj2 (js_environ *env, js_val obj, js_val proto,
                    int num_props, ...);

js_val js_restobj (js_environ *env, js_val from_obj,
                   int skip_count, ...);

//
// array
//

typedef struct js_arr js_arr;
struct js_arr {

    js_obj super;
    js_val *values;         // array cells
    uint32_t length;        // integer copy of length property
    uint32_t capacity;      // number of allocated cells
    js_val length_descr[2];
};

js_val js_newarr (js_environ *env, int num_values, ...);

js_val js_restarr_stk (js_environ *env, js_link *stk_ptr);

js_val js_restarr_iter (js_environ *env, js_val *iterator);

#define js_arr_max_length 0xFFFFFFFAU

//
// bigint
//

js_val js_newbig (js_environ *env, int len, uint32_t *ptr);

//
//  'try' exception handler
//

jmp_buf *js_entertry (js_environ *env);

js_val js_leavetry (js_environ *env);

#ifdef __GNUC__
__attribute__((noreturn))
#endif
js_val js_throw (js_environ *env, js_val throw_val);

//
// stack link
//

struct js_link {
    js_val value;
    js_link *next;
    js_link *prev;
    int depth;
};

#define js_link_allocated   0x40000000U
#define js_link_depth(link) ((link)->depth & 0x3FFFFFFFU)

void js_growstack (js_environ *env, js_link *stk, int needed);

#define js_stk_top (env->stack_top)

#define js_ensure_stack_at_least(n)             \
    if (unlikely(env->stack_size -              \
            js_link_depth(js_stk_top) < (n)))   \
        js_growstack(env, js_stk_top, (n));

// function entry prolog, set up stack frame
#define js_prolog_stack_frame() \
    stk_args->value.raw = func_val.raw | 2

// reset the flagged function object on the stack,
// restore stack pointer to the caller-saved value,
// and return the specified return value
#define js_return(retval) {                         \
    js_val js_return_tmp = (retval);                \
    (js_stk_top = stk_args)->value = js_undefined;  \
    return js_return_tmp; }

js_val js_arguments (js_environ *env,
                     js_val func_val, js_link *stk_args);

void js_arguments2 (js_environ *env, js_val func_val,
                    js_val args_val, js_link *stk_args);

void js_spreadargs (js_environ *env, js_val iterable);

void js_newiter (js_environ *env, js_val *new_iter,
                 int kind, js_val iterable_expr);

void js_nextiter1 (js_environ *env, js_val *iter);

bool js_nextiter2 (js_environ *env, js_val *iter,
                   int cmd, js_val arg);

//
// function
//

#define js_c_func_args \
    js_environ *env, js_val func_val, js_val this_val, js_link *stk_args

typedef js_val (*js_c_func) (js_c_func_args);

typedef struct js_func js_func;
struct js_func {

    js_obj super;
    js_c_func c_func;
    int64_t *shape_cache;
    js_val **closure_array;
    js_val *closure_temps;
    union {
        js_shape *new_shape;
        js_link *with_scope;
    } u;
    int closure_count;
    int flags;
    const char *where;
};

js_val js_newfunc (js_environ *env, js_c_func c_func,
                   js_val name, const char *where,
                   int arity, int closures, ...);

js_val js_callnew (js_environ *env, js_val func_val,
                   js_link *stk_args);

js_val js_callfunc (js_c_func_args);

js_val *js_newclosure (js_val func_val, js_val *old_val);

js_val *js_closureval (js_environ *env,
                       js_val func_val, int closure_index);

js_val js_newcoroutine (js_environ *env,
                        int kind, js_val func);

js_val js_yield (js_environ *env, js_val val);

js_val js_yield_star (js_environ *env, js_val iterable_val);

//
// object/array
//

js_val js_getprop (js_environ *env, js_val obj, js_val prop,
                   int64_t *shape_cache);

js_val js_setprop (js_environ *env, js_val obj,
                   js_val prop, js_val value,
                   int64_t *shape_cache);

js_val js_delprop (js_environ *env, js_val obj, js_val prop);

bool js_hasprop (js_environ *env, js_val obj, js_val prop);

bool js_instanceof (js_environ *env,
                    js_val inst_val, js_val func_val);

//
// unqualified globals, 'with' keyword
//

js_val js_scopewith (js_environ *env, js_val func, js_val obj);

js_val js_getwith (js_environ *env, js_val func,
                   js_val prop, js_val *ptr_local2);

js_val js_setwith (js_environ *env, js_val func,
                   js_val prop, js_val *ptr_local2,
                   js_val value);

js_val js_delwith (js_environ *env, js_val func, js_val prop);

js_val js_callwith (js_environ *env, js_val func,
                    js_val prop, js_val *ptr_local2,
                    js_link *stk_args);

//
// javascript functions
//

js_val js_unary_op (js_environ *env, int op, js_val val);

js_val js_binary_op (js_environ *env, int op,
                     js_val left, js_val right);

js_val js_typeof (js_environ *env, js_val val);

bool js_strict_eq (js_environ *env,
                   js_val left, js_val right);

bool js_loose_eq (js_environ *env,
                   js_val left, js_val right);

bool js_less_than (js_environ *env, int flags,
                   js_val left, js_val right);

js_val js_boxthis (js_environ *env, js_val this_val);

// ------------------------------------------------------------
//
// well-known strings.  see also js_str_init ()
//
// ------------------------------------------------------------

#define well_known_strings      \
    js_val str_arguments;       \
    js_val str_bigint;          \
    js_val str_boolean;         \
    js_val str_callee;          \
    js_val str_caller;          \
    js_val str_configurable;    \
    js_val str_constructor;     \
    js_val str_default;         \
    js_val str_done;            \
    js_val str_enumerable;      \
    js_val str_false;           \
    js_val str_function;        \
    js_val str_get;             \
    js_val str_length;          \
    js_val str_name;            \
    js_val str_next;            \
    js_val str_null;            \
    js_val str_number;          \
    js_val str_object;          \
    js_val str_prototype;       \
    js_val str_return;          \
    js_val str_set;             \
    js_val str_string;          \
    js_val str_symbol;          \
    js_val str_throw;           \
    js_val str_toString;        \
    js_val str_true;            \
    js_val str_undefined;       \
    js_val str_value;           \
    js_val str_valueOf;         \
    js_val str_writable;        \
                                \
    js_val str_nan;             \
    js_val str_empty;           // last one

// ------------------------------------------------------------
//
// environment and initialization
//
// ------------------------------------------------------------

#ifndef included_from_runtime

struct js_environ {

    // what we declare below is the public part at the start
    // of environment block, for use by the compiled program

    js_val global_obj;      // global object

    js_shape *shape_empty;
    js_val obj_proto_val;   // Object.prototype

    // if array js_obj->proto equals this, then we
    // can try fast-path optimization on array access
    js_obj *fast_arr_proto;

    // dummy shape cache field, if no shape cache variable
    int64_t dummy_shape_cache;

    // current value of the new.target meta property
    js_val new_target;

    // hint used when a function call fails, see also
    // write_func_name_hint () in expression_writer.js
    js_val func_name_hint;

    // shapes of an iterator result object
    js_shape *shape_value_done;
    js_shape *shape_done_value;

    js_link *stack_top;
    int stack_size;

       well_known_strings
#undef well_known_strings

    js_val big_zero;

    // end of the public section declared in runtime.h
    int last_public_field;
};

#endif

#define js_version_number 0x01
#define js_version_code ((offsetof(js_environ,last_public_field)<< 8)   \
                      |  (offsetof(js_arr,values)               << 16)  \
                      |  (offsetof(js_func,c_func)              << 24)  \
                      |  js_version_number            /* << 0*/)

//
// math helpers
//

/*__forceinline double js_hypot (double x, double y) {
    return ((isfinite(x) && isfinite(y))
                ? hypot(x, y) : js_nan.num);}*/

__forceinline double js_pow (double x, double y) {
    return (    (y == 0) ? 1
             :  ((!isfinite(y)
                        && (x == 1 || x == -1))
                    ? js_nan.num
             :  pow(x, y)));
}

__forceinline double js_round (double x) {
    return copysign(floor(x + 0.5), x);
}

__forceinline double js_sign (double x) {
    return (    (x == 0 || x != x) ? x
             : ((x < 0) ? -1 : 1));
}

//
// code generation macros
//

#if defined(__GNUC__)
#define likely(x) (__builtin_expect(!!(x),1))
#define unlikely(x) (__builtin_expect(!!(x),0))

#else //!__GNUC__
#define likely(x) (x)
#define unlikely(x) (x)
#endif
