# Completions for xbps-uchroot
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-uchroot

complete -c $progname -f -a "(__fish_complete_directories)"

complete -c $progname -s b -d 'Bind mounts src into CHROOTDIR/dest' -F
complete -c $progname -s O -d 'Setups a temporary directory and then creates an overlay layer (via overlayfs)'
complete -c $progname -s o -d 'Arguments passed to the tmpfs mount, if the O and t options are specified'
complete -c $progname -s t -d 'Mount temp directory in tmpfs (everything stored in RAM). Use only with -O'
