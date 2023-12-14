
const utils = require('./utils');
const utils_c = require('./utils_c');

// ------------------------------------------------------------

function get_value_or_c_name (node) {

    let cvar;
    if (typeof(node.value) === 'string') {
        if (node.value === '')
            cvar = 'str_empty';
        if (well_known_strings.indexOf(node.value) !== -1)
            cvar = 'str_' + node.value;
    } else if (node.value === 0n)
        cvar = 'big_zero';
    if (!cvar) {
        let value = node.value;
        if (utils_c.is_negated_literal(node))
            value = -value;
        return value;
    }
    node.c_name = `env->${cvar}`;
}

// ------------------------------------------------------------

function global_to_object_lookup (node, func_node) {

    // convert any Identifier nodes, flagged as lookups
    // on the global namespace, to MemberExpression nodes.

    // to preserve the links between nodes, the input node
    // is converted to type MemberExpression, and two new
    // Identifier nodes are created for object and property

    node.object = {
        type: 'Identifier',
        name: 'global',
        loc: node.loc,
        parent_node: node,
        scope: node.scope,
        decl_node: utils_c.env_global_decl_node
    };

    node.property = {
        type: 'Identifier',
        name: node.name,
        loc: node.loc,
        parent_node: node,
        scope: node.scope,
        is_property_name: true,
        c_name: node.c_name,
    };

    node.type = 'MemberExpression';
    node.name = undefined;
    node.c_name = undefined;
    node.computed = false;
    node.is_property_name = false;
    node.is_global_lookup = false;

    node.strict_mode = func_node.strict_mode;
}

// ------------------------------------------------------------

module.exports = class process_literals {

map = new Map();
shapes = new Map();
indexers = new Set();
globals;

// ------------------------------------------------------------

process_function (func_node) {

    if (!func_node.visit)
        throw [ func_node, 'expected function node' ];

    this.globals = [];

    func_node.visit([ 'Identifier', 'Literal' ], node => {

        let literal, indexer;

        if (node.type === 'Identifier') {
            if (node.is_property_name || node.is_function_name)
                literal = node.name;
            if (node.is_global_lookup)
                this.globals.push(node);
            indexer = true;

        } else if (node.type === 'Literal') {
            if (    typeof(node.value) !== 'number'
                 && typeof(node.value) !== 'boolean')
                literal = get_value_or_c_name(node);
            // check if literal is used in object['literal']
            indexer = (node.parent_node.type === 'MemberExpression'
                    && node.parent_node.property === node);
            indexer ||= (node.parent_node.type === 'Property');
        }

        if (literal === '__proto__') {
            const prop = node.parent_node;
            if (prop.type === 'Property' &&
                prop.parent_node.type === 'ObjectExpression' && !(
                    prop.computed || prop.method || prop.shorthand)) {
                // '__proto__:' (colon syntax, quoted or not)
                // specifies the internal prototype property
                node.is_proto_property = true;
                prop.is_proto_property = true;
                literal = undefined;
            }
        }

        if (literal) {

            let c_name = this.map.get(literal);
            if (!c_name) {
                c_name = 'literal_' + utils.get_unique_id();
                this.map.set(literal, c_name);
            }
            node.c_name = c_name;

            if (indexer)
                this.indexers.add(c_name);
        }
    });

    // look at object expressions and constructor functions
    // to deduce object shapes in advance.  note that we do
    // this only after c_names have been set for literals.

    this.literal_value = this.map.get('value');
    this.literal_done  = this.map.get('done');

    func_node.visit('ObjectExpression',
                    this.process_object_shape.bind(this));

    //
    // apply a 'with' scope or a default shape for 'new'.
    //
    // note that the 'with_scope' and 'new_shape' fields
    // in js_func are mutually-exclusive, so the following
    // two code blocks are in an if-else relationship.
    //

    if (func_node.strict_mode) {
        // js_func->new_shape field (which is set by
        // function_expression () in function_writer.js)
        // but only for strict mode functions
        this.process_constructor_shape(func_node);
    } else {
        // for non-strict mode function, the 'with_scope'
        // field overrides the 'new_shape' field
        utils_c.text_callback(func_node,
                        this.with_scope_text_callback);
    }

    // convert Identifier nodes, which are really lookups
    // on the global namespace, to MemberExpression nodes

    this.globals.forEach(var_node =>
            global_to_object_lookup(var_node, func_node));
    this.globals = null;
}

// ------------------------------------------------------------

process_object_shape (node) {

    let props = [];
    for (const prop of node.properties) {
        if (prop.type === 'SpreadElement') {
            // a spread element breaks the shape
            break;
        }
        if (prop.type !== 'Property')
            utils.throw_unexpected_property(node);
        if (!prop.key.c_name) {
            if (prop.is_proto_property)
                continue;
            else
                break;
        }
        if (!props.includes(prop.key.c_name)) {
            // duplicate properties are allowed, but they
            // should not take up extra values in the shape
            prop.is_shape_property = true;
            props.push(prop.key.c_name);
        }
    }

    if (!props.length)
        return; // unknown shape
    node.shape = this.append_shape(props);
}

// ------------------------------------------------------------

process_constructor_shape (func_node) {

    let props = [];

    func_node.visit('MemberExpression', (node) => {

        if (node.object.type === 'ThisExpression'
        &&  node.property.c_name
        &&  !node.property.is_proto_property) {

            if (!props.includes(node.property.c_name)) {
                // duplicate properties are allowed,
                // but they should not take up extra
                // values in the shape
                props.push(node.property.c_name);
            }
        }
    });

    if (!props.length)
        return; // unknown shape
    func_node.this_shape = this.append_shape(props);
    // install a callback for function_expression ()
    utils_c.text_callback(func_node,
                this.constructor_shape_text_callback);
}

// ------------------------------------------------------------

constructor_shape_text_callback (call_node, decl_node, text) {

    // if function is a constructor and we were able
    // to calculate an object shape for 'this', then
    // store the shape in the new function object,
    // for use by js_callnew () in func.c.  see also
    // process_constructor_shape () in literals.js
    let tmp = utils_c.alloc_temp_value(call_node);
    return `(${tmp}=${text},`
         + `((js_func*)js_get_pointer(${tmp}))->u.new_shape=`
         + `(js_shape*)${decl_node.this_shape.c_name},`
         + `${tmp})`;
}

// ------------------------------------------------------------

with_scope_text_callback (call_node, decl_node, text) {

    // if non-strict mode, copy 'with' scopes to the new
    // function.  see also js_scopewith () which looks
    // for this special bit flag (js_is_flagged_pointer).
    return `js_scopewith(env,${text},`
         + '(js_val){.raw=func_val.raw|2})';
}

// ------------------------------------------------------------

append_shape (props) {

    // check if this is one of the well-known shapes
    const well_known_shape = this.is_well_known_shape(props);
    if (well_known_shape)
        return { c_name: 'env->' + well_known_shape };

    const key = props.join(',');
    let shape = this.shapes.get(key);

    if (!shape) {
        this.shapes.set(key, shape = {
            properties: props,
            c_name: 'shape_' + utils.get_unique_id(),
        });
    }

    return shape;
}

// ------------------------------------------------------------

is_well_known_shape (props) {

    if (props.length != 2)
        return;
    const p0 = props[0];
    const p1 = props[1];
    if (p0 === this.literal_value && p1 === this.literal_done)
        return 'shape_value_done';
    if (p0 === this.literal_done  && p1 === this.literal_value)
        return 'shape_done_value';
}

// ------------------------------------------------------------

write_initialization (output) {

    let static_names = [];
    let func_stmts = [];
    output.push('');

    //
    // literals
    //

    for (const [value, c_name] of this.map) {

        static_names.push(',');
        static_names.push(c_name);

        switch (typeof(value)) {
            case 'string':
                this.write_string(c_name, value, output, func_stmts);
                break;
            case 'bigint':
                this.write_bigint(c_name, value, output, func_stmts);
                break;
            default:
                throw `unknown literal (2) type ${typeof(value)}`;
        }
    }

    if (static_names.length > 0) {
        static_names[0] = 'static js_val ';
        output.push(static_names.join('') + ';');
        static_names.length = 0;
    }

    //
    // shapes
    //

    for (const [_, shape] of this.shapes) {
        static_names.push(',');
        static_names.push('*' + shape.c_name);
        this.write_shape(shape, func_stmts);
    }

    if (static_names.length > 0) {
        static_names[0] = 'static const js_shape ';
        output.push(static_names.join('') + ';');
        static_names.length = 0;
    }

    //
    // combine output
    //

    output.push('');
    output.push('static void init_literals(js_environ *env){');
    for (const stmt of func_stmts)
        output.push(stmt);
    output.push('}');
}

// ------------------------------------------------------------

write_string (c_name, str_text, outfunc_output, infunc_output) {

    const num_chars = str_text.length;
    if (num_chars <= 0)
        return;

    let chars_text = '';
    for (let i = 0; i < num_chars; ++i)
        chars_text += str_text.charCodeAt(i) + ',';

    // if literal is known to be a string property,
    // indexing into an object, let js_newstr () know
    const intern_flag = !!this.indexers.has(c_name);

    const c_init = `${c_name}_init`;
    outfunc_output.push(
        `static wchar_t *${c_init}=(wchar_t[]){`
    +   /* flags and length */ `0,0,0,${chars_text}};` );

    infunc_output.push(
        `${c_name}=js_newstr(env,${intern_flag},`
    +   `${num_chars},${c_init});`);
}

// ------------------------------------------------------------

write_bigint (c_name, big_value, outfunc_output, infunc_output) {

    let cmp_value = big_value;
    let input_negative = false;
    if (cmp_value < 0) {
        cmp_value = -cmp_value;
        input_negative = true;
    }

    let words = [];
    let w = 0;
    while (cmp_value) {
        w = Number(big_value & 0xFFFFFFFFn);
        big_value >>= 32n;
        cmp_value >>= 32n;

        if (w === 0xFFFFFFFF)
            w = -1;
        else if (w > 9)
            w = '0x'+w.toString(16).toUpperCase();
        words.push(w + 'U');
    }

    // if last bit in last word does not reflect the
    // desired sign of the number, append a sign word
    let last_word_negative = !!(w & 0x80000000);
    if (last_word_negative != input_negative) {
        w = input_negative ? -1 : 0;
        words.push(w + 'U');
    }

    let num_chars = words.length;
    if (num_chars <= 0)
        return;

    let chars_text = '';
    for (let i = 0; i < num_chars; ++i)
        chars_text += words[i] + ',';

    const c_init = `${c_name}_init`;
    outfunc_output.push(
        `static uint32_t *${c_init}=(uint32_t[]){${chars_text}};`);

    infunc_output.push(
        `${c_name}=js_newbig(env,${num_chars},${c_init});`);
}

// ------------------------------------------------------------

write_shape (shape, output) {

    output.push('');
    const obj = 'obj_for_' + shape.c_name;
    output.push(`js_val ${obj}=js_newobj(env,env->shape_empty);`);
    for (const prop of shape.properties) {
        output.push(`js_setprop(env,${obj},${prop},`
                  + 'js_undefined,&env->dummy_shape_cache);');
    }
    output.push(
        `${shape.c_name}=((js_obj*)js_get_pointer(${obj}))->shape;`);
}

// ------------------------------------------------------------

} // end of class

// ------------------------------------------------------------

const well_known_strings = [    // see also runtime.h
    'arguments',
    'bigint', 'boolean',
    'callee', 'caller', 'configurable', 'constructor',
    'default', 'done',
    'enumerable',
    'false', 'function',
    'get',
    'length',
    'name', 'next', 'null', 'number',
    'object',
    'prototype',
    'return',
    'set', 'string', 'symbol',
    'throw', 'toString', 'true',
    'undefined',
    'value', 'valueOf',
    'writable'
];
