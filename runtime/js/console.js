
// ------------------------------------------------------------
//
// console
//
// ------------------------------------------------------------

;(function Console_init () {

let console = {};
defineNotEnum(_global, 'console', console);

//
// console.log
//

console.log = function log () {

    const n = arguments.length;
    log1(arguments[0]);
    for (let i = 1; i < n; i++) {
        js_str_print(' ');
        log1(arguments[i]);
    }
    js_str_print('\n');
}

//
// log1
//

function log1 (arg) {

    if (typeof(arg) === 'object') {
        if (arg !== null) {
            const isArray = (js_property_flags(arg, 'length') & 2);
            const f = isArray ? log2arr : log2obj;
            return f(arg);
        }

    } else if (typeof(arg) === 'function')
        return log2func(arg);

    else if (typeof(arg) === 'symbol') {
        // we don't call key.toString() here, in case
        // Symbol.prototype.toString was overridden
        return js_str_print(_shadow.Symbol_toString.call(arg));
    }

    // we reach here for a null value (type object),
    // or if type is one of undefined, string, or bigint
    let suffix = (typeof(arg) === 'bigint') ? 'n' : '';
    return js_str_print(arg + suffix);
}

//
// log2func
//

function log2func (arg) {

    if (arg === _shadow.Function.prototype)
        return log2obj(arg);
    js_str_print('[Function');
    if (js_getOrSetPrototype(arg) === null) {
        js_str_print(' (');
        js_str_print('null prototype');
        js_str_print(')');
    }
    if (arg.name) {
        js_str_print(': ');
        js_str_print(arg.name);
    } else
        js_str_print(' (anonymous)');
    js_str_print(']');
    log3(arg, ' { ', ' }');
}

//
// log2arr
//

function log2arr (arg) {

    if (js_getOrSetPrototype(arg) === null) {
        js_str_print('[Array(' + arg.length + '): ');
        js_str_print('null prototype');
        js_str_print('] [');
    } else
        js_str_print('[');

    const n = arg.length;
    if (n) {
        js_str_print(' ');

        let empties = 0;

        if (js_property_flags(arg, 0) & 0x10)
            log1(arg[0]);
        else
            empties++;

        for (let i = 1; i < n; i++) {
            if (js_property_flags(arg, i) & 0x10) {
                if (empties !== 0) {
                    if (i > empties)
                        js_str_print(', ');
                    js_str_print('<' + empties + ' empty items>, ');
                    empties = 0;
                } else
                    js_str_print(', ');
                log1(arg[i]);
            } else
                empties++;
        }

        if (empties !== 0)
            js_str_print(', <' + empties + ' empty items>');
    }

    if (!log3(arg, ', ', ' ]'))
        js_str_print(n ? ' ]' : ']');
}

//
// log2obj
//

function log2obj (arg) {

    let pfx = '';
    const proto = js_getOrSetPrototype(arg);

    let tag = is_primitive_wrapper(arg);
    if (tag) {
        js_str_print(tag);
        log3(arg, ' { ', ' }');
        return;
    }

    tag = get_constructor_name(proto);
    if (typeof(tag) !== 'string') {

        tag = arg[Symbol.toStringTag];
        if (typeof(tag) === 'string')
            pfx = 'Object ';
        else {
            tag = _shadow.Object_getSpecialTag(arg) || '';
            if (tag === 'Arguments')
                tag = '[' + tag + ']';
        }
        tag += ' ';
    }

    js_str_print(tag + '{');
    log3(arg, ' ', ' ');
    js_str_print('}');

    //
    function is_primitive_wrapper (arg) {
        let primitive_value =
            _shadow.unwrap_object_wrapper(arg, false);
        if (primitive_value !== undefined) {
            let primitive_type_name =
                    primitive_value.constructor.name;
            if (typeof(primitive_value) === 'bigint')
                primitive_value = primitive_value + 'n';
            return '[' + primitive_type_name
                 + ': ' + primitive_value + ']';
        }
    }

    //
    function get_constructor_name (proto) {

        if (proto === null)
            return '[Object: null prototype] ';
        if ((js_property_flags(proto, 'constructor') & 0x10)
        &&  (js_property_flags(proto.constructor, 'name') & 0x10)) {
            let name = proto.constructor.name;
            if (name === 'Object')
                name = '';
            if (name)
                name += ' ';
            return name;
        }
    }

}

//
// log3
//

function log3 (arg, open_str, close_str) {

    // get all enumerable string and symbol keys
    const keys = log3_keys(arg);
    const n = keys.length;
    if (n) {

        js_str_print(open_str);
        log4(arg, keys[0]);
        for (let i = 1; i < n; i++) {
            js_str_print(', ');
            log4(arg, keys[i]);
        }
        js_str_print(close_str);
    }

    return n;
}

//
// log3_keys
//

function log3_keys (arg) {

    let keys = js_keys_in_object(arg);
    let nums, syms;
    let make_new_array;

    const keys_length = keys.length;
    for (let i = 0; i < keys_length; i++) {

        const key = keys[i];
        const flg = js_property_flags(arg, key);

        // property is the typical enumerable data value,
        // keyed by a string, so it remains in the array
        if ((flg & 0x012C) === 0x120)
            continue;

        // otherwise, property may or may not be kept,
        // but in any case, not in the initial array
        keys[i] = undefined;
        make_new_array = true;

        // if not enumerable, or not data value, discard it
        if ((flg & 0x120) !== 0x120)
            continue;

        if (flg & 0x0004) {
            //
            // if property is a symbol
            //
            if (!syms)
                syms = [ key ];
            else
                syms[syms.length] = key;

        } else if (flg & 0x0008) {
            //
            // if property is an array index
            //
            const num = _shadow.Number(key); // toNumber
            if (!nums)
                nums = [ num ];
            else {
                const nums_length = nums.length;
                let j;
                for (j = 0; j < nums_length; j++) {
                    if (nums[j] > num) {
                        for (let k = nums_length; k > j; k--)
                            nums[k] = nums[k - 1];
                        break;
                    }
                }
                nums[j] = num;
            }
        }
    }

    if (make_new_array) {
        let keys2 = [];
        let keys2_index = 0;

        if (nums) {
            const nums_length = nums.length;
            for (let i = 0; i < nums_length; i++)
                keys2[keys2_index++] = nums[i];
        }
        for (let i = 0; i < keys_length; i++) {
            const key = keys[i];
            if (key !== undefined)
                keys2[keys2_index++] = key;
        }
        if (syms) {
            const syms_length = syms.length;
            for (let i = 0; i < syms_length; i++)
                keys2[keys2_index++] = syms[i];
        }

        keys = keys2;
    }

    return keys;
}

//
// log4
//

function log4 (obj, key) {

    let key2;
    if (typeof(key) === 'symbol') {
        // we don't call key.toString() here, in case
        // Symbol.prototype.toString was overridden
        key2 = _shadow.Symbol_toString.call(key);
    } else {
        key2 = '' + key;
        // xxx if key starts with a non-letter,
        // then wrap key with quotes
        /*if (key2[0] == '5')
            key2 = '\'' + key2 + '\'';*/
        /*if (key2 === '__proto__')
            key2 = '[' + key2 + ']';*/
    }
    js_str_print(key2);
    js_str_print(': ');
    const val = obj[key];
    if (typeof(val) === 'string')
        log1("'" + val + "'");
    else
        log1(val);
}

//
// console.trace
//

console.trace = function trace () {

    if (arguments.length)
        console.log('Trace:', ...arguments);
    else
        console.log('Trace');
    let str = _shadow.get_stack_trace_as_string(3);
    console.log(str);
}

//
// Symbol.toStringTag
//

defineConfig(console, Symbol.toStringTag, 'console');

})()    // Console_init
