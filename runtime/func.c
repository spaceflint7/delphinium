
// ------------------------------------------------------------
//
// functions
//
// ------------------------------------------------------------

#define jsf_abort_if_non_strict 1
#define jsf_not_constructor     2

struct js_closure_var {

    js_val value;
    js_val owner_or_ref_count;
    js_val *next;
};

// ------------------------------------------------------------
//
// js_newfunc
//
/// allocate and initialize a new function value.
//
// ------------------------------------------------------------

js_val js_newfunc (js_environ *env, js_c_func c_func,
                   js_val name, const char *where,
                   int arity, int closures, ...) {

    js_func *func = js_newexobj(env,
                        env->func_proto, env->func_shape1);
    func->c_func = c_func;

    // source location, a diagnostic aid for stack traces,
    // see also function_expression () in function_writer.js
    func->where = where;

    // arity may include some flag bits
    func->flags = arity & (js_strict_mode | js_not_constructor);

    // during initialization, always 'js_not_constructor'
    if (unlikely(env->internal_flags & jsf_not_constructor))
        func->flags |= js_not_constructor;

    // default shape for 'this' is null, see also js_callnew ()
    // (also note that 'with_scope' occupies the same space.)
    func->u.new_shape = NULL;

    // closures also includes the number of shape cache slots.
    // shape cache slots are tracked by get_shape_variable ()
    // in utils_c.js, and passed to this function through
    // function_expression () in expression_writer.js.
    // note the count takes up the high 16-bits in 'closures'.
    const int shape_caches = (closures >> 16) & 0xFFFF;
    if (!shape_caches)
        func->shape_cache = NULL;
    else {
        func->shape_cache =
                js_calloc(shape_caches, sizeof(int64_t));
        closures &= 0xFFFF;
    }

    // record closure variables in the function object.
    // note that the first (N = closures argument) entries
    // are zero, and the last N are the actual pointers.
    // see also:  js_closureval below.

    if ((func->closure_count = closures) != 0) {

        func->closure_array =
                js_calloc(closures * 2, sizeof(js_val *));

        // populate the last N elements with pointers
        va_list args;
        va_start(args, closures);
        js_val **closure_ptr = &func->closure_array[closures];
        while (closures-- > 0) {
            struct js_closure_var *closure_var =
                        va_arg(args, struct js_closure_var *);

            /*if (js_is_object(closure_var->owner_or_ref_count)) {
                xxx unlink closure from owner temps
            }*/

            *closure_ptr++ = &closure_var->value;
        }
        va_end(args);

    } else
        func->closure_array = NULL;

    func->closure_temps = NULL;

    // set up the two initial properties in a function object:
    js_val *values = func->super.values;
    const js_val zero = js_make_number(0.0);

    //
    // length { value: (arity), writable: false,
    //          enumerable: false, configurable: true }
    arity &= ~(js_strict_mode | js_not_constructor);
    values[0 /* length */] =
        js_make_descriptor(js_newdescr(
            js_descr_value | js_descr_config,
            js_make_number(arity), zero));
    //
    // name { value: (string), writable: false,
    //          enumerable: false, configurable: true }
    if (!js_is_primitive_string(name))
        name = env->str_empty;
    values[1 /* name */] =
        js_make_descriptor(js_newdescr(
            js_descr_value | js_descr_config,
            name, zero));

    //
    // in non-strict mode, function object has 'arguments'
    // and 'caller' properties, which are usually null,
    // except while the function is executing;  this is
    // handled by js_arguments (), js_arguments2 (), and
    // variable_declaration_special_node ()
    // in statement_writer.js
    //

    values[2 /* prototype */] = js_deleted;

    js_val ret_func =
            js_gc_manage(env, js_make_object(func));

    if (!(func->flags & js_strict_mode)) {

        // helper code to detect any non-strict functions
        // declared during initialization, and abort
        if (env->internal_flags & jsf_abort_if_non_strict) {
            fprintf(stderr, "Non-strict function!\n");
            exit(1); // __builtin_trap();
        }

        js_arguments2(env, ret_func, js_null, NULL);

        // calling js_arguments2 () triggers a shape change
        values = func->super.values;
    }

    //
    // prototype { value: (object), writable: true,
    //          enumerable: false, configurable: false }
    //
    // and the new object referenced by this property has
    //
    // constructor { value: (function), writable: true,
    //          enumerable: false, configurable: true }
    //

    if (!(func->flags & js_not_constructor)) {

        js_obj *new_obj = js_newexobj(
                env, env->obj_proto, env->func_shape2);

        new_obj->values[0] = js_make_descriptor(
            js_newdescr(js_descr_value | js_descr_write
                      | js_descr_config, ret_func, zero));

        js_val new_proto =
            js_gc_manage(env, js_make_object(new_obj));

        values[2 /* prototype */] =
                js_make_descriptor(js_newdescr(
                    js_descr_value | js_descr_write,
                    new_proto, zero));

        // notify due to property set, see js_setprop ()
        js_gc_notify(env, new_proto);
    }

    return ret_func;
}

// ------------------------------------------------------------
//
// js_bind_proxy
//
// ------------------------------------------------------------

static js_val js_bind_proxy (js_c_func_args) {

    js_func *func_obj = (js_func *)js_get_pointer(func_val);

    js_val **closure_array = func_obj->closure_array;
    int closure_count = func_obj->closure_count;

    // if bound function is called as a constructor --
    // see js_callnew () below -- then we need to keep
    // the 'this' value that was passed
    func_val = *closure_array[closure_count + 0];
    func_obj = (js_func *)js_get_pointer(func_val);

    if (unlikely(closure_array[1] == (void *)-1)) {
        closure_array[1] = NULL;

        if (func_obj->c_func == js_bind_proxy) {
            // if the next function is also bound, then
            // indicate to use the passed 'this' value
            func_obj->closure_array[1] = (void *)-1;
        }

    } else
        this_val = *closure_array[closure_count + 1];

    //
    // the number of closures, minus 2, is N, the number
    // of parameters to inject into the call.  we make
    // room for these N parameters, by moving arguments
    // forward N slots in the stack, and then copying
    // the 'bound' parameters, stored as closures.
    //
    js_growstack(env, js_stk_top, closure_count);

    js_link *old_stk_ptr = js_stk_top;
    js_link *new_stk_ptr = old_stk_ptr;
    for (int i = 2; i < closure_count; i++) {
        new_stk_ptr->value = js_undefined;
        new_stk_ptr = new_stk_ptr->next;
    }
    js_stk_top = new_stk_ptr;

    for (;;) {
        old_stk_ptr = old_stk_ptr->prev;
        new_stk_ptr = new_stk_ptr->prev;
        if (old_stk_ptr == stk_args)
            break;
        new_stk_ptr->value = old_stk_ptr->value;
    }

    for (int i = 2; i < closure_count; i++) {
        old_stk_ptr = old_stk_ptr->next;
        old_stk_ptr->value =
                *closure_array[closure_count + i];
    }

    // invoke the c function within the func_val object.
    // note that we assume func_val is a function object.
    // note also that callee should clean up the stack.
    return func_obj->c_func(
                    env, func_val, this_val, stk_args);
}

// ------------------------------------------------------------
//
// js_throw_func_name_hint
//
// ------------------------------------------------------------

static void js_throw_func_name_hint (js_environ *env,
                                     js_val func_name_hint,
                                     const char *error_func) {

    if (js_is_number(func_name_hint)) {
        // actually a pointer to an ascii string, see also
        // write_func_name_hint () in expression_writer.js
        const char *s = (void *)func_name_hint.raw;
        func_name_hint = js_str_c2(env, s, strlen(s));
    }

    js_callshadow(env, error_func, func_name_hint);
}

// ------------------------------------------------------------
//
// js_callnew
//
// ------------------------------------------------------------

js_val js_callnew (js_environ *env, js_val func_val,
                   js_link *stk_args) {

    js_val func_name_hint = env->func_name_hint;
    env->func_name_hint = js_undefined;

    //
    // the target of the 'new' operator must be a function
    // object that is not flagged as 'not a constructor'.
    // if the target is a bound function, find the actual
    // target function, and make sure it is a constructor.
    //
    js_func *func1;
    js_func *func2 = NULL;

    if (!js_is_object(func_val))
        func1 = NULL;
    else {
        func1 = js_get_pointer(func_val);
        if (!js_obj_is_exotic(func1, js_obj_is_function))
            func1 = NULL;
        else {
            func2 = func1;
            while (func2->c_func == js_bind_proxy) {
                // function object created by js_proto_bind (),
                // the bound function is in the closure array
                const int closure_count = func2->closure_count;
                js_val v = *func2->closure_array[closure_count];
                func2 = (js_func *)js_get_pointer(v);
            }
            if (func2->flags & js_not_constructor)
                func1 = NULL;
        }
    }

    if (!func1) {
        js_throw_func_name_hint(env, func_name_hint,
                            "TypeError_expected_constructor");
    }

    //
    // create an empty object for the constructor function,
    // using the shape pre-configured for that constructor,
    // see function_expression () in function_writer.js.
    //
    js_shape *shape = env->shape_empty;
    if (func2->flags & js_strict_mode) {
        shape = func2->u.new_shape;
        if (!shape)
            shape = env->shape_empty;
    } else
        shape = env->shape_empty;

    js_obj *this_obj = js_newexobj(
                        env, env->obj_proto, shape);
    for (int i = 0; i < shape->num_values; i++)
        this_obj->values[i] = js_deleted;

    // prototype is the object property 'prototype' combined
    // with the low 3 bits from its own js_obj->proto field,
    // as these bits determine the type of exotic object
    int64_t dummy_shape_cache;
    const js_val func2_val = js_make_object(func2);
    js_val proto_val = js_getprop(env, func2_val,
                                  env->str_prototype,
                                 &dummy_shape_cache);
    if (js_is_object(proto_val)) {
        js_obj *obj_ptr =
                (js_obj *)js_get_pointer(proto_val);
        uintptr_t proto = (uintptr_t)obj_ptr
                | ((uintptr_t)obj_ptr->proto & 7);
        this_obj->proto = (js_obj *)proto;
    }

    // bound functions generally ignore a passed 'this'
    // value, in favor of a pre-set 'this' value.  but
    // this should not happen when used as constructor.
    js_val this_val =
        js_gc_manage(env, js_make_object(this_obj));
    if (func1 != func2) {
        // indicate to use the passed 'this' value
        func1->closure_array[1] = (void *)-1;
    }

    // call the constructor function, with new.target
    // meta property set to the function object value
    env->new_target = func2_val;
    js_val ret_val = func1->c_func(
                    env, func_val, this_val, stk_args);
    env->new_target = js_undefined;

    // constructor can override returned object
    return js_is_object(ret_val) ? ret_val : this_val;
}

// ------------------------------------------------------------
//
// js_callfunc
//
// ------------------------------------------------------------

js_val js_callfunc (js_c_func_args) {

    js_val func_name_hint = env->func_name_hint;
    env->func_name_hint = js_undefined;

    if (js_is_object(func_val)) {

        js_obj *func_obj = (js_obj *)js_get_pointer(func_val);
        if (js_obj_is_exotic(func_obj, js_obj_is_function)) {

            js_prolog_stack_frame(); // stack frame

            return ((js_func *)func_obj)->c_func(
                        env, func_val, this_val, stk_args);
        }
    }

    //
    // handle proxy apply () here
    //

    //
    // throw on error
    //

    stk_args->value = js_uninitialized;

    js_throw_func_name_hint(env, func_name_hint,
                            "TypeError_expected_function");
    return js_undefined; // never reached
}

// ------------------------------------------------------------
//
// js_newclosure
//
// allocate a closure variable, i.e. a variable which is
// accessed by a nested function, and therefore must remain
// 'live' even when its own declaring function has returned.
//
// at some later point, the closure variable is assigned to
// a function via js_newfunc (), see there.  but until then,
// the closure value must remain reachable, because of gc.
// the array in js_func->closure_temps is used for this.
//
// ------------------------------------------------------------

js_val *js_newclosure (js_val func_val, js_val *old_val) {

    // allocate room to hold the new closure variable
    struct js_closure_var *new_closure =
                js_malloc(sizeof(struct js_closure_var));
    js_val *val_ptr = &new_closure->value;
    *val_ptr = old_val ? *old_val : js_uninitialized;

    // an owner function reference tells js_newfunc ()
    // this closure var can be found in 'closure_temps'
    new_closure->owner_or_ref_count = func_val;
    // insert the new closure var in 'closure_temps'
    js_func *func_obj = js_get_pointer(func_val);
    new_closure->next = func_obj->closure_temps;
    func_obj->closure_temps = val_ptr;

    return val_ptr;
}

// ------------------------------------------------------------
//
// js_closureval
//
// support routine for closure values.  closure variable is
// a local variable that is referenced in a nested function.
//
// when this function is invoked by the outer function, it
// is called with a special value to allocate the variable.
// these values values are then sent to the nested function
// as part of the call to js_newfunc (), see above.
//
// when this function is invoked by the inner function, it
// is called with the index number of a particular closure
// variable.  the indexing of closure variable is set by
// add_closure_variable () in variable_resolver.js.
//
// note that closure variables are initially set to a special
// value (js_uninitialized) to allow the nested function to
// detect uninitialized variables.
//
// ------------------------------------------------------------

js_val *js_closureval (js_environ *env,
                       js_val func_val,
                       int closure_index) {

    js_val *val_ptr;
    const js_func *func_obj = js_get_pointer(func_val);
    js_val **closure_array = func_obj->closure_array;
    const int closure_count = func_obj->closure_count;

    if (closure_index < 0) {
        // a negated index is used when the parent of
        // a nested function must forward closure vars
        // to its nested function, and that parent does
        // not itself reference those particular vars.
        // the negated index means the forwarded var may
        // not be initialized, so we must skip the check.
        // see also import_local_from_outer_func () in
        // function_writer.js
        closure_index = ~closure_index;
        if (closure_index < closure_count) {

            val_ptr = closure_array[closure_index
                                  + closure_count];
            return val_ptr;
        }

    } else if (closure_index < closure_count) {

        // request to reference a closure variable.
        // note that we first

        val_ptr = closure_array[closure_index
                              + closure_count];

        if (val_ptr->raw != js_uninitialized.raw)
            return (closure_array[closure_index] = val_ptr);

        // access to an uninitialized variable
        js_callshadow(
            env, "ReferenceError_uninitialized_variable",
            js_undefined);
    }

    // internal error caused by invalid parameters
    fprintf(stderr, "Closure error!\n");
    exit(1);
}

// ------------------------------------------------------------
//
// js_callfunc1
//
// ------------------------------------------------------------

static js_val js_callfunc1 (
                        js_environ *env, js_val func_val,
                        js_val this_val, js_val arg_val) {

    // we need just two stack slots, but make extra room
    js_ensure_stack_at_least(4);
    env->new_target = js_undefined; // fixme

    // per our calling convention, the link at stk_top
    // is reserved for func_val, and the link after that
    // is used for the optional argument
    js_link *stk_args = js_stk_top;
    js_stk_top = js_stk_top->next;
    js_stk_top->value = arg_val;
    js_stk_top = js_stk_top->next;

    // invoke the c function within the func_val object.
    // note that we assume func_val is a function object.
    // note also that callee should clean up the stack.
    return (((js_func *)js_get_pointer(func_val))->c_func)
                (env, func_val, this_val, stk_args);
}

// ------------------------------------------------------------
//
// js_callshadow
//
// calls a function defined as a property of the 'shadow'
// object, which is a mechanism for 'runtime2.js' to expose
// functions that we can reliably call (e.g. invoking Error
// constructors), even if the js program corrupts properties
// on the 'global' object itself.
//
// ------------------------------------------------------------

static js_val js_callshadow (js_environ *env,
                    const char *func_name, js_val arg_val) {

    js_val *ptr_func_val = js_ownprop(
                            env, env->shadow_obj,
                            js_str_c(env, func_name), false);

    if (ptr_func_val
    && js_obj_is_exotic(js_get_pointer(*ptr_func_val),
                        js_obj_is_function)) {

        return js_callfunc1(
                env, *ptr_func_val, js_undefined, arg_val);
    }

    fprintf(stderr, "CallShadow error! (%s)\n", func_name);
    exit(1);
    return js_undefined;
}

// ------------------------------------------------------------
//
// js_nop
//
// ------------------------------------------------------------

static js_val js_nop (js_c_func_args) {

    js_return(js_undefined);
}

// ------------------------------------------------------------
//
// js_proto_call
//
// Function.prototype.call
//
// ------------------------------------------------------------

static js_val js_proto_call (js_c_func_args) {

    func_val = this_val;

    js_link *stk_ptr = stk_args->next;
    if (stk_ptr != js_stk_top)
        this_val = stk_ptr->value;
    else {
        // no parameters passed, so pass along same 'stk_args'
        this_val = js_undefined;
        stk_ptr = stk_args;
    }

    js_val ret_val =
            js_callfunc(env, func_val, this_val, stk_ptr);

    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_proto_bind
//
// Function.prototype.bind
//
// ------------------------------------------------------------

static js_val js_proto_bind (js_c_func_args) {
    js_prolog_stack_frame();

    // make sure bind () was called on a function object
    func_val = this_val;
    js_throw_if_notfunc(env, func_val);

    // create the name 'bound (name)' for the new function.
    // note that if the target is already a bound function,
    // the name will intentionally be 'bound bound (name)'
    int64_t dummy_shape_cache;
    js_val func_name = js_getprop(env, func_val,
                                  env->str_name,
                                 &dummy_shape_cache);
    func_name = js_str_concat(env,
                    env->func_bound_prefix, func_name);

    // get the 'this' parameter for the new function,
    // and count the number of bound parameters

    int num_args = 0;

    js_link *arg_ptr = stk_args;
    if ((arg_ptr = arg_ptr->next) == js_stk_top)
        this_val = js_undefined;

    else {
        this_val = arg_ptr->value;

        while ((arg_ptr = arg_ptr->next) != js_stk_top)
            num_args++;
    }

    // create the new function.  initially we pass the flag
    // 'not a constructor' to js_newfunc () so it does not
    // create a prototype object along with a new function.
    // but we only keep the flag, if the target function
    // actually has that flag.

    js_val bound = js_newfunc(env, js_bind_proxy, func_name, NULL,
                        js_strict_mode | js_not_constructor,
                                        /* closures */ 0);
    js_func *func = js_get_pointer(bound);
    func->flags &= ~js_not_constructor;

    // calculate arity for the new function, which is that
    // of the target function, minus number of parameters
    js_val length = js_getprop(env, func_val,
                               env->str_length,
                               &dummy_shape_cache);
    if (js_is_number(length) && length.raw != INFINITY) {
        if ((length.num -= num_args) < 0)
            length.num = 0;
    }
    ((js_descriptor *)js_get_pointer(
                func->super.values[0 /* length */]))
                                ->data_or_getter = length;

    // keep the bind parameters in the closure array.
    // index 0 for the target function
    // index 1 for the overriding 'this' reference
    // index 2 and beyond for bound parameters

    int closure_count = func->closure_count = 2 + num_args;
    js_val **closure_array = func->closure_array =
            js_calloc(closure_count * 2, sizeof(js_val *));

    for (int i = 0; i < closure_count; i++) {
        closure_array[closure_count + i] =
                                js_malloc(sizeof(js_val));
    }

    *closure_array[closure_count + 0] = func_val;
    *closure_array[closure_count + 1] = this_val;

    arg_ptr = stk_args->next;
    for (int i = 2; i < closure_count; i++) {
        arg_ptr = arg_ptr->next;
        *closure_array[closure_count + i] = arg_ptr->value;
    }

    js_return(bound);
}

// ------------------------------------------------------------
//
// js_hasinstance
//
// ------------------------------------------------------------

static js_val js_hasinstance (js_c_func_args) {
    js_prolog_stack_frame();

    // implementation of 'object' instanceof 'function'.
    // the 'function' is passed in the 'this' argument.
    // the 'object' is passed as the first parameter.
    func_val = this_val;
    js_link *arg_ptr = stk_args->next;
    js_val inst_val = (arg_ptr != js_stk_top)
                    ? arg_ptr->value : js_undefined;

    js_val ret_val  = js_false;

    do {

        if (!js_is_object(func_val))
            break;

        js_obj *obj_ptr = (js_obj *)js_get_pointer(func_val);
        if (!js_obj_is_exotic(obj_ptr, js_obj_is_function))
            break;

        while (((js_func *)obj_ptr)->c_func == js_bind_proxy) {
            // function object created by js_proto_bind (),
            // the bound function is in the closure array
            const js_func *func_obj = (js_func *)obj_ptr;
            const int closure_count = func_obj->closure_count;
            func_val = *func_obj->closure_array[closure_count];
            obj_ptr = (js_obj *)js_get_pointer(func_val);
        }

        if (!js_is_object(inst_val))
            break;

        int64_t dummy_shape_cache;
        js_val prototype = js_getprop(env, func_val,
                                      env->str_prototype,
                                     &dummy_shape_cache);
        js_throw_if_notobj(env, prototype);
        js_obj *func_proto = js_get_pointer(prototype);

        js_obj *inst_ptr = js_get_pointer(inst_val);
        for (;;) {
            js_obj *inst_proto = js_obj_get_proto(inst_ptr);
            if (!inst_proto)
                break;
            if (inst_proto == func_proto) {
                ret_val = js_true;
                break;
            }
            inst_ptr = inst_proto;
        }

    } while (0);
    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_flag_as_constructor
//
// ------------------------------------------------------------

static js_val js_flag_as_constructor (js_c_func_args) {

    js_link *arg_ptr = stk_args->next;
    js_val arg_val = arg_ptr != js_stk_top
                   ? arg_ptr->value : js_undefined;

    if (js_is_object(arg_val)) {

        js_func *func_obj = js_get_pointer(arg_val);
        if (js_obj_is_exotic(func_obj, js_obj_is_function)) {

            func_obj->flags &= ~js_not_constructor;
        }
    }

    js_return(js_undefined);
}

// ------------------------------------------------------------
//
// js_func_init
//
// initialize this func module.  this is called by js_init ()
// as part of the initial initialization sequence, so keep in
// mind that not all functionality is ready for use.
//
// ------------------------------------------------------------

static void js_func_init (js_environ *env) {

    js_val obj;

    // set the default value for the new.target property
    env->new_target = js_undefined;

    // make sure func_name_hint is always set to undefined
    env->func_name_hint = js_undefined;

    // create a shape for a newly-created function object
    // with the properties:  length  name  prototype
    // and also for a newly-created prototype object, with
    // just the single property:  constructor

    obj = js_emptyobj(env);
    js_newprop(env, obj, env->str_length)    = js_make_number(0.0);
    js_newprop(env, obj, env->str_name)      = js_make_number(0.0);
    js_newprop(env, obj, env->str_prototype) = js_make_number(0.0);
    env->func_shape1 = ((js_obj *)js_get_pointer(obj))->shape;

    obj = js_emptyobj(env);
    js_newprop(env, obj, env->str_constructor) = js_make_number(0.0);
    env->func_shape2 = ((js_obj *)js_get_pointer(obj))->shape;

    // create the Function.prototype object, which is
    // required to be a non-constructor function.  note that
    // js_newfunc () uses 'func_proto' as the prototype, so
    // we first (temporarily) point that to Object.prototype.
    env->func_proto = (void *)
            ((uintptr_t)env->obj_proto | js_obj_is_function);
    obj = js_unnamed_func(js_nop, 0);

    js_func *func_proto = js_get_pointer(obj);
    env->func_proto = (void *)
                ((uintptr_t)func_proto | js_obj_is_function);

    // the prefix string 'bound '
    env->func_bound_prefix = js_str_c(env, "bound ");

    // expose the internal 'js_hasinstance' function, for runtime2.js
    obj = js_newfunc(env, js_hasinstance,
                     js_str_c(env, "[Symbol.hasInstance]"), NULL,
                     js_strict_mode, 0);
    js_newprop(env, env->shadow_obj,
        js_str_c(env, "js_hasinstance")) =
                                    env->func_hasinstance = obj;

    // expose Function.prototype.call as js_proto_call
    obj = js_newfunc(env, js_proto_call, js_str_c(env, "call"),
                NULL, js_strict_mode | 1, /* closures */ 0);
    js_newprop(env, env->shadow_obj,
        js_str_c(env, "js_proto_call")) = obj;

    // expose Function.prototype.bind as js_proto_bind
    obj = js_newfunc(env, js_proto_bind, js_str_c(env, "bind"),
                NULL, js_strict_mode | 1, /* closures */ 0);
    js_newprop(env, env->shadow_obj,
        js_str_c(env, "js_proto_bind")) = obj;

    // expose the utility function 'js_flag_as_constructor'
    js_newprop(env, env->shadow_obj,
            js_str_c(env, "js_flag_as_constructor")) =
                js_unnamed_func(js_flag_as_constructor, 1);

    // expose the utility function 'js_coroutine'
    js_newprop(env, env->shadow_obj,
        js_str_c(env, "js_coroutine")) =
            js_unnamed_func(js_coroutine, 2);
}
