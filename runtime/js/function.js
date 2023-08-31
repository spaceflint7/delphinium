
// ------------------------------------------------------------
//
// Function
//
// ------------------------------------------------------------

;(function Function_init () {

// the js_hasinstance () function from func.c
const js_hasinstance = _shadow.js_hasinstance;

//
// Function constructor
//

function Function (text) {
    _shadow.TypeError_unsupported_operation();
}

//
// Function prototype
//
// note that the prototype object was already created
// in js_func_init () in file func.c. it is attached
// to new function objects in js_newfunc ().
//

defineNotEnum(_global, 'Function', Function);
_shadow.Function = Function; // keep a copy

const Function_prototype = Function.__proto__;
defineNotEnum(Function_prototype, 'constructor', Function);

defineProperty(Function, 'prototype',
        { value: Function_prototype, writable: false });

//
// Function.prototype.arguments
// Function.prototype.caller
//
// these throw a TypeError.  the function object for a strict mode
// function does not include 'arguments' or 'caller' properties,
// so accessing those properties ends up throwing the error.
//

const deprecated_throw = function () {
    throw TypeError('Invalid use of \'arguments\', \'caller\' or \'callee\' property');
}

const deprecated_throw_descr = {
    configurable: true, /* not enumerable */
    get: deprecated_throw, set: deprecated_throw
}

defineProperty(Function_prototype, 'arguments', deprecated_throw_descr);
defineProperty(Function_prototype, 'caller', deprecated_throw_descr);

//
// Function.prototype.apply
//

defineNotEnum(Function_prototype, 'apply',
function apply (thisArg, argsArray) {

    if (argsArray === null || argsArray === undefined)
        return this.call(thisArg);

    if (!_shadow.isArray(argsArray)) {
        argsArray = _shadow.create_array_like_iterator(argsArray);
        console.log(argsArray);
    }

    return this.call(thisArg, ...argsArray);
});

//
// Function.prototype.bind
// Function.prototype.call
//

defineNotEnum(Function_prototype, 'bind', _shadow.js_proto_bind);
defineNotEnum(Function_prototype, 'call', _shadow.js_proto_call);

//
// Function.prototype.toString
//

defineNotEnum(Function_prototype, 'toString',
function toString () {
    if (typeof(this) !== 'function')
        _shadow.TypeError_expected_function();
    return 'function ' + this.name + '() { [native code] }';
});

//
// Function.prototype.hasInstance
//
// implements the internal OrdinaryHasInstance operation.
// see js_hasinstance defined in func.c
//

defineProperty(Function_prototype, Symbol.hasInstance,
    { value: _shadow.js_hasinstance });

//
// js_arguments ()
//
// called by js_arguments () in stack.c, to convert
// an array of parameters into the arguments object.
//

const arguments_symbol = Symbol('arguments_symbol');
_shadow.arguments_symbol = arguments_symbol;

const array_iterator = ([])[Symbol.iterator];

_shadow.js_arguments = function (input_array) {

    const arguments_object = {};

    // attach a hidden object, we don't use it, but
    // is used by Object_getSpecialTag() to identify
    const private_state = js_private_object(
                arguments_object, arguments_symbol, true);

    const arguments_length = input_array.length;
    for (let i = 0; i < arguments_length; i++)
        arguments_object[i] = input_array[i];

    defineNotEnum(arguments_object, 'length', arguments_length);

    if (input_array.callee) {
        // in non-strict mode, define 'callee' as a reference
        // to the most recently called function.  this ref is
        // passed in the 'callee' property of 'input_array',
        // see also js_arguments2 () in stack.c
        defineNotEnum(arguments_object, 'callee', input_array.callee);
    } else {
        // in strict mode, define 'callee' to throw an error,
        // like Function.prototype.caller, but non-configurable
        defineProperty(arguments_object, 'callee', {
            get: deprecated_throw, set: deprecated_throw });
    }

    // array-like iterator for argument properties
    defineNotEnum(arguments_object, Symbol.iterator, array_iterator);

    return arguments_object;
}

// ------------------------------------------------------------

})()    // Function_init
