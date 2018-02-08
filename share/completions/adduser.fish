#
# Command specific completions for the adduser command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -x -c adduser -a "(__fish_complete_users; __fish_complete_groups)"
complete -c adduser -l conf -d 'Specify config file' -r
complete -c adduser -l disabled-login -d 'Do not run passwd to set the password'
complete -c adduser -l disabled-password -d 'Like --disabled-login, but logins are still possible (for example using SSH RSA keys) but not using password authentication'
complete -c adduser -l force-badname -d 'By default, user and group names are checked against the configurable regular expression NAME_REGEX (or NAME_REGEX if --system is specified) specified in the configuration file'
complete -c adduser -l gecos -d 'Set the gecos field for the new entry generated' -r
complete -c adduser -l gid -d 'When creating a group, this option forces the new groupid to be the given number' -r
complete -c adduser -l group -d 'When combined with --system, a group with the same name and ID as the system user is created'
complete -c adduser -l help -d 'Display brief instructions'
complete -c adduser -l home -d 'Use specified directory as the user\'s home directory' -x -a '(__fish_complete_directories)'
complete -c adduser -l shell -d 'Use shell as the user\'s login shell, rather than the default specified by the configuration file' -x -a '(cat /etc/shells)'
complete -c adduser -l ingroup -d 'Add the new user to GROUP instead of a usergroup or the default group defined by USERS_GID in the configuration file' -x -a '(cut -d : -f 1 /etc/group)'
complete -c adduser -l no-create-home -d 'Do not create the home directory, even if it doesni\'t exist'
complete -c adduser -l quiet -d 'Suppress informational messages, only show warnings and errors'
complete -c adduser -l debug -d 'Be verbose, most useful if you want to nail down a problem with adduser'
complete -c adduser -l system -d 'Create a system user or group'
complete -c adduser -l uid -d 'Force the new userid to be the given number' -r
complete -c adduser -l firstuid -d 'Override the first uid in the range that the uid is chosen from (overrides FIRST_UID specified in the configuration file)' -r
complete -c adduser -l lastuid -d 'ID Override the last uid in the range that the uid is chosen from ( LAST_UID )' -r
complete -c adduser -l add_extra_groups -d 'Add new user to extra groups defined in the configuration file'
complete -c adduser -l version -d 'Display version and copyright information'
