# This is meant to be bound to key sequences such as \e#. It provides a simple way to quickly
# comment/uncomment the current command. This is something introduced by the Korn shell (ksh) in
# 1993. It allows you to capture a command in the shell history without executing it. Then
# retrieving the command from the shell history and removing the comment chars.
#
# This deliberately does not execute the command when removing the comment characters to give you an
# opportunity to modify the command. Also if the commandline is empty, the most recently commented
# out history item is uncommented and presented.

function __fish_toggle_comment_commandline --description 'Comment/uncomment the current command'
    set -l cmdlines (commandline -b)
    if test -z "$cmdlines"
        set cmdlines (history search -p "#" --max=1)
    end
    set -l cmdlines (printf '%s\n' '#'$cmdlines | string replace -r '^##' '')
    commandline -r $cmdlines
    string match -q '#*' $cmdlines[1]
    and commandline -f execute
end
