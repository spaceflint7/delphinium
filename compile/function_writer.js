
const write_statements = require('./statement_writer');
const write_expression = require('./expression_writer');
const shape_cache = require('./shape_cache');
const utils = require('./utils');
const utils_c = require('./utils_c');

// ------------------------------------------------------------

exports.write_function_forward = function (func, output) {

    const c_name =
        `func_${func.id?.name || 'anon'}_${func.unique_id}`;

    const c_prototype = `js_val ${c_name}(js_c_func_args)`;

    func.c_name = c_name;
    func.c_prototype = c_prototype;

    output.push(`static ${func.c_prototype};`);
}

// ------------------------------------------------------------

exports.write_function = function (func, output) {

    output.push('');
    output.push('// ------------------------------------------------------------');
    output.push(func.c_prototype);

    // the opening brace for the function is written by
    // block_statement () in statement_writer.js
    let insert_index = output.length;

    const block_stmt_list = func.body.body;
    write_var_locals(func, block_stmt_list);
    let param_locals = write_param_locals(func);
    shape_cache.collect_keys(func);
    write_statements(func.body, output);

    while (typeof(output[insert_index]) !== 'string')
        ++insert_index;

    // insert our own function object into the stack frame,
    // with bit 1 set, to indicate the start of a new frame.
    output.splice(++insert_index, 0,
        'stk_args->value.raw=func_val.raw|2;');

    // if coroutine, yield execution at this point,
    // see also js_coroutine_init3 () in coroutine.c
    if (func.inject_call_to_yield) {
        output.splice(++insert_index, 0,
                    'js_yield(env,js_make_number(0.));');
    }

    // initialize local references to arguments passed
    if (param_locals.length) {
        while (param_locals.length)
            output.splice(++insert_index, 0, param_locals.shift());
    }

    // initialize the stack base pointer
    const max_args_in_calls = count_max_args_in_call_stmt(func);
    if (max_args_in_calls >= 0) {
        // number of stack links needed, plus some spare room
        const n = max_args_in_calls + 4;
        const stk_decl = `js_ensure_stack_at_least(${n});`
        output.splice(++insert_index, 0, stk_decl);
    }

    if (!func.strict_mode) {
        // in a non-strict function, a primitive 'this' gets
        // boxed into an object, or replaced with 'global'
        output.splice(++insert_index, 0,
                        'this_val=js_boxthis(env,this_val);');
    }

    output.pop();   // discard last } in block

    // if last statement in function is not 'return',
    // inject a return(undefined)

    if (block_stmt_list.length === 0 ||
            block_stmt_list[block_stmt_list.length - 1].type
                !== 'ReturnStatement') {
        // discard last stack frame to unwind the stack
        write_statements({ type: 'ReturnStatement',
                           parent_node: func.body }, output);
    }

    // dummy statements to prevent warnings about unused variables
    let void_vars = '';
    for (var [var_name, var_node] of func.scope) {
        if (var_node.kind === 'var')
            void_vars += `(void)${var_node.c_name};`;
    }
    // dummy statements to prevent warnings about unused functions
    for (const child_func of func.child_funcs) {
        const c_name = child_func?.decl_node?.c_name;
        if (c_name)
            void_vars += `(void)${c_name};`;
    }
    if (void_vars)
        output.push(void_vars);
    output.push(`}//endfunc ${func.c_name}`);
}

// ------------------------------------------------------------

function write_param_locals (func_stmt) {

    let stk_ptr = 'stk_' + utils.get_unique_id();
    let c_names = [];
    let output = [];
    for (let node of func_stmt.params) {

        // if the parameter is used in a closure reference,
        // or is part of a binding/destructuring assignment,
        // then we collect the argument into a temp local,
        // rather than the actual local for the parameter.
        // we then allocate a closure variable, and assign
        // the value from the temp local to the closure var.
        let c_name, c_name_var, is_rest_elem,
            is_identifier, is_identifier_closure;
        if (node.type === 'RestElement') {
            is_rest_elem = true;
            node = node.argument;
        }
        if (node.type === 'Identifier') {
            // a plain identifier gets a c_name, even if it
            // is also a closure, or inside a rest element
            is_identifier = true;
            is_identifier_closure = node.is_closure;
            const clean_name = utils_c.clean_name(node);
            c_name = node.c_name =
                    `arg_${clean_name}_${node.unique_id}`;
        }
        if (is_identifier_closure || !is_identifier) {
            // a parameter declaration which is something
            // other than a non-closure plain identifier
            // (which would be destructuring assignment),
            // gets a temp var to hold the argument value.
            c_name = 'arg_' + utils.get_unique_id();
        }
        c_names.push(c_name);

        if (is_rest_elem) {
            // process_parameter () in variable_resolver.js
            // checks that rest parameter is last parameter
            output.push(
                `${c_name}=js_restarr_stk(env,${stk_ptr});`);
        } else {
            output.push(`if(likely(${stk_ptr}!=js_stk_top)){`
                      + `${c_name}=${stk_ptr}->value;`
                      + `${stk_ptr}=${stk_ptr}->next;`
                      + `}else ${c_name}=js_undefined;`);
        }

        if (is_identifier_closure) {
            // allocate a closure variable,
            // and assign the actual argument value
            output.push(`js_val *${node.c_name}=`
                      + `js_newclosure(env,&${c_name});`);
        } else if (!is_identifier) {

            if (!process_pattern(node, c_name, c_names, output))
                throw [ node, 'unknown parameter node ' + node.type ];
        }
    }

    if (c_names.length) {
        output.unshift(`js_link *${stk_ptr}=stk_args->next;`);
        output.unshift('js_val ' + c_names.join(',') + ';');
        // dummy stmts to prevent warnings about unused vars
        c_names.forEach(name => output.push(`(void)${name};`));
    }

    return output;

    //

    function process_pattern (node, arg_c_name,
                              all_c_names, output) {

        let node2list;
        if (node.type === 'ArrayPattern')
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
                node2 = node2.argument;
            if (node2.type !== 'Identifier')
                return false; // unknown node type

            const clean_name = utils_c.clean_name(node2);
            all_c_names.push(node2.c_name =
                `arg_${clean_name}_${node2.unique_id}`);
            // force identifier_expression ()
            // to use the c_name in this node
            node2.is_property_name = true;
        }

        let temp_block_node = {
            type: 'BlockStatement',
            temp_vals: [], init_text: [],
            is_temp_block_node: true,
        };

        let temp = {
            type: 'AssignmentExpression', operator: '=',
            left: node, // ArrayPattern or ObjectPattern
            right: {
                type: 'Literal', c_name: arg_c_name,
                parent_node: temp_block_node
            },
            parent_node: temp_block_node,
        };

        let text = write_expression(temp, false);
        temp_block_node.temp_vals.forEach(nm =>
            output.push(`js_val ${nm};`));
        output.push(...temp_block_node.init_text);
        output.push(text + ';');
        return true;
    }
}

// ------------------------------------------------------------

function write_var_locals (func_stmt, block_stmt_list) {

    // collect all local variables declared with 'var',
    // into a 'let' declaration statement at the top of the
    // function.  initialize all such locals to 'undefined'.

    var let_stmt = {
        type: 'VariableDeclaration',
        declarations: [],
        kind: 'let',
        parent_node: func_stmt.body,
    };

    for (var [name, node] of func_stmt.scope) {

        if (node.kind === 'var') {

            let_stmt.declarations.push({
                type: 'VariableDeclarator',
                id: node.id, unique_id: node.unique_id,
                init: utils_c.init_expr_undefined,
                // special case for variable_declaration ()
                is_closure_var_init: node.is_closure,
            });
        }
    }

    // in non-strict mode, it is legal to declare a function
    // immediately within an 'if' statement, i.e. without a
    // wrapping block:  if (true) function (...)
    // so we need to hoist that variable declaration to the
    // top of the function, as if a 'var' declaration.

    if (!func_stmt.strict_mode) {

        func_stmt.visit('FunctionDeclaration', node => {

            if (node.parent_node?.type === 'IfStatement') {

                let_stmt.declarations.push({
                    type: 'VariableDeclarator',
                    id: node.id, unique_id: node.unique_id,
                    init: utils_c.init_expr_undefined,
                    is_closure: node.is_closure,
                });
            }
        });
    }

    //
    // write variable declarations
    //

    if (let_stmt.declarations.length)
        block_stmt_list.unshift(let_stmt);
}

// ------------------------------------------------------------

function count_max_args_in_call_stmt (func) {

    let max_args = -1;

    const f = (node, recursive_arg_count) => {

        if (node.type === 'CallExpression'
        ||  node.type === 'NewExpression') {

            recursive_arg_count += node.arguments.length
                + 1; // for the func object

            if (recursive_arg_count > max_args)
                max_args = recursive_arg_count;

            node.arguments.forEach(sub_node =>
                f(sub_node, recursive_arg_count));

        } else {

            utils.get_child_nodes(node).forEach(sub_node =>
                f(sub_node, recursive_arg_count));
        }
    }

    f(func, 0);
    return max_args;
}

// ------------------------------------------------------------

exports.write_module_entry_point = function (func, output) {

    // the actual program entry point (in runtime.c) does not
    // know all the information about the function it has to
    // call:  if strict mode, number of shape cache slots,
    // number of closure slots.  so it cannot call js_newfunc
    // properly.  this generated support function creates and
    // returns the function object for the entry point func.

    if (func?.type !== 'FunctionDeclaration')
        throw [ func, 'expected main function' ];
    output.push('');
    output.push('// ------------------------------------------------------------');
    output.push('js_val js_main(js_c_func_args){');
    output.push('init_literals(env);');
    const blk = {   type: 'BlockStatement',
                    parent_node: func,
                    inhibit_braces: true,
                    scope: new Map(), temp_vals: [] };
    const stmt = { type: 'ReturnStatement',
                   argument: { type: 'FunctionExpression',
                               decl_node: func,
                               parent_node: blk } };
    blk.body = [ stmt ];
    write_statements(blk, output);
    output.push('}');
}

// ------------------------------------------------------------

exports.function_expression = function (expr) {

    const decl_node = expr.decl_node;
    const func_name = utils_c.get_variable_c_name(decl_node);
    const func_descr = expr.id?.c_name || 'js_undefined';
    const func_where = utils_c.make_function_where(decl_node);

    let params_length = decl_node.params.length || 0;
    if (params_length > 0 && 'RestElement' ===
            decl_node.params[params_length - 1].type)
        params_length--;
    if (params_length >= 0x40000000) {
        // number of parameters must not interfere with
        // any flag bits that share the 'arity' parameter
        throw [ expr, 'too many parameters' ];
    }

    let arity = '' + params_length;
    if (decl_node.strict_mode)
        arity = 'js_strict_mode|' + arity;
    if (decl_node.not_constructor) {
        // arrow/generator/async functions
        arity = 'js_not_constructor|' + arity;
    }

    // number of shape cache variables required,
    // determined by get_shape_variable () in utils_c.js
    if (decl_node.shape_cache_count >= 0xFFFF)
        throw [ expr, 'too many shape cache slots' ];

    const shapes = `(${(decl_node.shape_cache_count || 0)}<<16)|`;
    // number of closure slots, and pointers to local variables
    const func_node = utils.get_parent_func_node(expr);
    const closure = do_closures(decl_node.closures, expr, func_node);

    // use js_newfunc to build the new function object
    let text = `js_newfunc(env,${func_name},${func_descr},${func_where},`
         + `${arity},${shapes}${closure})`;

    // invoke text editing callbacks, e.g. from
    // process_constructor_shape ()
    if (decl_node.text_callbacks) {
        for (const cb of decl_node.text_callbacks)
            text = cb(expr, decl_node, text);
    }

    return text;

    function do_closures (closures, expr_node, func_node) {
        if (!closures)
            return '0';
        const results = [];
        for (let [decl_node, ref_node] of closures) {
            results.push((ref_node.closure_func === func_node)
                ? local_declared_in_this_func(decl_node, expr_node)
                : import_local_from_outer_func(decl_node, expr_node, func_node));
        }
        if (results.length >= 0xFFFF)
            throw [ expr_node, 'too many closure slots' ];
        return '' + results.length + ',' + results.join(',');
    }

    function local_declared_in_this_func (decl_node, expr_node) {
        // nested function refers to a local declared in this
        // function;  which would already be a pointer, thanks
        // to variable_declaration () in statement_writer.js.
        if (!decl_node.is_func_node)
            return utils_c.get_variable_c_name(decl_node);
        // a special exception when the nested function refers
        // to the internal function name, i.e. the name to the
        // right of the 'function' keyword in this expression:
        //      let var_in_parent_scope = function internal_name ()
        // in this case, we allocate a closure variable to hold
        // func_val, because func_val is probably on the stack.
        return `js_newclosure(env,&func_val)`;
    }

    function import_local_from_outer_func (decl_node, expr_node, func_node) {
        // nested function refers to a variable that is declared
        // in the parent scope.  this would already be taken care
        // of by add_closure_function () in variable_resolver.js,
        // so we just need to forward the closure references.
        const passthrough = func_node.closures.get(decl_node);
        if (passthrough) {
            // note the special flag to indicate an uninitialized
            // closure variable is allowed here, see also below,
            // closure_expression () and js_closureval () in func.c
            return exports.closure_expression(
                passthrough.closure_index, expr_node, true);
        } else
            throw [ expr_node, 'invalid closure reference' ];
    }
}

// ------------------------------------------------------------

exports.closure_expression = function (closure_index, node,
                                       passthrough_mode) {

    // it is possible for a closure variable to be referenced
    // before it was initialized to any value: for example:
    //      (() => { (() => v)(); let v; })()
    // to support this, func->closure_array has two sets of
    // closure pointers;  see js_newfunc () in func.c.  also
    // from func.c, js_closureval () copies a closure pointer
    // from the second set of pointers to the primary set,
    // thus "activating" the pointer, if its initial value is
    // set.  the final part in this process is to allocate
    // closure variables with a special value that can detect
    // they have not yet been initialized.  this is done by
    // by js_newclosure (), see also variable_declaration ()
    // in statement_writer.js

    let idx = closure_index;
    let pfx = passthrough_mode ? '~' : '';

    const tmp = `ptr_val_${utils.get_unique_id()}`;
    const fnc = '((js_func*)js_get_pointer(func_val))';
    utils_c.insert_init_text(node, `js_val *${tmp};`);
    node.closure_temp_ptr = tmp;
    return `(${tmp}=`
         + `(likely((${tmp}=${fnc}->closure_array[${idx}])!=NULL)`
         +  `?${tmp}:js_closureval(env,func_val,${pfx}${idx})))`;
}

// ------------------------------------------------------------

exports.meta_property_expression = function (expr) {

    if (expr.meta.type     === 'Identifier'
    &&  expr.meta.name     === 'new'
    &&  expr.property.type === 'Identifier'
    &&  expr.property.name === 'target') {

        const func_node = utils.get_parent_func_node(expr);
        let c_name = func_node.new_target_c_name;
        if (!c_name) {

            c_name = func_node.new_target_c_name =
                    `new_target_${utils.get_unique_id()}`;

            const init_text =
                    `js_val ${c_name}=env->new_target;`;
            utils_c.insert_init_text(expr, init_text);
        }

        return c_name;
    }

    throw [ expr, 'invalid meta property' ];
}

// ------------------------------------------------------------

exports.convert_to_coroutine = function (func) {

    func.not_constructor = true;
    func.inject_call_to_yield = true;

    let kind = 0;
    if (func.async)
        kind += 1;
    if (func.generator)
        kind += 2;

    utils_c.text_callback(func,

        (call_node, decl_node, text) =>

            `js_newcoroutine(env,${kind},${text})`
        );
}
