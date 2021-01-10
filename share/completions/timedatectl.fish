set -l commands status show set-time set-timezone list-timezones set-local-rtc set-ntp timesync-status show-timesync

complete -c timedatectl -f
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a status -d 'Show current time settings'
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a show -d 'Show properties of systemd-timedated'
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a set-time -d 'Set system time'
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a set-timezone -d 'Set system time zone'
complete -c timedatectl -n "__fish_seen_subcommand_from set-timezone" -a "(timedatectl list-timezones)"
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a list-timezones -d 'Show known time zones'
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a set-local-rtc -d 'Control whether RTC is in local time'
complete -c timedatectl -n "__fish_seen_subcommand_from set-local-rtc" -a 'true false'
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a set-ntp -d 'Control network time sync'
complete -c timedatectl -n "__fish_seen_subcommand_from set-ntp" -a 'true false'
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a timesync-status -d 'Show status of systemd-timesyncd'
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a show-timesync -d 'Show properties of systemd-timesyncd'

complete -c timedatectl -s h -l help -d 'Show this help message'
complete -c timedatectl -l version -d 'Show package version'
complete -c timedatectl -l no-pager -d 'Do not pipe output into a pager'
complete -c timedatectl -l no-ask-password -d 'Do not prompt for password'
complete -c timedatectl -s H -l host -r -d 'Operate on remote HOST'
complete -c timedatectl -s M -l machine -r -d 'Operate on local CONTAINER'
complete -c timedatectl -l adjust-system-clock -d 'Adjust system clock when changing local RTC mode'
complete -c timedatectl -l monitor -d 'Monitor status of systemd-timesyncd'
complete -c timedatectl -s p -l property -r -d 'Show only properties by this NAME'
complete -c timedatectl -s a -l all -d 'Show all properties'
complete -c timedatectl -l value -d 'Only show properties with values'
