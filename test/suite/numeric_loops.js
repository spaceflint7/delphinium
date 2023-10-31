'use strict';

// for-loop declaring more than one variable
for (let i = 1, j = 10; i < 10; i++, j *= 2)
    console.log(i, j);

// for-loops using a destructuring assignment
for (let [i, j] = [1, 10]; i < 10; i++, j *= 2)
    console.log(i, j);

stmt_label: console.log('statement label');
stmt_label: var stmt_label = 4;
stmt_label: break stmt_label;

block_label: {
    console.log('block label');
    break block_label;
    console.log('not printed');
}

loop1: for (let z = 0; z < 10; z++) continue loop1;

let count = 0;
loop1: for (;;) {
    console.log('top of loop1');
    loop2: for (;;) {
        count++;
        if (count === 300) {
            console.log('breaking out of both loops');
            break loop1;
        }
        if (!(count % 100)) {
            console.log('continue', count);
            continue loop1;
        }
    }
    console.log('after loop2');
}
console.log('after loop1');

// some calculation in while(){} form
let wcount = 1, wsum = 0;
wlabel: while (++wcount < 100)
    if (wcount & 1)
        wsum += wcount;
    else
        continue wlabel;
console.log(wsum);

// same calculation in do{}while() form
[wcount, wsum] = [1, 0];
wlabel: do {
    if (wcount & 1)
        wsum += wcount;
    else
        continue wlabel;
} while (++wcount < 100);
console.log(wsum);
