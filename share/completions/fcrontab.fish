#
# Command specific completions for the fcrontab command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c fcrontab -s u --description 'User Specify the user whose fcrontab will be managed, or "systab" for the system fcrontab'
complete -c fcrontab -s l --description 'List user\'s current fcrontab to standard output'
complete -c fcrontab -s e --description 'Edit user\'s current fcrontab using either the editor specified by the environment variable VISUAL, or EDITOR if VISUAL is not set'
complete -c fcrontab -s r --description 'Remove user\'s fcrontab'
complete -c fcrontab -s z --description 'Reinstall user\'s fcrontab from its source code'
complete -c fcrontab -s n --description 'Ignore previous version'
complete -c fcrontab -s c --description 'File Make fcrontab use config file file instead of default config file /usr/local/etc/fcron'
complete -c fcrontab -s d --description 'Run in debug mode'
complete -c fcrontab -s h --description 'Display a brief description of the options'
complete -c fcrontab -s V --description 'Display an informational message about fcrontab, including its version and the license under which it is distributed'
