
// ------------------------------------------------------------
//
// int64-to-int64 map
//
// ------------------------------------------------------------

#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct intmap intmap;

// intmap_create - create an int64-to-int64 map.
// keys are 64-bit integers (zero permitted),
// values are 64-bit integers (zero permitted).
// returns the map, or NULL if cannot allocate.
intmap *intmap_create (void);

// intmap_destroy - free the map
#define intmap_destroy(map) free(map)

// intmap_set_or_add - add or replace an entry.
// the first parameter is a pointer to the map,
// because the map may need to be reallocated to
// accomodate the new entry.  in such a case, the
// return value might be false if cannot allocate.
// in all other cases, the return value is true.
bool intmap_set_or_add (intmap **ptr_to_map,
                        uint64_t key, uint64_t value,
                        bool *was_added);

// intmap_get_or_del - retrieve and optionally
// delete an entry.  if the value parameter is
// non-NULL, it receives the value found, else
// zero is stored if an entry was not found.
// if the del parameter is true, and an entry
// was found, then it is deleted.
bool intmap_get_or_del (intmap *map,
                        uint64_t key, uint64_t *value,
                        bool del);

#define intmap_set(ptr_to_map,key,value) intmap_set_or_add(ptr_to_map,key,value,NULL)

// intmap_has - returns true or false for a key
#define intmap_has(map,key) intmap_get_or_del(map,key,NULL,false)

#define intmap_get(map,key,value) intmap_get_or_del(map,key,value,false)

// intmap_del - delete
#define intmap_del(map,key,value) intmap_get_or_del(map,key,value,true)

// intmap_get_next - iterate through the entries
// in the map.  initialize the index parameter to
// a value <= 0 before the first call.  returns
// true if the next entry is available, in which
// case, the 'key' and 'value' parameters are set,
// and the function can be called again.  returns
// false if the iteration is complete.  adding new
// entries during iteration may introduce errors.
bool intmap_get_next (intmap *map, int *index,
                      uint64_t *key, uint64_t *value);
