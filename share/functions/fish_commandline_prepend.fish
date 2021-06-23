function fish_commandline_prepend --description "Prepend the given string to the command-line, or remove the prefix if already there"
    if not commandline | string length -q
        commandline -r $history[1]
    end

    set -l escaped (string escape --style=regex -- $argv[1])
    set -l process (commandline -p | string collect)
    if set process (string replace -r -- "^$escaped " "" $process)
        commandline --replace -p -- $process
    else
        set -l cursor (commandline -C)
        commandline -C 0 -p
        commandline -i -- "$argv[1] "
        commandline -C (math $cursor + (string length -- "$argv[1] "))
    end
end
