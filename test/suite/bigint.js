'use strict';

//
// test calculations
//

let q = 0xFFFFFFF0n;
let p = 0x100000054n;
console.log(q >> 1n);
console.log(-0x1FFFFFFFFn >> 131n);
console.log(q << 1n);
console.log(p << 1n);
console.log(-0x1FFFFFFFFn << 32n);
console.log((q + 100n) - q);
console.log(q - p);
console.log(p - q);
console.log(p + q);
console.log(p * q);
console.log(0x11111n * -0x22222n);
console.log(123n + 456n);
console.log(1024n * 1024n * 1024n * 1024n * 1024n * 1024n * 1024n * 1024n);
console.log(0n - 1n);
console.log((0n - 128n) * (0n - 256n));
console.log(22222n / -11n);
console.log(2222222222222222222n / -11n);
console.log(2222222222222222222n / -11111111111n);
console.log(-2222222222222222222n % -11111111111n);
console.log(-222222 % -11111);
console.log(0x12345678BBC1703487654321D313AFB4n / 10n);
console.log(0x0E6C4CCCn / 10n);
console.log(0x12345678BBC1703487654321D313AFB412345678BBC1703487654321D313AFB412345678BBC1703487654321D313AFB412345678BBC1703487654321D313AFB412345678BBC1703487654321D313AFB412345678BBC1703487654321D313AFB412345678BBC1703487654321D313AFB412345678BBC1703487654321D313AFB412345678BBC1703487654321D313AFB4n);
console.log((-222222n) ** (6n));
console.log(120426568520230841655049121162304n * -222222n);
console.log(10973903978085048n*-2438642889818015536656n);
console.log((-222222n) ** (111n));
console.log((0x1000000000000000000000000001n|0x11110000222200001234n).toString(16));
console.log((0x1000000000000000000000000001n|-1n).toString(16));
console.log((~(0x123456780000876500004321n)).toString(16));
console.log((-(0x123456780000876500004321n)).toString(16));

// test comparisons
console.log(-1n >  -1.5);
console.log(-1n <  -1.5);
console.log(-1n >= -1.5);
console.log(-1n <= -1.5);
console.log(20 <= 10n);
console.log(20 > 10n);
console.log(-11n <  -11.5);
console.log(-11n <= -11.5);
console.log(111n <  NaN);
console.log(111n >  NaN);
console.log(111n <= NaN);
console.log(111n >= NaN);

//
// test other stuff
//

console.log(BigInt('      0x777      '));
let o777 = Object(BigInt('      0x777      '));
console.log('' + o777, ' ; ', o777);
o777.someProperty = true;
console.log(o777);
console.log(BigInt(o777));
console.log(BigInt.asIntN(11, o777), BigInt.asUintN(9, o777));

//
// test precision of printf.  if the printed result is not
// 2123399999999999919675653824207653549189961515718934528n
// then the build might references msvcrt!printf, which has
// less precision than libc printf.  in MinGW, this can be
// fixed by defining __USE_MINGW_ANSI_STDIO
//

console.log(BigInt(2.1234e54));
