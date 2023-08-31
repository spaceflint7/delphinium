
// ------------------------------------------------------------
//
// JavaScript-to-C compiler, first-level main program.
//
// ------------------------------------------------------------

'use strict';

// force strict mode in loading modules; inspired by
// https://github.com/isaacs/use-strict
(require('module')).wrapper[0] += '"use strict";'

if (process.argv.length === 2) {
    console.error(`usage: ${process.argv[1]} script.js`);
    process.exit(1);
}

(require('./compile/compile_file.js'))(process.argv[2]);
