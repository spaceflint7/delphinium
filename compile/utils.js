
// ------------------------------------------------------------

let next_unique_id = 1111;

exports.get_unique_id = function () {

    return next_unique_id++;
}

exports.set_unique_id_from_text = function (name) {

    const hashCode = (str) =>
        [...str].reduce((s, c) =>
                Math.imul(31, s) + c.charCodeAt(0) | 0, 0);

    next_unique_id = Math.abs(hashCode(name) % 9999);
}

// ------------------------------------------------------------

exports.is_basic_expr_node = function (node) {

    if (node.type === 'Literal')
        return true;
    if (node.type === 'Identifier' && !node.is_closure)
        return true;
    return false;
}

// ------------------------------------------------------------

exports.get_parent_func_node = function (node) {

    const node0 = node;
    while (node) {
        if (node.is_func_node)
            return node;
        else
            node = node.parent_node;
    }

    throw [ node0, 'cannot find parent function node' ];
}

// ------------------------------------------------------------

exports.get_parent_block_node = function (node, must_find = true) {

    const node0 = node;
    while (node) {
        if (node.type === 'BlockStatement')
            return node;
        else
            node = node.parent_node;
    }

    if (!must_find)
        return null;

    throw [ node0, 'cannot find parent block node' ];
}

// ------------------------------------------------------------

exports.is_descendant_of = function (node, test_node) {

    while (node) {
        if (node === test_node)
            return true;
        node = node.parent_node;
    }
    return false;
}

// ------------------------------------------------------------

exports.get_distance_from_block = function (find_node, block) {

    let distance = 0;
    let found = false;
    const f = (node) => {
        if (found || node === find_node)
            return (found = true);
        ++distance;
        exports.get_child_nodes(node).forEach(f);
    }
    f(block);
    return (found ? distance : -1);
}

// ------------------------------------------------------------

exports.throw_unexpected_property = function (node) {

    throw [ node, 'unexpected property in object expression' ];
}

// ------------------------------------------------------------

exports.get_child_nodes = function (node) {

    //if (!get_child_nodes_keys[node.type]) console.log(node);
    const child_nodes = [];
    for (const key of get_child_nodes_keys[node.type]) {

        const array_or_sub_node = node[key];

        if (Array.isArray(array_or_sub_node)) {
            for (const array_elem of array_or_sub_node) {
                if (array_elem?.type)
                    child_nodes.push(array_elem);
            }

        } else if (array_or_sub_node?.type)
            child_nodes.push(array_or_sub_node);
    }

    return child_nodes;
}

// properties in an Esprima node that lead to other nodes

var get_child_nodes_keys = {

    FunctionDeclaration:        [ 'id', 'params', 'body' ],
    VariableDeclaration:        [ 'declarations' ],
    VariableDeclarator:         [ 'id', 'init' ],
    ArrayPattern:               [ 'elements' ],
    AssignmentPattern:          [ 'left', 'right' ],
    ObjectPattern:              [ 'properties' ],
    RestElement:                [ 'argument' ],

    BlockStatement:             [ 'body' ],
    BreakStatement:             [ 'label' ],
    CatchClause:                [ 'param', 'body' ],
    ContinueStatement:          [ 'label' ],
    DoWhileStatement:           [ 'body', 'test' ],
    EmptyStatement:             [],
    ExpressionStatement:        [ 'expression' ],
    ForStatement:               [ 'init', 'test', 'update', 'body' ],
    ForInStatement:             [ 'left', 'right', 'body' ],
    ForOfStatement:             [ 'left', 'right', 'body' ],
    IfStatement:                [ 'test', 'consequent', 'alternate' ],
    LabeledStatement:           [ 'label', 'body' ],
    ReturnStatement:            [ 'argument' ],
    ThrowStatement:             [ 'argument' ],
    TryStatement:               [ 'block', 'handler', 'finalizer' ],
    WhileStatement:             [ 'test', 'body' ],
    WithStatement:              [ 'object', 'body' ],

    ArrayExpression:            [ 'elements' ],
    ArrowFunctionExpression:    [ 'id', 'params', 'body' ],
    AssignmentExpression:       [ 'left', 'right' ],
    BinaryExpression:           [ 'left', 'right' ],
    CallExpression:             [ 'callee', 'arguments' ],
    ChainExpression:            [ 'expression' ],
    ConditionalExpression:      [ 'test', 'consequent', 'alternate' ],
    FunctionExpression:         [ 'id', 'params', 'body' ],
    Identifier:                 [],
    Literal:                    [],
    LogicalExpression:          [ 'left', 'right' ],
    NewExpression:              [ 'callee', 'arguments' ],
    MemberExpression:           [ 'object', 'property' ],
    MetaProperty:               [ 'meta', 'property' ],
    ObjectExpression:           [ 'properties' ],
    Property:                   [ 'key', 'value' ],
    SequenceExpression:         [ 'expressions' ],
    SpreadElement:              [ 'argument' ],
    ThisExpression:             [],
    UnaryExpression:            [ 'argument' ],
    UpdateExpression:           [ 'argument' ],

};
