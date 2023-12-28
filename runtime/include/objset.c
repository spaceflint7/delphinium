
// ------------------------------
//
// object interning using a set,
// based on intmap.c
//
// ------------------------------

#include <stdlib.h>
#include <string.h>
#include "objset.h"

//
// types
//

typedef struct objset_entry objset_entry;
struct objset_entry {

    // zero if the entry is not in use.  negative value
    // is a bitwise NOT of the index number for the next
    // entry in order of insertion.  positive value is
    // a deleted entry.  must be first field.
    int32_t next_index;

    // hash calculated for the 'len' bytes at 'id_ptr'
    uint32_t id_hash;

    // the full id is stored outside the objset
    objset_id *id_ptr;
};

struct objset {

    // note that offsetof(set, first_index) must equal
    // offsetof(objset_entry, next_index), see 'last_index'.
    // this is zero, or a bitwise NOT of the index number.
    int32_t first_index;

    // this is the index of the most recently 'set' entry.
    // it is initially zero, so set[0]->next_index points
    // to 'first_index' just above, see also objset_create ()
    int32_t last_index;

    // number of entries allocated in this set
    int32_t capacity;

    // number of entries populated (including deleted)
    int32_t count;
};

#define header_size_in_entries \
    ((sizeof(objset) * 2 - 1) / sizeof(objset_entry))

#define get_entry(set,index) \
    ((objset_entry *)set + (index))

//
// objset_create
//

objset *objset_create (void) {

    // capacity is used as an AND mask,
    // so must always be a factor of two
    const int _initial_capacity = 4;

    // contiguous allocation for the objset control
    // structure and entries.  this means that index #0
    // corresponds to the objset control structure itself,
    // and valid entries start at 'header_size_in_entries'.

    const int num_entries_to_allocate =
        header_size_in_entries + _initial_capacity;

    objset *map = calloc(num_entries_to_allocate,
                         sizeof(objset_entry));

    if (map)
        map->capacity = _initial_capacity;

    return map;
}

//
// objset_resize_entry
//

static objset_entry *objset_resize_entry (
                            objset *map, uint32_t id_hash) {

    int index = id_hash & (map->capacity - 1);
    int retry = 0;

    for (;;) {
            ++retry;

        // note that when indexing, we skip the map control
        // structure, which resides just before the entries
        objset_entry *new_entry =
                get_entry(map, index + header_size_in_entries);

        if (!new_entry->next_index) {

            // we reached an empty entry that we can reserve,
            // update the chain of allocated entries
            index += header_size_in_entries;
            get_entry(map, map->last_index)->next_index = ~index;
            map->last_index = index;
            new_entry->next_index = ~0;
            return new_entry;
        }

        // otherwise the entry for the calculated hash is taken,
        // keep searching by advancing in sequential order
        index = (index + 1) & (map->capacity - 1);
    }
}

//
// objset_resize
//

static objset *objset_resize (objset *old_map) {

    // allocate a new map with twice the capacity.  this is
    // a contiguous allocation, same as in intmap_create ()

    int new_capacity = old_map->capacity << 1;

    objset *new_map = calloc(new_capacity
                           + header_size_in_entries,
                             sizeof(objset_entry));
    if (new_map) {

        new_map->capacity = new_capacity;
        new_map->count = old_map->count;

        // copy entries from the old map to the new map

        int old_index = old_map->first_index;
        if (old_index) {
            old_index = ~old_index;

            for (;;) {

                objset_entry *old_entry =
                                get_entry(old_map, old_index);

                if (old_entry->next_index < 0) {
                    // copy only non-deleted entries
                    objset_entry *new_entry =
                            objset_resize_entry(
                                    new_map, old_entry->id_hash);

                    new_entry->id_hash = old_entry->id_hash;
                    new_entry->id_ptr = old_entry->id_ptr;
                }

                old_index = ~old_entry->next_index;
                if (!old_index)
                    break;
            }
        }
    }

    return new_map;
}

//
// objset_hash
//

__forceinline uint32_t objset_hash (const void *data, int len) {

    // some crc32-inspired hash function, following reading here:
    // https://stackoverflow.com/questions/7666509/
    // (initially tried some combination of djb2 and sdbm)

    const char *ptr = data;
    const char *ptr_end = ptr + len;
    uint32_t hash = 0;
    while (ptr < ptr_end) {
        hash ^= *ptr;
        hash = (hash >> 1) ^ (0xEDB88320U & -(hash & 1));
        hash = (hash >> 1) ^ (0xEDB88320U & -(hash & 3));
        hash = (hash >> 1) ^ (0xEDB88320U & -(hash & 5));
        hash = (hash >> 1) ^ (0xEDB88320U & -(hash & 7));
        // input is probably a utf-16 string,
        // so in many cases, every second byte is zero
        ptr += 2;
    }
    return hash;
}

//
// objset_intern
//

objset_id *objset_intern (objset **ptr_to_objset,
                          objset_id *id_ptr, bool *copy) {

    objset *map = *ptr_to_objset;
    const uint32_t id_hash =
                        objset_hash(id_ptr->data, id_ptr->len);
    int index = id_hash & (map->capacity - 1);
    int del_index = 0;

    for (;;) {

        // note that when indexing, we skip the map control
        // structure, which resides just before the entries
        objset_entry *entry =
                get_entry(map, index + header_size_in_entries);

        if (!entry->next_index) {
            // if requested, make a copy of the key
            if (copy) {
                const uint32_t copy_len =
                        sizeof(objset_id) + id_ptr->len;
                objset_id *id_copy = malloc(copy_len);
                if (!id_copy) {
                    *copy = false;
                    return NULL;
                }
                memcpy(id_copy, id_ptr, copy_len);
                id_ptr = id_copy;
                *copy = true;
            }
            // we reached an entry that was never populated,
            // so there is no need to search any further
            if (del_index) {
                // overwrite a deleted entry if one was found
                // while searching.  we restore 'next' index
                // by negating it, see also objset_delete ()
                entry = get_entry(map, del_index);
                entry->next_index = -entry->next_index;

            } else if (++map->count < map->capacity >> 1) {
                // the map can accomodate a new entry without
                // yet having to grow it.  so we connect the
                // new entry (via its 'next' field) to the
                // last entry in the map, and point the last
                // entry in the map, to this new entry.
                index += header_size_in_entries;
                get_entry(map, map->last_index)
                                        ->next_index = ~index;
                map->last_index = index;
                entry->next_index = ~0;

            } else {
                // resize into a new objset, then
                // get the index for the new entry
                objset *new_map = objset_resize(map);
                if (!new_map) {
                    if (copy) {
                        *copy = false;
                        free(id_ptr);
                    }
                    return NULL;
                }
                *ptr_to_objset = new_map;
                entry = objset_resize_entry(new_map, id_hash);
            }

            entry->id_hash = id_hash;
            entry->id_ptr = id_ptr;
            return id_ptr;
        }

        if (entry->next_index < 0) {
            if (entry->id_hash == id_hash) {

                if (id_ptr->len == entry->id_ptr->len
                &&  memcmp(id_ptr->data, entry->id_ptr->data,
                           id_ptr->len) == 0) {

                    if (copy)
                        *copy = false;
                    return entry->id_ptr;
                }
            }
        } else {
            // entry->next_index > 0, so the entry is deleted
            // and we may overwrite it later (see above)
            del_index = index + header_size_in_entries;
        }

        // at this point, entry is deleted, or does not match.
        // keep searching by advancing in sequential order
        index = (index + 1) & (map->capacity - 1);
    }
}

//
// objset_search
//

objset_id *objset_search (const objset *map,
                          const void *id_data, int id_len) {

    const uint32_t id_hash = objset_hash(id_data, id_len);
    int index = id_hash & (map->capacity - 1);

    for (;;) {

        // note that when indexing, we skip the map control
        // structure, which resides just before the entries
        objset_entry *entry =
                get_entry(map, index + header_size_in_entries);

        if (!entry->next_index)
            return NULL;

        if (entry->next_index < 0) {
            if (entry->id_hash == id_hash) {

                if (id_len == entry->id_ptr->len
                &&  memcmp(id_data, entry->id_ptr->data,
                           id_len) == 0) {

                    return entry->id_ptr;
                }
            }
        }

        // keep searching by advancing in sequential order
        index = (index + 1) & (map->capacity - 1);
    }
}

#if false

//
// objset_delete
//

void objset_delete (objset *objset, objset_id *id_ptr) {

    const uint32_t id_hash =
                    objset_hash(id_ptr->data, id_ptr->len);
    int index = id_hash & (objset->capacity - 1);

    for (;;) {

        // note that when indexing, we skip the map control
        // structure, which resides just before the entries
        objset_entry *entry =
            get_entry(objset, index + header_size_in_entries);

        if (entry->key_ptr == key_ptr) {
            // an entry is marked as deleted, by negating
            // its next_index.  note that the value should
            // be negative, as it is a bitwise NOT of the
            // index number of the next entry
            entry->next_index = -entry->next_index;
            entry->id_hash = 0;
            entry->id_ptr = NULL;
            --objset->count;
            return;
        }

        // keep searching by advancing to the next entry in
        // sequential order.  note that this would become an
        // infinite loop if the entry cannot be found, but
        // we assume we can never deref a non-existant entry.
        index = (index + 1) & (objset->capacity - 1);
    }
}

//
// objset_next
//

bool objset_next (const objset *objset, int *index,
                  objset_key **key_ptr) {

    int next_index = *index;
    if (next_index < header_size_in_entries) {
        next_index = objset->first_index;
        if (!next_index)
            return false; // map is empty
        next_index = ~next_index;
    } else if (next_index >= objset->capacity + header_size_in_entries)
        return false;

    for (;;) {
        objset_entry *entry = get_entry(objset, next_index);
        if (!entry->next_index) {
            // end of the linked list of populated entries
            *index = objset->capacity + header_size_in_entries;
            if (key_ptr)
                *key_ptr = NULL;
            return false;
        }
        if (entry->next_index < 0) {
            // we have an entry to return
            if (key_ptr)
                *key_ptr = entry->key_ptr;
            if (!(*index = ~entry->next_index))
                *index = objset->capacity + header_size_in_entries;
            return true;
        }
        // next > 0 so this entry is deleted, skip to next
        next_index = -entry->next_index;
    }
}

#endif

#undef header_size_in_entries
#undef get_entry
