
// ------------------------------------------------------------
//
// debugging utilities
//
// ------------------------------------------------------------

// ------------------------------------------------------------
//
// js_object_print
//
// ------------------------------------------------------------

static void js_object_print (js_environ *env, const js_obj *obj) {

    printf("{");
    int count = 0;
    int index = 0;
    for (;;) {
        int64_t prop_key, idx_or_ptr;
        if (!intmap_get_next(obj->shape->props, &index,
                             (uint64_t *)&prop_key,
                             (uint64_t *)&idx_or_ptr))
            break;
        if (idx_or_ptr >= 0)
            continue;
        js_val value = obj->values[~idx_or_ptr];
        if (value.raw == js_deleted.raw)
            continue;
        if (js_is_descriptor(value)) {
            const js_descriptor *descr = js_get_pointer(value);
            const int flags = js_descr_flags_without_setter(descr)
                            & (js_descr_value | js_descr_enum);
            //if (flags != (js_descr_value | js_descr_enum))
            if (flags != js_descr_value)
                continue;
        }
        if (count++ > 0)
            printf(",");
        const objset_id *text = (const objset_id *)prop_key;
        printf(" %*.*ls : ",
            text->len/2, text->len/2,
            (wchar_t *)text->data);
        if (js_is_descriptor(value)) {
            const js_descriptor *descr = js_get_pointer(value);
            value = descr->data_or_getter;
        }
        js_print(env, value);
    }
    if (count > 0)
        printf(" ");
    printf("}");
}

// ------------------------------------------------------------
//
// js_arr_print
//
// ------------------------------------------------------------

static void js_arr_print (js_environ *env, const js_arr *arr) {

    printf("[");
    int count = 0;
    // length_descr[0] holds the length as a double
    uint32_t length = (uint32_t)arr->length_descr[0].num;
    uint32_t capacity = arr->capacity;
    for (uint32_t index = 0; index < length; index++) {
        js_val value = index < capacity
                     ? arr->values[index] : js_deleted;
        if (js_is_descriptor(value)) {
            const js_descriptor *descr = js_get_pointer(value);
            const int flags = js_descr_flags_without_setter(descr);
            if (!(flags & js_descr_value))
                continue;
        }
        if (count++ == 0)
            printf(" ");
        else
            printf(", ");
        if (value.raw == js_deleted.raw) {
            printf("<empty>");
            continue;
        }
        if (js_is_descriptor(value)) {
            const js_descriptor *descr = js_get_pointer(value);
            value = descr->data_or_getter;
        }
        js_print(env, value);
    }
    if (count > 0)
        printf(" ");
    printf("]");
}

// ------------------------------------------------------------
//
// js_print
//
// ------------------------------------------------------------

static void js_print (js_environ *env, js_val val) {

    if (val.raw == -1)
        printf("\n");
    else if (js_is_number(val)) {
        const double num_val = js_get_number(val);
        if (isnan(num_val))
            printf("NaN");
        else if (isinf(num_val)) {
            if (num_val < 0)
                printf("-Infinity");
            else
                printf("Infinity");
        } else
            printf("%.1f", js_get_number(val));
    } else if (js_is_primitive(val)) {
        if (js_get_primitive_type(val) == js_prim_is_bigint)
            val = js_big_tostring(env, val, 10);
        if (js_get_primitive_type(val) == js_prim_is_string) {
            const objset_id *id = js_get_pointer(val);
            const wchar_t *txt = (const wchar_t *)id->data;
            int len = id->len >> 1;
            printf("%*.*ls", len, len, txt);
        } else if (js_get_primitive_type(val) == js_prim_is_symbol) {
            const objset_id *id = js_get_pointer(val);
            const wchar_t *txt = (const wchar_t *)id->data;
            int len = id->len >> 1;
            printf("Symbol('%*.*ls')", len, len, txt);
        } else
            printf("?unknown primitive?");
    } else if (js_is_object(val)) {
        void *obj_ptr = js_get_pointer(val);
        if (js_obj_is_exotic(obj_ptr, js_obj_is_array)) {
            js_arr_print(env, obj_ptr);
            if (((js_obj *)obj_ptr)->shape == env->arr_shape)
                return;
        }
        if (js_obj_is_exotic(obj_ptr, js_obj_is_function))
            printf("function ");
        js_object_print(env, obj_ptr);
    } else if (val.raw == js_true.raw) {
        printf("true");
    } else if (val.raw == js_false.raw) {
        printf("false");
    } else if (val.raw == js_null.raw) {
        printf("null");
    } else if (js_is_undefined(val)) {
        printf("undefined");
    } else {
        printf("?unknown type?");
    }
}
