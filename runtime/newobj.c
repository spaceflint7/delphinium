
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

static void *js_newexobj (js_environ *env, js_obj *proto,
                          const js_shape *shape) {

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
    const int obj_len = struct_size
                      + max_values * sizeof(js_val);
    js_obj *obj = js_malloc(obj_len);

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

    js_obj *obj_ptr = (js_obj *)js_newexobj(
                            env, env->obj_proto, shape);
    int num_values = shape->num_values;
    if (num_values) {
        js_val *values = obj_ptr->values;
        va_list args;
        va_start(args, shape);
        while (num_values-- > 0)
            *values++ = va_arg(args, js_val);
        va_end(args);
    }
    return js_gc_manage(env, js_make_object(obj_ptr));
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

        } else {
            // called from js_newobj2 () for an object
            // that was already introduced to the gc
            if (js_is_object_or_primitive(src_value))
                js_gc_notify(env, src_value);
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
                js_val old_val = *ptr_value;
                *ptr_value = value;

                if (js_is_descriptor(old_val))
                    js_gc_free(env, js_get_pointer(old_val));

                if (js_is_object_or_primitive(value))
                    js_gc_notify(env, value);
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

    js_val new_obj = js_make_object(js_newexobj(env,
                        env->obj_proto, env->shape_empty));
    js_newobj2_spread(env, new_obj, from_obj, skip_list_ptr);
    return js_gc_manage(env, new_obj);
}

// ------------------------------------------------------------
//
// js_newprivobj
//
// ------------------------------------------------------------

static js_priv *js_newprivobj (
                    js_environ *env, js_val priv_type) {

    js_obj *priv_proto = (js_obj *)(
            ((uintptr_t)env->obj_proto & (~7))
                        | js_obj_is_private);
    js_priv *priv_obj = js_newexobj(
                            env, priv_proto,
                            env->shape_empty);
    priv_obj->type = priv_type;
    return priv_obj;
}

// ------------------------------------------------------------
//
// js_isprivobj
//
// ------------------------------------------------------------

static js_priv *js_isprivobj (
                    js_val obj_val, js_val priv_type) {

    if (js_is_object(obj_val)) {
        js_priv *priv = js_get_pointer(obj_val);
        if (js_obj_is_exotic(priv, js_obj_is_private)) {
            if (priv->type.raw == priv_type.raw)
                return priv;
        }
    }
    return NULL;
}

// ------------------------------------------------------------
//
// js_private_object
//
// this utility function creates a new private object
// with an attached hidden value, or returns the hidden
// value attached to a private object.
//
// if parameter 1 is a symbol, it specifies the type of
// the new private object which is created and returned.
// parameter 2 specifies the hidden value to attach.
//
// if parameter 1 is an object, then parameter 2 must
// match the type of the private object, in which case
// the hidden value is returned.  if the type does not
// match, throws an error, unless parameter 3 is null,
// in which case, returns undefined instead of throwing.
//
// ------------------------------------------------------------

static js_val js_private_object (js_c_func_args) {

    js_val ret_val = js_undefined;
    bool should_throw = true;

    do {

        // extract first parameter and second parameters
        js_link *arg_ptr = stk_args->next;
        if (arg_ptr == js_stk_top)
            break;
        js_val arg1 = arg_ptr->value;

        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        js_val arg2 = arg_ptr->value;

        if (js_is_primitive_symbol(arg1)) {
            //
            // create a new private exotic object
            //
            js_priv *priv = js_newprivobj(env, arg1);
                /*
            js_obj *priv_proto = (js_obj *)(
                    ((uintptr_t)env->obj_proto & (~7))
                                | js_obj_is_private);
            js_priv *priv = js_newexobj(
                                    env, priv_proto,
                                    env->shape_empty);
            priv->type = arg1;
                */
            priv->val_or_ptr.val = arg2;
            priv->gc_callback = NULL;

            ret_val = js_gc_manage(env,
                                js_make_object(priv));
            should_throw = false;
            break;
        }

        js_priv *priv = js_isprivobj(arg1, arg2);
        if (priv) {
            //
            // return hidden value
            //
            ret_val = priv->val_or_ptr.val;
            should_throw = false;
            break;
        }

        /*
        } else if (js_is_object(arg1)) {
            //
            // return hidden value
            //
            js_priv *priv = js_get_pointer(arg1);
            if (js_obj_is_exotic(priv, js_obj_is_private)
                    && priv->type.raw == arg2.raw) {

                ret_val = priv->val_or_ptr.val;
                should_throw = false;
                break;
            }
        }*/

        // don't throw if third parameter is null
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;
        if (arg_ptr->value.raw == js_null.raw)
            should_throw = false;

    } while (0);

    if (should_throw) {
        // throw on error
        js_callthrow("TypeError_incompatible_object");
    }

    js_return(ret_val);
}
