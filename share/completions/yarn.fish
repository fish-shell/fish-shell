# NOTE: Fish helper functions are your best friend here!
# see https://github.com/fish-shell/fish-shell/blob/master/share/functions/__fish_seen_subcommand_from.fish
# and https://github.com/fish-shell/fish-shell/blob/master/share/functions/__fish_use_subcommand.fish

complete -f -c yarn -n '__fish_use_subcommand' -a help

complete -f -c yarn -n '__fish_use_subcommand' -a access
complete -f -c yarn -n '__fish_seen_subcommand_from access' -a 'public restricted grant revoke ls-packages ls-collaborators edit'

complete -f -c yarn -n '__fish_use_subcommand' -a add
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l dev -s D
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l peer -s P
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l O -s O
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l exact -s E
complete -f -c yarn -n '__fish_seen_subcommand_from add' -l tilde -s T

complete -f -c yarn -n '__fish_use_subcommand' -a bin

complete -f -c yarn -n '__fish_use_subcommand' -a cache
complete -f -c yarn -n '__fish_seen_subcommand_from cache' -a 'ls dir clean'

complete -f -c yarn -n '__fish_use_subcommand' -a check
complete -f -c yarn -n '__fish_use_subcommand' -a clean

complete -f -c yarn -n '__fish_use_subcommand' -a config
complete -f -c yarn -n '__fish_seen_subcommand_from config' -a 'set get delete list'

complete -f -c yarn -n '__fish_use_subcommand' -a generate-lock-entry

complete -f -c yarn -n '__fish_use_subcommand' -a global
complete -f -c yarn -n '__fish_seen_subcommand_from global' -a 'add bin ls remove upgrade'
complete -f -c yarn -n '__fish_use_subcommand' -a info

complete -f -c yarn -n '__fish_use_subcommand' -a init
complete -f -c yarn -n '__fish_seen_subcommand_from init' -s y -l yes

complete -f -c yarn -n '__fish_use_subcommand' -a install

complete -f -c yarn -n '__fish_use_subcommand' -a licenses
complete -f -c yarn -n '__fish_seen_subcommand_from licenses' -a 'ls generate-disclaimer'
complete -f -c yarn -n '__fish_use_subcommand' -a link

complete -f -c yarn -n '__fish_use_subcommand' -a list
complete -f -c yarn -n '__fish_seen_subcommand_from list' -l depth

complete -f -c yarn -n '__fish_use_subcommand' -a login
complete -f -c yarn -n '__fish_use_subcommand' -a logout
complete -f -c yarn -n '__fish_use_subcommand' -a outdated

complete -f -c yarn -n '__fish_use_subcommand' -a owner
complete -f -c yarn -n '__fish_seen_subcommand_from owner' -a 'add rm ls'

complete -f -c yarn -n '__fish_use_subcommand' -a pack

complete -f -c yarn -n '__fish_use_subcommand' -a publish
complete -f -c yarn -n '__fish_seen_subcommand_from publish' -l access -a 'public restricted'
complete -f -c yarn -n '__fish_seen_subcommand_from publish' -l tag
complete -f -c yarn -n '__fish_seen_subcommand_from publish' -l new-version
complete -f -c yarn -n '__fish_seen_subcommand_from publish' -l message
complete -f -c yarn -n '__fish_seen_subcommand_from publish' -l no-git-tag-version
complete -f -c yarn -n '__fish_seen_subcommand_from publish' -l access
complete -f -c yarn -n '__fish_seen_subcommand_from publish' -l tag

complete -f -c yarn -n '__fish_use_subcommand' -a remove
complete -f -c yarn -n '__fish_use_subcommand' -a run

function __fish_yarn_run
  if test -e package.json; and type -q jq
    jq -r '.scripts | to_entries | map("\(.key)\t\(.value | tostring | .[0:20])") | .[]' package.json
  else if type -q jq
    command yarn run --json 2> /dev/null | jq -r '.data.hints? | to_entries | map("\(.key)\t\(.value | tostring |.[0:20])") | .[]'
  end
end

complete -f -c yarn -n '__fish_seen_subcommand_from run' -a "(__fish_yarn_run)"

complete -f -c yarn -n '__fish_use_subcommand' -a tag
complete -f -c yarn -n '__fish_seen_subcommand_from tag' -a 'add rm ls'

complete -f -c yarn -n '__fish_use_subcommand' -a team
complete -f -c yarn -n '__fish_seen_subcommand_from team' -a 'create destroy add rm ls'

complete -f -c yarn -n '__fish_use_subcommand' -a unlink
complete -f -c yarn -n '__fish_use_subcommand' -a upgrade
complete -f -c yarn -n '__fish_use_subcommand' -a upgrade-interactive

complete -f -c yarn -n '__fish_use_subcommand' -a version
complete -f -c yarn -n '__fish_seen_subcommand_from version' -l new-version
complete -f -c yarn -n '__fish_seen_subcommand_from version' -l message
complete -f -c yarn -n '__fish_seen_subcommand_from version' -l no-git-tag-version

complete -f -c yarn -n '__fish_use_subcommand' -a versions
complete -f -c yarn -n '__fish_use_subcommand' -a why

# These are the yarn commands all of them use the same options
set -g yarn_cmds access add bin cache check clean config generate-lock-entry global info init install licenses link list login logout outdated owner pack publish remove run tag team unlink upgrade upgrade-interactive version versions why

# Common short, long options
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l help -s h -d 'output usage information'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l version -s V -d 'output the version number'

# The rest of common options are all of them long
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l verbose -d 'output verbose messages on internal operations'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l offline -d 'trigger an error if any required dependencies are not available in local cache'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l prefer-offline -d 'use network only if dependencies are not available in local cache'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l strict-semver
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l json
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l ignore-scripts -d 'don\'t run lifecycle scripts'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l har -d 'save HAR output of network traffic'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l ignore-platform -d 'ignore platform checks'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l ignore-engines -d 'ignore engines check'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l ignore-optional -d 'ignore optional dependencies'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l force -d 'ignore all caches'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l no-bin-links -d 'don\'t generate bin links when setting up packages'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l flat -d 'only allow one version of a package'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l 'prod production'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l no-lockfile -d 'don\'t read or generate a lockfile'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l pure-lockfile -d 'don\'t generate a lockfile'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l frozen-lockfile -d 'don\'t generate a lockfile and fail if an update is needed'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l global-folder
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l modules-folder -d 'rather than installing modules into the node_modules folder relative to the cwd, output them here'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l cache-folder -d 'specify a custom folder to store the yarn cache'

complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l mutex -d 'use a mutex to ensure only one yarn instance is executing'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l mutex -a 'file network'

complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l no-emoji -d 'disable emoji in output'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l proxy
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l https-proxy
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l no-progress -d 'disable progress bar'
complete -f -c yarn -n '__fish_seen_subcommand_from $yarn_cmds' -l network-concurrency -d 'maximum number of concurrent network requests'
