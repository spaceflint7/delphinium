
// ------------------------------------------------------------
//
// js_spreadargs
//
// ------------------------------------------------------------

void js_spreadargs (js_environ *env, js_val iterable) {

    //
    // fast-path if iterable is a typical fast-path array.
    //

    /*if (js_is_object(iterable)) {

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
    }*/

    //
    // iterate the object via Symbol.iterator.  note that
    // this is true for arrays, because the the iterator
    // might be overridden on the array object itself, or
    // on Array.prototype or even Object.prototype.
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
            js_nextiter1(env, new_iter);
            return;
        }
    }

    // fail because the specified value is not iterable

    js_callthrow("TypeError_not_iterable");
}

// ------------------------------------------------------------
//
// js_checkiter
//
// ------------------------------------------------------------

static void js_checkiter (js_environ *env, js_val *iter,
                          js_val result) {

    if (!js_is_object(result))
        js_callthrow("TypeError_iterator_result");

    js_obj *obj_ptr = (js_obj *)js_get_pointer(result);
    js_val value, done;

    if (obj_ptr->shape == env->shape_value_done) {

        value = obj_ptr->values[0];
        done  = obj_ptr->values[1];

    } else if (obj_ptr->shape == env->shape_done_value) {

        done  = obj_ptr->values[0];
        value = obj_ptr->values[1];

    } else {

        int64_t dummy_shape_cache;
        value = js_getprop(env, result, env->str_value,
                            &dummy_shape_cache);
        done  = js_getprop(env, result, env->str_done,
                            &dummy_shape_cache);
    }

    iter[2] = value;
    if (js_is_truthy(done))
        iter[0].raw = 0; // terminate iteration
}

// ------------------------------------------------------------
//
// js_nextiter1
//
// steps an iterator prepared by js_newiter (), see above.
// the caller should check that iter[0] is non-zero,
// before assuming that iter[2] holds the next value.
//
// ------------------------------------------------------------

void js_nextiter1 (js_environ *env, js_val *iter) {

    js_val result = js_callfunc1(
                        env, /* next () func */ iter[0],
                        /* this */ iter[1], js_undefined);

    js_checkiter(env, iter, result);
}

// ------------------------------------------------------------
//
// js_throw_if_notfunc_iter
//
// ------------------------------------------------------------

static void js_throw_if_notfunc_iter (
                js_environ *env, js_val func, js_val name) {

    if (js_is_object(func) &&
            js_obj_is_exotic(js_get_pointer(func),
                             js_obj_is_function))
        return;

    js_callshadow(
            env, "TypeError_iterator_cannot_call", name);
}

// ------------------------------------------------------------
//
// js_nextiter2
//
// steps an iterator with support for return () and
// throw (), as required by coroutine yield* mechanism.
//
// for cmd = 0, calls 'next (arg)' on the iterator, like
// js_nextiter1 (), except this passes an extra argument.
//
// for cmd > 1, calls 'return (arg)' on the iterator,
// and returns its result; returns false if a function
// named return () is not found on the iterator.
//
// for cmd < 1, calls 'throw (arg)' on the iterator, and
// returns its result; if no such function, then it may
// call 'return (arg) on the iterator, if such a function
// exists, and then throw a TypeError exception.
//
// ------------------------------------------------------------

bool js_nextiter2 (js_environ *env, js_val *iter,
                   int cmd, js_val arg) {

    int64_t dummy_shape_cache;
    js_val func;

    if (cmd < 0) {
        //
        // request to call throw () on the iterator
        //
        func = js_getprop(env, iter[1],
                                env->str_throw,
                                &dummy_shape_cache);
        if (js_is_undefined_or_null(func)) {
            // we don't have a throw function (), so
            // check if we have a return () function
            js_val func2 = js_getprop(env, iter[1],
                                env->str_return,
                                &dummy_shape_cache);
            if (!js_is_undefined_or_null(func2)) {
                js_throw_if_notfunc_iter(
                        env, func2, env->str_return);
                // call return () without an argument
                js_callfunc1(env, func2,
                  /* this */ iter[1], js_undefined);
            }
        }
        // throw a TypeError if throw () is missing,
        // otherwise the logic below calls throw ()
        js_throw_if_notfunc_iter(
                        env, func, env->str_throw);

    } else if (cmd > 0) {
        //
        // request to call return () on the iterator
        //
        func = js_getprop(env, iter[1],
                                env->str_return,
                                &dummy_shape_cache);
        if (js_is_undefined_or_null(func)) {
            // tell caller that return () is missing
            return false;
        }
        js_throw_if_notfunc_iter(
                        env, func, env->str_return);

    } else {
        //
        // request to call next () on the iterator
        //
        func = iter[0];
    }

    js_val result = js_callfunc1(env, func,
                      /* this */ iter[1], arg);

    js_checkiter(env, iter, result);
    return true;
}
