
// ------------------------------------------------------------
//
// Number
//
// ------------------------------------------------------------

;(function Number_init () {

// ------------------------------------------------------------
//
// Number constructor
//
// ------------------------------------------------------------

function Number (val) {
    return js_num_util(val, 0x43 /* C */); // Constructor
}

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

defineProperty(Number, 'prototype',
      { value: Number_prototype, writable: false });

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
    return 'UNKNOWN VALUE OF NUMBER';
});

// ------------------------------------------------------------
//
// Number.isFinite
//
// ------------------------------------------------------------

defineNotEnum(Number, 'isFinite', function isFinite () {
});

// ------------------------------------------------------------
//
// Number.isInteger
//
// ------------------------------------------------------------

defineNotEnum(Number, 'isInteger', function isInteger () {
});

// ------------------------------------------------------------
//
// Number.isNaN
//
// ------------------------------------------------------------

defineNotEnum(Number, 'isNaN', function isNaN () {
});

// ------------------------------------------------------------
//
// Number.isSafeInteger
//
// ------------------------------------------------------------

defineNotEnum(Number, 'isSafeInteger', function isSafeInteger () {
});

// ------------------------------------------------------------
//
// Number.parseFloat
//
// ------------------------------------------------------------

defineNotEnum(Number, 'parseFloat', function parseFloat () {
});

// ------------------------------------------------------------
//
// Number.parseInt
//
// ------------------------------------------------------------

defineNotEnum(Number, 'parseInt', function parseInt () {
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
