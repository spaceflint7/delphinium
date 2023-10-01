
// ------------------------------------------------------------
//
// javascript ordinary object, which also serves as a
// base for the exotic objects: array, function, proxy.
//
// implemented using the 'shape' approach, in which the
// object holds property in a flat array, and does not
// list property keys.  instead, it points to a hash map
// a (embedded in a 'js_shape' struct), which maps from
// property key to array index.
//
// ------------------------------------------------------------

// ------------------------------------------------------------
//
// js_newexobj
//
// internal function to allocate and initialize an ordinary
// or exotic object.  'proto' parameter indirectly specifies
// the kind of exotic object, via the low bits in pointers.
// see also initializations of the various 'env->???_proto'
// pointers throughout this runtime code.  'shape' parameter
// specifies the shape descriptor, which specifies the number
// of value slots (for properties) to allocate in the object.
// note that initially, these slots are part of the object
// structure, but may point elsewhere as the object mutates.
//
// ------------------------------------------------------------

static void *js_newexobj (
                    js_obj *proto, const js_shape *shape) {

    // the low bits of the prototype pointer determine
    // the type of exotic object, and this in turn
    // determines the size of the object structure.
    // see also js_shape_switch ()
    const int exotic_type = (uintptr_t)proto & 7;
    const int struct_size = js_obj_struct_size(exotic_type);

    int max_values = shape->num_values;
    if (exotic_type == js_obj_is_ordinary && max_values == 0) {
        // if allocating an ordinary empty object,
        // make room in advance for a few properties
        max_values = js_obj_grow_factor;
    }

    // noe that the values are initially allocated just past
    // the object structure.  see also js_shape_switch ()
    js_obj *obj = js_malloc(struct_size
                          + max_values * sizeof(js_val));

    obj->proto = proto;
    obj->values = (js_val *)((uintptr_t)obj + struct_size);
    js_shape_set_in_obj(obj, shape);
    obj->max_values = max_values;

    return obj;
}

// ------------------------------------------------------------
//
// js_newobj API
//
// ------------------------------------------------------------

js_val js_newobj (js_environ *env, const js_shape *shape, ...) {

    js_obj *obj_ptr = (js_obj *)js_newexobj(env->obj_proto, shape);
    int num_values = shape->num_values;
    if (num_values) {
        js_val *values = obj_ptr->values;
        va_list args;
        va_start(args, shape);
        while (num_values-- > 0)
            *values++ = va_arg(args, js_val);
        va_end(args);
    }
    return js_make_object(obj_ptr);
}

// ------------------------------------------------------------
//
// js_newobj2_xetter
//
// ------------------------------------------------------------

static void js_newobj2_xetter (js_environ *env, js_val obj,
                               js_val prop, js_val value,
                               bool value_is_setter) {

    js_val getter = js_undefined;
    js_val setter = js_undefined;
    int flags;

    js_throw_if_notfunc(env, value);

    js_val *ptr_value = js_ownprop(env, obj, prop, true);
    if (js_is_descriptor(*ptr_value)) {

        js_descriptor *descr =
            (js_descriptor *)js_get_pointer(*ptr_value);

        flags = js_descr_flags_without_setter(descr);
        if (value_is_setter) {

            setter = value;
            flags |= js_descr_setter;
            if (flags & js_descr_getter) {
                getter = js_make_object(
                    js_get_pointer(descr->data_or_getter));
            }

        } else {

            getter = value;
            flags |= js_descr_getter;
            if (flags & js_descr_setter) {
                setter = js_make_object(
                    js_get_pointer(descr->setter_and_flags));
            }
        }

        flags |= js_descr_enum | js_descr_config;
        flags &= ~(js_descr_value | js_descr_write);

        js_setdescr(descr, flags, getter, setter);

    } else {

        if (value_is_setter) {

            setter = value;
            flags  = js_descr_setter;

        } else {

            getter = value;
            flags  = js_descr_getter;
        }

        flags |= js_descr_enum | js_descr_config;
        *ptr_value = js_make_descriptor(
                        js_newdescr(flags, getter, setter));
    }
}

// ------------------------------------------------------------
//
// js_newobj2_spread
//
// implement the spread syntax in an object expression,
// which is a shallow clone of all enumerable own
// properties from the spread source object
//
// an optional list of properties to skip is used by
// js_restobj (), which itself implements last/rest
// property in object destructuring, see also
// assignment_expression () in expression_writer.js
//
// ------------------------------------------------------------

static void js_newobj2_spread (js_environ *env,
                               js_val dst, js_val src,
                               js_val *skip_list) {

    js_obj *src_ptr = js_get_pointer(src);
    int index = 0;

    uint32_t arr_length = 0;
    if (js_obj_is_exotic(src_ptr, js_obj_is_array)) {
        int64_t int_len =
            ((js_arr *)src_ptr)->length_descr[0].num;
        if (int_len > 0)
            arr_length = int_len;
    }

    for (;;) {
        js_val prop, src_value;

        if (likely(arr_length == 0)) {

            int64_t prop_key, idx_or_ptr;
            if (!js_shape_get_next(src_ptr->shape,
                           &index, &prop_key, &idx_or_ptr))
                break;
            if (idx_or_ptr >= 0)
                continue;
            src_value = src_ptr->values[~idx_or_ptr];
            if (src_value.raw == js_deleted.raw)
                continue;
            prop = js_make_primitive(
                        prop_key, js_prim_is_string);

        } else {

            uint32_t prop_idx = ((uint32_t)index) + 1;
            src_value = js_arr_get(src, prop_idx);
            if (++index == arr_length) {
                index = 0;
                arr_length = 0;
            }
            prop = js_str_index(env, prop_idx);
        }

        //
        //
        //

        if (js_is_descriptor(src_value)) {
            js_descriptor *descr = js_get_pointer(src_value);
            const int flags = js_descr_flags_without_setter(descr);
            if (!(flags & js_descr_enum))
                continue;
            if (flags & js_descr_value)
                src_value = descr->data_or_getter;
            else if (flags & js_descr_getter) {
                int64_t dummy_shape_cache;
                src_value = js_getprop(env, src, prop,
                                       &dummy_shape_cache);
            } else {
                // the property is set to undefined if there
                // is neither a value nor a getter function
                src_value = js_undefined;
            }
        }

        if (skip_list) {    // called from js_restobj ()
            int i = 0;
            for (;; ++i) {
                const js_val prop2 = skip_list[i];
                if (!prop2.raw)
                    break;
                if (prop2.raw == prop.raw) {
                    i = -1;
                    break;
                }
            }
            if (i == -1)
                continue; // skip property in list
        }

        js_val *dst_value = js_ownprop(env, dst, prop, true);
        *dst_value = src_value;
    }
}

// ------------------------------------------------------------
//
// js_newobj2 (API)
//
// ------------------------------------------------------------

js_val js_newobj2 (js_environ *env, js_val obj, js_val proto,
                   int num_props, ...) {

    // this function is only ever called by code generated
    // by object_expression () in expression_writer.js,
    // so we don't have to validate 'obj' or any parameters
    js_obj *obj_ptr = js_get_pointer(obj);
    if (js_is_object(proto))
        obj_ptr->proto = js_get_pointer(proto);
    else if (proto.raw == js_null.raw)
        obj_ptr->proto = NULL;

    if (num_props) {
        va_list args;
        va_start(args, num_props);
        while (num_props-- > 0) {

            js_val prop = va_arg(args, js_val);
            js_val value = va_arg(args, js_val);

            bool setter = (prop.raw == js_next_is_setter.raw);
            if (prop.raw == js_next_is_getter.raw || setter) {

                // special flag passed by object_expression ()
                // to indicate a getter or a setter property
                prop = value;
                value = va_arg(args, js_val);
                js_newobj2_xetter(env, obj, prop, value, setter);

            } else if (prop.raw == js_next_is_spread.raw) {

                // special flag passed by object_expression ()
                // to indicate spread syntax
                js_newobj2_spread(env, obj, value, NULL);

            } else {

                // set a plain value.  but check if there was
                // a previous declaration for the property as
                // a descriptor, in which case, free its memory

                js_val *ptr_value =
                            js_ownprop(env, obj, prop, true);

                if (js_is_descriptor(*ptr_value))
                    free(js_get_pointer(*ptr_value));

                *ptr_value = value;
            }
        }
        va_end(args);
    }
    return obj;
}

// ------------------------------------------------------------
//
// js_restobj
//
// ------------------------------------------------------------

js_val js_restobj (js_environ *env, js_val from_obj,
                   int skip_count, ...) {

    js_val skip_list[skip_count];
    js_val *skip_list_ptr;
    if (!skip_count)
        skip_list_ptr = NULL;
    else {
        va_list args;
        va_start(args, skip_count);
        for (int i = 0; i < skip_count; ++i) {
            // make sure the property is interned,
            // for comparison in js_newobj2_spread ()
            skip_list[i] = js_make_primitive(
                js_shape_key(env, va_arg(args, js_val)),
                js_prim_is_string);
        }
        va_end(args);
        skip_list[skip_count].raw = 0;
        skip_list_ptr = skip_list;
    }

    js_val new_obj = js_make_object(
            js_newexobj(env->obj_proto, env->shape_empty));
    js_newobj2_spread(env, new_obj, from_obj, skip_list_ptr);
    return new_obj;
}

// ------------------------------------------------------------
//
// js_private_object
//
// this utility function connects or retrieves a hidden and
// private value that is attached to a user-visible empty
// object.  this enables logic in runtime2.js to maintain
// private state in object that are passed to user code.
//
// parameter 1 is the user-visible object.
// parameter 2 is a symbol that identifies the private data.
// parameter 3 is the private data to attach to the object.
//
// if parameter 3 is neither undefined nor null, it is
// attached to the user-visible object, which gets marked
// with the symbol in parameter 2.  the user-visible object
// must be empty, and must not have private data attached.
//
// if parameter 3 is undefined, the function checks if the
// object in parameter 1 is marked with the symbol passed in
// parameter 2, in which case it returns the private value
// attached to the object.  otherwise, it throws an error.
//
// if parameter 3 is null, the function does the same as
// above, except it returns undefined instead of throwing.
//
// ------------------------------------------------------------

static js_val js_private_object (js_c_func_args) {

    js_val third_arg = js_undefined;
    js_val ret_val = js_undefined;

    do {

        // extract first parameter, the user-visible object
        js_link *arg_ptr = stk_args->next;
        if (arg_ptr == js_stk_top)
            break;
        if (!js_is_object(arg_ptr->value))
            break;
        js_obj *obj_ptr =
                (js_obj *)js_get_pointer(arg_ptr->value);

        // extract second parameter, the object type symbol
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        js_val private_object_type_symbol = arg_ptr->value;

        // extract third parameter, the create-object flag
        arg_ptr = arg_ptr->next;
        if (arg_ptr != js_stk_top)
            third_arg = arg_ptr->value;

        if (!js_is_undefined_or_null(third_arg)) {

            if (obj_ptr->shape->num_values != 0)
                break;
            if (obj_ptr->max_values & js_obj_not_extensible)
                break;

            // switch to an object with two hidden values,
            // see also shape.c
            js_shape_switch(obj_ptr, 0,
                            private_object_type_symbol,
                            env->shape_private_object);

            if (obj_ptr->shape->num_values < 2
                    || obj_ptr->max_values < 2)
                break;

            ret_val = obj_ptr->values[1] = third_arg;

        } else {

            // if object has at least two values, and value[0]
            // matches the value passed in the second paremeter,
            // then we have a private object ref, in value[1]

            if (obj_ptr->values[0].raw !=
                            private_object_type_symbol.raw)
                break;

            ret_val = obj_ptr->values[1];
        }

    } while (0);

    if (js_is_undefined_or_null(ret_val)
                            && third_arg.raw != js_null.raw) {
        // throw on error, except if the third parameter == null
        js_callthrow("TypeError_incompatible_object");
    }

    js_return(ret_val);
}
