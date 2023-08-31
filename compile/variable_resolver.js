
// ------------------------------------------------------------
//
// parse local variable declarations nodes, with some syntax
// checking for duplicate or invalid declarations, and create
// links with nodes that reference local variables.
//
// the input is the result of function_splitter.js, i.e.
// an array of flat function nodes, with no nested functions.
//
// most processed nodes get these properties:
// scope - a Map object that links identifier names to their
//      first declaration node, within a particular scope.
//      note that a node may refrence the same scope object
//      as its parent node.  see process_scope_and_locals ()
//
// nodes with type: Identifier get an additional property:
// decl_node - the node declaring the local being referenced
// is_closure - true if the linked decl_node is in another
//      function, in which case, that node is also added to
//      that function's closure array.
//
// ------------------------------------------------------------

const utils = require('./utils');

// ------------------------------------------------------------

function process_parameter (node) {

    const func_stmt = node.parent_node;
    if (node.type !== 'Identifier'
    || !func_stmt.is_func_node) {

        throw [ node, 'unknown parameter node' ];
    }

    node.scope = func_stmt.scope;

    const other_node = node.scope.get(node.name);
    if (other_node && func_stmt.strict_mode) {
        // duplicate parameters in strict mode is an
        // error.  note that esprima should catch this,
        // so this code is probably never be reached.
        const wrapper_node = {
            id: node,
            loc: node.loc,
            scope: node.scope,
        };
        duplicate_identifier_error(wrapper_node, other_node);
    }

    node.unique_id = utils.get_unique_id();
    node.scope.set(node.name, node);
}

// ------------------------------------------------------------

function process_declarations (node) {

    if (node.is_func_node) {
        if (node.decl_node?.with_root_node) {
            node.with_root_node = node.decl_node.with_root_node;
            if (node.strict_mode)
                throw [ node, 'unsupported: strict mode function'
                            + ' declared in a \'with\' block' ];
        }
        return;
    }

    node.scope = node.parent_node.scope;

    if (node.parent_node.with_root_node)
        node.with_root_node = node.parent_node.with_root_node;

    if (node.type === 'FunctionDeclaration'
    ||  node.type === 'ClassDeclaration') {

        add_local_if_first_in_scope(node);

    } else if (node.type === 'VariableDeclaration') {

        const only_if_first_in_scope = (node.kind !== 'var');
        add_one_or_more_locals(node, only_if_first_in_scope);

    } else if (node.type === 'VariableDeclarator'
            && node.parent_node.kind === 'var'
            && node.with_root_node) {

        // we are inside a 'with' block, so ask literal.js
        // to stringify the property name, in case we need
        // to send it to js_setwith ().  see also
        // variable_declaration () in statement_writer.js
        node.id.is_property_name = true;

    } else if (node.type === 'BlockStatement'
            || node.type === 'ForStatement'
            || node.type === 'ForInStatement'
            || node.type === 'ForOfStatement'
            || node.type === 'SwitchStatement') {

        node.scope = new Map();

    } else if (node.type === 'MetaProperty'
            || node.type === 'ArrowFunctionExpression') {

        create_meta_property(node);

    } else if (node.type === 'CatchClause') {

        declare_catch_variable(node);

    } else if (node.type === 'WithStatement') {

        node.body.with_root_node = node;
    }
}

// ------------------------------------------------------------

function add_local_if_first_in_scope (node) {

    const name = node.id?.name;
    if (!name)
        missing_identifier_error(node);

    const other_node = node.scope.get(name);
    if (other_node)
        duplicate_identifier_error(node, other_node);

    node.unique_id = utils.get_unique_id();
    node.scope.set(name, node);
}

// ------------------------------------------------------------

function add_local_if_declared_var (node) {

    const name = node.id?.name;
    if (!name)
        missing_identifier_error(node);

    let parent_node = node;
    while (parent_node) {

        const check_node = parent_node;
        parent_node = check_node.parent_node;

        if (parent_node &&
                check_node.scope === parent_node.scope) {
            // we are traversing up the parent scope chain,
            // so don't examine nodes that share the same
            // scope as their parent;  just go to the parent
            continue;
        }

        // are we at the scope level of the entire function?
        const is_func_node = check_node.is_func_node;

        const other_node = check_node.scope.get(name);
        if (other_node) {
            // duplicate 'var' declarations are allowed,
            // but only if the previous declaration was also
            // a 'var' declaration, and it should have been
            // recorded at the scope of the entire function
            if (other_node.kind === 'var' && is_func_node) {
                // variable was already declared, we're done
                node.unique_id = other_node.unique_id;
                return;
            }

            // one special exception is that we allow a 'var'
            // declaration to override the self reference to
            // the function itself, added in process_function ()
            if (!(is_func_node && check_node === other_node))
                duplicate_identifier_error(node, other_node);
        }

        if (is_func_node) {
            // we reached the node at the top of the function,
            // we should add the new local at this scope level
            node.unique_id = utils.get_unique_id();
            check_node.scope.set(name, node);
            break;
        }
    }
}

// ------------------------------------------------------------

function add_one_or_more_locals (node, only_if_first_in_scope) {

    for (const decl of node.declarations) {

        if (decl.type === 'VariableDeclarator') {

            if (decl.id?.type === 'Identifier') {

                decl.kind = node.kind;
                decl.scope = node.scope;
                if (decl.kind === 'const')
                    decl.is_const = true;

                if (only_if_first_in_scope)
                    add_local_if_first_in_scope(decl);
                else
                    add_local_if_declared_var(decl);

                decl.id.unique_id = decl.unique_id;
                continue;
            }
        }

        throw [ node, `unexpected node ${node.type}` ];
    }
}

// ------------------------------------------------------------

function duplicate_identifier_error (node, other_node) {

    throw [ node, `identifier '${node.id.name}' has already been declared`
                + ` in line ${other_node.loc.start.line}` ];
}

// ------------------------------------------------------------

function missing_identifier_error (node) {

    // we should generally not reach here,
    // because esprima validates the syntax
    throw [ node, `expected an identifier in ${node.parent_node.type}` ];
}

// ------------------------------------------------------------

function create_meta_property (node) {

    let is_real_ref;
    if (node.type === 'ArrowFunctionExpression') {
        // a nested function may reference 'new.target'
        node = {
            meta:     { type: 'Identifier', name: 'new' },
            property: { type: 'Identifier', name: 'target' },
            child_nodes: [],
            parent_node: node,
        };
    } else
        is_real_ref = true;

    let meta_node = node.meta;
    let prop_node = node.property;

    if (meta_node.type !== 'Identifier'
    ||  prop_node.type !== 'Identifier')
        return;

    const var_name = meta_node.name + '.' + prop_node.name;

    // convert MetaProperty node to an Identifier node
    node.type = meta_node.type;
    node.name = var_name;
    node.meta = undefined;
    node.property = undefined;
    node.child_nodes.length = 0;

    // if the meta.property name is already in scope,
    // then we already did the work once, and we're done
    const func_node = utils.get_parent_func_node(node);
    let var_node = func_node.scope.get(var_name);
    if (var_node) {
        if (is_real_ref)
            var_node.is_meta_property_ref = true;
        return; // already declared this meta property
    }

    // an arrow function expression references the
    // new.target which originates in an enclosing scope
    if (func_node.type === 'ArrowFunctionExpression')
        return;

    //
    // otherwise we have to create a declaration node
    // for a local variable to hold the meta property.
    // we can overwrite the child 'meta' and 'property'
    // nodes for this purpose.
    //

    // make sure this is a well-formed function
    if (func_node.body.type !== 'BlockStatement'
    ||  !Array.isArray(func_node.body.body))
        throw [ node, 'unexpected function body' ];

    // overwrite prop_node as a VariableDeclarator node,
    // a child of meta_node, which in turn is overwritten
    // as the containing VariableDeclaration node.
    prop_node.type = 'VariableDeclarator';
    prop_node.name = undefined;
    prop_node.id = {
        type: 'Identifier',
        name: var_name,
        parent_node: prop_node,
        parent_property: 'id',
        child_nodes: [],
    };
    prop_node.init = null;
    prop_node.parent_node = meta_node;
    prop_node.parent_property = 'declarations';
    prop_node.child_nodes = [ prop_node.id ];
    prop_node.scope = func_node.scope;
    prop_node.is_meta_property = true;

    // overwrite meta_node as a VariableDeclaration node
    // which contains the newly-overwritten prop_node.
    meta_node.type = 'VariableDeclaration';
    meta_node.name = undefined;
    meta_node.declarations = [ prop_node ];
    meta_node.kind = 'const';
    meta_node.parent_node = func_node.body;
    meta_node.parent_property = 'body';
    meta_node.child_nodes = [ prop_node ];
    meta_node.scope = func_node.scope;
    meta_node.is_meta_property = true;

    if (is_real_ref)
        prop_node.is_meta_property_ref = true;

    // add the new variable declaration at the top of
    // the function body, and the topmost local scope
    func_node.body.body.unshift(meta_node);
    add_one_or_more_locals(meta_node, true);
}

// ------------------------------------------------------------

function create_arguments_object (func_node) {

    if (func_node.type === 'ArrowFunctionExpression')
        return;

    const node3 = {
        type: 'Identifier',
        name: 'arguments',
        parent_node: undefined,
        parent_property: 'id',
        child_nodes: [],
    };

    const node2 = {
        type: 'VariableDeclarator',
        id: node3,
        init: null,
        parent_node: undefined,
        parent_property: 'declarations',
        child_nodes: [ node3 ],
        scope: func_node.scope,
        is_arguments_object: true,
    };

    const node1 = {
        type: 'VariableDeclaration',
        declarations: [ node2 ],
        kind: 'let',
        parent_node: func_node.body,
        parent_property: 'body',
        child_nodes: [ node2 ],
        scope: func_node.scope,
        is_arguments_object: true,
    };

    node3.parent_node = node2;
    node2.parent_node = node1;

    // add the new variable declaration at the top of
    // the function body, and the topmost local scope.
    func_node.body.body.unshift(node1);
    add_one_or_more_locals(node1, true);

}

// ------------------------------------------------------------

function declare_catch_variable (node) {

    if (!node.param)
        return; // catch clause without catch binding

    //
    // the catch clause has a 'param' sub-node of type
    // Identifier | BindingPattern to declare the exception
    // variable, and we need to convert this to a proper
    // VariableDeclaration node, so it can be processed
    // by our normal variable resolution logic.
    //

    const decl_node = {
        type: 'VariableDeclaration', kind: 'let',
        declarations: [ {
            type: 'VariableDeclarator',
            id: node.param,
            init: null,
            parent_property: 'declarations',
            parent_node: null,
            child_nodes: [],
        } ],
        parent_property: 'param',
        parent_node: node,
        child_nodes: [],
    };

    const decl_sub_node = decl_node.declarations[0];
    decl_node.child_nodes.push(decl_sub_node);
    decl_sub_node.parent_node = decl_node;

    const decl_id_node = decl_sub_node.id;
    decl_sub_node.child_nodes.push(decl_id_node);
    decl_id_node.parent_node = decl_sub_node;

    // store the new decl_node in place of old node.param
    const node_param_index =
                    node.child_nodes.indexOf(node.param);
    node.child_nodes[node_param_index] = node.param = decl_node;
}

// ------------------------------------------------------------

function process_references (node) {

    // first, filter out some Identifier nodes that we
    // don't like:  those that describe a declaration
    // rather than a reference, and those that describe
    // literal static property names

    if (node.unique_id)
        return;

    if (node.parent_property === 'property'
    &&  node.parent_node.type === 'MemberExpression'
    && !node.parent_node.computed) {
        // don't try to resolve literal property names;
        // just let the literal.js module stringify them.
        // this is a property access expression: obj.prop
        node.is_property_name = true;
        return;
    }

    if (node.parent_property === 'key'
    &&  node.parent_node.type === 'Property'
    && !node.parent_node.computed) {
        // don't try to resolve literal property names;
        // just let the literal.js module stringify them
        // this is an object expression: { prop: value }
        node.is_property_name = true;
        return;
    }

    if (node.parent_property === 'id'
    &&  node.parent_node.type === 'FunctionExpression') {
        // don't try to resolve the name of a function
        // in a function expression;  this identifier
        // is added by process_function (), see there
        return;
    }

    const name = node.name;
    if (!name)
        missing_identifier_error(node);

    // find the identifier in the hierarchy of scope maps
    // which attached to parent nodes of the initial
    // identifier node.  when we reach the root node of a
    // function, we switch to the containing function.

    let with_decl_node;
    let is_closure = false;
    let parent_node = node;
    while (parent_node) {

        const check_node = parent_node;
        parent_node = check_node.parent_node;

        if (parent_node) {
            if (check_node === node.with_root_node) {
                // 'with' scoping rules dictate that properties
                // on the 'with' target take precedence over
                // locals declared outside the 'with' block.
                // therefore, once we hit the 'with' statement
                // while traversing up, then any local variable
                // we resolve, must be outside the 'with' block.
                with_decl_node = true;
            }
            if (check_node.scope === parent_node.scope) {
                // we are traversing up the parent scope chain,
                // so don't examine nodes that share the same
                // scope as their parent;  just go to the parent
                continue;
            }
        }

        const other_node = check_node.scope.get(name);
        if (other_node) {

            if (other_node.type === 'VariableDeclarator'
            &&  other_node.kind !== 'var') {

                check_uninitialized_reference(node, other_node);
            }

            if (other_node.is_arguments_object)
                other_node.is_arguments_object_ref = true;

            node.decl_node = other_node;
            if (other_node.is_const)
                node.is_const = true;
            if (is_closure)
                add_closure_variable(node);

            if (!with_decl_node)
                return;

            // if we hit the 'with' statement while traversing
            // upwards (see above check for 'with_root_node')
            // then we want to convert the identifier lookup
            // to global property lookup.  but keep the local
            // variable as a secondary fallback.  see also
            // member_expression_generic () in property_writer.js

            node.with_decl_node = node.decl_node;
            node.decl_node = undefined;
            break;
        }

        // keep scanning in the containing function,
        // for possible reference to a closure variable
        if (!parent_node && check_node.is_func_node
                         && check_node !== node.parent_node) {
            parent_node = check_node.decl_node;
            is_closure = true;
            if (parent_node?.with_root_node)
                with_decl_node = true;
        }
    }

    // don't treat this as a lookup node, if it is a decl node
    if (node.parent_node?.type.indexOf('Declaration') !== -1)
        return;

    if (name === 'undefined') {
        // the global variable 'undefined' is non-modifiable,
        // so if there is no local variable 'undefined', then
        // we can avoid a lookup and use a literal value.
        node.type = 'Literal';
        return;
    }

    if (name === 'NaN') {
        // similar to 'undefined'
        node.type = 'Literal';
        node.value = NaN;
        return;
    }

    // convert to member access on the global object
    node.is_property_name = true;
    node.is_global_lookup = true;
}

// ------------------------------------------------------------

function check_uninitialized_reference (ref_node, decl_node) {

    // cannot reference a variable as part of
    // the initialization expression of itself
    let error = utils.is_descendant_of(ref_node, decl_node);

    if (!error) {

        const block = utils.get_parent_block_node(ref_node);
        if (block === utils.get_parent_block_node(decl_node)
        &&  utils.get_distance_from_block(ref_node, block)
          < utils.get_distance_from_block(decl_node, block)) {

            // reference to a variable before its declaration
            error = true;
        }
    }

    if (error) {
        throw [ ref_node,
                'variable referenced before initialization' ];
    }
}

// ------------------------------------------------------------

function add_closure_variable (node) {

    let decl_node = node.decl_node;
    node.is_closure = true;
    decl_node.is_closure = true;

    let func_node = utils.get_parent_func_node(node);
    if (!func_node.closures)
        func_node.closures = new Map();

    let old_ref_node = func_node.closures.get(decl_node);
    if (old_ref_node) {
        // if the function already has a previous reference
        // to the same closure variable, then just copy the
        // information from that other identifier node
        node.closure_index = old_ref_node.closure_index;
        node.closure_func  = old_ref_node.closure_func;
    } else {
        // otherwise we add the variable to set of closure
        // variables, give it a closure index, and identify
        // the function where it was originally declared
        node.closure_index = func_node.closures.size;
        node.closure_func =
                utils.get_parent_func_node(decl_node);
        func_node.closures.set(decl_node, node);
    }
}

// ------------------------------------------------------------

function add_closure_function (inner_func, outer_func) {

    if (inner_func === outer_func)
        return;
    const inner_closures = inner_func.decl_node.closures;
    if (!inner_closures)
        return;

    // for each closure variable referenced in the nested
    // function 'inner_func', check if the variable is
    // declared outside the containing function 'outer_func'
    // which also happens to be the function currently being
    // processed by our caller, process_function ().

    for (let [decl_node, ref_node] of inner_closures) {

        if (ref_node.closure_func !== outer_func) {

            // this variable is external to the current
            // function, so it becomes a closure variable
            // in the current function

            const dummy_identifier_node = {
                decl_node: decl_node,
                parent_node: outer_func,
            };
            add_closure_variable(dummy_identifier_node);
        }
    }
}

// ------------------------------------------------------------

function process_function (func_stmt) {

    func_stmt.scope = new Map();

    // insert the name of the function into its own scope.
    // note that we only do this with a function or class
    // expression; in the case of a declaration statement,
    // the identifier is a variable in the parent scope.

    if (func_stmt?.id?.name && (
            func_stmt.type === 'FunctionExpression'
         || func_stmt.type === 'ClassExpression')) {

        if (func_stmt.strict_mode) {
            // in strict mode, the identifier is const
            func_stmt.is_const = true;
        }
        func_stmt.scope.set(func_stmt.id.name, func_stmt);
    }

    // insert the reference to 'arguments' array at the
    // point where it overrides the function id, but can
    // itself be overridden by parameter names
    create_arguments_object(func_stmt);

    // insert each parameter into the scope
    if (func_stmt.params?.length)
        func_stmt.params.forEach(process_parameter);

    // handle variable declarations
    func_stmt.visit(undefined, process_declarations);

    // resolve variable references
    func_stmt.visit('Identifier', process_references);

    // next, recursively process each child/nested function
    func_stmt.child_funcs.forEach(process_function);

    // check if a nested function references variables
    // that are declared outside its containing function
    func_stmt.visit([
        'FunctionDeclaration', 'FunctionExpression',
        'ClassDeclaration',  'ClassExpression',
        'ArrowFunctionExpression' ],
        node => add_closure_function(node, func_stmt));
}

// ------------------------------------------------------------

module.exports = function resolve_all_variables (functions) {

    // work the hierarchy of functions in a top-down manner
    // such that parent functions are processed before child
    // functions.  we start with functions without a parent.

    for (const func of functions) {
        if (func.is_func_node && !func.parent_func)
            process_function(func);
    }
}
