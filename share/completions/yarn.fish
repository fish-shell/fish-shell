# Yarn 1.22.4

source $__fish_data_dir/functions/__fish_npm_helper.fish
set -l yarn_add "yarn global add"

# Typically there is no need to check if (commandline -ct) begins with `--`
# because it won't be matched. But we can prevent the slowdown from getting
# a list of all packages and filtering through it if we only do that when
# completing what seems to be a package name.
complete -f -c yarn -n '__fish_seen_subcommand_from remove; and not __fish_is_switch' -xa '(__npm_installed_local_packages)'
complete -f -c yarn -n '__fish_seen_subcommand_from add; and not __fish_is_switch' -xa "(__npm_filtered_list_packages \"$yarn_add\")"

complete -f -c yarn -n __fish_use_subcommand -a help -d 'Show available commands and flags'

complete -f -c yarn -n __fish_use_subcommand -a add -d 'Add packages'
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l dev -s D
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l peer -s P
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l O -s O
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l exact -s E
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l tilde -s T

complete -f -c yarn -n __fish_use_subcommand -a bin -d 'Show location of Yarn `bin` folder'

complete -f -c yarn -n __fish_use_subcommand -a cache -d 'Manage Yarn cache'
complete -f -c yarn -n '__fish_seen_subcommand_from cache' -a clean

complete -f -c yarn -n __fish_use_subcommand -a config -d 'Manage Yarn configuration'
complete -f -c yarn -n '__fish_seen_subcommand_from config' -a 'set get delete list'

complete -f -c yarn -n __fish_use_subcommand -a exec -d 'Run binaries'

complete -f -c yarn -n __fish_use_subcommand -a info -d 'Show information about a package'

complete -f -c yarn -n __fish_use_subcommand -a init -d 'Interactively create or update `package.json`'
complete -f -c yarn -n '__fish_seen_subcommand_from init' -s y -l yes

complete -f -c yarn -n __fish_use_subcommand -a install -d 'Install packages'

complete -f -c yarn -n __fish_use_subcommand -a link -d 'Symlink a package'

complete -f -c yarn -n __fish_use_subcommand -a list -d 'List installed packages'
complete -f -c yarn -n '__fish_seen_subcommand_from list' -l depth

complete -f -c yarn -n __fish_use_subcommand -a node -d 'Run Node with the hook already setup'

complete -f -c yarn -n __fish_use_subcommand -a pack -d 'Create compressed archive of packages'

complete -f -c yarn -n __fish_use_subcommand -a remove -d 'Remove packages'
complete -f -c yarn -n __fish_use_subcommand -a run -d 'Run a defined package script'

function __fish_yarn_run
    if test -e package.json; and set -l python (__fish_anypython)
        # Warning: That weird indentation is necessary, because python.
        $python -S -c 'import json, sys; data = json.load(sys.stdin);
for k,v in data["scripts"].items(): print(k + "\t" + v[:18])' <package.json 2>/dev/null
    else if test -e package.json; and type -q jq
        jq -r '.scripts | to_entries | map("\(.key)\t\(.value | tostring | .[0:20])") | .[]' package.json
    else if type -q jq
        # Yarn is quite slow and still requires `jq` because the normal format is unusable.
        command yarn run --json 2>/dev/null | jq -r '.data.hints? | to_entries | map("\(.key)\t\(.value | tostring |.[0:20])") | .[]'
    end
end

# Scripts can be used like normal subcommands, or with `yarn run SCRIPT`.
complete -c yarn -n '__fish_use_subcommand; or __fish_seen_subcommand_from run' -xa "(__fish_yarn_run)"

complete -f -c yarn -n __fish_use_subcommand -a unlink -d 'Unlink a previously created symlink'
complete -f -c yarn -n __fish_use_subcommand -a unplug -d 'Force unpack packages'
complete -f -c yarn -n __fish_use_subcommand -a up -d 'Upgrade packages'
complete -f -c yarn -n __fish_use_subcommand -a upgrade-interactive -d 'Upgrade packages interactively'

complete -f -c yarn -n __fish_use_subcommand -a version -d 'Update the package version'
complete -f -c yarn -n '__fish_seen_subcommand_from version' -l new-version
complete -f -c yarn -n '__fish_seen_subcommand_from version' -l message
complete -f -c yarn -n '__fish_seen_subcommand_from version' -l no-git-tag-version

complete -f -c yarn -n __fish_use_subcommand -a why -d 'Show why a package is installed'
complete -f -c yarn -n __fish_use_subcommand -a workspace -d 'Manage workspace packages'
complete -f -c yarn -n __fish_use_subcommand -a workspaces -d 'Show workspaces information'

# These are the yarn commands all of them use the same options
set -g yarn_cmds add bin cache config exec info init install link node pack remove run unlink unplug up upgrade-interactive version why workspace workspaces

# Common short, long options
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l help -s h -d 'output usage information'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l version -s V -d 'output the version number'

# The rest of common options are all of them long
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l verbose -d 'output verbose messages on internal operations'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l offline -d 'trigger an error if any required dependencies are not available in local cache'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l prefer-offline -d 'use network only if dependencies are not available in local cache'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l strict-semver
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l json
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l ignore-scripts -d 'don\'t run lifecycle scripts'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l har -d 'save HAR output of network traffic'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l ignore-platform -d 'ignore platform checks'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l ignore-engines -d 'ignore engines check'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l ignore-optional -d 'ignore optional dependencies'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l force -d 'ignore all caches'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l no-bin-links -d 'don\'t generate bin links when setting up packages'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l flat -d 'only allow one version of a package'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l 'prod production'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l no-lockfile -d 'don\'t read or generate a lockfile'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l pure-lockfile -d 'don\'t generate a lockfile'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l frozen-lockfile -d 'don\'t generate a lockfile and fail if an update is needed'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l global-folder
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l modules-folder -d 'install modules here instead of node_modules'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l cache-folder -d 'specify a custom folder to store the yarn cache'

complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l mutex -d 'use a mutex to ensure only one yarn instance is executing'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l mutex -a 'file network'

complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l no-emoji -d 'disable emoji in output'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l proxy
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l https-proxy
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l no-progress -d 'disable progress bar'
complete -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l network-concurrency -d 'maximum number of concurrent network requests'
