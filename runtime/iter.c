
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
            js_nextiter(env, new_iter, js_undefined);
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

void js_nextiter (js_environ *env, js_val *iter, js_val arg) {

    js_val result = js_callfunc1(
                        env, /* next () func */ iter[0],
                        /* this */ iter[1], arg);

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
