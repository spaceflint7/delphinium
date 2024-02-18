
// ------------------------------------------------------------
//
// js_newarr
//
// ------------------------------------------------------------

js_val js_newarr (js_environ *env, int num_values, ...) {

    js_arr *arr = js_newexobj(
                    env, env->arr_proto, env->arr_shape);

    if (num_values > 0) {

        arr->capacity = arr->length = num_values;

        js_val *values = arr->values =
                js_malloc(num_values * sizeof(js_val));

        va_list args;
        va_start(args, num_values);
        while (num_values-- > 0)
            *values++ = va_arg(args, js_val);
        va_end(args);

    } else {

        arr->capacity = arr->length = 0;
        arr->values = NULL;
    }

    // create a descriptor for the 'length' property

    arr->super.values[0 /* length */] = js_make_descriptor(
            js_setdescr((js_descriptor *)arr->length_descr,
                        js_descr_value | js_descr_write,
                        js_make_number(arr->length),
                        js_make_number(0.0)));

    return js_gc_manage(env, js_make_object(arr));
}

// ------------------------------------------------------------
//
// js_restarr_stk
//
// ------------------------------------------------------------

js_val js_restarr_stk (js_environ *env, js_link *stk_ptr) {

    js_link *arg_ptr;

    int count = 0;
    for (arg_ptr = stk_ptr;
            arg_ptr != js_stk_top;
                arg_ptr = arg_ptr->next)
        count++;

    js_val arr_val = js_newarr(env, 0);
    if (count) {

        js_val *values = js_malloc(count * sizeof(js_val));

        int index = 0;
        for (arg_ptr = stk_ptr;
                arg_ptr != js_stk_top;
                    arg_ptr = arg_ptr->next)
            values[index++] = arg_ptr->value;

        js_arr *arr = (js_arr *)js_get_pointer(arr_val);
        arr->values = values;
        arr->length = count;
        js_compare_and_swap_32( // see js_arr_set ()
                    &arr->capacity, 0U, count);
        arr->length_descr[0].num = count;
    }

    return arr_val;
}

// ------------------------------------------------------------
//
// js_restarr_iter
//
// ------------------------------------------------------------

js_val js_restarr_iter (js_environ *env, js_val *iterator) {

    js_val arr_val = js_newarr(env, 0);
    uint32_t prop_idx = 0;
    while (iterator[0].raw) {
        js_arr_set(env, arr_val, ++prop_idx, iterator[2]);
        js_nextiter1(env, iterator);
    }
    return arr_val;
}

// ------------------------------------------------------------
//
// js_arr_get
//
// ------------------------------------------------------------

static js_val js_arr_get (js_val obj, uint32_t prop_idx) {

    js_arr *arr = (js_arr *)js_get_pointer(obj);

    // length_descr[0] holds the length as a double
    uint32_t length = (uint32_t)arr->length_descr[0].num;

    // prop_idx is one beyond requested index number
    const uint32_t index = prop_idx - 1;

    if (index < length && index < arr->capacity)
        return arr->values[index];

    // property index is not 'length', and is not an integer
    // index between 0 and the actual length of the array,
    // so we return false, so js_setprop_prototype () or
    // js_getprop () or can keep checking the prototype chain
    return js_deleted;
}

// ------------------------------------------------------------
//
// js_arr_set
//
// updates the array element at (prop_idx - 1).  grows the
// array if necessary, in which case, the 'length' property
// is updated.  this is a 'fast-path' call, and should not
// be used on arrays which contain descriptors.
//
// ------------------------------------------------------------

static void js_arr_set (js_environ *env, js_val obj,
                        uint32_t prop_idx, js_val value) {

    js_arr *arr = (js_arr *)js_get_pointer(obj);

    // length_descr[0] holds the length as a double
    uint32_t length = (uint32_t)arr->length_descr[0].num;
    const uint32_t capacity = arr->capacity;

    // if the element to set is at an index larger than
    // the current capacity of the array, then we need
    // to re-allocate the array

    js_val *values = arr->values;

    if (prop_idx > capacity) {

        // if the desired index/length is just beyond the
        // current end of the array, we assume the caller
        // would grow the array one element at a time,
        // and grow the array in larger incremeents.
        // otherwise, we grow to just the index requested.

        uint64_t new_capacity;
        if (prop_idx == capacity + 1) {
            // current capacity, divided by 16, and clamped
            // between 4 and 256, determines the increment
            uint32_t increment = capacity >> 4;
            if (increment < 4)
                increment = 4;
            else if (increment > 256)
                increment = 256;
            new_capacity = prop_idx + increment;
        } else
            new_capacity = prop_idx;

        if (new_capacity > js_max_index)
            new_capacity = js_max_index + 1;

        if (new_capacity < prop_idx) {
            js_callthrow("RangeError_array_length");
            return;
        }

        // re-allocate the values, and copy old elements

        js_val *new_values =
                    js_malloc(new_capacity * sizeof(js_val));
        memcpy(new_values, values, capacity * sizeof(js_val));

        // barrier to make sure the concurrent gc thread
        // can never see a combination of newer capacity
        // (which would be larger) but older 'values'
        arr->values = new_values;
        js_compare_and_swap_32(
                    &arr->capacity, 0U, new_capacity);

        // free old values
        js_gc_free(env, values);
        values = new_values;

        // clear elements from old length to new length
        js_val *ptr = &values[capacity];
        js_val *endptr = &values[new_capacity];
        while (ptr != endptr)
            *ptr++ = js_deleted;
    }

    // if the element to set is at an index larger than
    // the current length of the array, then we need to
    // make sure that all elements in between are 'empty'.
    //
    // note that prop_idx is index plus one (see also
    // js_str_is_length_or_number () in str.c)

    if (prop_idx > length) {

        // update length in the descriptor
        arr->length_descr[0].num = prop_idx;

        if (arr->length != -1U)
            arr->length = prop_idx;
    }

    // finally, set the requested element.

    values[prop_idx - 1] = value;
}

// ------------------------------------------------------------
//
// js_arr_check_length
//
// ------------------------------------------------------------

static uint32_t js_arr_check_length (
                        js_environ *env, js_val value) {

    // check that the passed value is a valid number for an
    // array length, converting non-numbers to a number if
    // necessary.

    uint32_t new_length;

    if (js_is_number(value))
        new_length = (uint32_t)value.num;

    else {

        // in the case of a non-numeric input:  as required by
        // the standard, in steps 3 and 4 of section 10.4.2.4
        // ArraySetLength:  convert input to a number _twice_.
        // first, implement ToUint32, which is toNumber() with
        // checks against infinites/NaNs.
        js_val v = js_tonumber(env, value); // 1st toNumber()
        if (~((v).raw) & js_exponent_mask)
            new_length = (uint32_t)v.num;
        else // all exponents bits are set
            new_length = 0;
        value = js_tonumber(env, value);    // 2nd toNumber()
    }

    if (js_make_number(new_length).raw != value.raw
                    || new_length > js_max_index) {

        js_callthrow("RangeError_array_length");
        new_length = 0;
    }

    return new_length;
}

// ------------------------------------------------------------
//
// js_arr_init
//
// ------------------------------------------------------------

static void js_arr_init (js_environ *env) {

    // create a shape for a newly-created array object
    // with the properties:  length

    js_val obj = js_emptyobj(env);
    js_obj *obj_ptr = js_get_pointer(obj);

    js_newprop(env, obj, env->str_length) = js_make_descriptor(
            js_newdescr(js_descr_value | js_descr_write,
                        js_make_number(0.0),
                        js_make_number(0.0)));

    env->arr_shape = obj_ptr->shape;

    // create the Array.prototype object.  note that
    // js_newarr () uses 'arr_proto' as the prototype,
    // so (temporarily) point that to Object.prototype.
    env->arr_proto = (void *)((uintptr_t)env->obj_proto
                   | js_obj_is_array);

    obj = js_newarr(env, 0);
    obj_ptr = js_get_pointer(obj);

    env->arr_proto = (void *)
                ((uintptr_t)obj_ptr | js_obj_is_array);

    // enable fast-path optimization on arrays.  used by
    // member_expression_array () in expression_writer.js
    // to check if fast-path optimization is permitted.
    // cleared if js code sets an indexer-like property
    // on Array.prototype or Object.prototype.
    env->fast_arr_proto = env->arr_proto;
}
