
// ------------------------------------------------------------
//
// forward declarations
//
// ------------------------------------------------------------

static void js_setprop_object (js_environ *env,
                            js_val obj, js_obj *obj_ptr,
                            js_val prop, js_val value,
                            int64_t *shape_cache);

static void js_setprop_array (js_environ *env, js_val obj,
                            js_val prop, uint32_t prop_idx,
                            js_val value);

static bool js_setprop_proxy (js_environ *env, js_val obj,
                            js_val prop, js_val value);

static bool js_setprop_prototype (js_environ *env,
                            js_val obj, js_obj *obj_ptr,
                            int64_t prop_key, js_val value,
                        js_val prop_name_in_case_of_error);

static void js_throw_if_primitive (js_environ *env, js_val obj);

// ------------------------------------------------------------
//
// js_setprop
//
// ------------------------------------------------------------

js_val js_setprop (js_environ *env, js_val obj, js_val prop,
                   js_val value, int64_t *shape_cache) {

    //printf("js_setprop ");js_print(env, prop);printf("\n");

    // clear the shape cache which records the shape id and
    // array index where the value is found.
    *shape_cache = 0;

    // prevent the js_deleted value from propagating, as in
    // for example:  obj.prop = ([])[0].  see also handling
    // of set-property expressions, in property_writer.js.
    if (value.raw == js_deleted.raw)
        value = js_undefined;

    // first thing we do is identify the object that we need
    // to scan:  the input object, if it is really an object.
    // or if primitive, this is the corresponding prototype.

    js_obj *obj_ptr;
    if (likely(js_is_object(obj)))
        obj_ptr = js_get_pointer(obj);

    else {

        if (likely(js_is_primitive_string(obj))) {

            // this is a string primitive, check if property
            // is valid on the string, i.e. it is 'length' or
            // an integer index between 0 and length.  if so,
            // we throw error in strict mode, else ignore.

            if (js_str_setprop(env, obj, prop))
                return value;

            // otherwise we need to scan String.prototype
            obj_ptr = env->str_proto;

        } else {

            if (js_is_undefined_or_null(obj)) {
                // throw if set property of undefined or null
                js_callshadow(env,
                    "TypeError_set_property_of_null_object",
                    prop);
            }

            // get the appropriate prototype object to scan
            obj_ptr = js_get_primitive_proto(env, obj);
        }
    }

    //
    // check object type.  if this is an array, and if the
    // property is an array indexer, then do array access,
    // via either a fast-path or slow-path function.
    //

    uintptr_t proto = (uintptr_t)obj_ptr->proto;

    if ((proto & 7) == js_obj_is_array) {

        uint32_t prop_idx =
                    js_str_is_length_or_number(env, prop);

        if (prop_idx != js_not_index) {

            if (unlikely(prop_idx == js_len_index)) {

                // setting array length
                if (!js_setprop_array_length(env, obj, value)) {
                    js_throw_if_strict_1(
                        "TypeError_readOnlyProperty", prop);
                }

            } else if (likely(
                    proto == (uintptr_t)env->fast_arr_proto
                 && ((js_arr *)obj_ptr)->length != -1U)) {

                // fast-path when there are no descriptors
                // on the array, and no integer-like values
                // on the prototype chain
                js_arr_set(env, obj, prop_idx, value);

            } else {

                // slow-path otherwise
                js_setprop_array(
                           env, obj, prop, prop_idx, value);
            }

            return value;
        }
    }

    //
    // if the object is a proxy, we must forward the call
    //

    if ((proto & 7) == js_obj_is_proxy) {

        if (js_setprop_proxy(env, obj, prop, value))
            return value;
    }

    //
    // otherwise the property name is a string or symbol
    // primitive values which is set via the object shape.
    //

    js_setprop_object(
            env, obj, obj_ptr, prop, value, shape_cache);

    if (   (obj_ptr == env->obj_proto
         || obj_ptr == env->arr_proto)) {

        if (js_str_is_length_or_number(env, prop)
                                        != js_not_index) {
            //
            // if an integer property is added on
            // Object.prototype or Array.prototype,
            // then disable the array fast-path
            //
            env->fast_arr_proto = NULL;
        }
    }

    return value;
}

// ------------------------------------------------------------
//
// js_setprop_object
//
// ------------------------------------------------------------

static void js_setprop_object (js_environ *env,
                            js_val obj, js_obj *obj_ptr,
                            js_val prop, js_val value,
                            int64_t *shape_cache) {

    js_shape *old_shape = obj_ptr->shape;
    js_shape *new_shape;
    int64_t idx_or_ptr;
    int64_t prop_key = js_shape_key(env, prop);

    if (js_shape_value(old_shape, prop_key, &idx_or_ptr)) {

        if (idx_or_ptr < 0) {

            idx_or_ptr = ~idx_or_ptr;
            js_val old_val = obj_ptr->values[idx_or_ptr];

            if (old_val.raw != js_deleted.raw) {

                if (js_is_descriptor(old_val)) {

                    // valid descriptor;  check if property
                    // can be updated, by updating a writable
                    // value, or calling a setter function.
                    if (js_descr_check_writable(
                                env, old_val, obj, value,
                    // determine can_update_value_in_descr:
                    // with primitive values, obj_ptr points
                    // to the prototype here, but should be
                    // considered 'read-only' at this point
                                js_is_object(obj), prop)) {
                        js_throw_if_primitive(env, obj);
                    }

                    // update shape cache for data descriptor
                    const int flags =
                        js_descr_flags_without_setter(
                            (js_descriptor *)
                                js_get_pointer(old_val));
                    if (flags & js_descr_value) {
                        js_shape_update_cache_key_descr(
                            shape_cache, obj_ptr,
                            idx_or_ptr, flags);
                    }

                    return;
                }

                // property was deleted; but check for property
                // on the prototype chain.  returns false if
                // not a writable value, or if a setter function
                // was called.  returns true if there is nothing
                // to prevent setting property on this object.
                if (!js_setprop_prototype(
                                env, obj, obj_ptr, prop_key,
                                value, prop))
                    return;
            }

            js_throw_if_primitive(env, obj);

            // value found directly on the object, we can cache
            // shape id and array index, see also js_getprop ()
            js_shape_update_cache_key(
                            shape_cache, obj_ptr, idx_or_ptr);

            obj_ptr->values[idx_or_ptr] = value;
            return;
        }

        new_shape = (js_shape *)idx_or_ptr;
    } else
        new_shape = NULL;

    // check if prototype chain permits setting the value.
    // returns false if not writable, or a setter function
    // was called.  otherwise returns true and we can add
    // the new property to the initial object

    if (!js_setprop_prototype(
                env, obj, obj_ptr, prop_key, value, prop))
        return;

    if (obj.raw == env->global_obj.raw) {
        // in strict mode, unqualified access should throw
        // ReferenceError for undeclared vars.  see also
        // js_obj_init () and js_obj_init_2 ()
        js_throw_if_strict_1(
            "ReferenceError_not_defined", prop);
    }

    js_throw_if_primitive(env, obj);
    if (js_throw_if_not_extensible(env, obj, prop, false))
        return; // not strict mode, just silently ignore

    // if object is not extensible, and property is not
    // already found on the object, then throw an error
    if (obj_ptr->max_values & js_obj_not_extensible) {
        js_throw_if_strict_1(
            "TypeError_object_not_extensible", prop);
        return; // silently ignore if not strict mode
    }

    // switch object to a shape which includes the new property
    const int old_count = old_shape->num_values;
    if (!new_shape) {
        new_shape = js_shape_new(
                        env, old_shape, old_count, prop_key);
    }
    js_shape_switch(obj_ptr, old_count, value, new_shape);

    // value was found directly on the object, we can cache
    // the shape id and array index, see also js_getprop ()
    js_shape_update_cache_key(shape_cache, obj_ptr, old_count);
}

// ------------------------------------------------------------
//
// js_setprop_array
//
// ------------------------------------------------------------

static void js_setprop_array (js_environ *env, js_val obj,
                            js_val prop, uint32_t prop_idx,
                            js_val value) {

    js_arr *arr = js_get_pointer(obj);

    // length_descr[0] holds the length as a double
    uint32_t length = (uint32_t)arr->length_descr[0].num;

    if (prop_idx <= length) {

        js_val old_val = arr->values[prop_idx - 1];

        if (old_val.raw != js_deleted.raw) {

            if (js_is_descriptor(old_val)) {
                // valid descriptor;  check if property can
                // be updated, by updating a writable value,
                // or calling a setter function
                if (js_descr_check_writable(
                                env, old_val, obj, value,
                    // determine can_update_value_in_descr:
                    // with primitive values, obj_ptr points
                    // to the prototype here, but should be
                    // considered 'read-only' at this point
                                js_is_object(obj), prop)) {
                    js_throw_if_primitive(env, obj);
                }

                return;
            }

            if (arr->super.proto) {
                // property was deleted; but check for property
                // on the prototype chain.  returns false if
                // not a writable value, or if a setter function
                // was called.  returns true if there is nothing
                // to prevent setting property on this object.
                const int64_t prop_key = js_shape_key(env, prop);
                if (!js_setprop_prototype(
                            env, obj, &arr->super, prop_key,
                            value, prop))
                    return;
            }
        }

        js_throw_if_primitive(env, obj);
        arr->values[prop_idx - 1] = value;
        return;
    }

    if (arr->super.proto) {
        // check if prototype chain permits setting the value.
        // returns false if not writable, or a setter function
        // was called.  otherwise returns true and we can add
        // the new property to the initial object
        const int64_t prop_key = js_shape_key(env, prop);
        if (!js_setprop_prototype(
                env, obj, &arr->super, prop_key, value, prop))
            return;
    }

    // array cannot grow if length is non-writable
    if (!js_descr_array_length_writable(arr)) {
        js_throw_if_strict_1(
            "TypeError_readOnlyProperty", prop);
    }

    js_throw_if_primitive(env, obj);
    if (js_throw_if_not_extensible(env, obj, prop, false))
        return; // not strict mode, just silently ignore

    js_arr_set(env, obj, prop_idx, value);
}

// ------------------------------------------------------------
//
// js_setprop_array_length
//
// ------------------------------------------------------------

static bool js_setprop_array_length (
                js_environ *env, js_val obj, js_val value) {

    js_arr *arr = js_get_pointer(obj);

    uint32_t new_length = js_arr_check_length(env, value);

    // length_descr[0] holds the length as a double
    uint32_t old_length = (uint32_t)arr->length_descr[0].num;

    if (new_length == old_length)
        return true;

    // length cannot be set if is already non-writable
    if (!js_descr_array_length_writable(arr))
        return false;

    // if new length would cause the array to grow,
    // we only need to make sure the length is writable

    if (new_length > old_length) {

        if (new_length > js_arr_max_length)
            return false;

        // length_descr[0] holds the length as a double
        arr->length_descr[0].num = new_length;

        if (arr->length != -1U)
            arr->length = old_length;

        return true;
    }

    // the new length would cause the array to shrink,
    // but the array is not allowed to shrink smaller
    // than the index of the last non-configurable elem

    for (;;) {

        js_val old_val = arr->values[--old_length];

        if (js_is_descriptor(old_val)) {
            js_descriptor *descr =
                (js_descriptor *)js_get_pointer(old_val);
            const int flags =
                js_descr_flags_without_setter(descr);
            if (!(flags & js_descr_write)) {

                // a non-configurable element was found
                ++old_length;
                break;
            }

            free(descr);
        }

        arr->values[old_length] = js_deleted;

        if (old_length == new_length)
            break;
    }

    //
    // update length and limit
    //

    // length_descr[0] holds the length as a double
    arr->length_descr[0].num = old_length;

    if (arr->length != -1U)
        arr->length = old_length;

    return (old_length == new_length);
}

// ------------------------------------------------------------
//
// js_setprop_proxy
//
// ------------------------------------------------------------

static bool js_setprop_proxy (js_environ *env, js_val obj,
                            js_val prop, js_val value) {

    printf("PROXY NOT IMPL YET\n");
    return false;
}

// ------------------------------------------------------------
//
// js_setprop_prototype
//
// returns false if property is found on the prototype chain,
// and is a setter function, which has already been called.
//
// returns false if a read-only property is found on the
// prototype chain:  a non-writable value, or a geter-only
// property.  in strict mode, a TypeError is thrown.
//
// returns false if a proxy object with a set trap is found
// on the prototype chain.  the set trap may throw errors.
//
// othewise, returns true.
//
// ------------------------------------------------------------

static bool js_setprop_prototype (js_environ *env,
                            js_val obj, js_obj *obj_ptr,
                            int64_t prop_key, js_val value,
                        js_val prop_name_in_case_of_error) {

    if (!obj_ptr) {
        // there is no prototype chain if prototype is null,
        // and the property may be set on the initial object
        return true;
    }

    for (;;) {

        uintptr_t proto = (uintptr_t)obj_ptr->proto;
        js_val old_val = js_deleted;

        if ((proto & 7) == js_obj_is_array) {

            const uint32_t prop_idx =
                js_str_is_length_or_number(env,
                    js_make_primitive(
                        prop_key, js_prim_is_string));

            if (prop_idx < js_len_index) {
                // returns js_deleted if no such element
                old_val = js_arr_get(obj, prop_idx);
            }

        } else if ((proto & 7) == js_obj_is_proxy) {

            const js_val prop =
                    js_make_primitive(
                        prop_key, js_prim_is_string);

            if (js_setprop_proxy(env, obj, prop, value)) {
                // proxy trap was called
                return false;
            }
        }

        if (old_val.raw == js_deleted.raw) {

            int64_t idx_or_ptr;
            if (js_shape_value(
                    obj_ptr->shape, prop_key, &idx_or_ptr)
                                            && idx_or_ptr < 0) {

                old_val = obj_ptr->values[~idx_or_ptr];
            }
        }

        if (old_val.raw != js_deleted.raw) {

            if (!js_is_descriptor(old_val)) {
                // we found a property without a descriptor,
                // which is the same as a writable property.
                // therefore it is valid to set the property
                // on the initial object as an own property.
                return true;
            }

            // check if the property descriptor is a setter
            // function and if so, call it;  otherwise check
            // if the property is writable, in which case it
            // may be set on the initial object
            return js_descr_check_writable(
                        env, old_val, obj, value, false,
                        prop_name_in_case_of_error);
        }

        // if we reached the end of the prototype chain,
        // then the property may be set on the initial object
        proto &= ~7;
        if (!proto)
            return true;
        obj_ptr = (js_obj *)proto;
    }
}

// ------------------------------------------------------------
//
// js_throw_if_primitive
//
// strict mode does not permit properties to be set on
// primitive values, so throw a TypeError in such a case
//
// ------------------------------------------------------------

static void js_throw_if_primitive (js_environ *env, js_val obj) {

    if (!js_is_object(obj)) {

        js_throw_if_strict_0("TypeError_primitiveProperty");
    }
}
