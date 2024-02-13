
// ------------------------------------------------------------
//
// garbage collector
//
// mark-and-sweep in a concurrent background thread,
// with minimal stop-the-world for stack walking.
//
// https://github.com/munificent/mark-sweep
// https://stackoverflow.com/questions/2364274
//
// ------------------------------------------------------------

#define js_gc_build

typedef struct js_gc_val {

    struct js_gc_val *next;
    js_val val;

} js_gc_val;

// garbage collector state
struct js_gc_env {

    // unused js_gc_val elements
    js_gc_val *free_pool;

    // list of values that were referenced
    js_gc_val *ref_values;
    js_gc_val *ref_values_2; // while sweeping

    // list of values to track by the gc
    js_gc_val *all_values;
    js_gc_val *all_values_2; // while sweeping

    // list of malloc-allocated blocks to free ()
    js_gc_val *free_void_ptrs;

    int num_new_values;
    int num_all_values;

    bool wakeup;
    bool sleeping;
    bool run_sweep;
    bool walked_stack;

    // number of values created before sweep
    int threshold;

    // pointer to main js environment
    js_environ *env;

    // reference to a number value, the property
    // _shadow.js_gc_util.count
    js_val *p_num_all_values;

    // inter-thread synchronization objects
    struct js_thread *thread;
    struct js_event  *event;
    struct js_mutex  *mutex;
};

// ------------------------------------------------------------

static void js_gc_mark_val (js_gc_env *gc, js_val val);

static void js_gc_collect (js_gc_env *gc, bool full);

// ------------------------------------------------------------
//
// js_gc_compare_and_swap
//
// checks and updates the gc-flags word for the
// value specified in 'val'.  the bits checked
// are specified in 'test_mask'.
//
// if 'set_or_clear' is true, checks if any bits
// in 'test_mask' are already set in the value,
// and if so, returns false.
//
// it then does a compare-and-swap to set or
// clear the bits specified in 'updt_mask'.
//
// finally, it returns true only if none of the
// bits in 'test_mask' are set in the result from
// CAS, which is the initial value of the flags.
//
// note that this function also returns false
// if the value is a string (or a symbol) and
// its flags indicate it is static or interned.
//
// ------------------------------------------------------------

static bool js_gc_compare_and_swap (
        js_val val, uint32_t test_mask,
        bool set_or_clear, uint32_t updt_mask) {

    // manipulates gc bits on a value.
    // if object, accesses js_obj->max_value.
    // if bigint, accesses its first word (length).
    // if string/symbol, accesses the flags field
    // of the objset_id struct (a 16-bit field)

    void *ptr = js_get_pointer(val);

    if (js_is_object(val)) {

        ptr = &((js_obj *)ptr)->max_values;

    } else {

        int prim_type = js_get_primitive_type(val);
        if (prim_type != js_prim_is_bigint) {

            // string/symbol point to an objset_id
            // struct with a 16-bit flags field
            objset_id *id = (objset_id *)ptr;
            ptr = ((char *)&(id)->flags);

            uint32_t word = *(uint16_t *)ptr;
            if (word & (js_str_in_objset
                      | js_str_is_static)) {
                // special case to always skip
                // interned or static strings
                return false;
            }

            uint16_t and_mask, or_bits;
            if (set_or_clear) {
                // we are about to turn bits on,
                // can quick-abort if already on
                word = (word << 16) & test_mask;
                if (word != 0)
                    return false;
                and_mask = (uint16_t)-1;
                or_bits = updt_mask >> 16;
            } else {
                and_mask = ~(updt_mask >> 16);
                or_bits = 0;
            }

            word = js_compare_and_swap_16(
                        ptr, and_mask, or_bits);

            word = (word << 16) & test_mask;
            return (word == 0);
        }

        // fallthrough for bigint, where ptr
        // already points at the length word
    }

    uint32_t word, and_mask, or_bits;
    if (set_or_clear) {
        // we are about to turn bits on,
        // can quick-abort if already on
        word = *(uint32_t *)ptr & test_mask;
        if (word != 0)
            return false;
        and_mask = -1U;
        or_bits = updt_mask;
    } else {
        and_mask = ~updt_mask;
        or_bits = 0;
    }

    word = js_compare_and_swap_32(
                    ptr, and_mask, or_bits);
    return ((word & test_mask) == 0);
}

// ------------------------------------------------------------
//
// js_gc_push_val
//
// ------------------------------------------------------------

static void js_gc_push_val (
                    js_gc_env *gc, js_val val, int which) {

    js_mutex_enter(gc->mutex);

    js_gc_val *elem = gc->free_pool;
    if (elem)
        gc->free_pool = elem->next;
    else {
        js_mutex_leave(gc->mutex);
        elem = js_malloc(sizeof(js_gc_val));
        js_mutex_enter(gc->mutex);
    }

    // select the list for the new element

    js_gc_val **push_list =
             which == 'F' ? &gc->free_void_ptrs
        :   (which == 'R' ? (gc->run_sweep
                          ? &gc->ref_values_2
                          : &gc->ref_values)
        :   (which == 'A' ? (gc->run_sweep
                          ? &gc->all_values_2
                          : &gc->all_values)
        :   NULL));

    elem->next = *push_list;
    elem->val = val;
    *push_list = elem;

    if (which != 'A') {

        gc->wakeup = true;
        bool sleeping = gc->sleeping;
        js_mutex_leave(gc->mutex);
        if (sleeping)
            js_event_post(gc->event);
    }
}

// ------------------------------------------------------------
//
// js_gc_walkstack1
//
// ------------------------------------------------------------

void js_gc_walkstack1 (js_environ *env,
                       js_link *stk_ptr,
                       js_try *try_handler,
                       js_val new_target) {

    if (try_handler) {
        const js_val val = try_handler->throw_val;
        if (js_is_object_or_primitive(val))
            js_gc_notify(env, val);
    }

    if (js_is_object_or_primitive(new_target))
        js_gc_notify(env, new_target);

    for (;;) {
        stk_ptr = stk_ptr->prev;
        if (!stk_ptr)
            break;
        const js_val val = stk_ptr->value;
        if (js_is_object_or_primitive(val))
            js_gc_notify(env, val);
    }
}

// ------------------------------------------------------------
//
// js_gc_walkstack
//
// ------------------------------------------------------------

static void js_gc_walkstack (js_environ *env) {

    js_gc_walkstack1(env, env->stack_top,
                     env->try_handler, env->new_target);

    /* extern */ void js_gc_walkstack2 (js_environ *env);
    js_gc_walkstack2(env);
}

// ------------------------------------------------------------
//
// js_gc_free
//
// ------------------------------------------------------------

static void js_gc_free (js_environ *env, void *ptr) {

#ifdef js_gc_build
    // this function is called to free memory which
    // is no longer needed, such as a previous array
    // of js_obj->values pointers, when the object
    // shape changes and a larger array is allocated.
    // free () cannot be used directly, because the
    // gc thread may be looking at the same array at
    // the same time.  this 'free void* ptrs' helper
    // lets the gc thread free the memory when safe.

    js_gc_env *gc = env->gc;
    js_val val = (js_val){ .raw = (uint64_t)ptr };
    js_gc_push_val(gc, val, 'F');

#else // !js_gc_build
    js_free(ptr);
#endif
}

// ------------------------------------------------------------
//
// js_gc_free_void_ptrs
//
// ------------------------------------------------------------

static void js_gc_free_void_ptrs (js_gc_env *gc) {

    // this function runs in the context of
    // the gc thread and continues processing
    // initiated by the js_gc_free () above

    for (;;) {
        js_mutex_enter(gc->mutex);

        void *ptr_to_free;
        js_gc_val *elem = gc->free_void_ptrs;

        if (elem) {
            // pop element from 'free_void_ptrs'
            ptr_to_free = (void *)elem->val.raw;
            gc->free_void_ptrs = elem->next;
            // push element into 'free_pool'
            elem->next = gc->free_pool;
            gc->free_pool = elem;
        }

        js_mutex_leave(gc->mutex);
        if (!elem)
            break;
        js_free(ptr_to_free);
    }
}

// ------------------------------------------------------------
//
// js_gc_manage
//
// ------------------------------------------------------------

static js_val js_gc_manage (js_environ *env, js_val val) {

#ifdef js_gc_build
    js_gc_env *gc = env->gc;

    // push new value into 'all_values' list,
    // the gc mutex remains held upon return
    js_gc_push_val(gc, val, 'A');

    // if more than X new values were created since
    // the last sweep, initiate a new sweep sequence.

    ++gc->num_all_values;
    int num_new_values = ++gc->num_new_values;

    do {

        if (gc->run_sweep ||
                num_new_values < gc->threshold) {

            js_mutex_leave(gc->mutex);
            break;
        }

        bool gc_is_doing_work =  gc->ref_values
                             || !gc->sleeping;

        js_mutex_leave(gc->mutex);

        if (num_new_values < gc->threshold * 2) {

            // while more than X values were created,
            // but still fewer than 2*X, we give the
            // gc thread a chance to become idle,
            // before we initiate a new sweep.
            if (gc_is_doing_work)
                break;

            if (!gc->walked_stack) {
                gc->walked_stack = true;
                js_gc_walkstack(gc->env);
                break;
            }

            // fallthrough -- more than X values were
            // created, but less than 2*X;  we walked
            // stacks and notified values; and the gc
            // thread is idle at this point.
        }

        js_gc_collect(gc, false); // request sweep

    } while (false);
#endif

    return val;
}

// ------------------------------------------------------------
//
// js_gc_mark_seq
//
// ------------------------------------------------------------

static void js_gc_mark_seq (js_gc_env *gc,
                            js_val *vals, int num) {

    while (num-- > 0) {

        js_val val = *vals++;
        if (js_is_descriptor(val)) {

            js_descriptor *descr =
                (js_descriptor *)js_get_pointer(val);

            const int flags =
                js_descr_flags_without_setter(descr);

            if (likely(!(flags & (  js_descr_getter
                                  | js_descr_setter)))) {

                // value descriptor, no getter or setter
                val = descr->data_or_getter;

            } else if (unlikely(flags & js_descr_setter)) {

                // accessor descriptor with a setter
                const void *p;
                if (likely(flags & js_descr_getter)) {

                    // descriptor also has a getter
                    p = js_get_pointer(descr->data_or_getter);
                    if (p)
                        js_gc_mark_val(gc, js_make_object(p));
                }

                p = js_get_pointer(descr->setter_and_flags);
                val = js_make_object(p);

            } else {

                // getter-only accessor descriptor
                const void *p =
                    js_get_pointer(descr->data_or_getter);
                val = js_make_object(p);
            }
        }

        js_gc_mark_val(gc, val);
    }
}

// ------------------------------------------------------------
//
// js_gc_mark_obj
//
// ------------------------------------------------------------

static void js_gc_mark_obj (js_gc_env *gc, js_obj *obj) {

    uintptr_t proto = (uintptr_t)obj->proto;
    const int exotic_type = (uintptr_t)proto & 7;

    proto &= ~7;
    if (likely(proto != 0))
        js_gc_mark_val(gc, js_make_object(proto));

    const int num_values = obj->shape->num_values;
    js_gc_mark_seq(gc, obj->values, num_values);

    if (exotic_type == js_obj_is_array) {

        const js_arr *arr = (js_arr *)obj;
        // length_descr[0] holds the length as a double
        uint32_t num =
                (uint32_t)arr->length_descr[0].num;
        if (num > arr->capacity)
            num = arr->capacity;
        js_gc_mark_seq(gc, arr->values, num);

    } else if (exotic_type == js_obj_is_function) {

        const js_func *func = (js_func *)obj;

        uint32_t num = func->closure_count;
        js_val **vals = func->closure_array + num;
        while (num-- > 0) {
            js_gc_mark_val(gc, **vals);
            vals++;
        }

        js_val *temp = func->closure_temps;
        while (temp) {
            js_gc_mark_val(gc, *temp);
            temp = ((struct js_closure_var *)temp)->next;
        }

        if (func->flags & js_strict_mode) {
            js_link *with_scope = func->u.with_scope;
            if (with_scope)
                js_gc_mark_val(gc, with_scope->value);
        }
    }
}

// ------------------------------------------------------------
//
// js_gc_mark_val
//
// ------------------------------------------------------------

static void js_gc_mark_val (js_gc_env *gc, js_val val) {

    // value must be object/string/symbol/bigint
    if (js_is_object_or_primitive(val) &&

        // mark the value, if it has not already
        // been processed during this gc iteration
        js_gc_compare_and_swap(val, js_gc_marked_bit,
                              true, js_gc_marked_bit)) {

            if (js_is_object(val)) {

                // recursively process a value object
                js_gc_mark_obj(gc, js_get_pointer(val));
            }
    }
}

// ------------------------------------------------------------
//
// js_gc_notify
//
// ------------------------------------------------------------

static void js_gc_notify (js_environ *env, js_val val) {

#ifdef js_gc_build
    // called by js_setprop () to notify the gc
    // that a value was ref'ed, while being set
    // as an object property or array index).

    bool marked_and_notify_bits_are_clear =

        js_gc_compare_and_swap(val,
            js_gc_marked_bit | js_gc_notify_bit,
                         true, js_gc_notify_bit);

    if (!marked_and_notify_bits_are_clear)
        return;

    js_gc_env *gc = env->gc;
    js_gc_push_val(gc, val, 'R');
#endif
}

// ------------------------------------------------------------
//
// js_gc_notify2
//
// ------------------------------------------------------------

void js_gc_notify2 (js_environ *env, uint64_t addr) {

    // called by js_gc_walkstack2 () while scanning
    // the CPU stack, which might contain values that
    // happen to look like NaN-boxed pointers, but do
    // not match any of our values.  js_gc_notify2 ()
    // therefore does not consider the input a valid
    // value, and does not check or set the marked or
    // notify bits.  however it sets bit 63, see also
    // js_gc_ref_values () below, which searches the
    // list 'all_values' in order to verify the input.

    js_val val = (js_val){ .raw = addr };

    if (js_is_object(val)) {
        // an object value on the stack may be flagged
        // (per js_make_flagged_pointer ()), typically
        // as a stack frame function object
        val.raw &= ~2;

    } else if (!js_is_primitive(val))
        return;

    val.raw |= 1ULL << 63;
    js_gc_env *gc = env->gc;
    js_gc_push_val(gc, val, 'R');
}

// ------------------------------------------------------------
//
// js_gc_ref_values
//
// ------------------------------------------------------------

static void js_gc_ref_values (js_gc_env *gc) {

    for (;;) {
        js_mutex_enter(gc->mutex);

        js_val val_to_mark;
        js_gc_val *elem = gc->ref_values;

        if (elem) {
            // pop element from 'ref_values'
            val_to_mark = elem->val;
            gc->ref_values = elem->next;
            // push element into 'free_pool'
            elem->next = gc->free_pool;
            gc->free_pool = elem;
        }

        // get all_values head, in case we need
        // to verify an untrusted input value
        js_gc_val *search_val = gc->all_values;

        js_mutex_leave(gc->mutex);
        if (!elem)
            break;

        // mark the value, but if it is untrusted
        // input from see js_gc_notify2 (), then
        // first find the input in 'all_values'

        if (val_to_mark.raw & (1ULL << 63)) {
            val_to_mark.raw ^= (1ULL << 63);

            while (search_val &&
                        search_val->val.raw !=
                                    val_to_mark.raw)
                search_val = search_val->next;

            if (!search_val)
                continue;
        }

        js_gc_mark_val(gc, val_to_mark);
    }
}

// ------------------------------------------------------------
//
// js_gc_run_sweep
//
// ------------------------------------------------------------

static void js_gc_run_sweep (js_gc_env *gc) {

    js_gc_val *marked_values = NULL;
    js_gc_val *marked_tail = NULL;
    int num_marked_values = 0;

    for (;;) {
        js_mutex_enter(gc->mutex);

        // pop element from 'all_values'
        js_gc_val *elem = gc->all_values;
        if (!elem)
            break;
        const js_val val = elem->val;
        gc->all_values = elem->next;

        js_mutex_leave(gc->mutex);

        bool marked_bit_was_clear =
            js_gc_compare_and_swap(
                val, js_gc_marked_bit, false,
                js_gc_marked_bit | js_gc_notify_bit);

        if (!marked_bit_was_clear) {

            elem->next = marked_values;
            marked_values = elem;
            ++num_marked_values;

            if (!marked_tail)
                marked_tail = elem;

        } else {

            // push half of the now-free element
            // into 'free_pool', delete the others
            if (num_marked_values & 1) {
                js_mutex_enter(gc->mutex);
                elem->next = gc->free_pool;
                gc->free_pool = elem;
                js_mutex_leave(gc->mutex);
            } else
                js_free(elem);

            // delete the value
            // xxx complex object delete
            void *ptr = js_get_pointer(val);
            *(volatile uint32_t *)ptr = 0xDEADF00D;
            js_free(ptr);
        }
    }

    //
    // if values were pushed into alternate lists
    // during sweep (see also js_gc_push_val ()),
    // then move those values to the primary lists
    //

    if (gc->ref_values_2) {
        gc->ref_values = gc->ref_values_2;
        gc->ref_values_2 = NULL;
    }

    js_gc_val *all_values_2 = gc->all_values_2;
    if (all_values_2) {
        marked_tail->next = all_values_2;
        gc->all_values_2 = NULL;
    }

    gc->all_values = marked_values;

    gc->p_num_all_values->num =
        (gc->num_all_values = num_marked_values);

    //
    // reset gc state and finish
    //

    gc->num_new_values = 0;
    gc->run_sweep = false;

    js_mutex_leave(gc->mutex);
}

// ------------------------------------------------------------
//
// js_gc_loop
//
// ------------------------------------------------------------

static void js_gc_loop (void *_gc_arg) {

    js_gc_env *gc = _gc_arg;
    const js_val shadow_obj = gc->env->shadow_obj;

    for (;;) {

        // a new gc cycle is beginning,
        // we start with marking the shadow object

        js_gc_compare_and_swap(shadow_obj,
            js_gc_marked_bit, true, js_gc_marked_bit);

        js_gc_mark_obj(gc, js_get_pointer(shadow_obj));

        for (;;) {

            // process queued incoming requests,
            // until sweep sequence is initiated
            js_gc_ref_values(gc);

            if (gc->run_sweep)
                break;

            js_gc_free_void_ptrs(gc);

            // sleep until more work arrives

            js_mutex_enter(gc->mutex);
            if (!gc->wakeup) {
                gc->sleeping = true;
                js_event_wait(gc->event, gc->mutex, -1U);
                gc->sleeping = false;
            }
            gc->wakeup = false;
            js_mutex_leave(gc->mutex);
        }

        js_gc_ref_values(gc);

        js_gc_run_sweep(gc);
    }
}

// ------------------------------------------------------------
//
// js_gc_collect
//
// ------------------------------------------------------------

static void js_gc_collect (js_gc_env *gc, bool full) {

    // wait while the gc thread is busy, possibly
    // with a previously-initiated sweep sequence.
    // wait until the thread has gone to sleep,
    // has no pending requests on its 'ref_values'
    // queue, and not flagged to begin sweeping.

    js_mutex_enter(gc->mutex);
    for (;;) {
        bool gc_is_doing_work =  gc->ref_values
                             ||  gc->run_sweep
                             || !gc->sleeping;
        if (!gc_is_doing_work)
            break;
        js_event_wait(gc->event, gc->mutex, 55U);
    }

    // now the gc thread has gone to sleep, after
    // marking 'shadow_obj' as well as any values
    // send via js_gc_notify ().  the last thing
    // to do is walk all stacks so the gc thread
    // can mark values referenced only by locals.
    js_mutex_leave(gc->mutex);
    js_gc_walkstack(gc->env);

    // we can now request sweep, then wait until
    // the sweep is finished.

    js_mutex_enter(gc->mutex);
    gc->run_sweep = gc->wakeup = true;
    gc->walked_stack = false;

    if (gc->sleeping)
        js_event_post(gc->event);

    while (full && gc->run_sweep)
        js_event_wait(gc->event, gc->mutex, 55U);

    js_mutex_leave(gc->mutex);
}

// ------------------------------------------------------------
//
// js_gc_configure
//
// ------------------------------------------------------------

static void js_gc_configure (js_gc_env *gc, js_link *arg_ptr) {

    js_link *stk_top = gc->env->stack_top;
    js_val arg_val = (arg_ptr = arg_ptr->next) != stk_top
                   ? arg_ptr->value : js_undefined;

    if (js_is_number(arg_val)) {

        int threshold = (int)js_get_number(arg_val);
        if (threshold > 1)
            gc->threshold = threshold;
    }

    //
    // start a thread for the garbage collector
    //

    if (!gc->thread) {

        void *thread = js_thread_new(js_gc_loop, gc);
        if (!thread)
            js_check_alloc(thread);

        gc->thread = thread;
    }
}

// ------------------------------------------------------------
//
// js_gc_util
//
// ------------------------------------------------------------

static js_val js_gc_util (js_c_func_args) {

    js_val ret_val = js_true;
#ifdef js_gc_build

    js_link *arg_ptr = stk_args->next;
    js_val arg_val = arg_ptr != js_stk_top
                   ? arg_ptr->value : js_undefined;
    js_gc_env *gc = env->gc;

    // null parameter - configure gc
    if (js_is_undefined_or_null(arg_val)) {

        js_gc_configure(gc, arg_ptr);

    // boolean parameter - requests collection.
    // if parameter is true, waits until sweep ends.

    } else if (js_is_boolean(arg_val)) {

        const bool full = arg_val.raw == js_true.raw;
        js_gc_collect(gc, full);
    }

#endif
    js_return(ret_val);
}

// ------------------------------------------------------------
//
// js_gc_init
//
// ------------------------------------------------------------

static void js_gc_init (js_environ *env) {

    // create the garbage collector work area

    js_gc_env *gc = /* alloc and clear */
                    js_calloc(1, sizeof(js_gc_env));

    gc->event = js_check_alloc(js_event_new());
    gc->mutex = js_check_alloc(js_mutex_new());

    gc->env = env;
    env->gc = gc;

    // inhibit sweep while initializing
    gc->threshold = INT_MAX;
}

// ------------------------------------------------------------
//
// js_gc_init_2
//
// ------------------------------------------------------------

static void js_gc_init_2 (js_environ *env) {

    js_gc_env *gc = env->gc;

    // publish the gc utility interface

    js_val name = js_str_c(env, "js_gc_util");
    js_val func;

    js_newprop(env, env->shadow_obj, name) = func =
        js_newfunc(env, js_gc_util, name, NULL,
                   js_strict_mode, /* closures */ 0);

    gc->p_num_all_values =
        js_ownprop(env, func, env->str_number, true);
    gc->p_num_all_values->num = gc->num_all_values;
}
