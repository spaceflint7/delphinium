
// ------------------------------------------------------------
//
// JavaScript-to-C compiler, third-level main program.
//
// called by compile_to_c.js.
//
// ------------------------------------------------------------

function translate_whole_file (top_level_stmt) {

    const output = [];

    const functions =
        (require('./function_splitter.js'))(top_level_stmt);

    (require('./variable_resolver.js'))(functions);

    const literals_collection = new (require('./literals.js'));
    const volatile_scanner = require('./volatile_scanner.js');
    const function_writer = require('./function_writer.js');

    output.push(`#include "runtime.h"`);

    functions.forEach(func => {
        function_writer.write_function_forward(func, output);
        literals_collection.process_function(func);
    });

    literals_collection.write_initialization(output);

    for (var func of functions) {
        if (func.has_try_block)
            volatile_scanner(func);
        if (func.generator || func.async)
            function_writer.convert_to_coroutine(func);
        function_writer.write_function(func, output);
    }

    function_writer.write_module_entry_point(
        functions.find(func => !func.parent_func), output);

    return output;
}

// ------------------------------------------------------------

function sort_top_level_stmts (program_node) {

    // sort the contents of the program node in two groups:
    // statements that declare module import or export info;
    // and all other statements, which can be placed into a
    // top-level function.

    if (program_node.type === 'Program'
    && Array.isArray(program_node.body)) {

        const top_level_stmts = [];
        const import_export_stmts = [];

        for (const node of program_node.body) {
            if (typeof(node.type) === 'string') {
                const array_to_push =
                    (node.type.endsWith("Declaration") &&
                        (    node.type.startsWith("Import")
                          || node.type.startsWith("Export")))
                    ? import_export_stmts : top_level_stmts;
                array_to_push.push(node);
            }
        }

        return [ top_level_stmts, import_export_stmts ];
    }

    return []; // [ undefined, undefined ]
}

// ------------------------------------------------------------

function convert_output_to_text (output) {

    let last_line, last_column;

    for (const line of output) {

        if (typeof(line) === 'string')
            console.log(line);

        else if (line?.start) {

            const this_line = line.start.line;
            const this_column = line.start.column + 1;

            if (this_column !== last_column) {

                console.log(`// line ${this_line} column ${this_column}`);
                last_line = this_line;
                last_column = this_column;

            } else if (this_line !== last_line) {

                console.log(`// line ${this_line}`);
                last_line = this_line;
            }
        }
    }
}

// ------------------------------------------------------------

module.exports = function compiler (program_node, source_path) {

    require('./utils').set_unique_id_from_text(source_path);

    try {

        let [ top_level_stmts, import_export_stmts ] =
                            sort_top_level_stmts(program_node);

        if (Array.isArray(top_level_stmts)) {

            const output = translate_whole_file({

                type: 'FunctionDeclaration',
                params: [],
                body: {
                    type: 'BlockStatement',
                    body: top_level_stmts },
                loc: program_node.loc,
                filename: (require('node:path'))
                            .basename(source_path),
            });

            convert_output_to_text(output);

        } else {

            throw [ program_node, 'parse error' ];
        }

    } catch (errobj) {

        if (Array.isArray(errobj)
        && errobj.length === 2
        && typeof(errobj[0]?.type) === 'string'
        && typeof(errobj[1] === 'string')) {

            let line = '?', column = '?';
            if (errobj[0].loc?.start) {
                line = errobj[0].loc.start.line;
                column = errobj[0].loc.start.column + 1;
            }

            console.error('\n===> error in'
              + ` ${source_path}:${line}:${column}:`
              + ` ${errobj[1]}\n`);
            process.exit(1);

        } else if (errobj)
            throw errobj;
    }
}
