
// ------------------------------------------------------------
//
// js_funcwith
//
// a utility function which ensures that a valid function
// parameter was passed to any of the 'with' functions below.
//
// ------------------------------------------------------------

static js_func *js_funcwith (js_val func,
                             bool return_null_if_strict) {

    if (js_is_object(func)) {

        const js_obj *obj_ptr = (js_obj *)js_get_pointer(func);
        if (js_obj_is_exotic(obj_ptr, js_obj_is_function)) {

            js_func *func_obj = (js_func *)obj_ptr;
            if (!(func_obj->flags & js_strict_mode)) {

                return func_obj;

            } else if (return_null_if_strict)
                return NULL;
        }
    }

    fprintf(stderr, "With error!\n");
    exit(1);
}

// ------------------------------------------------------------
//
// js_ignorewith
//
// if the 'with' scope object contains the 'unscopables'
// property, set to some object value;  and if that object
// has the lookup property, set to some truthy value;  then
// it means we have to pretend that the particular lookup
// property was not found on the 'with' scope object.
//
// ------------------------------------------------------------

static bool js_ignorewith (
        js_environ *env, js_val scope_obj, js_val prop) {

    int64_t dummy_shape_cache;

    js_val unscopables = js_getprop(env, scope_obj,
                                    env->sym_unscopables,
                                   &dummy_shape_cache);

    return (js_is_object(unscopables) && js_is_truthy(
                js_getprop(env, unscopables, prop,
                          &dummy_shape_cache)));
}

// ------------------------------------------------------------
//
// js_scopewith
//
// manipulate the 'with' scope chain in a function object.
// used by with_statement () in statement_writer.js to
// implement the actual 'with' keyword.  but also used by
// function_expression () in expression_writer.js, for the
// case where a function is declared inside a 'with' block:
//     with ({x:'!'}) {(()=> (()=> (()=>x)())())()}
//
// ------------------------------------------------------------

js_val js_scopewith (js_environ *env, js_val func, js_val obj) {

    js_func *func_obj = js_funcwith(func, false);
    js_link *with_scope;

    js_throw_if_nullobj(env, obj);

    if (obj.raw == js_discard_scope.raw) {

        // request to discard the most recent 'with'
        // scope, called by with_statement () upon
        // termination of the 'with' block

        with_scope = func_obj->u.with_scope;
        if (with_scope) {
            func_obj->u.with_scope = with_scope->next;
            free(with_scope);
        }

    } else if (js_is_flagged_pointer(obj)) {

        // request to copy the 'with' scope from the
        // declaring function into a new function object.
        // this is called by function_expression ()

        js_link *new_with_scope = NULL;
        js_func *obj_as_func = js_funcwith(obj, true);
        js_link *old_with_scope = obj_as_func
                    ? obj_as_func->u.with_scope : NULL;

        while (old_with_scope) {

            with_scope = js_malloc(sizeof(js_link));
            with_scope->value = old_with_scope->value;
            with_scope->next = NULL;
            with_scope->prev = NULL;
            with_scope->depth = 0;

            if (!new_with_scope)
                func_obj->u.with_scope = with_scope;
            else
                new_with_scope->next = with_scope;

            new_with_scope = with_scope;
            old_with_scope = old_with_scope->next;
        }

    } else {

        // insert a new 'with' scope element at the head
        // of the 'with' scope chain, which is attached
        // to the function object. see with_statement ()

        with_scope = js_malloc(sizeof(js_link));
        with_scope->value = obj;
        with_scope->next = func_obj->u.with_scope;
        with_scope->prev = NULL;
        with_scope->depth = 0;

        func_obj->u.with_scope = with_scope;
    }

    return func;
}

// ------------------------------------------------------------
//
// js_getwith
//
// ------------------------------------------------------------

js_val js_getwith (js_environ *env, js_val func,
                   js_val prop, js_val *ptr_local2) {

    const js_func *func_obj = js_funcwith(func, false);
    int64_t dummy_shape_cache;

    js_link *with_scope = func_obj->u.with_scope;
    while (with_scope) {

        js_val scope_obj = with_scope->value;
        js_val check_obj = js_is_object(scope_obj) ? scope_obj
            : js_make_object(js_get_primitive_proto(env, scope_obj));

        if (    js_hasprop(env, check_obj, prop)
        &&  !js_ignorewith(env, check_obj, prop)) {

            return js_getprop(env, scope_obj, prop,
                             &dummy_shape_cache);
        }

        with_scope = with_scope->next;
    }

    // in non-strict mode, the last object searched for
    // unqualified lookup is the global object, and if
    // the lookup still fails, ReferenceError is thrown

    if (!js_hasprop(env, env->global_obj, prop)) {

        if (ptr_local2) {
            // non-NULL if function process_references ()
            // in variable_resolver.js found a local var,
            // outside the 'with' block, which has same
            // name/identifier as the property to 'get'.
            // return the local if property is not found.
            return *ptr_local2;
        }

        js_callshadow(
            env, "ReferenceError_not_defined", prop);
    }

    return js_getprop(env, env->global_obj, prop,
                      &dummy_shape_cache);
}

// ------------------------------------------------------------
//
// js_setwith
//
// ------------------------------------------------------------

js_val js_setwith (js_environ *env, js_val func,
                   js_val prop, js_val *ptr_local2,
                   js_val value) {

    const js_func *func_obj = js_funcwith(func, false);
    int64_t dummy_shape_cache;

    js_link *with_scope = func_obj->u.with_scope;
    while (with_scope) {

        js_val scope_obj = with_scope->value;
        js_val check_obj = js_is_object(scope_obj) ? scope_obj
            : js_make_object(js_get_primitive_proto(env, scope_obj));

        if (    js_hasprop(env, check_obj, prop)
        &&  !js_ignorewith(env, check_obj, prop)) {

            return js_setprop(env, scope_obj, prop, value,
                              &dummy_shape_cache);
        }

        with_scope = with_scope->next;
    }

    if (ptr_local2) {
        // non-NULL if function process_references ()
        // in variable_resolver.js found a local var,
        // outside the 'with' block, which has same
        // name/identifier as the property to 'set'.
        // update the local if property is not found.
        return (*ptr_local2 = value);
    }

    // in non-strict mode, the last object used to try
    // to set the property, is the global object

    return js_setprop(env, env->global_obj, prop, value,
                      &dummy_shape_cache);
}

// ------------------------------------------------------------
//
// js_delwith
//
// ------------------------------------------------------------

js_val js_delwith (js_environ *env, js_val func, js_val prop) {

    const js_func *func_obj = js_funcwith(func, false);

    js_link *with_scope = func_obj->u.with_scope;
    while (with_scope) {

        js_val scope_obj = with_scope->value;
        js_val check_obj = js_is_object(scope_obj) ? scope_obj
            : js_make_object(js_get_primitive_proto(env, scope_obj));

        if (    js_hasprop(env, check_obj, prop)
        &&  !js_ignorewith(env, check_obj, prop)) {

            return js_delprop(env, scope_obj, prop);
        }

        with_scope = with_scope->next;
    }

    // in non-strict mode, the last object used to try
    // to delete the property, is the global object

    return js_delprop(env, env->global_obj, prop);
}

// ------------------------------------------------------------
//
// js_callwith
//
// ------------------------------------------------------------

js_val js_callwith (js_environ *env, js_val func,
                    js_val prop, js_val *ptr_local2,
                    js_link *stk_args) {

    const js_func *func_obj = js_funcwith(func, false);
    int64_t dummy_shape_cache;
    js_val call_func;

    js_link *with_scope = func_obj->u.with_scope;
    while (with_scope) {

        js_val scope_obj = with_scope->value;
        js_val check_obj = js_is_object(scope_obj) ? scope_obj
            : js_make_object(js_get_primitive_proto(env, scope_obj));

        if (    js_hasprop(env, check_obj, prop)
        &&  !js_ignorewith(env, check_obj, prop)) {

            call_func = js_getprop(env, scope_obj, prop,
                                   &dummy_shape_cache);

            return js_callfunc(
                env, call_func, scope_obj, stk_args);
        }

        with_scope = with_scope->next;
    }

    // in non-strict mode, the last object searched for
    // unqualified lookup is the global object, and if
    // the lookup still fails, ReferenceError is thrown

    if (!js_hasprop(env, env->global_obj, prop)) {

        if (ptr_local2) {
            // non-NULL if function process_references ()
            // in variable_resolver.js found a local var,
            // outside the 'with' block, which has same
            // name/identifier as the property to 'call'.
            // call the local if property is not found.
            return js_callfunc(
                env, *ptr_local2, js_undefined, stk_args);
        }

        js_callshadow(
            env, "ReferenceError_not_defined", prop);
    }

    call_func = js_getprop(env, env->global_obj, prop,
                           &dummy_shape_cache);

    // the function is called with 'this' set to 'undefined'
    return js_callfunc(
                env, call_func, js_undefined, stk_args);
}
