complete -c hostnamectl -f

set -l __hostnamectl_version $(hostnamectl --version &| string match -rg 'systemd (\d+).*')
if test "$__hostnamectl_version" -lt 249
    complete -c hostnamectl -n __fish_use_subcommand -xa "status\t'Show hostname and related info'
                                                            set-hostname\t'Set hostname'
                                                            set-icon-name\t'Set icon name'
                                                            set-chassis\t'Set chassis type'
                                                            set-deployment\t'Set deployment environment'
                                                            set-location\t'Set location'"
else
    complete -c hostnamectl -n __fish_use_subcommand -xa "status\t'Show hostname and related info'
                                                            hostname\t'Get or set hostname'
                                                            icon-name\t'Get or set icon name'
                                                            chassis\t'Get or set chassis type'
                                                            deployment\t'Get or set deployment environment'
                                                            location\t'Get or set location'"
end

complete -c hostnamectl -n "__fish_seen_subcommand_from set-chassis chassis" -xa "desktop laptop convertible server tablet handset watch embedded vm container"
complete -c hostnamectl -n "__fish_seen_subcommand_from set-deployment deployment" -xa "development integration staging production"

complete -c hostnamectl -l no-ask-password -d "Don't query for authentication for privileged operations"
complete -c hostnamectl -l static -d "Get or set static hostname"
complete -c hostnamectl -l transient -d "Get or set transient hostname"
complete -c hostnamectl -l pretty -d "Get or set pretty hostname"
complete -c hostnamectl -s H -l host -x -d "Execute operation on remote host"
complete -c hostnamectl -s M -l machine -x -d "Execute operation on local container"
complete -c hostnamectl -s h -l help -d "Print short help"
complete -c hostnamectl -l version -d "Print version"
complete -c hostnamectl -l json -d "Show output as JSON" -xa "short pretty off"
