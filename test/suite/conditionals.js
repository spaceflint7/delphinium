'use strict';

// ------------
// switch
// ------------

;(function () {
let print = '';
for (const some_value of [ true, 1, 'a', null ]) {
    if (print)
        print += ',';
    sw: switch (typeof(some_value)) {
        default:            print += 'unk';     let q = '?';
        case 'fallthrough': print += q;         break;
        case 'boolean':     print += 'bool';    break sw;
        case 'number':      print += 'num';     break sw;
        case 'string':      print += 'str';     break sw;
    }
}
console.log(print);
})()
