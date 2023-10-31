'use strict';

function test (f) { try { f(); } catch (e) { console.log(e.name); } }

// ------------
//
// test proper ordering when hoisting nested function
// declarations and hoisting plain 'var' declarations
//
// ------------

test (function () {
    // second function declaration overrides first
    function f() { console.log('f1'); }; f();
    function f() { console.log('f2 - test 1'); }; f();
})

test (function () {
    // functions declarations are hoisted before
    // 'var' declarations, so the string value
    // overrides both function declarations
    var f = 'x';
    function f() { console.log('f1'); }; f();
    function f() { console.log('f2'); }; f();
})

test (function () {
    // a 'var' declaration without initialization
    // does not alter the value of the local, which
    // remains set to the second function declarations
    var f;
    function f() { console.log('f1'); }; f();
    function f() { console.log('f2 - test 3'); }; f();
})

test (function () {
    // the first call should invoke the second function
    // declarations, but the second call should try to
    // call a string value, and throw an error
    var f;
    function f() { console.log('f1'); }; f();
    f = 'x';
    function f() { console.log('f2 - test 4'); }; f();
})

// ------------
//
// test function arity
//
// ------------

console.log(
    (function ( [p1, p2, ...p5], {p3, p4}, ...r) {})
        .length);
