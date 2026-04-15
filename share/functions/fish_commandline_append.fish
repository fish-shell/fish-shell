function fish_commandline_append --description "Append the given string to the command-line, or remove the suffix if already there"
    if not commandline | string length -q
        commandline -r $history[1]
    end

    set -l suffix $argv[1]
    set -l escaped (string escape --style=regex -- $suffix)
    set -l job (commandline -j | string collect)
    set -l tokens (commandline -jx)

    # Avoid double spaces if trailing whitespace is a word separator
    set -l last_raw_char (string sub --start -1 -- "$job")
    set -l last_token_char (string sub --start -1 -- "$tokens[-1]")
    if test $last_raw_char = " " && test $last_token_char != " "
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
