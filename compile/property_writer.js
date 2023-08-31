
const expression_writer = require('./expression_writer');
const shape_cache = require('./shape_cache');
const utils = require('./utils');
const utils_c = require('./utils_c');

// ------------------------------------------------------------

exports.chain_expression = function (expr) {

    const expr0 = expr;
    expr = expr.expression;

    // a MemberExpression node should have optional = true
    // when it is a child of a ChainExpression node
    if (expr.type !== 'MemberExpression' || !expr.optional) {

        // the 'callee' sub-node of a CallExpression node
        // may also have optional = true, in ChainExpression
        if (expr.type !== 'CallExpression' ||
                !(expr.optional || expr.callee.optional)) {

            throw [ expr0, 'unexpected chain element' ];
        }
    }

    return expression_writer(expr);
}

// ------------------------------------------------------------

exports.member_expression = function (expr, set_expr) {

    if (expr.computed) {

        const prop = utils_c.literal_property_name(expr);
        if (!prop) {

            return member_expression_array(expr, set_expr);
        }

        if (prop === 'length') {

            set_expr ||= expr.optional && 'optional';
            return member_expression_array_length(
                    expr.object, expr.property, set_expr);
        }
    }

    return member_expression_object(expr, set_expr);
}

// ------------------------------------------------------------

function member_expression_generic (expr, set_expr) {

    const obj = expression_writer(expr.object);
    const prop = expression_writer(expr.property);

    if (utils_c.is_global_lookup_non_strict(obj, expr)) {

        // unqualified property access in non-strict mode.

        const prop_var2 = utils_c.get_with_local2(expr, prop);

        if (!set_expr) {

            if (expr.parent_node.type === 'UnaryExpression'
            &&  expr.parent_node.operator === 'typeof'
            &&  !expr.with_decl_node) {
                // prevent typeof (undefined_variable) from
                // throwing if the property is not found.
                // we use the 'with' fallback variable logic
                // for this, see also get_with_local2 () in
                // utils_c.js, and process_references () in
                // variable_resolver.js
                const tmp = utils_c.alloc_temp_value(expr);
                return `(${tmp}=js_undefined,js_getwith(`
                    +       `env,func_val,${prop},&${tmp}))`;
            }

            return `js_getwith(env,func_val,${prop_var2})`;
        }

        const set = expression_writer(set_expr);
        return `js_setwith(env,func_val,${prop_var2},${set})`;

    }

    // normal property access

    if (!set_expr) {

        let text = '';
        if (expr.optional) {

            // use a temp var if object is non-trivial
            if (!utils.is_basic_expr_node(expr.object)) {
                const tmp = utils_c.alloc_temp_value(expr);
                text += `${tmp}=${obj},`;
                obj = tmp;
            }

            text += `unlikely(js_is_undefined_or_null(${obj}))`
                 +  `?js_undefined:`;
        }

        text += `js_getprop(env,${obj},${prop},`
             +  '&env->dummy_shape_cache)';

        return text;
    }

    const set = expression_writer(set_expr);
    return `js_setprop(env,${obj},${prop},${set},`
         + '&env->dummy_shape_cache)';
}

// ------------------------------------------------------------

function member_expression_object (expr, set_expr) {

    const global_lookup = shape_cache.is_global_lookup(expr);
    if (global_lookup && !expr.strict_mode) {
        // unqualified property access in non-strict
        // mode should take 'with' into account,
        // so fall back to generic access
        return member_expression_generic(expr, set_expr);
    }

    const shape = shape_cache.get_shape_var(expr);
    if (!shape) {

        if (expr.property.name === 'length') {
            // accessing length on an object that is not
            // compatible with shape caching, but it may
            // be an array, with an easy-to-reach length
            set_expr ||= expr.optional && 'optional';
            return member_expression_array_length(
                    expr.object, expr.property, set_expr);
        }
        // property access is not compatible with shape
        // caching, so fall back to generic access
        return member_expression_generic(expr, set_expr);
    }

    let obj = expression_writer(expr.object);
    const prop = expression_writer(expr.property);

    // use a temp var if object is a non-trivial expression
    let init_obj = '';
    if (!utils.is_basic_expr_node(expr.object)) {
        const tmp = utils_c.alloc_temp_value(expr);
        init_obj = `${tmp}=${obj},`;
        obj = tmp;
    }

    let optional_suffix = '';
    if (expr.optional && !global_lookup && !set_expr) {
        init_obj += `unlikely(js_is_undefined_or_null(${obj}))`
                 +  `?js_undefined:(`;
        optional_suffix = ')';
    }

    // can skip js_is_object(obj), if obj is env->global_obj
    let check_txt = global_lookup ? ''
                  : `js_is_object(${obj})&&`;

    // next check is for the cached shape id
    const obj_ptr = `((js_obj*)js_get_pointer(${obj}))`;
    check_txt += `${obj_ptr}->shape_id==((uint64_t)${shape})>>32`;

    // assign value if this is a set expression
    let value1 = '';    // in conditional
    let value2 = '';    // as parameter
    let func = 'js_getprop';

    if (set_expr) {
        value1 = expression_writer(set_expr);

        // use a temp var if value is a non-trivial expression
        if (!utils.is_basic_expr_node(set_expr)) {
            const tmp = utils_c.alloc_temp_value(expr);
            init_obj += `${tmp}=${value1},`;
            value1 = tmp;
        }

        // set 'set_expr' to the translated expression,
        // for use by member_expression_array_length ()
        set_expr = value1;

        value2 = ',' + value1;  // as parameter
        value1 = '=' + value1;  // in conditional
        func = 'js_setprop';

        // add a check to make sure this is not a
        // read-only data descriptor
        check_txt += `&&!(${shape}&0x40000000U)`;

        // add a check to avoid propagating js_deleted
        check_txt += `&&(${set_expr}.raw!=js_deleted.raw)`;
    }

    // build the access text for a 'get' operation:   check
    // if the cached offset (which is the low 32-bits of
    // the cache value) has bit 31 set.  if so, the property
    // is a data descriptor, so it is actually a pointer to
    // the value; otherwise it is the actual value.
    // see also:  js_getprop () and js_setprop_object ()
    let access_txt = `(unlikely(${shape}&0x80000000U)`
                   + `?(*((js_val*)js_get_pointer(${obj_ptr}->values[${shape}&(~0xC0000000U)]))${value1})`
                   + `:(${obj_ptr}->values[(uint32_t)${shape}]${value1}))`;

    let array_txt = '';
    if (set_expr && !global_lookup
                 && expr.property.name === 'length') {

        array_txt = member_expression_array_length(
                        obj, expr.property, set_expr);
    }

    if (!set_expr && global_lookup
                  && expr.parent_node.type === 'UnaryExpression'
                  && expr.parent_node.operator === 'typeof') {
        // prevent typeof (undefined_variable) from throwing,
        // by clearing the bit in env->global_obj that makes
        // js_getprop () throw ReferenceError, see also there
        obj = `js_make_object(js_get_pointer(${obj}))`;
    }

    return `(${init_obj}${array_txt}(likely(${check_txt})?${access_txt}`
         + `:${func}(env,${obj},${prop}${value2},&${shape})))`
         + optional_suffix;
}

// ------------------------------------------------------------

function member_expression_array (expr, set_expr) {

    let obj = expression_writer(expr.object);
    let prop = expression_writer(expr.property);

    // use a temp var if object is a non-trivial expression
    let init_text = '';
    if (!utils.is_basic_expr_node(expr.object)) {
        const tmp = utils_c.alloc_temp_value(expr);
        init_text += `${tmp}=${obj},`;
        obj = tmp;
    }

    let suffix_text = '&env->dummy_shape_cache)))';
    if (expr.optional && !set_expr) {
        init_text += `unlikely(js_is_undefined_or_null(${obj}))`
                  +  `?js_undefined:(`;
        suffix_text += ')';
    }

    const arr_obj = `((js_arr*)js_get_pointer(${obj}))`;

    // use a temp var if property is a non-trivial expression
    if (!utils.is_basic_expr_node(expr.property)) {
        const tmp = utils_c.alloc_temp_value(expr);
        init_text += `${tmp}=${prop},`;
        prop = tmp;
    }
    const prop_u = `(uint64_t)js_get_number(${prop})`;

    // make sure the array prototype permits fast-path
    // optimization on array access, and that the indexer
    // is an integer whole number without a fraction part
    let check_array = `js_is_object(${obj})&&`
       + `${arr_obj}->super.proto==env->fast_arr_proto&&`
       + `${arr_obj}->length!=-1U`; // array w/o descriptors
    let check_text = `js_is_number(${prop})&&${check_array}`
       + `&&js_make_number(${prop_u}).raw==${prop}.raw&&`;

    if (set_expr) {

        //
        // set array index (or object property)
        // if the indexer is an integer number less than
        // js_arr->set_limit, then update arr->values[index],
        // as well as js_arr->get_limit, and the length
        // property within the length descriptor.
        // otherwise call the slower set-index function
        //

        let value = expression_writer(set_expr);
        // use a temp var if value is a non-trivial expression
        if (!utils.is_basic_expr_node(set_expr)) {
            const tmp = utils_c.alloc_temp_value(expr);
            init_text += `${tmp}=${value},`;
            value = tmp;
        }

        // add a check to avoid propagating js_deleted
        check_text += `(${value}.raw!=js_deleted.raw)&&`;

        return `(${init_text}(likely(${check_text}`
             + `${prop_u}<${arr_obj}->capacity)`
             + `?((${arr_obj}->length_descr[0].num=`
             + `(${arr_obj}->length=(${arr_obj}->length>${prop_u}`
                                 + `?${arr_obj}->length:${prop_u}+1U)))`
             + `,${arr_obj}->values[${prop_u}]=${value})`
             + `:js_setprop(env,${obj},${prop},${value},${suffix_text}`;

    } else {

        //
        // get array index (or object property)
        // if the indexer is an integer number less than
        // js_arr->get_limit, then select arr->values[index].
        // otherwise call the slower get-index function
        //

        return `(${init_text}(likely(${check_text}`
             + `${prop_u}<(int64_t)${arr_obj}->capacity)`
             + `?${arr_obj}->values[${prop_u}]`
             + `:js_getprop(env,${obj},${prop},${suffix_text}`;
    }
}

// ------------------------------------------------------------

function member_expression_array_length (obj, prop, set_expr) {

    let called_for_shape_object;
    let init_text = '';
    let optional_suffix = '';
    if (typeof(obj) === 'string') {
        // if object is a string, then this call comes from
        // member_expression_object () for an object that is
        // compatible with shape optimization, and can only
        // get here as part of a 'set property' expression.
        if (!set_expr)
            throw [ obj, 'expected set expression' ];
        called_for_shape_object = true;

    } else {
        // otherwise object is a syntax tree node, and we
        // were called for an object which is not compatible
        // with shape optimization, and this may be either
        // a 'get property' or a 'set property' expression.
        let obj_text = expression_writer(obj);

        // use a temp var if object is a non-trivial expression
        if (!utils.is_basic_expr_node(obj)) {
            const tmp = utils_c.alloc_temp_value(obj);
            init_text += `${tmp}=${obj_text},`;
            obj = tmp;
        } else
            obj = obj_text;

        if (set_expr === 'optional') {
            init_text += `unlikely(js_is_undefined_or_null(${obj}))`
                      +  `?js_undefined:(`;
            optional_suffix += ')';
            set_expr = undefined;
        }
    }

    const arr_obj = `((js_arr*)js_get_pointer(${obj}))`;
    let check_text = `js_is_object(${obj})&&`
       + `${arr_obj}->super.proto==env->fast_arr_proto`;

    prop = expression_writer(prop);

    if (!set_expr) {
        // a 'get property' for length can only reach here
        // for an access without shape caching.  but we can
        // still check for an array, which has a length at
        // a known location, otherwise do plain 'get' via
        // js_getprop () as in member_expression_generic ()
        return `(${init_text}likely(${check_text})`
             + `?${arr_obj}->length_descr[0]`
             + `:js_getprop(env,${obj},${prop},`
             + '&env->dummy_shape_cache))'
             + optional_suffix;
    }

    // 'set property' for length

    let len = set_expr;
    if (typeof(len) !== 'string') {

        len = expression_writer(len);
        // use a temp var if value is a non-trivial expression
        if (!utils.is_basic_expr_node(set_expr)) {
            const tmp = utils_c.alloc_temp_value(set_expr);
            init_text += `${tmp}=${len},`;
            len = tmp;
        }
    }

    const len_u = `(uint64_t)js_get_number(${len})`;

    // make sure array does not contain any descriptors
    check_text += `&&${arr_obj}->length!=-1U`;
    // make sure the new length value is an integer,
    // is larger than the current length, and is smaller
    // than the maximum length.  note that if new length
    // is smaller than current, we must touch elements,
    // see also js_setprop_array_length () in runtime
    check_text = `js_is_number(${len})&&${check_text}`
       + `&&js_make_number(${len_u}).raw==${len}.raw`
       + `&&${len_u}>=${arr_obj}->length`
       + `&&${len_u}<=js_arr_max_length`;

    let text = `${init_text}likely(${check_text})`
             + `?(${arr_obj}->length=${len_u},${arr_obj}->length_descr[0]=${len})`
             + `:`;

    if (!called_for_shape_object) {
        text += `js_setprop(env,${obj},${prop},${len},`
             +  '&env->dummy_shape_cache)';
    }

    return text;
}

// ------------------------------------------------------------

exports.delete_expression = function (expr) {

    let obj, prop;
    const arg = expr.argument;

    if (arg.type === 'Identifier') {

        // in strict mode, unqualified 'delete var' is a
        // syntax error, should have been caught by esprima,
        // and should never reach here.
        // in non-strict mode, treat as 'delete global.var',
        // but global_to_object_lookup () in literal.js
        // should have converted this to a MemberExpression
        // already. so should never reach here either.
        throw [ node, 'unqualified delete' ];

    } else if (arg.type === 'MemberExpression') {

        obj = expression_writer(arg.object);
        prop = expression_writer(arg.property);

        if (utils_c.is_global_lookup_non_strict(obj, arg)) {
            // unqualified property access in non-strict mode
            return `js_delwith(env,func_val,${prop})`;
        }

    } else {

        // following the example of delete console.log(1),
        // per MDN, evaluate the expression and return true
        return '((' + expression_writer(arg) + '),js_true)';
    }

    return `js_delprop(env,${obj},${prop})`;
}
