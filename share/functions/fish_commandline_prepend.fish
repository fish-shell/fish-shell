function fish_commandline_prepend --description "Prepend the given string to the command-line, or remove the prefix if already there"
    if not commandline | string length -q
        commandline -r $history[1]
    end

    set -l process (commandline -p | string collect)

    set -l to_prepend "$argv[1] "
    if string match -qr '^ ' "$process"
        set to_prepend " $argv[1]"
    end

    set -l length_diff (string length -- "$to_prepend")
    set -l cursor_location (commandline -pC)

    set -l escaped (string escape --style=regex -- $to_prepend)
    if set process (string replace -r -- "^$escaped" "" $process)
        commandline --replace -p -- $process
        set length_diff "-$length_diff"
    else
        commandline -pC 0
        commandline -pi -- "$to_prepend"
    end

    commandline -pC (math "max 0,($cursor_location + $length_diff)")
end
