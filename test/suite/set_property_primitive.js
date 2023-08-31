
function test (f) { try { f(); } catch (e) { console.log(e.name); } }

// sloppy mode silently ignores when setting a property on a primitive
;(function () { (123).prop = 456; })()

// strict mode throws an error when setting a property on a primitive
test (function () { 'use strict'; (123).prop = 456; })

// the only exception is a setter in the primitive prototype
Object.defineProperty(Number.prototype, 'setter',
    { set(v) { 'use strict'; console.log('in setter', v); } });
;(function () { 'use strict'; (123).setter = 456; })()

// the exception above does not apply to writable properties
Number.prototype.plain = 4567;
Object.defineProperty(Number.prototype, 'writable',
    { value: 123, writable: true });

;(function () { 'use strict'; console.log((123).plain); })()
test (function () { 'use strict'; (123).plain = 4567; })
test (function () { 'use strict'; (123).writable = 4567; })

console.log('end of test');
