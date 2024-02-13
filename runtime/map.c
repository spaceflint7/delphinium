
// ------------------------------------------------------------
//
// support functionality for javascript map/set
//
// ------------------------------------------------------------

typedef struct js_map {

    intmap *objects_map;
    objset *strings_set;
    objset *bigints_set;
    struct js_lock *lock; // sync with gc
    int elem_count;

} js_map;

// ------------------------------------------------------------
//
// js_map_string
//
// ------------------------------------------------------------

static js_val js_map_string (js_environ *env, js_map *map,
                             js_val key, bool set_or_get) {

    // in plain js objects, the shape mechanism
    // interns string property values into the
    // global env->strings_set.  (see shape.c)
    // we do the same here with map string keys.

    objset_id *id = js_get_pointer(key);

    if (set_or_get) {   // 'set' command

        if (!map->strings_set) {
            map->strings_set =
                js_check_alloc(objset_create());
        }

        bool copy;
        id = js_check_alloc(objset_intern(
                    &map->strings_set, id, &copy));

        id->flags = js_prim_is_string;

    } else {            // 'get' command

        if (!map->strings_set) {
            // no string key can be found
            // in a map without a string set
            return js_make_number(0);
        }

        id = objset_search(map->strings_set,
                           id->data, id->len);
        if (!id) {
            // a string key can only be found
            // if it also exists in string set
            return js_make_number(0);
        }
    }

    return js_make_primitive_string(id);
}

// ------------------------------------------------------------
//
// js_map_bigint
//
// ------------------------------------------------------------

static js_val js_map_bigint (js_environ *env, js_map *map,
                             js_val key, bool set_or_get) {

    // in plain js objects, the shape mechanism
    // interns string property values into the
    // global env->strings_set.  (see shape.c)
    // we do the same here with map bigint keys.

    const uint32_t *ptr_big = js_get_pointer(key);
    const uint32_t len_big = (*ptr_big + 1)
                           * sizeof(uint32_t);

    if (!map->bigints_set) {

        if (!set_or_get) {  // 'get' command
            // no bigint key can be found
            // in a map without a bigint set
            return js_make_number(0);
        }

        if (!map->bigints_set) {

            map->bigints_set =
                js_check_alloc(objset_create());
        }
    }

    objset_id *id = objset_search(
            map->bigints_set, ptr_big, len_big);

    if (!id) {

        if (!set_or_get) {  // 'get' command
            // a bigint key can only be found
            // if it also exists in bigint set
            return js_make_number(0);
        }

        id = js_malloc(
                    sizeof(objset_id) + len_big);
        id->len = len_big;
        id->flags = 0;
        memcpy(id->data, ptr_big, len_big);

        objset_id *id2 =
            js_check_alloc(objset_intern(
                    &map->strings_set, id, NULL));

        if (unlikely(id != id2)) {
            // should never happen unless out of mem
            js_check_alloc(id2);
            fprintf(stderr, "Corrupted!\n");
            exit(1); // __builtin_trap();
        }
    }

    return js_make_primitive_bigint(id->data);
}

// ------------------------------------------------------------
//
// js_map_set
//
// ------------------------------------------------------------

static void js_map_set (js_environ *env, js_map *map,
                        js_link *arg_ptr) {

    js_val key = js_undefined;
    js_val val = js_undefined;

    if (arg_ptr != js_stk_top) {
        if ((arg_ptr = arg_ptr->next) != js_stk_top) {
            key = arg_ptr->value;
            if ((arg_ptr = arg_ptr->next) != js_stk_top)
                val = arg_ptr->value;
        }
    }

    //
    // if key is a primitive, make it unique
    //

    if (js_is_primitive(key)) {
        int prim_type = js_get_primitive_type(key);
        if (prim_type == js_prim_is_string)
            key = js_map_string(env, map, key, true);
        else if (prim_type == js_prim_is_bigint)
            key = js_map_bigint(env, map, key, true);
        else if (prim_type == js_prim_is_symbol) {
            // flag symbol so it never gets deleted
            objset_id *id = js_get_pointer(key);
            if (!js_str_is_interned(id))
                js_str_flag_as_interned(id);
        }

    } else if (js_is_object(key)) {
        // make sure the gc marks the key,
        // in case the map was already scanned,
        // and nothing else references that key.
        // we do the same for the value, below.
        //js_gc_notify(env, key);
    }
    //js_gc_notify(env, val);

    //
    // insert value into the map
    //

    js_lock_enter_exc(map->lock);

    bool was_added;
    bool ok = intmap_set_or_add(&map->objects_map,
                    key.raw, val.raw, &was_added);

    js_lock_leave_exc(map->lock);

    if (!ok)
        js_check_alloc(NULL); // out of memory

    if (was_added)
        map->elem_count++;
}

// ------------------------------------------------------------
//
// js_map_get
//
// ------------------------------------------------------------

static js_val js_map_get (js_environ *env, js_map *map,
                          js_link *arg_ptr, int cmd) {

    if (cmd == /* 0x4E */ 'N')
        return js_make_number((double)map->elem_count);

    js_val key = js_undefined;
    if (arg_ptr != js_stk_top) {
        if ((arg_ptr = arg_ptr->next) != js_stk_top)
            key = arg_ptr->value;
    }

    const bool _get = (cmd == /* 0x47 */ 'G');
    const bool _del = (cmd == /* 0x52 */ 'R');

    do {

        //
        // if key is a primitive, make it unique
        //

        if (js_is_primitive(key)) {
            int prim_type = js_get_primitive_type(key);
            if (prim_type == js_prim_is_string)
                key = js_map_string(env, map, key, false);
            else if (prim_type == js_prim_is_bigint)
                key = js_map_bigint(env, map, key, false);
            if (!key.raw)
                break;
        }

        //
        // get or delete value from the map
        //

        js_lock_enter_shr(map->lock);

        js_val val;
        bool ok = intmap_get_or_del(map->objects_map,
                            key.raw, &val.raw, _del);

        js_lock_leave_shr(map->lock);

        if (ok) {
            if (_get)
                return val;
            if (_del)
                map->elem_count--;
            return js_true;
        }

    } while (false);

    // result is undefined for a 'get' command,
    // or false for a 'has' or a 'del' command.
    return _get ? js_undefined : js_false;
}

// ------------------------------------------------------------
//
// js_map_iter
//
// ------------------------------------------------------------

static js_val js_map_iter (js_environ *env, js_map *map,
                           js_link *arg_ptr) {

    if (arg_ptr != js_stk_top) {
        if ((arg_ptr = arg_ptr->next) != js_stk_top) {
            js_val ctx = arg_ptr->value;

            void *ctx_ptr = js_get_pointer(ctx);
            if (js_obj_is_exotic(
                        ctx_ptr, js_obj_is_array)) {

                // ctx[0] is internal iteration index
                const js_val ctx_0 =
                            js_arr_get(ctx, (0U + 1U));
                int index = (int)js_get_number(ctx_0);

                uint64_t key, val;
                if (intmap_get_next(map->objects_map,
                                &index, &key, &val)) {

                    js_arr_set(env, ctx, (0U + 1U),
                            js_make_number(index));
                    js_arr_set(env, ctx, (1U + 1U),
                            ((js_val){ .raw = key }));
                    js_arr_set(env, ctx, (2U + 1U),
                            ((js_val){ .raw = val }));

                    return js_true;
                }
            }
        }
    }

    return js_false;
}

// ------------------------------------------------------------
//
// js_map_util_2
//
// ------------------------------------------------------------

static js_val js_map_util_2 (js_environ *env,
                             js_val arg, int cmd) {

    // command is 'D' for Destroy or 'E' for Erase
    if (    cmd >= /* 0x44 */ 'D'
         && cmd <= /* 0x45 */ 'E'
         && js_is_number(arg)) {

        js_map *map = (js_map *)(uintptr_t)arg.num;

        intmap_destroy(map->objects_map);
        objset_destroy(map->strings_set);
        objset_destroy(map->bigints_set);

        // command is 'E' for Erase
        if (cmd == /* 0x45 */ 'E') {

            map->objects_map =
                js_check_alloc(intmap_create());

            map->strings_set = NULL;
            map->bigints_set = NULL;
            map->elem_count  = 0;

        } else { // command is 'D' for Destroy

            js_lock_free(map->lock);
            js_free(map);
        }
    }

    if (cmd != /* 0x43 */ 'C')
        return js_undefined;

    // command is 'C' for Create
    js_map *map = js_malloc(sizeof(js_map));
    map->objects_map =
            js_check_alloc(intmap_create());
    map->strings_set = NULL;
    map->lock = js_check_alloc(js_lock_new());
    map->elem_count = 0;

    if (!js_is_undefined_or_null(arg)) {
        js_val iter[3];
        js_newiter(env, iter, 'O', arg);
        while (iter[0].raw) {
            // XXX
            js_nextiter1(env, iter);
        }
    }

    return js_make_number((double)(uintptr_t)map);
}

// ------------------------------------------------------------
//
// js_map_util
//
// ------------------------------------------------------------

static js_val js_map_util (js_c_func_args) {

    int cmd = 0;
    js_val arg = js_undefined;
    js_val ret = js_undefined;

    js_link *arg_ptr = stk_args;
    if ((arg_ptr = arg_ptr->next) != js_stk_top) {
        cmd = (int)arg_ptr->value.num;
        if ((arg_ptr = arg_ptr->next) != js_stk_top)
            arg = arg_ptr->value;
    }

    // command is (set) 'S' or (iter) 'I'
    // or (get) 'G' or 'H' or 'R'
    if (cmd >= /* 0x46 */ 'F' && js_is_number(arg)) {

        js_map *map =
                (js_map *)(uintptr_t)arg.num;

        if (cmd == /* 0x53 */ 'S') {
            // command is 'S' for Set
            js_map_set(env, map, arg_ptr);

        } else if (cmd == /* 0x49 */ 'I') {
            ret = js_map_iter(env, map, arg_ptr);

        } else {
            // command is neither 'S' nor 'I'
            ret = js_map_get(env, map, arg_ptr, cmd);
        }

    } else {
        // command is 'C' or 'D' or 'E'
        ret = js_map_util_2(env, arg, cmd);
    }

    js_return(ret);
}

// ------------------------------------------------------------
//
// js_map_init
//
// ------------------------------------------------------------

void js_map_init (js_environ *env) {

    // shadow.js_map_util function
    js_newprop(env, env->shadow_obj,
        js_str_c(env, "js_map_util")) =
            js_unnamed_func(js_map_util, 4);
}
