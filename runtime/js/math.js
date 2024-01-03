
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

const _shadow_math = _shadow.math;

defineNotEnum(Math, 'abs',      _shadow_math.abs);
defineNotEnum(Math, 'acos',     _shadow_math.acos);
defineNotEnum(Math, 'acosh',    _shadow_math.acosh);
defineNotEnum(Math, 'asin',     _shadow_math.asin);
defineNotEnum(Math, 'asinh',    _shadow_math.asinh);
defineNotEnum(Math, 'atan',     _shadow_math.atan);
defineNotEnum(Math, 'atanh',    _shadow_math.atanh);
defineNotEnum(Math, 'atan2',    _shadow_math.atan2);
defineNotEnum(Math, 'ceil',     _shadow_math.ceil);
defineNotEnum(Math, 'cbrt',     _shadow_math.cbrt);
defineNotEnum(Math, 'expm1',    _shadow_math.expm1);
defineNotEnum(Math, 'clz32',    _shadow_math.clz32);
defineNotEnum(Math, 'cos',      _shadow_math.cos);
defineNotEnum(Math, 'cosh',     _shadow_math.cosh);
defineNotEnum(Math, 'exp',      _shadow_math.exp);
defineNotEnum(Math, 'floor',    _shadow_math.floor);
defineNotEnum(Math, 'fround',   _shadow_math.fround);
defineNotEnum(Math, 'hypot',    _shadow_math.hypot);
defineNotEnum(Math, 'imul',     _shadow_math.imul);
defineNotEnum(Math, 'log',      _shadow_math.log);
defineNotEnum(Math, 'log1p',    _shadow_math.log1p);
defineNotEnum(Math, 'log2',     _shadow_math.log2);
defineNotEnum(Math, 'log10',    _shadow_math.log10);
defineNotEnum(Math, 'max',      _shadow_math.max);
defineNotEnum(Math, 'min',      _shadow_math.min);
defineNotEnum(Math, 'pow',      _shadow_math.pow);
defineNotEnum(Math, 'random',   _shadow_math.random);
defineNotEnum(Math, 'round',    _shadow_math.round);
defineNotEnum(Math, 'sign',     _shadow_math.sign);
defineNotEnum(Math, 'sin',      _shadow_math.sin);
defineNotEnum(Math, 'sinh',     _shadow_math.sinh);
defineNotEnum(Math, 'sqrt',     _shadow_math.sqrt);
defineNotEnum(Math, 'tan',      _shadow_math.tan);
defineNotEnum(Math, 'tanh',     _shadow_math.tanh);
defineNotEnum(Math, 'trunc',    _shadow_math.trunc);

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

defineConfig(Math, _Symbol.toStringTag, 'Math');

// ------------------------------------------------------------

})()    // Math_init
