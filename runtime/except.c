
// ------------------------------------------------------------
//
// 'try' exception handler
//
// ------------------------------------------------------------

struct js_try {

    js_try *parent_try;
    js_link *stack_top;
    js_val throw_val;
    jmp_buf jmp_buf;
};

// ------------------------------------------------------------
//
// js_entertry
//
// ------------------------------------------------------------

jmp_buf *js_entertry (js_environ *env) {

    js_try *try = js_malloc(sizeof(js_try));
    try->parent_try = env->try_handler;
    try->stack_top = env->stack_top;
    try->throw_val = js_uninitialized;

    env->try_handler = try;
    return &try->jmp_buf;
}

// ------------------------------------------------------------
//
// js_leavetry
//
// ------------------------------------------------------------

js_val js_leavetry (js_environ *env) {

    js_try *try = env->try_handler;
    js_val throw_val = try->throw_val;
    env->try_handler = try->parent_try;
    js_gc_free(env, try);
    return throw_val;
}

// ------------------------------------------------------------
//
// js_throw_unwind
//
// ------------------------------------------------------------

static void js_throw_unwind (
                    js_environ *env, js_link *try_frame) {

    // if the current function was called via js_callnew (),
    // then it will not return properly and reset new.target
    env->new_target = js_undefined;

    // func_name_hint should be undefined, but make sure
    env->func_name_hint = js_undefined;

    // walk back along the stack, to reset the 'arguments'
    // property for called function objects, because the
    // longjmp () at the end of this function is going to
    // prevent the normal way that 'arguments' gets reset.
    // see also return_statement () in statement_writer.js
    for (js_link *stk_ptr = js_stk_top;;) {

        stk_ptr = js_walkstack(stk_ptr);
        if (stk_ptr == try_frame)
            break;

        const js_func *func =
                (js_func *)js_get_pointer(stk_ptr->value);
        if (!(func->flags & js_strict_mode)) {

            js_arguments2(
                    env, stk_ptr->value, js_null, NULL);
        }

        // normally the js_return macro resets the flagged
        // function object values on the stack, but it may
        // not get a chance to, due to throw; so we do it.
        stk_ptr->value = js_undefined;
    }
}

// ------------------------------------------------------------
//
// js_throw
//
// ------------------------------------------------------------

js_val js_throw (js_environ *env, js_val throw_val) {

    js_try *try = env->try_handler;
    try->throw_val = throw_val;

    // find the enclosing stack frame at the time when
    // the 'try' handler was established
    js_link *try_frame = try->stack_top;
    if (try_frame->prev) {

        // non-NULL 'prev' pointer means this isn't the
        // initial 'try' frame, established in init.c,
        // which means the program will not exit due to
        // unhandled exception, so unwind the exception.

        try_frame = js_walkstack(try_frame);
        js_throw_unwind(env, try_frame);
    }

    // jump to the execution point where the 'try'
    // was set up via setjmp ().  see also wmain ()
    // or try_statement () in statement_writer.js
    env->stack_top = try->stack_top;
    longjmp(try->jmp_buf, 1);
}

// ------------------------------------------------------------
//
// js_throw_if_notfunc
//
// ------------------------------------------------------------

static void js_throw_if_notfunc (
                        js_environ *env, js_val func_val) {

    if (js_is_object(func_val) &&
            js_obj_is_exotic(js_get_pointer(func_val),
                             js_obj_is_function))
        return;

    js_callthrow("TypeError_expected_function");
}

// ------------------------------------------------------------
//
// js_throw_if_notobj
//
// ------------------------------------------------------------

static void js_throw_if_notobj (js_environ *env, js_val obj_val) {

    if (js_is_object(obj_val))
        return;

    js_callthrow("TypeError_expected_object");
}

// ------------------------------------------------------------
//
// js_throw_if_nullobj
//
// ------------------------------------------------------------

static void js_throw_if_nullobj (js_environ *env, js_val obj_val) {

    if (js_is_undefined_or_null(obj_val))
        js_callthrow("TypeError_convert_null_to_object");
}

// ------------------------------------------------------------
//
// js_throw_if_not_extensible
//
// strict mode does not permit properties to be set on
// non-extensible objects, throw a TypeError in such a case
//
// ------------------------------------------------------------

static bool js_throw_if_not_extensible (
                js_environ *env, js_val obj, js_val prop,
                bool always_throw_or_only_if_strict_mode) {

    // error in strict mode if obj is a primitive value
    if (js_is_object(obj)) {

        js_obj *obj_ptr = js_get_pointer(obj);
        if (obj_ptr->max_values & js_obj_not_extensible) {

            if (always_throw_or_only_if_strict_mode) {

                js_callshadow(env,
                        "TypeError_object_not_extensible",
                        prop);

            } else {

                js_throw_if_strict(env,
                        "TypeError_object_not_extensible",
                        prop);
            }

            return true;    // if not strict mode
        }
    }

    return false;
}

// ------------------------------------------------------------
//
// js_throw_if_strict
//
// ------------------------------------------------------------

static void js_throw_if_strict (
        js_environ *env, const char *func_name, js_val arg) {

    // check if the function object at the most recent
    // stack frame is a strict function, and if that is
    // the case, call a shadow function to throw an error.

    js_link *stk_ptr = js_walkstack(js_stk_top);

    const js_func *func =
            (js_func *)js_get_pointer(stk_ptr->value);
    if (func->flags & js_strict_mode) {

        js_callshadow(env, func_name, arg);
    }
}
