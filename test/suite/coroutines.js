'use strict';

//
// check prototypes
//

;(function () {

    const GeneratorFunction = function* () {}.constructor;
    console.log('#1', GeneratorFunction.__proto__ === Function);
    console.log('#2', typeof(GeneratorFunction.prototype));
    console.log('#3', GeneratorFunction.prototype.__proto__ === Function.prototype);

    // check that 'constructor' always point to same function
    console.log('#4', GeneratorFunction.prototype.constructor === GeneratorFunction);
    const GeneratorFunction2 = function* () {}.constructor;
    console.log('#5', GeneratorFunction.prototype.constructor === GeneratorFunction2);

    const GeneratorPrototype = GeneratorFunction.prototype.prototype;
    console.log('#6', typeof(GeneratorPrototype));

    console.log('#7',  Object.getOwnPropertyDescriptors(GeneratorFunction));
    console.log('#8',  Object.getOwnPropertyDescriptors(GeneratorFunction.__proto__));
    console.log('#9',  Object.getOwnPropertyDescriptors(GeneratorFunction.prototype));
    console.log('#10', Object.getOwnPropertyDescriptors(GeneratorPrototype));

    let gen_func = function* MyGenerator (abc, def, xyz) {};
    console.log('#11', Object.getOwnPropertyDescriptors(gen_func));
    console.log('#12', Object.getOwnPropertyDescriptors(gen_func().__proto__));
    console.log('#13', Object.getOwnPropertyDescriptors(gen_func().__proto__.__proto__));
    console.log('#14', Object.getOwnPropertyDescriptors(gen_func().__proto__.__proto__.__proto__));

})()

//
// test next (), return () and throw ()
//

;(function () {

    function*gen(){
        try{yield 1234;yield 5678;}
        finally{yield 'abc';yield 'def';}return 9;}

    var g1;
    g1=gen(); console.log(g1.return('OVERRIDE'));
    g1=gen(); console.log(g1.next(), g1.return('OVERRIDE'));
    g1=gen(); console.log(g1.next(), g1.return('OVERRIDE'), g1.next());
    g1=gen(); console.log(g1.next(), g1.return('OVERRIDE'), g1.next(), g1.next());
    try { g1=gen(); console.log(g1.next(), g1.return('OVERRIDE'), g1.throw('EXCEPTION')); }
    catch (e) { console.log(e); }
    try { g1=gen(); console.log(g1.throw('EXCEPTION'), g1.next()); }
    catch (e) { console.log(e); }

})()

//
// test next(arg) with yield*
//

;(function () {

    function* gen (i) {
        console.log('ENTER GEN', i);
        if (i === -1)
            return 7;
        while (i > 0)
            yield --i;
        console.log('IN GEN', yield* gen(i - 1));
    }

    for (const x of gen(5))
        console.log('OUT GEN', x);

    function* gen_mult_10 () {
        let n = 0;
        while (n !== -1)
            n = yield (n * 10);
    }

    function* gen_mult_5 () {
        let n = 0;
        while (n !== -1)
            n = yield (n * 5);
    }

    function* gen_mult () {
        for (;;) {
            switch (yield) {
                case  0: break;
                case  5: yield* gen_mult_5(); break;
                case 10: yield* gen_mult_10(); break;
                default: return;
            }
        }
    }

    let g = gen_mult();
    for (const x of [
            0,
            5,      1, 2, 3, 4, 5,      -1,
            10,     1, 2, 3, 4, 5,      -1,
            5,      1, 2, 3, 4, 5,      -1,
            10,     1, 2, 3, 4, 5,      -1,     ])
        console.log(g.next(x));

})()

//
// test throw () and return () with yield*
//

;(function () {

    function* gen_array() { yield* [ 123, 456, 789 ]; }

    try {
        console.log(gen_array().next());
        console.log(gen_array().return('R'));
        console.log(gen_array().throw('T'));
    } catch (e) { console.log('Exception', e); }

    let my_iter = {
        [Symbol.iterator]() { return my_iter },
        next() { return { value: ++my_iter.i, done: false } },
        return() { console.log('IN RETURN') },
        throw() { console.log('IN THROW') },
        i: 0 }

    function* gen_my_iter() { yield* my_iter; }
    try {
        console.log(gen_my_iter().next());
        console.log(gen_my_iter().return('R'));
        console.log(gen_my_iter().throw('T'));
    } catch (e) { console.log('Exception', e); }

})()

//
// more tests for throw () and return ()
//

;(function () {

    var g, gen_inner;

    function gen_inner_both (i) {
        let array = [ i + 1, i + 2, i + 3];
        let iter = array[Symbol.iterator]();
        iter.return = function (v) {
            console.log('Inner Return', v);
            return { value: 888, done: false };
        }
        iter.throw = function (e) {
            console.log('Inner Throw', e);
            return { value: 999, done: false };
        }
        return iter;
    }

    function gen_inner_no_throw (i) {
        let r = gen_inner_both(i);
        r.throw = null;
        return r;
    }

    function gen_inner_no_return (i) {
        let r = gen_inner_both(i);
        r.return = null;
        return r;
    }

    function* gen_outer () {
        try {
            yield* gen_inner(20);
            yield 1;
            yield* gen_inner(10);
            return 'THE END';
        } catch (e) {
            console.log('Outer Exc', e.name);
        }
    }

    // test throw () on a nested iterator which
    // does not itself have a throw () method.
    // return(undefined) should be called instead.
    gen_inner = gen_inner_no_throw;
    var g = gen_outer();
    console.log(g.next(), g.throw('Z'), g.next());

    // test return () on a nested iterator which
    // does not itself have a return () method.
    gen_inner = gen_inner_no_return;
    var g = gen_outer();
    console.log(g.next(), g.return('Z'), g.next());

    // test return () on a nested iterator
    gen_inner = gen_inner_both;
    var g = gen_outer();
    console.log(g.next(), g.return('Z'), g.next(), g.next(), g.next(), g.next(), g.next(), g.next(), g.next(), g.next());

})();

