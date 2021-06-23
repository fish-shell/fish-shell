function fish_commandline_append --description "Append the given string to the command-line, or remove the suffix if already there"
    if not commandline | string length -q
        commandline -r $history[1]
    end

    set -l escaped (string escape --style=regex -- $argv[1])
    set -l job (commandline -j | string collect)
    if set job (string replace -r -- $escaped\$ "" $job)
        set -l cursor (commandline -C)
        commandline -rj -- $job
        commandline -C $cursor
    else
        commandline -aj -- $argv[1]
    end
end
