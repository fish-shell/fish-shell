set -l commands status set-time{,zone} list-timezones set-local-rtc set-ntp

complete -c timedatectl -f
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a "status"
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a "set-time"
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a "set-timezone"
complete -c timedatectl -n "__fish_seen_subcommand_from set-timezone" -a "(timedatectl list-timezones)"
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a "list-timezones"
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a "set-local-rtc" -d "Maintain RTC in local time"
complete -c timedatectl -n "__fish_seen_subcommand_from set-local-rtc" -a "true false"
complete -c timedatectl -n "__fish_seen_subcommand_from set-local-rtc" -l adjust-system-clock --description 'Synchronize system clock from the RTC'
complete -c timedatectl -n "not __fish_seen_subcommand_from $commands" -a "set-ntp" -d "Use NTP"
complete -c timedatectl -n "__fish_seen_subcommand_from set-ntp" -a "true false"
complete -c timedatectl -l no-ask-password --description "Don't ask for password"
complete -c timedatectl -s H -l host --description 'Execute the operation on a remote host'
complete -c timedatectl -s M -l machine --description 'Execute operation on a local container'
complete -c timedatectl -s h -l help --description 'Print a short help text and exit'
complete -c timedatectl -l version --description 'Print a short version string and exit'
complete -c timedatectl -l no-pager --description 'Do not pipe output into a pager'
