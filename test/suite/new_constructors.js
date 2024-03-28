'use strict';

//
// first test:  constructors, new.target
//

function Car (name) {

    this.name = name;
    console.log('car created by constructor ' + (new.target)?.descr);

    this.drive = function () {

        console.log('driving an object created by ' + (new.target)?.descr);
        if (new.target === Car)
            console.log('it is a car named', this.car);
    }

    this.speed = () =>
        console.log('would return speed of car created by ' + (new.target)?.descr);
}

Car.descr = 'CarConstructorFunction';

function TestCar () {

    console.log('testing, step 1', new.target);
    let myCar = new Car('myCar');
    console.log('testing, step 2', new.target);
    myCar.drive();
    console.log('testing, step 3', new.target);
    myCar.speed();
    console.log('testing, step 4', new.target);
}

console.log(TestCar);
console.log(TestCar.__proto__);
console.log(TestCar.__proto__.__proto__);
new TestCar ();

//
// second test:  new constructors and bound functions
//

console.log('==========================================');

(function () {

function f1 (arg1, arg2) {
    console.log(    (this ? '<'+this.prop+'>' : 'UNKNOWN'),     new.target,  arg1, arg2);
    if (this) this.prop = 'NEWOBJ';
}

f1.prop = 'F1';
f1();
f1.call(f1, 123);

let f1obj = new f1();
console.log(f1obj);

let f2 = f1.bind({ prop: 'F2'}, 234);
console.log(f2.name);
f2();
f2.call(f1);

let f2obj = new f2(567);
console.log(f2obj);

let f3 = f2.bind({ prop: 'F3'}, 456);
console.log(f3.name);
f3();
f3.call(f1);

let f3obj = new f3(678);
console.log(f3obj);

})()

//
// third test:  new constructors and primitive values
//

console.log('==========================================');

(function () {

console.log(Boolean.prototype.valueOf(),
            Boolean(0), Boolean(1),
            new Boolean(0), new Boolean(1),
            Number(123).valueOf(),
            new Number(123).valueOf(),
            new Object(123).valueOf());

})()
