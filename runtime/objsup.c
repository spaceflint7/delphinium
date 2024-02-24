
// ------------------------------------------------------------
//
// miscellaneous object support utilities
//
// Object.getPrototypeOf, Object.setPrototypeOf
// Object.preventExtensions, Object.isExtensible
//
// note that per ES2015, thse function implementations do
// not throw a TypeError when passed a non-object argument.
//
// ------------------------------------------------------------

// ------------------------------------------------------------
//
// js_getOrSetPrototype (obj, proto)
//
// if 'proto' is specified, acts like Object.setPrototypeOf(),
// sets the prototype of 'obj' to 'proto' and returns 'obj'.
// otherwise behaves like Object.getPrototypeOf() and returns
// the prototype of 'obj'.
//
// ------------------------------------------------------------

static js_val js_getOrSetPrototype (js_c_func_args) {
    js_prolog_stack_frame();

    //
    // collect parameters, validate 'obj' parameter
    //

    js_val obj_val = js_undefined;
    js_val proto_val = js_undefined;
    bool set_proto = false;

    js_link *arg_ptr = stk_args;
    if ((arg_ptr = arg_ptr->next) != js_stk_top) {
        obj_val = arg_ptr->value;
        if ((arg_ptr = arg_ptr->next) != js_stk_top) {
            proto_val = arg_ptr->value;
            set_proto = true;
        }
    }

    js_throw_if_nullobj(env, obj_val);
    js_obj *obj_ptr;
    uintptr_t proto;

    if (!set_proto) {

        //
        // Object.getPrototypeOf - return prototype.
        // we strip low 3 bits from js_obj->proto pointer.
        //

        if (js_is_object(obj_val)) {
            obj_ptr = (js_obj *)js_get_pointer(obj_val);
            proto = (uintptr_t)obj_ptr->proto;
        } else {
            proto = (uintptr_t)
                js_get_primitive_proto(env, obj_val);
        }
        proto &= ~7;
        obj_val = proto ? js_make_object(proto) : js_null;

    } else {

        //
        // Object.setPrototypeOf - return 'obj' as passed,
        // after modifying the prototype of the object.
        //

        if (js_is_object(proto_val)) {
            // make sure the gc marks 'proto_val', even
            // in the case that nothing else references
            // it, and 'obj_val' was already scanned
            js_gc_notify(env, proto_val);
            // prototype is the object passed as 'proto_val'
            // combined with the low 3 bits from its own
            // js_obj->proto field, as these bits determine
            // the type of exotic object
            obj_ptr = (js_obj *)js_get_pointer(proto_val);
            proto = (uintptr_t)obj_ptr;

        } else {
            if (proto_val.raw != js_null.raw)
                js_callthrow("TypeError_invalid_prototype");
            proto = 0;
        }

        if (js_is_object(obj_val)) {

            obj_ptr = (js_obj *)js_get_pointer(obj_val);

            // check if new prototype is actually different
            uintptr_t tmp = (uintptr_t)obj_ptr->proto & ~7;
            if (tmp != proto) {

                // non-extensible via preventExtensions ()
                int read_only = obj_ptr->max_values
                              & js_obj_not_extensible;

                // Object.prototype has immutable prototype
                if (obj_ptr == env->obj_proto)
                    read_only = true;

                if (read_only)
                    js_callthrow("TypeError_readOnlyProperty");

                // new prototype must not create a cyclic loop
                for (tmp = proto; tmp;) {
                    if ((void *)tmp == obj_ptr)
                        js_callthrow("TypeError_cyclicPrototype");
                    const void *tmp2 = ((js_obj *)tmp)->proto;
                    tmp = (uintptr_t)tmp2 & ~7;
                }

                // low 3 bits of obj->proto determine obj type
                proto |= ((uintptr_t)obj_ptr->proto & 7);
                obj_ptr->proto = (js_obj *)proto;

                // note that if the prototype of an array object
                // is not equal Array.prototype, then indexing
                // is done in a slow-path rather than fast-path.
                // see also js_arr_init ()
            }
        }
    }

    js_return(obj_val);
}

// ------------------------------------------------------------
//
// js_preventExtensions
//
// ------------------------------------------------------------

static js_val js_preventExtensions (js_c_func_args) {

    js_val obj_val = js_undefined;

    js_link *arg_ptr = stk_args->next;
    if (arg_ptr != js_stk_top) {

        obj_val = arg_ptr->value;
        if (js_is_object(obj_val)) {

            js_obj *obj_ptr =
                    (js_obj *)js_get_pointer(obj_val);

            obj_ptr->max_values |= js_obj_not_extensible;

            if (js_obj_is_exotic(
                            obj_ptr, js_obj_is_array)) {
                // disable fast-path on this array
                ((js_arr *)obj_ptr)->length = -1U;
            }
        }
    }

    js_return(obj_val);
}

// ------------------------------------------------------------
//
// js_isExtensible - Object.isExtensible
//
// ------------------------------------------------------------

static js_val js_isExtensible (js_c_func_args) {

    js_val ret_val = js_false;

    js_link *arg_ptr = stk_args->next;
    if (arg_ptr != js_stk_top) {

        js_val obj_val = arg_ptr->value;
        if (js_is_object(obj_val)) {

            const int max_values =
                ((js_obj *)js_get_pointer(obj_val))
                                            ->max_values;

            if (!(max_values & js_obj_not_extensible))
                ret_val = js_true;
        }
    }

    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_keys_in_object
//
// ------------------------------------------------------------

static js_val js_keys_in_object (js_c_func_args) {
    js_prolog_stack_frame();

    js_val obj_val = js_undefined;
    js_link *arg_ptr = stk_args->next;
    if (arg_ptr != js_stk_top)
        obj_val = arg_ptr->value;
    js_throw_if_notobj(env, obj_val);

    js_val dst_arr = js_newarr(env, 0);
    uint32_t dst_idx = 0;

    js_shape *shape =
            ((js_obj *)js_get_pointer(obj_val))->shape;
    int src_idx = 0;

    for (;;) {

        int64_t prop_key, idx_or_ptr;
        if (!intmap_get_next(shape->props, &src_idx,
                             (uint64_t *)&prop_key,
                             (uint64_t *)&idx_or_ptr))
            break;
        if (idx_or_ptr >= 0)
            continue;

        int kind = ((const objset_id *)prop_key)->flags;
        if (kind & js_str_is_string)
            kind = js_prim_is_string;
        else if (kind & js_str_is_symbol)
            kind = js_prim_is_symbol;
        else
            continue;

        js_arr_set(env, dst_arr, ++dst_idx,
                   js_make_primitive(prop_key, kind));
    }

    js_return(dst_arr);
}

// ------------------------------------------------------------
//
// js_obj_init_2
//
// ------------------------------------------------------------

static void js_obj_init_2 (js_environ *env) {

    js_val name;

    // global self reference.  note that this reference is
    // stored without the flag bit that env->global_obj has.
    // thus js_getprop () and js_setprop_object () can detect
    // the difference between (1) code that was generated to
    // use env->global_obj, e.g. lookup of just 'a'; and (2)
    // lookup of 'global.a', i.e. code that first looks up
    // 'global', then does  member access via the result.

    js_val global = js_make_object(
                        js_get_pointer(env->global_obj));
    js_newprop(env, global, js_str_c(env, "global")) = global;

    // shadow reference in global.  the shadow object is used
    // for interoperability between the c and javascript parts
    // of this runtime, and is not accessible to user code
    js_val shadow = env->shadow_obj;
    js_newprop(env, global, js_str_c(env, "shadow")) = shadow;

    // shadow.js_defineProperty
    js_newprop(env, shadow,
        js_str_c(env, "js_defineProperty")) =
            js_unnamed_func(js_defineProperty, 3);

    // shadow.js_getOwnProperty
    js_newprop(env, shadow,
        js_str_c(env, "js_getOwnProperty")) =
            js_unnamed_func(js_getOwnProperty, 2);

    // shadow.js_get_or_set_prototype function
    js_newprop(env, shadow,
        js_str_c(env, "js_getOrSetPrototype")) =
            js_unnamed_func(js_getOrSetPrototype, 1);

    // shadow.preventExtensions function
    name = js_str_c(env, "preventExtensions");
    js_newprop(env, shadow, name) =
        js_newfunc(env, js_preventExtensions, name, NULL,
                   js_strict_mode | 1, /* closures */ 0);

    // shadow.isExtensible function
    name = js_str_c(env, "isExtensible");
    js_newprop(env, shadow, name) =
        js_newfunc(env, js_isExtensible, name, NULL,
                   js_strict_mode | 1, /* closures */ 0);

    // shadow.js_keys_in_object
    js_newprop(env, shadow,
        js_str_c(env, "js_keys_in_object")) =
            js_unnamed_func(js_keys_in_object, 1);

    // shadow.js_property_flags
    js_newprop(env, shadow,
        js_str_c(env, "js_property_flags")) =
            js_unnamed_func(js_property_flags, 2);

    // shadow.js_private_object
    js_newprop(env, shadow,
        js_str_c(env, "js_private_object")) =
            js_unnamed_func(js_private_object, 1);
}
