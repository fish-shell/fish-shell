function __fish_complete_jails
    ezjail-admin list | tail +3 | awk '{ print $4 }'
end

function __fish_complete_running_jails
    ezjail-admin list | tail +3 | string match -r '^.R' | awk '{ print $4 }'
end

function __fish_complete_stopped_jails
    ezjail-admin list | tail +3 | string match -r '^.S' | awk '{ print $4 }'
end

# archive
complete -xc ezjail-admin -n __fish_use_subcommand -a archive -d "create a backup of one or several jails"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from archive" -a "(__fish_complete_jails)" -d jail

# config
complete -xc ezjail-admin -n __fish_use_subcommand -a config -d "manage specific jails"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from config" -a "(__fish_complete_jails)" -d jail

# console
complete -xc ezjail-admin -n __fish_use_subcommand -a console -d "attach your console to a running jail"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from console" -a "(__fish_complete_jails)" -d jail

# create
complete -xc ezjail-admin -n __fish_use_subcommand -a create -d "installs a new jail inside ezjail's scope"

# delete
complete -xc ezjail-admin -n __fish_use_subcommand -a delete -d "removes a jail from ezjail's config"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from delete" -a "(__fish_complete_jails)" -d jail

# freeze
complete -xc ezjail-admin -n __fish_use_subcommand -a freeze -d "dump diffs between jail initialisation and freeze time into a flavour"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from freeze" -a "(__fish_complete_jails)" -d jail

# install
complete -xc ezjail-admin -n __fish_use_subcommand -a install -d "create the basejail from binary packages"

# list
complete -xc ezjail-admin -n __fish_use_subcommand -a list -d "list all jails"

# restart
complete -xc ezjail-admin -n __fish_use_subcommand -a restart -d "restart a running jail"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from restart" -a "(__fish_complete_running_jails)" -d jail

# restore
complete -xc ezjail-admin -n __fish_use_subcommand -a restore -d "create new jails from archived versions"

# snapshot
complete -xc ezjail-admin -n __fish_use_subcommand -a snapshot -d "create a snapshot of a jail"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from snapshot" -a "(__fish_complete_jails)" -d jail

# start
complete -xc ezjail-admin -n __fish_use_subcommand -a start -d "start a jail"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from start" -a "(__fish_complete_stopped_jails)" -d jail

# stop
complete -xc ezjail-admin -n __fish_use_subcommand -a stop -d "stop a running jail"
complete -xc ezjail-admin -n "__fish_seen_subcommand_from stop" -a "(__fish_complete_running_jails)" -d jail

# troubleshoot
complete -xc ezjail-admin -n __fish_use_subcommand -a troubleshoot -d "check for reasons for the jails to fail"

# update
complete -xc ezjail-admin -n __fish_use_subcommand -a update -d "create or update the basejail from source"
