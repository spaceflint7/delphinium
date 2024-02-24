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
let stringFrom = String.fromCharCode(90,NaN,undefined,false);
console.log(stringFrom.charCodeAt(0),
            stringFrom.charCodeAt(1),
            stringFrom.charCodeAt(2),
            stringFrom.charCodeAt(3),
            stringFrom.charCodeAt(4));
})()

//
// test #2
//

;(function () {

console.log(Object.getOwnPropertyDescriptor(String.prototype, 'at'));
console.log(Object.getOwnPropertyDescriptor(String.prototype, 'charAt'));
console.log(Object.getOwnPropertyDescriptor(String.prototype, 'slice'));
console.log(Object.getOwnPropertyDescriptor(String.prototype, 'substr'));
console.log(Object.getOwnPropertyDescriptor(String.prototype, 'substring'));

console.log(Object.getOwnPropertyDescriptors(String.prototype['at']));
console.log(Object.getOwnPropertyDescriptors(String.prototype['charAt']));
console.log(Object.getOwnPropertyDescriptors(String.prototype['slice']));
console.log(Object.getOwnPropertyDescriptors(String.prototype['substr']));
console.log(Object.getOwnPropertyDescriptors(String.prototype['substring']));

function logstr (s) { console.log('<' + s + '>'); }

logstr('   ZzZ   '.trim()     );
logstr('   ZzZ   '.trimStart());
logstr('   ZzZ   '.trimEnd());

const str1 = "The morning is upon us."; // The length of str1 is 23.
const str2 = str1.slice(1, 8);
const str3 = str1.slice(4, -2);
const str4 = str1.slice(12);
const str5 = str1.slice(30);
logstr(str2); // he morn
logstr(str3); // morning is upon u
logstr(str4); // is upon us.
logstr(str5); // ""

const aString = "Mozilla";
logstr(aString.substr(0, 1)); // 'M'
logstr(aString.substr(1, 0)); // ''
logstr(aString.substr(-1, 1)); // 'a'
logstr(aString.substr(1, -1)); // ''
logstr(aString.substr(-3)); // 'lla'
logstr(aString.substr(1)); // 'ozilla'
logstr(aString.substr(-20, 2)); // 'Mo'
logstr(aString.substr(20, 2)); // ''

logstr(aString.substring(0, 1)); // "M"
logstr(aString.substring(1, 0)); // "M"
logstr(aString.substring(0, 6)); // "Mozill"
logstr(aString.substring(4)); // "lla"
logstr(aString.substring(4, 7)); // "lla"
logstr(aString.substring(7, 4)); // "lla"
logstr(aString.substring(0, 7)); // "Mozilla"
logstr(aString.substring(0, 10)); // "Mozilla"

logstr('A'.at(0));
logstr('A'.at(-1));
logstr('A'.at(-Infinity));
logstr('A'.at(+Infinity));
logstr('A'.charAt(-Infinity));
logstr('A'.charAt(+Infinity));
logstr('A'.slice(-Infinity));
logstr('A'.slice(+Infinity));
logstr('A'.substr(-Infinity));
logstr('A'.substr(+Infinity));
logstr('A'.substring(-Infinity));
logstr('A'.substring(+Infinity));

const hello = "Hello, ";
logstr(hello.concat("Kevin", ". Have a nice day."));
// Hello, Kevin. Have a nice day.

const greetList = ["Hello", " ", "Venkat", "!"];
logstr("".concat(...greetList)); // "Hello Venkat!"

logstr("".concat({})); // "[object Object]"
logstr("".concat([])); // ""
logstr("".concat(null)); // "null"
logstr("".concat(true)); // "true"
logstr("".concat(4, 5)); // "45"

logstr("Z".concat());

})()

//
// test #3
//

;(function () {

console.log('---');
const str = "\ud83d\udc0e\ud83d\udc71\u2764";
for (let i = 0; i < str.length; i++) {
    console.log(str.codePointAt(i).toString(16));
}
console.log('---');
for (const codePoint of str) {
    console.log(codePoint.codePointAt(0).toString(16));
}
console.log('---');
let arr = [...str];
for (let i = 0; i < arr.length; i++)
    console.log(arr[i].codePointAt(0).toString(16));
console.log('---');

})()
