'use strict';

function abs (x) { console.log(Math.abs(x)); }
function acos (x) { console.log(Math.acos(x).toFixed(8)); }
function acosh (x) { console.log(Math.acosh(x).toFixed(8)); }
function asin (x) { console.log(Math.asin(x).toFixed(8)); }
function asinh (x) { console.log(Math.asinh(x).toFixed(8)); }
function atan (x) { console.log(Math.atan(x).toFixed(8)); }
function atanh (x) { console.log(Math.atanh(x).toFixed(8)); }
function atan2 (y, x) { console.log(Math.atan2(y, x).toFixed(8)); }

//console.log(Object.getOwnPropertyDescriptors(Math));
console.log((-0).toString(8),
            (-0).toFixed(8),
            (-0).toPrecision(8),
            (-0).toExponential(8));

abs(-Infinity); // Infinity
abs(-1); // 1
abs(-0); // 0
abs(0); // 0
abs(1); // 1
abs(Infinity); // Infinity

abs("-1"); // 1
abs(-2); // 2
abs(null); // 0
abs(" "); // 0
abs([]); // 0
abs([2]); // 2
abs([1, 2]); // NaN
abs({}); // NaN
abs("string"); // NaN
abs(); // NaN

acos(-2); // NaN
acos(-1); // 3.141592653589793 (π)
acos(0); // 1.5707963267948966 (π/2)
acos(0.5); // 1.0471975511965979 (π/3)
acos(1); // 0
acos(2); // NaN

acosh(0); // NaN
acosh(1); // 0
acosh(2); // 1.3169578969248166
acosh(Infinity); // Infinity

asin(-2); // NaN
asin(-1); // -1.5707963267948966 (-π/2)
asin(-0); // -0
asin(0); // 0
asin(0.5); // 0.5235987755982989 (π/6)
asin(1); // 1.5707963267948966 (π/2)
asin(2); // NaN

asinh(-Infinity); // -Infinity
asinh(-1); // -0.881373587019543
asinh(-0); // -0
asinh(0); // 0
asinh(1); // 0.881373587019543
asinh(Infinity); // Infinity

atan(-Infinity); // -1.5707963267948966 (-π/2)
atan(-0); // -0
atan(0); // 0
atan(1); // 0.7853981633974483  (π/4)
atan(Infinity); // 1.5707963267948966  (π/2)

atanh(-2); // NaN
atanh(-1); // -Infinity
atanh(-0); // -0
atanh(0); // 0
atanh(0.5); // 0.5493061443340548
atanh(1); // Infinity
atanh(2); // NaN

atan2(90, 15); // 1.4056476493802699
atan2(15, 90); // 0.16514867741462683
