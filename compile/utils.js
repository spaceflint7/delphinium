
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
        node.child_nodes.forEach(f);
    }
    f(block);
    return (found ? distance : -1);
}

// ------------------------------------------------------------

exports.throw_unexpected_property = function (node) {

    throw [ node, 'unexpected property in object expression' ];
}
