'use strict';

//
// test 'return' statements within exceptions
//

;(function () {

    function t1 (do_throw, first_value, second_value) {

        try {
            try {
                if (do_throw)
                    throw first_value;
                return first_value;
            } catch (q) {
                console.log('IN EXC');
            } finally {
                console.log('IN FINALLY 1');
            }
        } finally {
            console.log('IN FINALLY 2');
            if (second_value)
                return second_value;
        }
    }

    console.log(t1(false, '123'));
    console.log(t1(false, '123', '456'));
    console.log(t1(true, '123'));
    console.log(t1(true, '123', '456'));

})()

//
// test multiple try/catch with same variable
//

;(function () {

    try { throw('E1'); } catch (e) { console.log(e); }
    try { throw('E2'); } catch (e) { console.log(e); }
    console.log(typeof(e));

})()
