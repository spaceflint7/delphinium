
// ------------------------------------------------------------
//
// this module splits the input program into an array of
// function node tree, extracting all nested functions as
// elements in this top-level array.
//
// the input should be a top-level FunctionDeclaration node.
// the output is an array of separate function nodes.  there
// are no nested functions in the array, but there are links
// between function nodes via 'decl_node' properties;  see
// process_stmt_or_expr () and process_function_node ().
//
// each top-level function node gets several new properties:
// unique_id - generated using get_unique_id ()
// is_func_node - true
// child_funcs - array of function nodes originally defined
//      within this function, but already moved out of it
// parent_func - function node of the containing function
// strict_mode - to compile function in javascript strict mode
// decl_node   - links to the node which initially contained
//               the function before it was split off.
//
// and a utility function:  func_node.visit(filter, callback),
// with a filter that is a type string or an array of such,
// or omitted altogether.  see also visit_helper ()
//
// the original node, which initially contained the function
// definition, has its 'body' and 'params' properties reset,
// and a new 'decl_node' property links to the new function.
//
// additionally, most processed nodes get these properties:
// parent_node - the containing node in the hierarchy of nodes
//
// ------------------------------------------------------------

const utils = require('./utils');

// ------------------------------------------------------------

function process_stmt_or_expr (
                        node, parent_func_stmt, func_list) {

    if (node.type === 'ClassDeclaration'
    ||  node.type === 'FunctionDeclaration'
    ||  node.type === 'ClassExpression'
    ||  node.type === 'FunctionExpression'
    ||  node.type === 'ArrowFunctionExpression') {

        determine_function_name(node);

        process_function_node(
                    node, parent_func_stmt, func_list);

        for (const sub_node of utils.get_child_nodes(node))
            sub_node.parent_node = node;

    } else {

        if (node.type === 'MemberExpression'
        ||  node.type === 'CallExpression') {

            node.strict_mode = parent_func_stmt.strict_mode;
        }

        for (const sub_node of utils.get_child_nodes(node)) {

            sub_node.parent_node = node;

            process_stmt_or_expr(
                    sub_node, parent_func_stmt, func_list);
        }
    }
}

// ------------------------------------------------------------

function determine_function_name (func_node) {

    let id_node;

    do {

        // if function declaration specifies a name
        if (func_node.id?.name) {
            id_node = func_node.id;
            break;
        }

        let parent_node = func_node.parent_node;

        // if anonymous function in variable declaration
        if (parent_node.type === 'VariableDeclarator') {
            let var_id = parent_node.id;
            if (var_id?.name) {
                id_node = var_id;
                break;
            }
        }

        // if anonymous function is declared in assignment
        if (parent_node.type === 'AssignmentExpression') {
            let op = parent_node.operator;
            if (op === '='
            ||  op === '&&='
            ||  op === '||='
            ||  op === '??=') {

                let lhs_id = parent_node.left;
                if (lhs_id?.name) {
                    id_node = lhs_id;
                    break;
                }
            }
        }

    } while (false);
    if (id_node) {

        id_node.is_function_name = true;
        if (id_node !== func_node.id) {
            func_node.id = {
                type: 'Identifier',
                name: id_node.name,
                parent_node: func_node
            };
        }
    }
}

// ------------------------------------------------------------

function process_function_node (
            def_func_node, parent_func_stmt, func_list) {

    const old_node = def_func_node;
    const old_id   = old_node.id;

    //
    // clone the node declaring the function
    //

    const new_node = Object.assign({}, old_node);

    new_node.strict_mode = parent_func_stmt.strict_mode || false;
    new_node.parent_node = undefined;

    // clone the identifier sub-node, if one is present.
    // this new identifier node should translate to
    // 'func_obj' (see identifier_expression () in
    // expression_writer.js), while the old identifier
    // node should translate like any local variable.

    if (old_id) {

        new_node.id = {

            type: old_id.type,
            name: old_id.name,
            loc: Object.assign({}, old_id.loc),
            parent_node: new_node,
        };
    }

    // connect to the body node from the old node

    if (new_node.body) {

        new_node.body.parent_node = new_node;
    }

    // parse the node tree that now starts with the
    // new function node

    new_node.parent_func = parent_func_stmt;
    parent_func_stmt.child_funcs.push(new_node);

    process_function_tree(new_node, func_list);

    //
    // modify the old function node to detach the
    // node sub-tree that makes up the function
    //

    old_node.params = undefined;
    old_node.body = undefined;
    old_node.superClass = undefined;

    old_node.decl_node = new_node;
    new_node.decl_node = old_node;

    // if the old node was a function declaration, then
    // the declaration should be hoisted to the top of
    // the block where it was defined, similar to 'var'

    if (old_node.type === 'FunctionDeclaration') {

        const block = old_node.parent_node;
        if (block.type === 'BlockStatement') {

            const index_in_parent =
                        block.body.indexOf(old_node);
            block.body.splice(index_in_parent, 1);
            block.body.unshift(old_node);
        }
    }
}

// ------------------------------------------------------------

function process_function_tree (func_stmt, func_list) {

    func_stmt.unique_id = utils.get_unique_id();
    func_stmt.child_funcs = [];
    func_stmt.is_func_node = true;

    func_stmt.visit = visit_helper;

    // make sure the function contains a single BlockStatement
    let block_stmt = func_stmt.body;
    if (block_stmt?.type === 'BlockStatement'
    && Array.isArray(block_stmt?.body)) {

        // check for and discard the 'use strict' directive
        const block_stmt_list = block_stmt.body;
        if (block_stmt_list.length > 0
        &&  block_stmt_list[0].type === 'ExpressionStatement'
        &&  block_stmt_list[0].directive === 'use strict') {

            func_stmt.strict_mode = true;
            block_stmt_list.shift();
        }

    } else if (func_stmt.expression) {

        // convert a function expression to a BlockStatement
        // with a single ReturnStatement with the expression

        let return_stmt;
        block_stmt = {
                type: 'BlockStatement',
                body: [ return_stmt = {
                        type: 'ReturnStatement',
                        argument: func_stmt.body,
                        loc: func_stmt.body.loc,
                } ],
        };

        func_stmt.body = block_stmt;

    } else
        throw [ func_stmt, 'invalid function node' ];

    // connect child nodes to the parent function
    block_stmt.parent_node = func_stmt;
    for (const param_node of func_stmt.params)
        param_node.parent_node = func_stmt;

    // recursively process the BlockStatement
    process_stmt_or_expr(block_stmt, func_stmt, func_list);
    func_list.push(func_stmt);
}

// ------------------------------------------------------------

function visit_helper (filter, callback) {

    // visit each node in the tree

    const f = (node) => {

        const child_nodes = utils.get_child_nodes(node);

        if ((typeof(filter) === 'string' && filter === node.type)
        ||  (Array.isArray(filter) && filter.includes(node.type))
        ||  !filter) {

            callback(node);
        }

        child_nodes.forEach(f);
    }

    f(this);
}

// ------------------------------------------------------------

module.exports = function split_into_functions (top_func_node) {

    const list_of_all_functions = [];
    process_function_tree(top_func_node, list_of_all_functions);

    return list_of_all_functions;
}
