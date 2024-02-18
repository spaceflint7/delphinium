'use strict';

// ------------------------------------------------------------
//
// the shadow object keeps references to any global objects
// that our runtime logic might need to call, e.g. getting
// a TypeError object to report an error.
//
// ------------------------------------------------------------

const _global = global;

const _shadow = _global.shadow;
delete _global.shadow;
_shadow.global = _global;

let _Symbol;

// ------------------------------------------------------------
//
// global functions
//
// ------------------------------------------------------------

const defineProperty = _shadow.js_defineProperty;

function defineNotEnum (obj, prop, v) {
    return defineProperty(obj, prop, { value: v,
        configurable: true, writable: true, /* not enumerable */});
}

function defineConfig (obj, prop, v) {
    return defineProperty(obj, prop, { value: v,
        configurable: true /* not writable, not enumerable */});
}

function overrideFunctionName (func_obj, new_name) {
    return defineProperty(func_obj, 'name', { value: new_name });
}

// ------------------------------------------------------------
//
// internal functions exported by our C runtime
//
// ------------------------------------------------------------

const js_keys_in_object = _shadow.js_keys_in_object;
const js_property_flags = _shadow.js_property_flags;
const js_str_print = _shadow.js_str_print;
const js_getOrSetPrototype = _shadow.js_getOrSetPrototype;
const js_private_object = _shadow.js_private_object;

//
// global variables
//

defineNotEnum(_global, 'globalThis', _global);

//
// component / modules
//

#include "object.js"
#include "symbol.js"
#include "utils.js"
#include "string.js"
#include "array.js"
#include "function.js"

#include "math.js"
#include "console.js"
#include "number.js"
#include "boolean.js"
#include "bigint.js"
#include "mapset.js"
#include "error.js"

// all private values which must persist beyond this point,
// must be referenced via _shadow, or they will be collected.
gc.threshold = 12345; // also starts gc thread
