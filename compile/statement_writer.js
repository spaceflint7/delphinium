
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

    'LabeledStatement': labeled_statement,

    'VariableDeclaration': variable_declaration,

    'FunctionDeclaration': function_declaration,

    'IfStatement': if_statement,

    'WhileStatement': while_statement,
    'DoWhileStatement': while_statement,

    'ForStatement': for_statement,
    'ForInStatement': for_statement,
    'ForOfStatement': for_statement,

    'BreakStatement': break_continue_statement,
    'ContinueStatement': break_continue_statement,

    'ThrowStatement': throw_statement,

    'ReturnStatement': return_statement,

    'WithStatement': with_statement,

    'TryStatement': try_statement,

    'SwitchStatement': switch_statement,
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

    if (!stmt.inhibit_braces)
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

    if (!stmt.inhibit_braces)
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

function labeled_statement (stmt, output) {

    stmt.c_name = 'lbl_' + utils_c.clean_name(stmt.label)
                + '_' + utils.get_unique_id();

    // note that statement_writer () does not actually
    // insert the extra braces for a BlockStatement
    statement_writer(stmt.body, output, true);

    if (stmt.label_ref_in_continue) {
        // if the 'continue' flag is set, then assume
        // the 'body' was a loop control statement,
        // and output includes the text '//endloop'
        // where we insert the label with a 'c' suffix.
        // see also:  break_continue_statement () and
        // for_statement () and while_statement ()
        for (let index = output.length; index--; ) {
            if (output[index] === '}//endloop') {
                output.splice(
                    index, 0, stmt.c_name + 'c:;');
                break;
            }
        }
    }

    if (stmt.label_ref_in_break)
        output.push(stmt.c_name + 'b:;');
}

// ------------------------------------------------------------

function variable_declaration (stmt, output) {

    let num_declarations = stmt.declarations.length;
    if (!num_declarations)
        output.push(';');

    for (const decl of stmt.declarations) {

        const decl_type = decl.id?.type;
        if (decl_type === 'ArrayPattern') {
            if (variable_declaration_pattern(
                            stmt, decl, decl.id.elements, output))
                continue;
        }
        if (decl_type === 'ObjectPattern') {
            if (variable_declaration_pattern(
                            stmt, decl, decl.id.properties, output))
                continue;
        }

        if (decl_type !== 'Identifier')
            throw [ stmt, 'error in declaration' ];

        // initialize c_name even if we skip this var
        utils_c.get_variable_c_name(decl);

        if (!variable_declaration_special_node(decl))
            continue;

        variable_declaration_simple_decl(
                                stmt, decl, output);
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
            decl.skip_void_reference = true;
            return false;
        }
        text = `js_arguments(env,${func_val},stk_args)`
             + text;
        decl.init = { type: 'Literal', c_name: text };
    }

    return true;
}

// ------------------------------------------------------------

function variable_declaration_simple_decl (stmt, decl, output) {

    let c_name = utils_c.get_variable_c_name(decl);
    let text;

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
        if (stmt.kind === 'var') {
            if (!decl.init)
                return;
        } else {

            if (stmt.kind === 'const') {
                if (!stmt.inhibit_const) {
                    // see variable_declaration_pattern ()
                    text += 'const ';
                }
            } else if (stmt.kind !== 'let')
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

            } else if (stmt.parent_node.type === 'SwitchCase') {
                // special case for a 'let' (but not 'var')
                // in a case block, see switch_statement ()
                const switch_stmt = stmt.parent_node.parent_node;
                switch_stmt.init_text.push(
                    'js_val ' + c_name + '=js_undefined;');
                text = '';
            }

            if (!decl.init)
                decl.init = utils_c.init_expr_undefined;
        }

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

// ------------------------------------------------------------

function variable_declaration_pattern (
                            stmt, decl, decl_list, output) {

    let skip_void_reference = decl.skip_void_reference;
    if (stmt.kind !== 'var') {
        // even if this is a const declaration, we
        // discard the 'const' modifier, because the
        // declaration will be followed by assignment
        stmt.inhibit_const = true;
        //
        if (!(function do_elements (list) {
            for (let decl2 of list) {
                if (decl2 === null)
                    continue;
                if (decl2.type === 'RestElement') {
                    decl2 = decl2.argument;
                    if (decl2.type === 'ArrayPattern') {
                        if (!do_elements(decl2.elements))
                            return false;
                        continue;
                    }
                }
                if (decl2.type === 'ObjectPattern') {
                    if (!do_elements(decl2.properties))
                        return false;
                    continue;
                }
                if (decl2.type === 'Property')
                    decl2 = decl2.value;
                if (decl2.type !== 'Identifier')
                    return false;
                if (skip_void_reference)
                    decl2.decl_node.skip_void_reference = true;
                variable_declaration_simple_decl(
                                stmt, decl2.decl_node, output);
            }
            return true;
        })(decl_list)) return false;
    }

    let temp = {
        type: 'AssignmentExpression', operator: '=',
        left: decl.id, // ArrayPattern or ObjectPattern
        right: decl.init,
        parent_node: stmt.parent_node,
    };
    output.push(write_expression(temp, false) + ';');

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

    if (stmt.type === 'DoWhileStatement') {

        output.push('do');
        statement_writer(stmt.body, output, true);
        // labeled_statement () expects the following
        output[output.length - 1] += '//endloop';
    }

    output.push('while');
    output.push(compare_writer.write_condition(stmt.test));

    if (stmt.type === 'DoWhileStatement') {

        output.push(';');

    } else { // a plain while statement

        statement_writer(stmt.body, output, true);
        // labeled_statement () expects the following
        output[output.length - 1] += '//endloop';
    }
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
    output.push('for(/* init */;');
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

    if (stmt.loop_body) {
        statement_writer(stmt.loop_body, output, true);
        // labeled_statement () expects the following
        output[output.length - 1] += '//endloop';
    } else
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
                    + `js_nextiter1(env,${iter})`;

        return node;
    }
}

// ------------------------------------------------------------

function break_continue_statement (stmt, output) {

    if (stmt.label !== null) {

        let label = stmt;
        for (;;) {
            if (label.type === 'LabeledStatement'
            &&  label.label.name === stmt.label.name)
                break;
            label = label.parent_node;
            if (!label)
                throw [ stmt, 'cannot find label' ];
        }

        if (stmt.type === 'BreakStatement') {
            label.label_ref_in_break = true;
            label = label.c_name + 'b';

        } else if (stmt.type === 'ContinueStatement') {
            label.label_ref_in_continue = true;
            label = label.c_name + 'c';
        }

        output.push(`goto ${label};`);

    } else {

        if (stmt.type === 'BreakStatement')
            output.push('break;');
        else if (stmt.type === 'ContinueStatement')
            output.push('continue;');
    }
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

    if (return_inside_try(stmt, output))
        return;

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

    //
    // if the return statement appears within try/catch
    // block, we store the return value in a temporary
    // variable, and 'throw' without actually specifying
    // an exception, i.e. the exception value is set to
    // js_uninitialized.  see also try_statement ()
    //

    function return_inside_try (stmt, output) {

        // check if 'return' inside 'try/catch'
        for (let node2 = stmt;;) {
            const last_node = node2;
            node2 = node2.parent_node;
            if (!node2 || node2.is_func_node)
                return false;
            if (node2.type === 'TryStatement') {
                if (node2.finalizer === last_node) {
                    // if the 'return' appears inside the
                    // finalizer block, then it is already
                    // after unwinding the exception state
                    // (via the call to js_leavetry ()),
                    // so we don't consider this instance
                    // as a 'return' inside a try block.
                    // but, we do have to keep looking.
                    continue;
                }
                node2.nested_return = true;
                break;
            }
        }

        // allocate a temporary variable
        const func_node = utils.get_parent_func_node(stmt);
        let throw_ret_var = func_node.throw_ret_var;
        if (!throw_ret_var) {

            throw_ret_var = 'tmp_' + utils.get_unique_id();
            utils_c.insert_init_text(func_node.body,
                        'volatile js_val ' + throw_ret_var +
                        '=js_uninitialized;//tmp_ret_var');
            func_node.throw_ret_var = throw_ret_var;
        }

        const expr_text = stmt.argument
                        ? write_expression(stmt.argument)
                        : 'js_undefined';
        output.push('js_throw(env,'
                   + `((${throw_ret_var}=${expr_text}),`
                   + 'js_uninitialized));');

        return true;
    }
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
    statement_writer(stmt.block, output, false);
    output.push('}');

    const handler = stmt.handler;
    if (handler?.type !== 'CatchClause') {
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

            const var_decl = handler.param.declarations[0];
            var_decl.skip_void_reference = true;
            var_decl.init = {
                type: 'Literal', c_name: exc,
                parent_node: var_decl };
            statement_writer(handler.param, output);
            // we received one or more c-vars in the c_name
            // field, so add (void)references to prevent
            // potential warning about unused vars.  this
            // c_name field is set either as a side-effect
            // of get_variable_c_name () (for a simple var)
            // or add_c_name_for_void_reference () in case
            // this is a destructuring assignment.
            var_decl.c_name.split(',').map(
                        x => output.push(`(void)${x};`));
        }

        statement_writer(handler.body, output);
        output.push('}');
    }

    if (stmt.finalizer)
        statement_writer(stmt.finalizer, output);

    //
    // if there is a 'return' statement within this
    // 'try' block (even if inside some nested 'try'),
    // then that converted into a throw, see also
    // return_inside_try ().  in this case, we need
    // to implement that 'return' at this point.
    //

    if (stmt.nested_return) {

        let is_inside_try, throw_ret_var;
        for (let node2 = stmt;;) {
            node2 = node2.parent_node;
            if (node2.is_func_node) {
                // if any 'try' includes a nested return,
                // then a temporary variable to hold the
                // return value was already allocated,
                // see also return_inside_try ()
                throw_ret_var = node2.throw_ret_var;
                break;
            }
            if (node2.type === 'TryStatement') {
                // propagate the flag that this 'try'
                // block includes a nested return
                node2.nested_return = true;
                is_inside_try = true;
            }
        }

        output.push(`if(${throw_ret_var}.raw!=js_uninitialized.raw)`);
        if (is_inside_try)
            output.push('js_throw(env,js_uninitialized);');
        else
            output.push(`js_return(${throw_ret_var});`);
    }
}

// ------------------------------------------------------------

function switch_statement (stmt, output) {

    // create an artificial loop block, to allow for
    // standard use of 'break' statements within the
    // generated 'switch', which would actually be a
    // series of 'if' conditions
    output.push('do{//switch');

    // a local variable may be declared and assigned
    // in a case block and then referenced in a later
    // case block:  switch (x) { case 1: let y = 5;
    //                           case 2: print y; }
    // if jumping directly to case 2, then 'y' is in
    // scope, but uninitialized.  to fix this, we have
    // to hoist declarations of non-var locals to the
    // top of the 'switch' statement, and explicitly
    // initialize them to 'undefined'.  this is done
    // mostly by variable_declaration_simple_decl (),
    // and some code at the bottom of this function.
    let insert_index = output.length;
    stmt.init_text = [];

    // use a temp var if the switch discriminant
    // is not a trivial expression
    let lhs = write_expression(stmt.discriminant, false);
    if (!utils.is_basic_expr_node(stmt.discriminant)) {
        const tmp = utils_c.alloc_temp_value(stmt);
        output.push(`${tmp}=${lhs};`);
        lhs = tmp;
    }

    // write the conditions for each switch case,
    // with a goto to the corresponding case logic,
    // but always save the default case for last
    let def_node;
    for (const case_node of stmt.cases) {
        const lbl = case_node.c_name =
                'lbl_case_' + utils.get_unique_id();
        if (!case_node.test)
            def_node = case_node;
        else {
            const rhs = write_expression(case_node.test);
            output.push(`// line ${case_node.loc.start.line}`
                      + ` column ${case_node.loc.start.column + 1}`);
            output.push(`if(js_strict_eq(env,${lhs},${rhs}))`
                      + `goto ${lbl};`);
        }
    }
    if (def_node) {
        output.push(`// line ${def_node.loc.start.line}`
                  + ` column ${def_node.loc.start.column + 1}`);
        output.push(`goto ${def_node.c_name};`);
    }

    // write the case logic for each case
    for (const case_node of stmt.cases) {
        output.push(`// line ${case_node.loc.start.line}`
                  + ` column ${case_node.loc.start.column + 1}`);
        output.push(case_node.c_name + ':');
        statement_writer(case_node.consequent, output);
    }

    output.push('}while(0);//endswitch');

    // complete the hoisting of non-var locals to
    // the top of the 'switch' statement, see above.
    for (const decl of stmt.init_text)
        output.splice(insert_index++, 0, decl);
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
