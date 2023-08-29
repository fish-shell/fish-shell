set -l runtime userparameter_reload \
    log_level_increase \
    log_level_decrease \
    help \
    metrics \
    version

complete -c zabbix_agent2 -s c -l config -d "Specify an alternate config-file."
complete -c zabbix_agent2 -r -f -s R -l runtime-control -a "$runtime" -d "Perform administrative functions."
complete -c zabbix_agent2 -f -s p -l print -d "Print known items and exit."
complete -c zabbix_agent2 -f -s t -l test -d "Test single item and exit."
complete -c zabbix_agent2 -f -s h -l help -d "Display this help and exit."
complete -c zabbix_agent2 -f -s V -l version -d "Output version information and exit."
