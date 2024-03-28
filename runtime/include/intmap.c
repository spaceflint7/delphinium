
// cls && /gcc/bin/gcc -mconsole -municode -std=c99 -O3 intmap.c  && ./a.exe

// ------------------------------
//
// int64-to-int64 map, inspired by
// https://github.com/Mashpoe/c-hashmap
//
// ------------------------------

#include <stdlib.h>
#include "intmap.h"

typedef struct intmap_entry intmap_entry;
struct intmap_entry {

    // zero if the entry is not in use.  negative value
    // is a bitwise NOT of the index number for the next
    // entry in order of insertion.  positive value is
    // a deleted entry, see intmap_get_or_del ()
    int32_t next_index;
    int32_t unused;
    int64_t key;
    int64_t value;
};

struct intmap {

    // note that offsetof(intmap, first_index) must equal
    // offsetof(intmap_entry, next_index), see 'last_index'.
    // this is zero, or a bitwise NOT of the index number.
    int first_index;

    // this is the index of the most recently 'set' entry.
    // it is initially zero, so intmap[0]->next_index points
    // to 'first_index' just above, see also intmap_create ()
    int last_index;

    // number of entries allocated in this map
    int capacity;

    // number of entries populated (including deleted)
    int count;

};

#define header_size_in_entries                  \
    (   (   sizeof(struct intmap)               \
          + sizeof(struct intmap_entry) - 1)    \
        /   sizeof(struct intmap_entry))

#define get_entry(map,index) \
    ((intmap_entry *)map + (index))

//
// intmap_create
//

intmap *intmap_create (void) {

    // capacity is used as an AND mask,
    // so must always be a factor of two
    const int _initial_capacity = 4;

    // contiguous allocation for the intmap control
    // structure and entries.  this means that index #0
    // corresponds to the intmap control structure itself,
    // and valid entries start at 'header_size_in_entries'.

    const int num_entries_to_allocate =
        header_size_in_entries + _initial_capacity;

    intmap *map = calloc(num_entries_to_allocate,
                         sizeof(intmap_entry));

    if (map)
        map->capacity = _initial_capacity;

    return map;
}

//
// intmap_resize_entry
//

static intmap_entry *intmap_resize_entry (
                            intmap *map, uint64_t key) {

    int index = key & (map->capacity - 1);

    for (;;) {

        // note that when indexing, we skip the map control
        // structure, which resides just before the entries
        intmap_entry *new_entry =
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
// intmap_resize
//

static intmap *intmap_resize (intmap *old_map) {

    // allocate a new map with twice the capacity.  this is
    // a contiguous allocation, same as in intmap_create ()

    int new_capacity = old_map->capacity << 1;

    intmap *new_map = calloc(
                new_capacity + header_size_in_entries,
                sizeof(intmap_entry));

    if (new_map) {

        new_map->capacity = new_capacity;

        // copy entries from the old map to the new map

        int old_index = old_map->first_index;
        if (old_index) {
            old_index = ~old_index;

            for (;;) {

                intmap_entry *old_entry =
                                get_entry(old_map, old_index);

                if (old_entry->next_index < 0) {
                    // copy only non-deleted entries
                    intmap_entry *new_entry =
                            intmap_resize_entry(
                                    new_map, old_entry->key);

                    new_entry->key = old_entry->key;
                    new_entry->value = old_entry->value;
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
// intmap_set_or_add
//

bool intmap_set_or_add (intmap **ptr_to_map,
                        uint64_t key, uint64_t value,
                        bool *was_added) {

    intmap *map = *ptr_to_map;
    int index = key & (map->capacity - 1);
    int del_index = 0;

    for (;;) {

        // note that when indexing, we skip the map control
        // structure, which resides just before the entries
        intmap_entry *entry =
                get_entry(map, index + header_size_in_entries);

        if (!entry->next_index) {
            // we reached an entry that was never populated,
            // so there is no need to search any further
            if (was_added)
                *was_added = true;

            if (del_index) {
                // overwrite a deleted entry if one was found
                // while searching.  we restore 'next' index
                // by negating it, see also intmap_get_or_del ()
                entry = get_entry(map, del_index);
                entry->next_index = -entry->next_index;

            } else if ((++map->count) < map->capacity >> 1) {
                // the map can accomodate a new entry without
                // yet having to grow it.  so we connect the
                // new entry (via its 'next' field) to the last
                // entry in the map, and point the last entry
                // in the map, to this new entry.
                index += header_size_in_entries;
                get_entry(map, map->last_index)->next_index = ~index;
                map->last_index = index;
                entry->next_index = ~0;

            } else {
                // resize into a new objset, then
                // get the index for the new entry
                intmap *new_map = intmap_resize(map);
                if (!new_map)
                    return false;
                *ptr_to_map = new_map;
                entry = intmap_resize_entry(new_map, key);
            }

            entry->key = key;
            entry->value = value;
            return true;
        }

        if (entry->next_index < 0) {
            if (entry->key == key) {
                // found a non-deleted entry with a matching key,
                // so we can just overwrite the value
                entry->value = value;
                if (was_added)
                    *was_added = false;
                return true;
            }
        } else { // entry->next_index > 0
            // this entry is deleted, and we can overwrite it,
            // if we exhaust the search without a key match
            del_index = index + header_size_in_entries;
        }

        // at this point, entry is deleted, or does not match.
        // keep searching by advancing in sequential order
        index = (index + 1) & (map->capacity - 1);
    }
}

//
// intmap_get_or_add
//

bool intmap_get_or_add (intmap **ptr_to_map,
                        uint64_t key, uint64_t *value,
                        bool *was_added) {

    intmap *map = *ptr_to_map;
    int index = key & (map->capacity - 1);
    int del_index = 0;

    for (;;) {

        // note that when indexing, we skip the map control
        // structure, which resides just before the entries
        intmap_entry *entry =
                get_entry(map, index + header_size_in_entries);

        if (!entry->next_index) {
            // we reached an entry that was never populated,
            // so there is no need to search any further.
            if (was_added)
                *was_added = true;

            if (del_index) {
                // overwrite a deleted entry if one was found
                // while searching.  we restore 'next' index
                // by negating it, see also intmap_get_or_del ()
                entry = get_entry(map, del_index);
                entry->next_index = -entry->next_index;

            } else if ((++map->count) < map->capacity >> 1) {
                // the map can accomodate a new entry without
                // yet having to grow it.  so we connect the
                // new entry (via its 'next' field) to the last
                // entry in the map, and point the last entry
                // in the map, to this new entry.
                index += header_size_in_entries;
                get_entry(map, map->last_index)->next_index = ~index;
                map->last_index = index;
                entry->next_index = ~0;

            } else {

                // resize into a new map, then add the new entry.
                // this second, recursive call should neither
                // fail, nor keep recursing, because the resized
                // map should always have enough space
                intmap *new_map = intmap_resize(map);
                if (!new_map)
                    return false;
                *ptr_to_map = new_map;
                entry = intmap_resize_entry(new_map, key);
            }

            entry->key = key;
            entry->value = *value;
            return true;
        }

        if (entry->next_index < 0) {
            if (entry->key == key) {
                // return the matching entry that was found
                if (value)
                    *value = entry->value;
                if (was_added)
                    *was_added = false;
                return true;
            }
        } else { // entry->next_index > 0
            // this entry is deleted, and we can overwrite it,
            // if we exhaust the search without a key match
            del_index = index + header_size_in_entries;
        }

        // at this point, entry is deleted, or does not match.
        // keep searching by advancing in sequential order
        index = (index + 1) & (map->capacity - 1);
    }
}

//
// intmap_get_or_del
//

bool intmap_get_or_del (intmap *map,
                        uint64_t key, uint64_t *value,
                        bool del_if_found) {

    int index = key & (map->capacity - 1);

    for (;;) {

        // note that when indexing, we skip the map control
        // structure, which resides just before the entries
        intmap_entry *entry =
            get_entry(map, index + header_size_in_entries);

        if (!entry->next_index) {
            // we reached an entry that was never populated,
            // so there is no need to search any further
            if (value)
                *value = 0;
            return false;
        }

        if (entry->next_index < 0 && entry->key == key) {
            // found a non-deleted entry with a matching key
            if (value)
                *value = entry->value;

            if (del_if_found) {
                // an entry is marked as deleted, by negating
                // its next_index.  note that the value should
                // be negative, as it is a bitwise NOT of the
                // index number of the next entry (see also
                // struct intmap_entry and intmap_set_or_add ()).
                entry->next_index = -entry->next_index;
                entry->key = 0;
                entry->value = 0;
            }
            return true;
        }

        // entry is deleted, or does not match.  keep searching
        // by advancing to the next entry in sequential order
        index = (index + 1) & (map->capacity - 1);
    }
}

//
// intmap_get_next
//

bool intmap_get_next (intmap *map, int *index,
                      uint64_t *key, uint64_t *value) {

    int next_index = *index;
    if (next_index < header_size_in_entries) {
        next_index = map->first_index;
        if (!next_index)
            return false; // map is empty
        next_index = ~next_index;
    } else if (next_index >= map->capacity + header_size_in_entries)
        return false;

    for (;;) {
        intmap_entry *entry = get_entry(map, next_index);
        if (!entry->next_index) {
            // end of the linked list of populated entries
            *index = map->capacity + header_size_in_entries;
            if (key)
                *key = 0;
            if (value)
                *value = 0;
            return false;
        }
        if (entry->next_index < 0) {
            // we have an entry to return
            if (key)
                *key = entry->key;
            if (value)
                *value = entry->value;
            if (!(*index = ~entry->next_index))
                *index = map->capacity + header_size_in_entries;
            return true;
        }
        // next > 0 so this entry is deleted, skip to next
        next_index = -entry->next_index;
    }
}

#undef header_size_in_entries
#undef get_entry
