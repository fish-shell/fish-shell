#disable file seletion
complete -f tuned-adm

#get profile list
set profiles (tuned-adm list 2>&1 < /dev/null  | awk  '{print $2}' | tail -n +2 | head -n -1)

# all tuned-adm actions
set -l actions list available profiles active off profile profile_info recommend verify auto_profile profile_mode

#first argument
complete -k -c tuned-adm -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
    -xa "$actions"

# second argument for profile subcommand
complete -k -c tuned-adm -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from profile" \
    -xa "$profiles"

#options
complete -k -c tuned-adm -s h -l help -d "show this help message and exit"
complete -k -c tuned-adm -s v -l version -d "show program's version number and exit"
complete -k -c tuned-adm -s d -l debug -d "show debug messages"
complete -k -c tuned-adm -s a -l async -d "with dbus do not wait on commands completion and return immediately"
complete -k -c tuned-adm -s t -l timeout -d "with sync operation use specific timeout instead of the default 600 second(s)"
complete -k -c tuned-adm -s l -l loglevel -d "level of log messages to capture"
