function __fish_firejail_profiles
    path basename -- /etc/firejail/*.profile ~/.config/firejail/*.profile
end

function __fish_firejail_complete_sandboxes
    firejail --list | string replace -rf -- '([^:]+):([^:]+):([^:]+):(.*)' '$1\t$3: $4 ($2)\n$3\t$1: $4 ($2)'
end

complete -c firejail -f
complete -c firejail -l help -d 'Show help and exit'
complete -c firejail -l version -d 'Show version and exit'
complete -c firejail -l debug -d 'Print debug info'
complete -c firejail -l debug-blacklists -d 'Debug blacklisting'
complete -c firejail -l debug-whitelists -d 'Debug whitelisting'
complete -c firejail -l debug-caps -d 'Print known capabilities and exit'
complete -c firejail -l debug-errnos -d 'Print known error numbers and exit'
complete -c firejail -l debug-private-lib -d 'Debug --private-lib'
complete -c firejail -l debug-protocols -d 'Print known protocols and exit'
complete -c firejail -l debug-syscalls -d 'Print known syscalls and exit'
complete -c firejail -l debug-syscalls32 -d 'Print known 32-bit syscalls and exit'
complete -c firejail -l list -d 'List all sandboxes'
complete -c firejail -l tree -d 'Print a tree of all sandboxes'
complete -c firejail -l top -d 'Monitor sandboxes (like `top`)'
complete -c firejail -l shutdown -d 'Show help and exit'

# Profiles
# Note: firejail's option parsing is weird and *requires* the --foo=bar form
# So we don't use -r
complete -c firejail -l profiles -d 'Load a custom security profile' -a '(__fish_firejail_profiles)'

# Sandboxes
complete -c firejail -l join -d 'Join existing sandbox' -a '(__fish_firejail_complete_sandboxes)'

# Files
complete -c firejail -l hosts-file -d 'Show help and exit' -a '(__fish_firejail_profiles)'

# directory
complete -c firejail -l chroot -d 'Chroot into this directory' -a '(__fish_complete_directories)'
complete -c firejail -l tmpfs -d 'Mount tmpfs into sandbox' -a '(__fish_complete_directories)'
complete -c firejail -l blacklist -d 'Blacklist dir or file' -F
complete -c firejail -l noblacklist -d 'Disable blacklist for dir or file' -F
complete -c firejail -l whitelist -d 'Whitelist dir or file' -F
complete -c firejail -l nowhitelist -d 'Disable whitelist for dir or file' -F
complete -c firejail -l read-only -d 'Set dir or file read-only' -F
complete -c firejail -l read-write -d 'Set dir or file read-write' -F
# TODO: Comma-separated ("/etc/foo,/usr/etc/foo" bind-mounts /etc/foo on /usr/etc/foo)
complete -c firejail -l bind -d 'Bind-mount file on top of another' -F
complete -c firejail -l private -d 'Mount temporary home directories and delete on exit' -F
complete -c firejail -l netfilter -d 'Enable firewall given by this file' -F

# Interfaces
complete -c firejail -l net -d 'Enable network namespace connected to this interface' -a '(__fish_print_interfaces)'

# Commands
complete -c firejail -a '(__fish_complete_subcommand)'
