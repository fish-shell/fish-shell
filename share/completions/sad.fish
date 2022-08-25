complete sad -s 0 -l read0 -d 'Use \0 as stdin delimiter'
complete sad -s e -l exact -d 'String literal mode'
complete sad -x -s f -l flags -d "Regex flags: use `--help` instead of `-h` to see details"
complete sad -x -l fzf -a never -d "Additional Fzf options, disable = never"
complete sad -s h -d 'Print short help information'
complete sad -l help -d 'Print detailed help information'
complete sad -s k -l commit -d 'No preview, write changes to file'
complete sad -x -s p -l pager -a never -d 'Colourizing program, disable = never, default = $GIT_PAGER'
complete sad -x -s u -l unified -a "0 1 2 3 4 5" -d 'Same as in GNU diff --unified={size}, affects aggregate size'
complete sad -x -s V -l version -d 'Print version information'
