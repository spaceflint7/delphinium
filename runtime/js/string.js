
// ------------------------------------------------------------
//
// String
//
// ------------------------------------------------------------

;(function String_init () {

const Object = _global.Object;
const js_str_utf16 = _shadow.js_str_utf16;
const js_str_trim = _shadow.js_str_trim;

// ------------------------------------------------------------
//
// String constructor
//
// ------------------------------------------------------------

function String (...text) {

    if (text.length) {
        text = text[0];
        if (typeof(text) === 'symbol' && !new.target)
            text = _shadow.Symbol_toString.call(key);
        text = '' + text;
    } else
        text = '';

    if (new.target)
        text = _shadow.create_object_wrapper(text);

    return text;
}

defineProperty(String, 'length', { value: 1 });

_shadow.js_flag_as_constructor(String);

// ------------------------------------------------------------
//
// String prototype
//
// note that the prototype object was already created
// in js_str_init () in file str.c. it is attached to
// string values by js_get_primitive_proto () in obj.c.
//
// ------------------------------------------------------------

defineNotEnum(_global, 'String', String);
_shadow.String = String; // keep a copy

const String_prototype = ('').__proto__;
defineNotEnum(String_prototype, 'constructor', String);
defineProperty(String, 'prototype', { value: String_prototype });

// ------------------------------------------------------------
//
// String.fromCharCode
//
// ------------------------------------------------------------

defineNotEnum(String, 'fromCharCode',
function fromCharCode (...codeUnits) {
    const n = codeUnits.length;
    if (n === 1)
        return js_str_utf16(+codeUnits[0]);
    for (let i = 0; i < n; i++)
        codeUnits[i] = +codeUnits[i];
    return js_str_utf16(codeUnits);

});
defineProperty(String.fromCharCode, 'length', { value: 1 });

// ------------------------------------------------------------
//
// String.fromCodePoint
//
// ------------------------------------------------------------

defineNotEnum(String, 'fromCodePoint',
function fromCodePoint (...codePoints) {
    // todo
});
defineProperty(String.fromCodePoint, 'length', { value: 1 });

// ------------------------------------------------------------
//
// String functions
//
// ------------------------------------------------------------

defineNotEnum(String_prototype, 'anchor',       notImpl);
defineNotEnum(String_prototype, 'big',          notImpl);
defineNotEnum(String_prototype, 'blink',        notImpl);
defineNotEnum(String_prototype, 'bold',         notImpl);
defineNotEnum(String_prototype, 'fixed',        notImpl);
defineNotEnum(String_prototype, 'fontcolor',    notImpl);
defineNotEnum(String_prototype, 'fontsize',     notImpl);
defineNotEnum(String_prototype, 'italics',      notImpl);
defineNotEnum(String_prototype, 'link',         notImpl);
defineNotEnum(String_prototype, 'small',        notImpl);
defineNotEnum(String_prototype, 'strike',       notImpl);
defineNotEnum(String_prototype, 'sub',          notImpl);
defineNotEnum(String_prototype, 'sup',          notImpl);

// ------------------------------------------------------------
//
// charAt
// charCodeAt
// codePointAt
//
// ------------------------------------------------------------

const _charAt = _shadow.str_sup.charAt;

defineNotEnum(String_prototype, 'charAt',
function charAt (pos) {
    return _charAt.call(this, pos);
});

defineNotEnum(String_prototype, 'charCodeAt',
function charCodeAt (pos) {
    return js_str_utf16(_charAt.call(this, pos));
});

// ------------------------------------------------------------
//
// codePointAt
//
// ------------------------------------------------------------

defineNotEnum(String_prototype, 'codePointAt',
function codePointAt (pos) {

    let ch = _charAt.call(this, pos);
    if (ch === '')
        return undefined;

    let first = js_str_utf16(ch);
    if (first < 0xD800 || first > 0xDFFF) {
        // first is neither a leading surrogate
        // nor a trailing surrogate
        return first;
    }
    if (first >= 0xDC00) {
        // first is a trailing surrogate
        return first;
    }

    ch = _charAt.call(this, pos + 1);
    if (ch === '') {
        // there are no more characters
        return first;
    }
    let second = js_str_utf16(ch);
    if (second < 0xDC00 || second > 0xDFFF) {
        // seond is not a trailing surrogate
        return first;
    }

    return (   (first  - 0xD800) * 0x400
             + (second - 0xDC00) + 0x10000);
});

// ------------------------------------------------------------

defineNotEnum(String_prototype, 'concat', _shadow.str_sup.concat);

// ------------------------------------------------------------

//defineNotEnum(String_prototype, 'endsWith',
//defineNotEnum(String_prototype, 'includes',
//defineNotEnum(String_prototype, 'indexOf',
//defineNotEnum(String_prototype, 'lastIndexOf',

// ------------------------------------------------------------

//defineNotEnum(String_prototype, 'localeCompare',
//defineNotEnum(String_prototype, 'match',
//defineNotEnum(String_prototype, 'matchAll',
//defineNotEnum(String_prototype, 'normalize',

// ------------------------------------------------------------

//defineNotEnum(String_prototype, 'padEnd',
//defineNotEnum(String_prototype, 'padStart',
//defineNotEnum(String_prototype, 'repeat',
//defineNotEnum(String_prototype, 'replace',
//defineNotEnum(String_prototype, 'replaceAll',
//defineNotEnum(String_prototype, 'search',

// ------------------------------------------------------------
//
// String.prototype.slice
// String.prototype.split
// String.prototype.substr
// String.prototype.substring
//
// ------------------------------------------------------------

defineNotEnum(String_prototype, 'slice',     _shadow.str_sup.slice);
defineNotEnum(String_prototype, 'split',     null);
defineNotEnum(String_prototype, 'substr',    _shadow.str_sup.substr);
defineNotEnum(String_prototype, 'substring', _shadow.str_sup.substring);

// ------------------------------------------------------------

//defineNotEnum(String_prototype, 'startsWith',

// ------------------------------------------------------------
//
// String.prototype.toString
//
// ------------------------------------------------------------

// same as valueOf ()
defineNotEnum(String_prototype, 'toString',
function toString () {
    return _shadow.unwrap_object_wrapper(this, 'string');
});

// ------------------------------------------------------------
//
// String.prototype.trim
// String.prototype.trimStart
// String.prototype.trimLeft
// String.prototype.trimEnd
// String.prototype.trimRight
//
// ------------------------------------------------------------

function trim      () { return js_str_trim(this,  0); }
function trimStart () { return js_str_trim(this, -1); }
function trimEnd   () { return js_str_trim(this, +1); }

defineNotEnum(String_prototype, 'trim',      trim);
defineNotEnum(String_prototype, 'trimStart', trimStart);
defineNotEnum(String_prototype, 'trimLeft',  trimStart);
defineNotEnum(String_prototype, 'trimEnd',   trimEnd);
defineNotEnum(String_prototype, 'trimRight', trimEnd);

// ------------------------------------------------------------

//defineNotEnum(String_prototype, 'toLocaleLowerCase',
//defineNotEnum(String_prototype, 'toLocaleUpperCase',
//defineNotEnum(String_prototype, 'toLowerCase',
//defineNotEnum(String_prototype, 'toUpperCase',

// ------------------------------------------------------------
//
// String.prototype.valueOf
//
// ------------------------------------------------------------

// same as toString ()
defineNotEnum(String_prototype, 'valueOf',
function valueOf () {
    return _shadow.unwrap_object_wrapper(this, 'string');
});

// ------------------------------------------------------------
//
// at
//
// ------------------------------------------------------------

defineNotEnum(String_prototype, 'at', _shadow.str_sup.at);

// ------------------------------------------------------------

//defineNotEnum(String_prototype, 'isWellFormed', ??
//defineNotEnum(String_prototype, 'toWellFormed', ??

// ------------------------------------------------------------

defineNotEnum(String_prototype, _Symbol.iterator, iterator);

// ------------------------------------------------------------
//
// [Symbol.iterator] ()
//
// ------------------------------------------------------------

const string_iterator_symbol = _Symbol('string_iterator');

const iterator_prototype = { __proto__: _shadow.this_iterator };
defineNotEnum(iterator_prototype, 'next', iterator_next);
defineConfig(iterator_prototype, _Symbol.toStringTag, 'String Iterator');

function iterator () {

    const iterator_object = _shadow.create_array_like_iterator(
                                '' + this, string_iterator_symbol, true);
    iterator_object.__proto__ = iterator_prototype;
    return iterator_object;
}

function iterator_next () {

    const private_state = js_private_object(
                                this, string_iterator_symbol);

    let   index  = private_state.index;
    const length = private_state.length;
    const source = private_state.array;

    let value;
    let done = false;

    if (typeof(index) === 'number'
    &&  typeof(length) === 'number'
    &&  index < length) {

        value = source[index++];

        if (index < length) {
            let first = js_str_utf16(value);
            if (first >= 0xD800 && first <= 0xDBFF) {
                let second = js_str_utf16(source[index]);
                if (second >= 0xDC00 && second <= 0xDFFF) {
                    index++;
                    value = js_str_utf16(
                                (first  - 0xD800) * 0x400
                              + (second - 0xDC00) + 0x10000);
                }
            }
        }

        private_state.index = index;
    } else
        done = true;
    return { value, done };
}

overrideFunctionName(iterator, _shadow.this_iterator[_Symbol.iterator].name);
overrideFunctionName(iterator_next, 'next');

// ------------------------------------------------------------
//
// check_str_arg
//
// ------------------------------------------------------------

function check_str_arg (obj) {

    if (obj === undefined || obj === null)
        _shadow.TypeError_convert_null_to_object();
    return '' + obj;
}

// ------------------------------------------------------------
//
// notImpl function
//
// ------------------------------------------------------------

function notImpl () {

    throw TypeError('unimplemented String function');
}

})()    // String_init
