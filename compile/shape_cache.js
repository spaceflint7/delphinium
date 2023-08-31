
// ------------------------------------------------------------

const utils = require('./utils');
const utils_c = require('./utils_c');

// ------------------------------------------------------------

function make_shape_cache_key (expr) {

    if (expr.type === 'AssignmentExpression') {
        track_assignment(expr);
        return;
    }

    if (expr.shape_cache_key)
        return expr.shape_cache_key;

    /*if (expr.computed)
        return (expr.shape_cache_key = '?');*/

    // build shape key as: parent.object;counter.property
    // where parent is the optional shape key of the
    // containing MemberExpression.  'object' is the C name
    // for the variable being indexed, and 'property' is
    // the C name for the indexing literal.  and 'counter'
    // counts the number of assignments to the 'object',
    // so that a new shape key is used, after each time
    // an object variable is assigned a new value.

    let parent_key = expr.parent_node?.shape_cache_key;
    if (parent_key === '?')
        return (expr.shape_cache_key = '?');

    let object_key = get_hash(expr.object);
    if (object_key === '?')
        return (expr.shape_cache_key = '?');

    let property_key = get_hash(expr.property);
    if (property_key === '?')
        return (expr.shape_cache_key = '?');

    if (parent_key)
        parent_key = parent_key + '.';
    else
        parent_key = '';

    return (expr.shape_cache_key =
                `${parent_key}${object_key}.${property_key}`);

    function get_hash (expr) {

        switch (expr.type) {

            case 'MemberExpression':
                return make_shape_cache_key(expr);

            case 'Literal':

                if (expr.c_name)
                    return expr.c_name;
                break;

            case 'Identifier':

                let c_name;
                if (expr.decl_node) {
                    const decl_node = expr.decl_node;

                    if (expr.closure_index >= 0) {
                        // a closure identifier may not always
                        // have a proper decl_node useful for
                        // get_variable_c_name (), so generate
                        // some unique string shape key here
                        c_name = `${utils_c.clean_name(expr)}`
                               + `((closure_${expr.closure_index}))`;
                    } else
                        c_name = utils_c.get_variable_c_name(decl_node);

                    if (decl_node.assign_count)
                        c_name += `;${decl_node.assign_count}`;

                } else
                    c_name = utils_c.get_variable_c_name(expr);

                if (c_name === utils_c.env_global_decl_node.c_name)
                    c_name = '((global))';

                return c_name;

            case 'ThisExpression':
                return '((this))';

            case 'CallExpression':
                return get_hash(expr.callee);
        }

        return '?';
    }

    // when a variable is reassigned, the old shape might
    // no longer be relevant, so for best caching results,
    // we want to make sure that we use a new cache shape
    // variable.  to do this, we count assignments to the
    // same variable, and append the assignment counter
    // into the shape cache key, as discussed above.

    function track_assignment (expr) {
        if (expr.left.type === 'Identifier') {
            let decl_node = expr.left.decl_node;
            if (decl_node) {
                decl_node.assign_count = 1 +
                    (decl_node.assign_count || 0);
            }
        }
    }
}

// ------------------------------------------------------------

exports.collect_keys = function (func_node) {

    func_node.visit(
        [ 'AssignmentExpression', 'MemberExpression' ],
            make_shape_cache_key);

    /*func_node.visit(undefined, (node) => {
        if (node.shape_cache_key)
            console.log(node.shape_cache_key);
    });*/
}

// ------------------------------------------------------------

exports.get_shape_var = function get_shape_variable (node) {

    // the shape cache mechanism attaches a cache variable
    // for each property access on an object.  an initial
    // pass by make_shape_cache_keys () determines if the
    // property access can translate to a meaningful cache
    // key, which is usually a non-computed property which
    // indexes into a specific, named identifier.

    let shape_var;

    const shape_key = node.shape_cache_key;
    if (typeof(shape_key) === 'string' && shape_key !== '?') {

        // the shape cache is a map which translates the
        // cache key strings to an index within the cache,
        // or more precisely, within 'func_obj'.  see also
        // member_expression () above, and the allocation
        // of cache variables in function_expression ()
        // and in js_newfunc () in func.c

        const func_node = utils.get_parent_func_node(node);

        let map = func_node.shape_objects;
        if (!map) {
            map = func_node.shape_objects = new Map();
            func_node.shape_cache_count = 0;
        }

        shape_var = map.get(shape_key);
        if (!shape_var) {
            shape_var = func_node.shape_cache_count++;
            shape_var = '(((js_func*)js_get_pointer(func_val))'
                      + `->shape_cache[${shape_var}])`;
            map.set(shape_key, shape_var);
        }
    }

    return shape_var;
}

// ------------------------------------------------------------

exports.is_global_lookup = function (member_expr) {

    // make sure this is dot property name access (i.e.,
    // an identifier with a C variable name), or a bracket
    // property for a string literal (also a C var name).
    // if no C var, should have been handled as an array
    if (!member_expr.property.c_name) {

        throw [ member_expr,
                    'bad property in MemberExpression' ];
    }

    return utils_c.is_global_lookup(member_expr);
}
