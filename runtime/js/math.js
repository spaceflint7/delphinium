
// ------------------------------------------------------------
//
// Math
//
// ------------------------------------------------------------

;(function Math_init () {

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

defineNotEnum(Math, 'abs', _shadow.math.abs);
defineNotEnum(Math, 'acos', _shadow.math.acos);
defineNotEnum(Math, 'acosh', _shadow.math.acosh);
defineNotEnum(Math, 'asin', _shadow.math.asin);
defineNotEnum(Math, 'asinh', _shadow.math.asinh);
defineNotEnum(Math, 'atan', _shadow.math.atan);
defineNotEnum(Math, 'atanh', _shadow.math.atanh);
defineNotEnum(Math, 'atan2', _shadow.math.atan2);
defineNotEnum(Math, 'clz32', _shadow.math.clz32);

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
