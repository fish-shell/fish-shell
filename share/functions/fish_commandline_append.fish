function fish_commandline_append --description 'Append the given string to the command-line, or remove the suffix if already there'
    if not commandline | string length -q
        commandline -r $history[1]
    end

    set -l suffix $argv[1]
    set -l escaped (string escape --style=regex -- $suffix)
    set -l job (commandline -j | string collect)

    if test (string sub --start -1 "$job") = " "
        set suffix (string trim --left -- $suffix)
    end

    if set job (string replace -r -- $escaped\$ "" $job)
        set -l cursor (commandline -C)
        commandline -rj -- $job
        commandline -C $cursor
    else
        commandline -aj -- $suffix
    end
end
