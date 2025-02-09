set -l commands status install update remove is-installed random-seed systemd-efi-options reboot-to-firmware list set-default set-oneshot set-timeout set-timeout-oneshot

# Execute `bootctl list` and return entries
function __bootctl_entries
    if not type -q jq
        return 1
    end

    bootctl list --json short | jq '.[] | "\(.id)\t\(.showTitle)"' --raw-output
end

complete -c bootctl -f
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a status -d 'Show status of EFI variables'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a install -d 'Install systemd-boot'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a update -d 'Update systemd-boot'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a remove -d 'Remove systemd-boot'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a is-installed -d 'Test whether systemd-boot is installed'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a random-seed -d 'Initialize random seed'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a systemd-efi-options -d 'Query or set system options string'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a reboot-to-firmware -d 'Query or set reboot-to-firmware EFI flag'
complete -c bootctl -n "__fish_seen_subcommand_from reboot-to-firmware" -a 'true false'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a list -d 'List boot loader entries'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a set-default -d 'Set default boot loader entry'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a set-oneshot -d 'Set default boot loader entry (Once)'
complete -c bootctl -n "__fish_seen_subcommand_from set-default set-oneshot" -x -a '(__bootctl_entries)'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a set-timeout -d 'Set default boot loader timeout'
complete -c bootctl -n "not __fish_seen_subcommand_from $commands" -a set-timeout-oneshot -d 'Set default boot loader timeout (Once)'

complete -c bootctl -s h -l help -d 'Show this help'
complete -c bootctl -l version -d 'Print version'
complete -c bootctl -l esp-path -r -d 'Path to the EFI System Partition (ESP)'
complete -c bootctl -l boot-path -r -d 'Path to the $BOOT partition'
complete -c bootctl -s p -l print-esp-path -d 'Print path to the EFI System Partition'
complete -c bootctl -s x -l print-boot-path -d 'Print path to the $BOOT partition'
complete -c bootctl -l no-variables -d 'Do not touch EFI variables'
complete -c bootctl -l no-pager -d 'Do not pipe output into a pager'
complete -c bootctl -l graceful -d 'Do not print fail'
