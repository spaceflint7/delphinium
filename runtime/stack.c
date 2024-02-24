
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
    head->depth = js_link_allocated;
    // tail entry, should never actually be used
    head->next->value.raw = 0;
    head->next->prev = head;
    head->next->next = NULL;
    head->next->depth = 1;
    // stack size is 1 because we never count the tail
    env->stack_size = 1;
    env->stack_top = head;
}

// ------------------------------------------------------------
//
// js_stack_init_2
//
// ------------------------------------------------------------

static void js_stack_init_2 (js_environ *env) {

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

    int extra_links = needed -
            (env->stack_size - js_link_depth(stk));
    if (extra_links > 0) {
        extra_links += 7;
        env->stack_size += extra_links;

        // advance the 'depth' on all existing 'next' links
        js_link *stk_next = stk->next;
        js_link *old_link = stk_next;
        while (old_link) {
            old_link->depth += extra_links;
            old_link = old_link->next;
        }
        old_link = stk->next;

        // allocate all the additional links at once
        js_link *new_link =
            js_malloc(extra_links * sizeof(js_link));

        // attach the new links into the stack
        old_link = stk;
        while (extra_links --> 0) {
            new_link->depth =
                        js_link_depth(old_link) + 1;
            new_link->prev = old_link;
            old_link->next = new_link;
            old_link = new_link++;
        }
        // mark the first new link as allocated
        stk->next->depth |= js_link_allocated;
        // link the last one of the new elements
        old_link->next = stk_next;
    }
}

// ------------------------------------------------------------
//
// js_copystack
//
// create a copy of the stack entries starting with the
// 'first' element, and until, but excluding, the 'last'
// elements.  on return, the parameters are updated to
// point to the newly-created first and last elements.
//
// ------------------------------------------------------------

int js_copystack (js_link **p_first, js_link **p_last) {

    // count how many links to copy
    js_link *end_ptr = *p_last;
    js_link *stk_ptr = *p_first;
    int num_links = 1; // count the tail element
    while (stk_ptr != end_ptr) {
        num_links++;
        stk_ptr = stk_ptr->next;
    }

    // allocate all the additional links at once
    js_link *new_link =
            js_malloc(num_links * sizeof(js_link));
    new_link->prev = NULL;
    new_link->depth = js_link_allocated;
    int next_depth = 1;
    stk_ptr = *p_first;
    *p_first = new_link;

    // copy links into the newly allocated space
    while (stk_ptr != end_ptr) {
        js_link *next_link = new_link + 1;
        next_link->prev = new_link;
        new_link->value = stk_ptr->value;
        new_link->next = next_link;
        new_link = next_link;
        new_link->depth = next_depth++;
        stk_ptr = stk_ptr->next;
    }

    // create the tail element
    new_link->value.raw = 0;
    new_link->next = NULL;
    *p_last = new_link;
    return num_links;
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
    js_val args_obj = js_restarr_stk(env, stk_args->next);

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
// arguments utility for non-strict mode.
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

        js_gc_notify(env, func_val);

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

        js_gc_notify(env, func_val);
        js_gc_notify(env, args_val);
    }

    args_descr->data_or_getter = args_val;
    func_descr->data_or_getter = func_val;
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
            fprintf(stderr, "Stack error!\n");
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
    js_prolog_stack_frame();

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
