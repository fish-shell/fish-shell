set -l runtime config_cache_reload \
    housekeeper_execute \
    trigger_housekeeper_execute \
    log_level_increase \
    "log_level_increase=" \
    log_level_decrease \
    "log_level_decrease=" \
    snmp_cache_reload \
    secrets_reload \
    diaginfo \
    "diaginfo=" \
    prof_enable \
    prof_enable= \
    prof_disable \
    prof_disable= \
    service_cache_reload \
    ha_status \
    "ha_remove_node=" \
    ha_set_failover_delay

set -l scope rwlock mutex processing

function __fish_string_in_command -a ch
    string match -rq -- $ch (commandline)
end

function __fish_prepend -a prefix
    set -l log_target alerter \
        "alert manager" \
        "configuration syncer" \
        discoverer \
        escalator \
        "history syncer" \
        housekeeper \
        "http poller" \
        "icmp pinger" \
        "ipmi manager" \
        "ipmi poller" \
        "java poller" \
        poller \
        "preprocessing manager" \
        "preprocessing worker" \
        "proxy poller" \
        self-monitoring \
        "snmp trapper" \
        "task manager" \
        timer \
        trapper \
        "unreachable poller" \
        "vmware collector" \
        "history poller" \
        "availability manager" \
        "service manager" \
        "odbc poller"

    if string match -rq 'log_level_(in|de)crease' $prefix
        set var $log_target
    else if string match -rq 'prof_(en|dis)able' $prefix
        set var $log_target 'ha manager'
    else if string match -rq diaginfo $prefix
        set var historycache preprocessing alerting lld valuecache locks
    end

    for i in $var
        echo $prefix="$i"
    end
end

function __fish_list_nodes
    zabbix_server -R ha_status | tail -n+4 | awk '{print "ha_remove_node="$3}'
end

# General
complete -c zabbix_server -s c -l config -d "Path to the configuration file."
complete -c zabbix_server -f -s f -l foreground -d "Run Zabbix server in foreground."
complete -c zabbix_server -f -s h -l help -d "Display this help message."
complete -c zabbix_server -f -s V -l version -d "Display version number."
complete -c zabbix_server -f -s R -l runtime-control -a "$runtime" -d "Perform administrative functions."

# Log levels
complete -c zabbix_server -r -f -s R -l runtime-control -n "__fish_string_in_command log_level_increase" -a "(__fish_prepend log_level_increase)"
complete -c zabbix_server -r -f -s R -l runtime-control -n "__fish_string_in_command log_level_decrease" -a "(__fish_prepend log_level_decrease)"

# Prof enable
complete -c zabbix_server -r -f -s R -l runtime-control -n "__fish_string_in_command prof_enable" -a "(__fish_prepend prof_enable)"
complete -c zabbix_server -r -f -s R -l runtime-control -n "__fish_string_in_command prof_disable" -a "(__fish_prepend prof_disable)"

# HA nodes
complete -c zabbix_server -r -f -s R -l runtime-control -n "__fish_string_in_command ha_remove_node" -a "(__fish_list_nodes)"

# diaginfo
complete -c zabbix_server -r -f -s R -l runtime-control -n "__fish_string_in_command diaginfo" -a "(__fish_prepend diaginfo)"
