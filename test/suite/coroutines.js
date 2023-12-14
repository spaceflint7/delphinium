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
    console.log('#12', Object.getOwnPropertyDescriptors(gen_func().__proto__.__proto__));
    console.log('#12', Object.getOwnPropertyDescriptors(gen_func().__proto__.__proto__.__proto__));

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
