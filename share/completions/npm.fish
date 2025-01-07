# npm (https://npmjs.org) completions for Fish shell
# __fish_npm_needs_* and __fish_npm_using_* taken from:
# https://stackoverflow.com/questions/16657803/creating-autocomplete-script-with-sub-commands
# see also Fish's large set of completions for examples:
# https://github.com/fish-shell/fish-shell/tree/master/share/completions

source $__fish_data_dir/functions/__fish_npm_helper.fish
set -l npm_install "npm install --global"

function __fish_npm_needs_command
    set -l cmd (commandline -xpc)

    if test (count $cmd) -eq 1
        return 0
    end

    return 1
end

function __fish_npm_using_command
    set -l cmd (commandline -xpc)

    if test (count $cmd) -gt 1
        if contains -- $cmd[2] $argv
            return 0
        end
    end

    return 1
end

function __fish_npm_needs_option
    switch (commandline -ct)
        case "-*"
            return 0
    end
    return 1
end

# list available npm scripts and their parial content
function __fish_parse_npm_run_completions
    while read -l name
        set -l trim 20
        read -l value
        set value (string sub -l $trim -- $value)
        printf "%s\t%s\n" $name $value
    end
end

function __fish_npm_run
    # Complete `npm run` scripts
    # These are stored in package.json, which we need a tool to read.
    # python is very probably installed (we use it for other things!),
    # jq is slower but also a common tool,
    # npm is dog-slow and might check for updates online!
    if test -e package.json; and set -l python (__fish_anypython)
        # Warning: That weird indentation is necessary, because python.
        $python -S -c 'import json, sys; data = json.load(sys.stdin);
for k,v in data["scripts"].items(): print(k + "\t" + v[:18])' <package.json 2>/dev/null
    else if command -sq jq; and test -e package.json
        jq -r '.scripts | to_entries | map("\(.key)\t\(.value | tostring | .[0:20])") | .[]' package.json
    else if command -sq npm
        # Like above, only try to call npm if there's a command by that name to facilitate aliases that call nvm.
        command npm run | string match -r -v '^[^ ]|^$' | string trim | __fish_parse_npm_run_completions
    end
end

# run
complete -f -c npm -n __fish_npm_needs_command -a 'run-script run' -d 'Run arbitrary package scripts'
for c in run-script run rum urn
    complete -f -c npm -n "__fish_npm_using_command $c" -a "(__fish_npm_run)"
    complete -f -c npm -n "__fish_npm_using_command $c" -l if-present -d "Don't error on nonexistant script"
    complete -f -c npm -n "__fish_npm_using_command $c" -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -x -c npm -n "__fish_npm_using_command $c" -s script-shell -d 'The shell to use for scripts'
    complete -f -c npm -n "__fish_npm_using_command $c" -l foreground-scripts -d 'Run all build scripts in the foreground'
end

# access
set -l access_commands 'list get set grant revoke'
complete -f -c npm -n __fish_npm_needs_command -a access -d 'Set access level on published packages'
complete -x -c npm -n '__fish_npm_using_command access' -n "not __fish_seen_subcommand_from $access_commands" -a list -d 'List access info'
complete -x -c npm -n '__fish_npm_using_command access' -n "not __fish_seen_subcommand_from $access_commands" -a get -d 'Get access level'
complete -x -c npm -n '__fish_npm_using_command access' -n "not __fish_seen_subcommand_from $access_commands" -a grant -d 'Grant access to users'
complete -x -c npm -n '__fish_npm_using_command access' -n "not __fish_seen_subcommand_from $access_commands" -a revoke -d 'Revoke access from users'
complete -x -c npm -n '__fish_npm_using_command access' -n "not __fish_seen_subcommand_from $access_commands" -a set -d 'Set access level'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from list' -a 'packages collaborators'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from get' -a status
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from grant' -a 'read-only read-write'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from set' -a 'status=public status=private' -d 'Set package status'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from set' -a 'mfa=none mfa=publish mfa=automation' -d 'Set package MFA'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from set' -a '2fa=none 2fa=publish 2fa=automation' -d 'Set package MFA'
complete -f -c npm -n '__fish_npm_using_command access' -l json -d 'Output JSON'
complete -x -c npm -n '__fish_npm_using_command access' -l otp -d '2FA one-time password'
complete -x -c npm -n '__fish_npm_using_command access' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command access' -s h -l help -d 'Display help'

# adduser
complete -f -c npm -n __fish_npm_needs_command -a adduser -d 'Add a registry user account'
complete -f -c npm -n __fish_npm_needs_command -a login -d 'Login to a registry user account'
for c in adduser add-user login
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
    complete -x -c npm -n "__fish_npm_using_command $c" -l scope -d 'Log into a private repository'
    complete -x -c npm -n "__fish_npm_using_command $c" -l auth-type -a 'legacy web' -d 'Authentication strategy'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# audit
complete -f -c npm -n __fish_npm_needs_command -a audit -d 'Run a security audit'
complete -f -c npm -n '__fish_npm_using_command audit' -a signatures -d 'Verify registry signatures'
complete -f -c npm -n '__fish_npm_using_command audit' -a fix -d 'Install compatible updates to vulnerable deps'
complete -x -c npm -n '__fish_npm_using_command audit' -l audit-level -a 'info low moderate high critical none' -d 'Audit level'
complete -f -c npm -n '__fish_npm_using_command audit' -l dry-run -d 'Do not make any changes'
complete -f -c npm -n '__fish_npm_using_command audit' -s f -l force -d 'Removes various protections'
complete -f -c npm -n '__fish_npm_using_command audit' -l json -d 'Output JSON'
complete -f -c npm -n '__fish_npm_using_command audit' -l package-lock-only -d 'Only use package-lock.json, ignore node_modules'
complete -x -c npm -n '__fish_npm_using_command audit' -l omit -a 'dev optional peer' -d 'Omit dependency type'
complete -f -c npm -n '__fish_npm_using_command audit' -l foreground-scripts -d 'Run all build scripts in the foreground'
complete -f -c npm -n '__fish_npm_using_command audit' -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
complete -f -c npm -n '__fish_npm_using_command audit' -l install-links -d 'Install file: protocol deps as regular deps'
complete -f -c npm -n '__fish_npm_using_command audit' -s h -l help -d 'Display help'

# bugs
for c in bugs issues
    complete -f -c npm -n __fish_npm_needs_command -a "$c" -d 'Report bugs for a package in a web browser'
    complete -x -c npm -n "__fish_npm_using_command $c" -l browser -d 'Set browser'
    complete -x -c npm -n "__fish_npm_using_command $c" -l no-browser -d 'Print to stdout'
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# cache
complete -f -c npm -n __fish_npm_needs_command -a cache -d "Manipulates package's cache"
complete -f -c npm -n '__fish_npm_using_command cache' -a add -d 'Add the specified package to the local cache'
complete -f -c npm -n '__fish_npm_using_command cache' -a clean -d 'Delete data out of the cache folder'
complete -f -c npm -n '__fish_npm_using_command cache' -a ls -d 'Show the data in the cache'
complete -f -c npm -n '__fish_npm_using_command cache' -a verify -d 'Verify the contents of the cache folder'
complete -x -c npm -n '__fish_npm_using_command cache' -l cache -a '(__fish_complete_directories)' -d 'Cache location'
complete -f -c npm -n '__fish_npm_using_command cache' -s h -l help -d 'Display help'

# ci
# install-ci-test
complete -f -c npm -n __fish_npm_needs_command -a 'ci clean-install' -d 'Clean install a project'
complete -f -c npm -n __fish_npm_needs_command -a 'install-ci-test cit' -d 'Install a project with a clean slate and run tests'
# typos are intentional
for c in ci clean-install ic install-clean isntall-clean install-ci-test cit clean-install-test sit
    complete -x -c npm -n "__fish_npm_using_command $c" -l install-strategy -a 'hoisted nested shallow linked' -d 'Install strategy'
    complete -x -c npm -n "__fish_npm_using_command $c" -l omit -a 'dev optional peer' -d 'Omit dependency type'
    complete -x -c npm -n "__fish_npm_using_command $c" -l strict-peer-deps -d 'Treat conflicting peerDependencies as failure'
    complete -f -c npm -n "__fish_npm_using_command $c" -l foreground-scripts -d 'Run all build scripts in the foreground'
    complete -f -c npm -n "__fish_npm_using_command $c" -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-audit -d "Don't submit audit reports"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-bin-links -d "Don't symblink package executables"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-fund -d "Don't display funding info"
    complete -f -c npm -n "__fish_npm_using_command $c" -l dry-run -d 'Do not make any changes'
    complete -f -c npm -n "__fish_npm_using_command $c" -l install-links -d 'Install file: protocol deps as regular deps'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# completion
complete -f -c npm -n __fish_npm_needs_command -a completion -d 'Tab Completion for npm'
complete -f -c npm -n '__fish_npm_using_command completion' -s h -l help -d 'Display help'

# config
complete -f -c npm -n __fish_npm_needs_command -a config -d 'Manage the npm configuration files'
for c in config c
    set -l config_commands 'set get delete list edit fix'
    complete -x -c npm -n "__fish_npm_using_command $c" -n "not __fish_seen_subcommand_from $config_commands" -a set -d 'Sets the config keys to the values'
    complete -x -c npm -n "__fish_npm_using_command $c" -n "not __fish_seen_subcommand_from $config_commands" -a get -d 'Echo the config value(s) to stdout'
    complete -x -c npm -n "__fish_npm_using_command $c" -n "not __fish_seen_subcommand_from $config_commands" -a delete -d 'Deletes the key from all config files'
    complete -x -c npm -n "__fish_npm_using_command $c" -n "not __fish_seen_subcommand_from $config_commands" -a list -d 'Show all config settings'
    complete -x -c npm -n "__fish_npm_using_command $c" -n "not __fish_seen_subcommand_from $config_commands" -a edit -d 'Opens the config file in an editor'
    complete -x -c npm -n "__fish_npm_using_command $c" -n "not __fish_seen_subcommand_from $config_commands" -a fix -d 'Attempts to repair invalid config items'
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON'
    complete -f -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Edit global config'
    complete -x -c npm -n "__fish_npm_using_command $c" -l editor -d 'Specify the editor'
    complete -x -c npm -n "__fish_npm_using_command $c" -s L -l location -a 'global user project' -d 'Which config file'
    complete -f -c npm -n "__fish_npm_using_command $c" -s l -l long -d 'Show extended information'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end
# get, set also exist as shorthands
complete -f -c npm -n __fish_npm_needs_command -a get -d 'Get a value from the npm configuration'
complete -f -c npm -n '__fish_npm_using_command get' -s l -l long -d 'Show extended information'
complete -f -c npm -n '__fish_npm_using_command get' -s h -l help -d 'Display help'
# set
complete -f -c npm -n __fish_npm_needs_command -a set -d 'Set a value in the npm configuration'
complete -x -c npm -n '__fish_npm_using_command set' -s L -l location -a 'global user project' -d 'Which config file'
complete -f -c npm -n '__fish_npm_using_command set' -s g -l global -d 'Edit global config'
complete -f -c npm -n '__fish_npm_using_command set' -s h -l help -d 'Display help'

# dedupe
complete -f -c npm -n __fish_npm_needs_command -a dedupe -d 'Reduce duplication'
complete -f -c npm -n __fish_npm_needs_command -a find-dupes -d 'Find duplication'
for c in dedupe ddp find-dupes
    complete -x -c npm -n "__fish_npm_using_command $c" -l install-strategy -a 'hoisted nested shallow linked' -d 'Install strategy'
    complete -x -c npm -n "__fish_npm_using_command $c" -l strict-peer-deps -d 'Treat conflicting peerDependencies as failure'
    complete -x -c npm -n "__fish_npm_using_command $c" -l no-package-lock -d 'Ignore package-lock.json'
    complete -x -c npm -n "__fish_npm_using_command $c" -l omit -a 'dev optional peer' -d 'Omit dependency type'
    complete -f -c npm -n "__fish_npm_using_command $c" -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-audit -d "Don't submit audit reports"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-bin-links -d "Don't symblink package executables"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-fund -d "Don't display funding info"
    complete -f -c npm -n "__fish_npm_using_command $c" -l install-links -d 'Install file: protocol deps as regular deps'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'

    if test $c != find-dupes
        complete -f -c npm -n "__fish_npm_using_command $c" -l dry-run -d "Don't display funding info"
    end
end

# deprecate
complete -f -c npm -n __fish_npm_needs_command -a deprecate -d 'Deprecate a version of a package'
complete -x -c npm -n '__fish_npm_using_command deprecate' -l registry -d 'Registry base URL'
complete -x -c npm -n '__fish_npm_using_command deprecate' -l otp -d '2FA one-time password'
complete -f -c npm -n '__fish_npm_using_command deprecate' -s h -l help -d 'Display help'

# diff
complete -f -c npm -n __fish_npm_needs_command -a diff -d 'The registry diff command'
complete -x -c npm -n '__fish_npm_using_command diff' -l diff -d 'Arguments to compare'
complete -f -c npm -n '__fish_npm_using_command diff' -l diff-name-only -d 'Prints only filenames'
complete -x -c npm -n '__fish_npm_using_command diff' -l diff-unified -d 'The number of lines to print'
complete -f -c npm -n '__fish_npm_using_command diff' -l diff-ignore-all-space -d 'Ignore whitespace'
complete -f -c npm -n '__fish_npm_using_command diff' -l diff-no-prefix -d 'Do not show any prefix'
complete -x -c npm -n '__fish_npm_using_command diff' -l diff-src-prefix -d 'Source prefix'
complete -x -c npm -n '__fish_npm_using_command diff' -l diff-dst-prefix -d 'Destination prefix'
complete -f -c npm -n '__fish_npm_using_command diff' -l diff-text -d 'Treat all files as text'
complete -f -c npm -n '__fish_npm_using_command diff' -s g -l global -d 'Operates in "global" mode'
complete -x -c npm -n '__fish_npm_using_command diff' -l tag -d 'The tag used to fetch the tarball'
complete -f -c npm -n '__fish_npm_using_command diff' -s h -l help -d 'Display help'

# dist-tag
complete -f -c npm -n __fish_npm_needs_command -a dist-tag -d 'Modify package distribution tags'
for c in dist-tag dist-tags
    complete -f -c npm -n "__fish_npm_using_command $c" -a add -d 'Tag the package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a rm -d 'Clear a tag from the package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a ls -d 'List all dist-tags'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# docs
complete -f -c npm -n __fish_npm_needs_command -a docs -d 'Open docs for a package in a web browser'
for c in docs home
    complete -x -c npm -n "__fish_npm_using_command $c" -l browser -d 'Set browser'
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# doctor
complete -f -c npm -n __fish_npm_needs_command -a doctor -d 'Check your npm environment'
complete -f -c npm -n '__fish_npm_using_command doctor' -a ping -d 'Check npm ping'
complete -f -c npm -n '__fish_npm_using_command doctor' -a registry -d 'Check registry'
complete -f -c npm -n '__fish_npm_using_command doctor' -a versions -d 'Check installed versions'
complete -f -c npm -n '__fish_npm_using_command doctor' -a environment -d 'Check PATH'
complete -f -c npm -n '__fish_npm_using_command doctor' -a permissions -d 'Check file permissions'
complete -f -c npm -n '__fish_npm_using_command doctor' -a cache -d 'Verify cache'
complete -f -c npm -n '__fish_npm_using_command doctor' -s h -l help -d 'Display help'

# edit
complete -f -c npm -n __fish_npm_needs_command -a edit -d 'Edit an installed package'
complete -f -c npm -n '__fish_npm_using_command edit' -l editor -d 'Specify the editor'
complete -f -c npm -n '__fish_npm_using_command edit' -s h -l help -d 'Display help'

# exec
complete -f -c npm -n __fish_npm_needs_command -a exec -d 'Run a command from a local or remote npm package'
for c in exec x
    complete -x -c npm -n "__fish_npm_using_command $c" -l package -d 'The package(s) to install'
    complete -x -c npm -n "__fish_npm_using_command $c" -l call -d 'Specify a custom command'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# explain
for c in explain why
    complete -f -c npm -n __fish_npm_needs_command -a "$c" -d 'Explain installed packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# explore
complete -f -c npm -n __fish_npm_needs_command -a explore -d 'Browse an installed package'
complete -f -c npm -n '__fish_npm_using_command explore' -a shell -d 'The shell to open'
complete -f -c npm -n '__fish_npm_using_command explore' -s h -l help -d 'Display help'

# fund
complete -f -c npm -n __fish_npm_needs_command -a fund -d 'Retrieve funding information'
complete -f -c npm -n '__fish_npm_using_command fund' -l json -d 'Output JSON'
complete -x -c npm -n '__fish_npm_using_command fund' -l browser -d 'Set browser'
complete -f -c npm -n '__fish_npm_using_command fund' -l no-browser -d 'Print to stdout'
complete -f -c npm -n '__fish_npm_using_command fund' -l unicode -d 'Use unicode characters in the output'
complete -f -c npm -n '__fish_npm_using_command fund' -l no-unicode -d 'Use ascii characters over unicode glyphs'
complete -x -c npm -n '__fish_npm_using_command fund' -l which -d 'Which source URL to open (1-indexed)'
complete -f -c npm -n '__fish_npm_using_command fund' -s h -l help -d 'Display help'

# help
complete -f -c npm -n __fish_npm_needs_command -a help -d 'Get help on npm'
for c in help hlep
    complete -f -c npm -n "__fish_npm_using_command $c" -l viewer -a 'browser man' -d 'Program to view content'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
    complete -f -c npm -n "__fish_npm_using_command $c" -a registry -d 'The JavaScript Package Registry'
    complete -f -c npm -n "__fish_npm_using_command $c" -a removal -d 'Cleaning the Slate'
    complete -f -c npm -n "__fish_npm_using_command $c" -a logging -d 'Why, What & How We Log'
    complete -f -c npm -n "__fish_npm_using_command $c" -a scope -d 'How npm handles the "scripts" field'
    complete -f -c npm -n "__fish_npm_using_command $c" -a dependency-selectors -d 'Dependency Selector Syntax & Querying'
    complete -f -c npm -n "__fish_npm_using_command $c" -a npm -d 'javascript package manager'
    complete -f -c npm -n "__fish_npm_using_command $c" -a npmrc -d 'The npm config files'
    complete -f -c npm -n "__fish_npm_using_command $c" -a shrinkwrap -d 'A publishable lockfile'
    complete -f -c npm -n "__fish_npm_using_command $c" -a developers -d 'Developer Guide'
    complete -f -c npm -n "__fish_npm_using_command $c" -a npx -d 'Run a command from a local or remote npm package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a package-json -d "Specifics of npm's package.json handling"
    complete -f -c npm -n "__fish_npm_using_command $c" -a package-lock-json -d 'A manifestation of the manifest'
    complete -f -c npm -n "__fish_npm_using_command $c" -a package-spec -d 'Package name specifier'
    complete -f -c npm -n "__fish_npm_using_command $c" -a folders -d 'Folder Structures Used by npm'
    complete -f -c npm -n "__fish_npm_using_command $c" -a global -d 'Folder Structures Used by npm'
    complete -f -c npm -n "__fish_npm_using_command $c" -a workspaces -d 'FolderWorking with workspaces'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'run-script run' -d 'Run arbitrary package scripts'
    complete -f -c npm -n "__fish_npm_using_command $c" -a access -d 'Set access level on published packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -a adduser -d 'Add a registry user account'
    complete -f -c npm -n "__fish_npm_using_command $c" -a login -d 'Login to a registry user account'
    complete -f -c npm -n "__fish_npm_using_command $c" -a audit -d 'Run a security audit'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'bugs issues' -d 'Report bugs for a package in a web browser'
    complete -f -c npm -n "__fish_npm_using_command $c" -a cache -d "Manipulates package's cache"
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'ci clean-install' -d 'Clean install a project'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'install-ci-test cit' -d 'Install a project with a clean slate and run tests'
    complete -f -c npm -n "__fish_npm_using_command $c" -a config -d 'Manage the npm configuration files'
    complete -f -c npm -n "__fish_npm_using_command $c" -a dedupe -d 'Reduce duplication'
    complete -f -c npm -n "__fish_npm_using_command $c" -a find-dupes -d 'Find duplication'
    complete -f -c npm -n "__fish_npm_using_command $c" -a deprecate -d 'Deprecate a version of a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a diff -d 'The registry diff command'
    complete -f -c npm -n "__fish_npm_using_command $c" -a dist-tag -d 'Modify package distribution tags'
    complete -f -c npm -n "__fish_npm_using_command $c" -a docs -d 'Open docs for a package in a web browser'
    complete -f -c npm -n "__fish_npm_using_command $c" -a doctor -d 'Check your npm environment'
    complete -f -c npm -n "__fish_npm_using_command $c" -a edit -d 'Edit an installed package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a exec -d 'Run a command from a local or remote npm package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'explaiin why' -d 'Explain installed packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -a explore -d 'Browse an installed package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a fund -d 'Retrieve funding information'
    complete -f -c npm -n "__fish_npm_using_command $c" -a help -d 'Get help on npm'
    complete -f -c npm -n "__fish_npm_using_command $c" -a help-search -d 'Search npm help documentation'
    complete -f -c npm -n "__fish_npm_using_command $c" -a hook -d 'Manage registry hooks'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'init create' -d 'Create a package.json file'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'install add i' -d 'Install a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'install-test it' -d 'Install package(s) and run tests'
    complete -f -c npm -n "__fish_npm_using_command $c" -a logout -d 'Log out of the registry'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'ls list' -d 'List installed packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -a outdated -d 'Check for outdated packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -a org -d 'Manage orgs'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'owner author' -d 'Manage package owners'
    complete -f -c npm -n "__fish_npm_using_command $c" -a pack -d 'Create a tarball from a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a ping -d 'Ping npm registry'
    complete -f -c npm -n "__fish_npm_using_command $c" -a pkg -d 'Manages your package.json'
    complete -f -c npm -n "__fish_npm_using_command $c" -a prefix -d 'Display npm prefix'
    complete -f -c npm -n "__fish_npm_using_command $c" -a publish -d 'Publish a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a query -d 'Dependency selector query'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'uninstall remove un' -d 'Remove a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a repo -d 'Open package repository page in the browser'
    complete -f -c npm -n "__fish_npm_using_command $c" -a restart -d 'Restart a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a start -d 'Start a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a stop -d 'Stop a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a test -d 'Test a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a root -d 'Display npm root'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'search find' -d 'Search for packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -a star -d 'Mark your favorite packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -a stars -d 'View packages marked as favorites'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'update up upgrade' -d 'Update package(s)'
    complete -f -c npm -n "__fish_npm_using_command $c" -a unstar -d 'Remove star from a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a version -d 'Bump a package version'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'view info' -d 'View registry info'
    complete -f -c npm -n "__fish_npm_using_command $c" -a whoami -d 'Display npm username'
    complete -f -c npm -n "__fish_npm_using_command $c" -a 'link ln' -d 'Symlink a package folder'
    complete -f -c npm -n "__fish_npm_using_command $c" -a profile -d 'Change settings on your registry profile'
    complete -f -c npm -n "__fish_npm_using_command $c" -a prune -d 'Remove extraneous packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -a rebuild -d 'Rebuild a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a team -d 'Manage organization teams and team memberships'
    complete -f -c npm -n "__fish_npm_using_command $c" -a token -d 'Manage your authentication tokens'
    complete -f -c npm -n "__fish_npm_using_command $c" -a unpublish -d 'Remove a package from the registry'
    complete -f -c npm -n "__fish_npm_using_command $c" -a completion -d 'Tab Completion for npm'
    complete -f -c npm -n "__fish_npm_using_command $c" -a shrinkwrap -d 'Lock down dependency versions'
end

# help-search
complete -f -c npm -n __fish_npm_needs_command -a help-search -d 'Search npm help documentation'
complete -f -c npm -n '__fish_npm_using_command help-search' -s l -l long -d 'Show extended information'
complete -f -c npm -n '__fish_npm_using_command help-search' -s h -l help -d 'Display help'

# hook
set -l hook_commands 'add ls update rm'
complete -f -c npm -n __fish_npm_needs_command -a hook -d 'Manage registry hooks'
complete -f -c npm -n '__fish_npm_using_command hook' -n "not __fish_seen_subcommand_from $hook_commands" -a add -d 'Add a hook'
complete -f -c npm -n '__fish_npm_using_command hook' -n "not __fish_seen_subcommand_from $hook_commands" -a ls -d 'List all active hooks'
complete -f -c npm -n '__fish_npm_using_command hook' -n "not __fish_seen_subcommand_from $hook_commands" -a update -d 'Update an existing hook'
complete -f -c npm -n '__fish_npm_using_command hook' -n "not __fish_seen_subcommand_from $hook_commands" -a rm -d 'Remove a hook'
complete -f -c npm -n '__fish_npm_using_command hook' -n '__fish_seen_subcommand_from add' -l type -d 'Hook type'
complete -x -c npm -n '__fish_npm_using_command hook' -l registry -d 'Registry base URL'
complete -x -c npm -n '__fish_npm_using_command hook' -l otp -d '2FA one-time password'
complete -f -c npm -n '__fish_npm_using_command hook' -s h -l help -d 'Display help'

# init
complete -c npm -n __fish_npm_needs_command -a 'init create' -d 'Create a package.json file'
for c in init create innit
    complete -f -c npm -n "__fish_npm_using_command $c" -s y -l yes -d 'Automatically answer "yes" to all prompts'
    complete -f -c npm -n "__fish_npm_using_command $c" -s f -l force -d 'Removes various protections'
    complete -x -c npm -n "__fish_npm_using_command $c" -l scope -d 'Create a scoped package'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# install
# install-test
# link
complete -c npm -n __fish_npm_needs_command -a 'install add i' -d 'Install a package'
complete -f -c npm -n __fish_npm_needs_command -a 'install-test it' -d 'Install package(s) and run tests'
complete -f -c npm -n __fish_npm_needs_command -a 'link ln' -d 'Symlink a package folder'
# typos are intentional
for c in install add i in ins inst insta instal isnt isnta isntal isntall install-test it link ln
    complete -f -c npm -n "__fish_npm_using_command $c" -s S -l save -d 'Save to dependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-save -d 'Prevents saving to dependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -s P -l save-prod -d 'Save to dependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -s D -l save-dev -d 'Save to devDependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -s O -l save-optional -d 'Save to optionalDependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -s B -l save-bundle -d 'Also save to bundleDependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -s E -l save-exact -d 'Save dependency with exact version'
    complete -f -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Install package globally'
    complete -x -c npm -n "__fish_npm_using_command $c" -l install-strategy -a 'hoisted nested shallow linked' -d 'Install strategy'
    complete -x -c npm -n "__fish_npm_using_command $c" -l omit -a 'dev optional peer' -d 'Omit dependency type'
    complete -x -c npm -n "__fish_npm_using_command $c" -l strict-peer-deps -d 'Treat conflicting peerDependencies as failure'
    complete -x -c npm -n "__fish_npm_using_command $c" -l no-package-lock -d 'Ignore package-lock.json'
    complete -f -c npm -n "__fish_npm_using_command $c" -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-audit -d "Don't submit audit reports"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-bin-links -d "Don't symblink package executables"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-fund -d "Don't display funding info"
    complete -f -c npm -n "__fish_npm_using_command $c" -l dry-run -d 'Do not make any changes'
    complete -f -c npm -n "__fish_npm_using_command $c" -l install-links -d 'Install file: protocol deps as regular deps'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'

    if test $c != link -a $c != ln
        complete -f -c npm -n "__fish_npm_using_command $c" -l foreground-scripts -d 'Run all build scripts in the foreground'
        complete -f -c npm -n "__fish_npm_using_command $c" -l prefer-dedupe -d 'Prefer to deduplicate packages'
    end
end

# logout
complete -f -c npm -n __fish_npm_needs_command -a logout -d 'Log out of the registry'
complete -x -c npm -n '__fish_npm_using_command logout' -l registry -d 'Registry base URL'
complete -x -c npm -n '__fish_npm_using_command logout' -l scope -d 'Log out of private repository'
complete -f -c npm -n '__fish_npm_using_command logout' -s h -l help -d 'Display help'

# ls
# ll, la
complete -f -c npm -n __fish_npm_needs_command -a 'ls list ll' -d 'List installed packages'
for c in ls list ll la
    complete -f -c npm -n "__fish_npm_using_command $c" -s a -l all -d 'Also show indirect dependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON'
    complete -f -c npm -n "__fish_npm_using_command $c" -s l -l long -d 'Show extended information'
    complete -f -c npm -n "__fish_npm_using_command $c" -s p -l parseable -d 'Output parseable results'
    complete -f -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'List global packages'
    complete -x -c npm -n "__fish_npm_using_command $c" -l depth -d 'Dependency recursion depth'
    complete -x -c npm -n "__fish_npm_using_command $c" -l omit -a 'dev optional peer' -d 'Omit dependency type'
    complete -f -c npm -n "__fish_npm_using_command $c" -l linked -d 'Only show linked packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -l package-lock-only -d 'Only use package-lock.json, ignore node_modules'
    complete -f -c npm -n "__fish_npm_using_command $c" -l unicode -d 'Use unicode characters in the output'
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-unicode -d 'Use ascii characters over unicode glyphs'
    complete -f -c npm -n "__fish_npm_using_command $c" -l install-links -d 'Install file: protocol deps as regular deps'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# outdated
complete -f -c npm -n __fish_npm_needs_command -a outdated -d 'Check for outdated packages'
complete -f -c npm -n '__fish_npm_using_command outdated' -s a -l all -d 'Also show indirect dependencies'
complete -f -c npm -n '__fish_npm_using_command outdated' -l json -d 'Output JSON'
complete -f -c npm -n '__fish_npm_using_command outdated' -s l -l long -d 'Show extended information'
complete -f -c npm -n '__fish_npm_using_command outdated' -l parseable -d 'Output parseable results'
complete -f -c npm -n '__fish_npm_using_command outdated' -s g -l global -d 'Check global packages'
complete -f -c npm -n '__fish_npm_using_command outdated' -s h -l help -d 'Display help'

# org
complete -f -c npm -n __fish_npm_needs_command -a org -d 'Manage orgs'
for c in org ogr
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 2' -a set -d 'Add a new user'
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 2' -a rm -d 'Remove a user'
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 2' -a ls -d 'List all users'

    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 5' -n '__fish_seen_subcommand_from set' -a admin -d 'Add admin'
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 5' -n '__fish_seen_subcommand_from set' -a developer -d 'Add developer'
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 5' -n '__fish_seen_subcommand_from set' -a owner -d 'Add owner'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# owner
for c in owner author
    complete -f -c npm -n __fish_npm_needs_command -a "$c" -d 'Manage package owners'
    complete -f -c npm -n "__fish_npm_using_command $c" -a ls -d 'List package owners'
    complete -f -c npm -n "__fish_npm_using_command $c" -a add -d 'Add a new owner to package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a rm -d 'Remove an owner from package'
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
    complete -x -c npm -n "__fish_npm_using_command $c" -l otp -d '2FA one-time password'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# pack
complete -f -c npm -n __fish_npm_needs_command -a pack -d 'Create a tarball from a package'
complete -f -c npm -n '__fish_npm_using_command pack' -l dry-run -d 'Do not make any changes'
complete -f -c npm -n '__fish_npm_using_command pack' -l json -d 'Output JSON'
complete -x -c npm -n '__fish_npm_using_command pack' -l pack-destination -a '(__fish_complete_directories)' -d 'Tarball destination directory'
complete -f -c npm -n '__fish_npm_using_command pack' -s h -l help -d 'Display help'

# ping
complete -f -c npm -n __fish_npm_needs_command -a ping -d 'Ping npm registry'
complete -x -c npm -n '__fish_npm_using_command ping' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command ping' -s h -l help -d 'Display help'

# pkg
complete -f -c npm -n __fish_npm_needs_command -a pkg -d 'Manages your package.json'
complete -x -c npm -n '__fish_npm_using_command pkg' -a set -d 'Sets a value'
complete -x -c npm -n '__fish_npm_using_command pkg' -a get -d 'Retrieves a value'
complete -x -c npm -n '__fish_npm_using_command pkg' -a delete -d 'Deletes a key'
complete -f -c npm -n '__fish_npm_using_command pkg' -s f -l force -d 'Removes various protections'
complete -f -c npm -n '__fish_npm_using_command pkg' -l json -d 'Parse values with JSON.parse()'
complete -f -c npm -n '__fish_npm_using_command pkg' -s h -l help -d 'Display help'

# prefix
complete -f -c npm -n __fish_npm_needs_command -a prefix -d 'Display npm prefix'
complete -f -c npm -n '__fish_npm_using_command prefix' -s g -l global -d 'Display global prefix'
complete -f -c npm -n '__fish_npm_using_command prefix' -s h -l help -d 'Display help'

# profile
set -l profile_commands 'enable-2fa disable-2fa get set'
complete -f -c npm -n __fish_npm_needs_command -a profile -d 'Change settings on your registry profile'
complete -x -c npm -n '__fish_npm_using_command profile' -n "not __fish_seen_subcommand_from $profile_commands" -a enable-2fa -d 'Enables two-factor authentication'
complete -x -c npm -n '__fish_npm_using_command profile' -n "not __fish_seen_subcommand_from $profile_commands" -a disable-2fa -d 'Disables two-factor authentication'
complete -x -c npm -n '__fish_npm_using_command profile' -n "not __fish_seen_subcommand_from $profile_commands" -a get -d 'Display profile properties'
complete -x -c npm -n '__fish_npm_using_command profile' -n "not __fish_seen_subcommand_from $profile_commands" -a set -d 'Set the value of a profile property'
complete -x -c npm -n '__fish_npm_using_command profile' -n '__fish_seen_subcommand_from enable-2fa' -a auth-only -d 'Requiere an OTP on profile changes'
complete -x -c npm -n '__fish_npm_using_command profile' -n '__fish_seen_subcommand_from enable-2fa' -a auth-and-writes -d 'Also requiere an OTP on package changes'
complete -f -c npm -n '__fish_npm_using_command profile' -s h -l help -d 'Display help'

# prune
complete -f -c npm -n __fish_npm_needs_command -a prune -d 'Remove extraneous packages'
complete -x -c npm -n '__fish_npm_using_command prune' -l omit -a 'dev optional peer' -d 'Omit dependency type'
complete -f -c npm -n '__fish_npm_using_command prune' -l dry-run -d 'Do not make any changes'
complete -f -c npm -n '__fish_npm_using_command prune' -l json -d 'Output JSON'
complete -f -c npm -n '__fish_npm_using_command prune' -l foreground-scripts -d 'Run all build scripts in the foreground'
complete -f -c npm -n '__fish_npm_using_command prune' -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
complete -f -c npm -n '__fish_npm_using_command prune' -l install-links -d 'Install file: protocol deps as regular deps'
complete -f -c npm -n '__fish_npm_using_command prune' -s h -l help -d 'Display help'

# publish
complete -f -c npm -n __fish_npm_needs_command -a publish -d 'Publish a package'
complete -x -c npm -n '__fish_npm_using_command publish' -l tag -d 'Upload to tag'
complete -x -c npm -n '__fish_npm_using_command publish' -l access -d 'Restrict access' -a "public\t'Publicly viewable' restricted\t'Restricted access (scoped packages only)'"
complete -f -c npm -n '__fish_npm_using_command publish' -l dry-run -d 'Do not make any changes'
complete -x -c npm -n '__fish_npm_using_command publish' -l otp -d '2FA one-time password'
complete -f -c npm -n '__fish_npm_using_command publish' -l provenance -d 'Link to build location when publishing from CI/CD'
complete -f -c npm -n '__fish_npm_using_command publish' -s h -l help -d 'Display help'

# query
complete -f -c npm -n __fish_npm_needs_command -a query -d 'Dependency selector query'
complete -f -c npm -n '__fish_npm_using_command query' -s g -l global -d 'Query global packages'
complete -f -c npm -n '__fish_npm_using_command query' -s h -l help -d 'Display help'

# rebuild
complete -f -c npm -n __fish_npm_needs_command -a rebuild -d 'Rebuild a package'
for c in rebuild rb
    complete -x -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Rebuild global package'
    complete -f -c npm -n "__fish_npm_using_command $c" -l foreground-scripts -d 'Run all build scripts in the foreground'
    complete -f -c npm -n "__fish_npm_using_command $c" -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-bin-links -d "Don't symblink package executables"
    complete -f -c npm -n "__fish_npm_using_command $c" -l install-links -d 'Install file: protocol deps as regular deps'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# repo
complete -f -c npm -n __fish_npm_needs_command -a repo -d 'Open package repository page in the browser'
complete -f -c npm -n '__fish_npm_using_command repo' -s g -l global -d 'Display global root'
complete -x -c npm -n '__fish_npm_using_command repo' -l browser -d 'Set browser'
complete -x -c npm -n '__fish_npm_using_command repo' -l no-browser -d 'Print to stdout'
complete -x -c npm -n '__fish_npm_using_command repo' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command repo' -s h -l help -d 'Display help'

# restart
# start
# stop
# test
complete -f -c npm -n __fish_npm_needs_command -a restart -d 'Restart a package'
complete -f -c npm -n __fish_npm_needs_command -a start -d 'Start a package'
complete -f -c npm -n __fish_npm_needs_command -a stop -d 'Stop a package'
complete -f -c npm -n __fish_npm_needs_command -a test -d 'Test a package'
for c in restart start stop test tst t
    complete -f -c npm -n "__fish_npm_using_command $c" -s ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -x -c npm -n "__fish_npm_using_command $c" -s script-shell -d 'The shell to use for scripts'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# root
complete -f -c npm -n __fish_npm_needs_command -a root -d 'Display npm root'
complete -f -c npm -n '__fish_npm_using_command root' -s g -l global -d 'Display global root'
complete -f -c npm -n '__fish_npm_using_command root' -s h -l help -d 'Display help'

# search
complete -f -c npm -n __fish_npm_needs_command -a 'search find' -d 'Search for packages'
for c in search find s se
    complete -f -c npm -n "__fish_npm_using_command $c" -s l -l long -d 'Show extended information'
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON data'
    complete -f -c npm -n "__fish_npm_using_command $c" -l color -a always -d 'Print color'
    complete -x -c npm -n "__fish_npm_using_command $c" -l color -a always -d 'Print color'
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-color -d "Don't print color"
    complete -f -c npm -n "__fish_npm_using_command $c" -s p -l parseable -d 'Output parseable results'
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-description -d "Don't show the description"
    complete -x -c npm -n "__fish_npm_using_command $c" -l searchopts -d 'Space-separated search options'
    complete -x -c npm -n "__fish_npm_using_command $c" -l searchexclude -d 'Space-separated options to exclude from search'
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
    complete -f -c npm -n "__fish_npm_using_command $c" -l prefer-online -d 'Force staleness checks for cached data'
    complete -f -c npm -n "__fish_npm_using_command $c" -l prefer-offline -d 'Bypass staleness checks for cached data'
    complete -f -c npm -n "__fish_npm_using_command $c" -l offline -d 'Force offline mode'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# shrinkwrap
complete -f -c npm -n __fish_npm_needs_command -a shrinkwrap -d 'Lock down dependency versions'
complete -f -c npm -n '__fish_npm_using_command shrinkwrap' -s h -l help -d 'Display help'

# star
complete -f -c npm -n __fish_npm_needs_command -a star -d 'Mark your favorite packages'
complete -x -c npm -n '__fish_npm_using_command star' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command star' -l unicode -d 'Use unicode characters in the output'
complete -f -c npm -n '__fish_npm_using_command star' -l no-unicode -d 'Use ascii characters over unicode glyphs'
complete -x -c npm -n '__fish_npm_using_command star' -l otp -d '2FA one-time password'
complete -f -c npm -n '__fish_npm_using_command star' -s h -l help -d 'Display help'

# stars
complete -f -c npm -n __fish_npm_needs_command -a stars -d 'View packages marked as favorites'
complete -x -c npm -n '__fish_npm_using_command stars' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command stars' -s h -l help -d 'Display help'

# team
set -l team_commands 'create destroy add rm ls'
complete -f -c npm -n __fish_npm_needs_command -a team -d 'Manage organization teams and team memberships'
complete -x -c npm -n '__fish_npm_using_command team' -n "not __fish_seen_subcommand_from $team_commands" -a create -d 'Create a new team'
complete -x -c npm -n '__fish_npm_using_command team' -n "not __fish_seen_subcommand_from $team_commands" -a destroy -d 'Destroy an existing team'
complete -x -c npm -n '__fish_npm_using_command team' -n "not __fish_seen_subcommand_from $team_commands" -a add -d 'Add a user to an existing team'
complete -x -c npm -n '__fish_npm_using_command team' -n "not __fish_seen_subcommand_from $team_commands" -a rm -d 'Remove users from a team'
complete -x -c npm -n '__fish_npm_using_command team' -n "not __fish_seen_subcommand_from $team_commands" -a ls -d 'List teams or team members'
complete -x -c npm -n '__fish_npm_using_command team' -n 'not __fish_seen_subcommand_from ls' -l otp -d '2FA one-time password'
complete -x -c npm -n '__fish_npm_using_command team' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command team' -s p -l parseable -d 'Output parseable results'
complete -f -c npm -n '__fish_npm_using_command team' -l json -d 'Output JSON'
complete -f -c npm -n '__fish_npm_using_command team' -s h -l help -d 'Display help'

# token
set -l token_commands 'create destroy add rm ls'
complete -f -c npm -n __fish_npm_needs_command -a token -d 'Manage your authentication tokens'
complete -x -c npm -n '__fish_npm_using_command token' -n "not __fish_seen_subcommand_from $token_commands" -a list -d 'Shows active authentication tokens'
complete -x -c npm -n '__fish_npm_using_command token' -n "not __fish_seen_subcommand_from $token_commands" -a revoke -d 'Revokes an authentication token'
complete -x -c npm -n '__fish_npm_using_command token' -n "not __fish_seen_subcommand_from $token_commands" -a create -d 'Create a new authentication token'
complete -f -c npm -n '__fish_npm_using_command token' -n '__fish_seen_subcommand_from create' -l read-only -d 'Mark a token as unable to publish'
complete -x -c npm -n '__fish_npm_using_command token' -n '__fish_seen_subcommand_from create' -l cidr -d 'List of CIDR address'
complete -x -c npm -n '__fish_npm_using_command token' -l registry -d 'Registry base URL'
complete -x -c npm -n '__fish_npm_using_command token' -l otp -d '2FA one-time password'
complete -f -c npm -n '__fish_npm_using_command token' -s h -l help -d 'Display help'

# update
complete -f -c npm -n __fish_npm_needs_command -a 'update up upgrade' -d 'Update package(s)'
for c in update up upgrade udpate
    complete -f -c npm -n "__fish_npm_using_command $c" -s S -l save -d 'Save to dependencies'
    complete -x -c npm -n "__fish_npm_using_command $c" -l no-save -d 'Do not remove package from your dependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Update global package(s)'
    complete -x -c npm -n "__fish_npm_using_command $c" -l install-strategy -a 'hoisted nested shallow linked' -d 'Install strategy'
    complete -x -c npm -n "__fish_npm_using_command $c" -l omit -a 'dev optional peer' -d 'Omit dependency type'
    complete -x -c npm -n "__fish_npm_using_command $c" -l strict-peer-deps -d 'Treat conflicting peerDependencies as failure'
    complete -x -c npm -n "__fish_npm_using_command $c" -l no-package-lock -d 'Ignore package-lock.json'
    complete -f -c npm -n "__fish_npm_using_command $c" -l foreground-scripts -d 'Run all build scripts in the foreground'
    complete -f -c npm -n "__fish_npm_using_command $c" -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-audit -d "Don't submit audit reports"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-bin-links -d "Don't symblink package executables"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-fund -d "Don't display funding info"
    complete -f -c npm -n "__fish_npm_using_command $c" -l dry-run -d 'Do not make any changes'
    complete -f -c npm -n "__fish_npm_using_command $c" -l install-links -d 'Install file: protocol deps as regular deps'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# uninstall
complete -f -c npm -n __fish_npm_needs_command -a 'uninstall remove un' -d 'Remove a package'
for c in uninstall unlink remove rm r un
    complete -x -c npm -n "__fish_npm_using_command $c" -d 'Remove package' -a '(__npm_installed_local_packages)'
    complete -x -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Remove global package' -a '(__npm_installed_global_packages)'
    complete -f -c npm -n "__fish_npm_using_command $c" -s S -l save -d 'Save to dependencies'
    complete -x -c npm -n "__fish_npm_using_command $c" -l no-save -d 'Do not remove package from your dependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -l install-links -d 'Install file: protocol deps as regular deps'
end

# unpublish
complete -f -c npm -n __fish_npm_needs_command -a unpublish -d 'Remove a package from the registry'
complete -x -c npm -n '__fish_npm_using_command unpublish' -l dry-run -d 'Do not make any changes'
complete -x -c npm -n '__fish_npm_using_command unpublish' -s f -l force -d 'Removes various protections'
complete -f -c npm -n '__fish_npm_using_command unpublish' -s h -l help -d 'Display help'

# unstar
complete -f -c npm -n __fish_npm_needs_command -a unstar -d 'Remove star from a package'
complete -x -c npm -n '__fish_npm_using_command unstar' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command unstar' -l unicode -d 'Use unicode characters in the output'
complete -f -c npm -n '__fish_npm_using_command unstar' -l no-unicode -d 'Use ascii characters over unicode glyphs'
complete -x -c npm -n '__fish_npm_using_command unstar' -l otp -d '2FA one-time password'
complete -f -c npm -n '__fish_npm_using_command unstar' -s h -l help -d 'Display help'

# version
complete -f -c npm -n __fish_npm_needs_command -a version -d 'Bump a package version'
for c in version verison
    complete -x -c npm -n "__fish_npm_using_command $c" -a 'major minor patch premajor preminor prepatch prerelease'
    complete -f -c npm -n "__fish_npm_using_command $c" -l allow-same-version -d 'Allow same version'
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-commit-hooks -d "Don't run git commit hooks"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-git-tag-version -d "Don't tag the commit"
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON'
    complete -x -c npm -n "__fish_npm_using_command $c" -l preid -d 'The prerelease identifier'
    complete -f -c npm -n "__fish_npm_using_command $c" -l sign-git-tag -d 'Sign the version tag'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# view
complete -f -c npm -n __fish_npm_needs_command -a 'view info' -d 'View registry info'
for c in view info v show
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 2'
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 3' -a 'author bin bugs description engines exports homepage keywords license main name repository scripts type types'
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 3' -a 'dependencies devDependencies optionalDependencies peerDependencies'
    complete -f -c npm -n "__fish_npm_using_command $c" -n '__fish_is_nth_token 3' -a 'directories dist dist-tags gitHead maintainers readme time users version versions'
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON'
    complete -f -c npm -n "__fish_npm_using_command $c" -s h -l help -d 'Display help'
end

# whoami
complete -f -c npm -n __fish_npm_needs_command -a whoami -d 'Display npm username'
complete -f -c npm -n '__fish_npm_using_command whoami' -a registry -d 'Check registry'
complete -f -c npm -n '__fish_npm_using_command whoami' -s h -l help -d 'Display help'

# misc
complete -f -c npm -n '__fish_seen_subcommand_from add i in ins inst insta instal isnt isnta isntal isntall; and not __fish_is_switch' -a "(__npm_filtered_list_packages \"$npm_install\")"
