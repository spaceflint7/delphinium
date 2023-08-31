
//
// js_check_alloc
//

static void *js_check_alloc (void *ptr) {

    if (!ptr || (intptr_t)ptr & 7) {
        // check out of memory, and also if bad alignment.
        // gcc assumes malloc returns 8-byte aligned memory,
        // so this just checks for a zero pointer.
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    return ptr;
}

//
// js_malloc
//

static void *js_malloc (int size) {

    return js_check_alloc(malloc(size));
}

static void *js_calloc (int count, int size) {

    return js_check_alloc(calloc(count, size));
}
