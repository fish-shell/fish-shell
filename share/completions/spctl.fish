function __fish_spctl_has_assess
    contains -- --assess (commandline -xpc)
    or contains -- -a (commandline -xpc)
end

complete -c spctl -l assess -s a -d 'Assess one or more files against system policy'
complete -c spctl -l status -d 'Query whether the assessment subsystem is enabled or disabled'
complete -c spctl -l global-enable -d 'Enable the assessment subsystem (requires root)'
complete -c spctl -l global-disable -d 'Reveal the "allow apps from anywhere" option in Privacy & Security'
complete -c spctl -l disable-status -d 'Query whether the "allow apps from anywhere" option is available'

complete -c spctl -d 'Type of assessment to perform' -n __fish_spctl_has_assess -l type -s t -x -a '
execute\t"Assess code execution (default)"
install\t"Assess installer packages"
open\t"Assess document opening"'
complete -c spctl -d 'Request more verbose output (repeat to increase verbosity)' -n __fish_spctl_has_assess -l verbose -s v
complete -c spctl -d 'Continue assessing remaining files after a failed assessment' -n __fish_spctl_has_assess -l continue
complete -c spctl -d 'Do not query the assessment object cache (may be slower)' -n __fish_spctl_has_assess -l ignore-cache
complete -c spctl -d 'Do not store assessment outcome in the cache' -n __fish_spctl_has_assess -l no-cache
complete -c spctl -d 'Display assessment outcome as raw XML plist' -n __fish_spctl_has_assess -l raw
