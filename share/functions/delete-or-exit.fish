#
# This function deletes a character from the commandline if it is
# non-empty, and exits the shell otherwise. Implementing this
# functionality has been a longstanding request from various
# fish-users.
#

function delete-or-exit

    set -l cmd (commandline)

    switch "$cmd"

        case ''
            exit 0

        case '*'
            commandline -f delete-char

    end

end

