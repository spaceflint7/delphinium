
// ------------------------------------------------------------
//
// platform specific stuff
//
// ------------------------------------------------------------

#include <stdio.h>
typedef struct js_try js_try;
#include "runtime.h"
#include "coroutine.c"

#define jsf_created_any_coroutines 0x01000000U

// ------------------------------------------------------------
//
// coroutines - Windows x64
//
// ------------------------------------------------------------

#ifdef _WIN64

#include <windows.h>

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
    }

    void *new_fiber =
                CreateFiber(0, js_coroutine_init3, ctx);

    if (!old_fiber || !new_fiber) {
        fprintf(stderr, "Fiber error!\n");
        exit(1);
    }

    ctx->internal = (uintptr_t)old_fiber;
    SwitchToFiber(new_fiber);
}

// ------------------------------------------------------------

bool js_coroutine_kill2 (js_coroutine_context *ctx) {

    if (!ctx->internal)
        return false;
    void *other_fiber = (void *)ctx->internal;
    ctx->internal = 0;
    DeleteFiber(other_fiber);
    return true;
}

// ------------------------------------------------------------

void js_coroutine_switch2 (
        js_coroutine_context *ctx, int *state, js_val *val) {

    if (!ctx)
        ctx = GetFiberData();

    ctx->value = *val;
    ctx->state = *state;

    // switch main fiber <-> coroutine fiber
    void *other_fiber = (void *)ctx->internal;
    ctx->internal = (uintptr_t)GetCurrentFiber();
    SwitchToFiber(other_fiber);
    ctx->internal = (uintptr_t)other_fiber;

    *val = ctx->value;
    *state = ctx->state;
}

// ------------------------------------------------------------

#else
#error unknown arch
#endif
