#
# Command specific completions for the useradd command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c useradd -s c -l comment --description 'A comment about this user' -r
complete -c useradd -s d -l home --description 'Home directory for the new user' -x -a '(__fish_complete_directories)'
complete -c useradd -s G -l groups --description 'Supplementary groups' -xa '(__fish_append , (cat /etc/group|cut -d : -f 1))'
complete -c useradd -s h -l help --description 'Display help message and exit'
complete -c useradd -s m -l create-home --description 'The user\'s home directory will be created if it does not exist'
complete -c useradd -s n --description 'A group having the same name as the user being added to the system will be created by default (when -g is not specified)'
complete -c useradd -s K -l key --description 'Overrides default key/value pairs from /etc/login'
complete -c useradd -s o -l non-unique --description 'Allow the creation of a user account with a duplicate (non-unique) UID'
complete -c useradd -s p -l password --description 'The encrypted password, as returned by crypt(3)' -r
complete -c useradd -s u -l uid --description 'The numerical value of the user\'s ID' -r
complete -c useradd -s b -l base-dir --description 'The initial path prefix for a new user\'s home directory' -r -a '(__fish_complete_directories)'
complete -c useradd -s e -l expiredate --description 'The date on which the user account is disabled' -r
complete -c useradd -s f -l inactive --description 'The number of days after a password has expired before the account will be disabled' -r
complete -c useradd -s g -l gid --description 'The group name or ID for a new user\'s initial group' -x -a '(string match -r "^[^#].*" < /etc/group | cut -d : -f 1,3 | string replace -a ":" \n)'
complete -c useradd -s s -l shell --description 'Name of the new user\'s login shell' -x -a '(string match -r "^[^#].*" < /etc/shells)'
