complete -c isatty -x

if test -d /dev/fd
    complete -c isatty -k -a "(string replace /dev/fd/ '' /dev/fd/*)"
end

complete -c isatty -k -a stderr -d 2
complete -c isatty -k -a stdout -d 1
complete -c isatty -k -a stdin -d 0
