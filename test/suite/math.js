'use strict';

function abs    (x) { console.log(Math.abs(x)); }
function cbrt   (x) { console.log(Math.cbrt(x).toFixed(8)); }
function ceil   (x) { console.log(Math.ceil(x)); }
function clz32  (x) { console.log(Math.clz32(x)); }
function exp    (x) { console.log(Math.exp(x).toFixed(8)); }
function expm1  (x) { console.log(Math.expm1(x).toFixed(8)); }
function floor  (x) { console.log(Math.floor(x)); }
function fround (x) { console.log(Math.fround(x)); }
function hypot  (...a) { console.log(Math.hypot.call(null,...a).toFixed(8)); }
function imul   (x, y) { console.log(Math.imul(x, y)); }
function pow    (x, y) { console.log(Math.pow(x, y).toFixed(8)); }
function round  (x) { console.log(Math.round(x)); }
function sign   (x) { console.log(Math.sign(x)); }
function sqrt   (x) { console.log(Math.sqrt(x).toFixed(8)); }
function trunc  (x) { console.log(Math.trunc(x)); }

function log   (x) { console.log(Math.log(x).toFixed(8)); }
function log1p (x) { console.log(Math.log1p(x).toFixed(8)); }
function log2  (x) { console.log(Math.log2(x).toFixed(8)); }
function log10 (x) { console.log(Math.log10(x).toFixed(8)); }

function acos  (x)      { console.log(Math.acos(x).toFixed(8)); }
function acosh (x)      { console.log(Math.acosh(x).toFixed(8)); }
function asin  (x)      { console.log(Math.asin(x).toFixed(8)); }
function asinh (x)      { console.log(Math.asinh(x).toFixed(8)); }
function atan  (x)      { console.log(Math.atan(x).toFixed(8)); }
function atanh (x)      { console.log(Math.atanh(x).toFixed(8)); }
function atan2 (y, x)   { console.log(Math.atan2(y, x).toFixed(8)); }
function cos   (x)      { console.log(Math.cos(x).toFixed(8)); }
function cosh  (x)      { console.log(Math.cosh(x).toFixed(8)); }
function sin   (x)      { console.log(Math.sin(x).toFixed(8)); }
function sinh  (x)      { console.log(Math.sinh(x).toFixed(8)); }
function tan   (x)      { console.log(Math.tan(x).toFixed(8)); }
function tanh  (x)      { console.log(Math.tanh(x).toFixed(8)); }

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

cbrt(-Infinity); // -Infinity
cbrt(-1); // -1
cbrt(-0); // -0
cbrt(0); // 0
cbrt(1); // 1
cbrt(2); // 1.2599210498948732
cbrt(Infinity); // Infinity

ceil(-Infinity); // -Infinity
ceil(-7.004); // -7
ceil(-4); // -4
ceil(-0.95); // -0
ceil(-0); // -0
ceil(0); // 0
ceil(0.95); // 1
ceil(4); // 4
ceil(7.004); // 8
ceil(Infinity); // Infinity

clz32(1); // 31
clz32(1000); // 22
clz32(); // 32

exp(-Infinity); // 0
exp(-1); // 0.36787944117144233
exp(0); // 1
exp(1); // 2.718281828459045
exp(Infinity); // Infinity

expm1(-Infinity); // -1
expm1(-1); // -0.6321205588285577
expm1(-0); // -0
expm1(0); // 0
expm1(1); // 1.718281828459045
expm1(Infinity); // Infinity

floor(-Infinity); // -Infinity
floor(-45.95); // -46
floor(-45.05); // -46
floor(-0); // -0
floor(0); // 0
floor(4); // 4
floor(45.05); // 45
floor(45.95); // 45
floor(Infinity); // Infinity

fround(1.337); // 1.3370000123977661
fround(1.337) === 1.337; // false
fround(2 ** 150); // Infinity

hypot(3, 4); // 5
hypot(3, 4, 5); // 7.0710678118654755
hypot(); // 0
hypot(NaN); // NaN
hypot(NaN, Infinity); // Infinity
hypot(3, 4, "foo"); // NaN, since +'foo' => NaN
hypot(3, 4, "5"); // 7.0710678118654755, +'5' => 5
hypot(-3); // 3, the same as Math.abs(-3)

imul(2, 4); // 8
imul(-1, 8); // -8
imul(-2, -2); // 4
imul(0xffffffff, 5); // -5
imul(0xfffffffe, 5); // -10
imul(0x7FFFFFFF, 0x7FFFFFFF); // 1

console.log(1 ** Infinity, 1 ** NaN);
pow(7, 2); // 49
pow(7, 3); // 343
pow(2, 10); // 1024
pow(4, 0.5); // 2 (square root of 4)
pow(8, 1 / 3); // 2 (cube root of 8)
pow(2, 0.5); // 1.4142135623730951 (square root of 2)
pow(2, 1 / 3); // 1.2599210498948732 (cube root of 2)
pow(7, -2); // 0.02040816326530612 (1/49)
pow(8, -1 / 3); // 0.5
pow(-7, 2); // 49 (squares are positive)
pow(-7, 3); // -343 (cubes can be negative)
pow(-7, 0.5); // NaN (negative numbers don't have a real square root)
pow(-7, 1 / 3); // NaN
pow(0, 0); // 1 (anything ** ±0 is 1)
pow(Infinity, 0.1); // Infinity (positive exponent)
pow(Infinity, -1); // 0 (negative exponent)
pow(-Infinity, 1); // -Infinity (positive odd integer exponent)
pow(-Infinity, 1.5); // Infinity (positive exponent)
pow(-Infinity, -1); // -0 (negative odd integer exponent)
pow(-Infinity, -1.5); // 0 (negative exponent)
pow(0, 1); // 0 (positive exponent)
pow(0, -1); // Infinity (negative exponent)
pow(-0, 1); // -0 (positive odd integer exponent)
pow(-0, 1.5); // 0 (positive exponent)
pow(-0, -1); // -Infinity (negative odd integer exponent)
pow(-0, -1.5); // Infinity (negative exponent)
pow(0.9, Infinity); // 0
pow(1, Infinity); // NaN
pow(1.1, Infinity); // Infinity
pow(0.9, -Infinity); // Infinity
pow(1, -Infinity); // NaN
pow(1.1, -Infinity); // 0
pow(NaN, 0); // 1
pow(NaN, 1); // NaN
pow(1, NaN); // NaN

round(-Infinity); // -Infinity
round(-20.51); // -21
round(-20.5); // -20
round(-19.51); // -20
round(-19.5); // -19
round(-0.1); // -0
round(-0.49); // -0
round(-0.5); // -0
round(-0.51); // -1
round(0); // 0
round(0.1); // 0
round(0.49); // 0
round(0.5); // 1
round(0.51); // 1
round(20.49); // 20
round(20.5); // 21
round(19.51); // 20
round(19.5); // 20
round(42); // 42
round(Infinity); // Infinity
round(NaN);

sign(3); // 1
sign(-3); // -1
sign("-3"); // -1
sign(0); // 0
sign(-0); // -0
sign(NaN); // NaN
sign("foo"); // NaN
sign(); // NaN

sqrt(-1); // NaN
sqrt(-0); // -0
sqrt(0); // 0
sqrt(1); // 1
sqrt(2); // 1.414213562373095
sqrt(9); // 3
sqrt(Infinity); // Infinity

trunc(-Infinity); // -Infinity
trunc("-1.123"); // -1
trunc(-0.123); // -0
trunc(-0); // -0
trunc(0); // 0
trunc(0.123); // 0
trunc(13.37); // 13
trunc(42.84); // 42
trunc(Infinity); // Infinity

log(-1); // NaN
log(-0); // -Infinity
log(0); // -Infinity
log(1); // 0
log(10); // 2.302585092994046
log(Infinity); // Infinity

log1p(-2); // NaN
log1p(-1); // -Infinity
log1p(-0); // -0
log1p(0); // 0
log1p(1); // 0.6931471805599453
log1p(Infinity); // Infinity

log2(-2); // NaN
log2(-0); // -Infinity
log2(0); // -Infinity
log2(1); // 0
log2(2); // 1
log2(3); // 1.584962500721156
log2(1024); // 10
log2(Infinity); // Infinity

log10(-2); // NaN
log10(-0); // -Infinity
log10(0); // -Infinity
log10(1); // 0
log10(2); // 0.3010299956639812
log10(100000); // 5
log10(Infinity); // Infinity

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

cos(-Infinity); // NaN
cos(-0); // 1
cos(0); // 1
cos(1); // 0.5403023058681398
cos(Math.PI); // -1
cos(2 * Math.PI); // 1
cos(Infinity); // NaN

cosh(-Infinity); // Infinity
cosh(-1); // 1.5430806348152437
cosh(-0); // 1
cosh(0); // 1
cosh(1); // 1.5430806348152437
cosh(Infinity); // Infinity

sin(-Infinity); // NaN
sin(-0); // -0
sin(0); // 0
sin(1); // 0.8414709848078965
sin(Math.PI / 2); // 1
sin(Infinity); // NaN

sinh(-Infinity); // -Infinity
sinh(-0); // -0
sinh(0); // 0
sinh(1); // 1.1752011936438014
sinh(Infinity); // Infinity

tan(-Infinity); // NaN
tan(-0); // -0
tan(0); // 0
tan(1); // 1.5574077246549023
tan(Math.PI / 4); // 0.9999999999999999 (Floating point error)
tan(Infinity); // NaN

tanh(-Infinity); // -1
tanh(-0); // -0
tanh(0); // 0
tanh(1); // 0.7615941559557649
tanh(Infinity); // 1

// test uniform distribution of Math.random around 0.5.
// at 100,000 iterations, result should be around 50000.
let sum = 0;
let count = 100000;
for (let i = 0; i < count; i++)
    sum += Math.random();
console.log(count, sum > 49000, sum < 51000);

// min and max
console.log('max', Math.max(), Math.max(1, NaN), Math.max(10, 200, -Infinity, 5));
console.log('min ', Math.min(), Math.min(1, NaN), Math.min(10, 200, +Infinity, 5));
