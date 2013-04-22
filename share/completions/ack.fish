# Searching
complete -c ack -s i -l ignore-case -d 'Ignore case'
complete -c ack -l smart-case -d 'Ignore case when pattern contains no uppercase'
complete -c ack -l nosmart-case -l no-smart-case -d 'Don\'t ignore case'
complete -c ack -s v -l invert-match -d 'Invert match'
complete -c ack -s w -l word-regexp -d 'Match only whole words'
complete -c ack -s Q -l literal -d 'Quote all metacharacters'

# Search output
complete -c ack -l lines -d 'Only print line(s) NUM of each file'
complete -c ack -s l -l files-with-matches -d 'Only print filenames containing matches'
complete -c ack -s L -l files-without-matches -d 'Only print filenames with no matches'
complete -c ack -l output -d 'Output the evaluation of Perl expression for each line'
complete -c ack -s o -d 'Output the part of line matching pattern'
complete -c ack -l passthru -d 'Print all lines'
complete -c ack -l match -d 'Specify pattern explicitly'
complete -c ack -s m -l max-count -d 'Stop searching in each file after NUM matches'
complete -c ack -s 1 -d 'Stop searching after first match'
complete -c ack -s H -l with-filename -d 'Print the filename for each match'
complete -c ack -s h -l no-filename -d 'Suppress the prefixing filename on output'
complete -c ack -s c -l count -d 'Show number of lines matching per file'
complete -c ack -l column -d 'Show column number of first match'
complete -c ack -l nocolumn -l no-column -d 'Don\'t show column number of first match'
complete -c ack -x -s A -l after-context -d 'Print NUM lines of trailing context'
complete -c ack -x -s B -l before-context -d 'Print NUM lines of leading context'
complete -c ack -x -s C -l context -d 'Print NUM lines of context'
complete -c ack -l print0 -d 'Print null byte as separator between filenames'
complete -c ack -s s -d 'Suppress error messages about file errors'

# File presentation
complete -c ack -l pager -d 'Pipes all ack output through command'
complete -c ack -l nopager -l no-pager -d 'Do not send output through a pager'
complete -c ack -l heading -d 'Prints a filename heading above file\'s results'
complete -c ack -l noheading -l no-heading -d 'Don\'t print a filename heading above file\'s results'
complete -c ack -l break -d 'Print a break between results'
complete -c ack -l nobreak -l no-break -d 'Don\'t print a break between results'
complete -c ack -l group -d 'Filename heading and line break between results'
complete -c ack -l nogroup -l no-group -d 'No filename heading and no line breaks between results'
complete -c ack -l color -d 'Highlight the matching text'
complete -c ack -l nocolor -l no-color -l nocolour -l no-colour -d 'Don\'t highlight the matching text'
complete -c ack -l color-filename -d 'Set the color for filenames'
complete -c ack -l color-match -d 'Set the color for matches'
complete -c ack -l color-lineno -d 'Set the color for line numbers'
complete -c ack -l flush -d 'Flush output immediately'

# File finding
complete -c ack -s f -d 'Only print the files selected'
complete -c ack -s g -d 'Only select files matching pattern'
complete -c ack -l sort-files -d 'Sort the found files lexically'
complete -c ack -l show-types -d 'Show which types each file has'
complete -c ack -l files-from -d 'Read the list of files to search from file'
complete -c ack -s x -d 'Read the list of files to search from STDIN'

# File inclusion/exclusion
complete -c ack -l ignore-dir -l ignore-directory -d 'Ignore directory'
complete -c ack -l noignore-dir -l no-ignore-dir -l noignore-directory -l no-ignore-directory -d 'Don\'t ignore directory'
complete -c ack -l ignore-file -d 'Add filter for ignoring files'
complete -c ack -s r -s R -l recurse -d 'Recurse into subdirectories'
complete -c ack -s n -l no-recurse -d 'No descending into subdirectories'
complete -c ack -l follow -d 'Follow symlinks'
complete -c ack -l nofollow -l no-follow -d 'Don\'t follow symlinks'
complete -c ack -s k -l known-types -d 'Include only recognized files'
complete -c ack -l type -d 'Include only X files'

# File type specification
complete -c ack -l type-set -d 'Replaces definition of type'
complete -c ack -l type-add -d 'Specify definition of type'
complete -c ack -l type-del -d 'Removes all filters associated with type'

# Miscellaneous
complete -c ack -l noenv -l no-env -d 'Ignores environment variables and ackrc files'
complete -c ack -l ackrc -d 'Specifies location of ackrc file'
complete -c ack -l ignore-ack-defaults -d 'Ignore default definitions ack includes'
complete -c ack -l create-ackrc -d 'Outputs default ackrc'
complete -c ack -s ? -l help -d 'Shows help'
complete -c ack -l help-types -d 'Shows all known types'
complete -c ack -l dump -d 'Dump information on which options are loaded'
complete -c ack -l filter -d 'Forces ack to treat input as a pipe'
complete -c ack -l nofilter -l no-filter -d 'Forces ack to treat input as tty'
complete -c ack -l man -d 'Shows man page'
complete -c ack -l version -d 'Displays version and copyright'
complete -c ack -l thpppt -d 'Bill the Cat'
complete -c ack -l bar -d 'The warning admiral'

# File types
complete -c ack -l actionscript -d 'Search for ActionScript'
complete -c ack -l noactionscript -l no-actionscript -d 'Don\'t search for ActionScript'

complete -c ack -l ada -d 'Search for Ada'
complete -c ack -l noada -l no-ada -d 'Don\'t search for Ada'

complete -c ack -l asm -d 'Search for Assembler'
complete -c ack -l noasm -l no-asm -d 'Don\'t search for Assembler'

complete -c ack -l asp -d 'Search for ASP'
complete -c ack -l noasp -l no-asp -d 'Don\'t search for ASP'

complete -c ack -l aspx -d 'Search for ASP.NET'
complete -c ack -l noaspx -l no-aspx -d 'Don\'t search for ASP.NET'

complete -c ack -l batch -d 'Search for Batch files'
complete -c ack -l nobatch -l no-batch -d 'Don\'t search for Batch files'

complete -c ack -l cc -d 'Search for C'
complete -c ack -l nocc -l no-cc -d 'Don\'t search for C'

complete -c ack -l cfmx -d 'Search for ColdFusion'
complete -c ack -l nocfmx -l no-cfmx -d 'Don\'t search for ColdFusion'

complete -c ack -l clojure -d 'Search for Clojure'
complete -c ack -l noclojure -l no-clojure -d 'Don\'t search for Clojure'

complete -c ack -l cmake -d 'Search for CMake'
complete -c ack -l nocmake -l no-cmake -d 'Don\'t search for CMake'

complete -c ack -l cpp -d 'Search for C++'
complete -c ack -l nocpp -l no-cpp -d 'Don\'t search for C++'

complete -c ack -l csharp -d 'Search for C#'
complete -c ack -l nocsharp -l no-csharp -d 'Don\'t search for C#'

complete -c ack -l css -d 'Search for CSS'
complete -c ack -l nocss -l no-css -d 'Don\'t search for CSS'

complete -c ack -l delphi -d 'Search for Delphi'
complete -c ack -l nodelphi -l no-delphi -d 'Don\'t search for Delphi'

complete -c ack -l elisp -d 'Search for Emacs Lisp'
complete -c ack -l noelisp -l no-elisp -d 'Don\'t search for Emacs Lisp'

complete -c ack -l erlang -d 'Search for Erlang'
complete -c ack -l noerlang -l no-erlang -d 'Don\'t search for Erlang'

complete -c ack -l fortran -d 'Search for Fortran'
complete -c ack -l nofortran -l no-fortran -d 'Don\'t search for Fortran'

complete -c ack -l go -d 'Search for Go'
complete -c ack -l nogo -l no-go -d 'Don\'t search for Go'

complete -c ack -l groovy -d 'Search for Groovy'
complete -c ack -l nogroovy -l no-groovy -d 'Don\'t search for Groovy'

complete -c ack -l haskell -d 'Search for Haskell'
complete -c ack -l nohaskell -l no-haskell -d 'Don\'t search for Haskell'

complete -c ack -l hh -d 'Search for H'
complete -c ack -l nohh -l no-hh -d 'Don\'t search for H'

complete -c ack -l html -d 'Search for HTML'
complete -c ack -l nohtml -l no-html -d 'Don\'t search for HTML'

complete -c ack -l java -d 'Search for Java'
complete -c ack -l nojava -l no-java -d 'Don\'t search for Java'

complete -c ack -l js -d 'Search for JavaScript'
complete -c ack -l nojs -l no-js -d 'Don\'t search for JavaScript'

complete -c ack -l jsp -d 'Search for JavaServer Pages'
complete -c ack -l nojsp -l no-jsp -d 'Don\'t search for JavaServer Pages'

complete -c ack -l lisp -d 'Search for Lisp'
complete -c ack -l nolisp -l no-lisp -d 'Don\'t search for Lisp'

complete -c ack -l lua -d 'Search for Lua'
complete -c ack -l nolua -l no-lua -d 'Don\'t search for Lua'

complete -c ack -l make -d 'Search for Makefiles'
complete -c ack -l nomake -l no-make -d 'Don\'t search for Makefiles'

complete -c ack -l objc -d 'Search for Objective-C'
complete -c ack -l noobjc -l no-objc -d 'Don\'t search for Objective-C'

complete -c ack -l objcpp -d 'Search for Objective-C++'
complete -c ack -l noobjcpp -l no-objcpp -d 'Don\'t search for Objective-C++'

complete -c ack -l ocaml -d 'Search for Objective Caml'
complete -c ack -l noocaml -l no-ocaml -d 'Don\'t search for Objective Caml'

complete -c ack -l parrot -d 'Search for Parrot'
complete -c ack -l noparrot -l no-parrot -d 'Don\'t search for Parrot'

complete -c ack -l perl -d 'Search for Perl'
complete -c ack -l noperl -l no-perl -d 'Don\'t search for Perl'

complete -c ack -l php -d 'Search for PHP'
complete -c ack -l nophp -l no-php -d 'Don\'t search for PHP'

complete -c ack -l plone -d 'Search for Plone'
complete -c ack -l noplone -l no-plone -d 'Don\'t search for Plone'

complete -c ack -l python -d 'Search for Python'
complete -c ack -l nopython -l no-python -d 'Don\'t search for Python'

complete -c ack -l rake -d 'Search for Rakefiles'
complete -c ack -l norake -l no-rake -d 'Don\'t search for Rakefiles'

complete -c ack -l rr -d 'Search for R'
complete -c ack -l norr -l no-rr -d 'Don\'t search for R'

complete -c ack -l ruby -d 'Search for Ruby'
complete -c ack -l noruby -l no-ruby -d 'Don\'t search for Ruby'

complete -c ack -l rust -d 'Search for Rust'
complete -c ack -l norust -l no-rust -d 'Don\'t search for Rust'

complete -c ack -l scala -d 'Search for Scala'
complete -c ack -l noscala -l no-scala -d 'Don\'t search for Scala'

complete -c ack -l scheme -d 'Search for Scheme'
complete -c ack -l noscheme -l no-scheme -d 'Don\'t search for Scheme'

complete -c ack -l shell -d 'Search for Shell script'
complete -c ack -l noshell -l no-shell -d 'Don\'t search for Shell script'

complete -c ack -l smalltalk -d 'Search for Smalltalk'
complete -c ack -l nosmalltalk -l no-smalltalk -d 'Don\'t search for Smalltalk'

complete -c ack -l sql -d 'Search for SQL'
complete -c ack -l nosql -l no-sql -d 'Don\'t search for SQL'

complete -c ack -l tcl -d 'Search for Tcl'
complete -c ack -l notcl -l no-tcl -d 'Don\'t search for Tcl'

complete -c ack -l tex  -d 'Search for TeX'
complete -c ack -l notex  -l no-tex  -d 'Don\'t search for TeX'

complete -c ack -l tt -d 'Search for Template Toolkit'
complete -c ack -l nott -l no-tt -d 'Don\'t search for Template Toolkit'

complete -c ack -l vb -d 'Search for Visual Basic'
complete -c ack -l novb -l no-vb -d 'Don\'t search for Visual Basic'

complete -c ack -l verilog -d 'Search for Verilog'
complete -c ack -l noverilog -l no-verilog -d 'Don\'t search for Verilog'

complete -c ack -l vhdl -d 'Search for VHDL'
complete -c ack -l novhdl -l no-vhdl -d 'Don\'t search for VHDL'

complete -c ack -l vim -d 'Search for Vim script'
complete -c ack -l novim -l no-vim -d 'Don\'t search for Vim script'

complete -c ack -l xml -d 'Search for XML'
complete -c ack -l noxml -l no-xml -d 'Don\'t search for XML'

complete -c ack -l yaml -d 'Search for YAML'
complete -c ack -l noyaml -l no-yaml -d 'Don\'t search for YAML'
