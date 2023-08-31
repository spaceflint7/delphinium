
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
// child_nodes - array of child nodes referenced by this node
// parent_node - the containing node in the hierarchy of nodes
// parent_func - function node of the containing function
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

    } else {

        if (node.type === 'MemberExpression'
        ||  node.type === 'CallExpression') {

            node.strict_mode = parent_func_stmt.strict_mode;
        }

        for (const sub_node of node.child_nodes) {
            process_stmt_or_expr(
                sub_node, parent_func_stmt, func_list);
        }
    }
}

// ------------------------------------------------------------

function determine_function_name (func_node) {

    // if function declaration specifies a name
    if (func_node.id?.name) {
        func_node.id.is_function_name = true;
        return;
    }

    // if anonymous function is declared in assignment
    let parent_node = func_node.parent_node;
    if (parent_node.type === 'AssignmentExpression') {
        let op = parent_node.operator;
        if (op === '='
        ||  op === '&&='
        ||  op === '||='
        ||  op === '??=') {

            let lhs_id = parent_node.left.id;
            if (lhs_id?.name)
                lhs_id.is_function_name = true;
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
    new_node.child_nodes = [];

    // clone the identifier sub-node, if one is present.
    // this new identifier node should translate to
    // 'func_obj' (see identifier_expression () in
    // expression_writer.js), while the old identifier
    // node should translate like any local variable.

    if (old_id) {

        new_node.child_nodes.push(new_node.id = {

            type: old_id.type,
            name: old_id.name,
            loc: Object.assign({}, old_id.loc),
            parent_node: new_node,
            child_nodes: [],
        });
    }

    // connect to the body node from the old node

    if (new_node.body) {

        new_node.body.parent_node = new_node;
        new_node.child_nodes.push(new_node.body);
    }

    // connect the parameter nodes from the old node

    for (const param_node of old_node.params) {

        param_node.parent_node = new_node;
        new_node.child_nodes.push(param_node);
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

    old_node.child_nodes = [];
    if (old_id)
        old_node.child_nodes.push(old_id);

    old_node.decl_node = new_node;
    new_node.decl_node = old_node;

    //
    // if the old node was a function declaration, then
    // the declaration should be hoisted to the top of
    // the block where it was defined, similar to 'var'
    //

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
            block_stmt.child_nodes.shift();
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
                        child_nodes: func_stmt.child_nodes
                } ],
                parent_node: func_stmt,
                child_nodes: [ return_stmt ],
        };

        return_stmt.parent_node = block_stmt;
        return_stmt.argument.parent_node = return_stmt;

        func_stmt.body = block_stmt;
        func_stmt.child_nodes = [ block_stmt ];

    } else
        throw [ func_stmt, 'invalid function node' ];

    // recursively process the BlockStatement
    process_stmt_or_expr(block_stmt, func_stmt, func_list);
    func_list.push(func_stmt);
}

// ------------------------------------------------------------

function connect_nodes (node, parent_node) {

    // the input tree connects nodes via specific properties,
    // e.g. a 'body' property in some nodes, a 'test' property
    // in other nodes.  to allow for simple tree traversal,
    // we create generic 'parent_node' and 'child_node' links.

    // we do this here, in a first pass, before any additional
    // work which might add new properties and introduce some
    // non-parent-child links in the tree.

    const child_nodes = [];

    for (const key in node) {
        const array_or_sub_node = node[key];

        if (Array.isArray(array_or_sub_node)) {
            for (const array_elem of array_or_sub_node) {
                if (array_elem?.type) {
                    child_nodes.push(array_elem);
                    array_elem.parent_property = key;
                }
            }

        } else if (array_or_sub_node?.type) {
            child_nodes.push(array_or_sub_node);
            array_or_sub_node.parent_property = key;
        }
    }

    node.parent_node = parent_node;
    node.child_nodes = child_nodes;

    for (const sub_node of child_nodes)
        connect_nodes(sub_node, node);
}

// ------------------------------------------------------------

function visit_helper (filter, callback) {

    // visit each node in the tree

    const f = (node) => {

        if ((typeof(filter) === 'string' && filter === node.type)
        ||  (Array.isArray(filter) && filter.includes(node.type))
        ||  !filter) {

            callback(node);
        }
        node.child_nodes.forEach(f);
    }

    f(this);
}

// ------------------------------------------------------------

module.exports = function split_into_functions (top_func_node) {

    connect_nodes(top_func_node);

    const list_of_all_functions = [];
    process_function_tree(top_func_node, list_of_all_functions);

    return list_of_all_functions;
}
