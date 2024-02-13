
// ------------------------------------------------------------
//
// js_get_primitive_proto
//
// note that per ES2015, these function implementations do
// not throw a TypeError when passed a non-object argument.
//
// ------------------------------------------------------------

static js_obj *js_get_primitive_proto (
                            js_environ *env, js_val obj_val) {

    if (js_is_primitive(obj_val)) {

        const int primitive_type = js_get_primitive_type(obj_val);

        if (primitive_type == js_prim_is_string)
            return env->str_proto;

        if (primitive_type == js_prim_is_symbol)
            return env->sym_proto;

        if (primitive_type == js_prim_is_bigint)
            return env->big_proto;
    }

    if (js_is_number(obj_val))
        return env->num_proto;

    if (js_is_boolean(obj_val))
        return env->bool_proto;

    js_callthrow("TypeError_convert_null_to_object");
    return NULL;
}

// ------------------------------------------------------------
//
// js_obj_to_primitive_generic
//
// implements the internal ToPrimitive operation, including
// OrdinaryToPrimitive. this function should not be called
// directly; instead use js_obj_to_primitive_default (),
// js_obj_to_primitive_number / js_obj_to_primitive_string.
//
// ------------------------------------------------------------

static js_val js_obj_to_primitive_generic (
                            js_environ *env, js_val obj_val,
                            js_val hint_for_toPrimitive,
                            js_val first_function_to_try,
                            js_val second_function_to_try) {

    // if Symbol(toPrimitive) exists on the input object,
    // call it and return its result.

    int64_t dummy_shape_cache;

    js_val method = js_getprop(env, obj_val,
                               env->sym_toPrimitive,
                               &dummy_shape_cache);

    if (!js_is_undefined_or_null(method)) {

        js_throw_if_notfunc(env, method);
        js_val ret_val = js_callfunc1(env, method, obj_val,
                                      hint_for_toPrimitive);
        if (!js_is_object(ret_val))
            return ret_val;

    } else {

        //
        // do an OrdinaryToPrimitive-like sequence,
        // for 'string', i.e. try to convert first
        // using toString(), second using valueOf()
        //

        js_val prop_name = first_function_to_try;

        for (int i = 0; i < 2; i++) {

            method = js_getprop(env, obj_val, prop_name,
                                &dummy_shape_cache);

            if (js_is_object(method)) {

                const js_obj *obj_ptr =
                            (js_obj *)js_get_pointer(method);

                if (js_obj_is_exotic(
                        obj_ptr, js_obj_is_function)) {

                    js_val ret_val = js_callfunc1(
                        env, method, obj_val, js_undefined);

                    if (!js_is_object(ret_val))
                        return ret_val;
                }
            }

            prop_name = second_function_to_try;
        }
    }

    js_callthrow("TypeError_convert_object_to_primitive");
    return env->str_empty;
}

// ------------------------------------------------------------
//
// js_obj_to_primitive_default
//
// implements the internal ToPrimitive operation, where the
// hint (string or number) is not specified.  note that the
// caller need not make sure the input value is an object.
//
// ------------------------------------------------------------

static js_val js_obj_to_primitive_default (
                            js_environ *env, js_val obj_val) {

    if (!js_is_object(obj_val))
        return obj_val;

    return js_obj_to_primitive_generic(env, obj_val,
                // hint to pass to Symbol.toPrimitive ()
                env->str_default,
                // order of secondary functions to try
                env->str_valueOf, env->str_toString);
}

// ------------------------------------------------------------
//
// js_obj_to_primitive_number
//
// implements the internal ToPrimitive operation, including
// OrdinaryToPrimitive, where preferredType is 'number'.
//
// ------------------------------------------------------------

static js_val js_obj_to_primitive_number (
                            js_environ *env, js_val obj_val) {

    return js_obj_to_primitive_generic(env, obj_val,
                // hint to pass to Symbol.toPrimitive ()
                env->str_number,
                // order of secondary functions to try
                env->str_valueOf, env->str_toString);
}

// ------------------------------------------------------------
//
// js_obj_to_primitive_string
//
// implements the internal ToPrimitive operation, including
// OrdinaryToPrimitive, where preferredType is 'string'.
//
// ------------------------------------------------------------

static js_val js_obj_to_primitive_string (
                            js_environ *env, js_val obj_val) {

    return js_obj_to_primitive_generic(env, obj_val,
                // hint to pass to Symbol.toPrimitive ()
                env->str_string,
                // order of secondary functions to try
                env->str_toString, env->str_valueOf);
}

// ------------------------------------------------------------
//
// js_instanceof
//
// ------------------------------------------------------------

bool js_instanceof (js_environ *env,
                    js_val inst_val, js_val func_val) {

    js_throw_if_notobj(env, func_val);

    // if Symbol(hasInstance) exists on the 'func' object,
    // call it and return its result.

    int64_t dummy_shape_cache;

    js_val method = js_getprop(env, func_val,
                               env->sym_hasInstance,
                               &dummy_shape_cache);

    if (!js_is_undefined_or_null(method)) {
        // func_val has [Symbol.hasInstance], call it.
        // note that Function.prototype has this prop,
        // default value env->func_hasinstance
        js_throw_if_notfunc(env, method);
    } else {
        // otherwise use js_hasinstance in func.c
        js_throw_if_notfunc(env, func_val);
        method = env->func_hasinstance;
    }

    return js_is_truthy(js_callfunc1(
                        env, method, func_val, inst_val));
}

// ------------------------------------------------------------
//
// js_ownprop
//
// ------------------------------------------------------------

static js_val *js_ownprop (
    js_environ *env, js_val obj, js_val prop, bool can_add) {

    if (!js_is_object(obj))
        return NULL;

    int64_t prop_key = js_shape_key(env, prop);

    js_obj *obj_ptr = js_get_pointer(obj);
    js_shape *old_shape = obj_ptr->shape;
    js_shape *new_shape;

    int64_t idx_or_ptr;
    if (js_shape_value(old_shape, prop_key, &idx_or_ptr)) {

        if (idx_or_ptr < 0)
            return &obj_ptr->values[~idx_or_ptr];

        new_shape = (js_shape *)idx_or_ptr;
    } else
        new_shape = NULL;

    if (!can_add)
        return NULL;

    const int old_count = old_shape->num_values;
    if (!new_shape) {
        new_shape = js_shape_new(
            env, old_shape, old_count, prop_key);
    }

    // note that we initialize the new property to js_deleted
    js_shape_switch(
            env, obj_ptr, old_count, js_deleted, new_shape);
    return &obj_ptr->values[old_count];
}

// ------------------------------------------------------------
//
// js_obj_init
//
// ------------------------------------------------------------

static void js_obj_init (js_environ *env) {

    // allocate an empty object with a null prototype,
    // then make it the default prototype for Object
    env->obj_proto = js_newexobj(
                            env, NULL, env->shape_empty);
    env->obj_proto_val =
        js_gc_manage(env, js_make_object(env->obj_proto));

    // allocate the global object, but store it in the
    // environment with a flag bit.  this is used by
    // js_getprop () and js_setprop_object () to identify
    // global lookup.  see js_obj_init_2 () in objsup.c.
    const js_val global_obj = js_emptyobj(env);
    env->global_obj = js_make_flagged_pointer(
                            js_get_pointer(global_obj));

    // allocate the shadow object
    env->shadow_obj = js_emptyobj(env);

    // allocate the well-known iterator result shapes
    js_val iter_obj = js_emptyobj(env);
    js_newprop(env, iter_obj, env->str_value)   = js_make_number(0.0);
    js_newprop(env, iter_obj, env->str_done)    = js_make_number(0.0);
    env->shape_value_done = ((js_obj *)js_get_pointer(iter_obj))->shape;
    iter_obj = js_emptyobj(env);
    js_newprop(env, iter_obj, env->str_done)    = js_make_number(0.0);
    js_newprop(env, iter_obj, env->str_value)   = js_make_number(0.0);
    env->shape_done_value = ((js_obj *)js_get_pointer(iter_obj))->shape;
}
