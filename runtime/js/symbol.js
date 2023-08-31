
// ------------------------------------------------------------
//
// Symbol
//
// ------------------------------------------------------------

;(function Symbol_init () {

const js_sym_util = _shadow.js_sym_util;

const Object = _global.Object;

//
// Symbol constructor
//

function Symbol (description) {
    // the tostring conversion will throw an error,
    // if description itself is a Symbol, otherwise
    // will return a symbol with a string description
    return js_sym_util('' + description);
}

_shadow.js_flag_as_not_constructor(Symbol);

//
// Symbol prototype
//
// note that the prototype object was already created
// in js_str_init_2 () in file str.c. it is attached to
// symbol values by js_get_primitive_proto () in obj.c.
//

const Symbol_prototype = Symbol('').__proto__;

defineProperty(Symbol, 'prototype', {
    value: Symbol_prototype });

defineProperty(_global, 'Symbol', { configurable: true,
    value: Symbol, writable: true, /* not enumerable */ });

_shadow.Symbol = Symbol; // keep a copy

//
// Symbol.for ()
//

function _for (key) {
    // XXX manage using a set
    return js_sym_util(key); // shared symbol
}

Object.defineProperty(_for, 'name', { value: 'for' });

Object.defineProperty(Symbol, 'for', { configurable: true,
    value: _for, writable: true, /* not enumerable */ });

//
// keyFor ()
//

function keyFor (sym) {
    /// xxx if description is symbol, throw error
    // if description is not string, convert to string
    const text_and_type = js_sym_util(sym);
    // XXX only if shared
    return text_and_type;
}

Object.defineProperty(Symbol, 'keyFor', { configurable: true,
    value: keyFor, writable: true, /* not enumerable */ });

//
// Well-known symbols
//

Object.defineProperty(Symbol, 'asyncIterator', {
    value: Symbol('Symbol.asyncIterator') });

Object.defineProperty(Symbol, 'hasInstance', {
    value: Symbol('Symbol.hasInstance') });

Object.defineProperty(Symbol, 'isConcatSpreadable', {
    value: Symbol('Symbol.isConcatSpreadable') });

Object.defineProperty(Symbol, 'iterator', {
    value: Symbol('Symbol.iterator') });

Object.defineProperty(Symbol, 'match', {
    value: Symbol('Symbol.match') });

Object.defineProperty(Symbol, 'matchAll', {
    value: Symbol('Symbol.matchAll') });

Object.defineProperty(Symbol, 'replace', {
    value: Symbol('Symbol.replace') });

Object.defineProperty(Symbol, 'search', {
    value: Symbol('Symbol.search') });

Object.defineProperty(Symbol, 'toPrimitive', {
    value: Symbol('Symbol.toPrimitive') });

Object.defineProperty(Symbol, 'toStringTag', {
    value: Symbol('Symbol.toStringTag') });

Object.defineProperty(Symbol, 'unscopables', {
    value: Symbol('Symbol.unscopables') });

//
// Symbol.prototype
//

Object.defineProperty(Symbol_prototype, 'constructor', {
    value: Symbol, writable: true, configurable: true });

Object.defineProperty(Symbol_prototype, 'toString', {
    value: Symbol_toString, writable: true, configurable: true });

Object.defineProperty(Symbol_prototype, 'valueOf', {
    value: Symbol, writable: true, configurable: true });

Object.defineProperty(Symbol_prototype, 'description', {
    get() { return js_sym_util(undefined, this); },
    configurable: true });

defineConfig(Symbol_prototype, Symbol.toStringTag, 'Symbol');

Object.defineProperty(Symbol_prototype, Symbol.toPrimitive, {
    value: null, configurable: true });

//
// Symbol.prototype.toString
//

function Symbol_toString () {
    if (typeof(this) !== 'symbol')
        TypeError_incompatible_object('symbol');
    return 'Symbol(' + js_sym_util(this) + ')';
}

overrideFunctionName(Symbol_toString, 'toString');

_shadow.Symbol_toString = Symbol_toString;

// ------------------------------------------------------------

})()    // Symbol_init
