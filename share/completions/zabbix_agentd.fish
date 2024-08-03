set -l runtime userparameter_reload \
    log_level_increase \
    log_level_increase= \
    log_level_decrease \
    log_level_decrease=

function __fish_string_in_command -a ch
    string match -rq -- $ch (commandline)
end

function __fish_prepend -a prefix
    if string match -rq 'log_level_(in|de)crease' $prefix
        set var "active checks" collector listener
    end

    for i in $var
        echo $prefix="$i"
    end
end

# General
complete -c zabbix_agentd -s c -l config -d "Specify an alternate config-file."
complete -c zabbix_agentd -f -s f -l foreground -d "Run Zabbix agent in foreground."
complete -c zabbix_agentd -r -f -s R -l runtime-control -a "$runtime" -d "Perform administrative functions."
complete -c zabbix_agentd -f -s p -l print -d "Print known items and exit."
complete -c zabbix_agentd -f -s t -l test -d "Test single item and exit."
complete -c zabbix_agentd -f -s h -l help -d "Display this help and exit."
complete -c zabbix_agentd -f -s V -l version -d "Output version information and exit."

# Log levels
complete -c zabbix_agentd -r -f -s R -l runtime-control -n "__fish_string_in_command log_level_increase" -a "(__fish_prepend log_level_increase)"
complete -c zabbix_agentd -r -f -s R -l runtime-control -n "__fish_string_in_command log_level_decrease" -a "(__fish_prepend log_level_decrease)"
