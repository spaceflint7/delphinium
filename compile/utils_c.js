
// ------------------------------------------------------------

const utils = require('./utils');

// ------------------------------------------------------------

exports.init_expr_undefined = {

    type: 'Identifier',
    name: 'undefined',
    decl_node: { c_name: 'js_undefined' },
};

exports.env_global_decl_node = {

    type: 'Identifier',
    c_name: 'env->global_obj',
    as_expr: '(env->global_obj)',
};

// ------------------------------------------------------------

exports.clean_name = (id_node) =>

    id_node.name.replace(/[^a-zA-Z0-9_]/g, '_');

// ------------------------------------------------------------

exports.get_variable_c_name = function (node) {

    if (node.c_name)
        return node.c_name;

    if (node.unique_id && node.id?.name) {
        const clean_name = exports.clean_name(node.id);
        const prefix = node.is_parameter ? 'arg' : 'local';
        node.c_name = `${prefix}_${clean_name}_${node.unique_id}`;
        return node.c_name;
    }

    throw [ node, 'bad node for variable' ];
}

// ------------------------------------------------------------

exports.alloc_temp_value = function (node) {

    const block_node = utils.get_parent_block_node(node);
    let next_temp_val = 'val_' + utils.get_unique_id();
    block_node.temp_vals.push(next_temp_val);
    return next_temp_val;
}

// ------------------------------------------------------------

exports.alloc_temp_stack = function (node) {

    const block_node = utils.get_parent_block_node(node);
    let next_temp_stk = 'stk_' + utils.get_unique_id();
    block_node.temp_stks.push(next_temp_stk);
    return next_temp_stk;
}

// ------------------------------------------------------------

exports.insert_init_text = function (node, text) {

    const block_node = utils.get_parent_block_node(node);
    block_node.init_text.push(text);
}

// ------------------------------------------------------------

exports.is_global_lookup = function (node) {

    return node.type === 'MemberExpression'
        && node.object.type === 'Identifier'
        && node.object.decl_node === exports.env_global_decl_node;
}

// ------------------------------------------------------------

exports.is_global_lookup_non_strict = function (text, node) {

    return (text === exports.env_global_decl_node.as_expr
                 && !node.strict_mode);
}

// ------------------------------------------------------------

exports.literal_property_name = function (node) {

    // returns property name, or false
    return (    node.property.type === 'Literal'
             && node.property.c_name
             && node.property.value);
}

// ------------------------------------------------------------

exports.is_negated_literal = function (node) {

    if (node.type === 'Literal') {

        const parent_node = node.parent_node;
        return (parent_node.type === 'UnaryExpression'
            &&  parent_node.operator === '-');

    } else if (node.type === 'UnaryExpression') {

        return (node.operator === '-'
             && node.argument.type === 'Literal');
    }
}

// ------------------------------------------------------------

exports.make_function_where = function (node) {

    const node0 = node;
    while (node) {

        if (node.type === 'FunctionDeclaration' &&
                    typeof(node.filename) === 'string') {

            return `"${node.filename}:${node0.loc.start.line}"`;
        }

        node = node.parent_func;
    }

    throw [ node0, 'cannot find top-level node' ];
}

// ------------------------------------------------------------

exports.get_with_local2 = function (expr, prop) {

    // the unqualified identifier may matche a local variable
    // that was declared outside the 'with' block, but in such
    // a case, we still need to first do lookup on the 'with'
    // target.  if the lookup does not find the property, then
    // we use the outer local variable.

    let var2;
    const with_node = expr.with_decl_node;
    if (!with_node)
        var2 = 'NULL';

    else if (expr.closure_index >= 0) {
        const closure_expression =
            require('./function_writer').closure_expression;
        var2 = closure_expression(expr.closure_index, expr);

    } else if (with_node.type === 'VariableDeclarator') {

        var2 = exports.get_variable_c_name(with_node);
        if (!with_node.is_closure)
            var2 = '(js_val*)&' + var2;

    } else
        throw [ expr, 'unexpected variable reference in with' ];

    return prop + ',' + var2;
}

// ------------------------------------------------------------

exports.text_callback = function (node, callback) {

    if (!node.text_callbacks)
        node.text_callbacks = [];
    node.text_callbacks.push(callback);
}
