'use strict';

function fib (number) {
    let n1 = 0, n2 = 1, nextTerm;
    console.log('Fibonacci Series:');
    let output = '';
    for (let i = 1; i <= number; i++) {
        output += n1 + '   ';
        nextTerm = n1 + n2;
        n1 = n2;
        n2 = nextTerm;
    }
    console.log(output);
}

console.log('Hello, world!');
fib(40);
