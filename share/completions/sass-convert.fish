# Usage: sass-convert [options] [INPUT] [OUTPUT]

# Description:
#   Converts between CSS, indented syntax, and SCSS files. For example,
#   this can convert from the indented syntax to SCSS, or from CSS to
#   SCSS (adding appropriate nesting).

# Common Options:
#     -F, --from FORMAT                The format to convert from. Can be css, scss, sass.
#                                      By default, this is inferred from the input filename.
#                                      If there is none, defaults to css.
complete -c sass-convert -s F -l from -x -a "css scss sass" -d "The format to convert from"
#     -T, --to FORMAT                  The format to convert to. Can be scss or sass.
#                                      By default, this is inferred from the output filename.
#                                      If there is none, defaults to sass.
complete -c sass-convert -s T -l to -x -a "scss sass" -d "The format to convert to"
#     -i, --in-place                   Convert a file to its own syntax.
#                                      This can be used to update some deprecated syntax.
complete -c sass-convert -s i -l in-place -d "Convert a file to its own syntax"
#     -R, --recursive                  Convert all the files in a directory. Requires --from and --to.
complete -c sass-convert -s R -l recursive -d "Convert all the files in a directory"
#     -?, -h, --help                   Show this help message.
complete -c sass-convert -s '?' -s h -l help -f -d "Show help message"
#     -v, --version                    Print the Sass version.
complete -c sass-convert -s v -l version -f -d "Print the Sass version"

# Style:
#         --dasherize                  Convert underscores to dashes.
complete -c sass-convert -l dasherize -d "Convert underscores to dashes"
#         --indent NUM                 How many spaces to use for each level of indentation. Defaults to 2.
#                                      "t" means use hard tabs.
complete -c sass-convert -l indent -x -d "How many spaces to use for each level of indentation"
#         --old                        Output the old-style ":prop val" property syntax.
#                                      Only meaningful when generating Sass.
complete -c sass-convert -l old -d "Output the old-style ':prop val' property syntax"

# Input and Output:
#     -s, --stdin                      Read input from standard input instead of an input file.
#                                      This is the default if no input file is specified. Requires --from.
complete -c sass-convert -s s -l stdin -d "Read input from standard input instead of an input file"
#     -E, --default-encoding ENCODING  Specify the default encoding for input files.
complete -c sass-convert -s E -l default-encoding -x -d "Specify the default encoding for input files"
#         --unix-newlines              Use Unix-style newlines in written files.
#                                      Always true on Unix.
complete -c sass-convert -l unix-newlines -d "Use Unix-style newlines in written files"

# Miscellaneous:
#         --cache-location PATH        The path to save parsed Sass files. Defaults to .sass-cache.
complete -c sass-convert -l cache-location -r -d "The path to save parsed Sass files"
#     -C, --no-cache                   Don't cache to sassc files.
complete -c sass-convert -s C -l no-cache -d "Don't cache to sassc files"
#         --trace                      Show a full Ruby stack trace on error
complete -c sass-convert -l trace -d "Show a full Ruby stack trace on error"
