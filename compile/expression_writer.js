
// ------------------------------------------------------------

const function_writer = require('./function_writer');
const utils = require('./utils');
const utils_c = require('./utils_c');

// ------------------------------------------------------------

const expression_writers = {

    'Literal': literal_expression,

    'Identifier': identifier_expression,
    'ThisExpression': identifier_expression,

    'AssignmentExpression': assignment_expression,

    'CallExpression': call_expression,

    'NewExpression': new_expression,

    'SequenceExpression': sequence_expression,

    'ObjectExpression': object_expression,

    'ArrayExpression': array_expression,

    'UnaryExpression': unary_expression,

    'BinaryExpression': binary_expression,

    // delayed initialization for the following
    // 'FunctionExpression': function_writer.function_expression
    // 'ClosureExpression': function_writer.closure_expression
    // 'MemberExpression': property_writer.member_expression,
    // 'DeleteExpression': property_writer.delete_expression,
    // 'UpdateExpression': update_writer.update_expression,
    // 'CompareExpression': compare_writer.compare_expression,
    // 'TypeofExpression': compare_writer.typeof_expression,
    // 'LogicalExpression': compare_writer.logical_expression,
    // 'ConditionalExpression': compare_writer.condition_expression,
};

// ------------------------------------------------------------

function literal_expression (expr) {

    if (expr.c_name)
        return expr.c_name;

    if (expr.value === undefined)
        return 'js_undefined';

    if (expr.value === null)
        return 'js_null';

    if (expr.value === false)
        return 'js_false';

    if (expr.value === true)
        return 'js_true';

    if (typeof(expr.value) === 'number') {

        if (expr.value != expr.value)
            return 'js_nan';

        let num_str = expr.value.toString();
        if (num_str.indexOf('.') == -1) {

            let e_idx = num_str.indexOf('e');
            if (e_idx !== -1) {
                num_str = num_str.slice(0, e_idx)
                        + '.0'
                        + num_str.slice(e_idx);
            } else
                num_str += '.0';
        }

        if (utils_c.is_negated_literal(expr))
            num_str = '-' + num_str;

        num_str = `js_make_number(${num_str})`;
        return num_str;
    }

    throw `unknown literal value ${typeof(expr.value)}`;
}

// ------------------------------------------------------------

function identifier_expression (expr) {

    if (expr.type === 'ThisExpression')
        return 'this_val';

    // a reference to a closure variable that is declared in
    // an enclosing function.  the 'closures' array contains
    // pointers to such variables

    if (expr.is_closure) {

        const f = expression_writers['ClosureExpression'];
        return '(*' + f(expr.closure_index, expr) + ')';
    }

    // resolve_local_reference () in function_splitter.js
    // indicated if this is a static property name

    if (!expr.is_property_name) {

        // local identifier should have a 'decl_node' link
        // to the declaring node, where a 'c_name' property
        // specifies the translated variable name in C

        expr = expr.decl_node;

        if (expr.is_func_node)
            return 'func_val';

        if (expr.is_closure)
            return '*' + expr.c_name;
    }

    // local variables and property names should have 'c_name'
    return expr.c_name || utils_c.get_variable_c_name(expr);
}

// ------------------------------------------------------------

function unary_expression (expr) {

    if (expr.operator === 'delete') {

        const f = expression_writers['DeleteExpression'];
        return f(expr);
    }

    if (expr.operator === 'void') {
        return '(' + expression_writer(expr.argument)
                   + ',js_undefined)';
    }

    if (expr.operator === 'typeof') {

        const f = expression_writers['TypeofExpression'];
        return f(expr);
    }

    if (expr.operator === '!') {

        const f = expression_writers['LogicalExpression'];
        return f(expr);
    }

    if (expr.operator === '+' ||  expr.operator === '-'
    ||  expr.operator === '~') {

        if (utils_c.is_negated_literal(expr))
            return literal_expression(expr.argument);

        const f = expression_writers['UpdateExpression'];
        return f(expr);
    }

    throw [ expr, 'bad unary expression' ];
}

// ------------------------------------------------------------

function binary_expression (expr) {

    // if numeric operator, including bitwise operators,
    // and the string concat variant of the (+) operator
    if (update_expression_operators
                            .indexOf(expr.operator) != -1) {

        const f = expression_writers['UpdateExpression'];
        return f(expr);
    }

    if (compare_expression_operators
                            .indexOf(expr.operator) != -1) {

        const f = expression_writers['CompareExpression'];
        return f(expr);
    }

    throw [ expr, 'bad binary expression' ];
}

const update_expression_operators = [
    '+', '-', '*', '/', '%', '**',
    '|', '^', '&', '<<', '>>', '>>>',
    '+=', '-=', '*=', '/=', '%=', '**=',
    '|=', '^=', '&=', '<<=', '>>=', '>>>=',
];

const compare_expression_operators = [
    '==', '===', '!=', '!==',
    '<', '>', '<=', '>=',
    'instanceof', 'in',
];

// ------------------------------------------------------------

function assignment_expression (expr) {

    let op = expr.operator;
    if (op !== '=') {

        if (op === '&&=' || op === '||=' || op === '??=') {

            throw [ expr, 'bad assignment expression' ];
        }

        // any other assignment operator, e.g. +=
        const f = expression_writers['UpdateExpression'];
        return f(expr);
    }

    let left = expr.left;
    if (left.type === 'Identifier') {

        if (left.is_const)
            throw [ left, 'assignment to constant variable' ];

        left = expression_writer(left, false);
        const right = expression_writer(expr.right);

        return `${left}=${right}`;

    } else if (left.type === 'MemberExpression') {

        const f = expression_writers['MemberExpression'];
        return f(left, expr.right);
    }

    throw [ expr, `unexpected ${left.type} as l-value in assignment` ];
}

// ------------------------------------------------------------

function write_call_arguments (expr) {

    let stk_ptr = expr.stk_ptr = utils_c.alloc_temp_stack(expr);
    let stk_next = `,js_stk_top=js_stk_top->next,`;
    let text = `${stk_ptr}=js_stk_top${stk_next}`;

    for (var arg_expr of expr.arguments) {

        if (arg_expr.type === 'SpreadElement') {

            let spread_text =
                expression_writer(arg_expr.argument);
            text += `js_spreadargs(env,${spread_text}),`;
            continue;
        }

        let arg_text = expression_writer(arg_expr);

        // if argument expression is non-trivial, calculate
        // it into a temp var, to make sure the C compiler
        // has a clear order of evaluation of expressions
        if (!utils.is_basic_expr_node(arg_expr)) {
            let arg_tmp = utils_c.alloc_temp_value(arg_expr);
            text += `${arg_tmp}=${arg_text},`;
            arg_text = arg_tmp;
        }

        text += `js_stk_top->value=${arg_text}${stk_next}`;
    }

    return text;
}

// ------------------------------------------------------------

function write_func_name_hint (expr) {

    let error_hint;
    let callee = expr.callee;

    if (callee.type === 'MemberExpression') {

        // in case of error, try to identify the string
        // literal that was used to index the object.
        // note that this is a valid js string object,
        // generated by the logic in literal.js

        if (callee.property.c_name)
            error_hint = callee.property.c_name;

    } else if (callee.type === 'Identifier'
            && typeof(callee.name) === 'string') {

        // we don't have a js string that holds the name
        // of a local identifier, so we generate an ascii
        // string.  js_throw_func_name_hint () knows that
        // a number value must be an ascii string pointer.
        error_hint = '((js_val){.raw=(uint64_t)"'
                   + callee.name + '" })';
    }

    return (!error_hint) ? ''
            : `env->func_name_hint=${error_hint},`;
}

// ------------------------------------------------------------

function call_expression (expr) {

    let text = '';

    // if the function object is reached via property
    // lookup, the container object is the 'this' value.

    let callee = expr.callee;
    let callee_object;
    let func_var;

    let with_call = false;
    let this_var = 'js_undefined';
    if (callee.type === 'MemberExpression') {

        let this_expr = expression_writer(
                callee_object = callee.object);

        if (this_expr ===
                utils_c.env_global_decl_node.as_expr) {

            // call of an unqualified global identifier,
            // the 'this' object remains 'undefined'
            callee_object = undefined;

            // an unqualified, non-indexed call in non-strict
            // mode should consult the 'with' scope, and the
            // object containing the property should be passed
            // as 'this'.  see also js_callwith () in with.c
            with_call = !expr.strict_mode;
            func_var = expr.callee.property.c_name;

            func_var = utils_c.get_with_local2(callee, func_var);

        } else {

            callee.object = { type: 'Identifier',
                decl_node: { c_name: this_var =
                    utils_c.alloc_temp_value(expr) } };
            text += `${this_var}=${this_expr},`;
        }
    }

    // if the function object is an expression,
    // then cache it in a temporary variable

    if (!with_call) {

        func_var = expression_writer(expr.callee);
        if (!with_call && !utils.is_basic_expr_node(callee)) {

            let func_tmp = utils_c.alloc_temp_value(expr);
            text += `${func_tmp}=${func_var},`;
            func_var = func_tmp;
        }
    }

    // insert optional condition

    if (expr.optional || callee.optional) {

        text += '(unlikely(js_is_undefined_or_null(';
        if (!expr.optional) {
            // only callee ('this' object) is optional
            text += this_var;
        } else if (!with_call) {
            text += func_var;
            if (callee.optional) {
                text += ')||js_is_undefined_or_null('
                        + this_var;
            }
        }
        text += '))?js_undefined:(';
    }

    // call target with arguments

    text += write_call_arguments(expr);

    if (with_call) {

        text += write_func_name_hint(expr);
        text += `js_callwith(env,func_val,${func_var},${expr.stk_ptr})`;

    } else {

        text += `(likely(js_is_object(${func_var})&&js_obj_is_exotic(js_get_pointer(${func_var}),js_obj_is_function))`;
        text += `?((js_func*)js_get_pointer(${func_var}))->c_func:(`;
        text += write_func_name_hint(expr) + 'js_callfunc))';
        text += `(env,${func_var},${this_var},${expr.stk_ptr})`;
    }

    if (expr.optional || callee.optional)
        text += '))';

    if (callee_object)
        callee.object = callee_object;

    return text;
}

// ------------------------------------------------------------

function new_expression (expr) {

    // if the function object is an expression,
    // then cache it in a temporary variable

    let text = '';
    let callee = expression_writer(expr.callee);
    if (!utils.is_basic_expr_node(expr.callee)) {

        let tmp = utils_c.alloc_temp_value(expr.callee);
        text += `${tmp}=${callee},`;
        callee = tmp;
    }

    // call target with arguments

    text += write_call_arguments(expr);
    text += write_func_name_hint(expr);
    text += `js_callnew(env,${callee},${expr.stk_ptr})`;

    return text;
}

// ------------------------------------------------------------

function sequence_expression (expr) {

    let sequence = '';
    for (var seq_expr of expr.expressions)
        sequence += expression_writer(seq_expr) + ',';
    return sequence;
}

// ------------------------------------------------------------

function object_expression (expr) {

    let text = 'js_newobj(env,';
    text += expr?.shape?.c_name ?? 'env->shape_empty';

    // build a call to js_newobj () for all 'plain' value,
    // non-descriptor, literal properties in the shape

    let proto, extra = [];
    for (const prop of expr.properties) {
        if (prop.is_shape_property) {
            if (prop.kind === 'init')
                text += ',' + expression_writer(prop.value);
            else if (prop.kind === 'get'
                 ||  prop.kind === 'set') {
                // getter or setter requires js_newobj2 ()
                // but we still need a placeholder for the
                // corresponding value in the shape
                text += ',js_undefined';
                extra.push(prop);
            } else
                utils.throw_unexpected_property(expr);
        } else if (prop.is_proto_property)
            proto = prop;
        else
            extra.push(prop);
    }
    text += ')';

    // non-trivial properties require a secondary call to
    // js_newobj2 ().  this includes computed properties,
    // getters, setters, and the special __proto__ property

    if (proto || extra.length) {

        text = `js_newobj2(env,(${text}),`;
        if (proto)
            text += expression_writer(proto.value);
        else
            text += 'env->obj_proto_val';

        text += `,${extra.length}`;
        while (extra.length) {
            const prop = extra.shift();

            if (prop.type === 'SpreadElement') {
                // spread expression, defined as '...object'
                // or ...{ object expression }
                text += ',js_next_is_spread';
                text += ',' + expression_writer(prop.argument);
                continue;
            }

            if (prop.kind === 'get'
            &&  prop.value.type === 'FunctionExpression') {
                // getter, defined as - get prop() { ...}
                text += ',js_next_is_getter';

            } else if (prop.kind === 'set'
                   &&  prop.value.type === 'FunctionExpression') {
                // setter, defined as - set prop(val) { ... }
                text += ',js_next_is_setter';
            }

            // following with property name and property value
            text += ',' + expression_writer(prop.key);
            text += ',' + expression_writer(prop.value);
        }
        text += ')';
    }

    return text;
}

// ------------------------------------------------------------

function array_expression (expr) {

    // if this is a plain array expression which does not
    // include a spread element, then a single call to
    // js_newarr () in arr.c takes care of everything

    let found_spread = false;
    for (const elem of expr.elements) {
        if (elem?.type === 'SpreadElement') {
            found_spread = true;
            break;
        }
    }

    let text = 'js_newarr(env,';

    if (!found_spread) {

        text += expr.elements.length.toString();
        for (const elem of expr.elements) {
            const elem_text = !elem ? 'js_deleted'
                            : expression_writer(elem);
            text += ',' + elem_text;
        }
        return text + ')';
    }

    // otherwise we need a two-step process where we create
    // an empty array, push elements into it as if this was
    // a call to Array.push (), and then return the new array

    const tmp = utils_c.alloc_temp_value(expr);
    text = `${tmp}=${text},0),`;
    throw [ expr, 'to-impl along with call-spread '];
}

// ------------------------------------------------------------

function delayed_initialization (expr_type) {

    // because we are indirectly imported by
    // function_writer.js (via statement_writer.js),
    // function_writer.exports is still empty at the
    // time we initializae our expression_writers {}
    // above.  so we do delayed initialization.

    const function_writer = require('./function_writer.js');

    expression_writers['ArrowFunctionExpression'] =
                    function_writer.function_expression;

    expression_writers['FunctionExpression'] =
                    function_writer.function_expression;

    expression_writers['ClosureExpression'] =
                    function_writer.closure_expression;

    expression_writers['MetaProperty'] =
                    function_writer.meta_property_expression;

    const property_writer = require('./property_writer.js');

    expression_writers['ChainExpression'] =
                    property_writer.chain_expression;

    expression_writers['MemberExpression'] =
                    property_writer.member_expression;

    expression_writers['DeleteExpression'] =
                    property_writer.delete_expression;

    delayed_initialization = undefined;

    const update_writer = require('./update_writer.js');

    expression_writers['UpdateExpression'] =
                    update_writer.update_expression;

    const compare_writer = require('./compare_writer.js');

    expression_writers['CompareExpression'] =
                    compare_writer.compare_expression;

    expression_writers['TypeofExpression'] =
                    compare_writer.typeof_expression;

    expression_writers['LogicalExpression'] =
                    compare_writer.logical_expression;

    expression_writers['ConditionalExpression'] =
                    compare_writer.condition_expression;
}

// ------------------------------------------------------------

let expression_writer = module.exports =

                            function (expr, parentheses = true) {

    if (delayed_initialization)
        delayed_initialization();

    const f = expression_writers[expr.type];
    if (typeof(f) === 'function') {
        let text = f(expr);
        if (parentheses)
            text = `(${text})`;
        return text;
    }

    throw [ expr, `unknown expression type ${expr.type}` ];
}
