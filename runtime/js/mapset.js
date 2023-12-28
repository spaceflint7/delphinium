
// ------------------------------------------------------------
//
// Map and Set
//
// ------------------------------------------------------------

;(function MapSet_init () {

// the js_map_util () function from map.c
const js_map_util = _shadow.js_map_util;

// ------------------------------------------------------------
//
// Map constructor
//
// ------------------------------------------------------------

const map_symbol = Symbol('map');

function Map (iterable) {

    if (!new.target)
        _shadow.TypeError_constructor_requires_new('Map');
    const map = js_map_util(0x43 /* C */, iterable);
    js_private_object(this, map_symbol, map);
}

defineProperty(Map, 'length', { value: 0 });

defineNotEnum(_global, 'Map', Map);
_shadow.Map = Map; // keep a copy

// ------------------------------------------------------------
//
// Map.species
//
// ------------------------------------------------------------

// note, not same function object as Set[Symbol.species]
const Map_species = (function () { return this });
overrideFunctionName(Map_species, 'get [Symbol.species]');
defineProperty(Map, Symbol.species, {
                get: Map_species, enumerable: false });

// ------------------------------------------------------------
//
// Map prototype
//
// ------------------------------------------------------------

const Map_prototype = Map.prototype;
defineNotEnum(Map_prototype, 'get',     map_get);
defineNotEnum(Map_prototype, 'set',     map_set);
defineNotEnum(Map_prototype, 'has',     map_has);
defineNotEnum(Map_prototype, 'delete',  map_del);
defineNotEnum(Map_prototype, 'clear',   map_clr);
defineNotEnum(Map_prototype, 'entries', map_entries);
defineNotEnum(Map_prototype, 'forEach', map_foreach);
defineNotEnum(Map_prototype, 'keys',    map_keys);
defineProperty(Map_prototype,'size',    { get: map_siz, configurable: true });
defineNotEnum(Map_prototype, 'values',  map_values);
defineConfig(Map_prototype, Symbol.toStringTag, 'Map');
defineNotEnum(Map_prototype, Symbol.iterator, map_entries);

// ------------------------------------------------------------
//
// Map iterator
//
// ------------------------------------------------------------

const map_iterator_symbol = Symbol('map_iterator');
const map_iterator_prototype = { __proto__: _shadow.this_iterator };
defineNotEnum(map_iterator_prototype, 'next', function next () {
    return map_or_set_iterator_next(
            js_private_object(this, map_iterator_symbol));
});
defineConfig(map_iterator_prototype, Symbol.toStringTag, 'Map Iterator');

function map_entries () {
    const map = js_private_object(this, map_symbol);
    const iterator_object =
                create_map_or_set_iterator(
                    map_iterator_symbol, map);
    iterator_object.__proto__ = map_iterator_prototype;
    return iterator_object;
}

function map_keys () {
    const iterator_object = map_entries.call(this);
    const ctx = js_private_object(
                    iterator_object, map_iterator_symbol);
    ctx[4] = 1; // value is just key
    return iterator_object;
}

function map_values () {
    const iterator_object = map_entries.call(this);
    const ctx = js_private_object(
                    iterator_object, map_iterator_symbol);
    ctx[4] = 2; // value is just value
    return iterator_object;
}

function map_foreach (callbackFn, thisArg) {
    const iterator_object = map_entries.call(this);
    const ctx = js_private_object(
                    iterator_object, map_iterator_symbol);
    ctx[4] = 0; // don't return value
    for (;;) {
        map_or_set_iterator_next(ctx);
        if (ctx[0] < 0)
            break;
        callbackFn.call(thisArg, ctx[2], ctx[1], this);
    }
}

overrideFunctionName(map_entries, 'entries');
overrideFunctionName(map_keys,    'keys');
overrideFunctionName(map_values,  'values');
overrideFunctionName(map_foreach, 'foreach');

// ------------------------------------------------------------
//
// Map non-iterator functions
//
// ------------------------------------------------------------

function map_get (key) {
    const map = js_private_object(this, map_symbol);
    return js_map_util(0x47 /* G */, map, key);
}

function map_set (key, val) {
    const map = js_private_object(this, map_symbol);
    js_map_util(0x53 /* S */, map, key, val);
    return this;
}

function map_has (key) {
    const map = js_private_object(this, map_symbol);
    return js_map_util(0x46 /* H */, map, key);
}

function map_del (key) {
    const map = js_private_object(this, map_symbol);
    return js_map_util(0x52 /* R */, map, key);
}

function map_clr () {
    const map = js_private_object(this, map_symbol);
    js_map_util(0x45 /* E */, map);
}

function map_siz () {
    const map = js_private_object(this, map_symbol);
    return js_map_util(0x4E /* N */, map);
}

overrideFunctionName(map_get, 'get');
overrideFunctionName(map_set, 'set');
overrideFunctionName(map_has, 'has');
overrideFunctionName(map_del, 'delete');
overrideFunctionName(map_clr, 'clear');
overrideFunctionName(map_siz, 'get size');

// ------------------------------------------------------------
//
// Set constructor
//
// ------------------------------------------------------------

const set_symbol = Symbol('set');

function Set (iterable) {

    if (!new.target)
        _shadow.TypeError_constructor_requires_new('Set');
    const set = js_map_util(0x43 /* C */, iterable);
    js_private_object(this, set_symbol, set);
}

defineProperty(Set, 'length', { value: 0 });

defineNotEnum(_global, 'Set', Set);
_shadow.Set = Set; // keep a copy

// ------------------------------------------------------------
//
// Set.species
//
// ------------------------------------------------------------

// note, not same function object as Map[Symbol.species]
const Set_species = (function () { return this });
overrideFunctionName(Set_species, 'get [Symbol.species]');
defineProperty(Set, Symbol.species, {
                get: Set_species, enumerable: false });

// ------------------------------------------------------------
//
// Set prototype
//
// ------------------------------------------------------------

const Set_prototype = Set.prototype;
defineNotEnum(Set_prototype, 'has',     set_has);
defineNotEnum(Set_prototype, 'add',     set_add);
defineNotEnum(Set_prototype, 'delete',  set_del);
defineNotEnum(Set_prototype, 'clear',   set_clr);
defineNotEnum(Set_prototype, 'entries', set_entries);
defineNotEnum(Set_prototype, 'forEach', set_foreach);
defineProperty(Set_prototype,'size',    { get: set_siz, configurable: true });
defineNotEnum(Set_prototype, 'values',  set_values);
defineNotEnum(Set_prototype, 'keys',    set_keys);
defineConfig(Set_prototype, Symbol.toStringTag, 'Set');
defineNotEnum(Set_prototype, Symbol.iterator, set_values);

// ------------------------------------------------------------
//
// Set iterator
//
// ------------------------------------------------------------

const set_iterator_symbol = Symbol('set_iterator');
const set_iterator_prototype = { __proto__: _shadow.this_iterator };
defineNotEnum(set_iterator_prototype, 'next', function next () {
    return map_or_set_iterator_next(
            js_private_object(this, set_iterator_symbol));
});
defineConfig(set_iterator_prototype, Symbol.toStringTag, 'Set Iterator');

function set_entries () {
    const map = js_private_object(this, set_symbol);
    const iterator_object =
                create_map_or_set_iterator(
                    set_iterator_symbol, map);
    iterator_object.__proto__ = set_iterator_prototype;
    return iterator_object;
}

function set_keys () {
    const iterator_object = set_entries.call(this);
    const ctx = js_private_object(
                    iterator_object, set_iterator_symbol);
    ctx[4] = 1; // value is just key
    return iterator_object;
}

function set_values () {
    const iterator_object = set_entries.call(this);
    const ctx = js_private_object(
                    iterator_object, set_iterator_symbol);
    ctx[4] = 1; // value is just value
    return iterator_object;
}

function set_foreach (callbackFn, thisArg) {
    const iterator_object = set_entries.call(this);
    const ctx = js_private_object(
                    iterator_object, set_iterator_symbol);
    ctx[4] = 0; // don't return value
    for (;;) {
        map_or_set_iterator_next(ctx);
        if (ctx[0] < 0)
            break;
        callbackFn.call(thisArg, ctx[2], ctx[1], this);
    }
}

overrideFunctionName(set_entries, 'entries');
overrideFunctionName(set_keys,    'keys');
overrideFunctionName(set_values,  'values');
overrideFunctionName(set_foreach, 'foreach');

// ------------------------------------------------------------
//
// Map non-iterator functions
//
// ------------------------------------------------------------

function set_has (key) {
    const map = js_private_object(this, set_symbol);
    return js_map_util(0x46 /* H */, map, key);
}

function set_add (key) {
    const map = js_private_object(this, set_symbol);
    js_map_util(0x53 /* S */, map, key, key);
    return this;
}

function set_del (key) {
    const map = js_private_object(this, set_symbol);
    return js_map_util(0x52 /* R */, map, key);
}

function set_clr () {
    const map = js_private_object(this, set_symbol);
    js_map_util(0x45 /* E */, map);
}

function set_siz () {
    const map = js_private_object(this, set_symbol);
    return js_map_util(0x4E /* N */, map);
}

overrideFunctionName(set_has, 'has');
overrideFunctionName(set_add, 'add');
overrideFunctionName(set_del, 'delete');
overrideFunctionName(set_clr, 'clear');
overrideFunctionName(set_siz, 'get size');

// ------------------------------------------------------------
//
// Iterator
//
// ------------------------------------------------------------

function create_map_or_set_iterator (
                        type_symbol, map_or_set_obj) {

    const iterator_object = {};
    const ctx = js_private_object(
                    iterator_object, type_symbol, []);
    // ctx[0] is internal iteration index,
    // which is set to -1 when iteration is done
    //
    // after each iteration:
    // ctx[1] receives the iteration key
    // ctx[2] receives the iteration value
    //
    // ctx[3] is a reference to the map object
    // ctx[4] determines the result value:
    //       <= 0 to do nothing
    //          1 for just key
    //          2 for just value
    //          3 for [ key, value ]
    ctx[0] = 0;
    ctx[3] = map_or_set_obj;
    ctx[4] = 3; // default to [ key, value ]
    return iterator_object;
}

function map_or_set_iterator_next (ctx) {

    if (ctx[0] >= 0) {
        if (!js_map_util(0x49 /* I */, ctx[3], ctx))
            ctx[0] = -1;
    }

    // ctx[4] determines the result value,
    // see create_map_or_set_iterator ()
    const ctx_4 = ctx[4];
    if (ctx_4 <= 0)
        return;

    let value, done;
    if (ctx[0] < 0)
        done = true;
    else {
        done = false;
        if (ctx_4 === 1)
            value = ctx[1];
        else if (ctx_4 === 2)
            value = ctx[2];
        else if (ctx_4 === 3)
            value = [ ctx[1], ctx[2] ];
    }
    return { value, done };
}

// ------------------------------------------------------------

})()    // MapSet_init
