
// ------------------------------------------------------------
//
// JavaScript-to-C compiler, second-level main program.
//
// called by index.js in project root.
//
// ------------------------------------------------------------

module.exports = function read_and_compile_file (file_path) {

    //
    // parse the source file to a syntax tree using Esprima
    //

    let parsed_node, text_length;
    try {
        const file_text =
                require('fs').readFileSync(file_path).toString();
        text_length = file_text.length;
        // parseModule for strict mode, parseScript for sloppy mode
        parsed_node =
            require('../node_modules/esprima-next/dist/esprima.js')
                .parseScript(file_text, { loc: true });

    } catch (err) {

        // esprima has poor detection of unterminated comment
        if (err.description === 'Unexpected token ILLEGAL'
               && err.index === text_length) {
            err.description += ', unterminated comment?';
        }

        let msg = (err.description && err.lineNumber)
                ? `${err.lineNumber}:${err.column}: ${err.description}`
                : err.message;
        console.error(`\n===> error in ${file_path}: ${msg}\n`);
        process.exit(1);
    }

    //
    // translate the parsed syntax tree into c language
    //

    return require('./compile_to_c')(parsed_node, file_path);
}
