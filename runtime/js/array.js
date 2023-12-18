
// ------------------------------------------------------------
//
// Array
//
// ------------------------------------------------------------

;(function Array_init () {

const Object = _global.Object;
const check_obj_arg = _shadow.check_obj_arg;

//
// Array constructor
//

function Array (length) {

    return undefined;
}

//
// Array prototype
//
// note that the prototype object was already created
// in js_arr_init () in file arr.c. it is attached
// to new array objects in js_newarr ().
//

defineNotEnum(_global, 'Array', Array);
_shadow.Array = Array; // keep a copy

const Array_prototype = ([]).__proto__;
defineNotEnum(Array_prototype, 'constructor', Array);

defineProperty(Array, 'prototype',
    { value: Array_prototype, writable: false });

defineNotEnum(Array_prototype, 'concat', function concat () {});

defineNotEnum(Array_prototype, 'copyWithin', function copyWithin () {});

defineNotEnum(Array_prototype, 'pop', function pop () {
    // make sure length is a number (if possibly NaN),
    // and raise an error if length is bigint and symbol
    let len = +this.length;
    let element;
    if (len > 0) {
        --len;
        element = this[len];
        delete this[len];
    } else
        len = 0;
    this.length = len;
    return element;
});

defineNotEnum(Array_prototype, 'push', function push (...elements) {
    // make sure length is a number (if possibly NaN),
    // and raise an error if length is bigint and symbol
    let len = +this.length;
    if (!(len >= 0)) // if NaN or negative number
        len = 0;
    let count = elements.length;
    if (len + count > 2 ** 53 - 1)
        _shadow.RangeError_array_length();
    for (let index = 0; index < count; index++)
        this[len++] = elements[index];
    this.length = len;
});

//
// join
//

defineNotEnum(Array_prototype, 'join', function join (sep) {

    const arr = check_obj_arg(this);
    let len = +this.length;
    if (!(len >= 0)) // if NaN or negative number
        len = 0;

    sep = (sep === undefined) ? ',' : ('' + sep);
    let sep1 = '';

    let str = '';
    let idx = 0;
    while (idx < len) {
        str += sep1;
        sep1 = sep;
        const e = arr[idx++];
        str += (e === undefined || e === null)
            ? '' : ('' + e);
    }
    return str;
});

//
// keys
//

defineNotEnum(Array_prototype, 'keys', function keys () {});

//
// toString
//

defineNotEnum(Array_prototype, 'toString', function toString () {

    const arr = check_obj_arg(this);
    let func = arr['join'];
    if (typeof(func) !== 'function')
        func = _shadow.object_toString;
    return func.call(arr);
});

defineNotEnum(Array_prototype, Symbol.iterator, iterator);

//
// Array.prototype[Symbol.unscopables]
//

defineNotEnum(Array_prototype, Symbol.unscopables, {
    __proto__: null, copyWithin: true, entries: true,
    fill: true, find: true, findIndex: true, flat: true,
    flatMap: true,  includes: true, keys: true, values: true,
    at: true,  findLast: true, findLastIndex: true });

//
// Array.isArray
//

_shadow.isArray = function isArray (maybeArray) {
    // result should be exactly 0x0033 if length is a data property
    // in an array object, see also js_property_flags () in descr1.c
    return (js_property_flags(maybeArray, 'length') === 0x0033);
}

//
// Array.from
//

//defineNotEnum(Array, 'from', function from (src) {
//});

//
// [Symbol.iterator] ()
//

const array_iterator_symbol = Symbol('array_iterator');

const iterator_prototype = { __proto__: _shadow.this_iterator };
defineNotEnum(iterator_prototype, 'next', iterator_next);
defineConfig(iterator_prototype, Symbol.toStringTag, 'Array Iterator');

function iterator () {

    const iterator_object =
            _shadow.create_array_like_iterator(
                                this, array_iterator_symbol);
    iterator_object.__proto__ = iterator_prototype;
    return iterator_object;
}

function iterator_next () {

    const private_state = js_private_object(
                                this, array_iterator_symbol);
    return private_state.next();
}

overrideFunctionName(iterator, _shadow.this_iterator[Symbol.iterator].name);
overrideFunctionName(iterator_next, 'next');

})()    // Array_init
