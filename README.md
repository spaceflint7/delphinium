# Delphinium

JavaScript-to-C AOT compiler.  Currently an incomplete work-in-progress, useful for compiling small test programs.

To try it out, requirements are a `bash` shell, `node` on the path, and `gcc` installed in `/gcc`.  You may need to do `npm install` to restore `node_modules`.

Compile the test program: `./run.sh test/test-hello.js`  (The name 'run' here means run the compiler.)  The compiled program is generated in the `.obj` folder, try it out with: `.obj/tmp`

Although there is runtime support for some operations, the generated program is not interpreted.  Conceptually, it is somewhat similar to what a JIT compiler would generate.

There is a small test suite in folder `./test/suite` and a script to run all the tests in `./test/run-suite.sh`.

Some of the stuff that is already implemented:  JS primitives, objects, properties and accessors, arrays, functions, closures, exceptions, loops and conditions, operators.

Much still left to do: garbage collection, destructuring, proxy and reflect, classes, generator, promise, async, labeled statements, map and set, regular expressions, import/export and modules, improve conditionals, improve optimizations, improve runtime support library.
