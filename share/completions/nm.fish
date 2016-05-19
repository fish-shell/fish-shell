complete -c nm -s a -l debug-syms -d 'Display debugger-only symbols'
complete -c nm -s A -l print-file-name -d 'Print name of the input file before every symbol'
#complete -c nm -C -l demangle[=STYLE] Decode low-level symbol names into user-level names
#                     Same as --format=bsd
#                     The STYLE, if specified, can be `auto' (the default),
#                    `gnu', `lucid', `arm', `hp', `edg', `gnu-v3', `java' or `gnat'
complete -c nm      -l no-demangle       -d 'Do not demangle low-level symbol names'
complete -c nm -s D -l dynamic           -d 'Display dynamic symbols instead of normal symbols'
complete -c nm      -l defined-only      -d 'Display only defined symbols'
complete -c nm -s f -l format            -d 'Use the output format FORMAT. The default is "bsd"' -a 'bsd sysv posix'
complete -c nm -s g -l extern-only       -d 'Display only external symbols'
complete -c nm -s l -l line-numbers      -d 'Use debugging information to find a filename and line number for each symbol'
complete -c nm -s n -l numeric-sort      -d 'Sort symbols numerically by address'
complete -c nm -s o                      -d 'Print name of the input file before every symbol'
complete -c nm -s p -l no-sort           -d 'Do not sort the symbols'
complete -c nm -s P -l portability       -d 'Same as --format=posix'
complete -c nm -s r -l reverse-sort      -d 'Reverse the sense of the sort'
complete -c nm      -l plugin            -d 'Load the specified plugin'
complete -c nm -s S -l print-size        -d 'Print size of defined symbols'
complete -c nm -s s -l print-armap       -d 'Include index for symbols from archive members'
complete -c nm      -l size-sort         -d 'Sort symbols by size'
complete -c nm      -l special-syms      -d 'Include special symbols in the output'
complete -c nm      -l synthetic         -d 'Display synthetic symbols as well'
complete -c nm -s t -l radix             -d 'Use RADIX for printing symbol values'
complete -c nm      -l target            -d 'Specify the target object format as BFDNAME'
complete -c nm -s u -l undefined-only    -d 'Display only undefined symbols'
complete -c nm -s h -l help              -d 'Display this information'
complete -c nm -s V -l version           -d "Display this program's version number"

