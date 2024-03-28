
// ------------------------------------------------------------
//
// Number
//
// ------------------------------------------------------------

;(function Number_init () {

const js_num_util = _shadow.js_num_util;
const trunc = _shadow.math.trunc;

// ------------------------------------------------------------
//
// Number constructor
//
// ------------------------------------------------------------

function Number (num) {
    num = js_num_util(num, 0x43 /* C */); // Constructor
    if (new.target)
        num = _shadow.create_object_wrapper(num);
    return num;
}

_shadow.js_flag_as_constructor(Number);

// ------------------------------------------------------------
//
// Number prototype
//
// note that the prototype object was already created
// in js_num_init () in file num.c. it is attached to
// number values by js_get_primitive_proto () in obj.c.
//
// ------------------------------------------------------------

defineNotEnum(_global, 'Number', Number);
_shadow.Number = Number; // keep a copy

const Number_prototype = (0).__proto__;
defineNotEnum(Number_prototype, 'constructor', Number);
defineProperty(Number, 'prototype', { value: Number_prototype });

// ------------------------------------------------------------
//
// Number.prototype.toExponential
// Number.prototype.toFixed
// Number.prototype.toPrecision
// Number.prototype.toString
// Number.prototype.toLocaleString
//
// ------------------------------------------------------------

defineNotEnum(Number_prototype, 'toExponential',
function toExponential (fractionDigits) {
    return js_num_util(this, 0x45 /* E */, fractionDigits);
});

defineNotEnum(Number_prototype, 'toFixed',
function toFixed (fractionDigits) {
    return js_num_util(this, 0x46 /* F */, fractionDigits);
});

defineNotEnum(Number_prototype, 'toPrecision',
function toPrecision (precision) {
    return js_num_util(this, 0x50 /* P */, precision);
});

defineNotEnum(Number_prototype, 'toString',
function toString (radix) {
    return js_num_util(this, 0x53 /* S */, radix);
});

defineNotEnum(Number_prototype, 'toLocaleString',
function toLocaleString () { return this.toString(); });

// ------------------------------------------------------------
//
// Number.prototype.valueOf
//
// ------------------------------------------------------------

defineNotEnum(Number_prototype, 'valueOf',
function valueOf () {
    if (typeof(this) === 'number')
        return this;
    if (this === Number_prototype)
        return 0;
    return _shadow.unwrap_object_wrapper(this, 'number');
});

// ------------------------------------------------------------
//
// Number.isFinite
//
// ------------------------------------------------------------

let _isFinite;

defineNotEnum(Number, 'isFinite',
_isFinite = function isFinite (num) {
    if (typeof(num) !== 'number')
        return false;
    if (num !== num)
        return false;
    if (num === Infinity)
        return false;
    if (num === -Infinity)
        return false;
    return true;
});

// ------------------------------------------------------------
//
// Number.isInteger
//
// ------------------------------------------------------------

defineNotEnum(Number, 'isInteger',
function isInteger (num) {
    return (_isFinite(num) && trunc(num) === num);
});

// ------------------------------------------------------------
//
// Number.isNaN
//
// ------------------------------------------------------------

defineNotEnum(Number, 'isNaN', function isNaN (num) {
    return (num !== num);
});

// ------------------------------------------------------------
//
// Number.isSafeInteger
//
// ------------------------------------------------------------

defineNotEnum(Number, 'isSafeInteger',
function isSafeInteger (num) {
    if (_isFinite(num)) {
        if (num >= -9007199254740991 &&
            num <= 9007199254740991)
        {
            return (trunc(num) === num);
        }
    }
    return false;
});

// ------------------------------------------------------------
//
// Number.parseFloat
// Number.parseInt
//
// ------------------------------------------------------------

function parseFloat (val) {
    return js_num_util(val, 0x52 /* R */); // paRse
}

function parseInt (val, radix) {
    return js_num_util(val, 0x52 /* R */, radix); // paRse
}

defineNotEnum(Number,  'parseFloat', parseFloat);
defineNotEnum(Number,  'parseInt',   parseInt);

defineNotEnum(_global, 'parseFloat', parseFloat);
defineNotEnum(_global, 'parseInt',   parseInt);

// ------------------------------------------------------------
//
// global.isFinite
// global.isNaN
//
// ------------------------------------------------------------

defineNotEnum(_global, 'isFinite',
function isFinite (val) { return (isFinite(+val)); });

defineNotEnum(_global, 'isNaN',
function isNaN (val) {
    val = +val;
    return (val !== val);
});

// ------------------------------------------------------------
//
// constants
//
// ------------------------------------------------------------

defineProperty(Number, 'MAX_VALUE',         { value: 1.7976931348623157e308 });
defineProperty(Number, 'MIN_VALUE',         { value: 5e-324 });
defineProperty(Number, 'NaN',               { value: NaN });
defineProperty(Number, 'NEGATIVE_INFINITY', { value: -Infinity });
defineProperty(Number, 'POSITIVE_INFINITY', { value:  Infinity });
defineProperty(Number, 'MAX_SAFE_INTEGER',  { value: 9007199254740991 });
defineProperty(Number, 'MIN_SAFE_INTEGER',  { value: -9007199254740991 });
defineProperty(Number, 'EPSILON',           { value: 2.220446049250313e-16 });

})()    // Number_init
