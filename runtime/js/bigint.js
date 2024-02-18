
// ------------------------------------------------------------
//
// BigInt
//
// ------------------------------------------------------------

;(function BigInt_init () {

const js_big_util = _shadow.js_big_util;

// ------------------------------------------------------------
//
// BigInt constructor
//
// ------------------------------------------------------------

function BigInt (val) {
    return js_big_util(val, 0x43 /* C */); // Constructor
}

_shadow.js_flag_as_not_constructor(BigInt);

// ------------------------------------------------------------
//
// BigInt prototype
//
// note that the prototype object was already created
// in js_big_init () in file big.c. it is attached to
// number values by js_get_primitive_proto () in obj.c.
//
// ------------------------------------------------------------

defineNotEnum(_global, 'BigInt', BigInt);
_shadow.BigInt = BigInt; // keep a copy

const BigInt_prototype = (0n).__proto__;
defineNotEnum(BigInt_prototype, 'constructor', BigInt);

defineProperty(BigInt, 'prototype',
    { value: BigInt_prototype, writable: false });

// ------------------------------------------------------------
//
// BigInt.prototype.toString
// BigInt.prototype.toLocaleString
//
// ------------------------------------------------------------

defineNotEnum(BigInt_prototype, 'toString',
function toString (radix) {

    if (typeof(radix) === undefined)
        radix = 10;

    const v = _shadow.unwrap_object_wrapper(this, 'bigint');
    return js_big_util(v, 0x53 /* S */, radix);
});

defineNotEnum(BigInt_prototype, 'toLocaleString',
function toLocaleString () { return this.toString(); });

// ------------------------------------------------------------
//
// BigInt.prototype.valueOf
//
// ------------------------------------------------------------

defineNotEnum(BigInt_prototype, 'valueOf',
function valueOf () {
    return _shadow.unwrap_object_wrapper(this, 'bigint');
});

// ------------------------------------------------------------
//
// BigInt.asUintN
// BigInt.asIntN
//
// ------------------------------------------------------------

defineNotEnum(BigInt, 'asUintN', function asUintN (bits, v) {
    if (typeof(v) !== 'bigint')
        v = BigInt(v);
    return js_big_util(v, 0x55 /* U */, bits);
});

defineNotEnum(BigInt, 'asIntN', function asIntN (bits, v) {
    if (typeof(v) !== 'bigint')
        v = BigInt(v);
    return js_big_util(v, 0x49 /* I */, bits);
});

// ------------------------------------------------------------

_shadow.big_0 = 0n; // gc reference to env->big_zero

// ------------------------------------------------------------

})()    // BigInt_init
