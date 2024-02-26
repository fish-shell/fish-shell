# Heroku toolbelt completions
# By Jason Brokaw (github.com/jbbrokaw)

function __fish_list_available_addons
    heroku addons:list | awk -F":" '/^[a-z]/ {print $1}'
end

function __fish_list_installed_addons
    heroku addons | awk '{if (NR>1) print $1}'
end

function __fish_list_heroku_apps
    heroku apps | awk '{if (NR>1) print $1}'
end

function __fish_list_heroku_config_keys
    heroku config | awk -F':' '{if (NR>1) print $1}'
end

function __fish_list_heroku_domains
    heroku domains | awk '{if (NR>1) print $1}'
end

function __fish_list_heroku_dynos
    heroku ps | awk -F':' '{if (NR>1) print $1}'
end

function __fish_list_heroku_releases
    heroku releases | awk '{if (NR>1) print $1}'
end

function __fish_heroku_needs_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -eq 1
        return 0
    end
    return 1
end

function __fish_heroku_using_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

set -l heroku_looking -c heroku -n __fish_heroku_needs_command

# Main commands
complete $heroku_looking -xa addons -d 'list installed addons'
complete $heroku_looking -xa apps -d 'manage apps (create, destroy)'
complete $heroku_looking -xa auth -d 'authentication (login, logout)'
complete $heroku_looking -xa config -d 'manage app config vars'
complete $heroku_looking -xa domains -d 'manage custom domains'
complete $heroku_looking -xa logs -d 'display logs for an app'
complete $heroku_looking -xa ps -d 'manage dynos (dynos, workers)'
complete $heroku_looking -xa releases -d 'manage app releases'
complete $heroku_looking -xa run -d 'run one-off commands (console, rake)'
complete $heroku_looking -xa sharing -d 'manage collaborators on an app'

# Additional topics:

complete $heroku_looking -xa certs -d 'manage ssl endpoints for an app'
complete $heroku_looking -xa drains -d 'display syslog drains for an app'
complete $heroku_looking -xa features -d 'manage optional features'
complete $heroku_looking -xa fork -d 'clone an existing app'
complete $heroku_looking -xa help -d 'list commands and display help'
complete $heroku_looking -xa keys -d 'manage authentication keys'
complete $heroku_looking -xa labs -d 'manage optional features'
complete $heroku_looking -xa maintenance -d 'manage maintenance mode for an app'
complete $heroku_looking -xa members -d 'manage membership in organization accounts'
complete $heroku_looking -xa orgs -d 'manage organization accounts'
complete $heroku_looking -xa pg -d 'manage heroku-postgresql databases'
complete $heroku_looking -xa plugins -d 'manage plugins to the heroku gem'
complete $heroku_looking -xa regions -d 'list available regions'
complete $heroku_looking -xa stack -d 'manage the stack for an app'
complete $heroku_looking -xa status -d 'check status of heroku platform'
complete $heroku_looking -xa update -d 'update the heroku client'
complete $heroku_looking -xa version -d 'display version'

complete $heroku_looking -xa git:clone -d "clones heroku application to machine at DIRECTORY. defaults to app name."
complete $heroku_looking -xa git:remote -d "adds a git remote to an app repo (-a APP)"

# Addons subcommands
complete $heroku_looking -xa addons:add -d 'install an addon'
complete -c heroku -n '__fish_heroku_using_command addons:add' -fa '(__fish_list_available_addons)'

complete $heroku_looking -xa addons:docs -d "open an addon's documentation in your browser"
complete -c heroku -n '__fish_heroku_using_command addons:docs' -fa '(__fish_list_installed_addons)'

complete $heroku_looking -xa addons:downgrade -d 'downgrade an existing addon'
complete -c heroku -n '__fish_heroku_using_command addons:downgrade' -fa '(__fish_list_installed_addons)'

complete $heroku_looking -xa addons:list -d 'list all available addons'

complete $heroku_looking -xa addons:open -d "open an addon's dashboard in your browser"
complete -c heroku -n '__fish_heroku_using_command addons:open' -fa '(__fish_list_installed_addons)'

complete $heroku_looking -xa addons:remove -d 'uninstall one or more addons'
complete -c heroku -n '__fish_heroku_using_command addons:remove' -fa '(__fish_list_installed_addons)'

complete $heroku_looking -xa addons:upgrade -d 'upgrade an existing addon'
complete -c heroku -n '__fish_heroku_using_command addons:upgrade' -fa '(__fish_list_installed_addons)'

# Apps options and subcommands
complete -c heroku -n '__fish_heroku_using_command apps' -s o -l org -l ORG -d "the org to list the apps for"
complete -c heroku -n '__fish_heroku_using_command apps' -s A -l all -d "list all apps in the org. Not just joined apps"
complete -c heroku -n '__fish_heroku_using_command apps' -s p -l personal -d "list apps in personal account when a default org is set"

complete $heroku_looking -xa apps:create -d "create a new app (takes name)"

complete $heroku_looking -xa apps:destroy -d " permanently destroy an app (--app APP)"
complete -c heroku -n '__fish_heroku_using_command apps:destroy' -l app -s a -xa '(__fish_list_heroku_apps)'

complete $heroku_looking -xa apps:info -d "show detailed app information"

complete $heroku_looking -xa apps:join -d " add yourself to an organization app (--app APP)"
complete -c heroku -n '__fish_heroku_using_command apps:join' -l app -s a

complete $heroku_looking -xa apps:leave -d " remove yourself from an organization app (--app APP)"
complete -c heroku -n '__fish_heroku_using_command apps:leave' -l app -s a -xa '(__fish_list_heroku_apps)'

complete $heroku_looking -xa apps:lock -d " lock an organization app to restrict access"
complete -c heroku -n '__fish_heroku_using_command apps:lock' -l app -s a -xa '(__fish_list_heroku_apps)'

complete $heroku_looking -xa apps:open -d " open the app in a web browser"
complete -c heroku -n '__fish_heroku_using_command apps:open' -l app -s a -xa '(__fish_list_heroku_apps)'

complete $heroku_looking -xa apps:rename -d " rename the app (apps:rename newname --app oldname)"
complete -c heroku -n '__fish_heroku_using_command apps:rename' -l app -s a -xa '(__fish_list_heroku_apps)'

complete $heroku_looking -xa apps:unlock -d " unlock an organization app so that any org member can join it"
complete -c heroku -n '__fish_heroku_using_command apps:unlock' -l app -s a -xa '(__fish_list_heroku_apps)'

# Auth subcommands
complete $heroku_looking -xa auth:login -d "log in with your heroku credentials"
complete $heroku_looking -xa auth:logout -d "clear local authentication credentials"
complete $heroku_looking -xa auth:token -d "display your api token"
complete $heroku_looking -xa auth:whoami -d "display your heroku email address"

# Config options and subcommands
complete -c heroku -n '__fish_heroku_using_command config' -s s -l shell -d "output config vars in shell format"

complete $heroku_looking -xa config:get -d "display a config value for an app"
complete -c heroku -n '__fish_heroku_using_command config:get' -xa '(__fish_list_heroku_config_keys)' -d "display a config value for an app"

complete $heroku_looking -xa config:set -d "set one or more config vars (KEY=VALUE)"
complete -c heroku -n '__fish_heroku_using_command config:set' -fa '(__fish_list_heroku_config_keys)' -d "set one or more config vars (KEY=VALUE)"

complete $heroku_looking -xa config:unset -d "unset one or more config vars"
complete -c heroku -n '__fish_heroku_using_command config:unset' -fa '(__fish_list_heroku_config_keys)' -d "unset one or more config vars"

# Domains subcommands
complete $heroku_looking -xa domains:add -d "add a custom domain to an app"
complete $heroku_looking -xa domains:clear -d "remove all custom domains from an app"
complete $heroku_looking -xa domains:remove -d "remove a custom domain from an app"
complete -c heroku -n '__fish_heroku_using_command domains:remove' -fa '(__fish_list_heroku_domains)' -d "remove a custom domain from an app"

# Logs options
complete -c heroku -n '__fish_heroku_using_command logs' -s n -l num -l NUM -d "the number of lines to display"
complete -c heroku -n '__fish_heroku_using_command logs' -s p -l ps -l PS -d "only display logs from the given process"
complete -c heroku -n '__fish_heroku_using_command logs' -s s -l source -l SOURCE -d "only display logs from the given source"
complete -c heroku -n '__fish_heroku_using_command logs' -s t -l tail -d "continually stream logs"

# PG subcommands
complete $heroku_looking -xa pg:backups -d "manage backups of heroku postgresql databases"
complete $heroku_looking -xa pg:backups:cancel -d "cancel an in-progress backup or restore (default newest)"
complete $heroku_looking -xa pg:backups:capture -d "capture a new backup"
complete $heroku_looking -xa pg:backups:delete -d "delete a backup"
complete $heroku_looking -xa pg:backups:download -d "downloads database backup"
complete $heroku_looking -xa pg:backups:info -d "get information about a specific backup"
complete $heroku_looking -xa pg:backups:restore -d "restore a backup (default latest) to a database"
complete $heroku_looking -xa pg:backups:schedule -d "schedule daily backups for given database"
complete $heroku_looking -xa pg:backups:schedules -d "list backup schedule"
complete $heroku_looking -xa pg:backups:unschedule -d "stop daily backups"
complete $heroku_looking -xa pg:backups:url -d "get secret but publicly accessible URL of a backup"

# PS subcommands
complete $heroku_looking -xa ps:resize -d "resize dynos to the given size (DYNO1=1X|2X|PX)"
complete -c heroku -n '__fish_heroku_using_command ps:resize' -fa '(__fish_list_heroku_dynos)' -d "resize dynos to the given size (DYNO1=1X|2X|PX)"

complete $heroku_looking -xa ps:restart -d "restart an app dyno"
complete -c heroku -n '__fish_heroku_using_command ps:restart' -fa '(__fish_list_heroku_dynos)' -d "restart an app dyno"

complete $heroku_looking -xa ps:scale -d "scale dynos by the given amount (DYNO1=AMOUNT1)"
complete -c heroku -n '__fish_heroku_using_command ps:scale' -fa '(__fish_list_heroku_dynos)' -d "scale dynos by the given amount (DYNO1=AMOUNT1)"

complete $heroku_looking -xa ps:stop -d "stop an app dyno"
complete -c heroku -n '__fish_heroku_using_command ps:stop' -fa '(__fish_list_heroku_dynos)' -d "stop an app dyno"

# Releases options and subcommands
complete -c heroku -n '__fish_heroku_using_command releases' -s n -l num -l NUM -d "number of releases to show, maximum 50"

complete $heroku_looking -xa releases:info -d "view detailed information for a release"
complete -c heroku -n '__fish_heroku_using_command releases:info' -fa '(__fish_list_heroku_releases)' -d "view detailed information for a release"

complete $heroku_looking -xa releases:rollback -d "roll back to an older release"
complete -c heroku -n '__fish_heroku_using_command releases:rollback' -fa '(__fish_list_heroku_releases)' -d "roll back to an older release"

# Run options and subcommands
complete -c heroku -n '__fish_heroku_using_command run' -s s -l size -l SIZE -d "specify dyno size"

complete $heroku_looking -xa run:console -d "open a remote console session (with optional COMMAND)"
complete $heroku_looking -xa run:detached -d "run a detached dyno, where output is sent to your logs"
# complete $heroku_looking -xa run:rake -d "WARNING: `heroku run:rake` has been deprecated. Please use `heroku run rake` instead."
complete -c heroku -n '__fish_heroku_using_command run:detached' -s t -l tail -d "stream logs for the dyno"

# Sharing subcommands
complete $heroku_looking -xa sharing:add -d "add a collaborator to an app"
complete $heroku_looking -xa sharing:remove -d "remove a collaborator from an app"
complete $heroku_looking -xa sharing:transfer -d "transfers an app to another user or an organization."
