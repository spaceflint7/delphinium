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

/*test (function () {
    // var declaration in a block scope cannot override
    // a function declaration hoisted to function scope
    if (true) {
        var f = 1234;       // can't catch SyntaxError
        function f () {};
    }
})*/

// ------------
//
// test function arity
//
// ------------

console.log(
    (function ( [p1, p2, ...p5], {p3, p4}, ...r) {})
        .length);

// ------------
//
// arrow functions have no prototype
//
// ------------

test (function () {

    let arr_fun = () => {
        console.log(Object.getOwnPropertyDescriptors(arr_fun));
        console.log(arr_fun);
    };
    arr_fun();
    console.log(Object.getOwnPropertyDescriptors(arr_fun));
    console.log(new arr_fun());
})

// ------------
//
// method functions have no prototype
//
// ------------

test (function () {

    const obj = { f(a, ...b) {} };
    console.log(Object.getOwnPropertyDescriptors(obj)
              , Object.getOwnPropertyDescriptors(obj.f));
})

// ------------
//
// 'this' and 'arguments' in arrow functions
//
// ------------

;(function () {

  const x = (p) => { console.log(this,arguments[0],arguments[1]); } ;
  function y ()    { console.log(this,arguments[0],arguments[1]); }

  x(5678);
  x.call(1234, 33,44);
  y.call(1234, 55,66);

}).call('THIS', 77,88);
