
// ------------------------------------------------------------
//
// Object
//
// ------------------------------------------------------------

;(function Object_init () {

const js_getOwnProperty = _shadow.js_getOwnProperty;

// ------------------------------------------------------------
//
// Object constructor
//
// ------------------------------------------------------------

function Object (val) {

    if (typeof(val) === 'object' || typeof(val) === 'function')
        return val;

    // if called as a constructor, 'this' is already
    // a new object, otherwise create a new object
    const new_obj = new.target ? this : {};

    // Object(null) or Object(undefined) returns an empty object
    if (val === undefined || val === null)
        return new_obj;

    // other types of primitives
    return _shadow.create_object_wrapper(new_obj, val);
}

// ------------------------------------------------------------
//
// Object prototype
//
// note that the prototype object was already created
// in js_obj_init () in file obj.c. it is attached to
// object values by js_newobj () in newobj.c.
//
// ------------------------------------------------------------

defineNotEnum(_global, 'Object', Object);
_shadow.Object = Object; // keep a copy

const Object_prototype = js_getOrSetPrototype({});
defineNotEnum(Object_prototype, 'constructor', Object);

defineProperty(Object, 'prototype',
    { value: Object_prototype, writable: false });

// ------------------------------------------------------------
//
// Object.prototype.hasOwnProperty
//
// ------------------------------------------------------------

defineNotEnum(Object_prototype, 'hasOwnProperty',
function hasOwnProperty (prop) {
    // spec requires we must first check validity of 'prop'
    js_property_flags(_shadow, prop);
    return hasOwn(check_obj_arg(this), prop);
});

// ------------------------------------------------------------
//
// Object.prototype.isPrototypeOf
//
// ------------------------------------------------------------

defineNotEnum(Object_prototype, 'isPrototypeOf',
function isPrototypeOf (other) {
    if (typeof(other) !== 'object')
        return false;
    let this_obj = check_obj_arg(this);
    for (;;) {
        other = js_getOrSetPrototype(other);
        if (other === null)
            return false;
        if (other === this_obj)
            return true;
    }
});

// ------------------------------------------------------------
//
// Object.prototype.propertyIsEnumerable
//
// ------------------------------------------------------------

defineNotEnum(Object_prototype, 'propertyIsEnumerable',
function propertyIsEnumerable (prop) {
    // spec requires we must first check validity of 'prop'
    js_property_flags(_shadow, prop);
    return  ((js_property_flags(
                    check_obj_arg(obj), prop)
                            & 0x111) == 0x111);
});

// ------------------------------------------------------------
//
// Object.prototype.toString
// Object.prototype.toLocaleString
//
// ------------------------------------------------------------

defineNotEnum(Object_prototype, 'toString', toString);
defineNotEnum(Object_prototype, 'toLocaleString',
function toLocaleString () { return this.toString(); });

// ------------------------------------------------------------
//
// Object.prototype.valueOf
//
// ------------------------------------------------------------

defineNotEnum(Object_prototype, 'valueOf',
function valueOf () { return Object(this); });

// ------------------------------------------------------------
//
// Object.prototype.__proto__
//
// ------------------------------------------------------------

defineProperty(Object_prototype, '__proto__', {
    get()  { return js_getOrSetPrototype(this); },
    set(v) { return js_getOrSetPrototype(this, v) },
    /* enumerable: false, */ configurable: true });

// ------------------------------------------------------------
//
// Object.prototype.__defineGetter__
// Object.prototype.__defineSetter__
//
// ------------------------------------------------------------

defineNotEnum(Object_prototype, '__defineGetter__',
function __defineGetter__ (prop, func) {
    defineGetterOrSetter(this, prop, func, 'get'); });

defineNotEnum(Object_prototype, '__defineSetter__',
function __defineSetter__ (prop, func) {
    defineGetterOrSetter(this, prop, func, 'set'); });

function defineGetterOrSetter (this_obj, prop, func, which) {

    this_obj = check_obj_arg(this_obj);
    if (typeof(func) !== 'function')
        _shadow.TypeError_expected_function();
    const descr = { get: undefined, set: undefined,
                    enumerable: true, configurable: true };
    descr[which] = func;
    defineProperty(this_obj, prop, descr);
}

// ------------------------------------------------------------
//
// Object.prototype.__lookupGetter__
// Object.prototype.__defineSetter__
//
// ------------------------------------------------------------

defineNotEnum(Object_prototype, '__lookupGetter__',
function __lookupGetter__ (prop, func) {
    lookupGetterOrSetter(this, prop, func, 'get'); });

defineNotEnum(Object_prototype, '__lookupSetter__',
function __lookupSetter__ (prop, func) {
    lookupGetterOrSetter(this, prop, func, 'set'); });

function lookupGetterOrSetter (this_obj, prop, func, which) {

    this_obj = check_obj_arg(this_obj);
    js_property_flags(_shadow, prop);
    for (;;) {
        const descr = js_getOwnProperty(this_obj, prop);
        if (descr)
            return descr[which];
        this_obj = js_getOrSetPrototype(this_obj);
        if (this_obj === null)
            return undefined;
    }
}

// ------------------------------------------------------------
//
// Object.assign
//
// ------------------------------------------------------------

defineNotEnum(Object, 'assign',
function assign (target, sources) {

    target = check_obj_arg(target);
    for (let i = 2; i < arguments.length; i++) {
        let source = arguments[i];
        if (source !== null && source !== undefined) {
            source = Object(source);
            getOwnPropertyNamesAndSymbols(source, (key, flg) => {
                if ((flg & 0x0111) === 0x111) {
                    // property is enumerable (0x0100), and
                    // set (0x0010) in a valid object (0x0001)
                    target[key] = source[key];
                }
            });
        }
    }
    return target;

});

// ------------------------------------------------------------
//
// Object.create
//
// ------------------------------------------------------------

defineNotEnum(Object, 'create',
function create (new_proto, properties) {
    if (new_proto === null || typeof(new_proto) !== 'object')
        _shadow.TypeError_invalid_prototype();
    const new_obj = { __proto__: new_proto };
    if (properties !== undefined)
        defineProperties(new_obj, properties);
    return new_obj;
});

// ------------------------------------------------------------
//
// Object.defineProperties
// Object.defineProperty
//
// ------------------------------------------------------------

defineNotEnum(Object, 'defineProperties',           defineProperties);
defineNotEnum(Object, 'defineProperty',             defineProperty);
overrideFunctionName(defineProperty, 'defineProperty');

function defineProperties (object, properties) {
    properties = check_obj_arg(properties);
    const keys_and_descrs = [];
    let count = 0;
    getOwnPropertyNamesAndSymbols(properties, (key, flg) => {
        if ((flg & 0x0111) === 0x111) {
            // property is enumerable (0x0100), and
            // set (0x0010) in a valid object (0x0001)
            const descr = properties[key];
            if (typeof(descr) !== 'object')
                _shadow.TypeError_defineProperty_descriptor();
            keys_and_descrs[count++] = key;
            keys_and_descrs[count++] = descr;
        }
    });
    for (let index = 0; index < count; ) {
        const key = keys_and_descrs[index++];
        const descr = keys_and_descrs[index++];
        defineProperty(object, key, descr);
    }
    return object;
}

// ------------------------------------------------------------
//
// Object.getOwnPropertyDescriptor
// Object.getOwnPropertyDescriptors
//
// ------------------------------------------------------------

defineNotEnum(Object, 'getOwnPropertyDescriptor',
function getOwnPropertyDescriptor (obj, prop) {
    return js_getOwnProperty(check_obj_arg(obj), prop);
});

defineNotEnum(Object, 'getOwnPropertyDescriptors',
function getOwnPropertyDescriptors (obj) {
    const result = {};
    getOwnPropertyNamesAndSymbols(obj = check_obj_arg(obj),
        (key, flg) => {
            result[key] = js_getOwnProperty(obj, key);
        }
    );
    return result;
});

// ------------------------------------------------------------
//
// Object.getOwnPropertyNames
// Object.getOwnPropertySymbols
//
// ------------------------------------------------------------

defineNotEnum(Object, 'getOwnPropertyNames',
function getOwnPropertyNames (obj) {
    // look for strings (bit value 0x0004 clear)
    return getOwnPropertyNamesOrSymbols(check_obj_arg(obj), 0x0000);
});

defineNotEnum(Object, 'getOwnPropertySymbols',
function getOwnPropertySymbols (obj) {
    // look for symbols (bit value 0x0004 set)
    return getOwnPropertyNamesOrSymbols(check_obj_arg(obj), 0x0004);
});

// ------------------------------------------------------------
//
// Object.is
//
// ------------------------------------------------------------

defineNotEnum(Object, 'is', function is (v1, v2) {

    if (typeof(v1) === 'number' || typeof(v2) === 'number') {
        if (v1 === 0 || v2 === 0) {
            // comparing -0 === 0 is always true,
            // but 1 / v gives -Infinity or +Infinity
            return (1 / v1) === (1 / v2);
        }
        if (v1 !== v1) {
            // a NaN does not compare to itself
            return (v2 !== v2);
        }
    }
    return (v1 === v2);

});

// ------------------------------------------------------------
//
// Object.preventExtensions
// Object.seal
//
// ------------------------------------------------------------

function seal_or_freeze (obj, clear_write, clear_config) {

    if (!(js_property_flags(obj) & 1))
        return obj;
    _shadow.preventExtensions(obj);
    getOwnPropertyNamesAndSymbols(obj, key => {
        const descr = js_getOwnProperty(obj, key);
        let redef = false;
        if (clear_write && descr.writable) {
            descr.writable = false;
            redef = true;
        }
        if (clear_config && descr.configurable) {
            descr.configurable = false;
            redef = true;
        }
        if (redef)
            defineProperty(obj, key, descr);
    });
}

defineNotEnum(Object, 'preventExtensions', _shadow.preventExtensions);
defineNotEnum(Object, 'seal',   obj => seal_or_freeze(obj, false, true));
defineNotEnum(Object, 'freeze', obj => seal_or_freeze(obj, true,  true));

// ------------------------------------------------------------
//
// Object.getPrototypeOf
// Object.setPrototypeOf
//
// ------------------------------------------------------------

defineNotEnum(Object, 'getPrototypeOf',             null);
defineNotEnum(Object, 'setPrototypeOf',             null);

// ------------------------------------------------------------
//
// Object.isExtensible
// Object.isFrozen
// Object.isSealed
//
// ------------------------------------------------------------

function is_sealed_or_frozen (obj, check_write, check_config) {

    let result = !_shadow.isExtensible(obj);
    if (result) {
        getOwnPropertyNamesAndSymbols(obj, key => {
            const descr = js_getOwnProperty(obj, key);
            if (check_write && descr.writable)
                result = false;
            if (check_config && descr.configurable)
                result = false;
        });
    }
    return result;
}

defineNotEnum(Object, 'isExtensible', _shadow.isExtensible);
defineNotEnum(Object, 'isFrozen', obj => is_sealed_or_frozen(obj, true, true));
defineNotEnum(Object, 'isSealed', obj => is_sealed_or_frozen(obj, false, true));

// ------------------------------------------------------------
//
// Object.keys
// Object.entries
// Object.fromEntries
// Object.values
//
// ------------------------------------------------------------

defineNotEnum(Object, 'keys',                       null);
defineNotEnum(Object, 'entries',                    null);
defineNotEnum(Object, 'fromEntries',                null);
defineNotEnum(Object, 'values',                     null);

// ------------------------------------------------------------
//
// Object.hasOwn
//
// ------------------------------------------------------------

defineNotEnum(Object, 'hasOwn', hasOwn);

function hasOwn (obj, key) {

    const flg = js_property_flags(obj, key);
    return ((flg & 0x0011) === 0x0011) ? true : false;
}

// ------------------------------------------------------------
//
// getOwnPropertyNamesAndSymbols
//
// ------------------------------------------------------------

function getOwnPropertyNamesAndSymbols (obj, callback) {

    let keys = js_keys_in_object(obj);
    const keys_length = keys.length;
    // number keys
    let idx_arr, idx_num = 0;
    for (let i = 0; i < keys_length; i++) {
        const key_as_str = keys[i];
        const flg = js_property_flags(obj, key_as_str);
        if ((flg & 0x001D) === 0x0019) {
            // object is valid (0x0001),
            // property is set in object (0x0010),
            // property is integer index (0x0008),
            // property not a symbol (0x0004)
            const key_as_num = +key_as_str;
            if (!idx_arr) {
                if (key_as_num === idx_num) {
                    // as long as keys are enumerated
                    // in the proper order, we can
                    // just forward them to callback
                    idx_num++;
                    callback(key_as_str, flg);
                    continue;
                }
                idx_arr = [];
                idx_num = 0;
            }
            // hold the keys in a temp array
            idx_arr[idx_num++] = +key_as_num;
            idx_arr[idx_num++] = key_as_str;
            idx_arr[idx_num++] = flg;
        }
    }
    // numeric keys held in the temp array
    if (idx_arr) {
        while (idx_num) {
            let i = 0;
            for (let j = 3; j < idx_num; j += 3) {
                if (idx_arr[j] < idx_arr[i])
                    i = j;
            }
            callback(idx_arr[i + 1], idx_arr[i + 2]);
            idx_arr[i + 2] = idx_arr[--idx_num];
            idx_arr[i + 1] = idx_arr[--idx_num];
            idx_arr[i]     = idx_arr[--idx_num];
        }
    }
    // string keys
    for (let i = 0; i < keys_length; i++) {
        const key = keys[i];
        const flg = js_property_flags(obj, key);
        if ((flg & 0x001D) === 0x0011) {
            // object is valid (0x0001),
            // property is set in object (0x0010),
            // property not integer index (0x0008),
            // property not a symbol (0x0004)
            callback(key, flg);
        }
    }
    // symbol keys
    for (let i = 0; i < keys_length; i++) {
        const key = keys[i];
        const flg = js_property_flags(obj, key);
        if ((flg & 0x0015) === 0x0015) {
            // object is valid (0x0001),
            // property is set in object (0x0010),
            // property is a symbol (0x0004)
            callback(key, flg);
        }
    }
}

// ------------------------------------------------------------
//
// getOwnPropertyNamesOrSymbols
//
// compare_flag should be 0x0000 to list string keys,
//                     or 0x0004 to list symbol keys.
//
// ------------------------------------------------------------

function getOwnPropertyNamesOrSymbols (obj, compare_flag) {

    const keys = [];
    let keys_index = 0;
    getOwnPropertyNamesAndSymbols(obj, (key, flg) => {
        if ((flg & 0x0004) === compare_flag)
            keys[keys_index++] = key;
    });
    return keys;
}

// ------------------------------------------------------------
//
// toString
//
// ------------------------------------------------------------

function toString () {

    let tag;
    if (this === undefined)
        tag = 'Undefined';
    else if (this === null)
        tag = 'Null';
    else if (typeof(this) === 'number')
        tag = 'Number';
    else if (typeof(this) === 'bigint')
        tag = 'BigInt';
    else if (typeof(this) === 'boolean')
        tag = 'Boolean';
    else if (typeof(this) === 'string')
        tag = 'String';
    else if (typeof(this) === 'function')
        tag = 'Function';

    else if (typeof(this) === 'object') {
        tag = this[_Symbol.toStringTag];
        if (typeof(tag) !== 'string') {
            if (_shadow.isArray(this))
                tag = 'Array';
            else {
                tag = _shadow.Object_getSpecialTag(this);
                if (!tag)
                    tag = 'Object';
            }
        }
    } else
        tag = '?unknownTypeInToString?';
    return '[object ' + tag + ']';
}

_shadow.object_toString = toString;

// ------------------------------------------------------------
//
// Object_getSpecialTag
//
// ------------------------------------------------------------

_shadow.Object_getSpecialTag = function (arg) {

    if (js_private_object(arg, _shadow.arguments_symbol, null))
        return 'Arguments';

    if (arg instanceof Error)
        return 'Error';
}

// ------------------------------------------------------------
//
// check_obj_arg
//
// ------------------------------------------------------------

function check_obj_arg (obj) {

    if (obj === undefined || obj === null)
        _shadow.TypeError_convert_null_to_object();
    return Object(obj);
}

_shadow.check_obj_arg = check_obj_arg;

// ------------------------------------------------------------

})()    // Object_init
