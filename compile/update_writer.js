
const expression_writer = require('./expression_writer');
const shape_cache = require('./shape_cache');
const utils = require('./utils');
const utils_c = require('./utils_c');

// ------------------------------------------------------------

function update_expression_generic (lhs, rhs, opr, cmd, with_mode) {

    let text = '(';

    //
    // [ lhs_obj = non-trivial left expression, ]
    //

    let lhs_obj;
    if (!with_mode) {

        lhs_obj = expression_writer(lhs.object);
        // use a temp var if lhs is a non-trivial expression
        if (!utils.is_basic_expr_node(lhs.object)) {
            const tmp = utils_c.alloc_temp_value(lhs);
            text += `(${tmp}=${lhs_obj}),`;
            lhs_obj = tmp;
        }
    }

    let lhs_prop = expression_writer(lhs.property);

    //
    // lhs_var = js_getprop(...) or js_getwith(...)
    //

    const lhs_var = utils_c.alloc_temp_value(lhs);
    text += `(${lhs_var}=`;
    if (with_mode) {
        const prop_var2 =
                utils_c.get_with_local2(lhs, lhs_prop);
        text += `js_getwith(env,func_val,${prop_var2})),`;
    } else
        text += `js_getprop(env,${lhs_obj},${lhs_prop},`
             +  `&env->dummy_shape_cache)),`;

    //
    // evaluate right-hand and calculate the expression
    //

    let [ calc_text, calc_var, postfix_text ] =
                update_expression_calc(lhs_var, rhs, opr, cmd);

    text += calc_text;

    if (cmd.indexOf('assign') != -1) {

        // assign the result back into the object

        if (with_mode) {
            const prop_var2 =
                    utils_c.get_with_local2(lhs, lhs_prop);
            text += `,js_setwith(env,func_val,${prop_var2},`
                 +  `${calc_var})`;
        } else
            text += `,js_setprop(env,${lhs_obj},${lhs_prop},`
                 +  `${calc_var},&env->dummy_shape_cache)`;
    }

    return (text + postfix_text + ')');
}

// ------------------------------------------------------------

function update_expression_object (lhs, rhs, opr, cmd) {

    const global_lookup = shape_cache.is_global_lookup(lhs);
    if (global_lookup && !lhs.strict_mode) {
        // unqualified property access in non-strict
        // mode should take 'with' into account,
        // so fall back to generic access
        return update_expression_generic(
                                lhs, rhs, opr, cmd, true);
    }

    const shape = shape_cache.get_shape_var(lhs);

    if (lhs.property.name === 'length') {
        return update_expression_array_length(
                                lhs.object, lhs.property,
                                rhs, opr, cmd, shape);
    }

    if (!shape) {
        return update_expression_generic(
                                lhs, rhs, opr, cmd);
    }

    //
    // [ lhs_obj = non-trivial left expression, ]
    //

    let text = '(';

    let lhs_obj = expression_writer(lhs.object);
    // use a temp var if lhs is a non-trivial expression
    if (!utils.is_basic_expr_node(lhs.object)) {
        const tmp = utils_c.alloc_temp_value(lhs);
        text += `(${tmp}=${lhs_obj}),`;
        lhs_obj = tmp;
    }

    let lhs_prop = expression_writer(lhs.property);

    //
    // lhs_var = (is_object && is_valid_shape_cache
    //                      && not_a_descriptor)
    //         ? object[shape_index] : js_getprop(...)
    //

    const lhs_var = utils_c.alloc_temp_value(lhs);
    text += `(${lhs_var}=likely(`;
    // can skip js_is_object(obj), if obj is env->global_obj
    if (!global_lookup)
        text += `js_is_object(${lhs_obj})&&`;
    // next check is for the cached shape id
    const obj_ptr = `((js_obj*)js_get_pointer(${lhs_obj}))`;
    text += `${obj_ptr}->shape_id==((uint64_t)${shape})>>32`;
    // make sure descriptor bit 31 is not set in shape cache
    text += `&&!(${shape}&0x80000000U))`;
    // get value directly from object, unless a descriptor
    const lhs_val = `${obj_ptr}->values[(uint32_t)${shape}]`;
    text += `?${lhs_val}:js_getprop(`
         +  `env,${lhs_obj},${lhs_prop},&${shape})),`;

    //
    // evaluate right-hand and calculate the expression
    //

    let [ calc_text, calc_var, postfix_text ] =
                update_expression_calc(lhs_var, rhs, opr, cmd);

    text += calc_text;

    if (cmd.indexOf('assign') != -1) {

        // assign the result back into the object,
        // directly via the shape cache, if the shape cache
        // is non-zero, and has bit 31 clear.  otherwise,
        // assign it indirectly via js_setprop ()

        text += `,((int32_t)(uint32_t)${shape}>0`
             +  `?(${lhs_val}=${calc_var}):js_setprop(env,`
             +  `${lhs_obj},${lhs_prop},${calc_var},&${shape}))`;
    }

    return (text + postfix_text + ')');
}

// ------------------------------------------------------------

function update_expression_array (lhs, rhs, opr, cmd) {

    //
    // [ lhs_obj = non-trivial left expression, ]
    //

    let text = '(';

    let lhs_obj = expression_writer(lhs.object);
    // use a temp var if lhs is a non-trivial expression
    if (!utils.is_basic_expr_node(lhs.object)) {
        const tmp = utils_c.alloc_temp_value(lhs);
        text += `(${tmp}=${lhs_obj}),`;
        lhs_obj = tmp;
    }

    const arr_obj = `((js_arr*)js_get_pointer(${lhs_obj}))`;

    //
    // [ lhs_prop = non-trivial left indexer, ]
    //

    let lhs_prop = expression_writer(lhs.property);
    // use a temp var if lhs is a non-trivial expression
    if (!utils.is_basic_expr_node(lhs.property)) {
        const tmp = utils_c.alloc_temp_value(lhs);
        text += `(${tmp}=${lhs_prop}),`;
        lhs_prop = tmp;
    }

    //
    // lhs_var = (is_object && arr->super.proto == fast_arr
    //                      && arr->length != -1
    //                      && is_integer(lhs_idx = indexer)
    //                      && lhs_idx < arr->length)
    // ? arr->values[lhs_idx] : lhs_idx = -1, js_getprop(...)
    //

    const lhs_idx = utils_c.alloc_temp_value(lhs);
    const lhs_var = utils_c.alloc_temp_value(lhs);
    text += `(${lhs_var}=likely(js_is_object(${lhs_obj})&&`
         +  `${arr_obj}->super.proto==env->fast_arr_proto&&`
         +  `${lhs_prop}.raw==js_make_number(${lhs_idx}.raw=`
         +  `(int64_t)js_get_number(${lhs_prop})).raw&&`
         +  `${arr_obj}->length!=-1U&&` // array w/o descriptors
         +  `${lhs_idx}.raw<${arr_obj}->length)`;
    const lhs_val = `${arr_obj}->values[${lhs_idx}.raw]`;
    text += `?${lhs_val}:(${lhs_idx}.raw=(uint64_t)-1,`
         +  `js_getprop(env,${lhs_obj},${lhs_prop},`
         +  `&env->dummy_shape_cache))),`;

    //
    // evaluate right-hand and calculate the expression
    //

    let [ calc_text, calc_var, postfix_text ] =
                update_expression_calc(lhs_var, rhs, opr, cmd);

    text += calc_text;

    if (cmd.indexOf('assign') != -1) {

        // assign the result back into the array,
        // directly via the indexer,
        // or indirectly via js_setprop ()

        text += `,(${lhs_idx}.raw!=(uint64_t)-1`
             +  `?(${lhs_val}=${calc_var}):js_setprop(env,`
             +  `${lhs_obj},${lhs_prop},${calc_var},`
             +  `&env->dummy_shape_cache))`;
    }

    return (text + postfix_text + ')');
}

// ------------------------------------------------------------

function update_expression_array_length (lhs_obj_node, lhs_prop,
                                         rhs, opr, cmd, shape) {

    //
    // [ lhs_obj = non-trivial left expression, ]
    //

    let text = '(';

    let lhs_obj = expression_writer(lhs_obj_node);
    // use a temp var if lhs is a non-trivial expression
    if (!utils.is_basic_expr_node(lhs_obj_node)) {
        const tmp = utils_c.alloc_temp_value(lhs_obj_node);
        text += `(${tmp}=${lhs_obj}),`;
        lhs_obj = tmp;
    }

    lhs_prop = expression_writer(lhs_prop);

    const arr_obj = `((js_arr*)js_get_pointer(${lhs_obj}))`;

    const lhs_val_arr = `${arr_obj}->length_descr[0]`;
    const lhs_val_obj = `${arr_obj}->super.values[(uint32_t)${shape}]`;

    //
    // lhs_var = (is_object && arr->super.proto == fast_arr)
    // ? tmp_len = arr->length, arr->length_descr[0]
    // : tmp_len = -1, is_object && is_valid_shape_cache
    //                           && not_a_descriptor
    //                  ? object[shape_index]
    //                  : js_getprop(...)
    //
    // but note, that we only need tmp_len if this is an
    // update or assignment, but not for a BinaryExpression
    //

    let lhs_len, lhs_len_text1, lhs_len_text2;
    if (cmd.indexOf('assign') != -1) {
        lhs_len = utils_c.alloc_temp_value(lhs_obj_node);
        lhs_len_text1 = `${lhs_len}.raw=${arr_obj}->length,`;
        lhs_len_text2 = `${lhs_len}.raw=(uint64_t)-1,`;
    } else
        lhs_len_text1 = lhs_len_text2 = '';

    const lhs_var = utils_c.alloc_temp_value(lhs_obj_node);
    text += `(${lhs_var}=likely(js_is_object(${lhs_obj})&&`
         +  `${arr_obj}->super.proto==env->fast_arr_proto)`;
    text += `?(${lhs_len_text1}${lhs_val_arr}):(${lhs_len_text2}`;

    if (shape) {
        text += `(${arr_obj}->super.shape_id==((uint64_t)${shape})>>32`
             +  `&&!(${shape}&0x80000000U)?${lhs_val_obj}:`;
    }
    text += `js_getprop(env,${lhs_obj},${lhs_prop},&`;
    if (shape)
        text += shape + ')))),';
    else
        text += 'env->dummy_shape_cache))),';

    //
    // evaluate right-hand and calculate the expression
    //

    let [ calc_text, calc_var, postfix_text ] =
                update_expression_calc(lhs_var, rhs, opr, cmd);

    text += calc_text;

    if (lhs_len) { // if this is an update or assignment

        // the result must be a valid integer array length
        // and must be larger than the current length.  note
        // that arr->length is -1 (unsigned) when the length
        // property is non-writable.

        // assign the result back into the array length descr,
        // or into the values array via the shape cache index,
        // or if neither applies, indirectly via js_setprop ()

        text += `,((${calc_var}.raw==js_make_number(`
             +  `(int64_t)js_get_number(${calc_var})).raw&&`
             +  `(int64_t)js_get_number(${calc_var})>${lhs_len}.raw)`
             +  `?((${arr_obj}->length=(uint32_t)js_get_number(`
             +  `${lhs_val_arr}=${calc_var})),${calc_var}):`;

        if (shape) {
            text += `((int32_t)(uint32_t)${shape}>0`
                 +  `?(${lhs_val_obj}=${calc_var}):`;
        }
        text += `js_setprop(env,${lhs_obj},${lhs_prop},${calc_var},&`;
        if (shape)
            text += shape + ')))';
        else
            text += 'env->dummy_shape_cache))';
    }

    return (text + postfix_text + ')');
}

// ------------------------------------------------------------

function update_expression_simple (lhs, rhs, opr, cmd) {

    //
    // if left-hand side is a non-numeric literal, then
    // we can skip conditions and just call js_binop ()
    //

    if (lhs.type === 'Literal'
    &&  typeof(lhs.value) !== 'number'
    &&  cmd.indexOf('postfix') === -1) {

        return binop_call(opr, expression_writer(lhs),
                               expression_writer(rhs));
    }

    //
    // [ lhs_var = non-trivial left expression, ]
    //

    let text = '(';

    let lhs_var = expression_writer(lhs);
    // use a temp var if lhs is a non-trivial expression
    if (!utils.is_basic_expr_node(lhs)) {
        const tmp = utils_c.alloc_temp_value(lhs);
        text += `(${tmp}=${lhs_var}),`;
        lhs_var = tmp;
    }

    //
    // evaluate right-hand and calculate the expression
    //

    let [ calc_text, calc_var, postfix_text ] =
                update_expression_calc(lhs_var, rhs, opr, cmd);

    text += calc_text;

    if (cmd.indexOf('assign') != -1) {

        if (lhs.is_const)
            throw [ lhs, 'update of constant variable' ];

        if (lhs.closure_temp_ptr)
            text += `,(*${lhs.closure_temp_ptr}`;
        else
            text += `,(${lhs_var}`;
        text += `=${calc_var})`;
    }

    return (text + postfix_text + ')');
}

// ------------------------------------------------------------

function update_expression_calc (lhs_var, rhs, opr, cmd) {

    let text = '';

    //
    // postfix_var = copy of lhs_var
    //

    let postfix_text = '';
    if (cmd.indexOf('postfix') !== -1) {

        let postfix_var = utils_c.alloc_temp_value(rhs);
        text += `(${postfix_var}=${lhs_var}),`;
        postfix_text = `,${postfix_var}`;
    }

    //
    // rhs_var = right-hand expression
    //

    let rhs_var = expression_writer(rhs);
    // use a temp var if rhs is a non-trivial expression
    if (!utils.is_basic_expr_node(rhs)) {
        const tmp = utils_c.alloc_temp_value(rhs);
        text += `(${tmp}=${rhs_var}),`;
        rhs_var = tmp;
    }

    //
    // if right-hand side is a non-numeric literal, then
    // we can skip conditions and just call js_binop ()
    //

    const skip_condition = (rhs.type === 'Literal'
                  && typeof(rhs.value) !== 'number');

    //
    // calc_var = is_number (lhs_var | rhs_var)
    //          ? lhs_var (op) rhs_var
    //          : js_binop(...)
    //
    // see also comment for macro js_are_both_numbers ()
    //

    const calc_var = utils_c.alloc_temp_value(rhs);
    text += `(${calc_var}=`;

    const square = get_square_power(opr, rhs);
    if (square || skip_condition) {

        if (square <= 2 && !skip_condition) {

            text += `js_is_number(${lhs_var})?js_make_number(`
                 +  `js_get_number(${lhs_var})`;
            if (square == 2)
                text += `*js_get_number(${lhs_var})`;
            text += '):';
        }

    } else if (opr === '|' || opr === '^' || opr === '&') {

        text += `js_are_both_numbers(${lhs_var},${rhs_var})`
             + `?js_make_number(`
             +  `(uint32_t)js_get_number(${lhs_var})${opr}`
             +  `((uint32_t)js_get_number(${rhs_var}))):`;

    } else if (opr === '<<' || opr === '>>' || opr === '>>>') {

        let cast, opr2;
        if (opr === '>>>') {
            opr2 = '>>';
            cast = '(uint32_t)';
        } else {
            opr2 = opr;
            cast = '(int32_t)';
        }

        text += `js_are_both_numbers(${lhs_var},${rhs_var})`
             + `?js_make_number(`
             +  `${cast}js_get_number(${lhs_var})${opr2}`
             +  `(31&(uint32_t)js_get_number(${rhs_var}))):`;

    } else if (opr === '%') {

        text += `js_are_both_numbers(${lhs_var},${rhs_var})`
             + `?js_make_number(fmod(`
             +  `js_get_number(${lhs_var}),`
             +  `js_get_number(${rhs_var}))):`;

    } else { // arithmetic

        text += `js_are_both_numbers(${lhs_var},${rhs_var})`
             + `?js_make_number(`
             +  `js_get_number(${lhs_var})${opr}`
             +  `js_get_number(${rhs_var})):`;
    }

    text += binop_call(opr, lhs_var, rhs_var) + ')';

    return [ text, calc_var, postfix_text ];

    //
    // get_square_power
    //

    function get_square_power (opr, exp_node) {

        if (opr !== '**')
            return 0;

        if (exp_node.type === 'Literal') {
            const square = exp_node.value;
            if (square === 1 || square === 2)
                return square;
        }

        return 9;
    }
}

// ------------------------------------------------------------

function binop_call (opr, lhs_text, rhs_text) {

    if (opr === '>>>')
        opr = '>3';
    let opr_val = '0x';
    for (let opr_idx = 0; opr_idx < opr.length; opr_idx++)
        opr_val += opr.charCodeAt(opr_idx).toString(16).padStart(2, '0');

    return `js_binary_op(env,${opr_val},${lhs_text},${rhs_text})`;
}

// ------------------------------------------------------------

function unary_expression (expr) {

    let text = '';

    let arg = expression_writer(expr.argument, false);
    // use a temp var if arg is a non-trivial expression
    if (!utils.is_basic_expr_node(expr.argument)) {
        const tmp = utils_c.alloc_temp_value(expr);
        text += `(${tmp}=${arg}),`;
        arg = tmp;
    }

    switch (expr.operator) {

        case '~':
            text += `likely(js_is_number(${arg}))?js_make_number(`
                 +  `(int32_t)~(int64_t)js_get_number(${arg}))`
                 +  `:js_unary_op(env,'~',${arg})`;
            break;

        case '-':
            text += `likely(js_is_number(${arg}))`
                 +  `?js_make_number(-js_get_number(${arg}))`
                 +  `:js_unary_op(env,'-',${arg})`;
            break;

        case '+':
            text += `likely(js_is_number(${arg}))?${arg}`
                 +  `:js_unary_op(env,'+',${arg})`;
            break;

        default:
            throw [ expr, 'unknown unary operator' ];
    }

    return text;
}

// ------------------------------------------------------------

exports.update_expression = function (expr) {

    let lhs, rhs, opr, cmd;

    //
    // the type of expression determines the parameters
    // to pass to whichever function we select
    //

    switch (expr.type) {

        case 'UnaryExpression':

            return unary_expression(expr);

        case 'BinaryExpression':

            lhs = expr.left;
            rhs = expr.right;
            opr = expr.operator;
            cmd = 'binary';
            break;

        case 'AssignmentExpression':

            lhs = expr.left;
            rhs = expr.right;
            opr = expr.operator.slice(0, -1);
            cmd = 'assign';
            break;

        case 'UpdateExpression':

            lhs = expr.argument;
            rhs = { type: 'Literal', value: 1,
                    parent_node: expr };
            opr = expr.operator[0]; // + or -
            cmd = 'assign';
            if (!expr.prefix
            &&  !expr.parent_node.void_result)
                cmd += ',postfix';
            break;

        default:

            throw [ expr, 'bad update expression' ];
    }

    //
    // the type of the left-hand side determines
    // which function is called
    //

    if (lhs.type !== 'MemberExpression') {

        return update_expression_simple(lhs, rhs, opr, cmd);
    }

    if (lhs.computed) {

        const prop = utils_c.literal_property_name(lhs);
        if (!prop) {

            return update_expression_array(
                            lhs, rhs, opr, cmd);
        }

        if (prop === 'length') {

            return update_expression_array_length(
                            lhs.object, lhs.property,
                            rhs, opr, cmd);
        }
    }

    return update_expression_object(lhs, rhs, opr, cmd);
}
