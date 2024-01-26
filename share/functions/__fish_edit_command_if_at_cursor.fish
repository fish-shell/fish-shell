function __fish_edit_command_if_at_cursor --description 'If cursor is at the command token, edit the command source file'
    set -l editor $argv
    set -l tokens (commandline -xpc)
    set -l command
    if not set -q tokens[1]
        set command (commandline -xp)[1]
    else if test (count $tokens) = 1 && test -z "$(commandline -t)"
        set command $tokens[1]
    end

    set -q command[1]
    or return 1
    set -l command_path (command -v -- $command)
    or return 1
    test -w $command_path
    or return 1
    string match -q 'text/*' (file --brief --mime-type -- $command_path)
    or return 1

    __fish_disable_bracketed_paste
    $editor $command_path
    __fish_enable_bracketed_paste
    return 0
end
