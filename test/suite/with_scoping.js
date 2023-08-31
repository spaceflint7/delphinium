
// unqualified access looks up property in 'with' target
const o1 = { prop: "prop_in_o1" };
with (o1) { console.log(prop); }

// unqualified access looks up property in 'with' target,
// then in global namespace
with (o1) { console.log(Object); }

// if a local is declared inside the 'with' block,
// it takes precedence over the lookup in the 'with' target
with (o1) { let prop = 123; console.log(prop); prop = 456; }
console.log('after let:', o1.prop);

// if local is declared via 'var', the declaration is
// hoisted to the top, and only the assignment remains
// within the 'with' block, i.e. just 'prop = 234',
// which should translate to property access on 'o1'
with (o1) { var prop = 234; console.log(prop); }
console.log('after var:', o1.prop);

// if a local is declared outside the 'with' block,
// it is looked up only after trying the 'with' target
let prop2 = 'local_prop';
o1.prop2 = 'prop_in_o2';
with (o1) { console.log(prop2); prop2 += '_changed'; }
console.log(o1.prop2);
with (o1) { delete prop2; }
with (o1) { console.log(prop2); }

//
var sum2 = 1000;
o1.sum = 100;
with (o1) { var sum; sum += 200 };
with (o1) { let sum; sum += 200 };
with (o1) { sum2 += 2000; }; // sum2 local
o1.sum2 = 0;
with (o1) { sum2 += 9999; }; // sum2 in o1
console.log(o1.sum, sum2);

// a nested function (maintains the 'with' block
with (o1) {
    ;(function () { console.log(typeof(prop), prop); })()
}
// another test
let nested_x; with ({x:'!'}) {nested_x = (()=> (()=> (()=>x)())())()};
console.log(nested_x);

// using Symbol.unscopables to hide the property
o1[Symbol.unscopables] = { __proto__: null, prop: true }
with (o1) {
    console.log(prop);
}

// test Symbol.unscopables on Array.prototype
with (Array.prototype) { console.log('keys', typeof(keys)); }
Array.prototype[Symbol.unscopables].keys = false;
with (Array.prototype) { console.log('keys', typeof(keys)); }
