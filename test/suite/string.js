'use strict';

//
// test #1
//

;(function () {

const str = 'The quick red fox jumped over the lazy dog\'s back.';
const iterator = str[Symbol.iterator]();
let theChar = iterator.next();

while (!theChar.done && theChar.value !== ' ') {
    if (theChar.done)
        break;
    console.log(theChar.value);
    theChar = iterator.next();
}

let c = [];
c = (c ? 'YES' : 'NO');
console.log(c);

let obj = { hello: 'world', true: 'false' };
obj[Symbol('7')] = 'seven';
obj[5] = 'five';
obj[20] = 'twenty';
obj[9] = 'nine';
obj[15] = 'fifteen';
obj[12] = 'twelve';
console.log(obj);
console.log(Object.getOwnPropertyNames(obj));
console.log(Object.getOwnPropertyDescriptors(obj));

})()
