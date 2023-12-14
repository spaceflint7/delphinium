
// ------------------------------------------------------------
//
// descriptor
//
// ------------------------------------------------------------

typedef struct js_descriptor js_descriptor;
struct js_descriptor {

    js_val data_or_getter;
    js_val setter_and_flags;
};

#define js_is_descriptor(v)     js_is_flagged_pointer(v)
#define js_make_descriptor(p)   js_make_flagged_pointer(p)

#define js_descr_flags_without_setter(descr) \
    ((descr)->setter_and_flags.raw >> 56)

// ------------------------------------------------------------
//
// forward declarations
//
// ------------------------------------------------------------

static js_val js_defineProperty (js_c_func_args);

static js_val js_getOwnProperty_2 (
                            js_environ *env, js_val value);

/*static js_val js_preventExtensions (js_c_func_args);

static js_val js_isExtensible (js_c_func_args);*/

// ------------------------------------------------------------
//
// js_setdescr
//
// initialize the descriptor structure at 'descr' with
// specified flags and values.
//
// ------------------------------------------------------------

static js_descriptor *js_setdescr (
                            js_descriptor *descr, int flags,
                            js_val val1, js_val val2) {

    const uint64_t val2raw = (flags & js_descr_setter)
                           ? (uintptr_t)js_get_pointer(val2) : 0;
    descr->data_or_getter.raw   = val1.raw;
    descr->setter_and_flags.raw = val2raw
                                | ((uint64_t)(flags & 0xFF) << 56);
    return descr;
}

// ------------------------------------------------------------
//
// js_newdescr
//
// allocate and initialize a new descriptor structure with
// specified flags and values.
//
// ------------------------------------------------------------

static js_descriptor *js_newdescr (
                        int flags, js_val val1, js_val val2) {

    return js_setdescr(js_malloc(sizeof(js_descriptor)),
                       flags, val1, val2);
}

// ------------------------------------------------------------
//
// js_descr_array_length_writable
//
// ------------------------------------------------------------

static int js_descr_array_length_writable (
                                        const js_arr *arr) {

    const js_descriptor *length_descr =
                    (js_descriptor *)arr->length_descr;
    const int flags =
            js_descr_flags_without_setter(length_descr);
    return (flags &js_descr_write);
}

// ------------------------------------------------------------
//
// js_getOwnProperty - Object.getOwnPropertyDescriptor
//
// ------------------------------------------------------------

static js_val js_getOwnProperty (js_c_func_args) {

    js_val obj_val = js_undefined;
    js_val prop_val = js_undefined;
    js_val ret_val;

    js_link *arg_ptr = stk_args;
    if ((arg_ptr = arg_ptr->next) != js_stk_top) {
        obj_val = arg_ptr->value;
        if ((arg_ptr = arg_ptr->next) != js_stk_top)
            prop_val = arg_ptr->value;
    }
    js_throw_if_notobj(env, obj_val);

    js_val *prop_ptr = js_ownprop(env, obj_val, prop_val, false);
    if (prop_ptr)
        ret_val = js_getOwnProperty_2(env, *prop_ptr);
    else {
        // property not found, or this is a primitive value
        ret_val = js_undefined;
    }

    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_getOwnProperty_2 - Object.getOwnPropertyDescriptor
//
// ------------------------------------------------------------

static js_val js_getOwnProperty_2 (
                            js_environ *env, js_val value) {

    js_val writable;
    js_val getter, setter;
    js_val enumerable, configurable;

    if (js_is_descriptor(value)) {

        js_descriptor *descr = js_get_pointer(value);
        const int flags = js_descr_flags_without_setter(descr);

        enumerable   = (flags & js_descr_enum)
                     ? js_true : js_false;

        configurable = (flags & js_descr_config)
                     ? js_true : js_false;

        if (flags & (js_descr_getter | js_descr_setter)) {

            value = writable = js_deleted;
            getter = (descr->data_or_getter.raw & 0xFFFFFFFFFFF8)
                   ? js_make_object(
                        js_get_pointer(descr->data_or_getter))
                   : js_undefined;
            setter = (descr->setter_and_flags.raw & 0xFFFFFFFFFFF8)
                   ? js_make_object(
                        js_get_pointer(descr->setter_and_flags))
                   : js_undefined;

        } else {

            value = descr->data_or_getter;
            writable = (flags & js_descr_write)
                     ? js_true : js_false;
            getter = setter = js_deleted;
        }

    } else {

        writable = enumerable = configurable = js_true;
        getter = setter = js_deleted;

        if (value.raw == js_deleted.raw)
            value = js_undefined;
    }

    js_obj *obj = js_newexobj(env->obj_proto, env->descr_shape);
    js_val *values = obj->values;

    values[0] = value;
    values[1] = writable;
    values[2] = getter;
    values[3] = setter;
    values[4] = enumerable;
    values[5] = configurable;

    return js_make_object(obj);
}

// ------------------------------------------------------------
//
// js_property_flags
//
// bit 0x0001 - object is valid
//     0x0002 - object is an array
//     0x0004 - property is a symbol
//     0x0008 - property is integer array index
//     0x0010 - property is set in object
//     0x0020 - property is a data value
//     0x0100 - property is enumerable
//
// ------------------------------------------------------------

static js_val js_property_flags (js_c_func_args) {

    js_val obj_val = js_undefined;
    js_val prop_val = js_undefined;

    int flags = 0;
    const int _obj_is_valid        = 0x0001;
    const int _obj_is_array        = 0x0002;
    const int _prop_is_symbol      = 0x0004;
    const int _prop_is_array_index = 0x0008;
    const int _prop_is_set_in_obj  = 0x0010;
    const int _prop_is_data_value  = 0x0020;
    const int _prop_is_enumerable  = 0x0100;

    js_link *arg_ptr = stk_args;
    if ((arg_ptr = arg_ptr->next) != js_stk_top) {
        obj_val = arg_ptr->value;
        if ((arg_ptr = arg_ptr->next) != js_stk_top)
            prop_val = arg_ptr->value;
    }

    if (js_is_object(obj_val)) {
        flags |= _obj_is_valid;

        const js_obj *obj_ptr = js_get_pointer(obj_val);
        if (js_obj_is_exotic(obj_ptr, js_obj_is_array))
            flags |= _obj_is_array;

        js_val value = js_deleted;

        if (js_is_primitive_symbol(prop_val))
            flags |= _prop_is_symbol;

        else {

            uint32_t prop_idx =
                js_str_is_length_or_number(env, prop_val);

            if (prop_idx < js_len_index) {
                flags |= _prop_is_array_index;

                if (flags & _obj_is_array)
                    value = js_arr_get(obj_val, prop_idx);
            }
        }

        if (value.raw == js_deleted.raw) {

            js_val *value_ptr =
                js_ownprop(env, obj_val, prop_val, false);
            if (value_ptr)
                value = *value_ptr;
        }

        if (value.raw != js_deleted.raw) {
            flags |= _prop_is_set_in_obj;

            if (!js_is_descriptor(value)) {
                flags |= _prop_is_data_value
                      |  _prop_is_enumerable;

            } else {
                const js_descriptor *descr =
                            js_get_pointer(value);
                int descr_flags =
                    js_descr_flags_without_setter(descr);

                if (descr_flags & js_descr_value)
                    flags |= _prop_is_data_value;

                if (descr_flags & js_descr_enum)
                    flags |= _prop_is_enumerable;
            }
        }
    }

    js_return(js_make_number(flags));
}

// ------------------------------------------------------------
//
// js_descr_check_writable
//
// ------------------------------------------------------------

static bool js_descr_check_writable (
            js_environ *env, js_val descr_val, js_val obj,
            js_val value, bool can_update_value_in_descr,
            js_val prop_name_in_case_of_error) {

    // in strict mode, it is an error set a property that
    // is not writable, or getter but not a setter.  but if
    // not executing in strict mode, then silently ignored

    js_descriptor *descr = js_get_pointer(descr_val);
    const int flags = js_descr_flags_without_setter(descr);

    if (flags & js_descr_setter) {

        // call set function on the property
        js_val setter = js_make_object(js_get_pointer(
                                descr->setter_and_flags));
        js_callfunc1(env, setter, obj, value);
        return false;   // caller should not update property
    }

    if (flags & js_descr_write) {
        if (!can_update_value_in_descr)
            return true;    // caller should update property
        descr->data_or_getter = value;
        return false;   // caller should not update property
    }

    js_throw_if_strict_1("TypeError_readOnlyProperty",
                         prop_name_in_case_of_error);
    return false;       // caller should not update property
}

// ------------------------------------------------------------
//
// js_descr_init
//
// ------------------------------------------------------------

static void js_descr_init (js_environ *env) {

    // create a shape for an object with the properties
    // for a property descriptor

    js_val obj = js_emptyobj(env);
    js_newprop(env, obj, env->str_value)        = js_make_number(0.0);
    js_newprop(env, obj, env->str_writable)     = js_make_number(0.0);
    js_newprop(env, obj, env->str_get)          = js_make_number(0.0);
    js_newprop(env, obj, env->str_set)          = js_make_number(0.0);
    js_newprop(env, obj, env->str_enumerable)   = js_make_number(0.0);
    js_newprop(env, obj, env->str_configurable) = js_make_number(0.0);
    env->descr_shape = ((js_obj *)js_get_pointer(obj))->shape;
}
