# NOTE: Fish helper functions are your best friend here!
# see https://github.com/fish-shell/fish-shell/blob/master/share/functions/__fish_seen_subcommand_from.fish
# and https://github.com/fish-shell/fish-shell/blob/master/share/functions/__fish_use_subcommand.fish

# TODO: Access can have the following parameters public|restricted|grant|revoke|ls-packages|ls-collaborators|edit
complete -f -c yarn -n '__fish_use_subcommand' -a access

# TODO: D, dev|P, peer|O, optional|E, exact|T, tilde
complete -f -c yarn -n '__fish_use_subcommand' -a add
complete -f -c yarn -n '__fish_use_subcommand' -a bin

# TODO: ls|dir|clean
complete -f -c yarn -n '__fish_use_subcommand' -a cache

complete -f -c yarn -n '__fish_use_subcommand' -a check
complete -f -c yarn -n '__fish_use_subcommand' -a clean

# TODO: set|get|delete|list
complete -f -c yarn -n '__fish_use_subcommand' -a config

complete -f -c yarn -n '__fish_use_subcommand' -a generate-lock-entry

# TODO: [add|bin|ls|remove|upgrade]
complete -f -c yarn -n '__fish_use_subcommand' -a global
complete -f -c yarn -n '__fish_use_subcommand' -a info

# TODO: y, yes
complete -f -c yarn -n '__fish_use_subcommand' -a init
complete -f -c yarn -n '__fish_use_subcommand' -a install

# TODO: [ls|generate-disclaimer]
complete -f -c yarn -n '__fish_use_subcommand' -a licenses 
complete -f -c yarn -n '__fish_use_subcommand' -a link 

# TODO: depth
complete -f -c yarn -n '__fish_use_subcommand' -a list 
complete -f -c yarn -n '__fish_use_subcommand' -a login 
complete -f -c yarn -n '__fish_use_subcommand' -a logout 
complete -f -c yarn -n '__fish_use_subcommand' -a outdated 

# TODO: [add|rm|ls]
complete -f -c yarn -n '__fish_use_subcommand' -a owner 

# TODO: f, filename
complete -f -c yarn -n '__fish_use_subcommand' -a pack 

# TODO: [--tag <tag>] [--access <public|restricted>] --new-version [version] --message --no-git-tag-version --access --tag
complete -f -c yarn -n '__fish_use_subcommand' -a publish 

complete -f -c yarn -n '__fish_use_subcommand' -a remove 
complete -f -c yarn -n '__fish_use_subcommand' -a run 

# TODO: [add|rm|ls]
complete -f -c yarn -n '__fish_use_subcommand' -a tag 

# TODO: [create|destroy|add|rm|ls]
complete -f -c yarn -n '__fish_use_subcommand' -a team 

complete -f -c yarn -n '__fish_use_subcommand' -a unlink 
complete -f -c yarn -n '__fish_use_subcommand' -a upgrade 
complete -f -c yarn -n '__fish_use_subcommand' -a upgrade-interactive 

# TODO: --new-version --message --no-git-tag-version
complete -f -c yarn -n '__fish_use_subcommand' -a version

complete -f -c yarn -n '__fish_use_subcommand' -a versions 
complete -f -c yarn -n '__fish_use_subcommand' -a why 

# These are the yarn commands all of them use the same options
set -l YARN_COMMANDS access add bin cache check \
clean config generate-lock-entry global info init install \
licenses link list login logout outdated owner pack \
publish remove run tag team unlink upgrade upgrade-interactive \
version versions why

# Common short, long options
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l help -s h -d 'output usage information'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l version -s V -d 'output the version number'

# The rest of common options are all of them long
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l verbose -d 'output verbose messages on internal operations'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l offline -d 'trigger an error if any required dependencies are not available in local cache'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l prefer-offline -d 'use network only if dependencies are not available in local cache'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l strict-semver
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l json
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l ignore-scripts -d 'don\'t run lifecycle scripts'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l har -d 'save HAR output of network traffic'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l ignore-platform -d 'ignore platform checks'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l ignore-engines -d 'ignore engines check'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l ignore-optional -d 'ignore optional dependencies'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l force -d 'ignore all caches'    
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l no-bin-links -d 'don\'t generate bin links when setting up packages'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l flat-d 'only allow one version of a package'  
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l 'prod production'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l no-lockfile -d 'don\'t read or generate a lockfile'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l pure-lockfile -d 'don\'t generate a lockfile'           
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l frozen-lockfile -d 'don\'t generate a lockfile and fail if an update is needed'

# TODO we can put here a global folder or path
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l global-folder

# TODO we can put here a global folder or path
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l modules-folder -d 'rather than installing modules into the node_modules folder relative to the cwd, output them here'

# TODO we can put here a global folder or path
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l cache-folder -d 'specify a custom folder to store the yarn cache'

# TODO: We can specify mutex, file or network
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l mutex -d 'use a mutex to ensure only one yarn instance is executing'

complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l no-emoji -d 'disable emoji in output'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l proxy
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l https-proxy
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l no-progress -d 'disable progress bar'
complete -f -c yarn -n '__fish_seen_subcommand $(YARN_COMMANDS)' -l network-concurrency -d 'maximum number of concurrent network requests'