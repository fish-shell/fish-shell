function __fish_complete_jails
    ezjail-admin list | tail +3 | awk '{ print $4 }'
end

function __fish_complete_running_jails
    ezjail-admin list | tail +3 | grep '^.R' | awk '{ print $4 }'
end

function __fish_complete_stopped_jails
    ezjail-admin list | tail +3 | grep '^.S' | awk '{ print $4 }'
end

# archive
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a archive --description "create a backup of one or several jails"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from archive" -a "(__fish_complete_jails)" --description "jail"

# config
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a config --description "manage specific jails"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from config" -a "(__fish_complete_jails)" --description "jail"

# console
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a console --description "attach your console to a running jail"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from console" -a "(__fish_complete_jails)" --description "jail"

# create
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a create --description "installs a new jail inside ezjail's scope"

# delete
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a delete --description "removes a jail from ezjail's config"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from delete" -a "(__fish_complete_jails)" --description "jail"

# freeze
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a freeze --description "dump diffs between jail initialisation and freeze time into a flavour"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from freeze" -a "(__fish_complete_jails)" --description "jail"

# install
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a install --description "create the basejail from binary packages"

# list
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a list --description "list all jails"

# restart
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a restart --description "restart a running jail"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from restart" -a "(__fish_complete_running_jails)" --description "jail"

# restore
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a restore --description "create new jails from archived versions"

# snapshot
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a snapshot --description "create a snapshot of a jail"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from snapshot" -a "(__fish_complete_jails)" --description "jail"

# start
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a start --description "start a jail"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from start" -a "(__fish_complete_stopped_jails)" --description "jail"

# stop
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a stop --description "stop a running jail"
complete -xc "ezjail-admin" -n "__fish_seen_subcommand_from stop" -a "(__fish_complete_running_jails)" --description "jail"

# troubleshoot
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a troubleshoot --description "check for reasons for the jails to fail"

# update
complete -xc "ezjail-admin" -n "__fish_use_subcommand" -a update --description "create or update the basejail from source"
