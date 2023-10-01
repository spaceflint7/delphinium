
// ------------------------------------------------------------
//
// Math
//
// ------------------------------------------------------------

;(function BigInt_init () {

const Math = {};

// ------------------------------------------------------------
//
// constants in global
//
// ------------------------------------------------------------

defineProperty(_global, 'NaN',      { value: NaN });
defineProperty(_global, 'Infinity', { value: 1 / 0 });

defineNotEnum(_global,  'Math', Math);

// ------------------------------------------------------------
//
// functions in Math
//
// ------------------------------------------------------------

defineConfig(Math, 'abs', function abs (x) {
    return (x === 0) ? 0 : ((x > 0) ? x : -x) });

// ------------------------------------------------------------
//
// constants in Math
//
// ------------------------------------------------------------

defineProperty(Math,    'E',        { value: 2.718281828459045 });
defineProperty(Math,    'LN10',     { value: 2.302585092994046 });
defineProperty(Math,    'LN2',      { value: 0.6931471805599453 });
defineProperty(Math,    'LOG10E',   { value: 0.4342944819032518 });
defineProperty(Math,    'LOG2E',    { value: 1.4426950408889634 });
defineProperty(Math,    'PI',       { value: 3.141592653589793 });
defineProperty(Math,    'SQRT1_2',  { value: 0.7071067811865476 });
defineProperty(Math,    'SQRT2',    { value: 1.4142135623730951 });

defineConfig(Math, Symbol.toStringTag, 'Math');

// ------------------------------------------------------------

})()    // Math_init