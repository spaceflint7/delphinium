'use strict';

if (!global.gc)
    global.gc = function () {};

//
// test stack walking
//

;(function () {
    let o = 'Test' + 'String';
    gc(true); gc(true);
    console.log(o + '_1!');
})();

//
// test stack walking in coroutines
//

;(function () {

    ;(function () {
        const generator = (function* () {
            let o = 'Test' + 'String';
            yield 0;
            gc(true); gc(true);
            console.log(o + '_2!');
        })();
        generator.next();
        gc(true); gc(true);
        generator.next();
    })();

    // garbage-collect the generator instance
    ;(function r (depth) {
        const a = 1; const b = 2; const c = 3;
        const d = 4; const e = 5; const f = 6;
        gc(true); gc(true);
        if (depth < 4)
            r(depth + 1);
    })(1);

})();

//
// test map
//

;(function () {

    ;(function () {

        let m = (function () {
            const m = (function () {
                const m = new Map([
                    [123,  ' '+'abc' ],
                    [123n, ' '+'ABC' ],
                    [-0,   'TestString' + '_3!'],
                ]);
                return m;
            })();
            gc(true); gc(true);
            return m;
        })();

        (function (m) {
            console.log(m.get(0));
        })(m);

        m = null;
    })();

    // garbage-collect the map instance
    ;(function r (depth) {
        const a = 1; const b = 2; const c = 3;
        const d = 4; const e = 5; const f = 6;
        gc(true); gc(true);
        if (depth < 4)
            r(depth + 1);
    })(1);

})();
