
// ------------------------------------------------------------
//
// forward declarations
//
// ------------------------------------------------------------

static js_val js_stack_trace (js_c_func_args);

// ------------------------------------------------------------
//
// js_stack_init
//
// initialize this func module.  this is called by js_init ()
// as part of the initial initialization sequence, so keep in
// mind that not all functionality is ready for use.
//
// ------------------------------------------------------------

static void js_stack_init (js_environ *env) {

    //
    // initialize the call stack, which is a double-linked list
    // of 'stack link' elements.  initially we just set up the
    // head link, and a tail link.  see also js_growstack ()
    //

    js_link *head = js_check_alloc(malloc(sizeof(js_link) * 2));
    head->value.raw = 0;
    head->prev = NULL;
    head->next = head + 1;
    head->depth = 0;
    // tail entry, should never actually be used
    head->next->value.raw = 0;
    head->next->prev = head;
    head->next->next = NULL;
    head->next->depth = 1;
    // stack size is 1 because we never count the tail
    env->stack_size = 1;
    env->stack_top = head;

    // expose the 'js_stack_trace' utility function
    js_newprop(env, env->shadow_obj,
        js_str_c(env, "js_stack_trace")) =
            js_unnamed_func(js_stack_trace, 0);
}

// ------------------------------------------------------------
//
// js_growstack
//
// ------------------------------------------------------------

void js_growstack (js_environ *env, js_link *stk, int needed) {

    if (stk != env->stack_top) {
        fprintf(stderr, "Stack error!\n");
        exit(1);
    }

    int extra_links = needed - (env->stack_size - stk->depth);
    if (extra_links > 0) {
        extra_links += 7;
        env->stack_size += extra_links;

        // advance the 'depth' on all existing 'next' links
        js_link *stk_next = stk->next;
        js_link *old_link = stk_next;
        do {
            old_link->depth += extra_links;
            old_link = old_link->next;
        } while (old_link);
        old_link = stk->next;

        // allocate all the additional links at once
        js_link *new_link =
                js_malloc(extra_links * sizeof(js_link));

        // attach the new links into the stack
        old_link = stk;
        while (extra_links --> 0) {
            new_link->depth = old_link->depth + 1;
            new_link->prev = old_link;
            old_link->next = new_link;
            old_link = new_link++;
        }
        // link the last one of the new elements
        old_link->next = stk_next;
    }
}

// ------------------------------------------------------------
//
// js_arguments
//
// ------------------------------------------------------------

js_val js_arguments (js_environ *env,
                     js_val func_val, js_link *stk_args) {

    js_link *save_stk_top = js_stk_top;

    // collect all the parameters passed to the function,
    // between 'stk_args' and the stack top pointer,
    // into a temporary array
    js_val args_obj = js_restarr(env, stk_args);

    // if function is in non-strict mode, then 'func_val'
    // was passed, and we have to update its properties
    if (unlikely(!js_is_undefined_or_null(func_val)))
        js_arguments2(env, func_val, args_obj, stk_args);

    // we continue processing in the function js_arguments
    // in the javascript portion of the runtime, reachable
    // through the shadow object
    func_val = env->func_arguments;
    if (unlikely(func_val.raw == 0)) {
        int64_t dummy_shape_cache;
        func_val = js_getprop(env, env->shadow_obj,
                              js_str_c(env, "js_arguments"),
                              &dummy_shape_cache);
        js_throw_if_notfunc(env, func_val);
        env->func_arguments = func_val;
    }

    args_obj = js_callfunc1(env, func_val, js_undefined, args_obj);
    js_stk_top = save_stk_top;
    return args_obj;
}

// ------------------------------------------------------------
//
// js_arguments2
//
// arguments utility for non-strict mode.  if args_val is
//
// ------------------------------------------------------------

void js_arguments2 (js_environ *env, js_val func_val,
                    js_val args_val, js_link *stk_args) {

    js_val *val_ptr;

    //
    // get or create the 'arguments' property
    //
    js_descriptor *args_descr;
    val_ptr = js_ownprop(env, func_val,
                         env->str_arguments, true);
    if (js_is_descriptor(*val_ptr))
        args_descr = js_get_pointer(*val_ptr);
    else {
        // non-writable, non-enumerable, non-configurable
        args_descr = js_newdescr(
            js_descr_value, js_null, js_make_number(0.0));
        *val_ptr = js_make_descriptor(args_descr);
    }

    //
    // get or create the 'caller' property
    //
    js_descriptor *func_descr;
    val_ptr = js_ownprop(env, func_val,
                         env->str_caller, true);
    if (js_is_descriptor(*val_ptr))
        func_descr = js_get_pointer(*val_ptr);
    else {
        // non-writable, non-enumerable, non-configurable
        func_descr = js_newdescr(
            js_descr_value, js_null, js_make_number(0.0));
        *val_ptr = js_make_descriptor(func_descr);
    }

    if (js_is_undefined_or_null(args_val)) {
        // if args_val is null, then we were called by
        // return_statement () in statement_writer.js
        args_val = func_val = js_null;

    } else {

        // otherwise we are called from js_arguments ()
        // above, upon entry to a non-strict function.
        //
        // first, we point the 'callee' property in the
        // the newly-created 'arguments' object to the
        // function that was just entered
        *js_ownprop(env, args_val,
                    env->str_callee, true) = func_val;

        // next, we want to set the 'caller' property
        // on the function that was just entered, to the
        // function that called it.  we scan the stack
        // backwards, looking for a flagged pointer
        for (;;) {
            if (!(stk_args = stk_args->prev))
                return;
            func_val = stk_args->value;
            if (js_is_flagged_pointer(func_val)) {
                func_val = js_make_object(
                            js_get_pointer(func_val));
                break;
            }
        }
    }

    args_descr->data_or_getter = args_val;
    func_descr->data_or_getter = func_val;
}

// ------------------------------------------------------------
//
// js_spreadargs
//
// ------------------------------------------------------------

void js_spreadargs (js_environ *env, js_val iterable) {

    //
    // fast-path if iterable is a typical fast-path array.
    //

    if (js_is_object(iterable)) {

        js_obj *obj_ptr = (js_obj *)js_get_pointer(iterable);
        uintptr_t proto = (uintptr_t)obj_ptr->proto;
        uint32_t length;
        if (likely(proto == (uintptr_t)env->fast_arr_proto
               && (-1U != (length =
                            ((js_arr *)obj_ptr)->length)))) {
            //
            // push each array element from 0 to 'length'
            //
            js_val *values = ((js_arr *)obj_ptr)->values;
            js_growstack(env, js_stk_top, length);
            while (length != 0) {
                length--;
                js_stk_top->value = *values++;
                js_stk_top = js_stk_top->next;
            }

            return;
        }
    }

    //
    // otherwise take the slow-path via Symbol.iterator
    //

    int64_t dummy_shape_cache;
    js_val iterator = js_getprop(env, iterable,
                                 env->sym_iterator,
                                &dummy_shape_cache);

    if (js_is_object(iterator) &&
            js_obj_is_exotic(js_get_pointer(iterator),
                             js_obj_is_function)) {

        iterator = js_callfunc1(env, iterator,
                                iterable, js_undefined);
        if (js_is_object(iterator)) {

            js_val next = js_getprop(env, iterator,
                                     env->str_next,
                                    &dummy_shape_cache);

            if (js_is_object(next) &&
                js_obj_is_exotic(js_get_pointer(next),
                                    js_obj_is_function)) {

                //
                // an iterator function returned an iterable
                // object with a next () function, so keep
                // calling this function, and pushing its
                // result 'value' into the stack, until it
                // returns a result with 'done' set to true
                //

                for (;;) {

                    js_val result = js_callfunc1(
                        env, next, iterator, js_undefined);

                    if (!js_is_object(result))
                        js_callthrow("TypeError_iterator_result");

                    if (js_is_truthy(js_getprop(
                                    env, result, env->str_done,
                                   &dummy_shape_cache)))
                        return;

                    js_stk_top->value = js_getprop(
                                    env, result, env->str_value,
                                   &dummy_shape_cache);
                    js_stk_top = js_stk_top->next;
                }
            }
        }
    }

    //
    //
    //

    js_callthrow("TypeError_not_iterable");
}

// ------------------------------------------------------------
//
// js_newiter
//
// prepares an iterator for consumption by a loop, e.g.
// for_statement () in statement_writer.js, in the case
// of a for-of / for-in loop.  parameter 'new_iter' should
// points to an array of three values:
// new_iter[0] is the next () function for this iterator;
// new_iter[1] is a reference to the iterator object;
// new_iter[2] receives the next result value, if not done.
//
// ------------------------------------------------------------

void js_newiter (js_environ *env, js_val *new_iter,
                 int kind, js_val iterable_val) {

    int64_t dummy_shape_cache;

    // if we fail, make sure we return a non-callable result
    new_iter[0].raw = 0;

    if (kind == 'I') {

        //
        // for-in iterator
        //
        // locate the _shadow.for_in_iterator function,
        // provided by runtime2.js
        if (!env->for_in_iterator.raw) {
            env->for_in_iterator = js_getprop(
                            env, env->shadow_obj,
                            js_str_c(env, "for_in_iterator"),
                            &dummy_shape_cache);
        }

        // the for_in_iterator () function creates an iterator
        // which enumerates the keys of the specifieid object
        new_iter[1] = js_callfunc1(env, env->for_in_iterator,
                                   js_undefined, iterable_val);

    } else {

        //
        // for-of iterator
        //
        // invoke [Symbol.iterator] on the specified object

        js_val method = js_getprop(env, iterable_val,
                                   env->sym_iterator,
                                  &dummy_shape_cache);

        if (js_is_object(method)
        &&  js_obj_is_exotic(js_get_pointer(method),
                             js_obj_is_function)) {

            new_iter[1] = js_callfunc1(
                            env, method, iterable_val,
                            js_undefined);

        } else {
            // make sure the check below fails,
            // and we end up throwing an error
            new_iter[1].raw = 0;
        }
    }

    //
    // the iterator object at new_iter[1] must be an
    // object, and must provide a next () function
    //

    if (js_is_object(new_iter[1])) {

        js_val method = js_getprop(env, new_iter[1],
                                   env->str_next,
                                  &dummy_shape_cache);

        if (js_is_object(method)
        &&  js_obj_is_exotic(js_get_pointer(method),
                             js_obj_is_function)) {

            new_iter[0] = method;
            js_nextiter(env, new_iter);
            return;
        }
    }

    // fail because the specified value is not iterable

    js_callthrow("TypeError_not_iterable");
}

// ------------------------------------------------------------
//
// js_nextiter
//
// steps an iterator prepared by js_newiter (), see above.
// the caller should check that iter[0] is non-zero,
// before assuming that iter[2] holds the next value.
//
// ------------------------------------------------------------

void js_nextiter (js_environ *env, js_val *iter) {

    js_val result = js_callfunc1(
                        env, /* next () func */ iter[0],
                        /* this */ iter[1], js_undefined);

    if (!js_is_object(result))
        js_callthrow("TypeError_iterator_result");

    int64_t dummy_shape_cache;

    if (js_is_truthy(js_getprop(env, result, env->str_done,
                               &dummy_shape_cache))) {
        // terminate iteration
        iter[0].raw = 0;

    } else {
        // return next result
        iter[2] = js_getprop(env, result, env->str_value,
                            &dummy_shape_cache);
    }
}

// ------------------------------------------------------------
//
// js_walkstack
//
// walk the stack chain backwards, starting at the pointer
// passed via parameter, and stopping at the next frame,
// which is the parent or calling stack frame.  on initial
// call, pass js_stk_top.  on subsequent calls, pass the
// return value from the last call.
//
// ------------------------------------------------------------

static js_link *js_walkstack (js_link *stk_ptr) {

    // we start by going to the previous link, because
    // stk_ptr is either (1) js_stk_top, which is always
    // one stack link past the last pushed stack value;
    // or (2) return value from a previous call to this
    // function, and we want to advance the stack.

    for (;;) {

        if (!(stk_ptr = stk_ptr->prev)) {
            fprintf(stderr, "Stack error!");
            exit(1);
        }

        // the function value that marks the beginning
        // of a new frame has bit 1 set (see also
        // write_function () in function_writer.js),
        // and  has bit 0 set as a pointer object.
        // which means we can use the descriptor test,
        // even though it is not a property descriptor.
        if (js_is_flagged_pointer(stk_ptr->value)) {

            const void *any_obj =
                    js_get_pointer(stk_ptr->value);
            if (any_obj && js_obj_is_exotic(
                    any_obj, js_obj_is_function)) {

                return stk_ptr;
            }
        }
    }
}

// ------------------------------------------------------------
//
// js_stack_trace
//
// ------------------------------------------------------------

static js_val js_stack_trace (js_c_func_args) {

    stk_args->value.raw = func_val.raw | 2; // stack frame

    /* DUMP CONTENTS OF STACK
    js_link *s0 = stk_args;
    for (;;) {
        s0 = s0->prev;
        if (!s0)
            break;
        printf("[%p] ", (void *)s0);
        const js_val s0v = s0->value;
        if (js_is_object(s0v)) {
            js_obj *ptr = (js_obj *)js_get_pointer(s0v);
            if (js_obj_is_exotic(ptr, js_obj_is_function)) {
                js_val name = ptr->values[env->func_name];
                if (js_is_primitive_string(name))
                    js_print(env, name);
                //js_print(env, ptr->values[env->func_name]);
                printf("func@ %s ", ((js_func *)ptr)->where);
                if (s0v.raw & 2)
                    printf("(flagged) ");
            }
        }
        js_print(env,s0v);printf("\n");
    }*/

    js_val arr = js_newarr(env, 0);
    uint32_t idx = 0;
    for (js_link *stk_ptr = stk_args;;) {
        stk_ptr = js_walkstack(stk_ptr);
        const js_func *func =
                (js_func *)js_get_pointer(stk_ptr->value);
        // store function object reference without the '|2'
        js_arr_set(env, arr, ++idx, js_make_object(func));
        // store source location
        js_val where;
        if (func->where) {
            const char *str = func->where;
            where = js_str_c2(env, str, strlen(str));
        } else
            where = env->str_empty;
        js_arr_set(env, arr, ++idx, where);
        // check if we reached the end of the stack chain
        if (!stk_ptr->prev)
            break;
    }
    js_return(arr);
}

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
    free(try);
    return throw_val;
}

// ------------------------------------------------------------
//
// js_throw
//
// ------------------------------------------------------------

js_val js_throw (js_environ *env, js_val throw_val) {

    js_try *try = env->try_handler;
    try->throw_val = throw_val;

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

        if (stk_ptr == try->stack_top) {
            js_stk_top = stk_ptr;
            break;
        }
    }

    // jump to the execution point where the 'try' was
    // set up via setjmp ().  see also wmain () or
    // try_statement () in statement_writer.js
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

static void js_throw_if_not_extensible (js_environ *env, js_val obj) {

    // error in strict mode if obj is a primitive value
    if (js_is_object(obj)) {

        js_obj *obj_ptr = js_get_pointer(obj);
        if (obj_ptr->max_values & js_obj_not_extensible) {

            js_throw_if_strict_0(
                "TypeError_object_not_extensible");
        }
    }
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
