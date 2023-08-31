
// ------------------------------------------------------------
//
// object interning using a set
//
// ------------------------------------------------------------

#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct objset objset;

typedef struct objset_id objset_id;
struct objset_id {

    // length of the id, in bytes
    uint32_t len;

    // extra info for use by caller
    uint16_t flags;

    // id data, possibly not null-terminated
    wchar_t data[];
};

// create object set
objset *objset_create (void);

// adds the specified id to the objset.  if the specified
// id was not added yet, the objset_id itself (i.e. not a
// copy) is added to the objset, and returned.  if the id
// is already in the objset, as determined by comparison
// by value, rather than pointer address comparison, then
// the previously-added objset_id is returned.
// returns NULL an error.
objset_id *objset_intern (objset **ptr_to_objset,
                          objset_id *id);

// search for an existing objset id, without adding it.
objset_id *objset_search (const objset *objset,
                          const void *id_data, int id_len);

#if false

// deletes the specified id from the objset.
void objset_delete (objset *objset, objset_id *id);

// enumerate the object set.  set index <= 0 on first
// call, and keep calling while the return value is true.
bool objset_next (const objset *objset, int *index,
                  objset_id **id_ptr);

#endif
