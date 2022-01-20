# Usage: sass [options] [INPUT] [OUTPUT]

# Description:
#   Converts SCSS or Sass files to CSS.

# Common Options:
#     -I, --load-path PATH             Specify a Sass import path.
complete -c sass -s I -l load-path -r -d "Specify a import path"
#     -r, --require LIB                Require a Ruby library before running Sass.
complete -c sass -s r -l require -x -d "Require a Ruby library before running"
#         --compass                    Make Compass imports available and load project configuration.
complete -c sass -l compass -d "Enable Compass imports and load project configuration"
#     -t, --style NAME                 Output style. Can be nested (default), compact, compressed, or expanded.
complete -c sass -s t -l style -x -a "nested compact compressed expanded" -d "Output style"
#     -?, -h, --help                   Show this help message.
complete -c sass -s '?' -s h -l help -f -d "Show help message"
#     -v, --version                    Print the Sass version.
complete -c sass -s v -l version -f -d "Print the version"

# Watching and Updating:
#         --watch                      Watch files or directories for changes.
#                                      The location of the generated CSS can be set using a colon:
#                                        sass --watch input.sass:output.css
#                                        sass --watch input-dir:output-dir
complete -c sass -l watch -d "Watch files or directories for changes"
#         --poll                       Check for file changes manually, rather than relying on the OS.
#                                      Only meaningful for --watch.
complete -c sass -l poll -d "Check for file changes manually, don't rely on OS"
#         --update                     Compile files or directories to CSS.
#                                      Locations are set like --watch.
complete -c sass -l update -d "Compile files or directories to CSS"
#     -f, --force                      Recompile every Sass file, even if the CSS file is newer.
#                                      Only meaningful for --update.
complete -c sass -s f -l force -d "Recompile every Sass file, even if the CSS file is newer"
#         --stop-on-error              If a file fails to compile, exit immediately.
#                                      Only meaningful for --watch and --update.
complete -c sass -l stop-on-error -d "If a file fails to compile, exit immediately"

# Input and Output:
#         --scss                       Use the CSS-superset SCSS syntax.
complete -c sass -l scss -d "Use the CSS-superset SCSS syntax."
#         --sourcemap=TYPE             How to link generated output to the source files.
#                                        auto (default): relative paths where possible, file URIs elsewhere
#                                        file: always absolute file URIs
#                                        inline: include the source text in the sourcemap
#                                        none: no sourcemaps
complete -c sass -l sourcemap -x -a "auto\t'(default) relative paths where possible, file URIs elsewhere'
sfile\t'always absolute file URIs'
inline\t'include the source text in the sourcemap'
none\t'no sourcemaps'" -d "How to link generated output to the source files"
#     -s, --stdin                      Read input from standard input instead of an input file.
#                                      This is the default if no input file is specified.
complete -c sass -s s -l stdin -d "Read input from standard input instead of an input file"
#     -E, --default-encoding ENCODING  Specify the default encoding for input files.
complete -c sass -s E -l default-encoding -x -d "Specify the default encoding for input files"
#         --unix-newlines              Use Unix-style newlines in written files.
#                                      Always true on Unix.
complete -c sass -l unix-newlines -d "Use Unix-style newlines in written files"
#     -g, --debug-info                 Emit output that can be used by the FireSass Firebug plugin.
complete -c sass -s g -l debug-info -d "Emit output that can be used by the FireSass Firebug plugin"
#     -l, --line-numbers               Emit comments in the generated CSS indicating the corresponding source line.
#         --line-comments
complete -c sass -s l -l line-numbers -l line-comments -d "Indicate corresponding source line with comments"

# Miscellaneous:
#     -i, --interactive                Run an interactive SassScript shell.
complete -c sass -s i -l interactive -d "Run an interactive SassScript shell"
#     -c, --check                      Just check syntax, don't evaluate.
complete -c sass -s c -l check -d "Just check syntax, don't evaluate"
#         --precision NUMBER_OF_DIGITS How many digits of precision to use when outputting decimal numbers.
#                                      Defaults to 5.
complete -c sass -l precision -x -d "Set precision when outputting decimal numbers"
#         --cache-location PATH        The path to save parsed Sass files. Defaults to .sass-cache.
complete -c sass -l cache-location -r -d "The path to save parsed Sass files"
#     -C, --no-cache                   Don't cache parsed Sass files.
complete -c sass -s C -l no-cache -d "Don't cache parsed Sass files"
#         --trace                      Show a full Ruby stack trace on error.
complete -c sass -l trace -d "Show a full Ruby stack trace on error"
#     -q, --quiet                      Silence warnings and status messages during compilation.
complete -c sass -s q -l quiet -d "Silence warnings and status messages during compilation"
