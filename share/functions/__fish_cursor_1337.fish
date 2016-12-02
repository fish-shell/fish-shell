# Set the cursor using the '\e]1337;' sequence recognized by iTerm2 on macOS and possibly other
# terminals.
function __fish_cursor_1337 -d 'Set cursor using OSC command 1337'
    set -l shape $argv[1]
    switch "$shape"
        case block
            echo -en '\e]1337;CursorShape=0\x7'
        case underscore
            echo -en '\e]1337;CursorShape=2\x7'
        case line
            echo -en '\e]1337;CursorShape=1\x7'
    end
end
