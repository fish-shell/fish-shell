set -l command batsh

complete -c $command -f

complete -c $command -s h -l help \
    -a 'pager\tdefault plain groff' \
    -d 'Show help'

complete -c $command -s v -l version -d 'Show version'

complete -c $command \
    -a 'bash\t"Compile to Bash" batsh\t"Format file" winbat\t"Compile to Batch"'
