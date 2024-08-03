set -l runtime config_cache_reload \
    snmp_cache_reload \
    housekeeper_execute \
    diaginfo \
    diaginfo= \
    log_level_increase \
    log_level_increase= \
    log_level_decrease \
    log_level_decrease=

function __fish_string_in_command -a ch
    string match -rq -- $ch (commandline)
end

function __fish_prepend -a prefix
    set -l log_target "configuration syncer" \
        "data sender" \
        discoverer \
        "history syncer" \
        housekeeper \
        "http poller" \
        "icmp pinger" \
        "ipmi manager" \
        "ipmi poller" \
        "java poller" \
        poller \
        self-monitoring \
        "snmp trapper" \
        "task manager" \
        trapper \
        "unreachable poller" \
        "vmware collector"

    if string match -rq 'log_level_(in|de)crease' $prefix
        set var $log_target
    else if string match -rq diaginfo $prefix
        set var historycache preprocessing
    end

    for i in $var
        echo $prefix="$i"
    end
end

# General
complete -c zabbix_proxy -s c -l config -d "Use an alternate config-file."
complete -c zabbix_proxy -f -s f -l foreground -d "Run Zabbix agent in foreground."
complete -c zabbix_proxy -f -s R -l runtime-control -a "$runtime" -d "Perform administrative functions."
complete -c zabbix_proxy -f -s h -l help -d "Display this help and exit."
complete -c zabbix_proxy -f -s V -l version -d "Output version information and exit."

# Logs
complete -c zabbix_proxy -r -f -s R -l runtime-control -n "__fish_string_in_command log_level_increase" -a "(__fish_prepend log_level_increase)"
complete -c zabbix_proxy -r -f -s R -l runtime-control -n "__fish_string_in_command log_level_decrease" -a "(__fish_prepend log_level_decrease)"

# Diag info
complete -c zabbix_proxy -r -f -s R -l runtime-control -n "__fish_string_in_command diaginfo" -a "(__fish_prepend diaginfo)"
