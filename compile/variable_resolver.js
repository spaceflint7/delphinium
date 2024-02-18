
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

    let func_stmt;
    do {

        func_stmt = node.parent_node;
        if (!func_stmt.is_func_node)
            break; // error
        node.scope = func_stmt.scope;

        if (node.type === 'Identifier')
            process_one_parameter(node);
        else {
            // destructuring assignment in parameters.
            // first, we connect the sub-tree of nodes
            (function set_parents_recursively (node1) {
                for (const node2 of utils.get_child_nodes(node1)) {
                    node2.parent_node = node1;
                    node2.scope = node1.scope;
                    set_parents_recursively(node2);
                }
            })(node);
            // the actual function argument should not be
            // accessible by name, but is later used as the
            // right-hand  side of a destructuring assignment.
            // the identifiers on the left-hand side of that
            // assignment should be entered into scope.
            if (!process_pattern_parameters(node))
                break; // error
        }
        return;

    } while (false);

    throw [ node, 'unknown parameter node: ' + node.type ];

    // place a simple identifier in function-level scope

    function process_one_parameter (node) {

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
        node.is_parameter = true;
        node.scope.set(node.name, node);
    }

    // recursively process a destructuring assignment
    // pattern in order to identify all the simple
    // identifier names that we need to place in scope

    function process_pattern_parameters (node) {

        if (node.type === 'RestElement')
            node = node.argument;
        else if (node.type === 'AssignmentPattern')
            node = node.left;
        let node2list;
        if (node.type === 'Identifier')
            node2list = [ node ];
        else if (node.type === 'ArrayPattern')
            node2list = node.elements;
        else if (node.type === 'ObjectPattern')
            node2list = node.properties;
        else
            return false; // unknown node type

        for (let node2 of node2list) {
            if (node2.type === 'Property') {
                node2 = node2.value;
                if (node2.type === 'AssignmentPattern')
                    node2 = node2.left;
            } else if (node2.type === 'RestElement')
                return process_pattern_parameters(node2);
            if (node2.type !== 'Identifier')
                return false; // unknown node type
            node2.unique_id = utils.get_unique_id();
            process_one_parameter(node2);
        }

        return true;
    }
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

    if (node.type === 'FunctionDeclaration') {

        let do_strict_mode_scoping = false;
        const declaration_is_at_function_scope =
            node.parent_node.parent_node.is_func_node;
        if (!declaration_is_at_function_scope) {
            // in strict mode, function declarations are scoped
            // to a block;  in non-strict mode, to the function
            const func_node = utils.get_parent_func_node(node);
            do_strict_mode_scoping = func_node.strict_mode;
        }
        if (do_strict_mode_scoping)
            add_local_if_first_in_scope(node);
        else
            add_local_if_declared_var(node);

    } else if (node.type === 'ClassDeclaration') {

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

    } else if (node.type === 'MetaProperty') {

        create_meta_property(node, true);

    } else if (node.type === 'ThisExpression') {

        create_meta_property_for_parent_this(node);

    } else if (node.type === 'ArrowFunctionExpression') {

        create_meta_properties_in_arrow_function(node);

    } else if (node.type === 'CatchClause') {

        node.scope = new Map();
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

    const other_node = node.scope.get(name)
                    ?? find_parameter_node(node, name);
    if (other_node && !other_node.is_catch_variable)
        duplicate_identifier_error(node, other_node);

    node.unique_id = utils.get_unique_id();
    node.scope.set(name, node);

    // at the function level, 'let' and 'const' locals
    // may not override parameter names
    function find_parameter_node (node, name) {
        if (node.is_arguments_object || node.is_meta_property)
            return;
        const block_node = node.parent_node.parent_node;
        if (block_node.type === 'BlockStatement') {
            const func_node = block_node.parent_node;
            if (func_node.is_func_node)
                return func_node.scope.get(name);
        }
    }
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
            if (!(is_func_node && check_node === other_node)) {
                // allow a 'var' declaration to specify the
                // name of a function parameter, effectively
                // changing the declaration to an assignment
                if (other_node.is_parameter) {
                    node.unique_id = other_node.unique_id;
                    node.is_parameter = true;
                    return;
                }
                // also allow a 'var' declaration to override
                // any function declaration in the same scope,
                // but only at the function-level scope
                if (other_node.type !== 'FunctionDeclaration')
                    duplicate_identifier_error(node, other_node);

                else if (node.kind === 'var') {
                    const declaration_is_at_function_scope =
                        other_node.parent_node
                                        .parent_node.is_func_node;
                    if (!declaration_is_at_function_scope) {
                        duplicate_identifier_error(
                                                node, other_node);
                    }
                    // a local with this name already exists
                    // as a result of a function declaration
                    node.unique_id = other_node.unique_id;
                    return;

                } else if (node.type === 'FunctionDeclaration') {
                    // in non-strict mode, a function declaration
                    // in a nested block shares the same local as
                    // the declaration at the function-level scope
                    const func_node = utils.get_parent_func_node(node);
                    if (!func_node.strict_mode) {
                        node.unique_id = other_node.unique_id;
                        return;
                    }
                }
            }
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

    // first, we collect all declarations into a list.
    // most declarations are found as an Identifier node,
    // directly below a VariableDeclarator parent node.
    // but with destructuring, there are one or more
    // layers of ArrayPattern or ObjectPattern nodes.

    const list = [];

    for (const decl of node.declarations) {
        if (decl.type === 'VariableDeclarator') {
            if (add_one_to_list(list, decl?.id))
                continue;
        }
        throw [ node, `unexpected node ${node.type}` ];
    }

    // if the next node is a simple identifier,
    // add it to the list, otherwise process the
    // pattern node, possibly recursively

    function add_one_to_list (list, decl) {

        const decl_type = decl.type;
        if (decl_type === 'Identifier') {
            list.push(decl);
            return true;
        }
        if (decl_type === 'Property') {
            list.push(decl.value);
            return true;
        }
        if (decl_type === 'ArrayPattern')
            return add_some_to_list(list, decl.elements, true);
        if (decl_type === 'ObjectPattern')
            return add_some_to_list(list, decl.properties);
    }

    function add_some_to_list (list, list2, can_skip_null) {

        for (let decl2 of list2) {
            if (decl2 === null && can_skip_null)
                continue;
            if (decl2.type === 'RestElement')
                decl2 = decl2.argument;
            if (!add_one_to_list(list, decl2, decl2.type))
                return false;
        }
        return true;
    }

    //
    // now we have a list of identifier 'leaf' nodes.
    //

    let new_decls;

    for (let decl of list) {

        // if the declaration is not a direct child of
        // a VariableDeclarator node, then create a fake
        // VariableDeclarator node between the top-level
        // VariableDeclaration and the actual Identifier.

        if (decl.parent_node.type === 'VariableDeclarator')
            decl = decl.parent_node;
        else {

            if (!new_decls)
                node.declarations_2 = new_decls = [];
            new_decls.push(decl = decl.decl_node = {
                type: 'VariableDeclarator',
                id: decl, parent_node: node });
        }

        // add the new Identifier node into the scope

        decl.kind = node.kind;
        decl.scope = node.scope;
        if (decl.kind === 'const')
            decl.is_const = true;

        if (only_if_first_in_scope)
            add_local_if_first_in_scope(decl);
        else
            add_local_if_declared_var(decl);

        decl.id.unique_id = decl.unique_id;
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

function create_meta_property (node, is_real_ref, func_node) {

    if (!func_node) {
        // if called from process_declarations (),
        func_node = utils.get_parent_func_node(node);
    }

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

    // if the meta.property name is already in scope,
    // then we already did the work once, and we're done
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
    };
    prop_node.init = null;
    prop_node.parent_node = meta_node;
    prop_node.scope = func_node.scope;
    prop_node.is_meta_property = true;

    // overwrite meta_node as a VariableDeclaration node
    // which contains the newly-overwritten prop_node.
    meta_node.type = 'VariableDeclaration';
    meta_node.name = undefined;
    meta_node.declarations = [ prop_node ];
    meta_node.kind = 'const';
    meta_node.parent_node = func_node.body;
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

function create_meta_property_for_parent_this (node) {

    // convert a ThisExpression node which occurs
    // within an arrow function to our meta property
    const func_node = utils.get_parent_func_node(node);
    if (func_node.type !== 'ArrowFunctionExpression')
        return;

    node.type     = 'MetaProperty';
    node.meta     = { type: 'Identifier', name: 'parent' };
    node.property = { type: 'Identifier', name: 'this' };

    create_meta_property(node, true, func_node);
}

// ------------------------------------------------------------

function create_meta_properties_in_arrow_function (node) {

    // create meta property nodes in advance.
    // the property flag 'is_meta_property_ref'
    // becomes set if a particular meta property
    // actually gets referenced by the function.

    const func_node = utils.get_parent_func_node(node);

    create_meta_property({
        meta:     { type: 'Identifier', name: 'new' },
        property: { type: 'Identifier', name: 'target' },
        parent_node: node }, false, func_node);

    create_meta_property({
        meta:     { type: 'Identifier', name: 'parent' },
        property: { type: 'Identifier', name: 'this' },
        parent_node: node }, false, func_node);
}

// ------------------------------------------------------------

function create_arguments_object (func_node) {

    if (func_node.type === 'ArrowFunctionExpression')
        return;

    const node3 = {
        type: 'Identifier',
        name: 'arguments',
        parent_node: undefined,
    };

    const node2 = {
        type: 'VariableDeclarator',
        id: node3,
        init: null,
        parent_node: undefined,
        scope: func_node.scope,
        is_arguments_object: true,
    };

    const node1 = {
        type: 'VariableDeclaration', kind: 'let',
        declarations: [ node2 ],
        parent_node: func_node.body,
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

    node.param = {
        type: 'VariableDeclaration', kind: 'let',
        declarations: [ {
            type: 'VariableDeclarator',
            id: node.param,
            init: null,
            loc: node.loc,
            is_catch_variable: true,
            parent_node: null,
        } ],
        parent_node: node,
    };

    let node_var_decl = node.param;
    let node_decl_0   = node_var_decl.declarations[0];
    let node_id       = node_decl_0.id;

    node_id.parent_node         = node_decl_0;
    node_decl_0.parent_node     = node_var_decl;
    node_var_decl.parent_node   = node;

    process_declarations(node_var_decl);
}

// ------------------------------------------------------------

function process_references (node) {

    // first, filter out some Identifier nodes that we
    // don't like:  those that describe a declaration
    // rather than a reference, and those that describe
    // literal static property names

    if (node.unique_id)
        return;
    let parent_node = node.parent_node;

    // check if property access expression: obj.prop
    // or if it is an object expression: { prop: value }
    const is_property_access =
        (   parent_node.type === 'MemberExpression'
         && parent_node.property === node);
    const is_object_expression =
        (   parent_node.type === 'Property'
         && (   parent_node.parent_node.type === 'ObjectExpression'
             || parent_node.parent_node.type === 'ObjectPattern')
         && parent_node.key === node);
    if (    (is_property_access || is_object_expression)
         && !parent_node.computed) {
        // don't try to resolve literal property names;
        // just let the literal.js module stringify them.
        node.is_property_name = true;
        return;
    }

    if (parent_node.type === 'FunctionExpression'
    &&  parent_node.id === node) {
        // don't try to resolve the name of a function
        // in a function expression;  this identifier
        // is added by process_function (), see there
        return;
    }

    if ((   parent_node.type === 'LabeledStatement'
         || parent_node.type === 'BreakStatement'
         || parent_node.type === 'ContinueStatement')
    &&  parent_node.label === node) {
        // don't try to resolve the name of a label
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

    parent_node = node;
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

            if (other_node.is_parameter) {

                check_uninitialized_parameter(node, other_node);
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

    if (error) {
        // ignore an error caused by a function name node
        // that was created by determine_function_name ()
        if (ref_node.is_function_name &&
                ref_node.parent_node.not_constructor)
                        error = false;
    }

    if (!error) {

        const block = utils.get_parent_block_node(ref_node, false);
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

function check_uninitialized_parameter (ref_node, decl_node) {

    // check if a default initializer for a parameter
    // references an earlier parameter which has not
    // yet been initialized, e.g. function f (p = p + 1)

    const func_node = utils.get_parent_func_node(decl_node);
    const ref_node_0 = ref_node;

    for (;;) {
        const parent_node = ref_node.parent_node;
        if (parent_node.type === 'BlockStatement') {
            // if hit a block statement then we are
            // not below any parameter decl nodes
            return;
        }
        if (parent_node.is_func_node)
            break;
        ref_node = parent_node;
    }

    if (utils.get_distance_from_block(ref_node, func_node)
     <= utils.get_distance_from_block(decl_node, func_node)) {

        throw [ ref_node_0,
                'parameter referenced before initialization' ];
    }

    ref_node_0.temp_block_stmt = {};
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

// ------------------------------------------------------------

module.exports.add_closure_variable = add_closure_variable;
module.exports.add_closure_function = add_closure_function;
