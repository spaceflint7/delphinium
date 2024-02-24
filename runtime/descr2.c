
// ------------------------------------------------------------
//
// descriptor - defineProperty
//
// ------------------------------------------------------------

// ------------------------------------------------------------
//
// forward declarations
//
// ------------------------------------------------------------

static bool js_defineProperty_object (
                        js_environ *env, js_val obj_val,
                        js_val prop_val, js_val descr_val);

static bool js_defineProperty_array (
                        js_environ *env, js_val obj_val,
                        js_val prop_val, js_val descr_val);

static bool js_defineProperty_array_length (
                        js_environ *env,
                        js_val obj_val, js_val descr_val);

static bool js_defineProperty_proxy (
                        js_environ *env, js_val obj_val,
                        js_val prop_val, js_val descr_val);

static js_val js_defineProperty_generic (
                        js_environ *env,
                        js_val descr_val, js_val old_val);

static js_val js_getOwnProperty_2 (
                            js_environ *env, js_val value);

// ------------------------------------------------------------
//
// js_defineProperty
//
// implementation of the Object.defineProperty function
//
// ------------------------------------------------------------

static js_val js_defineProperty (js_c_func_args) {
    js_prolog_stack_frame();

    //
    // function prolog, initialize parameters
    //

    js_val obj_val = js_undefined;
    js_val prop_val = js_undefined;
    js_val descr_val = js_undefined;

    js_link *arg_ptr = stk_args;
    if ((arg_ptr = arg_ptr->next) != js_stk_top) {
        obj_val = arg_ptr->value;
        if ((arg_ptr = arg_ptr->next) != js_stk_top) {
            prop_val = arg_ptr->value;
            if ((arg_ptr = arg_ptr->next) != js_stk_top)
                descr_val = arg_ptr->value;
        }
    }

    // target must be an object, not a primitive value,
    // and descriptor must be an object

    js_throw_if_notobj(env, obj_val);

    if (!js_is_object(descr_val))
        js_callthrow("TypeError_defineProperty_descriptor");

    // check the type of object that is being modified

    bool success = false;

    js_obj *obj_ptr = (js_obj *)js_get_pointer(obj_val);

    if (js_obj_is_exotic(obj_ptr, js_obj_is_proxy)) {

        // a proxy defineProperty trap can return false,
        // to prevent modification of the property
        success = js_defineProperty_proxy(
                    env, obj_val, prop_val, descr_val);

    } else {

        success = (js_obj_is_exotic(obj_ptr, js_obj_is_array)
            ? js_defineProperty_array
            : js_defineProperty_object)(
                    env, obj_val, prop_val, descr_val);
    }

    if (!success)
        js_callthrow("TypeError_defineProperty_3");

    js_return(obj_val);
}

// ------------------------------------------------------------
//
// js_defineProperty_object
//
// ------------------------------------------------------------

static bool js_defineProperty_object (
                        js_environ *env, js_val obj_val,
                        js_val prop_val, js_val descr_val) {

    int *ptr_shape_id =
                &((js_obj *)js_get_pointer(obj_val))->shape_id;
    int old_shape_id = *ptr_shape_id;

    // if object is not extensible, and property is not
    // already found on the object, then throw an error
    js_obj *obj_ptr = (js_obj *)js_get_pointer(obj_val);
    if (obj_ptr->max_values & js_obj_not_extensible) {
        if (!js_ownprop(env, obj_val, prop_val, false)) {
            js_throw_if_not_extensible(
                        env, obj_val, prop_val, true);
        }
    }

    js_val *old_ptr = js_ownprop(env, obj_val, prop_val, true);
    js_val old_val = *old_ptr;
    js_val new_val = js_defineProperty_generic(
                        env, descr_val, old_val);

    if (new_val.raw == js_deleted.raw)
        return false; // can't redefine property

    if (new_val.raw != old_val.raw) {
        *old_ptr = new_val;
        // some callers may already have a shape cache on this
        // property in this object, see js_getprop ().  when a
        // cache is in use, callers bypass js_getprop () and
        // grab the value directly off the object, which would
        // be incorrect if the property was changed from plain
        // value to descriptor.  so if the shape of the object
        // did not change, i.e. some property was overwritten,
        // then change the cache id on the object (not on the
        // actual shape) to invalidate any existing caches.
        if (old_shape_id == *ptr_shape_id) {
            if (js_is_descriptor(new_val)
            ||  js_is_descriptor(old_val)) {

                *ptr_shape_id = ++env->next_unique_id;
            }
        }
    }

    return true;
}

// ------------------------------------------------------------
//
// js_defineProperty_array
//
// ------------------------------------------------------------

static bool js_defineProperty_array (
                        js_environ *env, js_val obj_val,
                        js_val prop_val, js_val descr_val) {

    // check that the property is an integer array index,
    // or it is 'length' and the descriptor has a 'value'.
    // if neither is the case, then this is defineProperty
    // on an object, not an array.

    uint32_t prop_idx =
                js_str_is_length_or_number(env, prop_val);

    if (prop_idx >= js_len_index) {

        // if not 'length', or is 'length' but without 'value'
        if (prop_idx == js_not_index ||
                !js_hasprop(env, descr_val, env->str_value)) {

            return js_defineProperty_object(
                        env, obj_val, prop_val, descr_val);

        } else {

            return js_defineProperty_array_length(
                        env, obj_val, descr_val);
        }
    }

    // handle defineProperty on an array element

    js_arr *arr = (js_arr *)js_get_pointer(obj_val);
    if (prop_idx > arr->length_descr[0].num) {
        if (!js_descr_array_length_writable(arr))
            return false;
    }

    js_val old_val = js_arr_get(obj_val, prop_idx);
    js_val new_val = js_defineProperty_generic(
                        env, descr_val, old_val);

    if (new_val.raw == js_deleted.raw)
        return false; // can't redefine property

    if (new_val.raw != old_val.raw) {

        // if object is not extensible, and index is not
        // already set in the array, then throw an error
        js_throw_if_not_extensible(
                        env, obj_val, prop_val, true);

        // this will grow the array if necessary
        js_arr_set(env, obj_val, prop_idx, new_val);

        // disable fast-path on this array
        if (js_is_descriptor(new_val))
            arr->length = -1U;
    }

    return true;
}

// ------------------------------------------------------------
//
// js_defineProperty_array_length
//
// ------------------------------------------------------------

static bool js_defineProperty_array_length (
                        js_environ *env,
                        js_val obj_val, js_val descr_val) {

    js_arr *arr = (js_arr *)js_get_pointer(obj_val);

    // check if setting writable flag in the length.
    // a non-writable length can't be made writable.

    bool clear_writable = false;
    int64_t dummy_shape_cache;

    if (js_hasprop(env, descr_val, env->str_writable)) {

        if (js_is_truthy(
                js_getprop(env, descr_val,
                           env->str_writable,
                           &dummy_shape_cache))) {

            const int flags =
                        arr->length_descr[1].raw >> 56;
            if (!(flags & js_descr_write)) {

                // already non-writable, can't make writable
                return false;
            }

        } else
            clear_writable = true;
    }

    // handle defineProperty 'length' by calling the
    // function js_setprop_array_length ()
    //
    // this call throws a RangeError if the length is
    // not valid array length.  however, it will never
    // throw an error -- but it will return false --
    // if length is non-writable, or if the new length
    // would cause the deletion of non-configurable
    // array elements.  but then we return a false
    // value from this function, which might then throw.

    bool success = js_setprop_array_length(env, obj_val,
            js_getprop(env, descr_val, env->str_value,
                       &dummy_shape_cache));

    // code generated by member_expression_array () can
    // circumvent some checks, if set_limit is large
    // enough, and js_setprop () would also choose the
    // 'fast-path' with less checks.  so if the length
    // is made non-writable, we have to disable the
    // fast-path, to ensure correctness.  this must be
    // done even if the return value was false.

    if (clear_writable) {

        // turn off writable bit
        arr->length_descr[1].raw &=
                    ~((uint64_t)js_descr_write << 56);

        // disable fast-path on this array
        arr->length = -1U;
    }

    return success;
}

// ------------------------------------------------------------
//
// js_defineProperty_proxy
//
// ------------------------------------------------------------

static bool js_defineProperty_proxy (
                        js_environ *env, js_val obj_val,
                        js_val prop_val, js_val descr_val) {
    printf("PROXY NOT IMPL YET\n");
    return false;
}

// ------------------------------------------------------------
//
// js_defineProperty_parse
//
// ------------------------------------------------------------

static void js_defineProperty_parse (
                    js_environ *env, js_val descr_val,
                    js_descriptor *out_descr) {

    int64_t dummy_shape_cache;
    js_val val1 = out_descr->data_or_getter;
    js_val val2 = js_make_object(js_get_pointer(
                            out_descr->setter_and_flags));
    js_val tmp;
    int flags = js_descr_flags_without_setter(out_descr);

    // check if input descriptor has 'get' or 'set' properties
    bool has_get = js_hasprop(env, descr_val, env->str_get);
    bool has_set = js_hasprop(env, descr_val, env->str_set);

    bool has_val = js_hasprop(env, descr_val, env->str_value);
    bool has_w = js_hasprop(env, descr_val, env->str_writable);
    if (has_val || has_w) {

        //
        // data descriptor, specifies 'value' or 'writable'
        //

        flags &= ~(js_descr_getter | js_descr_setter);
        flags |= js_descr_value;

        if (has_val) {
            val1 = js_getprop(env, descr_val, env->str_value,
                              &dummy_shape_cache);
            if (js_is_object_or_primitive(val1))
                js_gc_notify(env, val1);
        }

        if (has_w) {

            tmp = js_getprop(env, descr_val, env->str_writable,
                             &dummy_shape_cache);

            if (js_is_truthy(tmp))
                flags |= js_descr_write;
            else
                flags &= ~js_descr_write;
        }

        // abort if descriptor also has accessor properties
        if (has_get || has_set)
            js_callthrow("TypeError_defineProperty_4");

    } else if (has_get || has_set) {

        //
        // accessor descriptor, specifies 'set' or 'get'
        //

        bool err = false;
        flags &= ~(js_descr_value | js_descr_write);

        // getter property

        if (has_get) {

            val1 = js_getprop(env, descr_val, env->str_get,
                              &dummy_shape_cache);

            if (js_is_object(val1) &&
                    js_obj_is_exotic(js_get_pointer(val1),
                                     js_obj_is_function)) {
                flags |= js_descr_getter;
                js_gc_notify(env, val1);
            } else
                err = true;
        } else
            flags &= ~js_descr_getter;

        // setter

        if (has_set) {

            val2 = js_getprop(env, descr_val, env->str_set,
                              &dummy_shape_cache);

            if (js_is_object(val2) &&
                    js_obj_is_exotic(js_get_pointer(val2),
                                     js_obj_is_function)) {
                flags |= js_descr_setter;
                js_gc_notify(env, val2);
            } else
                err = true;
        } else
            flags &= ~js_descr_setter;

        // abort if 'get' or 'set' property is not a function
        if (err) {
            js_callthrow("TypeError_defineProperty_5");
        }
    }

    // check enumerable

    if (js_hasprop(env, descr_val, env->str_enumerable)) {

        tmp = js_getprop(env, descr_val,
                         env->str_enumerable,
                         &dummy_shape_cache);

        if (js_is_truthy(tmp))
            flags |= js_descr_enum;
        else
            flags &= ~js_descr_enum;
    }

    // check configurable

    if (js_hasprop(env, descr_val, env->str_configurable)) {

        tmp = js_getprop(env, descr_val,
                         env->str_configurable,
                         &dummy_shape_cache);

        if (js_is_truthy(tmp))
            flags |= js_descr_config;
        else
            flags &= ~js_descr_config;
    }

    js_setdescr(out_descr, flags, val1, val2);
}

// ------------------------------------------------------------
//
// js_defineProperty_generic
//
// ------------------------------------------------------------

static js_val js_defineProperty_generic (
                        js_environ *env, js_val descr_val,
                        js_val old_val) {

    js_descriptor descr2, *descr1;
    int flags;

    if (js_is_descriptor(old_val)) {

        // there was already a descriptor for this property.
        // parse the input descriptor and then compare them.

        descr1 = (js_descriptor *)js_get_pointer(old_val);

        descr2 = *descr1; // copy descriptor
        js_defineProperty_parse(env, descr_val, &descr2);

        flags = js_descr_flags_without_setter(descr1);

        // if the old descriptor was non-configurable,
        // then the request is an error, unless:
        // new descriptor and old descriptor are the same,
        // or old descriptor is a writable value, and only
        // the value and/or writable fields are modified

        if (!(flags & js_descr_config)) {

            uint64_t diff = descr1->setter_and_flags.raw
                          ^ descr2.setter_and_flags.raw;

            if (flags & js_descr_write)
                diff &= ~(((uint64_t)js_descr_write) << 56);

            else if (descr1->data_or_getter.raw
                            != descr2.data_or_getter.raw)
                return js_deleted;  // can't redefine

            if (diff)
                return js_deleted;  // can't redefine
        }

        // return success, updating the old descriptor
        *descr1 = descr2;
        return old_val;
    }

    // the property did not exist, or was not a descriptor,
    // it can always be overwritten.  note (1) that config
    // and enum flags will default to true only if the
    // property already existed; and (2) that 'js_deleted'
    // is the initial value for uninitialized properties.

    if (old_val.raw == js_deleted.raw) {
        descr2.data_or_getter = js_undefined;
        flags = js_descr_value;
    } else {
        descr2.data_or_getter = old_val;
        flags = js_descr_enum | js_descr_config
              | js_descr_value | js_descr_write;
    }
    descr2.setter_and_flags.raw = ((uint64_t)flags) << 56;

    js_defineProperty_parse(env, descr_val, &descr2);

    // if new descriptor is the same as a non-descriptor
    // property, i.e. configurable, enumerable, writable
    // data property, then it can just be a plain value
    flags = js_descr_flags_without_setter(&descr2);
    if (flags == (   js_descr_value | js_descr_write
                   | js_descr_enum | js_descr_config))
        return descr2.data_or_getter;

    // return success, allocating a new descriptor object
    descr1 = js_malloc(sizeof(js_descriptor));
    *descr1 = descr2; // copy descriptor
    return js_make_descriptor(descr1);
}
