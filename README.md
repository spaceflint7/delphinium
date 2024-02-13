# Delphinium

JavaScript-to-C AOT compiler.  Currently an incomplete work-in-progress, useful for compiling small test programs.

To try it out, requirements are a `bash` shell, `node`, and `gcc`.  You may need to do `npm install` to restore `node_modules`.

Compile the test program: `make test/test-hello.js`  The compiled program is generated in the `.obj` folder, try it out with: `.obj/tmp`

Although there is runtime support for some operations, the generated program is not interpreted.  Conceptually, it is somewhat similar to what a JIT compiler would generate.

There is a small test suite in folder `./test/suite` which can be run using the command `make tests`.  To clean the output `.obj` directory, use `make clean`.

Some of the stuff that is already implemented:  JS primitives, objects, properties and accessors, arrays, functions, closures, exceptions, loops and conditions, operators, destructuring assignment, bigints, generators, map and set, garbage collection.

Much still left to do: proxy and reflect, classes, promise, async, regular expressions, import/export and modules, improve conditionals, improve optimizations, improve runtime support library.
