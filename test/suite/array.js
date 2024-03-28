'use strict';

// ------------------------------------------------------------
//
// array
//
// ------------------------------------------------------------

;(function () {

// test that a set-property operation converts
// deleted/empty cells to the undefined value
let arr = [];
arr.length = 7;
console.log(arr);
arr[0] = arr[1];
console.log(arr);
delete arr[0];
console.log(arr);
arr[0] = arr[1];
console.log(arr);

// test Array.push on array, and array-like objects
let o;
o = { length: 'X' }; Array.prototype.push.call(o, 'Z'); console.log(o);
o = { length: NaN }; Array.prototype.push.call(o, 'Z'); console.log(o);
o = { length: 8   }; Array.prototype.push.call(o, 'Z'); console.log(o);
o = [ ,,, ];         Array.prototype.push.call(o, 'Z'); console.log(o, o.length);

// test Array.pop
o = { length: 'X' }; console.log(Array.prototype.pop.call(o));
o = { length: NaN }; console.log(Array.prototype.pop.call(o));
o = { length: 1, '0': 'Z' }; console.log(Array.prototype.pop.call(o), o);

// make sure array iteration works via Symbol.iterator
arr = [ 123, 456, 789 ];
arr[Symbol.iterator] = function () {
    const array = this;
    let   index = this.length;
    return { next() {
        if (index === 0)
            return { done: true };
        else
            return { value: array[--index] };
    } } };

for (const elem of arr) console.log(elem);
var q1, q2, q3;
[q1, q2, q3] = arr;
console.log(q1, q2, q3);
console.log(...arr);

})();

// ------------------------------------------------------------
//
// map
//
// ------------------------------------------------------------

;(function () {

    var m = new Map([
      [123,  ' '+'abc' ],
      [123n, ' '+'ABC' ],
      [-0,   ' '+'ZERO'],
    ]);
    for (let [k, v] of m)
        console.log('MAP KEY',k,'VALUE<',v,'>');
    for (let k of m)
        console.log('MAP KEY',k);
    for (let v of m)
        console.log('MAP VALUE<',v,'>');

})();


// ------------------------------------------------------------
//
// set
//
// ------------------------------------------------------------

;(function () {

    var s = new Set([ 123, 123n, -0 ]);
    for (let k of s)
        console.log('SET KEY',k);
    for (let v of s)
        console.log('SET VALUE<',v,'>');

})();
