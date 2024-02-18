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

let stringWrapper = new String('ABCD');
stringWrapper[99] = 'test';
stringWrapper[Symbol.iterator] = true;
console.log(stringWrapper);

console.log(String(), 'vs', String(undefined));

console.log(String.prototype.charAt.call(false,4));
console.log('Z'.charAt());
console.log('Z'.charCodeAt());
console.log('Z'.charCodeAt());
let stringFrom = String.fromCharCode(90,NaN,undefined,false);
console.log(stringFrom.charCodeAt(0),
            stringFrom.charCodeAt(1),
            stringFrom.charCodeAt(2),
            stringFrom.charCodeAt(3),
            stringFrom.charCodeAt(4));

})()
