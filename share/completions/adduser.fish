#
# Command specific completions for the adduser command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -x -c adduser -a "(__fish_complete_users; __fish_complete_groups)"
complete -c adduser -l conf -d 'Specify config file' -r
complete -c adduser -l disabled-login -d "Don't run passwd to set the password"
complete -c adduser -l disabled-password -d "Don't set password, but allow non-password logins"
complete -c adduser -l force-badname -d 'Apply only a weak validity check of the user/group name'
complete -c adduser -l gecos -d 'Set the gecos field for the new entry generated' -r
complete -c adduser -l gid -d 'When creating a group, force groupid to be the given number' -r
complete -c adduser -l group -d 'Create a group'
complete -c adduser -l help -d 'Display brief instructions'
complete -c adduser -l home -d "Set user's home directory" -x -a '(__fish_complete_directories)'
complete -c adduser -l shell -d 'Set shell for user' -x -a '(string match --regex '^[^#].*' < /etc/shells)'
complete -c adduser -l ingroup -d 'Add the new user to GROUP' -x -a '(cut -d : -f 1 /etc/group)'
complete -c adduser -l no-create-home -d "Don't create home directory"
complete -c adduser -l quiet -d 'Only show warnings and errors'
complete -c adduser -l debug -d 'Verbose'
complete -c adduser -l system -d 'Create system user or group'
complete -c adduser -l uid -d 'Force new userid to be the given number' -r
complete -c adduser -l firstuid -d 'Override the first uid in the uid range' -r
complete -c adduser -l lastuid -d 'Override the last uid in the uid range' -r
complete -c adduser -l add_extra_groups -d 'Add new user to extra groups'
complete -c adduser -l version -d 'Display version and copyright'
