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
