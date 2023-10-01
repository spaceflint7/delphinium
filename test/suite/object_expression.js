'use strict';

let shortHand = 777;

const o1 = {
  bar: 123,
  set bar(val) { console.log('NOT INVOKED') },
  foo: 456,
  ...{ // spread expression overwrites properties
       // and does not invoke any setters already defined
       foo: 321,
       bar: 654,
       baz: 111,
       // spread copies the result of getter as a plain value
       get was_getter() { return 987 },
       // spread copies a setter-only property as undefined
       set was_setter(val) {},
       // spread does not copy properties from prototype
       __proto__: { ignored_from_prototype: 2 } },
  // property name and value in one identifier
  shortHand
};

const o2 = {
  get foo() { return 'GETTING' },
  set foo(val) { console.log('SETTING'); console.log(val); },
  __proto__: o1,
};

console.log(o1);
console.log(o2.foo);
o2.foo = 123;
console.log(o2.was_getter);

// test that spread syntax also copies array elements
const o3 = { ... [ 123, 456, 789 ] };
console.log(o3);
