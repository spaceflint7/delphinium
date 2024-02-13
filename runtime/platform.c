
// ------------------------------------------------------------
//
// platform specific stuff
//
// ------------------------------------------------------------

#include <stdio.h>
#define included_from_platform
#include "runtime.c"
#include "coroutine.c"

#define jsf_created_any_coroutines 0x01000000U

#ifdef _WIN64
#include <windows.h>

// ------------------------------------------------------------
//
// gc stack walk - Windows x64
//
// ------------------------------------------------------------

void js_gc_walkstack2 (js_environ *env) {

    /* extern */ void js_gc_notify2 (
                        js_environ *env, uint64_t addr);
    //
    // send current (non-scratch) registers to gc,
    // in case any of them holds a local variable
    //

    jmp_buf regs;
    setjmp(regs);
    js_gc_notify2(env, ((_JUMP_BUFFER *)regs)->Rbx);
    js_gc_notify2(env, ((_JUMP_BUFFER *)regs)->Rbp);
    js_gc_notify2(env, ((_JUMP_BUFFER *)regs)->Rsi);
    js_gc_notify2(env, ((_JUMP_BUFFER *)regs)->Rdi);
    js_gc_notify2(env, ((_JUMP_BUFFER *)regs)->R12);
    js_gc_notify2(env, ((_JUMP_BUFFER *)regs)->R13);
    js_gc_notify2(env, ((_JUMP_BUFFER *)regs)->R14);
    js_gc_notify2(env, ((_JUMP_BUFFER *)regs)->R15);

    //
    // send contents of the CPU stack to the gc,
    // in case it holds some of the local variables
    //

    NT_TIB *tib = (NT_TIB *)__readgsqword(0x30);
    uint64_t *rsp =
            (uint64_t *)((_JUMP_BUFFER *)regs)->Rsp;
    while ((void *)rsp < tib->StackBase)
        js_gc_notify2(env, *rsp++);

    //
    // after walking the stack in the context of
    // the invoking fiber, we have to do the same
    // in every other fiber as well.  we do this
    // only when called from js_gc_walkstack ()
    //

    js_coroutine_context *ctx = env->coroutine_contexts;
    if (!ctx || ctx->internal)
        return;

    //
    // coroutine_contexts->internal is non-zero
    // to indicate the originating fiber, when
    // we are doing cross-fiber gc stack walking.
    //
    // js_coroutine_switch2 () responds to this
    // fiber-switch by calling js_gc_walkstack2
    // (this function), and immediately switching
    // back to the caller (i.e., back here).
    //

    void *current_fiber = GetCurrentFiber();
    ctx->internal = (uintptr_t)current_fiber;

    while (ctx) {
        void *other_fiber = (void *)ctx->internal2;
        if (other_fiber != current_fiber)
            SwitchToFiber(other_fiber);
        ctx = ctx->next;
    }

    env->coroutine_contexts->internal = 0;
}

// ------------------------------------------------------------
//
// coroutines - Windows x64
//
// ------------------------------------------------------------

void js_coroutine_init2 (
                js_environ *env, js_coroutine_context *ctx) {

    void *old_fiber = NULL;
    if (env->internal_flags & jsf_created_any_coroutines)
        old_fiber = GetCurrentFiber();
    else {
        old_fiber = ConvertThreadToFiber(NULL);
        if (!old_fiber &&
                    GetLastError() == ERROR_ALREADY_FIBER)
            old_fiber = GetCurrentFiber();
        env->internal_flags |= jsf_created_any_coroutines;
        // store the main fiber in the head element
        env->coroutine_contexts->internal2 = old_fiber;
    }

    void *new_fiber =
                CreateFiber(0, js_coroutine_init3, ctx);

    if (!old_fiber || !new_fiber) {
        fprintf(stderr, "Fiber error!\n");
        exit(1);
    }

    ctx->internal = (uintptr_t)old_fiber;
    ctx->internal2 = new_fiber;
    SwitchToFiber(new_fiber);
}

// ------------------------------------------------------------

void js_coroutine_kill2 (js_coroutine_context *ctx) {

    DeleteFiber(ctx->internal2);
    ctx->internal2 = NULL;
}

// ------------------------------------------------------------

void js_coroutine_switch2 (
        js_coroutine_context *ctx, int *state, js_val *val) {

    if (!ctx)
        ctx = GetFiberData();

    ctx->value = *val;
    ctx->state = *state;

    // save environment of caller
    js_environ *env      = ctx->env;
    js_link *stack_top   = env->stack_top;
    int      stack_size  = env->stack_size;
    js_try  *try_handler = env->try_handler;
    js_val   new_target  = env->new_target;

    // switch main fiber <-> coroutine fiber
    void *other_fiber = (void *)ctx->internal;
    ctx->internal = (uintptr_t)GetCurrentFiber();
    SwitchToFiber(other_fiber);

    for (;;) {

        void *fiber_of_gc_caller = (void *)
            env->coroutine_contexts->internal;
        if (!fiber_of_gc_caller)
            break;

        // if we reach here, we are not resumed by
        // normal program flow, instead were called
        // by js_gc_stackwalk2 () as part of the gc

        extern void js_gc_walkstack1 (js_environ *env,
                                      js_link *stk_ptr,
                                      js_try *try_handler,
                                      js_val new_target);
        js_gc_walkstack1(env, stack_top,
                         try_handler, new_target);

        js_gc_walkstack2(env);

        SwitchToFiber(fiber_of_gc_caller);
    }

    // restore environment of caller
    env->stack_top   = stack_top;
    env->stack_size  = stack_size;
    env->try_handler = try_handler;
    env->new_target  = new_target;

    // when we reach here, this is a normal resume
    ctx->internal = (uintptr_t)other_fiber;
    *val = ctx->value;
    *state = ctx->state;
}

// ------------------------------------------------------------
//
// js_current_time - Windows x64
//
// ------------------------------------------------------------

uint64_t js_current_time () {

    union {
        FILETIME ft;
        uint64_t u64;
    } u;
    // in units of 100-nanosecond, which we convert
    // to units of microsecond, where 1ms == 1000ns
    GetSystemTimeAsFileTime(&u.ft);
    return u.u64 * 10;
}

// ------------------------------------------------------------
//
// js_elapsed_time - Windows x64
//
// ------------------------------------------------------------

/*uint64_t js_elapsed_time () {
}*/

// ------------------------------------------------------------
//
// js_platform_init - Windows x64
//
// ------------------------------------------------------------

/*void js_platform_init () {
}*/

// ------------------------------------------------------------
//
// threads - Windows x64
//
// ------------------------------------------------------------

void *js_thread_new (void (*func)(void *), void *arg) {

    return CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
}

// ------------------------------------------------------------

void *js_mutex_new () {

    CRITICAL_SECTION *critsec =
                malloc(sizeof(CRITICAL_SECTION));
    if (critsec)
        InitializeCriticalSection(critsec);
    return critsec;
}

void js_mutex_enter (void *mutex) {

    CRITICAL_SECTION *critsec = mutex;
    EnterCriticalSection(critsec);
}

void js_mutex_leave (void *mutex) {

    CRITICAL_SECTION *critsec = mutex;
    LeaveCriticalSection(critsec);
}

// ------------------------------------------------------------

void *js_event_new () {

    CONDITION_VARIABLE *condvar =
                malloc(sizeof(CONDITION_VARIABLE));
    if (condvar)
        InitializeConditionVariable(condvar);
    return condvar;
}

void js_event_post (void *event) {

    CONDITION_VARIABLE *condvar = event;
    WakeConditionVariable(condvar);
}

void js_event_wait (void *event, void *mutex,
                    uint32_t timeout_in_ms_or_minus1) {

    CONDITION_VARIABLE *condvar = event;
    CRITICAL_SECTION *critsec = mutex;
    SleepConditionVariableCS(condvar, critsec,
                            timeout_in_ms_or_minus1);
}

// ------------------------------------------------------------

void *js_lock_new () {

    SRWLOCK *lock = malloc(sizeof(SRWLOCK));
    if (lock)
        InitializeSRWLock(lock);
    return lock;
}

void js_lock_free (void *lock) { free(lock); }
void js_lock_enter_shr (void *lock) { AcquireSRWLockShared(lock); }
void js_lock_leave_shr (void *lock) { ReleaseSRWLockShared(lock); }
void js_lock_enter_exc (void *lock) { AcquireSRWLockExclusive(lock); }
void js_lock_leave_exc (void *lock) { ReleaseSRWLockExclusive(lock); }

// ------------------------------------------------------------

uint16_t js_compare_and_swap_16 (
    void *ptr, uint16_t and_mask, uint16_t or_bits)
{
    for (;;) {
        uint16_t old = *(uint16_t *)ptr;
        uint16_t new = (old & and_mask) | or_bits;
        if (old == __sync_val_compare_and_swap(
                        (uint16_t *)ptr, old, new))
            return old;
    }
}

uint32_t js_compare_and_swap_32 (
    void *ptr, uint32_t and_mask, uint32_t or_bits)
{
    for (;;) {
        uint32_t old = *(uint32_t *)ptr;
        uint32_t new = (old & and_mask) | or_bits;
        if (old == __sync_val_compare_and_swap(
                        (uint32_t *)ptr, old, new))
            return old;
    }
}

// ------------------------------------------------------------

#else
#error unknown arch
#endif
