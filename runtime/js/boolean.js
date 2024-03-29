
// ------------------------------------------------------------
//
// Boolean
//
// ------------------------------------------------------------

;(function Boolean_init () {

// ------------------------------------------------------------
//
// Boolean constructor
//
// ------------------------------------------------------------

function Boolean (bool) {

    bool = bool ? true : false;
    if (new.target)
        bool = _shadow.create_object_wrapper(bool);

    return bool;
}

_shadow.js_flag_as_constructor(Boolean);

// ------------------------------------------------------------
//
// Boolean prototype
//
// note that the prototype object was already created
// in js_num_init () in file num.c. it is attached to
// number values by js_get_primitive_proto () in obj.c.
//
// ------------------------------------------------------------

defineNotEnum(_global, 'Boolean', Boolean);
_shadow.Boolean = Boolean; // keep a copy

const Boolean_prototype = (true).__proto__;
defineNotEnum(Boolean_prototype, 'constructor', Boolean);
defineProperty(Boolean, 'prototype', { value: Boolean_prototype });

// ------------------------------------------------------------
//
// Boolean.prototype.toString
//
// ------------------------------------------------------------

defineNotEnum(Boolean_prototype, 'toString',
function toString () {
    const v = _shadow.unwrap_object_wrapper(this, 'boolean');
    return v ? 'true' : 'false';
});

// ------------------------------------------------------------
//
// Boolean.prototype.valueOf
//
// ------------------------------------------------------------

defineNotEnum(Boolean_prototype, 'valueOf',
function valueOf () {
    if (this === Boolean_prototype)
        return false;
    return _shadow.unwrap_object_wrapper(this, 'boolean');
});


// ------------------------------------------------------------

})()    // Boolean_init
