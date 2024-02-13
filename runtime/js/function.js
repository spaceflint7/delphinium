
// ------------------------------------------------------------
//
// Function
//
// ------------------------------------------------------------

;(function Function_init () {

// the js_hasinstance () function from func.c
const js_hasinstance = _shadow.js_hasinstance;
// the js_coroutine () function from coroutine.c
const js_coroutine = _shadow.js_coroutine;

//
// Function constructor
//

function Function (text) {
    // incomplete implementation, see also ECMA 20.2.1.1
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

defineProperty(Function_prototype, _Symbol.hasInstance,
    { value: _shadow.js_hasinstance });

//
// js_arguments ()
//
// called by js_arguments () in stack.c, to convert
// an array of parameters into the arguments object.
//

const arguments_symbol = _Symbol('arguments_symbol');
_shadow.arguments_symbol = arguments_symbol;

const array_iterator = ([])[_Symbol.iterator];

_shadow.js_arguments = function js_arguments (input_array) {

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
    defineNotEnum(arguments_object, _Symbol.iterator, array_iterator);

    return arguments_object;
}

//
// CoroutineFunction ()
//

_shadow.CoroutineFunction = function CoroutineFunction (kind) {

    // this function is called by js_newcoroutine ()
    // in coroutine.c, as result of code injected by
    // convert_to_coroutine () in function_writer.js.

    if (kind === 2)
        kind = Generator;
    else
        _shadow.TypeError_unsupported_operation();

    const wrapped = this;
    const wrapper_proto = new kind;
    const wrapper = function () {

        const ctx = js_coroutine(
                        0x49 /* I */, wrapped, this);
        const gen = { __proto__: wrapper_proto };
        js_private_object(gen, kind.context_symbol, ctx);
        return gen;
    }

    // copy 'length' and 'name' from the function being wrapped
    defineProperty(wrapper, 'length', { value: wrapped.length });
    defineProperty(wrapper, 'name', { value: wrapped.name });
    defineProperty(wrapper, 'prototype', { value: wrapper_proto });
    js_getOrSetPrototype(wrapper, kind.wrapper_prototype);

    return wrapper;
}

//
// GeneratorFunction
//

function GeneratorFunction (unused) {
    // incomplete implementation, see also ECMA 20.2.1.1
    _shadow.TypeError_unsupported_operation();
}

js_getOrSetPrototype(GeneratorFunction, Function);

var GeneratorFunction_prototype = {
    __proto__: Function.prototype };
defineConfig(GeneratorFunction_prototype, 'prototype',
                Generator.prototype);
defineConfig(GeneratorFunction_prototype, 'constructor',
                GeneratorFunction);
defineConfig(GeneratorFunction_prototype, _Symbol.toStringTag,
                'GeneratorFunction');

defineProperty(GeneratorFunction, 'prototype',
                {   value: GeneratorFunction_prototype,
                    writable: false });

_shadow.GeneratorFunction = GeneratorFunction;

//
// Generator
//

function Generator () { }

const generator_context_symbol = _Symbol('generator_context_symbol');
Generator.context_symbol = generator_context_symbol;
Generator.wrapper_prototype = GeneratorFunction_prototype;

const Generator_prototype = Generator.prototype;

js_getOrSetPrototype(Generator_prototype, _shadow.this_iterator);

defineProperty(Generator_prototype, 'constructor',
                { value: GeneratorFunction_prototype, writable: false });

defineNotEnum(Generator_prototype, 'next',   Generator_next);
defineNotEnum(Generator_prototype, 'return', Generator_return);
defineNotEnum(Generator_prototype, 'throw',  Generator_throw);

defineConfig(Generator_prototype, _Symbol.toStringTag, 'Generator');

_shadow.Generator = Generator;

//
// Generator_next
//

function Generator_next (next_val) {

    const ctx = js_private_object(
                    this, generator_context_symbol);
    return js_coroutine(0x4E /* N */, ctx, next_val);
}

Object.defineProperty(Generator_next, 'name', { value: 'next' });

//
// Generator_return
//

function Generator_return (return_val) {

    const ctx = js_private_object(
                    this, generator_context_symbol);
    return js_coroutine(0x52 /* R */, ctx, return_val);
}

Object.defineProperty(Generator_return, 'name', { value: 'return' });

//
// Generator_throw
//

function Generator_throw (throw_val) {

    const ctx = js_private_object(
                    this, generator_context_symbol);
    return js_coroutine(0x54 /* T */, ctx, throw_val);
}

Object.defineProperty(Generator_throw, 'name', { value: 'throw' });

// ------------------------------------------------------------

})()    // Function_init
