
;(function Error_init () {

// ------------------------------------------------------------
//
// Error
//
// ------------------------------------------------------------

function Error (message) {

    return create_error_object(
                this, message, new.target ?? Error);
}

defineNotEnum(_global, 'Error', Error);
_shadow.Error = Error; // keep a copy

// ------------------------------------------------------------
//
// Error.prototype
//
// ------------------------------------------------------------

const Error_prototype = Error.prototype;
defineProperty(Error, 'prototype',
      { value: Error_prototype, writable: false });

defineNotEnum(Error_prototype, 'name', 'Error');

defineNotEnum(Error_prototype, 'message', '');

// ------------------------------------------------------------
//
// Error.prototype.toString
//
// ------------------------------------------------------------

defineNotEnum(Error_prototype, 'toString', function toString () {

    if (typeof(this) !== 'object')
        _shadow.TypeError_expected_object();

    let name = this.name;
    if (name === undefined)
        name = 'Error';
    else
        name = '' + name;

    let msg = this.message;
    if (msg === undefined)
        msg = '';
    else
        msg = '' + msg;

    return (name === '' ? msg
          : (msg === '' ? name : (name + ': ' + msg)));
});

// ------------------------------------------------------------
//
// RangeError
//
// ------------------------------------------------------------

function RangeError (message) {

    return create_error_object(
                this, message, new.target ?? RangeError);
}

const RangeError_prototype = RangeError.prototype;
defineProperty(RangeError, 'prototype',
      { value: RangeError_prototype, writable: false });

RangeError_prototype.__proto__ = Error_prototype;
defineNotEnum(RangeError_prototype, 'name', 'RangeError');

defineNotEnum(_global, 'RangeError', RangeError);
_shadow.RangeError = RangeError; // keep a copy

// ------------------------------------------------------------
//
// ReferenceError
//
// ------------------------------------------------------------

function ReferenceError (message) {

    return create_error_object(
                this, message, new.target ?? ReferenceError);
}

const ReferenceError_prototype = ReferenceError.prototype;
defineProperty(ReferenceError, 'prototype',
      { value: ReferenceError_prototype, writable: false });

ReferenceError_prototype.__proto__ = Error_prototype;
defineNotEnum(ReferenceError_prototype, 'name', 'ReferenceError');

defineNotEnum(_global, 'ReferenceError', ReferenceError);
_shadow.ReferenceError = ReferenceError; // keep a copy

// ------------------------------------------------------------
//
// TypeError
//
// ------------------------------------------------------------

function TypeError (message) {

    return create_error_object(
                this, message, new.target ?? TypeError);
}

const TypeError_prototype = TypeError.prototype;
defineProperty(TypeError, 'prototype',
      { value: TypeError_prototype, writable: false });

TypeError_prototype.__proto__ = Error_prototype;
defineNotEnum(TypeError_prototype, 'name', 'TypeError');

defineNotEnum(_global, 'TypeError', TypeError);
_shadow.TypeError = TypeError; // keep a copy

// ------------------------------------------------------------
//
// SyntaxError
//
// ------------------------------------------------------------

function SyntaxError (message) {

    return create_error_object(
                this, message, new.target ?? SyntaxError);
}

const SyntaxError_prototype = SyntaxError.prototype;
defineProperty(SyntaxError, 'prototype',
      { value: SyntaxError_prototype, writable: false });

SyntaxError_prototype.__proto__ = Error_prototype;
defineNotEnum(SyntaxError_prototype, 'name', 'SyntaxError');

defineNotEnum(_global, 'SyntaxError', SyntaxError);
_shadow.SyntaxError = SyntaxError; // keep a copy

// ------------------------------------------------------------
//
// error utilities
//
// ------------------------------------------------------------

_shadow.TypeError_defineProperty_descriptor = function throw_TypeError () {

    throw TypeError('Property descriptor must be an object');
}

_shadow.TypeError_defineProperty_3 = function throw_TypeError () {

    throw TypeError('Cannot redefine property');
}

_shadow.TypeError_defineProperty_4 = function throw_TypeError () {

    throw TypeError('Property descriptor must specify either data or accessor');
}

_shadow.TypeError_defineProperty_5 = function throw_TypeError () {

    throw TypeError('Property descriptor must specify function in accessor');
}

_shadow.TypeError_cyclicPrototype = function throw_TypeError () {

    throw TypeError('Cyclic prototype value');
}

_shadow.TypeError_readOnlyProperty = function throw_TypeError (prop) {

    prop = prop ? (': ' + prop) : '';
    throw TypeError('Cannot set read-only property' + prop);
}

_shadow.TypeError_primitiveProperty = function throw_TypeError () {

    throw TypeError('Cannot set or delete properties on a primitive value');
}

_shadow.TypeError_deleteProperty = function throw_TypeError (prop) {

    prop = prop ? (': ' + prop) : '';
    throw TypeError('Cannot delete a non-configurable property' + prop);
}

_shadow.TypeError_convert_null_to_object = function throw_TypeError () {

    throw TypeError('Cannot convert undefined or null to object');
}

_shadow.TypeError_get_property_of_null_object = function throw_TypeError (prop) {

    prop = prop ? (': ' + prop) : '';
    throw TypeError('Cannot get property of undefined or null');
}

_shadow.TypeError_set_property_of_null_object = function throw_TypeError (prop) {

    prop = prop ? (': ' + prop) : '';
    throw TypeError('Cannot set property of undefined or null');
}

_shadow.TypeError_object_not_extensible = function throw_TypeError () {

    throw TypeError('Cannot add property to non-extensible object');
}

_shadow.TypeError_expected_symbol = function throw_TypeError () {

    throw TypeError('Type mismatch, expected a symbol');
}

_shadow.TypeError_expected_number = function throw_TypeError () {

    throw TypeError('Type mismatch, expected a number');
}

_shadow.TypeError_expected_integer = function throw_TypeError () {

    throw TypeError('Type mismatch, expected an integer number');
}

_shadow.TypeError_expected_bigint = function throw_TypeError () {

    throw TypeError('Type mismatch, expected a BigInt');
}

_shadow.TypeError_expected_object = function throw_TypeError () {

    throw TypeError('Type mismatch, expected an object');
}

_shadow.TypeError_expected_function = function throw_TypeError (funcName) {

    if (funcName)
        funcName = ': ' + funcName;
    else
        funcName = '';
    throw TypeError('Type mismatch, expected a function' + funcName);
}

_shadow.TypeError_expected_constructor = function throw_TypeError (funcName) {

    if (funcName)
        funcName = ': ' + funcName;
    else
        funcName = '';
    throw TypeError('Type mismatch, expected a constructor function' + funcName);
}

_shadow.TypeError_invalid_prototype = function throw_TypeError () {

    throw TypeError('Prototype must be object or null');
}

_shadow.TypeError_convert_object_to_primitive = function throw_TypeError () {

    throw TypeError('Cannot convert object to primitive value');
}

_shadow.TypeError_convert_symbol_to_string = function throw_TypeError () {

    throw TypeError('Cannot convert symbol to string value');
}

_shadow.TypeError_invalid_argument = function throw_TypeError () {

    throw TypeError('Invalid argument');
}

_shadow.TypeError_iterator_result = function throw_TypeError () {

    throw TypeError('Iterator result is not an object');
}

_shadow.TypeError_not_iterable = function throw_TypeError () {

    throw TypeError('Type mismatch, expected iterable object');
}

_shadow.TypeError_incompatible_object = function throw_TypeError (expectedType) {

    if (expectedType)
        expectedType = ', expected ' + expectedType;
    else
        expectedType = '';
    throw TypeError('Incompatible object' + expectedType);
}

_shadow.TypeError_mix_types_in_arithmetic = function throw_TypeError () {

    throw TypeError('Cannot mix incompatible types in arithmetic');
}

_shadow.TypeError_unsupported_operation = function throw_TypeError () {

    throw TypeError('Operation not supported');
}

_shadow.ReferenceError_uninitialized_variable = function throw_ReferenceError () {

    throw ReferenceError('Cannot access uninitialized variable');
}

_shadow.ReferenceError_not_defined = function throw_ReferenceError (prop) {

    prop = prop ? (': ' + prop) : '';
    throw ReferenceError('Property or variable is not defined' + prop);
}

_shadow.RangeError_array_length = function throw_RangeError () {

    throw ReferenceError('Invalid array length');
}

_shadow.RangeError_invalid_argument = function throw_RangeError () {

    throw RangeError('Invalid argument');
}

_shadow.RangeError_division_by_zero = function throw_RangeError () {

    throw RangeError('Division by zero');
}

_shadow.RangeError_bigint_too_large = function throw_RangeError () {

    throw RangeError('BigInt value is too large');
}

_shadow.SyntaxError_invalid_argument = function throw_SyntaxError () {

    throw SyntaxError('Invalid argument');
}

// ------------------------------------------------------------
//
// create_error_object
//
// ------------------------------------------------------------

function create_error_object (err_obj, message, constructor) {

    // we are supposed to be called by an Error function,
    // which may be called via 'new' keyword and 'this'
    // is  already created.  or it may be called directly,
    // and we have to allocate 'this' here
    if (!err_obj)
        err_obj = { __proto__: constructor.prototype };

    // get a stack trace which starts at our caller
    let stack = '\n' + _shadow.get_stack_trace_as_string(3);

    if (message !== undefined) {
        message = '' + message;
        defineNotEnum(err_obj, 'message', message);
        stack = constructor.name + ': ' + message + stack;
    } else
        stack = constructor.name + stack;
    defineNotEnum(err_obj, 'stack', stack);

    return err_obj;
}

// ------------------------------------------------------------
//
// stack_trace_to_string
//
// ------------------------------------------------------------

_shadow.get_stack_trace_as_string = function get_stack_trace_as_string (i) {

    // stack trace is an array with two elements per
    // stack frame, the first is the function object,
    // the second is the source location.  parameter
    // 'i' specifies how many initial frames to skip

    let arr = _shadow.js_stack_trace();
    let str = '';
    for (i *= 2; i < arr.length; i += 2) {
        let name = arr[i].name;
        if (typeof(name) !== 'string' || !name.length) {
            if (i + 2 >= arr.length)
                name = '<top-level>';
            else
                name = '<anonymous>';
        }
        let where = arr[i + 1];
        if (typeof(where) === 'string')
            where = ' (declared at ' + where + ')';
        else
            where = '';
        if (str)
            str += '\n';
        str += '    at ' + name + where;
    }
    return str;
}

overrideFunctionName(_shadow.js_stack_trace, 'js_stack_trace');

// ------------------------------------------------------------
//
// get_exception_as_string
//
// ------------------------------------------------------------

_shadow.get_exception_as_string = function (exc) {

    // called by report_unhandled_exception () in init.c
    // to convert the thrown object to a printable string

    let str;

    try {

        str = '' + exc;

        if (typeof(exc) === 'symbol')
            str = _shadow.Symbol_toString.call(exc);

        else if ((exc instanceof Error)
               && typeof(exc.stack) === 'string')
            str = exc.stack; // Error object with stack trace

        else
            str = '' + exc;

    } catch (exc2) {

        str = 'An exception ocurred while processing the unhandled exception';

        if (typeof(exc2) === 'object') {
            const func = exc2.constructor;
            if (typeof(func) === 'function') {
                const name = func.name;
                if (typeof(name) === 'string')
                    str += ': ' + name;
            }

        } else if (typeof(exc2) !== 'symbol')
            str += ': ' + exc2;
    }

    return str;
}

// ------------------------------------------------------------

})()    // Error_init
