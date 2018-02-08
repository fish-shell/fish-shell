complete -c wvdial -xa "(__fish_complete_wvdial_peers)" -d "wvdial connections"
complete -c wvdial -s c -l chat -d 'Run wvdial as chat replacement from within pppd'
complete -c wvdial -s C -l config -r -d 'Run wvdial with alternative config file'
complete -c wvdial -s n -l no-syslog -d 'Don\'t output debug information'
