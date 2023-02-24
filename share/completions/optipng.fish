complete -x -c optipng
complete -x -c optipng -n '__fish_should_complete_switches; and __fish_is_first_arg' -a '-?\t"show help"'
complete -x -c optipng -n '__fish_should_complete_switches; and __fish_is_first_arg' -a '-h\t"show help"'
complete -x -c optipng -n '__fish_should_complete_switches; and __fish_is_first_arg' -a '-help\t"show help"'
complete -x -c optipng -n __fish_should_complete_switches -a '-v\t"run in verbose mode"'
complete -x -c optipng -n __fish_should_complete_switches -a '-backup\t"keep a backup of the modified files"'
complete -x -c optipng -n __fish_should_complete_switches -a '-keep\t"keep a backup of the modified files"'
complete -x -c optipng -n __fish_should_complete_switches -a '-clobber\t"overwrite existing files"'
complete -x -c optipng -n __fish_should_complete_switches -a '-fix\t"enable error recovery"'
complete -x -c optipng -n __fish_should_complete_switches -a '-force\t"enforce writing of a new output file"'
complete -x -c optipng -n __fish_should_complete_switches -a '-preserve\t"preserve file attributes if possible"'
complete -x -c optipng -n __fish_should_complete_switches -a '-quiet\t"run in quiet mode"'
complete -x -c optipng -n __fish_should_complete_switches -a '-silent\t"run in quiet mode"'
complete -x -c optipng -n __fish_should_complete_switches -a '-simulate\t"run in simulation mode"'
complete -x -c optipng -n __fish_should_complete_switches -a '-full\t"produce a full IDAT report (might reduce speed)"'
complete -x -c optipng -n __fish_should_complete_switches -a '-nb\t"no bit depth reduction"'
complete -x -c optipng -n __fish_should_complete_switches -a '-nc\t"no color type reduction"'
complete -x -c optipng -n __fish_should_complete_switches -a '-np\t"no palette reduction"'
complete -x -c optipng -n __fish_should_complete_switches -a '-nx\t"no reductions"'
complete -x -c optipng -n __fish_should_complete_switches -a '-nz\t"no IDAT recoding"'
complete -x -c optipng -n __fish_should_complete_switches -a '-snip\t"cut one image out of multi-image or animation files"'
complete -x -c optipng -n __fish_should_complete_switches -a '-strip\t"strip specified metadata objects (e.g. "all")"'

for n in (seq 0 9)
    complete -x -c optipng -n __fish_should_complete_switches -a "-o$n\t\"PNG optimization level $n\""
end

complete -x -c optipng -n __fish_should_complete_switches -a '-i0\t"PNG interlace type 0"'
complete -x -c optipng -n __fish_should_complete_switches -a '-i1\t"PNG interlace type 1"'

for n in (seq 0 5)
    complete -x -c optipng -n __fish_should_complete_switches -a "-f$n\t'PNG delta filters $n'"
end

complete -x -c optipng -n __fish_should_complete_switches -a '-out\t"write output file to <file>"'
complete -x -c optipng -n __fish_should_complete_switches -a '-dir\t"write output file(s) to <directory>"'
complete -x -c optipng -n __fish_should_complete_switches -a '-log\t"log messages to <file>"'

complete -x -c optipng -n 'not __fish_prev_arg_in -out -dir -log' \
    -k -a '(__fish_complete_suffix .png .pnm .tiff .bmp)'

complete -x -c optipng -n '__fish_prev_arg_in -dir' -a '(__fish_complete_directories)'
