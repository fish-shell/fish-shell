#
# Command specific completions for the adduser command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -x -c adduser -a "(__fish_complete_users; __fish_complete_groups)"
complete -c adduser -l conf --description 'Specify config file' -r
complete -c adduser -l disabled-login --description 'Do not run passwd to set the password'
complete -c adduser -l disabled-password --description 'Like --disabled-login, but logins are still possible (for example using SSH RSA keys) but not using password authentication'
complete -c adduser -l force-badname --description 'By default, user and group names are checked against the configurable regular expression NAME_REGEX (or NAME_REGEX if --system is specified) specified in the configuration file'
complete -c adduser -l gecos --description 'Set the gecos field for the new entry generated' -r
complete -c adduser -l gid --description 'When creating a group, this option forces the new groupid to be the given number' -r
complete -c adduser -l group --description 'When combined with --system, a group with the same name and ID as the system user is created'
complete -c adduser -l help --description 'Display brief instructions'
complete -c adduser -l home --description 'Use specified directory as the user\'s home directory' -x -a '(__fish_complete_directories)'
complete -c adduser -l shell --description 'Use shell as the user\'s login shell, rather than the default specified by the configuration file' -x -a '(cat /etc/shells)'
complete -c adduser -l ingroup --description 'Add the new user to GROUP instead of a usergroup or the default group defined by USERS_GID in the configuration file' -x -a '(cut -d : -f 1 /etc/group)'
complete -c adduser -l no-create-home --description 'Do not create the home directory, even if it doesni\'t exist'
complete -c adduser -l quiet --description 'Suppress informational messages, only show warnings and errors'
complete -c adduser -l debug --description 'Be verbose, most useful if you want to nail down a problem with adduser'
complete -c adduser -l system --description 'Create a system user or group'
complete -c adduser -l uid --description 'Force the new userid to be the given number' -r
complete -c adduser -l firstuid --description 'Override the first uid in the range that the uid is chosen from (overrides FIRST_UID specified in the configuration file)' -r
complete -c adduser -l lastuid --description 'ID Override the last uid in the range that the uid is chosen from ( LAST_UID )' -r
complete -c adduser -l add_extra_groups --description 'Add new user to extra groups defined in the configuration file'
complete -c adduser -l version --description 'Display version and copyright information'
