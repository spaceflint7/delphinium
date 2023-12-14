
// ------------------------------------------------------------
//
// js-to-c runtime, private declarations
//
// ------------------------------------------------------------

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#undef environ // defined in stdlib.h

#include "include/intmap.h"
#include "include/objset.h"

#define included_from_runtime
#include "runtime.h"

// ------------------------------------------------------------
//
// macros and flags
//
// ------------------------------------------------------------

#define js_descr_value  0x01
#define js_descr_write  0x02
#define js_descr_getter 0x04
#define js_descr_setter 0x08
#define js_descr_enum   0x10
#define js_descr_config 0x20

// return size of exotic object structure
#define js_obj_struct_size(exotic_type)                         \
    (   (exotic_type) == js_obj_is_ordinary ? sizeof(js_obj)    \
      : (exotic_type) == js_obj_is_array    ? sizeof(js_arr)    \
      : (exotic_type) == js_obj_is_function ? sizeof(js_func)   \
      : -999999)

// number of properties to create in advance on an empty
// object in js_newexobj (), and number of properties to
// add in advance when shape changes in js_shape_switch ()
#define js_obj_grow_factor 3

// get the prototype of an object
#define js_obj_get_proto(obj_ptr) \
    ((js_obj *)((uintptr_t)(obj_ptr)->proto & ~7))

// current source file and line number
#define js_unnamed_func(c_func,num_args)            \
    js_newfunc(env, (c_func), env->str_empty, NULL, \
               js_strict_mode | (num_args), /* closures */ 0)

// ------------------------------------------------------------
//
// environment type
//
// ------------------------------------------------------------

typedef struct js_try js_try;

struct js_environ {

    // what we declare below is the public part at the start
    // of environment block, for use by the compiled program,
    // keep in sync with declaration in runtime.h

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

    int internal_flags;  // various jsf_xxx flags

    // end of the public section declared in runtime.h
    int last_public_field;
    int first_private_field;

    int next_unique_id;

    js_try *try_handler;
    js_val shadow_obj;
    objset *strings_set;
    js_shape *shape_private_object;
    js_val obj_constructor; // Object constructor

    js_obj *obj_proto;      // Object.prototype
    js_obj *str_proto;      // String.prototype
    js_obj *sym_proto;      // Symbol.prototype
    js_obj *num_proto;      // Number.prototype
    js_obj *big_proto;      // BigInt.prototype
    js_obj *bool_proto;     // Boolean.prototype

    // functions
    js_val func_arguments;  // js_arguments () in js runtime
    js_obj *func_proto;     // Function.prototype
    js_shape *func_shape1;  // shape af a new function object
    js_shape *func_shape2;  // shape of a new prototype object
    int func_length;
    int func_name;
    int func_prototype;
    js_val func_hasinstance;
    js_val func_bound_prefix;
    js_val func_new_coroutine;

    // descriptors
    js_shape *descr_shape;

    // arrays
    js_obj *arr_proto;      // Array.prototype
    js_shape *arr_shape;
    int arr_length;

    // symbols
    js_val sym_iterator;
    js_val sym_hasInstance;
    js_val sym_toPrimitive;
    js_val sym_unscopables;

    // number
    char *num_string_buffer;

    // iterator
    js_val for_in_iterator;
};

// ------------------------------------------------------------
//
// other source files
//
// ------------------------------------------------------------

#include "decls.c"

#include "mem.c"
#include "str.c"
#include "shape.c"
#include "descr1.c"
#include "descr2.c"
#include "newobj.c"
#include "obj.c"
#include "objsup.c"
#include "prop1.c"
#include "prop2.c"
#include "with.c"
#include "num.c"
#include "stack.c"
#include "coroutine.c"
#include "func.c"
#include "arr.c"
#include "big1.c"
#include "big2.c"
#include "mix.c"
#include "except.c"
#include "iter.c"
#include "init.c"
#include "debug.c"

#include "include/intmap.c"
#include "include/objset.c"
