# Usage: scss [options] [INPUT] [OUTPUT]

# Description:
#   Converts SCSS or Sass files to CSS.

# Common Options:
#     -I, --load-path PATH             Specify a Sass import path.
complete -c scss -s I -l load-path -r -d "Specify a Sass import path"
#     -r, --require LIB                Require a Ruby library before running Sass.
complete -c scss -s r -l require -r -d "Require a Ruby library before running Sass"
#         --compass                    Make Compass imports available and load project configuration.
complete -c scss -l compass -d "Enable Compass imports and load project configuration"
#     -t, --style NAME                 Output style. Can be nested (default), compact, compressed, or expanded.
complete -c scss -s t -l style -x -a "nested compact compressed expanded" -d "Output style"
#     -?, -h, --help                   Show this help message.
complete -c scss -s '?' -s h -l help -f -d "Show help message"
#     -v, --version                    Print the Sass version.
complete -c scss -s v -l version -f -d "Print the Sass version"

# Watching and Updating:
#         --watch                      Watch files or directories for changes.
#                                      The location of the generated CSS can be set using a colon:
#                                        scss --watch input.scss:output.css
#                                        scss --watch input-dir:output-dir
complete -c scss -l watch -d "Watch files or directories for changes"
#         --poll                       Check for file changes manually, rather than relying on the OS.
#                                      Only meaningful for --watch.
complete -c scss -l poll -d "Check for file changes manually, don't rely on OS"
#         --update                     Compile files or directories to CSS.
#                                      Locations are set like --watch.
complete -c scss -l update -d "Compile files or directories to CSS"
#     -f, --force                      Recompile every Sass file, even if the CSS file is newer.
#                                      Only meaningful for --update.
complete -c scss -s f -l force -d "Recompile every Sass file, even if the CSS file is newer"
#         --stop-on-error              If a file fails to compile, exit immediately.
#                                      Only meaningful for --watch and --update.
complete -c scss -l stop-on-error -d "If a file fails to compile, exit immediately"

# Input and Output:
#         --sass                       Use the indented Sass syntax.
complete -c scss -l sass -d "Use the indented Sass syntax"
#         --sourcemap=TYPE             How to link generated output to the source files.
#                                        auto (default): relative paths where possible, file URIs elsewhere
#                                        file: always absolute file URIs
#                                        inline: include the source text in the sourcemap
#                                        none: no sourcemaps
complete -c scss -l sourcemap -x -d "How to link generated output to the source files" -a \
    "auto\t'(default) relative paths where possible, file URIs elsewhere'
file\t'always absolute file URIs'
inline\t'include the source text in the sourcemap'
none\t'no sourcemaps'"
#     -s, --stdin                      Read input from standard input instead of an input file.
#                                      This is the default if no input file is specified.
complete -c scss -s s -l stdin -d "Read input from standard input instead of an input file"
#     -E, --default-encoding ENCODING  Specify the default encoding for input files.
complete -c scss -s E -l default-encoding -x -d "Specify the default encoding for input files"
#         --unix-newlines              Use Unix-style newlines in written files.
#                                      Always true on Unix.
complete -c scss -l unix-newlines -d "Use Unix-style newlines in written files"
#     -g, --debug-info                 Emit output that can be used by the FireSass Firebug plugin.
complete -c scss -s g -l debug-info -d "Emit output that can be used by the FireSass Firebug plugin"
#     -l, --line-numbers               Emit comments in the generated CSS indicating the corresponding source line.
#         --line-comments
complete -c scss -s l -l line-numbers -l line-comments -d "Indicate corresponding source line with comments"

# Miscellaneous:
#     -i, --interactive                Run an interactive SassScript shell.
complete -c scss -s i -l interactive -d "Run an interactive SassScript shell"
#     -c, --check                      Just check syntax, don't evaluate.
complete -c scss -s c -l check -d "Just check syntax, don't evaluate"
#         --precision NUMBER_OF_DIGITS How many digits of precision to use when outputting decimal numbers.
#                                      Defaults to 5.
complete -c scss -l precision -x -d "Set precision when outputting decimal numbers"
#         --cache-location PATH        The path to save parsed Sass files. Defaults to .sass-cache.
complete -c scss -l cache-location -r -d "The path to save parsed Sass files"
#     -C, --no-cache                   Don't cache parsed Sass files.
complete -c scss -s C -l no-cache -d "Don't cache parsed Sass files"
#         --trace                      Show a full Ruby stack trace on error.
complete -c scss -l trace -d "Show a full Ruby stack trace on error"
#     -q, --quiet                      Silence warnings and status messages during compilation.
complete -c scss -s q -l quiet -d "Silence warnings and status messages during compilation"
