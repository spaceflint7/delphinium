
// ------------------------------------------------------------
//
// Utilities
//
// ------------------------------------------------------------

;(function Utils_init () {

//
// _shadow.this_iterator
//
// a default iterator which returns itself
//

const this_iterator = {};
defineNotEnum(this_iterator, Symbol.iterator, function () { return this; });
overrideFunctionName(this_iterator[Symbol.iterator], '[Symbol.iterator]');
_shadow.this_iterator = this_iterator;

//
// _shadow.array_like_iterator_next
//
// a generic iterator for an array-like object, i.e.
// object which can be indexed with an integer >= 0.
//

const array_like_iterator_prototype = {};

array_like_iterator_prototype.next = function () {

    const index = this.index;
    const length = this.length;

    let value;
    let done = false;

    if (typeof(index) === 'number'
    &&  typeof(length) === 'number'
    &&  index < length) {

        value = this.array[index];
        this.index = index + 1;
    } else
        done = true;
    return { value, done };
}

//
// _shadow.create_array_like_iterator
//

_shadow.create_array_like_iterator = function (source, type_symbol) {

    const iterator_object = {};
    const private_state = js_private_object(
                                iterator_object, type_symbol, {});
    private_state.__proto__ = array_like_iterator_prototype;
    private_state.array = source;
    private_state.length = source.length;
    private_state.index = 0;
    return iterator_object;
}

// ------------------------------------------------------------
//
// used by Object () constructor
//
// ------------------------------------------------------------

const object_wrapper_symbol = Symbol('object_wrapper');

// ------------------------------------------------------------

_shadow.set_object_wrapper_value =
function set_object_wrapper_value (obj, value) {

    // just attach a value as private state, without
    // changing the prototype.  used for Boolean.prototype.
    js_private_object(obj, object_wrapper_symbol, value);
}

// ------------------------------------------------------------

_shadow.create_object_wrapper =
function create_object_wrapper (obj, primitiveValue) {

    // attach the primitive value as private state
    js_private_object(
            obj, object_wrapper_symbol, primitiveValue);

    // set the prototype of the new object to
    // the prototype of the input primitive value
    js_getOrSetPrototype(
            obj, js_getOrSetPrototype(primitiveValue));

    if (typeof(primitiveValue) === 'string') {

        // in a string wrapper, create array-like properties
        const len = primitiveValue.length;
        for (let i = 0; i < len; i++) {
            defineProperty(obj, i, {
                    value: primitiveValue[i],
                    enumerable: true
            });
        }
        defineProperty(obj, 'length', { value: len });
    }

    return obj;
}

// ------------------------------------------------------------

_shadow.unwrap_object_wrapper =
function unwrap_object_wrapper (obj, primitiveType) {

    if (typeof(obj) === primitiveType)
        return obj;

    // extract the bigint value if this is a primitive
    // wrapped in an object (see Object () constructor),
    // otherwise calls TypeError_incompatible_object ()
    const private_object = js_private_object(
            obj, object_wrapper_symbol, null);

    if (primitiveType !== false) {
        if (typeof(private_object) !== primitiveType)
            _shadow.TypeError_incompatible_object(primitiveType);
    }

    return private_object;
}

// ------------------------------------------------------------

_shadow.for_in_iterator = function for_in_iterator (obj) {

    console.log('utils.js: for_in not yet implemented');
    return [];
}

// ------------------------------------------------------------

})()    // Utils_init
