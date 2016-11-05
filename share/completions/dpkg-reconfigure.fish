# Completions for the `dpkg-reconfigure` command

complete -f -c dpkg-reconfigure -a '(__fish_print_packages)' --description 'Package'

# Support flags
complete -x -f -c dpkg-reconfigure -s h -l help     --description 'Display help'

# General options
complete -f -c dpkg-reconfigure -s f -l frontend -r -a "dialog readline noninteractive gnome kde editor web" --description 'Set configuration frontend'
complete -f -c dpkg-reconfigure -s p -l priority -r -a "low medium high critical" --description 'Set priority threshold'
complete -f -c dpkg-reconfigure -l default-priority --description "Use current default ("(echo get debconf/priority | debconf-communicate | string match -r '\w+$')") priority threshold"
complete -f -c dpkg-reconfigure -s u -l unseen-only --description 'Show only unseen question'
complete -f -c dpkg-reconfigure -l force --description 'Reconfigure also inconsistent packages'
complete -f -c dpkg-reconfigure -l no-reload --description 'Prevent reloading templates'
