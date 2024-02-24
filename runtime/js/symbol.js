
// ------------------------------------------------------------
//
// Symbol
//
// ------------------------------------------------------------

;(function Symbol_init () {

const js_sym_util = _shadow.js_sym_util;

// ------------------------------------------------------------
//
// Symbol constructor
//
// ------------------------------------------------------------

function Symbol (description) {
    // the tostring conversion will throw an error,
    // if description itself is a Symbol, otherwise
    // will return a symbol with a string description
    return js_sym_util('' + description);
}

// ------------------------------------------------------------
//
// Symbol prototype
//
// note that the prototype object was already created
// in js_str_init_2 () in file str.c. it is attached to
// symbol values by js_get_primitive_proto () in obj.c.
//
// ------------------------------------------------------------

defineNotEnum(_global, 'Symbol', Symbol);
_shadow.Symbol = _Symbol = Symbol; // keep a copy

const Symbol_prototype = Symbol('').__proto__;
defineProperty(Symbol, 'prototype', { value: Symbol_prototype });

// ------------------------------------------------------------
//
// Symbol registry:
//
// Symbol.for ()
// Symbol.keyFor ()
//
// ------------------------------------------------------------

let sym_reg_map; // Map () is not available yet

function sym_reg_for (key) {
    if (!sym_reg_map)
        sym_reg_map = new Map();
    key = '' + key;
    let sym = sym_reg_map.get(key);
    if (typeof(sym) !== 'symbol') {
        sym = js_sym_util(key); // new symbol
        sym_reg_map.set(key, sym);
        sym_reg_map.set(sym, key);
    }
    return sym;
}

const sym_reg_key = (sym) => sym_reg_map?.get(sym);

overrideFunctionName(sym_reg_for, 'for');
overrideFunctionName(sym_reg_key, 'keyFor');

defineNotEnum(Symbol, 'for', sym_reg_for);
defineNotEnum(Symbol, 'keyFor', sym_reg_key);

//
// Well-known symbols
//

const define_well_known_symbol = (x) =>
    defineProperty(Symbol, x,
            { value: Symbol('Symbol.' + x) });

define_well_known_symbol('asyncIterator');
define_well_known_symbol('hasInstance');
define_well_known_symbol('isConcatSpreadable');
define_well_known_symbol('iterator');
define_well_known_symbol('match');
define_well_known_symbol('matchAll');
define_well_known_symbol('replace');
define_well_known_symbol('search');
define_well_known_symbol('species');
define_well_known_symbol('split');
define_well_known_symbol('toPrimitive');
define_well_known_symbol('toStringTag');
define_well_known_symbol('unscopables');

//
// Symbol.prototype
//

defineNotEnum(Symbol_prototype, 'constructor', Symbol);
defineNotEnum(Symbol_prototype, 'toString', toString);
defineNotEnum(Symbol_prototype, 'valueOf', valueOf);
defineProperty(Symbol_prototype, 'description',
                { get: get_descr, configurable: true });
defineConfig(Symbol_prototype, Symbol.toStringTag, 'Symbol');
defineConfig(Symbol_prototype, Symbol.toPrimitive, toPrimitive);

//
// Symbol.prototype functions
//

function ensure_symbol (val) {
    if (typeof(val) !== 'symbol')
        _shadow.TypeError_incompatible_object('symbol');
    return val;
}

function toString () {
    return 'Symbol(' + js_sym_util(ensure_symbol(this)) + ')';
}

_shadow.Symbol_toString = toString;

function get_descr () { return js_sym_util(ensure_symbol(this)); }
overrideFunctionName(get_descr, 'get description');

function valueOf () { return ensure_symbol(this); }
function toPrimitive () { return ensure_symbol(this); }
overrideFunctionName(toPrimitive, 'Symbol.toPrimitive');

// ------------------------------------------------------------

})()    // Symbol_init
