function __fail2ban_jails
    # No need to deduplicate because fish will take care of that for us
    path basename {,/usr/local}/etc/fail2ban/filter.d/*.{conf,local} | path change-extension ""
end

function __fail2ban_actions
    # No need to deduplicate because fish will take care of that for us
    path basename {,/usr/local}/etc/fail2ban/action.d/*.{conf,local} | path change-extension ""
end

# basic options
complete -c fail2ban-client -s c -l conf -d "Configuration dir"
complete -c fail2ban-client -s s -l socket -d "Socket path"
complete -c fail2ban-client -s p -l pidfile -d "Pidfile path"
complete -c fail2ban-client -l pname -d "Name of the process"
complete -c fail2ban-client -l loglevel -d "loglevel of client" -xa "CRITICAL ERROR WARNING NOTICE INFO DEBUG TRACEDEBUG HEAVYDEBUG"
complete -c fail2ban-client -l logtarget -d "Logging target" -a "stdout stderr syslog sysout" # or path
complete -c fail2ban-client -l logtarget -d Syslogsocket -a auto # or path
complete -c fail2ban-client -s d -d "Dump configuration"
complete -c fail2ban-client -l dp -l dump-pretty -d "Dump configuration (pretty)"
complete -c fail2ban-client -s t -l test -d "Test configuration"
complete -c fail2ban-client -s i -d "Run in interactive mode"
complete -c fail2ban-client -s v -d "Increase verbosity"
complete -c fail2ban-client -s q -d "Decrease verbosity"
complete -c fail2ban-client -s x -d "Force execution of server"
complete -c fail2ban-client -s b -d "Start server in background (default)"
complete -c fail2ban-client -s f -d "Start server in foreground"
complete -c fail2ban-client -l str2sec -d "Convert time abbr format to secs"
complete -c fail2ban-client -s h -l help -d "Display help message"
complete -c fail2ban-client -s V -l version -d "Display client version"

# subcommands
complete -c fail2ban-client -n __fish_is_first_token -xa start -d "Start fail2ban server or jail"
complete -c fail2ban-client -n __fish_is_first_token -xa restart -d "Restart server or jail"
complete -c fail2ban-client -n __fish_is_first_token -xa reload -d "Reload server configuration"
complete -c fail2ban-client -n __fish_is_first_token -xa stop -d "Stop fail2ban server or jail"
complete -c fail2ban-client -n __fish_is_first_token -xa unban -d "Unban ip address(es)"
complete -c fail2ban-client -n __fish_is_first_token -xa banned -d "List jails w/ their banned IPs"
complete -c fail2ban-client -n __fish_is_first_token -xa status -d "Get server or jail status"
complete -c fail2ban-client -n __fish_is_first_token -xa ping -d "Check if server is alive"
complete -c fail2ban-client -n __fish_is_first_token -xa help -d "Prints usage synopsis"
complete -c fail2ban-client -n __fish_is_first_token -xa version -d "Prints server version"
complete -c fail2ban-client -n __fish_is_first_token -xa set
complete -c fail2ban-client -n __fish_is_first_token -xa get
complete -c fail2ban-client -n __fish_is_first_token -xa flushlogs -d "Flushes log files and reopens"
complete -c fail2ban-client -n __fish_is_first_token -xa add

complete -c fail2ban-client -n "__fish_seen_subcommand_from start" -xa "(__fail2ban_jails)"
complete -c fail2ban-client -n "__fish_seen_subcommand_from stop" -xa "(__fail2ban_jails)"
complete -c fail2ban-client -n "__fish_seen_subcommand_from status" -xa "(__fail2ban_jails)"
complete -c fail2ban-client -n "__fish_seen_subcommand_from restart" -xa "(__fail2ban_jails)"
complete -c fail2ban-client -n "__fish_seen_subcommand_from reload" -xa "(__fail2ban_jails)"
complete -c fail2ban-client -n "__fish_seen_subcommand_from add" -xa "(__fail2ban_jails)"

# restart options
complete -c fail2ban-client -n "__fish_seen_subcommand_from restart" -l unban
complete -c fail2ban-client -n "__fish_seen_subcommand_from reload" -l if-exists

# reload options
complete -c fail2ban-client -n "__fish_seen_subcommand_from restart" -l restart
complete -c fail2ban-client -n "__fish_seen_subcommand_from reload" -l unban
complete -c fail2ban-client -n "__fish_seen_subcommand_from reload" -l all
complete -c fail2ban-client -n "__fish_seen_subcommand_from reload" -l if-exists

# unban options
complete -c fail2ban-client -n "__fish_seen_subcommand_from unban" -l all

# get/set loglevel
complete -c fail2ban-client -n "__fish_seen_subcommand_from get" -n "__fish_prev_arg_in get" -xa loglevel -d "Get server log level"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in set" -xa loglevel -d "Change server log level"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in loglevel" -xa "CRITICAL ERROR WARNING NOTICE INFO DEBUG TRACEDEBUG HEAVYDEBUG"

# get/set logtarget
complete -c fail2ban-client -n "__fish_seen_subcommand_from get" -n "__fish_prev_arg_in get" -xa logtarget -d "Get server log output"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in set" -xa logtarget -d "Change server log output"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in logtarget" -a "STDOUT STDERR SYSLOG SYSTEMD-JOURNAL" # or path

# get/set syslogsocket
complete -c fail2ban-client -n "__fish_seen_subcommand_from get" -n "__fish_prev_arg_in get" -xa syslogsocket -d "Get server syslog socket"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in set" -xa syslogsocket -d "Change server syslog socket"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in syslogsocket" -a auto # or path

# get/set dbfile
complete -c fail2ban-client -n "__fish_seen_subcommand_from get" -n "__fish_prev_arg_in get" -xa dbfile -d "Get server db path"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in set" -xa dbfile -d "Change server db path"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in dbfile" -a None # or path

# get/set dbmaxmatches
complete -c fail2ban-client -n "__fish_seen_subcommand_from get" -n "__fish_prev_arg_in get" -xa dbmaxmatches -d "Get max matches stored per ticket"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in set" -xa dbmaxmatches -d "Set max matches stored per ticket"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in dbmaxmatches" -xa "(seq 0 100)"

# get/set dbpurgeage
complete -c fail2ban-client -n "__fish_seen_subcommand_from get" -n "__fish_prev_arg_in get" -xa dbpurgeage -d "Get secs ban history will be kept"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in set" -xa dbpurgeage -d "Set secs ban history will be kept"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in dbpurgeage" -xa "(seq 0 30 3600)"

# get/set <jail> options
complete -c fail2ban-client -n "__fish_seen_subcommand_from set get" -n "__fish_prev_arg_in set get" -xa "(__fail2ban_jails)"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_is_nth_token 3" -xa "idle ignoreself addignoreip delignoreip ignorecommand ignorecache addlogpath dellogpath logencoding addjournalmatch deljournalmatch addfailregex delfailregex bantime datepattern usedns attempt banip unbanip maxretry maxmatches maxlines addaction delaction action"
complete -c fail2ban-client -n "__fish_seen_subcommand_from get" -n "__fish_is_nth_token 3" -xa "banned logpath logencoding journalmatch ignoreself ignoreip ignorecommand failregex ignoreregex findtime bantime datepattern usedns banip maxretry maxmatches maxlines actions action actionproperties actionmethods action"

# complete actions for `get/set jail action/actionproperties/actionmethods` and `set jail addaction/delaction`
complete -c fail2ban-client -n "__fish_seen_subcommand_from get set" -n "__fish_prev_arg_in action{,properties,methods}" -n "__fish_is_nth_token 4" -xa "(__fail2ban_actions)"
complete -c fail2ban-client -n "__fish_seen_subcommand_from set" -n "__fish_prev_arg_in {add,del}action" -n "__fish_is_nth_token 4" -xa "(__fail2ban_actions)"

# complete action commands for `get/set jail action <action>`
complete -c fail2ban-client -n "__fish_seen_subcommand_from get set" -n "__fish_seen_subcommand_from action" -n "__fish_is_nth_token 5" -xa "actionstart actionstop actioncheck actionban actionunban timeout"

# specific enumerable jail properties
complete -c fail2ban-client -n "__fish_seen_subcommand_from get set" -n "__fish_prev_arg_in idle" -xa "on off"
complete -c fail2ban-client -n "__fish_seen_subcommand_from get set" -n "__fish_prev_arg_in ignoreself" -xa "true false"
