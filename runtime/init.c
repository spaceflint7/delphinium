
// ------------------------------------------------------------
//
// js_init
//
// ------------------------------------------------------------

static js_environ *js_init (int64_t version_code) {

    if (version_code != js_version_code
        // check alignments for some fields in some structures
    ||  offsetof(js_environ,last_public_field) + sizeof(int)
                    != offsetof(js_environ,first_private_field)
    ||  offsetof(js_arr,values) != 32) {

        fprintf(stderr, "Version mismatch!\n");
        exit(1);
    }

    js_environ *env = (js_environ *)
        /* alloc and clear */ js_calloc(1, sizeof(js_environ));

    env->func_abort_non_strict = true;

    // initialize components
    js_str_init(env);
    js_shape_init(env);
    js_obj_init(env);
    js_descr_init(env);
    js_func_init(env);
    js_stack_init(env);
    js_str_init_2(env);
    js_obj_init_2(env);
    js_num_init(env);
    js_big_init(env);
    js_arr_init(env);

    return env;
}

// ------------------------------------------------------------
//
// js_init3
//
// additional initialization that should run after the
// javascript runtime module has completed its initialized.
//
// ------------------------------------------------------------

static void js_init3 (js_environ *env) {

    int64_t dummy_shape_cache;

    //
    // collect some well-known symbols, defined by symbol.js
    //

    js_val symbol = js_getprop(env, env->shadow_obj,
            js_str_c(env, "Symbol"), &dummy_shape_cache);

    env->sym_iterator = js_getprop(env, symbol,
            js_str_c(env, "iterator"), &dummy_shape_cache);

    env->sym_hasInstance = js_getprop(env, symbol,
            js_str_c(env, "hasInstance"), &dummy_shape_cache);

    env->sym_toPrimitive = js_getprop(env, symbol,
            js_str_c(env, "toPrimitive"), &dummy_shape_cache);

    env->sym_unscopables = js_getprop(env, symbol,
            js_str_c(env, "unscopables"), &dummy_shape_cache);

    //
    // disable detection of non-strict functions
    //

    env->func_abort_non_strict = false;
}

// ------------------------------------------------------------
//
// declare external functions
//
// ------------------------------------------------------------

// components of this runtime written in javascript,
// its 'js_main' function renamed to 'js_init2'
extern js_val js_init2(js_c_func_args);

// compiled c form of the javascript program to run
extern js_val js_main(js_c_func_args);

// ------------------------------------------------------------
//
// call_module_entry_point
//
// ------------------------------------------------------------

static js_val call_module_entry_point (
                js_environ *env, js_c_func module_c_func) {

    // our calling convention dictates the caller sets
    // env->stack_top just past arguments it pushed, including
    // the first stack link, which is left for the callee to
    // insert its own function object

    js_link *stk_base = env->stack_top;
    env->stack_top = stk_base->next;

    // this runtime library code does not know which values to
    // pass in parameters 'arity' and 'closures' to js_newfunc
    // for the main function of the module we need to invoke.
    // the workaround is to call a middle-man function which
    // initializes the module, and creates the function object
    // that we can call.  see also write_module_entry_point ()
    // in function_writer.js.

    js_val module_main_func = js_callfunc(env,
            js_newfunc(env, module_c_func, js_undefined, NULL,
                       js_strict_mode | 0, 0),
            js_undefined, stk_base);

    // second call to the actual main function.  our calling
    // convention dictates the callee restores env->stack_top
    // to its own 'stk_arg' parameter (which was passed just
    // above as 'stk_base).  so set up the stack again, then
    // invoke the actual main function.

    env->stack_top = stk_base->next;

    return js_callfunc(
            env, module_main_func, js_undefined, stk_base);
}

// ------------------------------------------------------------
//
// report_unhandled_exception
//
// ------------------------------------------------------------

static void report_unhandled_exception (
                        js_environ *env, js_val exception) {

    const wchar_t *txt_ptr = L"?";
               int txt_len = 1;

    if (setjmp(*js_entertry(env)) == 0) {

        // call get_exception_as_string () from error.js
        // on the thrown object, to convert it to a string.
        // note that this is also protected by try/catch
        // logic, in case another exception occurs.

        exception = js_callshadow(
                env, "get_exception_as_string", exception);

        if (js_is_primitive_string(exception)) {

            const objset_id *id = js_get_pointer(exception);
            txt_ptr = id->data;
            txt_len = id->len >> 1;
        }
    }

    fprintf(stderr, "\nUnhandled exception: "
                    "%*.*ls\n", txt_len, txt_len, txt_ptr);
}

// ------------------------------------------------------------
//
// wmain
//
// ------------------------------------------------------------

int __stdcall wmain (int argc, wchar_t **argv) {

    // initialize
    js_environ *env = js_init(js_version_code);

    js_growstack(env, js_stk_top, 4);

    volatile js_val exit_code = js_undefined;

    if (setjmp(*js_entertry(env)) == 0) {

        // initialize runtime components written in javscript,
        // in the runtime2.js module
        call_module_entry_point(env, js_init2);

        js_init3(env);

        // call the main program module
        exit_code = call_module_entry_point(env, js_main);
    }

    js_val exception = js_leavetry(env);
    if (exception.raw != js_uninitialized.raw) {

        report_unhandled_exception(env, exception);
    }

    if (!js_is_number(exit_code))
        exit_code = js_make_number(0.0);

    printf("\n");
    return (int)exit_code.num;
}
