
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

// free object set
#define objset_destroy(objset) free(objset)

// adds the specified id to the objset.  (1) but if the
// specified id is already found in the objset, returns
// the previously-added objset_id structure.  if the
// copy parameter is non-NULL, it is set to false.
// if the specified id is not found in the objset, then
// (2) if the copy parameter is NULL, then the passed
// objset_id is inserted into the objset and returned,
// or (3) if the copy parameter is non-NULL, then the
// objset_id structure is duplicated using malloc and
// memcpy, and copy is set to true.
objset_id *objset_intern (objset **ptr_to_objset,
                          objset_id *id, bool *copy);

// search for an existing objset id, without adding it.
objset_id *objset_search (const objset *objset,
                          const void *id_data, int id_len);

#if false

// deletes the specified id from the objset.
void objset_delete (objset *objset, objset_id *id);

#endif

// enumerate the object set.  set index <= 0 on first
// call, and keep calling while the return value is true.
bool objset_next (const objset *objset, int *index,
                  objset_id **id_ptr);

