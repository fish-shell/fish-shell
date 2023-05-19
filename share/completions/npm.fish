# npm (https://npmjs.org) completions for Fish shell
# __fish_npm_needs_* and __fish_npm_using_* taken from:
# https://stackoverflow.com/questions/16657803/creating-autocomplete-script-with-sub-commands
# see also Fish's large set of completions for examples:
# https://github.com/fish-shell/fish-shell/tree/master/share/completions

source $__fish_data_dir/functions/__fish_npm_helper.fish
set -l npm_install "npm install --global"

function __fish_npm_needs_command
    set -l cmd (commandline -opc)

    if test (count $cmd) -eq 1
        return 0
    end

    return 1
end

function __fish_npm_using_command
    set -l cmd (commandline -opc)

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

function __fish_complete_npm -d "Complete the commandline using npm's 'completion' tool"
    # Note that this function will generate undescribed completion options, and current fish
    # will sometimes pick these over versions with descriptions.
    # However, this seems worth it because it means automatically getting _some_ completions if npm updates.

    # Defining an npm alias that automatically calls nvm if necessary is a popular convenience measure.
    # Because that is a function, these local variables won't be inherited and the completion would fail
    # with weird output on stdout (!). But before the function is called, no npm command is defined,
    # so calling the command would fail.
    # So we'll only try if we have an npm command.
    if command -sq npm
        # npm completion is bash-centric, so we need to translate fish's "commandline" stuff to bash's $COMP_* stuff
        # COMP_LINE is an array with the words in the commandline
        set -lx COMP_LINE (commandline -opc)
        # COMP_CWORD is the index of the current word in COMP_LINE
        # bash starts arrays with 0, so subtract 1
        set -lx COMP_CWORD (math (count $COMP_LINE) - 1)
        # COMP_POINT is the index of point/cursor when the commandline is viewed as a string
        set -lx COMP_POINT (commandline -C)
        # If the cursor is after the last word, the empty token will disappear in the expansion
        # Readd it
        if test -z (commandline -ct)
            set COMP_CWORD (math $COMP_CWORD + 1)
            set COMP_LINE $COMP_LINE ""
        end
        command npm completion -- $COMP_LINE 2>/dev/null
    end
end

# use npm completion for most of the things,
# except options completion (because it sucks at it)
# and run-script completion (reading package.json is faster).
# and uninstall (reading dependencies is faster and more robust and yarn needs the functions anyways)
# see: https://github.com/npm/npm/issues/9524
# and: https://github.com/fish-shell/fish-shell/pull/2366
complete -f -c npm -n 'not __fish_npm_needs_option; and not __fish_npm_using_command run-script run rum urn access audit cache uninstall unlink remove rm r un version verison view info show v' -a "(__fish_complete_npm)"

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
complete -x -c npm -n "__fish_npm_using_command access; and not __fish_seen_subcommand_from $access_commands" -a list -d 'List access info'
complete -x -c npm -n "__fish_npm_using_command access; and not __fish_seen_subcommand_from $access_commands" -a get -d 'Get access level'
complete -x -c npm -n "__fish_npm_using_command access; and not __fish_seen_subcommand_from $access_commands" -a grant -d 'Grant access to users'
complete -x -c npm -n "__fish_npm_using_command access; and not __fish_seen_subcommand_from $access_commands" -a revoke -d 'Revoke access from users'
complete -x -c npm -n "__fish_npm_using_command access; and not __fish_seen_subcommand_from $access_commands" -a set -d 'Set access level'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from list' -a 'packages collaborators'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from get' -a status
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from grant' -a 'read-only read-write'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from set' -a 'status=public status=private' -d 'Set package status'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from set' -a 'mfa=none mfa=publish mfa=automation' -d 'Set package MFA'
complete -x -c npm -n '__fish_npm_using_command access' -n '__fish_seen_subcommand_from set' -a '2fa=none 2fa=publish 2fa=automation' -d 'Set package MFA'
complete -f -c npm -n '__fish_npm_using_command access' -l json -d 'Output JSON'
complete -x -c npm -n '__fish_npm_using_command access' -l otp -d '2FA one-time password'
complete -x -c npm -n '__fish_npm_using_command access' -l registry -d 'Registry base URL'

# adduser
complete -f -c npm -n __fish_npm_needs_command -a adduser -d 'Add a registry user account'
complete -f -c npm -n __fish_npm_needs_command -a login -d 'Login to a registry user account'
for c in adduser add-user login
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
    complete -x -c npm -n "__fish_npm_using_command $c" -l scope -d 'Log into a private repository'
    complete -x -c npm -n "__fish_npm_using_command $c" -l auth-type -a 'legacy web' -d 'Authentication strategy'
end

# audit
complete -f -c npm -n __fish_npm_needs_command -a audit -d 'Run a security audit'
complete -f -c npm -n '__fish_npm_using_command audit' -a signatures -d 'Verify registry signatures'
complete -f -c npm -n '__fish_npm_using_command audit' -a fix -d 'Install compatible updates to vulnerable deps'
complete -x -c npm -n '__fish_npm_using_command audit' -l audit-level -a 'info low moderate high critical none' -d 'Audit level'
complete -f -c npm -n '__fish_npm_using_command audit' -l dry-run -d 'Do not make any changes'
complete -f -c npm -n '__fish_npm_using_command audit' -l force -d 'Removes various protections'
complete -f -c npm -n '__fish_npm_using_command audit' -l json -d 'Output JSON'
complete -f -c npm -n '__fish_npm_using_command audit' -l package-lock-only -d 'Only use package-lock.json, ignore node_modules'
complete -x -c npm -n '__fish_npm_using_command audit' -l omit -a 'dev optional peer' -d 'Omit dependency type'
complete -f -c npm -n '__fish_npm_using_command audit' -l foreground-scripts -d 'Run all build scripts in the foreground'
complete -f -c npm -n '__fish_npm_using_command audit' -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
complete -f -c npm -n '__fish_npm_using_command audit' -l install-links -d 'Install file: protocol deps as regular deps'

# bugs
for c in bugs issues
    complete -f -c npm -n __fish_npm_needs_command -a "$c" -d 'Report bugs for a package in a web browser'
    complete -x -c npm -n "__fish_npm_using_command $c" -l browser -d 'Set browser'
    complete -x -c npm -n "__fish_npm_using_command $c" -l no-browser -d 'Print to stdout'
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
end

# cache
complete -f -c npm -n __fish_npm_needs_command -a cache -d "Manipulates package's cache"
complete -f -c npm -n '__fish_npm_using_command cache' -a add -d 'Add the specified package to the local cache'
complete -f -c npm -n '__fish_npm_using_command cache' -a clean -d 'Delete data out of the cache folder'
complete -f -c npm -n '__fish_npm_using_command cache' -a ls -d 'Show the data in the cache'
complete -f -c npm -n '__fish_npm_using_command cache' -a verify -d 'Verify the contents of the cache folder'
complete -x -c npm -n '__fish_npm_using_command cache' -l cache -a '(__fish_complete_directories)' -d 'Cache location'

# ci
# install-ci-test
complete -f -c npm -n __fish_npm_needs_command -a 'ci clean-install' -d 'Clean install a project'
complete -f -c npm -n __fish_npm_needs_command -a 'install-ci-test cit' -d 'Install a project with a clean slate and run tests'
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
end

# config
complete -f -c npm -n __fish_npm_needs_command -a config -d 'Manage the npm configuration files'
for c in config c
    complete -f -c npm -n "__fish_npm_using_command $c" -a set -d 'Sets the config keys to the values'
    complete -f -c npm -n "__fish_npm_using_command $c" -a get -d 'Echo the config value(s) to stdout'
    complete -f -c npm -n "__fish_npm_using_command $c" -a delete -d 'Deletes the key from all configuration files'
    complete -f -c npm -n "__fish_npm_using_command $c" -a list -d 'Show all the config settings'
    complete -f -c npm -n "__fish_npm_using_command $c" -a ls -d 'Show all the config settings'
    complete -f -c npm -n "__fish_npm_using_command $c" -a edit -d 'Opens the config file in an editor'
    complete -f -c npm -n "__fish_npm_using_command $c" -a fix -d 'Attempts to repair invalid configuration items'
    complete -f -c npm -n "__fish_npm_using_command $c" -a fix -d 'Attempts to repair invalid configuration items'
end
# get, set also exist as shorthands
complete -f -c npm -n __fish_npm_needs_command -a get -d 'Get a value from the npm configuration'
complete -f -c npm -n __fish_npm_needs_command -a set -d 'Set a value in the npm configuration'

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

    if test $c != find-dupes
        complete -f -c npm -n "__fish_npm_using_command $c" -l dry-run -d "Don't display funding info"
    end
end

# deprecate
complete -f -c npm -n __fish_npm_needs_command -a deprecate -d 'Deprecate a version of a package'
complete -x -c npm -n '__fish_npm_using_command deprecate' -l registry -d 'Registry base URL'
complete -x -c npm -n '__fish_npm_using_command deprecate' -l otp -d '2FA one-time password'

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

# dist-tag
complete -f -c npm -n __fish_npm_needs_command -a dist-tag -d 'Modify package distribution tags'
for c in dist-tag dist-tags
    complete -f -c npm -n "__fish_npm_using_command $c" -a add -d 'Tag the package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a rm -d 'Clear a tag from the package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a ls -d 'List all dist-tags'
end

# docs
complete -f -c npm -n __fish_npm_needs_command -a docs -d 'Open docs for a package in a web browser'
for c in docs home
    complete -x -c npm -n "__fish_npm_using_command $c" -l browser -d 'Set browser'
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
end

# doctor
complete -f -c npm -n __fish_npm_needs_command -a doctor -d 'Check your npm environment'
complete -f -c npm -n '__fish_npm_using_command doctor' -a ping -d 'Check npm ping'
complete -f -c npm -n '__fish_npm_using_command doctor' -a registry -d 'Check registry'
complete -f -c npm -n '__fish_npm_using_command doctor' -a versions -d 'Check installed versions'
complete -f -c npm -n '__fish_npm_using_command doctor' -a environment -d 'Check PATH'
complete -f -c npm -n '__fish_npm_using_command doctor' -a permissions -d 'Check file permissions'
complete -f -c npm -n '__fish_npm_using_command doctor' -a cache -d 'Verify cache'

# edit
complete -f -c npm -n __fish_npm_needs_command -a edit -d 'Edit an installed package'
complete -f -c npm -n '__fish_npm_using_command edit' -a editor -d 'Specify the editor'

# exec
complete -f -c npm -n __fish_npm_needs_command -a exec -d 'Run a command from a local or remote npm package'
for c in exec x
    complete -x -c npm -n "__fish_npm_using_command $c" -l package -d 'The package(s) to install'
    complete -x -c npm -n "__fish_npm_using_command $c" -l call -d 'Specify a custom command'
end

# explain
for c in explain why
    complete -f -c npm -n __fish_npm_needs_command -a "$c" -d 'Explain installed packages'
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON'
end

# explore
complete -f -c npm -n __fish_npm_needs_command -a explore -d 'Browse an installed package'
complete -f -c npm -n '__fish_npm_using_command explore' -a shell -d 'The shell to open'

# fund
complete -f -c npm -n __fish_npm_needs_command -a fund -d 'Retrieve funding information'
complete -f -c npm -n '__fish_npm_using_command fund' -l json -d 'Output JSON'
complete -x -c npm -n '__fish_npm_using_command fund' -l browser -d 'Set browser'
complete -f -c npm -n '__fish_npm_using_command fund' -l no-browser -d 'Print to stdout'
complete -f -c npm -n '__fish_npm_using_command fund' -l unicode -d 'Use unicode characters in the output'
complete -f -c npm -n '__fish_npm_using_command fund' -l no-unicode -d 'Use ascii characters over unicode glyphs'
complete -x -c npm -n '__fish_npm_using_command fund' -l which -d 'Which source URL to open (1-indexed)'

# help-search
complete -f -c npm -n __fish_npm_needs_command -a help-search -d 'Search npm help documentation'
complete -f -c npm -n '__fish_npm_using_command help-search' -s l -l long -d 'Show extended information'

# hook
complete -f -c npm -n __fish_npm_needs_command -a hook -d 'Manage registry hooks'
complete -f -c npm -n '__fish_npm_using_command hook' -a add -d 'Add a hook'
complete -f -c npm -n '__fish_npm_using_command hook; and __fish_seen_subcommand_from add' -l type -d 'Hook type'
complete -f -c npm -n '__fish_npm_using_command hook' -a ls -d 'List all active hooks'
complete -f -c npm -n '__fish_npm_using_command hook' -a update -d 'Update an existing hook'
complete -f -c npm -n '__fish_npm_using_command hook' -a rm -d 'Remove a hook'
complete -x -c npm -n '__fish_npm_using_command hook' -l registry -d 'Registry base URL'
complete -x -c npm -n '__fish_npm_using_command hook' -l otp -d '2FA one-time password'

# init
complete -c npm -n __fish_npm_needs_command -a 'init create' -d 'Create a package.json file'
for c in init create innit
    complete -f -c npm -n "__fish_npm_using_command $c" -s y -l yes -d 'Automatically answer "yes" to all prompts'
    complete -f -c npm -n "__fish_npm_using_command $c" -l force -d 'Removes various protections'
    complete -x -c npm -n "__fish_npm_using_command $c" -l scope -d 'Create a scoped package'
end

# install
# install-test
complete -c npm -n __fish_npm_needs_command -a 'install add i' -d 'Install a package'
complete -f -c npm -n __fish_npm_needs_command -a 'install-test it' -d 'Install package(s) and run tests'
for c in install add i 'in' ins inst insta instal isnt isnta isntal isntall install-test it
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
    complete -f -c npm -n "__fish_npm_using_command $c" -l foreground-scripts -d 'Run all build scripts in the foreground'
    complete -f -c npm -n "__fish_npm_using_command $c" -l ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-audit -d "Don't submit audit reports"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-bin-links -d "Don't symblink package executables"
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-fund -d "Don't display funding info"
    complete -f -c npm -n "__fish_npm_using_command $c" -l dry-run -d 'Do not make any changes'
    complete -f -c npm -n "__fish_npm_using_command $c" -l install-links -d 'Install file: protocol deps as regular deps'
end

# logout
complete -f -c npm -n __fish_npm_needs_command -a logout -d 'Log out of the registry'
complete -x -c npm -n '__fish_npm_using_command logout' -l registry -d 'Registry base URL'
complete -x -c npm -n '__fish_npm_using_command logout' -l scope -d 'Log out of private repository'

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
end

# outdated
complete -f -c npm -n __fish_npm_needs_command -a outdated -d 'Check for outdated packages'
complete -f -c npm -n '__fish_npm_using_command outdated' -s a -l all -d 'Also show indirect dependencies'
complete -f -c npm -n '__fish_npm_using_command outdated' -l json -d 'Output JSON'
complete -f -c npm -n '__fish_npm_using_command outdated' -s l -l long -d 'Show extended information'
complete -f -c npm -n '__fish_npm_using_command outdated' -l parseable -d 'Output parseable results'
complete -f -c npm -n '__fish_npm_using_command outdated' -s g -l global -d 'Check global packages'

# org
complete -f -c npm -n __fish_npm_needs_command -a org -d 'Manage orgs'
for c in org ogr
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 2" -a set -d 'Add a new user'
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 2" -a rm -d 'Remove a user'
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 2" -a ls -d 'List all users'

    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 5" -n '__fish_seen_subcommand_from set' -a admin -d 'Add admin'
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 5" -n '__fish_seen_subcommand_from set' -a developer -d 'Add developer'
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 5" -n '__fish_seen_subcommand_from set' -a owner -d 'Add owner'
end

# owner
for c in owner author
    complete -f -c npm -n __fish_npm_needs_command -a "$c" -d 'Manage package owners'
    complete -f -c npm -n "__fish_npm_using_command $c" -a ls -d 'List package owners'
    complete -f -c npm -n "__fish_npm_using_command $c" -a add -d 'Add a new owner to package'
    complete -f -c npm -n "__fish_npm_using_command $c" -a rm -d 'Remove an owner from package'
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
    complete -x -c npm -n "__fish_npm_using_command $c" -l otp -d '2FA one-time password'
end

# pack
complete -f -c npm -n __fish_npm_needs_command -a pack -d 'Create a tarball from a package'
complete -f -c npm -n '__fish_npm_using_command pack' -l dry-run -d 'Do not make any changes'
complete -f -c npm -n '__fish_npm_using_command pack' -l json -d 'Output JSON'
complete -x -c npm -n '__fish_npm_using_command pack' -l pack-destination -a '(__fish_complete_directories)' -d 'Tarball destination directory'

# ping
complete -f -c npm -n __fish_npm_needs_command -a ping -d 'Ping npm registry'
complete -x -c npm -n '__fish_npm_using_command ping' -l registry -d 'Registry base URL'

# pkg
complete -f -c npm -n __fish_npm_needs_command -a pkg -d 'Manages your package.json'
complete -x -c npm -n '__fish_npm_using_command pkg' -a set -d 'Sets a value'
complete -x -c npm -n '__fish_npm_using_command pkg' -a get -d 'Retrieves a value'
complete -x -c npm -n '__fish_npm_using_command pkg' -a delete -d 'Deletes a key'
complete -f -c npm -n '__fish_npm_using_command pkg' -l force -d 'Removes various protections'
complete -f -c npm -n '__fish_npm_using_command pkg' -l json -d 'Parse values with JSON.parse()'

# prefix
complete -f -c npm -n __fish_npm_needs_command -a prefix -d 'Display npm prefix'
complete -f -c npm -n '__fish_npm_using_command prefix' -s g -l global -d 'Display global prefix'

# publish
complete -f -c npm -n __fish_npm_needs_command -a publish -d 'Publish a package'
complete -x -c npm -n '__fish_npm_using_command publish' -l tag -d 'Upload to tag'
complete -x -c npm -n '__fish_npm_using_command publish' -l access -d 'Restrict access' -a "public\t'Publicly viewable' restricted\t'Restricted access (scoped packages only)'"
complete -f -c npm -n '__fish_npm_using_command publish' -l dry-run -d 'Do not make any changes'
complete -x -c npm -n '__fish_npm_using_command publish' -l otp -d '2FA one-time password'
complete -f -c npm -n '__fish_npm_using_command publish' -l provenance -d 'Link to build location when publishing from CI/CD'

# query
complete -f -c npm -n __fish_npm_needs_command -a query -d 'Dependency selector query'
complete -f -c npm -n '__fish_npm_using_command query' -s g -l global -d 'Query global packages'

# remove
complete -f -c npm -n __fish_npm_needs_command -a 'uninstall remove un' -d 'Remove a package'
for c in uninstall unlink remove rm r un
    complete -x -c npm -n "__fish_npm_using_command $c" -d 'Remove package' -a '(__npm_installed_local_packages)'
    complete -x -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Remove global package' -a '(__npm_installed_global_packages)'
    complete -x -c npm -n "__fish_npm_using_command $c" -l no-save -d 'Do not remove package from your dependencies'
end

# repo
complete -f -c npm -n __fish_npm_needs_command -a repo -d 'Open package repository page in the browser'
complete -f -c npm -n '__fish_npm_using_command repo' -s g -l global -d 'Display global root'
complete -x -c npm -n '__fish_npm_using_command repo' -l browser -d 'Set browser'
complete -x -c npm -n '__fish_npm_using_command repo' -l no-browser -d 'Print to stdout'
complete -x -c npm -n '__fish_npm_using_command repo' -l registry -d 'Registry base URL'

# restart
# start
# stop
# test
complete -f -c npm -n __fish_npm_needs_command -a 'restart' -d 'Restart a package'
complete -f -c npm -n __fish_npm_needs_command -a 'start' -d 'Start a package'
complete -f -c npm -n __fish_npm_needs_command -a 'stop' -d 'Stop a package'
complete -f -c npm -n __fish_npm_needs_command -a 'test' -d 'Test a package'
for c in restart start stop test tst t
    complete -f -c npm -n "__fish_npm_using_command $c" -s ignore-scripts -d "Don't run pre-, post- and life-cycle scripts"
    complete -x -c npm -n "__fish_npm_using_command $c" -s script-shell -d 'The shell to use for scripts'
end

# root
complete -f -c npm -n __fish_npm_needs_command -a root -d 'Display npm root'
complete -f -c npm -n '__fish_npm_using_command root' -s g -l global -d 'Display global root'

# search
complete -f -c npm -n __fish_npm_needs_command -a 'search find' -d 'Search for packages'
for c in search find s se
    complete -f -c npm -n "__fish_npm_using_command $c" -s l -l long -d 'Show extended information'
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON data'
    complete -f -c npm -n "__fish_npm_using_command $c" -l color -a always -d 'Print color'
    complete -f -c npm -n "__fish_npm_using_command $c" -s p -l parseable -d 'Output parseable results'
    complete -f -c npm -n "__fish_npm_using_command $c" -l no-description -d "Don't show the description"
    complete -x -c npm -n "__fish_npm_using_command $c" -l searchopts -d 'Space-separated search options'
    complete -x -c npm -n "__fish_npm_using_command $c" -l searchexclude -d 'Space-separated options to exclude from search'
    complete -x -c npm -n "__fish_npm_using_command $c" -l registry -d 'Registry base URL'
    complete -f -c npm -n "__fish_npm_using_command $c" -l prefer-online -d 'Force staleness checks for cached data'
    complete -f -c npm -n "__fish_npm_using_command $c" -l prefer-offline -d 'Bypass staleness checks for cached data'
    complete -f -c npm -n "__fish_npm_using_command $c" -l offline -d 'Force offline mode'
end

# star
complete -f -c npm -n __fish_npm_needs_command -a star -d 'Mark your favorite packages'
complete -x -c npm -n '__fish_npm_using_command star' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command star' -l unicode -d 'Use unicode characters in the output'
complete -f -c npm -n '__fish_npm_using_command star' -l no-unicode -d 'Use ascii characters over unicode glyphs'
complete -x -c npm -n '__fish_npm_using_command star' -l otp -d '2FA one-time password'

# stars
complete -f -c npm -n __fish_npm_needs_command -a stars -d 'View packages marked as favorites'
complete -x -c npm -n '__fish_npm_using_command stars' -l registry -d 'Registry base URL'

# update
complete -f -c npm -n __fish_npm_needs_command -a 'update up upgrade' -d 'Update package(s)'
for c in update up upgrade udpate
    complete -f -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Update global package(s)'
end

# unstar
complete -f -c npm -n __fish_npm_needs_command -a unstar -d 'Remove star from a package'
complete -x -c npm -n '__fish_npm_using_command unstar' -l registry -d 'Registry base URL'
complete -f -c npm -n '__fish_npm_using_command unstar' -l unicode -d 'Use unicode characters in the output'
complete -f -c npm -n '__fish_npm_using_command unstar' -l no-unicode -d 'Use ascii characters over unicode glyphs'
complete -x -c npm -n '__fish_npm_using_command unstar' -l otp -d '2FA one-time password'

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
end

# view
complete -f -c npm -n __fish_npm_needs_command -a 'view info' -d 'View registry info'
for c in view info v show
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 2"
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 3" -a 'author bin bugs description engines exports homepage keywords license main name repository scripts type types'
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 3" -a 'dependencies devDependencies optionalDependencies peerDependencies'
    complete -f -c npm -n "__fish_npm_using_command $c; and __fish_is_nth_token 3" -a 'directories dist dist-tags gitHead maintainers readme time users version versions'
    complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Output JSON'
end

# whoami
complete -f -c npm -n __fish_npm_needs_command -a whoami -d 'Display npm username'
complete -f -c npm -n '__fish_npm_using_command whoami' -a registry -d 'Check registry'

# misc shorter explanations
complete -f -c npm -n __fish_npm_needs_command -a completion -d 'Tab Completion for npm'
complete -f -c npm -n __fish_npm_needs_command -a 'help hlep' -d 'Get help on npm'
complete -f -c npm -n __fish_npm_needs_command -a shrinkwrap -d 'Lock down dependency versions'

complete -f -c npm -n __fish_npm_needs_command -a 'link ln' -d 'Symlink a package folder'
complete -f -c npm -n __fish_npm_needs_command -a profile -d 'Change settings on your registry profile'
complete -f -c npm -n __fish_npm_needs_command -a prune -d 'Remove extraneous packages'
complete -f -c npm -n __fish_npm_needs_command -a 'rebuild rb' -d 'Rebuild a package'
complete -f -c npm -n __fish_npm_needs_command -a team -d 'Manage organization teams and team memberships'
complete -f -c npm -n __fish_npm_needs_command -a token -d 'Manage your authentication tokens'
complete -f -c npm -n __fish_npm_needs_command -a unpublish -d 'Remove a package from the registry'
complete -f -c npm -n '__fish_seen_subcommand_from add i in ins inst insta instal isnt isnta isntal isntall; and not __fish_is_switch' -a "(__npm_filtered_list_packages \"$npm_install\")"
