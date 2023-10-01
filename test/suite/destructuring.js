'use strict';

// ------------
// test 1
// ------------

;(function () {

    let key = 'z'; // String.fromCharCode(97 + 25); // 'z'
    const { [key]: foo } = { z: 'bar' };

    function f ( first, { [key]: foo, ...more }, ...last ) {
        console.log(first, foo, more, last);

        const [a, b, ...{ pop, push }] = [123, 456, 789];
        console.log(a, b, pop, push);
    }

    f( 123, { z: 999, hello: true, world: true }, 456, 789 );
    key = 'a';
    f( 123, { a: 999, hello: true, world: true }, 456, 789 );

    function series (mul, ...nums) {
        console.log(nums);
    }

    series(1, 2, 3, 4, 5);

})()

// ------------
// test 2
// ------------

;(function () {

    let key = 'a';
    function f1 ( first, { [key]: foo, ...more }, ...last ) {
        console.log(foo, more, last);
    }
    f1 ( 123, { a: 999, b: 888, c: 777 }, 456, 789)

    var obj = { key: 'c' };
    function f2 ( first, { [obj.key]: foo, ...more }, ...last ) {
        console.log(foo, more, last);
    }
    f2 ( 123, { a: 999, b: 888, c: 777 }, 456, 789)

    function f3 ( { [key + obj.key + 'e']: foo } ) {
        console.log(foo);
    }
    f3({ace: 123456789 })
})()

// ------------
// test 3
// ------------

;(function () {
    // test object-pattern destructuring
    let o = { prop: 123 };
    var prop1, rest = {};
    ({ prop1: o.prop2 = 456, ...rest } = o);
    const { length } = [ 123, 456 ];
    console.log(o, prop1, rest, length);
})();

// ------------
// test 4
// ------------

;(function () {

    // prepare array for tests below
    var arr = [ , , , 'hello', 'world', 'third', 'fourth' ];
    arr.someProperty = undefined;
    arr.someProperty2 = 'theProp';
    Object.defineProperty(arr, 'someProperty3', {
        get() { console.log('IN GET') }
    });

    // test object-pattern destructuring
    var q1, q2, q3, q4;
    [ ,,, q1, ...{ length: q2 = f(), ['0']: q3, ...q4 }] = arr;
    console.log(q1, q2, q3, q4);

    function f() { console.log(q1); }

    // destructuring into closure variables (see below)
    const [a, b, ...{ length }] = [1, 2, 3, 4, 5];

    // test order of operations, in particular
    // when default values are specified.
    let someProperty, someProperty2, someProperty3;
        ({ someProperty = (() => console.log(a, b, length))(),
         someProperty2 = (() => console.log('should not'))(),
         someProperty3 } = arr);
})();

// ------------
// test 5
// ------------

;(function () {

    // destructuring in function parameters
    function f1(...args) { console.log(args); }
    function f2(...[p1, p2]) { console.log(p1, '&', p2); }
    function f3([p1, p2]) { console.log(p1, '&', p2); }
    function f4({p1, p2}) { console.log(p1, '&', p2); }
    function f5({p1, p2, p3: pX = 111}) { console.log(p1, '&', p2, '&', pX); }

    f1(123, 456, 789);
    f2([123, 456], 789);
    f3([123, 456], 789);
    f4({p1: 123, p2: 456}, 789);
    f5({p1: 123, p2: 456});

})();

// ------------
// test 6
// ------------

;(function () {

    try {
        throw [ 54321, { prop: 12345 }];
    } catch ([ elem, { prop, ...rest1 }, ...rest2]) {
        console.log(elem, prop);
    }

})();
