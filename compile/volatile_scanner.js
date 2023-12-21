
const utils = require('./utils');

// ------------------------------------------------------------

module.exports = function volatile_scanner (func) {

    // the c spec does not guarantee the value of
    // non-volatile local variables which are set
    // between setjmp and longjmp.  for example,
    // js_val x = 123; js-try-via-setjmp {
    //        x = 456; js-throw-via-longjmp; }
    // if x is held in a CPU register, then the
    // longjmp effectively resets x to 123.
    //
    // the 'volatile' modifier works around this
    // problem by prohibiting optimizations, so
    // we mark some locals as volatile, if:
    //
    // (1) the local is updated within a try {}.
    // (2) the local was already declared prior
    // to the start of that particular try {}.
    // (3) the local is referenced past the end
    // of the try {} body in question.

    look_for_try_blocks(func.body, new Set());
}

// ------------------------------------------------------------

function look_for_try_blocks (node, vars) {

    if (node.type === 'TryStatement') {

        // we hit a try {} statement, scan its body
        // for any variables that are updated, and
        // which were declared before the try {}

        find_potential_vars(node, vars);

        // recursively process catch and finally

        if (node.handler)
            look_for_try_blocks(node.handler, vars);

        if (node.finalizer)
            look_for_try_blocks(node.finalizer, vars);

    } else if (node.type === 'Identifier') {

        // we hit a reference, check if this is for
        // a variable that was added to the set of
        // variables that may need 'volatile'.

        const decl_node = node.decl_node;
        if (vars.has(decl_node)) {

            decl_node.is_volatile_var = true;
            vars.delete(decl_node);
        }

    } else {

        // otherwise keep processing recursively

        for (const node2 of utils.get_child_nodes(node)) {

            look_for_try_blocks(node2, vars);
        }
    }
}

// ------------------------------------------------------------

function find_potential_vars (try_node, vars_to_track) {

    let vars_updated_in_try = new Set();

    // collect any non-closure variables that are
    // assigned or updated inside the try {} block
    find_vars_updated_inside_try(
        try_node.block, vars_updated_in_try);

    // discard any variables that are scoped to
    // this try {} block, but any other variable
    // is added to the set of variables to track
    // for any references below the try {} block.
    discard_vars_declared_inside_try(
        try_node, vars_updated_in_try, vars_to_track);
}

// ------------------------------------------------------------

function find_vars_updated_inside_try (node, vars, in_update) {

    if (node.type === 'AssignmentExpression') {

        find_vars_updated_inside_try(
                    node.left, vars, true);

        find_vars_updated_inside_try(
                    node.right, vars, in_update);

    } else if (node.type === 'UpdateExpression') {

        find_vars_updated_inside_try(
                    node.argument, vars, true);

    } else if (node.type === 'Identifier') {

        const decl_node = node.decl_node;
        if (in_update && !decl_node.is_volatile_var
                      && !decl_node.is_closure) {

            vars.add(decl_node);
        }

    } else {

        for (const node2 of utils.get_child_nodes(node)) {

            find_vars_updated_inside_try(
                        node2, vars, in_update);
        }
    }
}

// ------------------------------------------------------------

function discard_vars_declared_inside_try (try_node, vars1, vars2) {

    for (const v of vars1) {

        let v2 = v;
        while (v2 && v2 !== try_node)
            v2 = v2.parent_node;

        if (!v2)
            vars2.add(v);
    }

    return vars2;
}
