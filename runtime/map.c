
// ------------------------------------------------------------
//
// support functionality for javascript map/set
//
// ------------------------------------------------------------

typedef struct js_map {

    intmap *objects_map;
    objset *strings_set;
    int elem_count;

} js_map;

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

    if (js_is_primitive_string(key)) {

        if (!map->strings_set) {
            map->strings_set =
                js_check_alloc(objset_create());
        }

        // in plain js objects, the shape mechanism
        // interns string property values into the
        // global env->strings_set.  (see shape.c)
        // we do the same here with map keys, but
        // we duplicate the string value, and use
        // a per-map objset cache.
        bool copy;
        objset_id *id = js_get_pointer(key);
        id = js_check_alloc(objset_intern(
                        &map->strings_set, id, &copy));
        id->flags = js_str_is_string;
        key = js_make_primitive(id, js_prim_is_string);
    }

    bool was_added;
    if (!intmap_set_or_add(&map->objects_map,
                    key.raw, val.raw, &was_added))
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

        if (js_is_primitive_string(key)) {

            if (!map->strings_set) {
                // no string key can be found
                // in a map without a string set
                break;
            }

            objset_id *id = js_get_pointer(key);
            id = objset_search(map->strings_set,
                               id->data, id->len);
            if (!id) {
                // a string key can only be found
                // if it also exists in string set
                break;
            }
            key = js_make_primitive(
                            id, js_prim_is_string);
        }

        js_val val;
        if (intmap_get_or_del(map->objects_map,
                        key.raw, &val.raw, _del)) {
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

        map->objects_map = NULL;
        map->strings_set = NULL;
        map->elem_count  = 0;

        // command is 'E' for Erase
        if (cmd == /* 0x45 */ 'E') {
            map->objects_map =
                js_check_alloc(intmap_create());
        }
    }

    if (cmd != /* 0x43 */ 'C')
        return js_undefined;

    // command is 'C' for Create
    js_map *map = js_malloc(sizeof(js_map));
    map->objects_map =
            js_check_alloc(intmap_create());
    map->strings_set = NULL;
    map->elem_count  = 0;

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
            // command is not 'S'
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
