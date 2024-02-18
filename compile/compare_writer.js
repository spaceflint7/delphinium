
const expression_writer = require('./expression_writer');
const utils = require('./utils');
const utils_c = require('./utils_c');

// ------------------------------------------------------------

function compare_equality (expr) {

    let text;

    // a shortcut typeof check is possible when comparing
    // the result of typeof to a specific type string
    if (is_typeof(expr.left) || is_typeof(expr.right)) {

        text = exports.typeof_expression(expr);
    }

    if (!text) {

        // otherwise value comparison, which may involve
        // a call to the runtime, when neither value is
        // known.  but if one of the value is literal,
        // in some cases it may be possible to inline
        // equality test without making a function call.

        let left = expression_writer(expr.left);
        let right = expression_writer(expr.right);

        if (expr.operator.length === 2) {

            text = inline_compare(expr.left, expr.right,
                                  left, right, true);
            if (!text) {

                let suffix;
                if (!utils.is_basic_expr_node(expr.left)) {
                    // if left is not a trivial expression,
                    // it may cause a side-effect on right,
                    // so evaluate left before function call
                    const tmp = utils_c.alloc_temp_value(expr);
                    text = `((${tmp}=${left}),`;
                    left = tmp;
                    suffix = ')';

                } else
                    text = suffix = '';

                // loose equality
                text += `js_loose_eq(env,${left},${right})${suffix}`;
            }

        } else {

            text = inline_compare(expr.left, expr.right,
                                  left, right);
            if (!text) {

                // strict equality
                text = strict_equality(expr, left, right,
                                    operand_flags(expr));
            }
        }
    }

    let negate = (expr.operator[0] === '!');
    return convert_c_to_js_boolean(expr, text, negate);

    //
    // strict equality, when either side is a literal of
    // a type that can compare directly, can save a call
    //

    function inline_compare (left_node, right_node,
                             left_text, right_text,
                             is_loose_equality) {

        let chk, lit_val;
        let lit_type = is_literal(left_node);
        if (lit_type) {
            lit_val = left_node.value;
            chk = right_text;
        } else {
            lit_type = is_literal(right_node);
            if (lit_type) {
                lit_val = right_node.value;
                chk = left_text
            }
        }

        if (is_loose_equality) {

            const is_undefined = (lit_type === 'undefined');
            const is_null =
                (lit_type === 'object' && lit_val === null);

            if (is_undefined || is_null)
                return `js_is_undefined_or_null(${chk})`;

        } else {

            if (lit_type === 'undefined')
                return `js_is_undefined(${chk})`;

            if (lit_type === 'object' && lit_val === null)
                return `${chk}.raw==js_null.raw`;

            if (lit_type === 'boolean' && lit_val === true)
                return `${chk}.raw==js_true.raw`;

            if (lit_type === 'boolean' && lit_val === false)
                return `${chk}.raw==js_false.raw`;

            if (lit_type === 'number') {
                if (lit_val != lit_val)
                    lit_val = 'NAN';
                return `${chk}.num==${lit_val}`;
            }
        }
    }

    //
    // check if node is a typeof expression
    //

    function is_typeof (node) {
        return node.type === 'UnaryExpression'
            && node.operator === 'typeof';
    }

    //
    // check type of literal node
    //

    function is_literal (node) {
        if (node.type === 'TemplateLiteral')
            return 'string';
        if (node.type === 'Literal' && node.value)
            return typeof(node.value);
    }

    //
    // prepare operand flags
    //

    function operand_flags (expr) {

        let any_string_operand,
            non_number_operand;

        let type = is_literal(expr.left);
        if (type && type !== 'number') {
            non_number_operand = true;
            any_string_operand = (type === 'string')
                            || is_typeof(expr.left);
        } else {
            type = is_literal(expr.right);
            if (type && type !== 'number') {
                non_number_operand = true;
                any_string_operand = (type === 'string')
                            || is_typeof(expr.right);
            }
        }

        return { any_string_operand,
                 non_number_operand };
    }

    //
    // strict equality
    //
    // before actually calling the runtime, we can still
    // do a quick test to determine if we can compare by
    // reference instead of calling js_equals ()
    //

    function strict_equality (expr, left, right, flags) {

        let text = '(';

        // use a temp var if lhs is a non-trivial expression
        if (!utils.is_basic_expr_node(expr.left)) {
            const tmp = utils_c.alloc_temp_value(expr.left);
            text += `(${tmp}=${left}),`;
            left = tmp;
        }
        // use a temp var if rhs is a non-trivial expression
        if (!utils.is_basic_expr_node(expr.right)) {
            const tmp = utils_c.alloc_temp_value(expr.right);
            text += `(${tmp}=${right}),`;
            right = tmp;
        }

        if (left === right) {

            // if both operand are same, we still have to
            // do the comparison, in case the value is NaN
            if (!flags.non_number_operand) {
                text += `js_is_number(${left})`
                     +  `?(${left}.num==${right}.num):`;
            }
            text += 'true)';

        } else {

            // check if we can do strict equality inline, skipping
            // the call to js_equals.  if both inputs are numbers,
            // compare inline as numbers.  otherwise, we check if
            // either value is (1) a primitive object, which should
            // be compared by value rather than by reference, or
            // (2) js_deleted, which must be equal to undefined.
            //
            // note that the bit which we use to detect the special
            // js_deleted value (also used in js_is_flagged_pointer)
            // should not be set in any 'user' objects.
            //
            // also note that technically, symbol primitives can be
            // compared by reference, but a compromise is made in
            // order to keep this test simple for the common case.
            //

            if (!flags.non_number_operand) {
                // if either operand is known to be not a number,
                // then skip checking that both are numbers
                text += `js_are_both_numbers(${left},${right})`
                     +  `?(${left}.num==${right}.num):`;
            }
            if (!flags.any_string_operand) {
                // if either operand is known to be a string,
                // then skip checking that either is a primitive
                text += `(unlikely(((${left}.raw|${right}.raw)&(js_primitive_bit|2))==0)`
                     +  `?(${left}.raw==${right}.raw):`;
            }
            text += `js_strict_eq(env,${left},${right}))`;
            if (!flags.any_string_operand)
                text += ')';
        }

        return text;
    }
}

// ------------------------------------------------------------

function compare_relational (expr) {

    let text = '';

    let lhs = expression_writer(expr.left);
    // use a temp var if lhs is a non-trivial expression
    if (!utils.is_basic_expr_node(expr.left)) {
        const tmp = utils_c.alloc_temp_value(expr.left);
        text += `(${tmp}=${lhs}),`;
        lhs = tmp;
    }

    let rhs = expression_writer(expr.right);
    // use a temp var if rhs is a non-trivial expression
    if (!utils.is_basic_expr_node(expr.right)) {
        const tmp = utils_c.alloc_temp_value(expr.right);
        text += `(${tmp}=${rhs}),`;
        rhs = tmp;
    }

    text += `js_are_both_numbers(${lhs},${rhs})`
         +  `?(js_get_number(${lhs})${expr.operator}js_get_number(${rhs}))`
         +  ':js_less_than(env,';

    switch (expr.operator) {

        // flag bits:  bit 0 set (value 1), to evaluate
        // left-hand side (third parameter) before
        // right-hand (fourth parameter), otherwise
        // right-hand side is evaluated first.
        // bit 4 set (value 0x10) if negate the result
        // when neither operand is a NaN value.
        // see ECMA section 13.10 Relational Operators
        case '<':   text += `0x01,${lhs},${rhs})`;  break;
        case '>':   text += `0x00,${rhs},${lhs})`; break;
        case '<=':  text += `0x10,${rhs},${lhs})`; break;
        case '>=':  text += `0x11,${lhs},${rhs})`; break;
    }

    return convert_c_to_js_boolean(expr, text, false);
}

// ------------------------------------------------------------

function is_property_in_object (expr) {

    // make sure property is evaluated before object,
    // per the requirements of the 'in' operator.
    let text = '';
    let prop = expression_writer(expr.left);
    // use a temp var if lhs is a non-trivial expression
    if (!utils.is_basic_expr_node(expr.left)) {
        const tmp = utils_c.alloc_temp_value(expr.left);
        text += `(${tmp}=${prop}),`;
        prop = tmp;
    }

    let obj = expression_writer(expr.right);
    text += `js_hasprop(env,${obj},${prop})`;
    return convert_c_to_js_boolean(expr, text, false);
}

// ------------------------------------------------------------

function is_instance_of_function (expr) {

    let text = 'js_instanceof(env,'
             + expression_writer(expr.left) + ','
             + expression_writer(expr.right) + ')';
    return convert_c_to_js_boolean(expr, text, false);
}

// ------------------------------------------------------------

exports.compare_expression = function (expr) {

    if (expr.type === 'BinaryExpression') {

        switch (expr.operator) {

            case '===': case '!==': case '==': case '!=':
                return compare_equality(expr);

            case '<': case '>': case '<=': case '>=':
                return compare_relational(expr);

            case 'in':
                return is_property_in_object(expr);

            case 'instanceof':
                return is_instance_of_function(expr);
        }
    }

    throw [ expr, `unknown expression type ${expr.type}` ];
}

// ------------------------------------------------------------

exports.typeof_expression = function (expr) {

    //
    // unless typeof is an operand for equality comparison
    // (see below), we need to get an actual type string
    //

    if (expr.type === 'UnaryExpression') {

        let text = expression_writer(expr.argument);
        return `js_typeof(env,${text})`;
    }

    //
    // if we get here, it means that one of the operands
    // of an equality comparison is typeof, see also
    // compare_equality () above.  check if the other
    // operand is a string for a known type
    //

    let val_arg;
    let str_arg = is_simple_string(expr.left);
    if (str_arg)
        val_arg = expr.right;
    else {
        str_arg = is_simple_string(expr.right);
        if (str_arg)
            val_arg = expr.left;
    }

    if (str_arg !== 'bigint'
    &&  str_arg !== 'boolean'
    &&  str_arg !== 'function'
    &&  str_arg !== 'number'
    &&  str_arg !== 'object'
    &&  str_arg !== 'string'
    &&  str_arg !== 'symbol'
    &&  str_arg !== 'undefined')
        return;

    //
    // evaluate the argument and test its type
    //

    let text = '';

    val_arg = val_arg.argument;
    let chk = expression_writer(val_arg);
    // use a temp var if lhs is a non-trivial expression
    if (!utils.is_basic_expr_node(val_arg)) {
        const tmp = utils_c.alloc_temp_value(val_arg);
        text += `(${tmp}=${chk}),`;
        chk = tmp;
    }

    switch (str_arg) {

        case 'bigint':
        case 'string':
        case 'symbol':

            text += `js_is_primitive_${str_arg}(${chk})`;
            break;

        case 'boolean':
        case 'number':
        case 'undefined':

            text += `js_is_${str_arg}(${chk})`;
            break;

        case 'function':

            text += `js_is_object(${chk})&&`
                 +  `js_obj_is_exotic(`
                 +  `js_get_pointer(${chk}),`
                 +  `js_obj_is_function)`;
            break;

        case 'object':

            text += `((js_is_object(${chk})&&`
                 +  `!js_obj_is_exotic(`
                 +  `js_get_pointer(${chk}),`
                 +  `js_obj_is_function))||`
                 +  `${chk}.raw==js_null.raw)`;
            break;
    }

    return text;

    //
    // return the node as a simple string, if possible
    //

    function is_simple_string (node) {

        if (node.type === 'Literal') {

            if (typeof(node.value) === 'string') {

                return node.value;
            }

        } else if (node.type === 'TemplateLiteral') {

            if (node.quasis.length === 1
            &&  node.expressions.length === 0) {

                return node.quasis[0].value.cooked;
            }
        }
    }
}

// ------------------------------------------------------------

exports.logical_expression = function (expr) {

    let text = '(';

    if (expr.operator === '!') {
        // in this case we are called by unary_expression ()
        // for what is actually a UnaryExpression node
        return logical_not_expression(expr);
    }

    // special case for checking for complementary literals
    const is_special_text = is_special_case(expr);
    if (is_special_text) {
        text = is_special_text;
        if (expr.parent_node.can_accept_c_bool)
            expr.result_is_c_bool = true;
        else
            text += `?js_true:js_false`;
        return text;
    }

    expr.can_accept_c_bool = true;
    let left = expression_writer(expr.left);

    if (expr.left.result_is_c_bool) {

        if (expr.operator === '??') {
            // this operator evaluates the right-hand side
            // only if the left-hand side is undefined or
            // null.  but in this path, the left-hand side
            // is a boolean result, so the right-hand side
            // never gets evaluated.
            text += left;

        } else {

            let right = expression_writer(expr.right);

            if (expr.right.result_is_c_bool) {

                // both sides of the expression evaluates to
                // an expression that produces a C boolean

                text += `(${left}${expr.operator}${right})`;

                if (expr.parent_node.can_accept_c_bool)
                    expr.result_is_c_bool = true;
                else
                    text += `?js_true:js_false`;

            } else {

                // the left side evaluates to a C boolean,
                // but the right side is more complicated

                switch (expr.operator) {

                    case '&&':
                        text += `${left}?${right}:js_false`;
                        break;

                    case '||':
                        text += `${left}?js_true:${right}`;
                        break;

                    default:
                        text = undefined;
                        break;
                }
            }
        }

    } else {

        //
        // left side evaluates to a js_val expression,
        // so we check using js_is_truthy / js_is_falsy
        //

        // use a temp var if left is a non-trivial expression
        if (!utils.is_basic_expr_node(expr.left)) {
            const tmp = utils_c.alloc_temp_value(expr);
            text += `(${tmp}=${left}),`;
            left = tmp;
        }

        expr.can_accept_c_bool = false;
        let right = expression_writer(expr.right);
        switch (expr.operator) {

            case '&&':
                text += `js_is_falsy(${left})?${left}:${right}`;
                break;

            case '||':
                text += `js_is_truthy(${left})?${left}:${right}`;
                break;

            case '??':
                text += `js_is_undefined_or_null(${left})`
                     +  `?${right}:${left}`;
                break;

            default:
                text = undefined;
                break;
        }
    }

    if (!text)
        throw [ expr, 'unknown logical operator' ];

    return text + ')';

    // check if comparing the same variable to two
    // complementary values, like undefined/null and
    // true/false, and replace that with a simple check.

    function is_special_case (expr) {

        // parent logical expression should be || or &&.
        let op1, op2, text;
        if (expr.operator === '||') {
            op1 = '==';
            op2 = '===';
            text = '';
        } else if (expr.operator === '&&') {
            op1 = '!=';
            op2 = '!==';
            text = '!';
        } else
            return;

        // two child expressions that check equality
        const c1 = expr.left;
        const c2 = expr.right;
        if (    (c1.operator !== op1 && c1.operator !== op2)
             || (c2.operator !== op1 && c2.operator !== op2))
            return;

        // when testing for null or undefined, we can allow
        // loose equality as well as strict equality.  but
        // for other primitives, loose equality may involve
        // calling functions on objects, so we require strict
        const strict_eq = (c1.operator.length == 3
                        && c2.operator.length == 3);

        // identifier must be same in both child expressions.
        // note that it must be an identifier, because other
        // kinds of lookup might end up calling user functions
        // which might have an effect on the second comparison.
        let var0, lit1, lit2;

        if (c1.left?.decl_node?.type === 'Identifier') {
            var0 = c1.left;
            lit1 = c1.right;

        } else if (c1.right?.decl_node?.type === 'Identifier') {
            var0 = c1.right;
            lit1 = c1.left;
        }

        if (lit1?.type === 'Literal') {
            if (var0.decl_node === c2.left.decl_node)
                lit2 = c2.right;
            else if (var0.decl_node === c2.right.decl_node)
                lit2 = c2.left;
        }
        if (lit2?.type !== 'Literal')
            return;

        const idx1 = special_case_literals.indexOf(
                                    lit1.raw || lit1.name);
        const idx2 = special_case_literals.indexOf(
                                    lit2.raw || lit2.name);
        if ((idx1 ^ idx2) <= 1) {

            if (idx1 <= 1)      // check null or undefined
                text += 'js_is_undefined_or_null';
            else if (strict_eq) // check boolean
                text += 'js_is_boolean';
            else
                return;

            text += '(' + expression_writer(var0, false) + ')';
            return text;
        }
    }
}

const special_case_literals = [ 'undefined', 'null', 'true', 'false' ];

// ------------------------------------------------------------

function logical_not_expression (expr) {

    // if the parent node can accept a C-language boolean,
    // then let this logical-NOT node propagate a c-bool
    // from its own child, if applicable.  for example:
    //      if (!(a==b))

    let text = expression_writer(expr.argument);

    if (expr.result_is_c_bool) {

        if (expr.parent_node.can_accept_c_bool)
            return text;
        return convert_c_to_js_boolean(expr, text, true);
    }

    if (expr.called_from_write_condition)
        return text;

    // use a temp var if left is a non-trivial expression
    let prefix = '';
    let suffix = '';
    if (!utils.is_basic_expr_node(expr.argument)) {
        const tmp = utils_c.alloc_temp_value(expr);
        prefix = '(' + tmp + '=' + text + ',';
        suffix = ')';
        text = tmp;
    }

    text = `js_is_falsy(${text})?js_true:js_false`;
    return prefix + text + suffix;
}

// ------------------------------------------------------------

exports.condition_expression = function (expr) {

    let text = exports.write_condition(expr.test);
    text += '?' + expression_writer(expr.consequent);
    text += ':' + expression_writer(expr.alternate);
    return text;
}

// ------------------------------------------------------------

function convert_c_to_js_boolean (expr, text, negate) {

    if (expr.parent_node.operator === '!') {
        // parent node is a unary logical NOT expression
        return convert_c_to_js_boolean(
                        expr.parent_node, text, !negate);
    }

    if (expr.parent_node.can_accept_c_bool) {

        // caller is evaluating the current expression
        // via write_condition (), see function below
        expr.result_is_c_bool = true;
        if (negate)
            text = `!(${text})`;

    } else {

        // if parent is unable to generate a C language
        // condition, as for example, a ForStatement
        // parent can, then we need to convert the C
        // boolean result: zero to js_false, non-zero
        // to js_true.  and taking unary negation into
        // account to negate the result.

        let res1 = 'js_true', res2 = 'js_false';
        if (negate)
            [ res1, res2 ] = [ res2, res1 ];

        text = `(${text})?${res1}:${res2}`;
    }

    return text;
}

// ------------------------------------------------------------

exports.write_condition = function (expr) {

    expr.parent_node.can_accept_c_bool = true;
    expr.called_from_write_condition = true;
    let text = expression_writer(expr);

    if (!expr.result_is_c_bool) {
        let cond = true;
        let expr2 = expr;
        while (expr2.operator === '!') {
            expr2 = expr2.argument;
            cond = !cond;
        }
        cond = cond ? 'truthy' : 'falsy';

        // use a temp var if left is a non-trivial expression
        let text1 = '';
        let text2 = text;
        let text3 = '';
        if (!utils.is_basic_expr_node(expr2)) {
            const tmp = utils_c.alloc_temp_value(expr);
            text1 = `((${tmp}=${text}),`;
            text2 = `(${tmp})`;
            text3 = ')';
        }

        text = `${text1}(js_is_${cond}${text2})${text3}`;
    }

    return text;
}
