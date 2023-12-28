
// ------------------------------------------------------------
//
// shape
//
// attaching properties to objects using a hash map means each
// property requires room for a hashmap entry (some 24 bytes),
// and property access involves calculation of a hash value.
//
// the shape mechanism stores properties in a sequential array
// of values, which is allocated per object.  the 'shape' is
// the companion descriptor that maps properties to indices.
//
// the shape descriptors are organized in a tree structure,
// such that a shape mapping for a particular property key is
// either an array index, or points to a new shape descriptor
// where the same property key maps to an array index.
//
// ------------------------------------------------------------

struct js_shape {

    intmap *props;
    int unique_id;
    int num_values;
};

// if bit 31 is set in js_obj->max_values, object is not extensible
#define js_obj_not_extensible 0x80000000U

// ------------------------------------------------------------
//
// js_shape_key
//
// implementation of section 7.1.19 ToPropertyKey
//
// ------------------------------------------------------------

static uint64_t js_shape_key (js_environ *env, js_val prop) {

    if (js_is_object(prop)) {

        prop = js_obj_to_primitive_string(env, prop);
    }

    do {

        if (js_is_primitive(prop)) {

            int prim_type = js_get_primitive_type(prop);

            if (prim_type == js_prim_is_string) {

                objset_id *id = js_get_pointer(prop);
                if (js_str_is_interned(id))
                    return (uint64_t)id;
                break; // skip toString()
            }

            if (prim_type == js_prim_is_symbol)
                return (uint64_t)js_get_pointer(prop);

            if (prim_type == js_prim_is_bigint) {
                prop = js_big_tostring(env, prop, 10);
                break; // skip toString()
            }
        }

        prop = js_tostring(env, prop);

    } while (false);

    // intern the string

    objset_id *id = js_get_pointer(prop);
    js_str_intern(&env->strings_set, id);
    return (uint64_t)id;
}

// ------------------------------------------------------------
//
// js_shape_new
//
// ------------------------------------------------------------

static js_shape *js_shape_new (js_environ *env,
                               js_shape *old_shape,
                               int old_count,
                               int64_t new_key) {

    intmap *old_map = old_shape->props;
    intmap *new_map = js_check_alloc(intmap_create());

    int64_t prop_key, idx_or_ptr;
    int index = 0;
    for (;;) {
        if (!intmap_get_next(old_map, &index,
                             (uint64_t *)&prop_key,
                             (uint64_t *)&idx_or_ptr))
            break;
        if (idx_or_ptr < 0) {
            // copy declarations of all actual properties
            // from the old shape to the new shape
            intmap_set(&new_map, prop_key, idx_or_ptr);
        }
    }

    intmap_set(&new_map, new_key, ~old_count);

    js_shape *new_shape = js_malloc(sizeof(js_shape));
    new_shape->props = new_map;
    new_shape->unique_id = ++env->next_unique_id;
    new_shape->num_values = old_count + 1;

    intmap_set(&old_map, new_key, (int64_t)new_shape);
    old_shape->props = old_map;

    return new_shape;
}

// ------------------------------------------------------------
//
// js_shape_set_in_obj
//
// ------------------------------------------------------------

#define js_shape_set_in_obj(obj_ptr,shape_ptr) \
    (obj_ptr)->shape = (js_shape *)(shape_ptr); \
    (obj_ptr)->shape_id = (shape_ptr)->unique_id; \

// ------------------------------------------------------------
//
// js_shape_switch
//
// ------------------------------------------------------------

static void js_shape_switch (js_obj *obj_ptr,
                             int old_count,
                             js_val new_value,
                             js_shape *new_shape) {

    // note that old_count should never actually be larger
    // than max_values.  only smaller, which means we just
    // add a new value.  or equal, which means we need to
    // allocate extra room, we allocate more than one cell.

    if (old_count >= (obj_ptr->max_values
                            & (~js_obj_not_extensible))) {

        const int new_count = old_count + js_obj_grow_factor;

        js_val *new_vals =
                    js_malloc(sizeof(js_val) * new_count);

        if (old_count) {

            js_val *old_vals = obj_ptr->values;
            memcpy(new_vals, old_vals, old_count * sizeof(js_val));

            // free old values unless it is the initial set
            // of values, allocated during js_newexobj ()
            const int exotic_type = (uintptr_t)obj_ptr->proto & 7;
            const int struct_size = js_obj_struct_size(exotic_type);
            if (old_vals != (js_val *)((uintptr_t)obj_ptr + struct_size))
                free(old_vals);
        }

        obj_ptr->max_values &= js_obj_not_extensible;
        obj_ptr->max_values |= new_count;

        obj_ptr->values = new_vals;
    }

    obj_ptr->values[old_count] = new_value;

    js_shape_set_in_obj(obj_ptr, new_shape);
}

// ------------------------------------------------------------
//
// js_shape_index
//
// ------------------------------------------------------------

int js_shape_index (js_environ *env, js_shape *shape, js_val prop) {

    int64_t idx_or_ptr;
    intmap_get(shape->props, js_shape_key(env, prop),
               (uint64_t *)&idx_or_ptr);
    return (int)~idx_or_ptr;
}

// ------------------------------------------------------------
//
// js_shape_value
//
// returns info about a specific property in the shape.
//
// if result is less than zero, it is bitwise NOT (~)
// of the array index within 'js_obj->values' for the
// particular property.
//
// if result is greater than zero, it is a pointer to
// the next shape in the hierarchy.
//
// ------------------------------------------------------------

#define js_shape_value(shape,prop,val) \
    intmap_get((shape)->props, (prop), (uint64_t *)val)

// ------------------------------------------------------------
//
// js_shape_update_cache_key
//
// calculate shape cache that member_expression_object ()
// in property_writer.js uses to update object properties
// directly, without calling js_getprop() or js_setprop().
//
// ------------------------------------------------------------

#define js_shape_update_cache_key(cache_ptr,obj_ptr,index) \
    *(cache_ptr) = (((int64_t)(obj_ptr)->shape_id << 32) | index)

// ------------------------------------------------------------
//
// js_shape_update_cache_key_descr
//
// the js_shape_update_cache_key () optimization discussed
// above is for properties directly on the js_obj->values
// array, but we want that with data descriptor properties
// as well.  we do this by turning on bit 31 in the index
// value (not the object/shape id).  if this is a read-only
// data descriptor, we turn on also bit 30.  as above, see
// also member_expression_object () in property_writer.js
//
// ------------------------------------------------------------

#define js_shape_update_cache_key_descr(cache_ptr,obj_ptr,index,flags) \
    js_shape_update_cache_key(cache_ptr,obj_ptr,((index) | \
        (((flags) & js_descr_write) ? 0x80000000U : 0xC0000000U)))

// ------------------------------------------------------------
//
// js_shape_get_next
//
// ------------------------------------------------------------

#define js_shape_get_next(shape,index,prop_key,idx_or_ptr) \
    intmap_get_next((shape)->props,(index),     \
                    (uint64_t *)(prop_key),     \
                    (uint64_t *)(idx_or_ptr))

// ------------------------------------------------------------
//
// js_shape_init
//
// ------------------------------------------------------------

static void js_shape_init (js_environ *env) {

    // create a shape for an object with no properties,
    // this shape is at the root of the shape hierarchy

    js_shape *shape = js_malloc(sizeof(js_shape));
    shape->props = js_check_alloc(intmap_create());
    shape->unique_id = ++env->next_unique_id;
    shape->num_values = 0;
    env->shape_empty = shape;

    // create a shape with room for two properties
    // without access keys, see js_private_object ()

    shape = js_malloc(sizeof(js_shape));
    shape->props = js_check_alloc(intmap_create());
    shape->unique_id = ++env->next_unique_id;
    shape->num_values = 2;
    env->shape_private_object = shape;
}
