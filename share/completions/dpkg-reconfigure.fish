# Completions for the `dpkg-reconfigure` command

complete -f -c dpkg-reconfigure -a '(__fish_print_apt_packages)' -d Package

# Support flags
complete -x -f -c dpkg-reconfigure -s h -l help -d 'Display help'

# General options
complete -f -c dpkg-reconfigure -s f -l frontend -r -a "dialog readline noninteractive gnome kde editor web" -d 'Set configuration frontend'
complete -f -c dpkg-reconfigure -s p -l priority -r -a "low medium high critical" -d 'Set priority threshold'
complete -f -c dpkg-reconfigure -l default-priority -d "Use current default priority threshold"
complete -f -c dpkg-reconfigure -s u -l unseen-only -d 'Show only unseen question'
complete -f -c dpkg-reconfigure -l force -d 'Reconfigure also inconsistent packages'
complete -f -c dpkg-reconfigure -l no-reload -d 'Prevent reloading templates'
