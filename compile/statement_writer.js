
// ------------------------------------------------------------

const write_expression = require('./expression_writer');
const compare_writer = require('./compare_writer');
const utils = require('./utils');
const utils_c = require('./utils_c');

// ------------------------------------------------------------

const statement_writers = {

    'EmptyStatement': empty_statement,

    'BlockStatement': block_statement,

    'ExpressionStatement': expression_statement,

    'VariableDeclaration': variable_declaration,

    'FunctionDeclaration': function_declaration,

    'IfStatement': if_statement,

    'WhileStatement': while_statement,

    'ForStatement': for_statement,
    'ForInStatement': for_statement,
    'ForOfStatement': for_statement,

    'BreakStatement': break_continue_statement,
    'ContinueStatement': break_continue_statement,

    'ThrowStatement': throw_statement,

    'ReturnStatement': return_statement,

    'WithStatement': with_statement,

    'TryStatement': try_statement,
};

// ------------------------------------------------------------

const new_closure_var_prefix = 'js_val *';
const new_closure_var_suffix = '=js_newclosure(env,NULL);'

// ------------------------------------------------------------

function empty_statement (stmt, output) { }

// ------------------------------------------------------------

function block_statement (stmt, output) {

    // save the index past the opening brace, in case we need
    // to insert declarations for temporary variables.  these
    // would be allocated by 'alloc_temp_val' in utils_c,
    // while processing statements within the block

    output.push('{');
    let temp_index = output.length;

    stmt.temp_vals = [];
    stmt.temp_stks = [];
    // a caller may set init_text in advance
    if (!Array.isArray(stmt.init_text))
        stmt.init_text = [];

    if (Array.isArray(stmt.body))
        statement_writer(stmt.body, output);

    else if (typeof(stmt.body) === 'function')
        stmt.body(stmt, output);

    else
        throw [ stmt, 'unexpected block body' ];

    function process_temp_list (list, text, asterisk) {
        if (list.length) {
            for (;;) {
                text += asterisk + list.shift();
                if (!list.length)
                    break;
                text += ',';
            }
            output.splice(temp_index, 0, text + ';');
        }
    }

    while (stmt.init_text.length)
        output.splice(temp_index, 0, stmt.init_text.shift());

    process_temp_list(stmt.temp_vals, 'js_val ', '');
    process_temp_list(stmt.temp_stks, 'js_link ', '*');

    // dummy statements to prevent warnings about unused variables
    for (var [var_name, var_node] of stmt.scope) {
        if (!var_node.skip_void_reference)
            output.push(`(void)${var_node.c_name};`);
    }

    output.push('}');
}

// ------------------------------------------------------------

function expression_statement (stmt, output) {

    if (stmt.expression.type === 'Literal')
        return; // statement has no effect
    // set flag for update_expression () in update_writer.js
    stmt.void_result = true;
    const text = write_expression(stmt.expression, false);
    output.push(text + ';');
}

// ------------------------------------------------------------

function variable_declaration (stmt, output) {

    let num_declarations = stmt.declarations.length;
    if (!num_declarations)
        output.push(';');

    for (const decl of stmt.declarations) {

        if (decl.id?.type !== 'Identifier')
            throw [ stmt, 'error in declaration' ];

        let c_name = utils_c.get_variable_c_name(decl);
        let text;

        if (!variable_declaration_special_node(decl))
            continue;

        if (decl.is_closure) {

            // if this is a 'var' declaration, then we insert
            // init at the function scope, else block scope
            const block_node = (stmt.kind !== 'var') ? stmt
                : utils.get_parent_func_node(stmt).body;

            // push a source line to the top of the containing
            // block, to allocate memory for the local variable.
            // its initial value will be set to js_uninitialized.
            // see js_newclosure () & js_closureval () in func.c,
            // and closure_expression () in expression_writer.js.
            utils_c.insert_init_text(block_node,
                new_closure_var_prefix + c_name + new_closure_var_suffix);

            if (!decl.init) {
                // the closure variable should become accessible
                // at the point of its declaration, so initialize
                // it to undefined if no explicit initial value
                decl.init = utils_c.init_expr_undefined;
            }

            text = '*' + c_name;

        } else {

            // normal local variable.  if declared with the
            // 'var' keyword, then it was handled by function
            // write_var_locals () in function_writer.js.
            // but we might still need to initialize it.

            text = '';
            if (stmt.kind !== 'var') {
                if (stmt.kind === 'const')
                    text += 'const ';
                else if (stmt.kind !== 'let')
                    throw [ stmt, 'error in declaration' ];
                text += 'js_val ';

                if (decl.is_closure_var_init) {
                    // special case for a 'var' (not let/const)
                    // which is a closure variable.  memory was
                    // allocated for it by the block above, but
                    // the initial value of js_uninitialized
                    // must now be replaced with js_undefined.
                    // see also write_var_locals ()
                    text = '*';
                }

                if (!decl.init)
                    decl.init = utils_c.init_expr_undefined;

            } else if (!decl.init)
                continue;

            text += c_name;
        }

        const init_expr = write_expression(decl.init, true);

        if (decl.with_root_node && stmt.kind === 'var') {
            //
            // in the case of a 'var' local declared inside a
            // 'with' statement, we effectively have an assignment
            // here, which must go through 'with' logic.  see also
            // process_declarations () in variable_resolver.js
            //
            const prop_var2 =
                utils_c.get_with_local2(decl, decl.id.c_name);
            text = `js_setwith(env,func_val,`
                 + `${prop_var2},${init_expr});`;

        } else {
            //
            // otherwise this is simple declaration/assignment
            //
            text += '=' + init_expr + ';';
        }

        output.push(text);
    }
}

// ------------------------------------------------------------

function variable_declaration_special_node (decl) {

    if (decl.init)
        return true;

    //
    // if this is a declaration node for a pseudo-var
    // that holds a meta property, as created by
    // create_meta_property () in variable_resolver.js
    // then fill in the actual declaration init code
    //

    if (decl.is_meta_property) {
        if (!decl.is_meta_property_ref
        &&  !decl.is_closure) {
            // meta property was never actually referenced
            return false;
        }
        if (decl.id.name === 'new.target') {
            decl.init = { type: 'Literal',
                c_name: 'env->new_target' };
        } else
            throw [ decl, 'unknown meta property' ];
    }

    //
    // create_arguments_object () in variable_resolver.js
    // inserts an 'arguments' var into every function,
    // but we only initialize it if actually referenced,
    // see also js_arguments () in stack.c.
    //

    if (decl.is_arguments_object) {
        let func_val = 'js_undefined';
        let text = '';
        if (!utils.get_parent_func_node(decl).strict_mode) {
            // note that if the function is not strict mode,
            // then 'arguments' property in its function object
            // must be updated, so we call js_arguments () even
            // if the 'arguments' pseudo-var is not referenced.
            // we pass a reference to the current function in
            // the 'callee' argument, in this non-strict case.
            func_val = 'func_val'
            // inject a suffix to prevent the potential
            // warning about an unused local variable
            text = `);((void)${decl.c_name}`;
        } else if (!decl.is_arguments_object_ref) {
            // if strict mode, and 'arguments' is not
            // explicitly referenced, then don't create it
            return false;
        }
        text = `js_arguments(env,${func_val},stk_args)`
             + text;
        decl.init = { type: 'Literal', c_name: text };
    }

    return true;
}

// ------------------------------------------------------------

function function_declaration (stmt, output) {

    const func_name = stmt.id?.name;
    if (!func_name)
        throw [ stmt, 'function declaration without a name' ];

    let copy_node = Object.assign(stmt, { type: 'FunctionExpression'});
    let func_expr = write_expression(copy_node, false);
    const var_name = utils_c.get_variable_c_name(stmt);

    // in non-strict mode, a function declaration inside an
    // 'if' statement is hoisted to the top of the function.
    // see also write_var_locals () in function_writer.js,
    // which declares such variables
    let js_val_prefix = 'js_val ';
    if (stmt.parent_node?.type === 'IfStatement') {
        const func_stmt = utils.get_parent_func_node(stmt);
        if (!func_stmt.strict_mode)
            js_val_prefix = stmt.is_closure ? '*' : '';

    } else if (stmt.is_closure) {
        // the function is referenced by a nested function,
        // so it must be declared as a closure variable,
        // i.e. a pointer to a value, in the same way that
        // variable_declaration () handles a closure var.
        utils_c.insert_init_text(stmt,
            new_closure_var_prefix + var_name + new_closure_var_suffix);
        js_val_prefix = '*';
    }

    output.push(`${js_val_prefix}${var_name}=${func_expr};`);
}

// ------------------------------------------------------------

function if_statement (stmt, output) {

    const cond = compare_writer.write_condition(stmt.test);
    output.push('if' + cond);

    statement_writer(stmt.consequent, output, true);
    if (stmt.alternate) {
        output.push('else');
        statement_writer(stmt.alternate, output, true);
    }
}

// ------------------------------------------------------------

function while_statement (stmt, output) {

    output.push('while');
    output.push(compare_writer.write_condition(stmt.test));
    statement_writer(stmt.body, output, true);
}

// ------------------------------------------------------------

function for_statement (stmt, output) {

    // we want to make sure the entire 'for' statement
    // is enclosed in a C block, which we do by altering
    // the statement type to 'BlockStatement' and making
    // sure that block_statement () calls us back

    if (stmt.type !== 'BlockStatement') {

        const save_type = stmt.type;

        stmt.type = 'BlockStatement';
        stmt.save_type = save_type;
        stmt.loop_body = stmt.body;
        stmt.body = for_statement;

        statement_writer(stmt, output);

        stmt.type = save_type;
        stmt.body = stmt.loop_body;
        stmt.save_type = undefined;
        stmt.loop_body = undefined;

        return;
    }

    //
    // process the 'for' statement, now in a C block.
    //

    const stmt_init = select_init_node(stmt);

    output.push('for(');
    output.push('/* init */');

    let update_text = '';
    if (stmt_init.type === 'VariableDeclaration') {
        for (const decl of stmt_init.declarations)
            decl.skip_void_reference = true;
        variable_declaration(stmt_init, output);
        update_text = collect_closure_vars(stmt);
    } else
        output.push(write_expression(stmt_init, false) + ';');

    let test_text = ';';
    if (typeof(stmt.test) === 'string')
        test_text = stmt.test + test_text;
    else if (stmt.test) {
        test_text = compare_writer.write_condition(stmt.test)
                  + test_text;
    }
    output.push('/* test */');
    output.push(test_text);

    if (typeof(stmt.update) === 'string')
        update_text += stmt.update;
    else if (stmt.update) {
        // set flag for update_expression () in update_writer.js
        stmt.void_result = true;
        update_text += write_expression(stmt.update, false);
        stmt.void_result = false;
    } else if (update_text.at(-1) === ',') {
        // if collect_closure_vars () injects update-text,
        // and there is no other update text, then we have
        // to discard a superfluous comma
        update_text = update_text.slice(0, -1);
    }
    output.push('/* update */');
    output.push(update_text + ')');

    if (stmt.loop_body)
        statement_writer(stmt.loop_body, output, true);
    else
        output.push(';');

    //
    // if any variables declared in the loop 'init' clause
    // are closure variables, then variable_declaration ()
    // added init text for them into the top of the block,
    // which is actually the current node, because the
    // 'ForStatement' was changed to a 'BlockStatement'.
    //
    // we want to allocate closure memory for each such
    // variable on every iteration, not just once at the
    // top of the loop.  so find each line of init text:
    //      js_val *local_var_nnnn=js_newclosure ()...
    // as generated by variable_declaration, and copy
    // this text into the update clause of the for-loop.
    //

    function collect_closure_vars (stmt) {

        let init_text = stmt.init_text;
        let update_text = '';

        for (let i = 0; i < init_text.length; i++) {
            const line = init_text[i];
            let j = line.indexOf(new_closure_var_prefix);
            let k = line.indexOf(new_closure_var_suffix, j);
            if (j !== -1 && k !== -1) {

                // the update clause allocates a new copy of
                // the loop control variable, with same value
                j += new_closure_var_prefix.length;
                update_text +=
                    // closure_var =
                    //      new_closure_var(closure_var),
                    line.substring(j, k)
                +   '=js_newclosure(env,'
                +   line.substring(j, k)
                +   '),';
            }
        }

        return update_text;
    }

    //
    // create a initializer expression for the loop var,
    // if this is a for-of / for-in statement
    //

    function select_init_node (stmt) {

        if (stmt.init)
            return stmt.init;   // traditional 'for'

        let node = stmt.left;
        if (!node) {
            // traditional 'for' without init clause
            return { type: 'Literal', c_name: '/**/' };
        }

        // if we reach here, it must be a for-of / for-in
        // statement, with both 'left' and 'right' nodes
        if (!stmt.right)
            throw [ stmt, 'invalid for' ];

        // insert a statement at the top of the block to
        // create the iterator, by calling a function which
        // calls Symbol.hasIterator () on right-hand expr.
        //
        // it uses three temp values, declared as an array:
        // array[0] references the iterator next() function,
        // which gets reset to zero when iteration is done;
        // array[1] references the iterator 'this' object;
        // and array[2] gets the next result value.
        // see also js_newiter () and js_nextiter ()
        //
        const iter = 'iter_' + utils.get_unique_id();
        utils_c.insert_init_text(stmt,
            `js_val ${iter}[3];js_newiter(env,${iter},`
            + `'${stmt.save_type[3]}',`
            + write_expression(stmt.right) + ');');

        let decl_0;
        if (node.type === 'Identifier') {
            decl_0 = node.decl_node || node;
            node = { type: 'Literal', c_name: '/**/' };

        } else if (node.type === 'VariableDeclaration'
                && node.declarations.length === 1) {

            if (node.kind === 'const')
                node.kind = 'let';
            decl_0 = node.declarations[0];
            decl_0.init = null;

        } else
            throw [ stmt, 'bad loop variable' ];

        const c_name = utils_c.get_variable_c_name(decl_0);
        const deref = decl_0.is_closure ? '*' : '';

        // if the iterator is valid, update the
        // loop variable with the next result
        stmt.test = `likely(${iter}[0].raw!=0)`
            + `?(${deref}${c_name}=${iter}[2],true):false`;

        stmt.update = `(void)${c_name},`
                    + `js_nextiter(env,${iter})`;

        return node;
    }
}

// ------------------------------------------------------------

function break_continue_statement (stmt, output) {

    if (stmt.label !== null) {

        console.log(stmt);
        throw [ stmt, 'not supported yet - break/continue label' ];
    }

    if (stmt.type === 'BreakStatement')
        output.push('break;');
    else if (stmt.type === 'ContinueStatement')
        output.push('continue;');
    else
        throw [ stmt, 'unknown break/continue type' ];
}

// ------------------------------------------------------------

function throw_statement (stmt, output) {

    const expr_text = stmt.argument
                    ? write_expression(stmt.argument)
                    : 'js_undefined';
    const stmt_text = `js_throw(env,${expr_text});`;
    output.push(stmt_text);
}

// ------------------------------------------------------------

function return_statement (stmt, output) {

    if (stmt.parent_node
    && !utils.get_parent_func_node(stmt).strict_mode) {
        // if not strict mode function, then js_arguments ()
        // called by variable_declaration_special_node (),
        // to set 'arguments' and 'caller' in the function
        // object, which we now have to reset to null.
        // see also js_throw () in stack.c, which resets
        // the 'arguments' property in case of exception.
        output.push(
            'js_arguments2(env,func_val,js_null,NULL);');
    }

    const expr_text = stmt.argument
                    ? write_expression(stmt.argument)
                    : 'js_undefined';
    // discard last stack frame and return an expression
    output.push(`js_return(${expr_text});`);
}

// ------------------------------------------------------------

function with_statement (stmt, output) {

    // apply the target expression as the 'with' scope.
    // see also property access functions in file with.c
    output.push('js_scopewith(env,func_val,' +
                        write_expression(stmt.object) + ');');
    // write the statements in the 'with' block
    statement_writer(stmt.body, output);
    // discard the most recent 'with' scope
    output.push('js_scopewith(env,func_val,js_discard_scope);');
}

// ------------------------------------------------------------

function try_statement (stmt, output) {

    output.push('if(setjmp(*js_entertry(env))==0){');
    statement_writer(stmt.block, output);
    output.push('}');

    const handler = stmt.handler;
    if (handler.type !== 'CatchClause') {
        // no catch clause
        output.push(`js_leavetry(env);`);
    } else {

        const exc = 'exc_' + utils.get_unique_id();
        output.push(`js_val ${exc}=js_leavetry(env);`);
        output.push(`if(${exc}.raw!=js_uninitialized.raw){`);

        if (handler.param) {

            // note that originally, the sub-node of the
            // CatchClause node was not VariableDeclaration,
            // see also declare_catch_variable ()
            if (handler.param.type !== 'VariableDeclaration')
                throw [ stmt, 'unexpected catch clause' ];

            // destructuring should also be permitted here
            const var_decl = handler.param.declarations[0];
            var_decl.skip_void_reference = true;
            var_decl.init = { type: 'Literal', c_name: exc };
            statement_writer(handler.param, output);
            output.push(`(void)${var_decl.c_name};`);
        }

        statement_writer(handler.body, output);
        output.push('}');
    }

    if (stmt.finalizer)
        statement_writer(stmt.finalizer, output);
}

// ------------------------------------------------------------
//
// statement_writer
//
// main entry point for this module
//
// ------------------------------------------------------------

function statement_writer (stmt_or_list, output, extra_braces) {

    if (Array.isArray(stmt_or_list)) {

        for (const stmt of stmt_or_list)
            statement_writer(stmt, output);

    } else {

        const stmt = stmt_or_list;
        output.push(stmt.loc);

        const f = statement_writers[stmt?.type];
        if (f) {

            if (extra_braces) {
                if (stmt.type !== 'BlockStatement')
                    output.push('{');
                else
                    extra_braces = false;
            }

            f(stmt, output);

            if (extra_braces)
                output.push('}');

        } else
            throw [ stmt, `unknown statement type ${stmt?.type}` ];
    }
}

// ------------------------------------------------------------

module.exports = statement_writer;
