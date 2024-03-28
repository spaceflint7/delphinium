
// ------------------------------------------------------------
//
// support functionality for javascript map/set
//
// ------------------------------------------------------------

typedef struct js_map {

    intmap *key2val_map;
    objset *strings_set;
    objset *bigints_set;
    struct js_mutex *mutex; // sync with gc
    uint32_t elem_count;

} js_map;

static void js_map_gc_callback (
                js_gc_env *gc, js_priv *obj, int why);

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

    if (!map) {
        // called by js_map_iter to clone key
        uint32_t len = sizeof(objset_id) + id->len;
        objset_id *id2 = js_malloc(len);
        memcpy(id2, id, len);
        return js_gc_manage(env,
                    js_make_primitive_string(id2));
    }

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

    if (!map) {
        // called by js_map_iter to clone key
        const objset_id *id = js_get_pointer(key);
        const uint32_t id_len = id->len;
        uint32_t *big =
            js_malloc(sizeof(uint32_t) + id_len);
        *big = id->len / sizeof(uint32_t);
        memcpy(big + 1, id->data, id_len);
        return js_gc_manage(env,
                    js_make_primitive_bigint(big));
    }

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

    const uint32_t *ptr_big = js_get_pointer(key);
    const uint32_t len_big = js_big_length(ptr_big)
                           * sizeof(uint32_t);

    objset_id *id = objset_search(
            map->bigints_set, ++ptr_big, len_big);

    if (!id) {

        if (!set_or_get) {  // 'get' command
            // a bigint key can only be found
            // if it also exists in bigint set
            return js_make_number(0);
        }

        id = js_malloc(sizeof(objset_id) + len_big);
        id->len = len_big;
        id->flags = js_prim_is_bigint;
        memcpy(id->data, ptr_big, len_big);

        objset_id *id2 =
            js_check_alloc(objset_intern(
                    &map->bigints_set, id, NULL));

        if (unlikely(id != id2)) {
            // should never happen unless out of mem
            js_check_alloc(id2);
            fprintf(stderr, "Corrupted!\n");
            exit(1); // __builtin_trap();
        }
    }

    return js_make_primitive_bigint(id);
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
        /*else if (prim_type == js_prim_is_symbol) {
            // flag symbol so it never gets deleted
            objset_id *id = js_get_pointer(key);
            if (!js_str_is_interned(id))
                js_str_flag_as_interned(id);
        }*/

    } else if (js_is_object(key)) {
        // make sure the gc marks the key,
        // in case the map was already scanned,
        // and nothing else references that key.
        // we do the same for the value, below.
        js_gc_notify(env, key);

    } else if (js_is_number(key)) {
        // make sure canonical zero and NaN
        if (key.num == 0)
            key.num = 0;
        else if (key.num != key.num)
            key = js_nan;
    }

    if (js_is_object_or_primitive(val))
        js_gc_notify(env, val);

    //
    // insert value into the map
    //

    js_mutex_enter(map->mutex);

    bool was_added;
    bool ok = intmap_set_or_add(&map->key2val_map,
                    key.raw, val.raw, &was_added);

    js_mutex_leave(map->mutex);

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

        } else if (js_is_number(key)) {
            // make sure canonical zero and NaN
            if (key.num == 0)
                key.num = 0;
            else if (key.num != key.num)
                key = js_nan;
        }

        //
        // get or delete value from the map
        //

        js_val val;
        bool ok = intmap_get_or_del(map->key2val_map,
                            key.raw, &val.raw, _del);

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

    do {
        if (arg_ptr == js_stk_top)
            break;
        if ((arg_ptr = arg_ptr->next) == js_stk_top)
            break;

        js_val ctx = arg_ptr->value;
        void *ctx_ptr = js_get_pointer(ctx);
        if (!js_obj_is_exotic(ctx_ptr, js_obj_is_array))
            break;

        // ctx[0] is internal iteration index
        const js_val ctx_0 =
                        js_arr_get(ctx, (0U + 1U));
        int index = (int)js_get_number(ctx_0);

        js_val key, val;
        if (!intmap_get_next(map->key2val_map,
                        &index, &key.raw, &val.raw))
            break;

        if (js_is_primitive(key)) {
            int prim_type = js_get_primitive_type(key);
            if (prim_type == js_prim_is_string)
                key = js_map_string(env, NULL, key, 0);
            else if (prim_type == js_prim_is_bigint)
                key = js_map_bigint(env, NULL, key, 0);
        }

        js_arr_set(env, ctx, (0U + 1U),
                js_make_number(index));
        js_arr_set(env, ctx, (1U + 1U), key);
        js_arr_set(env, ctx, (2U + 1U), val);

        return js_true;

    } while (false);
    return js_false;
}

// ------------------------------------------------------------
//
// js_map_create
//
// ------------------------------------------------------------

static js_val js_map_create (js_environ *env,
                             js_val kind,
                             js_link *arg_ptr) {

    // kind must be digits 1 through 4
    if (    (kind.raw & 0xFF) < 0x31
         || (kind.raw & 0xFF) > 0x34)
        return js_undefined;

    // the next parameter is an object that will
    // be set as the prototype for the new map
    if (!js_is_object(arg_ptr->value))
        return js_undefined;
    uintptr_t proto = arg_ptr->value.raw
                    & js_pointer_mask;

    // allocate a map structure
    js_map *map = js_malloc(sizeof(js_map));
    map->key2val_map =
            js_check_alloc(intmap_create());
    map->strings_set = NULL;
    map->bigints_set = NULL;
    map->mutex = js_check_alloc(js_mutex_new());
    map->elem_count = 0;

    // create a private object for the map
    js_priv *priv = js_newprivobj(env, kind);
    proto |= ((uintptr_t)priv->super.proto) & 7;
    priv->super.proto = (void *)proto;
    priv->val_or_ptr.ptr = map;
    priv->gc_callback = js_map_gc_callback;
    return js_gc_manage(env, js_make_object(priv));
}

// ------------------------------------------------------------
//
// js_map_free_set
//
// ------------------------------------------------------------

static void js_map_free_set (objset *set) {

    if (set) {
        for (int index = 0;;) {
            objset_id *id;
            if (!objset_next(set, &index, &id))
                break;
            free(id);
        }
        objset_destroy(set);
    }
}

// ------------------------------------------------------------
//
// js_map_clear
//
// ------------------------------------------------------------

static void js_map_clear (js_map *map) {

    js_mutex_enter(map->mutex);

    void *old_key2val = map->key2val_map;
    void *old_strings = map->strings_set;
    void *old_bigints = map->bigints_set;

    map->key2val_map =
            js_check_alloc(intmap_create());
    map->strings_set = NULL;
    map->bigints_set = NULL;
    map->elem_count  = 0;

    js_mutex_leave(map->mutex);

    intmap_destroy(old_key2val);
    js_map_free_set(old_strings);
    js_map_free_set(old_bigints);
}

// ------------------------------------------------------------
//
// js_map_gc_callback
//
// ------------------------------------------------------------

static void js_map_gc_callback (
                js_gc_env *gc, js_priv *priv, int why) {

    js_map *map = priv->val_or_ptr.ptr;

    if (why == 0) {
        // if notified about collection, free map
        intmap_destroy(map->key2val_map);
        js_map_free_set(map->strings_set);
        js_map_free_set(map->bigints_set);
        js_free(map->mutex);
        js_free(map);
        return;
    }

    // otherwise recursively mark keys or values,
    // in map, set, or weakmap, but not weakset
    int kind = priv->type.raw & 0xFF;
    if (kind == 0x34) // weakset
        return;

    for (int index = 0;;) {

        js_mutex_enter(map->mutex);
        js_val key, val;
        bool more = intmap_get_next(map->key2val_map,
                        &index, &key.raw, &val.raw);
        js_mutex_leave(map->mutex);
        if (!more)
            break;

        if (js_is_object(key) ||
                js_is_primitive_symbol(key)) {

            js_gc_mark_val(gc, key);
        }
        js_gc_mark_val(gc, val);
    }
}

// ------------------------------------------------------------
//
// js_map_util
//
// ------------------------------------------------------------

static js_val js_map_util (js_c_func_args) {
    js_prolog_stack_frame();

    js_val ret = js_undefined;
    do {
        js_link *arg_ptr = stk_args;

        // first parameter is command, which
        // includes the kind of map/set object
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;

        int cmd = (int)arg_ptr->value.num;
        js_val kind =
            ((js_val){ .raw = 0x4D655000 /* MAP0 */
                            + (cmd & 0xFF) });
        cmd = cmd >> 8;

        // we expect at least one more parameter
        arg_ptr = arg_ptr->next;
        if (arg_ptr == js_stk_top)
            break;

        // if command is create, we are done
        if (cmd == /* 0x43 */ 'C') {
            ret = js_map_create(env, kind, arg_ptr);
            break;
        }

        // second parameter is the map/set object,
        // which must match the specified 'kind'
        js_priv *priv =
                    js_isprivobj(arg_ptr->value, kind);
        if (!priv)
            js_callthrow("TypeError_incompatible_object");
        js_map *map = priv->val_or_ptr.ptr;

        if (cmd == /* 0x53 */ 'S') {
            // command is 'S' for Set
            js_map_set(env, map, arg_ptr);

        } else if (cmd == /* 0x49 */ 'I') {
            ret = js_map_iter(env, map, arg_ptr);

        } else if (cmd != /* 0x45 */ 'E') {
            // command is not 'S' or 'I' or 'E'
            ret = js_map_get(env, map, arg_ptr, cmd);

        } else
            js_map_clear(map);

    } while (0);
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
