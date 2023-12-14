
// ------------------------------------------------------------
//
// coroutine, a function executing in a secondary context,
// where this context includes a CPU stack, a js stack,
// and a js try/catch chain.  this coroutine mechanism is
// used to implement javascript generators in function.js.
//
// ------------------------------------------------------------

typedef struct js_coroutine_context {

    uintptr_t internal;
    js_val value;
    js_val value2;
    int state;
#define js_coroutine_is_resumed 0x40000000U

    int stack_size;
    js_link *stack_head;
    js_link *stack_tail;
    js_try *try_handler;

    js_environ *env;

} js_coroutine_context;

// ------------------------------------------------------------
//
// functions from platform.c or for use by it
//
// ------------------------------------------------------------

/* extern */ void js_coroutine_init2 (
                js_environ *env, js_coroutine_context *ctx);

/* extern */ bool js_coroutine_kill2 (
                                js_coroutine_context *ctx);

/* extern */ void js_coroutine_switch2 (
                                js_coroutine_context *ctx,
                                int *state, js_val *val);

/* public */ void js_coroutine_init3 (void *_ctx);

// ------------------------------------------------------------

// runtime.c declares 'included_from_runtime', so if not
// defined, then we are probably being included from the
// file platform.c, in which case, we want to stop here

#ifdef included_from_runtime

// ------------------------------------------------------------
//
// js_newcoroutine
//
// this is a helper utility function.  it is used by
// code inserted by convert_to_coroutine () in
// function_writer.js.  it calls CoroutineFunction ()
// in function.js, via the 'shadow' object.
//
// ------------------------------------------------------------

js_val js_newcoroutine (js_environ *env,
                        int kind, js_val func) {

    if (unlikely(env->func_new_coroutine.raw == 0)) {

        int64_t dummy_shape_cache;

        env->func_new_coroutine = js_getprop(
                    env, env->shadow_obj,
                    js_str_c(env, "CoroutineFunction"),
                    &dummy_shape_cache);

        js_throw_if_notfunc(env, env->func_new_coroutine);
    }

    return js_callfunc1(
                env, env->func_new_coroutine, func,
                js_make_number((double)kind));
}

// ------------------------------------------------------------
//
// js_yield
//
// ------------------------------------------------------------

js_val js_yield (js_environ *env, js_val val) {

    // save environment of coroutine
    js_link *stk_top    = env->stack_top;
    int      stk_size   = env->stack_size;
    js_try  *try_hndlr  = env->try_handler;

    // resume execution in the caller's context
    int state = 0;
    js_coroutine_switch2(NULL, &state, &val);
    state &= ~js_coroutine_is_resumed;

    // restore environment of coroutine
    env->stack_top      = stk_top;
    env->stack_size     = stk_size;
    env->try_handler    = try_hndlr;

    // throw exception, if caller requested
    if (state)
        js_throw(env, val);
    return val;
}

// ------------------------------------------------------------
//
// js_yield_star
//
// ------------------------------------------------------------

js_val js_yield_star (js_environ *env, js_val iterable_val) {

    js_val iter[3];
    js_newiter(env, iter, 'O', iterable_val);

    while (likely(iter[0].raw != 0)) {
        const js_val arg = js_yield(env, iter[2]);
        js_nextiter(env, iter, arg);
    }

    return iter[2];
}

// ------------------------------------------------------------
//
// js_coroutine_init1
//
// ------------------------------------------------------------

static js_val js_coroutine_init1 (
                        js_environ *env, js_val func_val,
                        js_val this_val, js_link *stk_last) {

    // allocate context and store parameters
    js_throw_if_notfunc(env, func_val);
    js_coroutine_context *ctx =
            js_malloc(sizeof(js_coroutine_context));
    ctx->env = env;
    ctx->value  = func_val;
    ctx->value2 = this_val;
    ctx->state = 0; // normal running state

    // we know that we are called by function wrapper ()
    // (nested within CoroutineFunction () in function.js).
    // we make a copy of the stack entries with parameters.

    // the stack frame of our caller, js_coroutine (),
    // is the end of the stack range that we want to copy;
    // now walk back to find the start of that range,
    // which would be the stack frame of wrapper ()

    js_link *stk_first = js_walkstack(stk_last);
    ctx->stack_size = js_copystack(&stk_first, &stk_last);
    ctx->stack_head = stk_first;
    ctx->stack_tail = stk_last;

    // save environment of caller
    js_link *stk_top    = env->stack_top;
    int      stk_size   = env->stack_size;
    js_try  *try_hndlr  = env->try_handler;

    // create the coroutine, then continue in init3
    js_coroutine_init2(env, ctx);

    // restore environment of caller
    env->stack_top      = stk_top;
    env->stack_size     = stk_size;
    env->try_handler    = try_hndlr;

    // return context address as a plain number
    return js_make_number((double)(uintptr_t)ctx);
}

// ------------------------------------------------------------
//
// js_coroutine_init3
//
// ------------------------------------------------------------

void js_coroutine_init3 (void *_ctx) {

    // switch to a new stack and a new top-level
    // 'try' handler to catch unhandled exceptions

    js_coroutine_context *ctx = _ctx;
    js_environ *env = ctx->env;
    env->stack_top  = ctx->stack_tail;
    env->stack_size = ctx->stack_size;

    env->try_handler = NULL;
    if (setjmp(*js_entertry(env)) == 0) {
        ctx->try_handler = env->try_handler;

        const js_val func_val = ctx->value;
        const js_val this_val = ctx->value2;
        ctx->value = ctx->value2 = js_uninitialized;

        const js_c_func c_func =
            ((js_func *)js_get_pointer(func_val))->c_func;

        // jump to the actual coroutine function.  note
        // that it should immediately call js_yield (),
        // and control returns to js_coroutine_init1 (),
        // to finish creation of the coroutine instance.
        // see write_function () in function_writer.js

        ctx->value = c_func(env, func_val, this_val,
                            ctx->stack_head);
    }

    js_val exception = js_leavetry(env);
    if (exception.raw != js_uninitialized.raw) {
        ctx->value = exception;
        ctx->state = -1; // set state to 'throw'

    } else {
        if (ctx->value2.raw != js_uninitialized.raw) {
            // value2 can specify an override return value,
            // see also coroutine () with the 'R' command
            ctx->value = ctx->value2;
        }
        ctx->state = 1; // set state to 'return'
    }

    // last switch out of terminating coroutine
    js_coroutine_switch2(ctx, &ctx->state, &ctx->value);
}

// ------------------------------------------------------------
//
// js_coroutine_resume
//
// ------------------------------------------------------------

static js_val js_coroutine_resume (
                js_environ *env, js_coroutine_context *ctx,
                js_val cmd, js_val val) {

    int should_throw;

    // command is 'R' for Return
    if (cmd.num == /* 0x52 */ (double)'R') {
        // set an override return value, and throw
        // a special value (js_uninitialized) which
        // is ignored by exception handling logic.
        // see also js_coroutine_init3 () and
        // return_statement () in statement_writer.js
        ctx->value2 = val;
        val = js_uninitialized;
        should_throw = 1;

    // command is 'T' for Throw
    } else if (cmd.num == /* 0x54 */ (double)'T') {
        should_throw = 1;

    // command is 'N' for Next
    } else if (cmd.num == /* 0x4E */ (double)'N')
        should_throw = 0;

    else {
        // unknown command, so the following should
        // cause coroutine () to throw an exception
        return js_uninitialized;
    }

    if (!ctx->internal) {
        // the coroutine has already ended, we just
        // return { value: undefined, done: true }
        return js_newobj(env, env->shape_value_done,
                            js_undefined, js_true);
    }

    if (ctx->state & js_coroutine_is_resumed) {
        // a coroutine that has already been resumed,
        // and has not yet yielded back to its caller,
        // is being resumed a second time
        js_callthrow("TypeError_coroutine_already_resumed");
    }

    // save environment of caller
    js_link *stk_top    = env->stack_top;
    int      stk_size   = env->stack_size;
    js_try  *try_hndlr  = env->try_handler;

    // resume execution in the target context
    int state = should_throw | js_coroutine_is_resumed;
    js_coroutine_switch2(ctx, &state, &val);

    // restore environment of caller
    env->stack_top      = stk_top;
    env->stack_size     = stk_size;
    env->try_handler    = try_hndlr;

    // if state is non-zero, coroutine has ended
    js_val done;
    if (state != 0) {

        js_coroutine_kill2(ctx);

        // throw exception, if coroutine requested
        if (state < 0)
            js_throw(env, val);

        done = js_true;
    } else
        done = js_false;

    return js_newobj(env,
                env->shape_value_done, val, done);
}


// ------------------------------------------------------------
//
// js_coroutine
//
// ------------------------------------------------------------

static js_val js_coroutine (js_c_func_args) {

    js_val cmd  = js_undefined;
    js_val arg1 = js_undefined;
    js_val arg2 = js_undefined;
    js_val ret  = js_uninitialized;

    js_link *arg_ptr = stk_args;
    if ((arg_ptr = arg_ptr->next) != js_stk_top) {
        cmd = arg_ptr->value;
        if ((arg_ptr = arg_ptr->next) != js_stk_top)
            arg1 = arg_ptr->value;
        if ((arg_ptr = arg_ptr->next) != js_stk_top)
            arg2 = arg_ptr->value;
    }

    // command is 'I' for Initialize
    if (cmd.num == /* 0x49 */ (double)'I') {
        // argument 1 is the function value to execute
        // in a coroutine context.  argument 2 is the
        // 'this' value to bind.
        ret = js_coroutine_init1(
                    env, arg1, arg2, stk_args);

    } else if (js_is_number(arg1)) {

        js_coroutine_context *ctx =
                (js_coroutine_context *)
                        (uintptr_t)arg1.num;

        // command is 'K' for Kill
        if (cmd.num == /* 0x4B */ (double)'K') {
            ret = js_coroutine_kill2(ctx)
                ? js_true : js_false;

        } else {
            ret = js_coroutine_resume(
                        env, ctx, cmd, arg2);
        }
    }

    if (ret.raw == js_uninitialized.raw)
        js_callthrow("TypeError_unsupported_operation");
    js_return(ret);
}

// ------------------------------------------------------------

#endif // included_from_runtime
