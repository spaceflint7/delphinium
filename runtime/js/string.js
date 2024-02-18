
// ------------------------------------------------------------
//
// String
//
// ------------------------------------------------------------

;(function String_init () {

const Object = _global.Object;
const js_str_utf16 = _shadow.js_str_utf16;

//
// String constructor
//

function String (...text) {

    if (text.length) {
        text = text[0];
        if (typeof(text) === 'symbol' && !new.target)
            text = _shadow.Symbol_toString.call(key);
        text = '' + text;
    } else
        text = '';

    if (new.target)
        text = _shadow.create_object_wrapper({}, text);

    return text;
}

defineProperty(String, 'length', { value: 1 });

//
// String prototype
//
// note that the prototype object was already created
// in js_str_init () in file str.c. it is attached to
// string values by js_get_primitive_proto () in obj.c.
//

defineNotEnum(_global, 'String', String);
_shadow.String = String; // keep a copy

const String_prototype = ('').__proto__;
defineNotEnum(String_prototype, 'constructor', String);

defineProperty(String, 'prototype',
    { value: String_prototype, writable: false });

//
// String.fromCharCode
//

defineNotEnum(String, 'fromCharCode', {
fromCharCode (...codeUnits) {
    const n = codeUnits.length;
    if (n === 1)
        return js_str_utf16(+codeUnits[0]);
    for (let i = 0; i < n; i++)
        codeUnits[i] = +codeUnits[i];
    return js_str_utf16(codeUnits);

}}.fromCharCode);
defineProperty(String.fromCharCode, 'length', { value: 1 });

//
// String functions
//

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

//
// charAt
//

defineNotEnum(String_prototype, 'charAt', function charAt (pos) {

    const str = check_str_arg(this);
    if ((pos = +pos) !== pos)
        pos = 0; // NaN --> 0
    if (pos >= 0 && pos < str.length)
        return str[pos];
    return '';
});

//
// charCodeAt
//

defineNotEnum(String_prototype, 'charCodeAt', function charCodeAt (pos) {

    const str = check_str_arg(this);
    if ((pos = +pos) !== pos)
        pos = 0; // NaN --> 0
    if (pos >= 0 && pos < str.length)
        return js_str_utf16(str[pos]);
    return NaN;
});

//defineNotEnum(String_prototype, 'codePointAt',
//defineNotEnum(String_prototype, 'concat',
//defineNotEnum(String_prototype, 'endsWith',
//defineNotEnum(String_prototype, 'includes',
//defineNotEnum(String_prototype, 'indexOf',
//defineNotEnum(String_prototype, 'lastIndexOf',
//defineNotEnum(String_prototype, 'localeCompare',
//defineNotEnum(String_prototype, 'match',
//defineNotEnum(String_prototype, 'matchAll',
//defineNotEnum(String_prototype, 'normalize',
//defineNotEnum(String_prototype, 'padEnd',
//defineNotEnum(String_prototype, 'padStart',
//defineNotEnum(String_prototype, 'repeat',
//defineNotEnum(String_prototype, 'replace',
//defineNotEnum(String_prototype, 'replaceAll',
//defineNotEnum(String_prototype, 'search',
//defineNotEnum(String_prototype, 'slice',
//defineNotEnum(String_prototype, 'split',
//defineNotEnum(String_prototype, 'substr',
//defineNotEnum(String_prototype, 'substring',

//defineNotEnum(String_prototype, 'startsWith',
//defineNotEnum(String_prototype, 'toString',
//defineNotEnum(String_prototype, 'trim',
//defineNotEnum(String_prototype, 'trimStart',
//defineNotEnum(String_prototype, 'trimLeft',
//defineNotEnum(String_prototype, 'trimEnd',
//defineNotEnum(String_prototype, 'trimRight',
//defineNotEnum(String_prototype, 'toLocaleLowerCase',
//defineNotEnum(String_prototype, 'toLocaleUpperCase',
//defineNotEnum(String_prototype, 'toLowerCase',
//defineNotEnum(String_prototype, 'toUpperCase',

// ------------------------------------------------------------
//
// String.prototype.valueOf
//
// ------------------------------------------------------------

defineNotEnum(String_prototype, 'valueOf',
function valueOf () {
    return _shadow.unwrap_object_wrapper(this, 'string');
});

//defineNotEnum(String_prototype, 'at',

//defineNotEnum(String_prototype, 'isWellFormed', ??
//defineNotEnum(String_prototype, 'toWellFormed', ??

defineNotEnum(String_prototype, _Symbol.iterator, iterator);

//
// [Symbol.iterator] ()
//

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
    return private_state.next();
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
